/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * Phase 5: Performance Optimization - ThreadPool Implementation
 */

#include "ThreadPool.h"
#include "../../Config/PlayerbotConfig.h"
#include "Log.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

#include <algorithm>
#include <random>

namespace Playerbot {
namespace Performance {

// ============================================================================
// WorkStealingQueue Implementation
// ============================================================================

template<typename T>
WorkStealingQueue<T>::WorkStealingQueue()
    : _array(std::make_unique<Node[]>(INITIAL_CAPACITY))
{
}

template<typename T>
bool WorkStealingQueue<T>::Push(T item)
{
    int64_t bottom = _bottom.load(std::memory_order_relaxed);
    int64_t top = _top.load(std::memory_order_acquire);

    // Check if queue is full
    if (bottom - top >= static_cast<int64_t>(_capacity.load(std::memory_order_relaxed)))
    {
        // Try to expand
        if (_capacity.load(std::memory_order_relaxed) < MAX_CAPACITY)
        {
            Expand();
        }
        else
        {
            return false; // Queue full, cannot expand further
        }
    }

    // Store item
    size_t index = static_cast<size_t>(bottom) & IndexMask();
    _array[index].data.store(item, std::memory_order_relaxed);

    // Ensure item is written before incrementing bottom
    std::atomic_thread_fence(std::memory_order_release);

    _bottom.store(bottom + 1, std::memory_order_relaxed);
    return true;
}

template<typename T>
bool WorkStealingQueue<T>::Pop(T& item)
{
    int64_t bottom = _bottom.load(std::memory_order_relaxed) - 1;
    _bottom.store(bottom, std::memory_order_relaxed);

    std::atomic_thread_fence(std::memory_order_seq_cst);

    int64_t top = _top.load(std::memory_order_relaxed);

    if (top <= bottom)
    {
        // Non-empty queue
        size_t index = static_cast<size_t>(bottom) & IndexMask();
        item = _array[index].data.load(std::memory_order_relaxed);

        if (top == bottom)
        {
            // Last item - compete with steal
            if (!_top.compare_exchange_strong(top, top + 1,
                std::memory_order_seq_cst,
                std::memory_order_relaxed))
            {
                // Lost race with steal
                item = nullptr;
                _bottom.store(bottom + 1, std::memory_order_relaxed);
                return false;
            }
            _bottom.store(bottom + 1, std::memory_order_relaxed);
        }
        return true;
    }
    else
    {
        // Empty queue
        _bottom.store(bottom + 1, std::memory_order_relaxed);
        return false;
    }
}

template<typename T>
bool WorkStealingQueue<T>::Steal(T& item)
{
    int64_t top = _top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    int64_t bottom = _bottom.load(std::memory_order_acquire);

    if (top < bottom)
    {
        // Non-empty queue
        size_t index = static_cast<size_t>(top) & IndexMask();
        item = _array[index].data.load(std::memory_order_relaxed);

        // Try to increment top
        if (!_top.compare_exchange_strong(top, top + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed))
        {
            // Lost race
            return false;
        }

        return true;
    }

    return false; // Empty queue
}

template<typename T>
void WorkStealingQueue<T>::Expand()
{
    std::lock_guard<std::mutex> lock(_expansionMutex);

    size_t oldCapacity = _capacity.load(std::memory_order_relaxed);
    size_t newCapacity = std::min(oldCapacity * 2, MAX_CAPACITY);

    if (newCapacity == oldCapacity)
        return; // Already at max

    auto newArray = std::make_unique<Node[]>(newCapacity);

    // Copy existing items
    int64_t bottom = _bottom.load(std::memory_order_relaxed);
    int64_t top = _top.load(std::memory_order_relaxed);

    for (int64_t i = top; i < bottom; ++i)
    {
        size_t oldIndex = static_cast<size_t>(i) & (oldCapacity - 1);
        size_t newIndex = static_cast<size_t>(i) & (newCapacity - 1);
        newArray[newIndex].data.store(
            _array[oldIndex].data.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    }

    _array = std::move(newArray);
    _capacity.store(newCapacity, std::memory_order_release);
}

// Explicit template instantiation
template class WorkStealingQueue<Task*>;

// ============================================================================
// WorkerThread Implementation
// ============================================================================

WorkerThread::WorkerThread(ThreadPool* pool, uint32 workerId, uint32 cpuCore)
    : _pool(pool)
    , _workerId(workerId)
    , _cpuCore(cpuCore)
{
    _thread = std::thread(&WorkerThread::Run, this);

    if (_pool->GetConfiguration().enableCpuAffinity)
    {
        SetAffinity();
    }
}

WorkerThread::~WorkerThread()
{
    if (_thread.joinable())
    {
        Shutdown();
        _thread.join();
    }
}

void WorkerThread::Run()
{
    auto lastActiveTime = std::chrono::steady_clock::now();

    while (_running.load(std::memory_order_relaxed))
    {
        bool didWork = false;

        // Try to execute task from local queues (priority order)
        if (TryExecuteTask())
        {
            didWork = true;
            lastActiveTime = std::chrono::steady_clock::now();
        }
        // Try to steal work from other workers
        else if (_pool->GetConfiguration().enableWorkStealing && TryStealTask())
        {
            didWork = true;
            lastActiveTime = std::chrono::steady_clock::now();
        }

        if (!didWork)
        {
            // Track idle time
            auto now = std::chrono::steady_clock::now();
            auto idleTime = std::chrono::duration_cast<std::chrono::microseconds>(now - lastActiveTime).count();
            _metrics.totalIdleTime.fetch_add(idleTime, std::memory_order_relaxed);

            // Sleep if no work available
            Sleep();
        }
    }
}

bool WorkerThread::TryExecuteTask()
{
    // Try each priority level in order
    for (size_t i = 0; i < static_cast<size_t>(TaskPriority::COUNT); ++i)
    {
        Task* task = nullptr;
        if (_localQueues[i].Pop(task) && task)
        {
            auto startTime = std::chrono::steady_clock::now();

            // Execute task
            task->Execute();

            auto endTime = std::chrono::steady_clock::now();
            auto workTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            // Update metrics
            _metrics.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
            _metrics.totalWorkTime.fetch_add(workTime, std::memory_order_relaxed);

            // Notify pool
            _pool->RecordTaskCompletion(task);

            return true;
        }
    }

    return false;
}

bool WorkerThread::TryStealTask()
{
    uint32 attempts = 0;
    uint32 maxAttempts = _pool->GetConfiguration().maxStealAttempts;

    while (attempts < maxAttempts)
    {
        _metrics.stealAttempts.fetch_add(1, std::memory_order_relaxed);

        // Get random worker to steal from
        uint32 victimId = GetRandomWorkerIndex();
        if (victimId == _workerId)
            continue;

        WorkerThread* victim = _pool->GetWorker(victimId);
        if (!victim)
            continue;

        // Try to steal from each priority level
        for (size_t i = 0; i < static_cast<size_t>(TaskPriority::COUNT); ++i)
        {
            Task* task = nullptr;
            if (victim->_localQueues[i].Steal(task) && task)
            {
                _metrics.stealSuccesses.fetch_add(1, std::memory_order_relaxed);

                auto startTime = std::chrono::steady_clock::now();

                // Execute stolen task
                task->Execute();

                auto endTime = std::chrono::steady_clock::now();
                auto workTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                _metrics.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
                _metrics.totalWorkTime.fetch_add(workTime, std::memory_order_relaxed);

                _pool->RecordTaskCompletion(task);

                return true;
            }
        }

        ++attempts;

        // Exponential backoff
        if (attempts < maxAttempts)
        {
            std::this_thread::yield();
        }
    }

    return false;
}

bool WorkerThread::SubmitLocal(Task* task, TaskPriority priority)
{
    size_t index = static_cast<size_t>(priority);
    return _localQueues[index].Push(task);
}

void WorkerThread::Wake()
{
    if (_sleeping.load(std::memory_order_relaxed))
    {
        std::lock_guard<std::mutex> lock(_wakeMutex);
        _wakeCv.notify_one();
    }
}

void WorkerThread::Shutdown()
{
    _running.store(false, std::memory_order_relaxed);
    Wake();
}

void WorkerThread::SetAffinity()
{
#ifdef _WIN32
    HANDLE thread = _thread.native_handle();
    DWORD_PTR mask = 1ULL << _cpuCore;
    SetThreadAffinityMask(thread, mask);
#else
    pthread_t thread = _thread.native_handle();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(_cpuCore, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
#endif
}

void WorkerThread::Sleep()
{
    _sleeping.store(true, std::memory_order_relaxed);

    std::unique_lock<std::mutex> lock(_wakeMutex);
    _wakeCv.wait_for(lock, _pool->GetConfiguration().workerSleepTime, [this]() {
        return !_running.load(std::memory_order_relaxed);
    });

    _sleeping.store(false, std::memory_order_relaxed);
}

uint32 WorkerThread::GetRandomWorkerIndex() const
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32> dist(0, _pool->GetWorkerCount() - 1);
    return dist(rng);
}

// ============================================================================
// ThreadPool Implementation
// ============================================================================

ThreadPool::ThreadPool(Configuration config)
    : _config(config)
{
    // CRITICAL VALIDATION: Ensure configuration is valid
    // NOTE: Cannot log here - constructor may be called before logging system is ready
    if (_config.numThreads < 1)
    {
        _config.numThreads = 1;  // Silent fix - any logging will happen in EnsureWorkersCreated()
    }

    // CRITICAL FIX: Don't create workers here anymore - they are created lazily on first Submit()
    // This prevents worker threads from starting before World is fully initialized
    _workers.reserve(_config.numThreads);
    // Workers will be created by EnsureWorkersCreated() on first Submit() call
}

ThreadPool::~ThreadPool()
{
    Shutdown(true);
}

void ThreadPool::EnsureWorkersCreated()
{
    // Fast path: Already created (no locking needed)
    if (_workersCreated.load(std::memory_order_acquire))
        return;

    // Slow path: Need to create workers (with locking for thread-safety)
    std::lock_guard<std::mutex> lock(_workerCreationMutex);

    // Double-check after acquiring lock (another thread may have created workers)
    if (_workersCreated.load(std::memory_order_relaxed))
        return;

    // CRITICAL VALIDATION: Runtime check before creating workers
    if (_config.numThreads < 1)
    {
        TC_LOG_FATAL("playerbot.performance",
            "CRITICAL ERROR: ThreadPool configured with numThreads = 0! Cannot create workers. "
            "This should have been caught in GetThreadPool() validation. System is in invalid state. "
            "Server will not function correctly without worker threads!");
        // Early return to prevent crash from accessing empty _workers vector
        return;
    }

    // Create worker threads (World is now fully initialized)
    TC_LOG_INFO("playerbot.performance", "Creating {} ThreadPool workers on first use", _config.numThreads);

    for (uint32 i = 0; i < _config.numThreads; ++i)
    {
        uint32 cpuCore = i % std::thread::hardware_concurrency();
        _workers.push_back(std::make_unique<WorkerThread>(this, i, cpuCore));
    }

    // Mark as created (release memory ordering ensures all workers are visible to other threads)
    _workersCreated.store(true, std::memory_order_release);

    TC_LOG_INFO("playerbot.performance", "ThreadPool workers created successfully");
}

// Note: Submit() template implementation moved to header for lambda support

bool ThreadPool::WaitForCompletion(std::chrono::milliseconds timeout)
{
    auto start = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if all queues are empty
        bool allEmpty = true;
        for (const auto& worker : _workers)
        {
            for (size_t i = 0; i < static_cast<size_t>(TaskPriority::COUNT); ++i)
            {
                if (!worker->_localQueues[i].Empty())
                {
                    allEmpty = false;
                    break;
                }
            }
            if (!allEmpty)
                break;
        }

        if (allEmpty)
            return true;

        // Check timeout
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start) >= timeout)
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ThreadPool::Shutdown(bool waitForPending)
{
    if (_shutdown.exchange(true))
        return; // Already shutting down

    TC_LOG_INFO("playerbot.performance", "ThreadPool shutting down (waitForPending={})", waitForPending);

    if (waitForPending)
    {
        WaitForCompletion(_config.shutdownTimeout);
    }

    // Stop all workers
    for (auto& worker : _workers)
    {
        worker->Shutdown();
    }

    // Wait for threads to finish
    for (auto& worker : _workers)
    {
        if (worker->_thread.joinable())
        {
            worker->_thread.join();
        }
    }

    _workers.clear();

    TC_LOG_INFO("playerbot.performance", "ThreadPool shutdown complete");
}

size_t ThreadPool::GetActiveThreads() const
{
    size_t active = 0;
    for (const auto& worker : _workers)
    {
        if (!worker->_sleeping.load(std::memory_order_relaxed))
        {
            ++active;
        }
    }
    return active;
}

size_t ThreadPool::GetQueuedTasks() const
{
    size_t total = 0;
    for (const auto& worker : _workers)
    {
        for (size_t i = 0; i < static_cast<size_t>(TaskPriority::COUNT); ++i)
        {
            total += worker->_localQueues[i].Size();
        }
    }
    return total;
}

size_t ThreadPool::GetQueuedTasks(TaskPriority priority) const
{
    size_t total = 0;
    size_t index = static_cast<size_t>(priority);
    for (const auto& worker : _workers)
    {
        total += worker->_localQueues[index].Size();
    }
    return total;
}

std::chrono::microseconds ThreadPool::GetAverageLatency() const
{
    uint64 completed = _metrics.totalCompleted.load(std::memory_order_relaxed);
    if (completed == 0)
        return std::chrono::microseconds(0);

    uint64 totalLatency = _metrics.totalLatency.load(std::memory_order_relaxed);
    return std::chrono::microseconds(totalLatency / completed);
}

double ThreadPool::GetThroughput() const
{
    uint64 completed = _metrics.totalCompleted.load(std::memory_order_relaxed);

    // Calculate throughput over last second
    // This is a simplified calculation - in production, you'd want a sliding window
    return static_cast<double>(completed);
}

uint32 ThreadPool::SelectWorkerRoundRobin()
{
    uint32 next = _nextWorker.fetch_add(1, std::memory_order_relaxed);
    return next % _config.numThreads;
}

uint32 ThreadPool::SelectWorkerLeastLoaded()
{
    uint32 leastLoaded = 0;
    size_t minQueueSize = SIZE_MAX;

    for (uint32 i = 0; i < _config.numThreads; ++i)
    {
        size_t queueSize = 0;
        for (size_t j = 0; j < static_cast<size_t>(TaskPriority::COUNT); ++j)
        {
            queueSize += _workers[i]->_localQueues[j].Size();
        }

        if (queueSize < minQueueSize)
        {
            minQueueSize = queueSize;
            leastLoaded = i;
        }
    }

    return leastLoaded;
}

void ThreadPool::RecordTaskCompletion(Task* task)
{
    auto completionTime = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
        completionTime - task->submittedAt).count();

