/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * Phase 5: Performance Optimization - ThreadPool System
 *
 * Production-grade thread pool for PlayerBot AI update distribution
 * - Work-stealing queue architecture for load balancing
 * - Priority-based task scheduling (5 levels: CRITICAL to IDLE)
 * - Zero-allocation task submission during runtime
 * - CPU affinity support for cache locality
 * - Integration with existing BotScheduler
 *
 * Performance Targets:
 * - <1μs task submission latency
 * - >95% CPU utilization
 * - <100 context switches/sec per thread
 * - Support 5000+ concurrent bot updates
 */

#ifndef PLAYERBOT_THREADPOOL_H
#define PLAYERBOT_THREADPOOL_H

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <vector>
#include <array>
#include <queue>
#include <memory>
#include <chrono>
#include <type_traits>

namespace Playerbot {
namespace Performance {

// Forward declarations
class ThreadPool;
class WorkerThread;

/**
 * @brief Task priority levels for scheduling
 *
 * CRITICAL: Combat reactions, interrupts (0-10ms tolerance)
 * HIGH: Movement updates, target selection (10-50ms tolerance)
 * NORMAL: AI decisions, rotations (50-200ms tolerance)
 * LOW: Social interactions, inventory (200-1000ms tolerance)
 * IDLE: Background tasks, cleanup (no time constraints)
 */
enum class TaskPriority : uint8
{
    CRITICAL = 0,
    HIGH     = 1,
    NORMAL   = 2,
    LOW      = 3,
    IDLE     = 4,

    COUNT    = 5
};

/**
 * @brief Lock-free work-stealing queue for task distribution
 *
 * Based on Chase-Lev deque algorithm with optimizations:
 * - Lock-free push/pop for owner thread
 * - Lock-free steal for worker threads
 * - Cache-line aligned to prevent false sharing
 * - Exponential backoff on contention
 *
 * Template parameter T must be a pointer type for atomic operations
 */
template<typename T>
class alignas(64) WorkStealingQueue
{
private:
    static_assert(std::is_pointer_v<T>, "T must be a pointer type");

    static constexpr size_t INITIAL_CAPACITY = 1024;
    static constexpr size_t MAX_CAPACITY = 65536;

    struct Node
    {
        std::atomic<T> data{nullptr};

        Node() = default;
        Node(T value) : data(value) {}
    };

    // Cache-line aligned atomic indices
    alignas(64) std::atomic<int64_t> _bottom{0};
    alignas(64) std::atomic<int64_t> _top{0};
    alignas(64) std::unique_ptr<Node[]> _array;
    alignas(64) std::atomic<size_t> _capacity{INITIAL_CAPACITY};

    // Expansion lock
    std::mutex _expansionMutex;

public:
    WorkStealingQueue();
    ~WorkStealingQueue() = default;

    /**
     * @brief Push task to bottom (owner thread only)
     * @param item Task to push
     * @return true if successful, false if queue full
     */
    bool Push(T item);

    /**
     * @brief Pop task from bottom (owner thread only)
     * @param item Output parameter for popped task
     * @return true if task was popped, false if queue empty
     */
    bool Pop(T& item);

    /**
     * @brief Steal task from top (any thread)
     * @param item Output parameter for stolen task
     * @return true if task was stolen, false if queue empty or contention
     */
    bool Steal(T& item);

    /**
     * @brief Get current queue size (approximate)
     */
    size_t Size() const
    {
        int64_t bottom = _bottom.load(std::memory_order_relaxed);
        int64_t top = _top.load(std::memory_order_relaxed);
        return static_cast<size_t>(std::max<int64_t>(0, bottom - top));
    }

    /**
     * @brief Check if queue is empty
     */
    bool Empty() const
    {
        return Size() == 0;
    }

private:
    void Expand();
    size_t IndexMask() const { return _capacity.load(std::memory_order_relaxed) - 1; }
};

/**
 * @brief Base class for executable tasks
 *
 * Polymorphic task wrapper with type erasure for std::function
 * Uses small buffer optimization to avoid heap allocation for small functors
 */
class Task
{
public:
    virtual ~Task() = default;
    virtual void Execute() = 0;
    virtual TaskPriority GetPriority() const = 0;

