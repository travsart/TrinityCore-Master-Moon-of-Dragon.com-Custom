/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file PoolStatistics.h
 * @brief Statistics and metrics structures for Instance Bot Pool
 *
 * This file defines comprehensive statistics tracking for:
 * - Pool slot utilization
 * - Per-role and per-faction metrics
 * - Assignment activity
 * - Performance timing
 * - Historical data
 *
 * Statistics are designed for:
 * - Operational monitoring
 * - Capacity planning
 * - Performance optimization
 * - Administrator dashboards
 */

#pragma once

#include "Define.h"
#include "PoolSlotState.h"
#include <array>
#include <chrono>
#include <string>

namespace Playerbot
{

// ============================================================================
// SLOT STATISTICS
// ============================================================================

/**
 * @brief Statistics for pool slot states
 *
 * Tracks how many slots are in each state for capacity monitoring.
 */
struct SlotStateStats
{
    /// Slots by state
    uint32 emptySlots = 0;
    uint32 creatingSlots = 0;
    uint32 warmingSlots = 0;
    uint32 readySlots = 0;
    uint32 reservedSlots = 0;
    uint32 assignedSlots = 0;
    uint32 cooldownSlots = 0;
    uint32 maintenanceSlots = 0;

    /**
     * @brief Get total tracked slots
     */
    uint32 GetTotal() const
    {
        return emptySlots + creatingSlots + warmingSlots + readySlots +
               reservedSlots + assignedSlots + cooldownSlots + maintenanceSlots;
    }

    /**
     * @brief Get available slots (ready for assignment)
     */
    uint32 GetAvailable() const
    {
        return readySlots;
    }

    /**
     * @brief Get in-use slots (reserved or assigned)
     */
    uint32 GetInUse() const
    {
        return reservedSlots + assignedSlots;
    }

    /**
     * @brief Get transitional slots (creating, warming, cooldown, maintenance)
     */
    uint32 GetTransitional() const
    {
        return creatingSlots + warmingSlots + cooldownSlots + maintenanceSlots;
    }

    /**
     * @brief Get utilization percentage
     * @return Percentage of non-empty slots that are in-use
     */
    float GetUtilizationPct() const
    {
        uint32 total = GetTotal() - emptySlots;
        if (total == 0)
            return 0.0f;
        return (static_cast<float>(GetInUse()) / static_cast<float>(total)) * 100.0f;
    }

    /**
     * @brief Get availability percentage
     * @return Percentage of pool that is ready for assignment
     */
    float GetAvailabilityPct() const
    {
        uint32 total = GetTotal() - emptySlots;
        if (total == 0)
            return 0.0f;
        return (static_cast<float>(readySlots) / static_cast<float>(total)) * 100.0f;
    }

    /**
     * @brief Reset all counters
     */
    void Reset()
    {
        emptySlots = creatingSlots = warmingSlots = readySlots = 0;
        reservedSlots = assignedSlots = cooldownSlots = maintenanceSlots = 0;
    }
};

// ============================================================================
// ROLE STATISTICS
// ============================================================================

/**
 * @brief Statistics per bot role (Tank/Healer/DPS)
 */
struct RoleStats
{
    BotRole role = BotRole::DPS;

    /// Slot counts
    uint32 totalSlots = 0;
    uint32 readySlots = 0;
    uint32 assignedSlots = 0;
    uint32 reservedSlots = 0;

    /// Activity metrics
    uint32 assignmentsThisHour = 0;
    uint32 assignmentsToday = 0;

    /// Timing metrics
    std::chrono::milliseconds avgAssignmentTime{0};
    std::chrono::milliseconds avgInstanceDuration{0};

    /**
     * @brief Get availability percentage
     */
    float GetAvailabilityPct() const
    {
        if (totalSlots == 0)
            return 0.0f;
        return (static_cast<float>(readySlots) / static_cast<float>(totalSlots)) * 100.0f;
    }

    /**
     * @brief Reset hourly counters
     */
    void ResetHourly()
    {
        assignmentsThisHour = 0;
    }

