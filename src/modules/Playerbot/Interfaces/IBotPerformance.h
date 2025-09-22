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
#include <memory>
#include <string>
#include <chrono>

namespace Playerbot
{

/**
 * @struct PerformanceMetrics
 * @brief Standard performance metrics structure
 */
struct PerformanceMetrics
{
    float avgSpawnLatency = 0.0f;           // Average spawn time in milliseconds
    float avgDatabaseLatency = 0.0f;        // Average database query time in milliseconds
    float avgCallbackLatency = 0.0f;        // Average callback processing time in milliseconds
    float avgLockWaitTime = 0.0f;           // Average lock wait time in milliseconds

    uint32 spawnRequestsPerSec = 0;         // Spawn requests per second
    uint32 successfulSpawnsPerSec = 0;      // Successful spawns per second
    uint32 failedSpawnsPerSec = 0;          // Failed spawns per second
    float spawnSuccessRate = 1.0f;          // Success rate (0.0 - 1.0)

    uint32 activeBotCount = 0;              // Current active bot count
    uint32 memoryPerBotMB = 0;              // Memory usage per bot in MB
    float cpuUsagePercent = 0.0f;           // CPU usage percentage

    bool scalabilityHealthy = true;         // Overall scalability health
    std::string performanceStatus = "HEALTHY"; // Performance status string
};

/**
 * @interface IBotPerformanceMonitor
 * @brief Abstract interface for bot performance monitoring
 *
 * ABSTRACTION LAYER: Provides clean interface for performance monitoring
 * that can be implemented by different monitoring strategies.
 *
 * Benefits:
 * - Clean separation of performance tracking from implementation
 * - Easy testing through mocking
 * - Support for different monitoring backends
 * - Clear API contract for performance consumers
 * - Facilitates performance data aggregation
 */
class IBotPerformanceMonitor
{
public:
    virtual ~IBotPerformanceMonitor() = default;

    // === LIFECYCLE ===
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // === PERFORMANCE RECORDING ===
    virtual void RecordSpawnLatency(uint64 microseconds) = 0;
    virtual void RecordDatabaseLatency(uint64 microseconds) = 0;
    virtual void RecordCallbackLatency(uint64 microseconds) = 0;
    virtual void RecordLockWaitTime(uint64 microseconds) = 0;

    // === THROUGHPUT TRACKING ===
    virtual void RecordSpawnRequest() = 0;
    virtual void RecordSuccessfulSpawn() = 0;
    virtual void RecordFailedSpawn() = 0;

    // === RESOURCE MONITORING ===
    virtual void RecordMemoryUsage(uint32 botCount, uint64 totalMemoryBytes) = 0;
    virtual void RecordCpuUsage(float cpuPercent) = 0;

    // === METRICS RETRIEVAL ===
    virtual PerformanceMetrics GetSnapshot() const = 0;
    virtual bool IsPerformanceHealthy() const = 0;
    virtual std::string GetPerformanceStatus() const = 0;

    // === STATISTICS MANAGEMENT ===
    virtual void ResetCounters() = 0;

    // === TIMER CREATION ===
    virtual std::unique_ptr<class IBotPerformanceTimer> CreateSpawnTimer() = 0;
    virtual std::unique_ptr<class IBotPerformanceTimer> CreateDatabaseTimer() = 0;
    virtual std::unique_ptr<class IBotPerformanceTimer> CreateCallbackTimer() = 0;
};

/**
 * @interface IBotPerformanceTimer
 * @brief RAII timer interface for automatic performance measurement
 */
class IBotPerformanceTimer
{
public:
    virtual ~IBotPerformanceTimer() = default;

    // Timer automatically records duration on destruction
    virtual void Cancel() = 0;  // Cancel recording
    virtual uint64 GetElapsedMicroseconds() const = 0;
};

/**
 * @interface IBotResourceMonitor
 * @brief Abstract interface for bot resource monitoring
 */
class IBotResourceMonitor
{
public:
    virtual ~IBotResourceMonitor() = default;

    // === RESOURCE TRACKING ===
    virtual void RecordMemoryAllocation(size_t bytes) = 0;
    virtual void RecordMemoryDeallocation(size_t bytes) = 0;
    virtual void RecordCpuTime(uint64 microseconds) = 0;

    // === RESOURCE QUERIES ===
    virtual uint64 GetTotalMemoryUsage() const = 0;
    virtual uint64 GetMemoryUsagePerBot() const = 0;
    virtual float GetCpuUsagePercent() const = 0;
    virtual uint32 GetActiveThreadCount() const = 0;

    // === RESOURCE LIMITS ===
    virtual bool IsMemoryUsageAcceptable() const = 0;
    virtual bool IsCpuUsageAcceptable() const = 0;
    virtual bool CanAllocateMore() const = 0;
};

/**
 * @interface IBotMetricsCollector
 * @brief Abstract interface for collecting and aggregating bot metrics
 */
class IBotMetricsCollector
{
public:
    virtual ~IBotMetricsCollector() = default;

    // === METRICS COLLECTION ===
    virtual void CollectMetrics() = 0;
    virtual void CollectSystemMetrics() = 0;
    virtual void CollectBotMetrics() = 0;

    // === METRICS EXPORT ===
    virtual std::string ExportMetricsAsJson() const = 0;
    virtual std::string ExportMetricsAsPrometheus() const = 0;
    virtual void WriteMetricsToFile(std::string const& filename) const = 0;

    // === HISTORICAL DATA ===
    virtual void StoreHistoricalSnapshot() = 0;
    virtual std::vector<PerformanceMetrics> GetHistoricalData(std::chrono::minutes duration) const = 0;
};

} // namespace Playerbot