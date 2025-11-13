/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AdaptiveSpawnThrottler.h"
#include "Config/PlayerbotConfig.h"
#include "Log.h"
#include "GameTime.h"
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// ThrottlerConfig Implementation
// ============================================================================

void ThrottlerConfig::LoadFromConfig()
{
    baseSpawnIntervalMs = sPlayerbotConfig->GetInt("Playerbot.Throttler.BaseSpawnIntervalMs", 100);
    minSpawnIntervalMs = sPlayerbotConfig->GetInt("Playerbot.Throttler.MinSpawnIntervalMs", 50);
    maxSpawnIntervalMs = sPlayerbotConfig->GetInt("Playerbot.Throttler.MaxSpawnIntervalMs", 5000);

    normalPressureMultiplier = sPlayerbotConfig->GetFloat("Playerbot.Throttler.PressureMultiplier.Normal", 1.0f);
    elevatedPressureMultiplier = sPlayerbotConfig->GetFloat("Playerbot.Throttler.PressureMultiplier.Elevated", 2.0f);
    highPressureMultiplier = sPlayerbotConfig->GetFloat("Playerbot.Throttler.PressureMultiplier.High", 4.0f);
    criticalPressureMultiplier = sPlayerbotConfig->GetFloat("Playerbot.Throttler.PressureMultiplier.Critical", 0.0f);

    burstWindowMs = sPlayerbotConfig->GetInt("Playerbot.Throttler.BurstWindow.Seconds", 10) * 1000;
    maxBurstsPerWindow = sPlayerbotConfig->GetInt("Playerbot.Throttler.BurstWindow.Requests", 50);

    enableAdaptiveThrottling = sPlayerbotConfig->GetBool("Playerbot.Throttler.EnableAdaptive", true);
    enableCircuitBreaker = sPlayerbotConfig->GetBool("Playerbot.Throttler.EnableCircuitBreaker", true);
    enableBurstPrevention = sPlayerbotConfig->GetBool("Playerbot.Throttler.EnableBurstPrevention", true);

    TC_LOG_INFO("module.playerbot.throttler",
        "AdaptiveSpawnThrottler config loaded: Base={}ms, Range=[{}-{}ms], Adaptive={}, CircuitBreaker={}, BurstPrevention={}",
        baseSpawnIntervalMs, minSpawnIntervalMs, maxSpawnIntervalMs,
        enableAdaptiveThrottling, enableCircuitBreaker, enableBurstPrevention);
}

// ============================================================================
// AdaptiveSpawnThrottler Implementation
// ============================================================================

bool AdaptiveSpawnThrottler::Initialize(ResourceMonitor* resourceMonitor, SpawnCircuitBreaker* circuitBreaker)
{
    if (_initialized)
        return true;

    if (!resourceMonitor || !circuitBreaker)
    {
        TC_LOG_ERROR("module.playerbot.throttler",
            "AdaptiveSpawnThrottler::Initialize() called with null dependencies");
        return false;
    }

    TC_LOG_INFO("module.playerbot.throttler", "Initializing AdaptiveSpawnThrottler...");

    // Store dependencies
    _resourceMonitor = resourceMonitor;
    _circuitBreaker = circuitBreaker;

    // Load configuration
    _config.LoadFromConfig();

    // Initialize state
    _lastSpawnTime = GameTime::Now();
    _currentSpawnInterval = _config.baseSpawnIntervalMs;

    _initialized = true;
    TC_LOG_INFO("module.playerbot.throttler", " AdaptiveSpawnThrottler initialized successfully");
    return true;
}

void AdaptiveSpawnThrottler::Update(uint32 diff)
{
    if (!_initialized)
        return;

    // Update burst tracking (remove old entries)
    UpdateBurstTracking();

    // Recalculate spawn interval based on current conditions
    if (_config.enableAdaptiveThrottling)
        RecalculateInterval();

    // Reset spawn counter for this update cycle
    _spawnsSinceLastUpdate = 0;
}

