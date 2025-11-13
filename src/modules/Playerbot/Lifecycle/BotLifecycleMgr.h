/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
// Forward declaration - BotScheduler.h included in .cpp
// Forward declaration - BotSpawner.h included in .cpp
#include "DatabaseEnv.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IBotLifecycleMgr.h"
#include <shared_mutex>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
// Forward declarations
namespace Playerbot
{
    class BotScheduler;
    class BotSpawner;
}

struct LifecycleEventInfo
{
    enum Type
    {
        SCHEDULER_LOGIN,
        SCHEDULER_LOGOUT,
        SPAWNER_SUCCESS,
        SPAWNER_FAILURE,
        POPULATION_UPDATE,
        SYSTEM_SHUTDOWN,
        MAINTENANCE_REQUIRED
    };

    Type eventType;
    ObjectGuid botGuid;
    uint32 accountId;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    uint32 processingTimeMs = 0;
    std::string correlationId;
};

// Performance monitoring metrics
struct PerformanceMetrics
{
    std::atomic<uint32> totalBotsManaged{0};
    std::atomic<uint32> activeBots{0};
    std::atomic<uint32> scheduledBots{0};
    std::atomic<uint32> eventsProcessedPerSecond{0};
    std::atomic<uint32> averageEventProcessingTimeMs{0};
    std::atomic<uint32> failedSpawnsLastHour{0};
    std::atomic<float> systemCpuUsage{0.0f};
    std::atomic<uint64> memoryUsageMB{0};

    std::chrono::system_clock::time_point lastUpdate;
    uint32 eventCountThisSecond = 0;
    uint32 totalProcessingTimeThisSecond = 0;
};

// Lifecycle statistics
struct LifecycleStatistics
{
    uint32 totalLifecycleEvents = 0;
    uint32 successfulSpawns = 0;
    uint32 failedSpawns = 0;
    uint32 scheduledLogins = 0;
    uint32 scheduledLogouts = 0;
    uint32 populationUpdates = 0;
    uint32 maintenanceRuns = 0;
    float averageResponseTimeMs = 0.0f;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastUpdate;
};

class TC_GAME_API BotLifecycleMgr final : public IBotLifecycleMgr
{
public:
    static BotLifecycleMgr* instance();

    // Core lifecycle management
    bool Initialize() override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    // Event-driven coordination
    void ProcessSchedulerEvents() override;
    void ProcessSpawnerEvents() override;
    void ProcessMaintenanceEvents() override;

    // Lifecycle coordination
    void OnBotLoginRequested(ObjectGuid guid, std::string const& pattern) override;
    void OnBotLogoutRequested(ObjectGuid guid, std::string const& reason) override;
    void OnBotSpawnSuccess(ObjectGuid guid, uint32 accountId) override;
    void OnBotSpawnFailure(ObjectGuid guid, std::string const& reason) override;
    void OnPopulationTargetChanged(uint32 zoneId, uint32 targetPopulation) override;

    // Population management
    void UpdateZonePopulations() override;
    void BalancePopulation() override;
    void HandlePopulationPressure() override;

    // Performance monitoring
    PerformanceMetrics const& GetPerformanceMetrics() const override { return _metrics; }
    void LogPerformanceReport() override;

    // Configuration and control
    void SetEnabled(bool enabled) override { _enabled = enabled; }
    bool IsEnabled() const override { return _enabled; }

    void SetMaxConcurrentOperations(uint32 maxOps) override { _maxConcurrentOperations = maxOps; }
    uint32 GetMaxConcurrentOperations() const override { return _maxConcurrentOperations; }

    void SetUpdateIntervalMs(uint32 intervalMs) override { _updateIntervalMs = intervalMs; }
    uint32 GetUpdateIntervalMs() const override { return _updateIntervalMs; }

    // Maintenance and health
    bool IsHealthy() const override;
    void RunMaintenance() override;
    void EmergencyShutdown() override;

