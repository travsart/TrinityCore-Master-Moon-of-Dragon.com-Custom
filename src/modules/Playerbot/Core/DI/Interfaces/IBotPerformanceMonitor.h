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
#include <string>

namespace Playerbot
{

// Forward declarations
struct SystemPerformanceMetrics;
class UpdateTimeHistogram;

/**
 * @brief Interface for bot performance monitoring and auto-scaling
 *
 * Provides comprehensive performance monitoring for 5000+ concurrent bots with
 * real-time metrics collection, histogram tracking, automatic load shedding,
 * and performance degradation detection.
 */
class TC_GAME_API IBotPerformanceMonitor
{
public:
    virtual ~IBotPerformanceMonitor() = default;

    // Initialization
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Tick monitoring
    virtual void BeginTick(uint32 currentTime) = 0;
    virtual void EndTick(uint32 currentTime, uint32 botsUpdated, uint32 botsSkipped) = 0;

    // Performance metrics
    virtual void RecordBotUpdateTime(uint32 microseconds) = 0;
    virtual SystemPerformanceMetrics const& GetMetrics() const = 0;

    // Auto-scaling
    virtual void CheckPerformanceThresholds() = 0;
    virtual void TriggerLoadShedding(uint32 targetReduction) = 0;
    virtual void TriggerLoadRecovery(uint32 targetIncrease) = 0;

    // Degradation detection
    virtual bool IsPerformanceDegraded() const = 0;
    virtual bool IsSystemOverloaded() const = 0;
    virtual float GetCurrentLoad() const = 0;

    // Configuration
    virtual void SetTargetTickTime(uint32 microseconds) = 0;
    virtual void SetMaxTickTime(uint32 microseconds) = 0;
    virtual void SetLoadShedThreshold(uint32 microseconds) = 0;
    virtual void SetAutoScalingEnabled(bool enabled) = 0;

    virtual uint32 GetTargetTickTime() const = 0;
    virtual uint32 GetMaxTickTime() const = 0;
    virtual bool IsAutoScalingEnabled() const = 0;

    // Histogram access
    virtual UpdateTimeHistogram const& GetHistogram() const = 0;

    // Statistics and logging
    virtual void LogPerformanceReport() const = 0;
    virtual void LogDetailedStatistics() const = 0;
    virtual void ResetStatistics() = 0;
};

} // namespace Playerbot