    /**
     * @brief Reset daily counters
     */
    void ResetDaily()
    {
        assignmentsToday = 0;
    }
};

// ============================================================================
// FACTION STATISTICS
// ============================================================================

/**
 * @brief Statistics per faction (Alliance/Horde)
 */
struct FactionStats
{
    Faction faction = Faction::Alliance;

    /// Slot counts
    uint32 totalSlots = 0;
    uint32 readySlots = 0;
    uint32 assignedSlots = 0;

    /// Per-role breakdown
    std::array<RoleStats, static_cast<size_t>(BotRole::Max)> roleStats;

    /// Activity metrics
    uint32 assignmentsThisHour = 0;
    uint32 battlegroundsFilledThisHour = 0;

    /**
     * @brief Get ready count for role
     */
    uint32 GetReadyForRole(BotRole role) const
    {
        auto idx = static_cast<size_t>(role);
        if (idx < roleStats.size())
            return roleStats[idx].readySlots;
        return 0;
    }

    /**
     * @brief Reset hourly counters
     */
    void ResetHourly()
    {
        assignmentsThisHour = 0;
        battlegroundsFilledThisHour = 0;
        for (auto& rs : roleStats)
            rs.ResetHourly();
    }
};

// ============================================================================
// ACTIVITY STATISTICS
// ============================================================================

/**
 * @brief Statistics for pool activity and throughput
 */
struct ActivityStats
{
    // ========================================================================
    // HOURLY METRICS
    // ========================================================================

    /// Assignments this hour
    uint32 assignmentsThisHour = 0;

    /// Releases this hour
    uint32 releasesThisHour = 0;

    /// JIT creations this hour
    uint32 jitCreationsThisHour = 0;

    /// Reservations this hour
    uint32 reservationsThisHour = 0;

    /// Reservation cancellations this hour
    uint32 cancellationsThisHour = 0;

    /// Warmups completed this hour
    uint32 warmupsThisHour = 0;

    /// Cooldowns expired this hour
    uint32 cooldownsExpiredThisHour = 0;

    // ========================================================================
    // DAILY METRICS
    // ========================================================================

    uint32 assignmentsToday = 0;
    uint32 releasesToday = 0;
    uint32 jitCreationsToday = 0;

    // ========================================================================
    // INSTANCE TYPE BREAKDOWN (hourly)
    // ========================================================================

    uint32 dungeonsFilledThisHour = 0;
    uint32 raidsFilledThisHour = 0;
    uint32 battlegroundsFilledThisHour = 0;
    uint32 arenasFilledThisHour = 0;

    // ========================================================================
    // SUCCESS/FAILURE
    // ========================================================================

    uint32 successfulRequestsThisHour = 0;
    uint32 failedRequestsThisHour = 0;
    uint32 timeoutRequestsThisHour = 0;

    /**
     * @brief Get request success rate
     */
    float GetSuccessRatePct() const
    {
        uint32 total = successfulRequestsThisHour + failedRequestsThisHour + timeoutRequestsThisHour;
        if (total == 0)
            return 100.0f;
        return (static_cast<float>(successfulRequestsThisHour) / static_cast<float>(total)) * 100.0f;
    }

    /**
     * @brief Reset hourly counters
     */
    void ResetHourly()
    {
        assignmentsThisHour = releasesThisHour = jitCreationsThisHour = 0;
        reservationsThisHour = cancellationsThisHour = 0;
        warmupsThisHour = cooldownsExpiredThisHour = 0;
        dungeonsFilledThisHour = raidsFilledThisHour = 0;
        battlegroundsFilledThisHour = arenasFilledThisHour = 0;
        successfulRequestsThisHour = failedRequestsThisHour = timeoutRequestsThisHour = 0;
    }