bool AdaptiveSpawnThrottler::CanSpawnNow() const
{
    if (!_initialized)
        return true;  // Default allow if not initialized

    // 1. Check circuit breaker (highest priority check)
    if (_config.enableCircuitBreaker && _circuitBreaker)
    {
        if (!_circuitBreaker->AllowSpawn())
        {
            TC_LOG_TRACE("module.playerbot.throttler",
                "Spawn blocked by circuit breaker (state: {})",
                GetCircuitStateName(_circuitBreaker->GetState()));
            return false;
        }
    }

    // 2. Check if in burst prevention mode
    if (_config.enableBurstPrevention && IsInBurstPrevention())
    {
        TC_LOG_TRACE("module.playerbot.throttler",
            "Spawn blocked by burst prevention ({} recent spawns in {}ms window)",
            _recentSpawnTimes.size(), _config.burstWindowMs);
        return false;
    }

    // 3. Check if enough time passed since last spawn
    Milliseconds timeSinceLastSpawn = std::chrono::duration_cast<Milliseconds>(
        GameTime::Now() - _lastSpawnTime);

    if (timeSinceLastSpawn < Milliseconds(_currentSpawnInterval))
    {
        TC_LOG_TRACE("module.playerbot.throttler",
            "Spawn throttled: {}ms since last spawn, {}ms interval required",
            timeSinceLastSpawn.count(), _currentSpawnInterval);
        return false;
    }

    return true;
}

void AdaptiveSpawnThrottler::RecordSpawnSuccess()
{
    if (!_initialized)
        return;

    // Update last spawn time
    TimePoint now = GameTime::Now();
    _lastSpawnTime = now;

    // Add to burst tracking window
    _recentSpawnTimes.push_back(now);

    // Update metrics
    ++_spawnsSinceLastUpdate;

    TC_LOG_TRACE("module.playerbot.throttler",
        "Spawn success recorded (interval: {}ms, recent spawns: {})",
        _currentSpawnInterval, _recentSpawnTimes.size());
}

void AdaptiveSpawnThrottler::RecordSpawnFailure(std::string_view reason)
{
    if (!_initialized)
        return;

    // Forward to circuit breaker
    if (_circuitBreaker)
        _circuitBreaker->RecordFailure(reason);

    // Update metrics
    ++_totalSpawnsThrottled;

    TC_LOG_DEBUG("module.playerbot.throttler",
        "Spawn failure recorded{}{} (total throttled: {})",
        reason.empty() ? "" : " - Reason: ",
        reason,
        _totalSpawnsThrottled);
}

float AdaptiveSpawnThrottler::GetCurrentSpawnRate() const
{
    if (_currentSpawnInterval == 0)
        return 0.0f;

    // Convert interval to rate: rate = 1000ms / intervalMs
    return 1000.0f / static_cast<float>(_currentSpawnInterval);
}

Milliseconds AdaptiveSpawnThrottler::GetTimeUntilNextSpawn() const
{
    if (!_initialized)
        return Milliseconds(0);

    Milliseconds timeSinceLastSpawn = std::chrono::duration_cast<Milliseconds>(
        GameTime::Now() - _lastSpawnTime);

    Milliseconds intervalMs = Milliseconds(_currentSpawnInterval);

    if (timeSinceLastSpawn >= intervalMs)
        return Milliseconds(0);

    return intervalMs - timeSinceLastSpawn;
}

ThrottlerMetrics AdaptiveSpawnThrottler::GetMetrics() const
{
    ThrottlerMetrics metrics;

    metrics.currentSpawnIntervalMs = _currentSpawnInterval;
    metrics.currentSpawnRatePerSec = GetCurrentSpawnRate();
    metrics.effectiveMultiplier = GetPressureMultiplier() * GetCircuitBreakerMultiplier();

    if (_resourceMonitor)
        metrics.currentPressure = _resourceMonitor->GetPressureLevel();

    if (_circuitBreaker)
        metrics.circuitState = _circuitBreaker->GetState();

    metrics.spawnsSinceLastUpdate = _spawnsSinceLastUpdate;
    metrics.totalSpawnsThrottled = _totalSpawnsThrottled;
    metrics.burstPreventionTriggers = _burstPreventionCount;

    metrics.timeSinceLastSpawn = std::chrono::duration_cast<Milliseconds>(
        GameTime::Now() - _lastSpawnTime);

    // Calculate average spawn interval from recent spawns
    if (_recentSpawnTimes.size() >= 2)
    {
        Milliseconds totalDuration = std::chrono::duration_cast<Milliseconds>(
            _recentSpawnTimes.back() - _recentSpawnTimes.front());
        metrics.averageSpawnInterval = Milliseconds(
            totalDuration.count() / (_recentSpawnTimes.size() - 1));
    }

    return metrics;
}

