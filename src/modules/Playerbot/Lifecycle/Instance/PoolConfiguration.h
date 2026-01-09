/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file PoolConfiguration.h
 * @brief Configuration structures for the Instance Bot Pool system
 *
 * This file defines all configurable parameters for:
 * - Pool sizes (per faction, per role)
 * - Level bracket distribution
 * - Gear score requirements
 * - Timing parameters (cooldowns, timeouts, warmup)
 * - JIT factory settings
 * - Orchestrator behavior
 *
 * All values can be overridden via playerbots.conf configuration file.
 *
 * Design Philosophy:
 * - Sensible defaults for immediate use
 * - Comprehensive customization for different server environments
 * - Performance-oriented settings for high-concurrency scenarios
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
// POOL SIZE CONFIGURATION
// ============================================================================

/**
 * @brief Configuration for pool sizes per faction and role
 *
 * The warm pool maintains pre-logged-in bots for instant assignment.
 * Default distribution targets instant 5-man and 10-man content:
 * - Alliance: 20 Tanks, 30 Healers, 50 DPS = 100 total
 * - Horde: 20 Tanks, 30 Healers, 50 DPS = 100 total
 * - Total warm pool: 200 bots
 */
struct PoolSizeConfig
{
    // ========================================================================
    // PER-FACTION POOL SIZES
    // ========================================================================

    /// Alliance warm pool distribution
    uint32 allianceTanks = 20;          ///< Alliance tank bots
    uint32 allianceHealers = 30;        ///< Alliance healer bots
    uint32 allianceDPS = 50;            ///< Alliance DPS bots

    /// Horde warm pool distribution
    uint32 hordeTanks = 20;             ///< Horde tank bots
    uint32 hordeHealers = 30;           ///< Horde healer bots
    uint32 hordeDPS = 50;               ///< Horde DPS bots

    // ========================================================================
    // OVERFLOW / JIT LIMITS
    // ========================================================================

    /// Maximum bots that JIT factory can create on-demand
    uint32 maxOverflowBots = 500;

    /// Maximum bots created per second during overflow
    uint32 overflowCreationRate = 10;

    /// Maximum concurrent JIT creation operations
    uint32 maxConcurrentCreations = 10;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Get total Alliance pool size
     */
    uint32 GetAllianceTotal() const
    {
        return allianceTanks + allianceHealers + allianceDPS;
    }

    /**
     * @brief Get total Horde pool size
     */
    uint32 GetHordeTotal() const
    {
        return hordeTanks + hordeHealers + hordeDPS;
    }

    /**
     * @brief Get total warm pool size (both factions)
     */
    uint32 GetTotalWarmPool() const
    {
        return GetAllianceTotal() + GetHordeTotal();
    }

    /**
     * @brief Get total tank count (both factions)
     */
    uint32 GetTotalTanks() const
    {
        return allianceTanks + hordeTanks;
    }

    /**
     * @brief Get total healer count (both factions)
     */
    uint32 GetTotalHealers() const
    {
        return allianceHealers + hordeHealers;
    }

    /**
     * @brief Get total DPS count (both factions)
     */
    uint32 GetTotalDPS() const
    {
        return allianceDPS + hordeDPS;
    }

    /**
     * @brief Get role count for specific faction
     */
    uint32 GetRoleCount(Faction faction, BotRole role) const
    {
        if (faction == Faction::Alliance)
        {
            switch (role)
            {
                case BotRole::Tank:   return allianceTanks;
                case BotRole::Healer: return allianceHealers;
                case BotRole::DPS:    return allianceDPS;
                default:              return 0;
            }
        }
        else
        {
            switch (role)
            {
                case BotRole::Tank:   return hordeTanks;
                case BotRole::Healer: return hordeHealers;
                case BotRole::DPS:    return hordeDPS;
                default:              return 0;
            }
        }
    }
};

// ============================================================================
// LEVEL CONFIGURATION
// ============================================================================

/**
 * @brief Configuration for level bracket distribution and requirements
 *
 * Pool bots are distributed across level brackets to support
 * all game content from leveling dungeons to max-level raids.
 */