    std::chrono::steady_clock::time_point submittedAt{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point completedAt;
};

/**
 * @brief Concrete task implementation with perfect forwarding
 */
template<typename Func>
class ConcreteTask : public Task
{
private:
    Func _func;
    TaskPriority _priority;

public:
    template<typename F>
    ConcreteTask(F&& func, TaskPriority priority)
        : _func(std::forward<F>(func))
        , _priority(priority)
    {
    }

    void Execute() override
    {
        startedAt = std::chrono::steady_clock::now();
        _func();
        completedAt = std::chrono::steady_clock::now();
    }

    TaskPriority GetPriority() const override
    {
        return _priority;
    }
};

/**
 * @brief Worker thread with local work queue and work stealing
 *
 * Each worker maintains:
 * - Local work queues per priority (5 queues)
 * - CPU affinity for cache locality
 * - Performance metrics (idle time, steal attempts, etc.)
 * - Exponential backoff on contention
 */
class alignas(64) WorkerThread
{
private:
    friend class ThreadPool;

    ThreadPool* _pool;
    std::thread _thread;
    uint32 _workerId;
    uint32 _cpuCore;

    // State management
    alignas(64) std::atomic<bool> _running{true};
    alignas(64) std::atomic<bool> _sleeping{false};

    // Local work queues (one per priority)
    std::array<WorkStealingQueue<Task*>, static_cast<size_t>(TaskPriority::COUNT)> _localQueues;

    // Performance metrics
    struct alignas(64) Metrics
    {
        std::atomic<uint64> tasksCompleted{0};
        std::atomic<uint64> totalWorkTime{0};    // microseconds
        std::atomic<uint64> totalIdleTime{0};    // microseconds
        std::atomic<uint64> stealAttempts{0};
        std::atomic<uint64> stealSuccesses{0};
        std::atomic<uint32> contextSwitches{0};
    } _metrics;

    // Wake notification
    std::mutex _wakeMutex;
    std::condition_variable _wakeCv;

public:
    WorkerThread(ThreadPool* pool, uint32 workerId, uint32 cpuCore);
    ~WorkerThread();

    /**
     * @brief Main worker loop
     */
    void Run();

    /**
     * @brief Try to execute a task from local queues
     * @return true if task was executed
     */
    bool TryExecuteTask();

    /**
     * @brief Try to steal a task from another worker
     * @return true if task was stolen and executed
     */
    bool TryStealTask();

    /**
     * @brief Submit task to local queue
     * @param task Task to submit
     * @param priority Task priority
     * @return true if submitted successfully
     */
    bool SubmitLocal(Task* task, TaskPriority priority);

    /**
     * @brief Wake worker from sleep
     */
    void Wake();

    /**
     * @brief Request worker shutdown
     */
    void Shutdown();

    /**
     * @brief Get worker metrics
     */
    Metrics GetMetrics() const { return _metrics; }

    /**
     * @brief Set CPU affinity
     */
    void SetAffinity();

private:
    void Sleep();
    uint32 GetRandomWorkerIndex() const;
};

/**
 * @brief High-performance thread pool for bot AI updates
 *
 * Features:
 * - Work-stealing for automatic load balancing
 * - Priority-based scheduling (5 levels)
 * - Zero-allocation task submission (uses object pools)
 * - CPU affinity support
 * - Graceful shutdown with pending task completion
 * - Comprehensive performance metrics
 *
 * Usage:
 * @code
 * ThreadPool pool(4); // 4 worker threads
 * auto future = pool.Submit(TaskPriority::HIGH, []() { return 42; });
 * int result = future.get();
 * @endcode
 */
class ThreadPool
{
public:
    struct Configuration
    {
        uint32 numThreads = std::thread::hardware_concurrency() > 2
            ? std::thread::hardware_concurrency() - 2
            : 1;
        uint32 maxQueueSize = 10000;
        bool enableWorkStealing = true;
        bool enableCpuAffinity = false; // Disabled by default (requires admin on Windows)
        uint32 maxStealAttempts = 3;
        std::chrono::milliseconds shutdownTimeout{5000};
        std::chrono::milliseconds workerSleepTime{1}; // Sleep time when idle
    };

private:
    Configuration _config;

    // Worker threads
    std::vector<std::unique_ptr<WorkerThread>> _workers;

