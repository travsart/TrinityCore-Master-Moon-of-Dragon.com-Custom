/*
 * Copyright (C) 2024 TrinityCore Playerbot Module
 *
 * Thread Safety Policy and Lock Ordering for 5000+ Bot Scalability
 *
 * This file defines the global threading policy for the Playerbot module
 * to prevent deadlocks, minimize contention, and ensure scalability.
 */

#ifndef MODULE_PLAYERBOT_THREADING_POLICY_H
#define MODULE_PLAYERBOT_THREADING_POLICY_H

#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>
#include <folly/concurrency/ConcurrentHashMap.h>

namespace Playerbot
{
namespace Threading
{
    /**
     * CRITICAL: Lock Ordering Hierarchy (NEVER VIOLATE)
     *
     * Locks must ALWAYS be acquired in this order to prevent deadlocks:
     * 1. Session locks (BotWorldSessionMgr)
     * 2. Bot AI locks (BotAI)
     * 3. Combat Coordinator locks (InterruptCoordinator, ThreatCoordinator)
     * 4. Movement locks (PositionManager, FormationManager)
     * 5. Resource locks (CooldownManager, ResourceManager)
     * 6. Metrics/Statistics locks (Performance monitors)
     *
     * NEVER acquire a higher-level lock while holding a lower-level lock!
     */
    enum class LockLevel : uint8
    {
        SESSION = 1,      // Highest priority - Session management
        BOT_AI = 2,       // Bot AI state
        COMBAT = 3,       // Combat coordination
        MOVEMENT = 4,     // Movement and positioning
        RESOURCE = 5,     // Resource management
        METRICS = 6       // Lowest priority - Statistics
    };

    /**
     * Thread-safe lock wrapper that enforces ordering
     */
    template<typename MutexType>
    class OrderedLock
    {
    public:
        OrderedLock(MutexType& mutex, LockLevel level, const char* name = nullptr)
            : _mutex(mutex), _level(level), _name(name ? name : "unknown")
        {
            ValidateLockOrder();
        }

        MutexType& GetMutex() { return _mutex; }
        LockLevel GetLevel() const { return _level; }
        const char* GetName() const { return _name; }

    private:
        void ValidateLockOrder();

        MutexType& _mutex;
        LockLevel _level;
        const char* _name;

        static thread_local LockLevel _currentThreadLevel;
    };

    /**
     * Lock-free alternatives for hot paths
     */

    // Use TBB concurrent hash map for bot collections
    template<typename Key, typename Value>
    using ConcurrentHashMap = tbb::concurrent_hash_map<Key, Value>;

    // Use TBB concurrent queue for spawn queues
    template<typename T>
    using ConcurrentQueue = tbb::concurrent_queue<T>;

    // Use folly concurrent hash map for ultra-high throughput
    template<typename Key, typename Value>
    using HighThroughputMap = folly::ConcurrentHashMap<Key, Value>;

    /**
     * Performance-optimized atomic counters
     */
    template<typename T>
    class RelaxedAtomic
    {
    public:
        RelaxedAtomic(T initial = T{}) : _value(initial) {}

        T load() const { return _value.load(std::memory_order_relaxed); }
        void store(T val) { _value.store(val, std::memory_order_relaxed); }
        T fetch_add(T val) { return _value.fetch_add(val, std::memory_order_relaxed); }
        T fetch_sub(T val) { return _value.fetch_sub(val, std::memory_order_relaxed); }

    private:
        std::atomic<T> _value;
    };

    /**
     * Scoped performance timer for lock hold time analysis
     */
    class ScopedLockTimer
    {
    public:
        ScopedLockTimer(const char* lockName);
        ~ScopedLockTimer();

    private:
        const char* _lockName;
        std::chrono::high_resolution_clock::time_point _startTime;
        static std::atomic<uint64> _totalLockTime;
        static std::atomic<uint32> _lockCount;
    };