struct PoolLevelConfig
{
    // ========================================================================
    // LEVEL BRACKET DISTRIBUTION
    // ========================================================================

    /// Distribution of pool bots across level brackets
    /// Index: 0=Starting (1-10), 1=ChromieTime (10-60), 2=Dragonflight (60-70), 3=TheWarWithin (70-80)
    std::array<float, 4> bracketDistribution = {
        0.05f,   // 5% Starting (1-10) - Few needed, leveling is quick
        0.15f,   // 15% Chromie Time (10-60) - Some leveling content
        0.20f,   // 20% Dragonflight (60-70) - Prior expansion
        0.60f    // 60% The War Within (70-80) - Current expansion, most content
    };

    /// Level ranges for each bracket
    static constexpr uint32 STARTING_MIN = 1;
    static constexpr uint32 STARTING_MAX = 10;
    static constexpr uint32 CHROMIE_MIN = 10;
    static constexpr uint32 CHROMIE_MAX = 60;
    static constexpr uint32 DF_MIN = 60;
    static constexpr uint32 DF_MAX = 70;
    static constexpr uint32 TWW_MIN = 70;
    static constexpr uint32 TWW_MAX = 80;

    // ========================================================================
    // GEAR SCORE REQUIREMENTS BY CONTENT
    // ========================================================================

    /// Minimum gear score for normal dungeons
    uint32 normalDungeonMinGS = 350;

    /// Minimum gear score for heroic dungeons
    uint32 heroicDungeonMinGS = 400;

    /// Minimum gear score for Mythic 0 dungeons
    uint32 mythic0DungeonMinGS = 450;

    /// Minimum gear score for Mythic+ dungeons
    uint32 mythicPlusDungeonMinGS = 480;

    /// Minimum gear score for normal raids
    uint32 normalRaidMinGS = 480;

    /// Minimum gear score for heroic raids
    uint32 heroicRaidMinGS = 510;

    /// Minimum gear score for mythic raids
    uint32 mythicRaidMinGS = 540;

    /// Minimum gear score for rated PvP
    uint32 ratedPvPMinGS = 500;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Get bracket for level
     * @param level Character level
     * @return Bracket index (0-3)
     */
    static uint32 GetBracketForLevel(uint32 level)
    {
        if (level < STARTING_MAX)
            return 0;
        if (level < CHROMIE_MAX)
            return 1;
        if (level < DF_MAX)
            return 2;
        return 3;
    }

    /**
     * @brief Get level range for bracket
     * @param bracket Bracket index
     * @param[out] minLevel Minimum level
     * @param[out] maxLevel Maximum level
     */
    static void GetLevelRange(uint32 bracket, uint32& minLevel, uint32& maxLevel)
    {
        switch (bracket)
        {
            case 0:  minLevel = STARTING_MIN; maxLevel = STARTING_MAX; break;
            case 1:  minLevel = CHROMIE_MIN;  maxLevel = CHROMIE_MAX;  break;
            case 2:  minLevel = DF_MIN;       maxLevel = DF_MAX;       break;
            default: minLevel = TWW_MIN;      maxLevel = TWW_MAX;      break;
        }
    }

    /**
     * @brief Calculate target count for bracket based on total pool size
     * @param totalPoolSize Total pool size
     * @param bracket Bracket index
     * @return Target bot count for bracket
     */
    uint32 GetTargetCountForBracket(uint32 totalPoolSize, uint32 bracket) const
    {
        if (bracket >= bracketDistribution.size())
            return 0;

        return static_cast<uint32>(totalPoolSize * bracketDistribution[bracket]);
    }
};

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================

/**
 * @brief Configuration for timing-related parameters
 *
 * All timing values can be adjusted based on server performance
 * and player experience requirements.
 */
struct PoolTimingConfig
{
    // ========================================================================
    // COOLDOWNS
    // ========================================================================

    /// Cooldown between bot assignments (seconds)
    /// Prevents the same bot from being assigned repeatedly
    std::chrono::seconds cooldownDuration{300};  // 5 minutes