void AdaptiveSpawnThrottler::RecalculateInterval()
{
    uint32 newInterval = CalculateSpawnInterval();

    if (newInterval != _currentSpawnInterval)
    {
        TC_LOG_DEBUG("module.playerbot.throttler",
            "Spawn interval adjusted: {}ms → {}ms (rate: {:.1f} → {:.1f} bots/sec)",
            _currentSpawnInterval, newInterval,
            1000.0f / _currentSpawnInterval, 1000.0f / newInterval);

        _currentSpawnInterval = newInterval;
    }
}

uint32 AdaptiveSpawnThrottler::CalculateSpawnInterval() const
{
    if (!_initialized)
        return _config.baseSpawnIntervalMs;

    // Start with base interval
    float interval = static_cast<float>(_config.baseSpawnIntervalMs);

    // Apply pressure multiplier (reduces spawn rate under pressure)
    float pressureMultiplier = GetPressureMultiplier();
    if (pressureMultiplier > 0.0f)
        interval = interval / pressureMultiplier;  // Lower multiplier = longer interval
    else
        interval = static_cast<float>(_config.maxSpawnIntervalMs);  // Critical pressure: max interval

    // Apply circuit breaker multiplier
    float circuitMultiplier = GetCircuitBreakerMultiplier();
    if (circuitMultiplier > 0.0f)
        interval = interval / circuitMultiplier;
    else
        interval = static_cast<float>(_config.maxSpawnIntervalMs);  // Circuit OPEN: max interval

    // Clamp to configured min/max
    uint32 finalInterval = static_cast<uint32>(interval);
    finalInterval = std::clamp(finalInterval, _config.minSpawnIntervalMs, _config.maxSpawnIntervalMs);

    return finalInterval;
}

float AdaptiveSpawnThrottler::GetPressureMultiplier() const
{
    if (!_resourceMonitor)
        return _config.normalPressureMultiplier;

    ResourcePressure pressure = _resourceMonitor->GetPressureLevel();

    switch (pressure)
    {
        case ResourcePressure::NORMAL:
            return _config.normalPressureMultiplier;
        case ResourcePressure::ELEVATED:
            return _config.elevatedPressureMultiplier;
        case ResourcePressure::HIGH:
            return _config.highPressureMultiplier;
        case ResourcePressure::CRITICAL:
            return _config.criticalPressureMultiplier;
        default:
            return 1.0f;
    }
}

float AdaptiveSpawnThrottler::GetCircuitBreakerMultiplier() const
{
    if (!_circuitBreaker || !_config.enableCircuitBreaker)
        return 1.0f;

    CircuitState state = _circuitBreaker->GetState();

    switch (state)
    {
        case CircuitState::CLOSED:
            return 1.0f;    // Normal operation (100%)
        case CircuitState::HALF_OPEN:
            return 0.5f;    // Limited spawning (50%)
        case CircuitState::OPEN:
            return 0.0f;    // Blocked (0%)
        default:
            return 1.0f;
    }
}

bool AdaptiveSpawnThrottler::IsInBurstPrevention() const
{
    if (!_config.enableBurstPrevention)
        return false;

    // Check if recent spawn count exceeds burst threshold
    return _recentSpawnTimes.size() >= _config.maxBurstsPerWindow;
}

void AdaptiveSpawnThrottler::UpdateBurstTracking()
{
    if (_recentSpawnTimes.empty())
        return;

    TimePoint now = GameTime::Now();
    TimePoint cutoffTime = now - Milliseconds(_config.burstWindowMs);

    // Remove spawn timestamps older than burst window
    size_t initialSize = _recentSpawnTimes.size();

    while (!_recentSpawnTimes.empty() &&
           _recentSpawnTimes.front() < cutoffTime)
    {
        _recentSpawnTimes.pop_front();
    }

    // Track burst prevention triggers
    if (initialSize >= _config.maxBurstsPerWindow &&
        _recentSpawnTimes.size() < _config.maxBurstsPerWindow)
    {
        ++_burstPreventionCount;
        TC_LOG_DEBUG("module.playerbot.throttler",
            "Burst prevention deactivated ({} spawns cleared from {}ms window)",
            initialSize - _recentSpawnTimes.size(), _config.burstWindowMs);
    }
}

} // namespace Playerbot