    /**
     * Lock-free bot state for read-heavy operations
     */
    template<typename T>
    class LockFreeState
    {
    public:
        LockFreeState() : _version(0) {}

        void Update(T newState)
        {
            uint64 newVersion = _version.fetch_add(1, std::memory_order_release) + 1;
            _state.store(newState, std::memory_order_release);
            _currentVersion.store(newVersion, std::memory_order_release);
        }

        T Read(uint64& version) const
        {
            T state;
            uint64 ver;
            do {
                ver = _currentVersion.load(std::memory_order_acquire);
                state = _state.load(std::memory_order_acquire);
            } while (ver != _currentVersion.load(std::memory_order_acquire));

            version = ver;
            return state;
        }

    private:
        std::atomic<T> _state;
        std::atomic<uint64> _version;
        std::atomic<uint64> _currentVersion;
    };

    /**
     * Thread pool for bot AI updates
     */
    class BotThreadPool
    {
    public:
        static BotThreadPool& Instance();

        void Initialize(uint32 threadCount = 0); // 0 = auto-detect
        void Shutdown();

        template<typename Func>
        void EnqueueTask(Func&& task);

        uint32 GetThreadCount() const { return _threadCount; }
        uint32 GetQueueSize() const { return _taskQueue.unsafe_size(); }

    private:
        BotThreadPool() = default;
        ~BotThreadPool() { Shutdown(); }

        void WorkerThread();

        std::vector<std::thread> _workers;
        ConcurrentQueue<std::function<void()>> _taskQueue;
        std::atomic<bool> _shutdown{false};
        uint32 _threadCount{0};
    };

    /**
     * RAII lock guards with automatic ordering validation
     */
    template<typename MutexType>
    class OrderedLockGuard
    {
    public:
        OrderedLockGuard(OrderedLock<MutexType>& lock)
            : _lock(lock.GetMutex()), _level(lock.GetLevel())
        {
            _previousLevel = _currentLevel;
            if (_currentLevel != LockLevel::SESSION && _level <= _currentLevel)
            {
                TC_LOG_ERROR("module.playerbot.threading",
                    "Lock ordering violation! Attempting to acquire lock level {} while holding level {}",
                    static_cast<int>(_level), static_cast<int>(_currentLevel));
            }
            _currentLevel = _level;
        }

        ~OrderedLockGuard()
        {
            _currentLevel = _previousLevel;
        }

    private:
        std::lock_guard<MutexType> _lock;
        LockLevel _level;
        LockLevel _previousLevel;
        static thread_local LockLevel _currentLevel;
    };

    /**
     * Reader-writer lock with ordering
     */
    template<typename MutexType = std::shared_mutex>
    class OrderedSharedLock
    {
    public:
        OrderedSharedLock(OrderedLock<MutexType>& lock)
            : _lock(lock.GetMutex()), _level(lock.GetLevel())
        {
            _previousLevel = _currentLevel;
            if (_currentLevel != LockLevel::SESSION && _level <= _currentLevel)
            {
                TC_LOG_ERROR("module.playerbot.threading",
                    "Shared lock ordering violation! Level {} while holding {}",
                    static_cast<int>(_level), static_cast<int>(_currentLevel));
            }
            _currentLevel = _level;
        }

        ~OrderedSharedLock()
        {
            _currentLevel = _previousLevel;
        }

    private:
        std::shared_lock<MutexType> _lock;
        LockLevel _level;
        LockLevel _previousLevel;
        static thread_local LockLevel _currentLevel;
    };

    // Thread-local storage for current lock level
    template<typename MutexType>
    thread_local LockLevel OrderedLockGuard<MutexType>::_currentLevel = LockLevel::METRICS;

    template<typename MutexType>
    thread_local LockLevel OrderedSharedLock<MutexType>::_currentLevel = LockLevel::METRICS;

} // namespace Threading
} // namespace Playerbot

#endif // MODULE_PLAYERBOT_THREADING_POLICY_H