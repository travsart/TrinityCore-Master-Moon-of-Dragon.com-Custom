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
#include "ObjectGuid.h"
#include <string>
#include <functional>
#include <chrono>

enum class LifecycleEventType
{
    SCHEDULER_LOGIN,
    SCHEDULER_LOGOUT,
    SPAWNER_SUCCESS,
    SPAWNER_FAILURE,
    POPULATION_UPDATE,
    SYSTEM_SHUTDOWN,
    MAINTENANCE_REQUIRED
};

struct LifecycleEventInfo
{
    using Type = LifecycleEventType;
};

/**
 * @brief Interface for bot lifecycle coordination
 *
 * Coordinates bot spawning, scheduling, and population management with
 * event-driven architecture and performance monitoring.
 */
class TC_GAME_API IBotLifecycleMgr
{
public:
    struct PerformanceMetrics
    {
        uint32 totalBotsManaged;
        uint32 activeBots;
        uint32 scheduledBots;
        uint32 eventsProcessedPerSecond;
        uint32 averageEventProcessingTimeMs;
        uint32 failedSpawnsLastHour;
        float systemCpuUsage;
        uint64 memoryUsageMB;
        uint32 eventCountThisSecond;
        uint32 totalProcessingTimeThisSecond;
        std::chrono::system_clock::time_point lastUpdate;
    };

    struct LifecycleStatistics
    {
        uint32 totalLifecycleEvents;
        uint32 successfulSpawns;
        uint32 failedSpawns;
        uint32 scheduledLogins;
        uint32 scheduledLogouts;
        uint32 populationUpdates;
        uint32 maintenanceRuns;
        float averageResponseTimeMs;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point lastUpdate;
    };

    using EventHandler = std::function<void(LifecycleEventInfo const&)>;

    virtual ~IBotLifecycleMgr() = default;

    // Core lifecycle management
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // Event-driven coordination
    virtual void ProcessSchedulerEvents() = 0;
    virtual void ProcessSpawnerEvents() = 0;
    virtual void ProcessMaintenanceEvents() = 0;

    // Lifecycle coordination
    virtual void OnBotLoginRequested(ObjectGuid guid, std::string const& pattern) = 0;
    virtual void OnBotLogoutRequested(ObjectGuid guid, std::string const& reason) = 0;
    virtual void OnBotSpawnSuccess(ObjectGuid guid, uint32 accountId) = 0;
    virtual void OnBotSpawnFailure(ObjectGuid guid, std::string const& reason) = 0;
    virtual void OnPopulationTargetChanged(uint32 zoneId, uint32 targetPopulation) = 0;

    // Population management
    virtual void UpdateZonePopulations() = 0;
    virtual void BalancePopulation() = 0;
    virtual void HandlePopulationPressure() = 0;

    // Performance monitoring
    virtual PerformanceMetrics const& GetPerformanceMetrics() const = 0;
    virtual void LogPerformanceReport() = 0;

    // Configuration and control
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetMaxConcurrentOperations(uint32 maxOps) = 0;
    virtual uint32 GetMaxConcurrentOperations() const = 0;
    virtual void SetUpdateIntervalMs(uint32 intervalMs) = 0;
    virtual uint32 GetUpdateIntervalMs() const = 0;

    // Maintenance and health
    virtual bool IsHealthy() const = 0;
    virtual void RunMaintenance() = 0;
    virtual void EmergencyShutdown() = 0;

    // Statistics and reporting
    virtual LifecycleStatistics GetStatistics() const = 0;
    virtual void ResetStatistics() = 0;

    // Event subscription system
    virtual uint32 RegisterEventHandler(LifecycleEventInfo::Type eventType, EventHandler handler) = 0;
    virtual void UnregisterEventHandler(uint32 handlerId) = 0;
};