    /// Shorter cooldown for overflow/JIT bots (seconds)
    std::chrono::seconds overflowCooldownDuration{60};  // 1 minute

    // ========================================================================
    // TIMEOUTS
    // ========================================================================

    /// Maximum time for reservation to be fulfilled (ms)
    std::chrono::milliseconds reservationTimeout{60000};  // 1 minute

    /// Maximum time for bot warmup/login (ms)
    std::chrono::milliseconds warmupTimeout{30000};  // 30 seconds

    /// Maximum time for JIT bot creation (ms)
    std::chrono::milliseconds jitCreationTimeout{60000};  // 1 minute

    /// Time before recycled JIT bots are cleaned up (minutes)
    std::chrono::minutes recycleTimeout{5};

    // ========================================================================
    // UPDATE INTERVALS
    // ========================================================================

    /// Pool maintenance update interval (ms)
    uint32 updateIntervalMs = 1000;  // 1 second

    /// Database sync interval (ms)
    uint32 dbSyncIntervalMs = 60000;  // 1 minute

    /// Statistics calculation interval (ms)
    uint32 statsIntervalMs = 5000;  // 5 seconds

    /// Pool replenishment check interval (ms)
    uint32 replenishIntervalMs = 10000;  // 10 seconds
};

// ============================================================================
// BEHAVIOR CONFIGURATION
// ============================================================================

/**
 * @brief Configuration for pool behavior and automation
 */
struct PoolBehaviorConfig
{
    // ========================================================================
    // AUTOMATION
    // ========================================================================

    /// Automatically replenish pool when bots are assigned
    bool autoReplenish = true;

    /// Persist pool state to database
    bool persistToDatabase = true;

    /// Warm pool on server startup
    bool warmOnStartup = true;

    /// Use JIT factory when pool is insufficient
    bool enableJITFactory = true;

    /// Enable bot recycling for JIT bots
    bool enableRecycling = true;

    // ========================================================================
    // THRESHOLDS
    // ========================================================================

    /// Use JIT factory when pool drops below this percentage
    uint32 jitThresholdPct = 20;

    /// Maximum recycled bots to keep
    uint32 maxRecycledBots = 100;

    /// Minimum bots to keep ready per role (prevents exhaustion)
    uint32 minBotsPerRole = 5;

    // ========================================================================
    // ASSIGNMENT PREFERENCES
    // ========================================================================

    /// Prefer bots that haven't been used recently
    bool spreadAssignments = true;

    /// Prefer bots with higher gear scores
    bool preferHighGearScore = true;

    /// Prefer bots with higher success rates
    bool preferHighSuccessRate = true;
};

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================

/**
 * @brief Configuration for pool logging and debugging
 */
struct PoolLoggingConfig
{
    /// Log individual bot assignments
    bool logAssignments = true;

    /// Log pool state changes
    bool logPoolChanges = false;

    /// Log JIT factory operations
    bool logJITOperations = true;

    /// Log cooldown expirations
    bool logCooldowns = false;

    /// Log reservation operations
    bool logReservations = true;

    /// Log detailed statistics periodically
    bool logDetailedStats = false;

    /// Log level for pool operations (0=disabled, 1=error, 2=warn, 3=info, 4=debug)
    uint32 logLevel = 3;
};

// ============================================================================
// MASTER CONFIGURATION
// ============================================================================

/**
 * @brief Master configuration structure for Instance Bot Pool
 *
 * This structure aggregates all configuration sub-structures
 * and provides methods for loading from config file.
 */
struct TC_GAME_API InstanceBotPoolConfig
{
    // ========================================================================
    // ENABLE/DISABLE
    // ========================================================================

    /// Master enable switch for instance bot pool
    bool enabled = true;

    // ========================================================================
    // SUB-CONFIGURATIONS
    // ========================================================================

    PoolSizeConfig poolSize;
    PoolLevelConfig levelConfig;
    PoolTimingConfig timing;
    PoolBehaviorConfig behavior;
    PoolLoggingConfig logging;

    // ========================================================================
    // LOADING
    // ========================================================================

    /**
     * @brief Load configuration from config file
     *
     * Reads all Playerbot.Instance.Pool.* settings from playerbots.conf
     */
    void LoadFromConfig();