    /**
     * @brief Reset daily counters
     */
    void ResetDaily()
    {
        assignmentsToday = releasesToday = jitCreationsToday = 0;
    }
};

// ============================================================================
// TIMING STATISTICS
// ============================================================================

/**
 * @brief Statistics for timing and performance
 */
struct TimingStats
{
    /// Average time to assign a bot (pool lookup + assignment)
    std::chrono::microseconds avgAssignmentTime{0};

    /// Average time for bot warmup (login)
    std::chrono::milliseconds avgWarmupTime{0};

    /// Average time for JIT bot creation
    std::chrono::milliseconds avgJITCreationTime{0};

    /// Average instance duration
    std::chrono::minutes avgInstanceDuration{0};

    /// Average cooldown time actually used
    std::chrono::seconds avgCooldownTime{0};

    /// Peak assignment time observed
    std::chrono::microseconds peakAssignmentTime{0};

    /// Peak JIT creation time observed
    std::chrono::milliseconds peakJITCreationTime{0};

    // ========================================================================
    // SAMPLE COUNTERS (for calculating averages)
    // ========================================================================

    uint32 assignmentSamples = 0;
    uint32 warmupSamples = 0;
    uint32 jitSamples = 0;
    uint32 instanceSamples = 0;
    uint32 cooldownSamples = 0;

    /**
     * @brief Record an assignment time sample
     */
    void RecordAssignment(std::chrono::microseconds time)
    {
        if (assignmentSamples == 0)
        {
            avgAssignmentTime = time;
        }
        else
        {
            // Running average
            avgAssignmentTime = std::chrono::microseconds(
                (avgAssignmentTime.count() * assignmentSamples + time.count()) /
                (assignmentSamples + 1));
        }
        ++assignmentSamples;

        if (time > peakAssignmentTime)
            peakAssignmentTime = time;
    }

    /**
     * @brief Record a warmup time sample
     */
    void RecordWarmup(std::chrono::milliseconds time)
    {
        if (warmupSamples == 0)
        {
            avgWarmupTime = time;
        }
        else
        {
            avgWarmupTime = std::chrono::milliseconds(
                (avgWarmupTime.count() * warmupSamples + time.count()) /
                (warmupSamples + 1));
        }
        ++warmupSamples;
    }

    /**
     * @brief Record a JIT creation time sample
     */
    void RecordJITCreation(std::chrono::milliseconds time)
    {
        if (jitSamples == 0)
        {
            avgJITCreationTime = time;
        }
        else
        {
            avgJITCreationTime = std::chrono::milliseconds(
                (avgJITCreationTime.count() * jitSamples + time.count()) /
                (jitSamples + 1));
        }
        ++jitSamples;

        if (time > peakJITCreationTime)
            peakJITCreationTime = time;
    }

    /**
     * @brief Reset all timing stats
     */
    void Reset()
    {
        avgAssignmentTime = std::chrono::microseconds(0);
        avgWarmupTime = std::chrono::milliseconds(0);
        avgJITCreationTime = std::chrono::milliseconds(0);
        avgInstanceDuration = std::chrono::minutes(0);
        avgCooldownTime = std::chrono::seconds(0);
        peakAssignmentTime = std::chrono::microseconds(0);
        peakJITCreationTime = std::chrono::milliseconds(0);
        assignmentSamples = warmupSamples = jitSamples = instanceSamples = cooldownSamples = 0;
    }
};

// ============================================================================
// MASTER STATISTICS
// ============================================================================

/**
 * @brief Complete statistics snapshot for Instance Bot Pool
 *
 * This structure aggregates all statistics into a single snapshot
 * that can be used for monitoring, logging, and admin displays.
 */
struct TC_GAME_API PoolStatistics
{
    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    /// When these statistics were calculated
    std::chrono::system_clock::time_point timestamp;

    /// When the current hour started (for hourly resets)
    std::chrono::system_clock::time_point hourStart;

    /// When the current day started (for daily resets)
    std::chrono::system_clock::time_point dayStart;

    // ========================================================================
    // SLOT STATISTICS
    // ========================================================================

    SlotStateStats slotStats;

