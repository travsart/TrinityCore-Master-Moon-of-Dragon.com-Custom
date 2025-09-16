/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_MGR_H
#define BOT_SESSION_MGR_H

#include "Define.h"
#include "BotSession.h"
#include <parallel_hashmap/phmap.h>
#include <tbb/concurrent_vector.h>
#include <tbb/task_group.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <boost/lockfree/stack.hpp>
#include <atomic>
#include <shared_mutex>
#include <thread>
#include <span>
#include <memory>
#include <vector>
#include <chrono>

namespace Playerbot {

/**
 * CRITICAL IMPLEMENTATION REQUIREMENTS:
 *
 * 1. CONCURRENCY REQUIREMENTS:
 *    - MUST use phmap::parallel_flat_hash_map (8+ submaps)
 *    - MUST use Intel TBB work-stealing thread pool
 *    - MUST support 100-5000 concurrent sessions
 *    - MUST achieve linear scaling to 500 bots
 *
 * 2. BATCH PROCESSING REQUIREMENTS:
 *    - MUST group sessions in batches of 64
 *    - MUST use parallel batch updates with TBB
 *    - MUST implement SIMD optimization where applicable
 *    - MUST achieve < 0.05% CPU per bot
 *
 * 3. MEMORY MANAGEMENT REQUIREMENTS:
 *    - MUST use session pool with pre-allocation (1000 sessions)
 *    - MUST implement hibernation manager
 *    - MUST defragment memory every 60 seconds
 *    - MUST achieve 99% memory reduction for hibernated sessions
 *
 * 4. PERFORMANCE MONITORING REQUIREMENTS:
 *    - MUST track active/hibernated session counts
 *    - MUST monitor packets per second globally
 *    - MUST track total memory usage
 *    - MUST calculate average CPU percentage
 */
class TC_GAME_API BotSessionMgr final
{
public:
    // Singleton with thread-safe lazy initialization
    static BotSessionMgr* instance()
    {
        static BotSessionMgr instance;
        return &instance;
    }

    // Initialization with Intel TBB thread pool
    bool Initialize();
    void Shutdown();

    // === HIGH-PERFORMANCE SESSION MANAGEMENT ===

    // Session pool operations with object pooling
    BotSession* CreateSession(uint32 bnetAccountId);
    void ReleaseSession(uint32 bnetAccountId);
    BotSession* GetSession(uint32 bnetAccountId) const;

    // Batch session operations
    std::vector<BotSession*> CreateSessionBatch(std::vector<uint32> const& bnetAccountIds);
    void ReleaseSessionBatch(std::vector<uint32> const& bnetAccountIds);

    // === PARALLEL UPDATE SYSTEM ===

    // Intel TBB parallel session updates
    void UpdateAllSessions(uint32 diff);

    // Work-stealing batch processing
    void ProcessSessionBatch(std::span<BotSession*> batch, uint32 diff);

    // === MEMORY OPTIMIZATION SYSTEM ===

    // Hibernation management for memory efficiency
    void HibernateInactiveSessions();
    void ReactivateSessionsForMap(uint32 mapId);
    void ReactivateSessionsForGroup(uint32 groupId);

    // Memory defragmentation and cleanup
    void DefragmentMemory();
    void CollectGarbage();

    // === PERFORMANCE MONITORING ===

    struct GlobalMetrics {
        std::atomic<uint32> activeSessions{0};
        std::atomic<uint32> hibernatedSessions{0};
        std::atomic<uint32> totalSessions{0};
        std::atomic<uint64> packetsPerSecond{0};
        std::atomic<uint64> bytesPerSecond{0};
        std::atomic<size_t> totalMemoryUsage{0};
        std::atomic<uint32> cpuTimeUs{0};
        std::atomic<uint32> hibernationEvents{0};
        std::atomic<uint32> reactivationEvents{0};
        std::atomic<uint32> sessionCreations{0};
        std::atomic<uint32> sessionDeletions{0};
    };

    GlobalMetrics const& GetGlobalMetrics() const { return _globalMetrics; }

    // Performance queries
    uint32 GetActiveSessionCount() const { return _globalMetrics.activeSessions.load(); }
    uint32 GetHibernatedSessionCount() const { return _globalMetrics.hibernatedSessions.load(); }
    double GetAverageCpuPercentage() const;
    size_t GetTotalMemoryUsage() const { return _globalMetrics.totalMemoryUsage.load(); }