    _metrics.totalCompleted.fetch_add(1, std::memory_order_relaxed);
    _metrics.totalLatency.fetch_add(latency, std::memory_order_relaxed);

    // Clean up task
    delete task;
}

// Global instance
static std::unique_ptr<ThreadPool> g_threadPool;
static std::mutex g_threadPoolMutex;

ThreadPool& GetThreadPool()
{
    std::lock_guard<std::mutex> lock(g_threadPoolMutex);
    if (!g_threadPool)
    {
        ThreadPool::Configuration config;

        // Load configuration from playerbots.conf
        // CRITICAL FIX: Don't try to load config if PlayerbotConfig isn't initialized yet
        // Just use default configuration - config will be reloaded later if needed
        try
        {
            if (PlayerbotConfig::instance())
            {
                config.numThreads = PlayerbotConfig::instance()->GetUInt(
                    "Playerbot.Performance.ThreadPool.WorkerCount",
                    config.numThreads);
                config.maxQueueSize = PlayerbotConfig::instance()->GetUInt(
                    "Playerbot.Performance.ThreadPool.MaxQueueSize",
                    config.maxQueueSize);
                config.enableWorkStealing = PlayerbotConfig::instance()->GetBool(
                    "Playerbot.Performance.ThreadPool.EnableWorkStealing",
                    config.enableWorkStealing);
                config.enableCpuAffinity = PlayerbotConfig::instance()->GetBool(
                    "Playerbot.Performance.ThreadPool.EnableCpuAffinity",
                    config.enableCpuAffinity);
            }
        }
        catch (...)
        {
            // Config not ready yet, use defaults
        }

        // CRITICAL VALIDATION: Ensure numThreads >= 1 (prevent crash from invalid config)
        // NOTE: Cannot log here - GetThreadPool() may be called before logging system is ready
        if (config.numThreads < 1)
        {
            config.numThreads = 1;  // Silent fix - logging will happen in EnsureWorkersCreated()
        }

        g_threadPool = std::make_unique<ThreadPool>(config);

        // CRITICAL FIX: Don't log here - GetThreadPool() may be called before logging system is ready
        // Worker creation logging happens in EnsureWorkersCreated() when workers are actually created
    }
    return *g_threadPool;
}

} // namespace Performance
} // namespace Playerbot