    // ========================================================================
    // PER-ROLE STATISTICS
    // ========================================================================

    std::array<RoleStats, static_cast<size_t>(BotRole::Max)> roleStats;

    // ========================================================================
    // PER-FACTION STATISTICS
    // ========================================================================

    std::array<FactionStats, static_cast<size_t>(Faction::Max)> factionStats;

    // ========================================================================
    // ACTIVITY STATISTICS
    // ========================================================================

    ActivityStats activity;

    // ========================================================================
    // TIMING STATISTICS
    // ========================================================================

    TimingStats timing;

    // ========================================================================
    // JIT FACTORY STATISTICS
    // ========================================================================

    uint32 jitPendingRequests = 0;
    uint32 jitActiveCreations = 0;
    uint32 recycledBotsAvailable = 0;

    // ========================================================================
    // ORCHESTRATOR STATISTICS
    // ========================================================================

    uint32 pendingDungeonRequests = 0;
    uint32 pendingRaidRequests = 0;
    uint32 pendingBGRequests = 0;
    uint32 pendingArenaRequests = 0;
    uint32 activeInstances = 0;

    // ========================================================================
    // AGGREGATE METHODS
    // ========================================================================

    /**
     * @brief Get total pool size (all factions)
     */
    uint32 GetTotalPoolSize() const
    {
        return slotStats.GetTotal();
    }

    /**
     * @brief Get total ready bots
     */
    uint32 GetTotalReady() const
    {
        return slotStats.readySlots;
    }

    /**
     * @brief Get total assigned bots
     */
    uint32 GetTotalAssigned() const
    {
        return slotStats.assignedSlots;
    }

    /**
     * @brief Get ready bots for faction
     */
    uint32 GetReadyForFaction(Faction faction) const
    {
        auto idx = static_cast<size_t>(faction);
        if (idx < factionStats.size())
            return factionStats[idx].readySlots;
        return 0;
    }

    /**
     * @brief Get ready bots for role
     */
    uint32 GetReadyForRole(BotRole role) const
    {
        auto idx = static_cast<size_t>(role);
        if (idx < roleStats.size())
            return roleStats[idx].readySlots;
        return 0;
    }

    /**
     * @brief Get ready bots for faction and role
     */
    uint32 GetReadyForFactionAndRole(Faction faction, BotRole role) const
    {
        auto fIdx = static_cast<size_t>(faction);
        if (fIdx < factionStats.size())
            return factionStats[fIdx].GetReadyForRole(role);
        return 0;
    }

    /**
     * @brief Get overall utilization percentage
     */
    float GetUtilization() const
    {
        return slotStats.GetUtilizationPct();
    }

    /**
     * @brief Get overall availability percentage
     */
    float GetAvailability() const
    {
        return slotStats.GetAvailabilityPct();
    }

    /**
     * @brief Get total pending requests (all types)
     */
    uint32 GetTotalPendingRequests() const
    {
        return pendingDungeonRequests + pendingRaidRequests +
               pendingBGRequests + pendingArenaRequests;
    }

    // ========================================================================
    // RESET METHODS
    // ========================================================================

    /**
     * @brief Reset hourly counters
     */
    void ResetHourly()
    {
        hourStart = std::chrono::system_clock::now();
        activity.ResetHourly();
        for (auto& rs : roleStats)
            rs.ResetHourly();
        for (auto& fs : factionStats)
            fs.ResetHourly();
    }

    /**
     * @brief Reset daily counters
     */
    void ResetDaily()
    {
        dayStart = std::chrono::system_clock::now();
        activity.ResetDaily();
        for (auto& rs : roleStats)
            rs.ResetDaily();
    }

    /**
     * @brief Reset all statistics
     */
    void Reset()
    {
        timestamp = std::chrono::system_clock::now();
        hourStart = timestamp;
        dayStart = timestamp;
        slotStats.Reset();
        activity.ResetHourly();
        activity.ResetDaily();
        timing.Reset();
        jitPendingRequests = jitActiveCreations = recycledBotsAvailable = 0;
        pendingDungeonRequests = pendingRaidRequests = pendingBGRequests = 0;
        pendingArenaRequests = activeInstances = 0;
    }