    // Statistics and reporting
    LifecycleStatistics GetStatistics() const override { return _statistics; }
    void ResetStatistics() override;

    // Event subscription system
    using EventHandler = std::function<void(LifecycleEventInfo const&)>;
    uint32 RegisterEventHandler(LifecycleEventInfo::Type eventType, EventHandler handler) override;
    void UnregisterEventHandler(uint32 handlerId) override;

private:
    BotLifecycleMgr() = default;
    ~BotLifecycleMgr() = default;

    // Core components (singleton references)
    Playerbot::BotScheduler* _scheduler;
    Playerbot::BotSpawner* _spawner;

    // Event processing (TBB removed - using std:: equivalents)
    std::queue<LifecycleEventInfo> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_SPAWNER> _eventQueueMutex;

    // Thread management
    std::unique_ptr<std::thread> _workerThread;
    std::atomic<bool> _running{false};
    std::atomic<bool> _enabled{true};

    // Performance and configuration
    mutable PerformanceMetrics _metrics;
    mutable LifecycleStatistics _statistics;

    uint32 _updateIntervalMs = 1000; // 1 second default
    uint32 _maxConcurrentOperations = 10;
    uint32 _maintenanceIntervalMinutes = 30;

    // Event subscription
    struct EventSubscription
    {
        uint32 id;
        LifecycleEventInfo::Type eventType;
        EventHandler handler;
    };

    std::vector<EventSubscription> _eventHandlers;
    std::atomic<uint32> _nextHandlerId{1};
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_SPAWNER> _handlersMutex;

    // Internal processing
    void WorkerThreadProc();
    void ProcessEventQueue();
    void ProcessEvent(LifecycleEventInfo const& eventInfo);

    void NotifyEventHandlers(LifecycleEventInfo const& eventInfo);

    void UpdatePerformanceMetrics();
    void RecordEvent(LifecycleEventInfo::Type eventType, uint32 processingTimeMs);

    // Database operations
    void LogLifecycleEvent(LifecycleEventInfo const& eventInfo);
    void UpdateZonePopulationInDatabase(uint32 zoneId, uint32 currentBots);

    // Population coordination
    void CoordinateSchedulerAndSpawner();
    void HandlePopulationImbalance(uint32 zoneId, int32 difference);

    // Maintenance operations
    void CleanupOldEvents();
    void OptimizePerformance();
    void ValidateSystemHealth();

    // Timing and coordination
    std::chrono::system_clock::time_point _lastUpdate;
    std::chrono::system_clock::time_point _lastMaintenance;
    std::chrono::system_clock::time_point _startTime;

    // Health monitoring
    std::atomic<uint32> _consecutiveErrors{0};
    std::atomic<uint32> _healthCheckFailures{0};
    static constexpr uint32 MAX_CONSECUTIVE_ERRORS = 10;
    static constexpr uint32 MAX_HEALTH_CHECK_FAILURES = 5;

    // Correlation tracking
    std::string GenerateCorrelationId();
    std::unordered_map<std::string, std::vector<LifecycleEventInfo>> _correlatedEvents;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_SPAWNER> _correlationMutex;
};

// Lifecycle event logging macros
#define LIFECYCLE_LOG_INFO(message, ...) \
    TC_LOG_INFO("playerbots.lifecycle", "[BotLifecycleMgr] " message, ##__VA_ARGS__)

#define LIFECYCLE_LOG_ERROR(message, ...) \
    TC_LOG_ERROR("playerbots.lifecycle", "[BotLifecycleMgr] ERROR: " message, ##__VA_ARGS__)

#define LIFECYCLE_LOG_WARN(message, ...) \
    TC_LOG_WARN("playerbots.lifecycle", "[BotLifecycleMgr] WARNING: " message, ##__VA_ARGS__)

#define LIFECYCLE_LOG_DEBUG(message, ...) \
    TC_LOG_DEBUG("playerbots.lifecycle", "[BotLifecycleMgr] " message, ##__VA_ARGS__)