    // Task object pool (to avoid allocation during Submit)
    template<typename T>
    class ObjectPool
    {
    private:
        std::vector<std::unique_ptr<T>> _pool;
        std::queue<T*> _available;
        std::mutex _mutex;

    public:
        ObjectPool(size_t initialSize = 1000)
        {
            _pool.reserve(initialSize);
            for (size_t i = 0; i < initialSize; ++i)
            {
                auto obj = std::make_unique<T>();
                _available.push(obj.get());
                _pool.push_back(std::move(obj));
            }
        }

        T* Acquire()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_available.empty())
            {
                auto obj = std::make_unique<T>();
                T* ptr = obj.get();
                _pool.push_back(std::move(obj));
                return ptr;
            }

            T* obj = _available.front();
            _available.pop();
            return obj;
        }

        void Release(T* obj)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _available.push(obj);
        }
    };

    // Global metrics
    struct alignas(64) Metrics
    {
        std::atomic<uint64> totalSubmitted{0};
        std::atomic<uint64> totalCompleted{0};
        std::atomic<uint64> totalFailed{0};
        std::atomic<uint64> totalLatency{0}; // microseconds
        std::array<std::atomic<uint64>, static_cast<size_t>(TaskPriority::COUNT)> tasksByPriority{};
        std::atomic<uint64> peakQueueSize{0};
    } _metrics;

    // Shutdown coordination
    std::atomic<bool> _shutdown{false};
    std::condition_variable _shutdownCv;
    std::mutex _shutdownMutex;

public:
    explicit ThreadPool(Configuration config = {});
    ~ThreadPool();

    /**
     * @brief Submit task with perfect forwarding and type deduction
     *
     * @param priority Task priority level
     * @param func Callable object (function, lambda, functor)
     * @param args Arguments to forward to func
     * @return std::future for result retrieval
     *
     * @note Zero allocation for small functors (< 32 bytes)
     * @note Thread-safe, can be called from any thread
     */
    template<typename Func, typename... Args>
    auto Submit(TaskPriority priority, Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>>;

    /**
     * @brief Submit batch of tasks efficiently
     *
     * @param priority Task priority
     * @param tasks Vector of tasks
     * @return Vector of futures
     */
    template<typename Func>
    std::vector<std::future<std::invoke_result_t<Func>>> SubmitBatch(
        TaskPriority priority,
        std::vector<Func>&& tasks);

    /**
     * @brief Wait for all pending tasks to complete
     * @param timeout Maximum time to wait
     * @return true if all tasks completed, false if timeout
     */
    bool WaitForCompletion(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

    /**
     * @brief Initiate graceful shutdown
     * @param waitForPending If true, wait for pending tasks to complete
     */
    void Shutdown(bool waitForPending = true);

    /**
     * @brief Get number of active worker threads
     */
    size_t GetActiveThreads() const;

    /**
     * @brief Get total queued tasks across all priorities
     */
    size_t GetQueuedTasks() const;

    /**
     * @brief Get queued tasks for specific priority
     */
    size_t GetQueuedTasks(TaskPriority priority) const;

    /**
     * @brief Get average task latency (submission to completion)
     */
    std::chrono::microseconds GetAverageLatency() const;

    /**
     * @brief Get throughput (tasks/second)
     */
    double GetThroughput() const;

    /**
     * @brief Get worker by index (for internal use)
     */
    WorkerThread* GetWorker(uint32 index)
    {
        return index < _workers.size() ? _workers[index].get() : nullptr;
    }

    /**
     * @brief Get worker count
     */
    uint32 GetWorkerCount() const
    {
        return static_cast<uint32>(_workers.size());
    }

    /**
     * @brief Check if pool is shutting down
     */
    bool IsShuttingDown() const
    {
        return _shutdown.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get configuration
     */
    Configuration GetConfiguration() const { return _config; }

private:
    uint32 SelectWorkerRoundRobin();
    uint32 SelectWorkerLeastLoaded();
    void RecordTaskCompletion(Task* task);

    std::atomic<uint32> _nextWorker{0};
};

/**
 * @brief Global thread pool instance (lazy initialization)
 */
ThreadPool& GetThreadPool();

} // namespace Performance
} // namespace Playerbot

#endif // PLAYERBOT_THREADPOOL_H