    // ========================================================================
    // LOGGING/DISPLAY
    // ========================================================================

    /**
     * @brief Generate summary string for logging
     */
    std::string ToSummaryString() const;

    /**
     * @brief Generate detailed string for logging
     */
    std::string ToDetailedString() const;

    /**
     * @brief Print to log
     */
    void PrintToLog() const;
};

// ============================================================================
// RESERVATION TRACKING
// ============================================================================

/**
 * @brief Tracking information for a bot reservation
 *
 * Reservations are used to pre-allocate bots for upcoming instances,
 * particularly for large content that needs many bots.
 */
struct Reservation
{
    uint32 reservationId = 0;                   ///< Unique reservation ID
    InstanceType instanceType = InstanceType::Dungeon;
    uint32 contentId = 0;                       ///< Dungeon/Raid/BG ID
    uint32 playerLevel = 0;                     ///< Player's level (for matching)
    Faction playerFaction = Faction::Alliance;  ///< Player's faction

    /// Requirements
    uint32 tanksNeeded = 0;
    uint32 healersNeeded = 0;
    uint32 dpsNeeded = 0;
    uint32 allianceNeeded = 0;  ///< For PvP (both factions)
    uint32 hordeNeeded = 0;     ///< For PvP (both factions)

    /// Fulfillment tracking
    std::vector<ObjectGuid> reservedBots;
    uint32 tanksFulfilled = 0;
    uint32 healersFulfilled = 0;
    uint32 dpsFulfilled = 0;
    uint32 allianceFulfilled = 0;
    uint32 hordeFulfilled = 0;

    /// Timing
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point deadline;

    /**
     * @brief Check if reservation is fully fulfilled
     */
    bool IsFulfilled() const
    {
        if (RequiresBothFactions(instanceType))
        {
            return allianceFulfilled >= allianceNeeded &&
                   hordeFulfilled >= hordeNeeded;
        }
        return tanksFulfilled >= tanksNeeded &&
               healersFulfilled >= healersNeeded &&
               dpsFulfilled >= dpsNeeded;
    }

    /**
     * @brief Get fulfillment percentage
     */
    float GetFulfillmentPct() const
    {
        if (RequiresBothFactions(instanceType))
        {
            uint32 total = allianceNeeded + hordeNeeded;
            uint32 fulfilled = allianceFulfilled + hordeFulfilled;
            if (total == 0) return 100.0f;
            return (static_cast<float>(fulfilled) / static_cast<float>(total)) * 100.0f;
        }
        uint32 total = tanksNeeded + healersNeeded + dpsNeeded;
        uint32 fulfilled = tanksFulfilled + healersFulfilled + dpsFulfilled;
        if (total == 0) return 100.0f;
        return (static_cast<float>(fulfilled) / static_cast<float>(total)) * 100.0f;
    }

    /**
     * @brief Get total bots needed
     */
    uint32 GetTotalNeeded() const
    {
        if (RequiresBothFactions(instanceType))
            return allianceNeeded + hordeNeeded;
        return tanksNeeded + healersNeeded + dpsNeeded;
    }

    /**
     * @brief Get total bots fulfilled
     */
    uint32 GetTotalFulfilled() const
    {
        if (RequiresBothFactions(instanceType))
            return allianceFulfilled + hordeFulfilled;
        return tanksFulfilled + healersFulfilled + dpsFulfilled;
    }

    /**
     * @brief Check if deadline has passed
     */
    bool IsExpired() const
    {
        return std::chrono::steady_clock::now() > deadline;
    }

    /**
     * @brief Get time remaining until deadline
     */
    std::chrono::seconds TimeRemaining() const
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            return std::chrono::seconds(0);
        return std::chrono::duration_cast<std::chrono::seconds>(deadline - now);
    }
};

} // namespace Playerbot