    /**
     * @brief Validate configuration values
     * @return true if all values are valid
     */
    bool Validate() const;

    /**
     * @brief Get default configuration
     * @return Default config instance
     */
    static InstanceBotPoolConfig GetDefault();

    /**
     * @brief Print configuration to log
     */
    void PrintToLog() const;
};

// ============================================================================
// JIT FACTORY CONFIGURATION
// ============================================================================

/**
 * @brief Configuration specific to JIT (Just-In-Time) Bot Factory
 *
 * The JIT factory creates bots on-demand when the warm pool is exhausted,
 * typically for large content like 40-man raids or 40v40 battlegrounds.
 */
struct JITFactoryConfig
{
    /// Enable JIT factory
    bool enabled = true;

    /// Maximum concurrent bot creations
    uint32 maxConcurrentCreations = 10;

    /// Maximum requests in queue
    uint32 maxQueuedRequests = 50;

    /// Time before recycled bots are deleted (minutes)
    uint32 recycleTimeoutMinutes = 5;

    /// Maximum recycled bots to keep
    uint32 maxRecycledBots = 100;

    /// Use template cloning for fast creation
    bool useTemplateCloning = true;

    /// Pre-serialize bot templates for speed
    bool preSerializeTemplates = true;

    /// Log factory operations
    bool logOperations = true;

    // Priority settings (lower = higher priority)
    uint8 dungeonPriority = 1;
    uint8 arenaPriority = 2;
    uint8 raidPriority = 3;
    uint8 battlegroundPriority = 4;

    // Timeout settings (milliseconds)
    uint32 dungeonTimeoutMs = 30000;
    uint32 raidTimeoutMs = 60000;
    uint32 battlegroundTimeoutMs = 120000;
    uint32 arenaTimeoutMs = 15000;

    /**
     * @brief Load from config file
     */
    void LoadFromConfig();
};

// ============================================================================
// NOTE: InstanceOrchestratorConfig is defined in InstanceBotOrchestrator.h
// ============================================================================

// ============================================================================
// CONTENT REQUIREMENTS DEFAULTS
// ============================================================================

/**
 * @brief Default content requirements for instance types
 *
 * These defaults are used when content-specific requirements are not
 * defined in the database. They provide reasonable defaults for
 * standard WoW content.
 */
struct ContentRequirementDefaults
{
    // 5-man Dungeons
    static constexpr uint32 DUNGEON_TANKS = 1;
    static constexpr uint32 DUNGEON_HEALERS = 1;
    static constexpr uint32 DUNGEON_DPS = 3;
    static constexpr uint32 DUNGEON_TOTAL = 5;

    // 10-man Raids
    static constexpr uint32 RAID_10_TANKS = 2;
    static constexpr uint32 RAID_10_HEALERS = 3;
    static constexpr uint32 RAID_10_DPS = 5;
    static constexpr uint32 RAID_10_TOTAL = 10;

    // 25-man Raids
    static constexpr uint32 RAID_25_TANKS = 3;
    static constexpr uint32 RAID_25_HEALERS = 6;
    static constexpr uint32 RAID_25_DPS = 16;
    static constexpr uint32 RAID_25_TOTAL = 25;

    // 40-man Raids
    static constexpr uint32 RAID_40_TANKS = 4;
    static constexpr uint32 RAID_40_HEALERS = 10;
    static constexpr uint32 RAID_40_DPS = 26;
    static constexpr uint32 RAID_40_TOTAL = 40;

    // Battlegrounds
    static constexpr uint32 WSG_IOC_PER_FACTION = 10;      // Warsong Gulch, etc.
    static constexpr uint32 AB_PER_FACTION = 15;           // Arathi Basin
    static constexpr uint32 AV_IOC_PER_FACTION = 40;       // Alterac Valley, Isle of Conquest

    // Arenas
    static constexpr uint32 ARENA_2V2 = 2;
    static constexpr uint32 ARENA_3V3 = 3;
    static constexpr uint32 ARENA_5V5 = 5;
};

} // namespace Playerbot