    // Session queries
    std::vector<BotSession*> GetActiveSessions() const;
    std::vector<BotSession*> GetHibernatedSessions() const;
    std::vector<BotSession*> GetSessionsForMap(uint32 mapId) const;

    // Configuration
    void SetMaxSessions(uint32 maxSessions) { _maxSessions = maxSessions; }
    void SetBatchSize(uint32 batchSize) { _batchSize = std::min(batchSize, MAX_BATCH_SIZE); }
    void SetHibernationThreshold(std::chrono::minutes threshold) { _hibernationThreshold = threshold; }

    // Administrative commands
    bool IsEnabled() const { return _enabled; }
    void SetEnabled(bool enabled) { _enabled = enabled; }

private:
    BotSessionMgr() = default;
    ~BotSessionMgr() = default;
    BotSessionMgr(BotSessionMgr const&) = delete;
    BotSessionMgr& operator=(BotSessionMgr const&) = delete;

    // === CORE DATA STRUCTURES ===

    // High-performance parallel hashmap with 8 submaps for optimal concurrency
    using SessionMap = phmap::parallel_flat_hash_map<
        uint32,                                    // BattleNet account ID
        std::unique_ptr<BotSession>,              // Session pointer
        std::hash<uint32>,                        // Hash function
        std::equal_to<>,                          // Key equality
        std::allocator<std::pair<uint32, std::unique_ptr<BotSession>>>,
        8,                                        // 8 submaps for optimal concurrency
        std::shared_mutex                         // Shared mutex for read-heavy operations
    >;

    SessionMap _sessions;

    // TBB concurrent vector for lock-free session list operations
    tbb::concurrent_vector<BotSession*> _activeSessions;
    tbb::concurrent_vector<BotSession*> _hibernatedSessions;

    // Object pool for session pre-allocation
    boost::lockfree::stack<BotSession*> _sessionPool{1000}; // Pre-allocate 1000 sessions

    // Intel TBB task group for parallel processing
    std::unique_ptr<tbb::task_group> _taskGroup;

    // === PERFORMANCE OPTIMIZATION ===

    // Batch processing configuration
    static constexpr uint32 DEFAULT_BATCH_SIZE = 64;
    static constexpr uint32 MAX_BATCH_SIZE = 256;
    uint32 _batchSize = DEFAULT_BATCH_SIZE;

    // Memory management
    std::chrono::steady_clock::time_point _lastDefragmentation;
    std::chrono::minutes _hibernationThreshold{5};

    // === METRICS AND MONITORING ===

    mutable GlobalMetrics _globalMetrics;
    std::chrono::steady_clock::time_point _lastMetricsUpdate;
    std::chrono::steady_clock::time_point _startTime;

    // === CONFIGURATION ===

    std::atomic<bool> _enabled{false};
    std::atomic<bool> _initialized{false};
    std::atomic<uint32> _maxSessions{5000};

    // === PRIVATE IMPLEMENTATION METHODS ===

    // Session lifecycle
    BotSession* AllocateSession(uint32 bnetAccountId);
    void DeallocateSession(BotSession* session);
    void PreallocateSessionPool();

    // Batch processing optimization
    void UpdateSessionBatches(uint32 diff);
    void ProcessBatchParallel(std::span<BotSession*> batch, uint32 diff);

    // Memory management
    void UpdateHibernationCandidates();
    void PerformMemoryDefragmentation();
    void CompactSessionVectors();

    // Metrics and monitoring
    void UpdateGlobalMetrics();
    void ResetMetrics();
    void AggregateSessionMetrics();

    // Thread safety helpers
    void AddToActiveList(BotSession* session);
    void RemoveFromActiveList(BotSession* session);
    void MoveToHibernatedList(BotSession* session);
    void MoveToActiveList(BotSession* session);

    // Performance validation
    void ValidatePerformanceTargets() const;
    void LogPerformanceWarnings() const;
};

} // namespace Playerbot

// Global access macro for convenience
#define sBotSessionMgr Playerbot::BotSessionMgr::instance()

#endif // BOT_SESSION_MGR_H