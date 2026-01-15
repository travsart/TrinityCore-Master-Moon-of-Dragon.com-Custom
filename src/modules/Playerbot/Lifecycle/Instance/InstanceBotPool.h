/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file InstanceBotPool.h
 * @brief Pre-warmed bot pool for instant instance assignment
 *
 * The InstanceBotPool maintains a pool of pre-logged-in bots ready for
 * instant assignment to dungeons, raids, battlegrounds, and arenas.
 *
 * Architecture:
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                        INSTANCE BOT POOL                                │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   WARM POOL (~200 bots)                                                 │
 * │   ┌─────────────────────────────────────────────────────────────────┐   │
 * │   │  Alliance (100)          │  Horde (100)                        │   │
 * │   │  ├── Tanks: 20           │  ├── Tanks: 20                      │   │
 * │   │  ├── Healers: 30         │  ├── Healers: 30                    │   │
 * │   │  └── DPS: 50             │  └── DPS: 50                        │   │
 * │   └─────────────────────────────────────────────────────────────────┘   │
 * │                                                                         │
 * │   SLOT STATES:                                                          │
 * │   [Ready] ──assign──> [Assigned] ──release──> [Cooldown] ──expire──>   │
 * │      ↑                                                              │   │
 * │      └──────────────────────────────────────────────────────────────┘   │
 * │                                                                         │
 * │   RESERVATION SYSTEM:                                                   │
 * │   - Pre-reserve bots for upcoming large content                         │
 * │   - Prevents pool exhaustion during BG/raid formation                   │
 * │   - Auto-cancel on timeout                                              │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses parallel_flat_hash_map with internal sharding
 * - Statistics are atomic or protected by shared mutex
 *
 * Performance:
 * - O(1) bot lookup by GUID
 * - O(n) bot selection by criteria (with early termination)
 * - Assignment target: <100ms for 5-man group
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "PoolSlotState.h"
#include "PoolConfiguration.h"
#include "PoolStatistics.h"
#include "InstanceBotSlot.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <unordered_map>

namespace Playerbot
{

// Forward declarations
class BotFactory;
class BotSession;

/**
 * @brief Assignment result for battlegrounds (both factions)
 */
struct BGAssignment
{
    std::vector<ObjectGuid> allianceBots;   ///< Alliance bots assigned
    std::vector<ObjectGuid> hordeBots;      ///< Horde bots assigned
    bool success = false;                    ///< Whether assignment succeeded
    std::string errorMessage;                ///< Error message if failed
};

/**
 * @brief Assignment result for arenas
 */
struct ArenaAssignment
{
    std::vector<ObjectGuid> teammates;      ///< Teammate bots
    std::vector<ObjectGuid> opponents;      ///< Opponent bots
    bool success = false;
    std::string errorMessage;
};

/**
 * @brief Pre-warmed bot pool for instant instance assignment
 *
 * Singleton class managing a pool of pre-logged-in bots for instant
 * assignment to dungeons, raids, battlegrounds, and arenas.
 */
class TC_GAME_API InstanceBotPool
{
public:
    /**
     * @brief Get singleton instance
     */
    static InstanceBotPool* Instance();

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the pool
     * @return true if successful
     *
     * Loads configuration, initializes data structures, and optionally
     * loads persistent pool data from database.
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     *
     * Saves pool state to database (if configured), releases all bots,
     * and cleans up resources.
     */
    void Shutdown();

    /**
     * @brief Main update loop
     * @param diff Time since last update in milliseconds
     *
     * Called from world update loop. Handles:
     * - Cooldown expiration
     * - Reservation timeouts
     * - Pool replenishment
     * - Statistics updates
     */
    void Update(uint32 diff);

    /**
     * @brief Load configuration from config file
     */
    void LoadConfig();

    // ========================================================================
    // POOL MANAGEMENT
    // ========================================================================

    /**
     * @brief Warm up the pool (create and login bots)
     *
     * Called during server startup or when pool needs replenishment.
     * Creates bots according to configuration and logs them in.
     */
    void WarmPool();

    /**
     * @brief Replenish depleted slots
     *
     * Checks pool levels and creates new bots as needed to maintain
     * configured pool sizes.
     */
    void ReplenishPool();

    /**
     * @brief Get count of available bots by role and pool type
     * @param role Bot role (Tank/Healer/DPS)
     * @param poolType Pool type (PvE/PvP_Alliance/PvP_Horde)
     * @return Number of ready bots matching criteria
     */
    uint32 GetAvailableCount(BotRole role, PoolType poolType) const;

    /**
     * @brief Get count of available bots by role and faction
     * @param role Bot role
     * @param faction Faction
     * @return Number of ready bots matching criteria
     */
    uint32 GetAvailableCount(BotRole role, Faction faction) const;

    /**
     * @brief Get count of available bots matching level range
     * @param role Bot role
     * @param faction Faction
     * @param level Target level
     * @param range Acceptable level variance (±range)
     * @return Number of ready bots matching criteria
     */
    uint32 GetAvailableCountForLevel(BotRole role, Faction faction,
                                      uint32 level, uint32 range = 5) const;

    /**
     * @brief Get total pool size
     * @return Total number of slots (all states)
     */
    uint32 GetTotalPoolSize() const;

    /**
     * @brief Get count of ready bots
     * @return Number of bots ready for assignment
     */
    uint32 GetReadyCount() const;

    /**
     * @brief Get count of assigned bots
     * @return Number of bots currently in instances
     */
    uint32 GetAssignedCount() const;

    // ========================================================================
    // BOT ASSIGNMENT - PvE
    // ========================================================================

    /**
     * @brief Assign bots for dungeon (5-man)
     * @param dungeonId Dungeon ID
     * @param playerLevel Player's level (for matching)
     * @param playerFaction Player's faction
     * @param tanksNeeded Number of tanks needed
     * @param healersNeeded Number of healers needed
     * @param dpsNeeded Number of DPS needed
     * @return Vector of assigned bot GUIDs (empty if failed)
     */
    std::vector<ObjectGuid> AssignForDungeon(
        uint32 dungeonId,
        uint32 playerLevel,
        Faction playerFaction,
        uint32 tanksNeeded,
        uint32 healersNeeded,
        uint32 dpsNeeded);

    /**
     * @brief Assign bots for raid
     * @param raidId Raid ID
     * @param playerLevel Player's level
     * @param playerFaction Player's faction
     * @param tanksNeeded Number of tanks needed
     * @param healersNeeded Number of healers needed
     * @param dpsNeeded Number of DPS needed
     * @return Vector of assigned bot GUIDs
     */
    std::vector<ObjectGuid> AssignForRaid(
        uint32 raidId,
        uint32 playerLevel,
        Faction playerFaction,
        uint32 tanksNeeded,
        uint32 healersNeeded,
        uint32 dpsNeeded);

    // ========================================================================
    // BOT ASSIGNMENT - PvP
    // ========================================================================

    /**
     * @brief Assign bots for battleground (both factions)
     * @param bgTypeId Battleground type ID
     * @param bracketLevel Level bracket
     * @param allianceNeeded Alliance bots needed
     * @param hordeNeeded Horde bots needed
     * @return BGAssignment with both faction bot lists
     */
    BGAssignment AssignForBattleground(
        uint32 bgTypeId,
        uint32 bracketLevel,
        uint32 allianceNeeded,
        uint32 hordeNeeded);

    /**
     * @brief Assign bots for arena
     * @param arenaType Arena type (2v2, 3v3, 5v5)
     * @param bracketLevel Level bracket
     * @param playerFaction Player's faction
     * @param teammatesNeeded Teammate bots needed
     * @param opponentsNeeded Opponent bots needed
     * @return ArenaAssignment with teammates and opponents
     */
    ArenaAssignment AssignForArena(
        uint32 arenaType,
        uint32 bracketLevel,
        Faction playerFaction,
        uint32 teammatesNeeded,
        uint32 opponentsNeeded);

    // ========================================================================
    // BOT RELEASE
    // ========================================================================

    /**
     * @brief Release bots back to pool
     * @param bots Vector of bot GUIDs to release
     */
    void ReleaseBots(std::vector<ObjectGuid> const& bots);

    /**
     * @brief Release single bot back to pool
     * @param botGuid Bot GUID
     * @param success Whether the instance was completed successfully
     */
    void ReleaseBot(ObjectGuid botGuid, bool success = true);

    /**
     * @brief Release all bots from an instance
     * @param instanceId Instance ID
     */
    void ReleaseBotsFromInstance(uint32 instanceId);

    // ========================================================================
    // RESERVATION SYSTEM
    // ========================================================================

    /**
     * @brief Create reservation for upcoming instance
     * @param type Instance type
     * @param contentId Content ID (dungeon/raid/bg ID)
     * @param playerLevel Player's level
     * @param playerFaction Player's faction
     * @param tanksNeeded Tanks needed
     * @param healersNeeded Healers needed
     * @param dpsNeeded DPS needed
     * @param allianceNeeded Alliance bots (for PvP)
     * @param hordeNeeded Horde bots (for PvP)
     * @return Reservation ID (0 if failed)
     */
    uint32 CreateReservation(
        InstanceType type,
        uint32 contentId,
        uint32 playerLevel,
        Faction playerFaction,
        uint32 tanksNeeded,
        uint32 healersNeeded,
        uint32 dpsNeeded,
        uint32 allianceNeeded = 0,
        uint32 hordeNeeded = 0);

    /**
     * @brief Check if reservation can be fulfilled
     * @param reservationId Reservation ID
     * @return true if all reserved bots are still available
     */
    bool CanFulfillReservation(uint32 reservationId) const;

    /**
     * @brief Get fulfillment percentage for reservation
     * @param reservationId Reservation ID
     * @return Percentage of bots reserved (0-100)
     */
    float GetReservationFulfillment(uint32 reservationId) const;

    /**
     * @brief Fulfill reservation and get bots
     * @param reservationId Reservation ID
     * @return Vector of bot GUIDs (empty if failed)
     *
     * Transitions reserved bots to Assigned state.
     */
    std::vector<ObjectGuid> FulfillReservation(uint32 reservationId);

    /**
     * @brief Cancel reservation
     * @param reservationId Reservation ID
     *
     * Returns reserved bots to Ready state.
     */
    void CancelReservation(uint32 reservationId);

    /**
     * @brief Get estimated wait time for content
     * @param type Instance type
     * @param contentId Content ID
     * @param tanksNeeded Tanks needed
     * @param healersNeeded Healers needed
     * @param dpsNeeded DPS needed
     * @return Estimated wait time (0 if instant)
     */
    std::chrono::milliseconds GetEstimatedWaitTime(
        InstanceType type,
        uint32 contentId,
        uint32 tanksNeeded,
        uint32 healersNeeded,
        uint32 dpsNeeded) const;

    // ========================================================================
    // QUERIES
    // ========================================================================

    /**
     * @brief Get slot info for a bot
     * @param botGuid Bot GUID
     * @return Pointer to slot (nullptr if not found)
     */
    InstanceBotSlot const* GetSlot(ObjectGuid botGuid) const;

    /**
     * @brief Check if bot is from pool
     * @param botGuid Bot GUID
     * @return true if bot is a pool bot
     */
    bool IsPoolBot(ObjectGuid botGuid) const;

    /**
     * @brief Get bot's current instance
     * @param botGuid Bot GUID
     * @return Instance ID (0 if not assigned)
     */
    uint32 GetBotInstanceId(ObjectGuid botGuid) const;

    /**
     * @brief Check if pool can provide bots for content
     * @param type Instance type
     * @param tanksNeeded Tanks needed
     * @param healersNeeded Healers needed
     * @param dpsNeeded DPS needed
     * @return true if pool has sufficient bots
     */
    bool CanProvideBotsFor(
        InstanceType type,
        uint32 tanksNeeded,
        uint32 healersNeeded,
        uint32 dpsNeeded) const;

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get current statistics snapshot
     * @return Pool statistics
     */
    PoolStatistics GetStatistics() const;

    /**
     * @brief Print status report to log
     */
    void PrintStatusReport() const;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Get current configuration
     * @return Configuration reference
     */
    InstanceBotPoolConfig const& GetConfig() const { return _config; }

    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void SetConfig(InstanceBotPoolConfig const& config);

    /**
     * @brief Check if pool is enabled
     * @return true if enabled and initialized
     */
    bool IsEnabled() const { return _config.enabled && _initialized.load(); }

    /**
     * @brief Check if pool is initialized
     * @return true if initialized
     */
    bool IsInitialized() const { return _initialized.load(); }

    // ========================================================================
    // CALLBACKS
    // ========================================================================

    /// Callback for when bot assignment fails
    using AssignmentFailedCallback = std::function<void(InstanceType, uint32, std::string const&)>;

    /**
     * @brief Set callback for assignment failures
     * @param callback Callback function
     */
    void SetAssignmentFailedCallback(AssignmentFailedCallback callback);

    /// Callback for when pool needs overflow (JIT)
    using OverflowNeededCallback = std::function<void(BotRole, Faction, uint32, uint32)>;

    /**
     * @brief Set callback for overflow requirements
     * @param callback Callback function (role, faction, level, count)
     */
    void SetOverflowNeededCallback(OverflowNeededCallback callback);

private:
    InstanceBotPool() = default;
    ~InstanceBotPool() = default;
    InstanceBotPool(InstanceBotPool const&) = delete;
    InstanceBotPool& operator=(InstanceBotPool const&) = delete;

    // ========================================================================
    // INTERNAL METHODS - Bot Creation
    // ========================================================================

    /**
     * @brief Create a new pool bot
     * @param role Bot role
     * @param poolType Pool type
     * @param level Target level
     * @param deferWarmup If true, skip immediate login attempt (ProcessWarmingRetries will handle it gradually)
     * @return Bot GUID (empty if failed)
     */
    ObjectGuid CreatePoolBot(BotRole role, PoolType poolType, uint32 level, bool deferWarmup = false);

    /**
     * @brief Warm up a single bot (login)
     * @param botGuid Bot GUID
     * @return true if warmup started successfully
     */
    bool WarmUpBot(ObjectGuid botGuid);

    /**
     * @brief Called when bot warmup completes
     * @param botGuid Bot GUID
     * @param success Whether warmup succeeded
     */
    void OnBotWarmupComplete(ObjectGuid botGuid, bool success);

    // ========================================================================
    // INTERNAL METHODS - Bot Selection
    // ========================================================================

    /**
     * @brief Select best available bot matching criteria
     * @param role Required role
     * @param faction Required faction
     * @param level Target level
     * @param minGearScore Minimum gear score
     * @return Best matching bot GUID (empty if none available)
     */
    ObjectGuid SelectBestBot(BotRole role, Faction faction, uint32 level, uint32 minGearScore);

    /**
     * @brief Select multiple bots matching criteria
     * @param role Required role
     * @param faction Required faction
     * @param level Target level
     * @param count Number of bots needed
     * @param minGearScore Minimum gear score
     * @return Vector of selected bot GUIDs
     */
    std::vector<ObjectGuid> SelectBots(BotRole role, Faction faction,
                                        uint32 level, uint32 count, uint32 minGearScore);

    /**
     * @brief Assign bot to instance
     * @param botGuid Bot GUID
     * @param instanceId Instance ID
     * @param contentId Content ID
     * @param type Instance type
     * @param targetLevel Target level for the bot (player's level to match)
     * @return true if assignment succeeded
     */
    bool AssignBot(ObjectGuid botGuid, uint32 instanceId, uint32 contentId, InstanceType type, uint32 targetLevel);

    // ========================================================================
    // INTERNAL METHODS - Pool Maintenance
    // ========================================================================

    /**
     * @brief Retry warmup for bots stuck in Warming state (async DB commit delay)
     */
    void ProcessWarmingRetries();

    /**
     * @brief Process cooldown expirations
     */
    void ProcessCooldowns();

    /**
     * @brief Process reservation timeouts
     */
    void ProcessReservations();

    /**
     * @brief Update statistics
     */
    void UpdateStatistics();

    /**
     * @brief Check hourly reset
     */
    void CheckHourlyReset();

    // ========================================================================
    // INTERNAL METHODS - Database
    // ========================================================================

    /**
     * @brief Sync pool state to database
     */
    void SyncToDatabase();

    /**
     * @brief Load pool state from database
     */
    void LoadFromDatabase();

    // ========================================================================
    // DATA MEMBERS - Configuration
    // ========================================================================

    InstanceBotPoolConfig _config;

    // ========================================================================
    // DATA MEMBERS - Pool Storage
    // ========================================================================

    /// Pool slots indexed by bot GUID
    /// Using unordered_map with shared_mutex for thread safety
    std::unordered_map<ObjectGuid, InstanceBotSlot> _slots;
    mutable std::shared_mutex _slotsMutex;

    /// Quick lookup indices
    /// role -> faction -> level bracket -> list of ready bot GUIDs
    using ReadyIndex = std::unordered_map<BotRole,
                       std::unordered_map<Faction,
                       std::unordered_map<uint32, std::vector<ObjectGuid>>>>;
    ReadyIndex _readyIndex;
    mutable std::shared_mutex _readyIndexMutex;

    // ========================================================================
    // DATA MEMBERS - Reservations
    // ========================================================================

    std::unordered_map<uint32, Reservation> _reservations;
    std::atomic<uint32> _nextReservationId{1};
    mutable std::mutex _reservationMutex;

    // ========================================================================
    // DATA MEMBERS - Statistics
    // ========================================================================

    mutable PoolStatistics _stats;
    mutable std::shared_mutex _statsMutex;
    std::atomic<bool> _statsDirty{true};

    // ========================================================================
    // DATA MEMBERS - Hourly Tracking
    // ========================================================================

    std::chrono::system_clock::time_point _hourStart;
    std::chrono::system_clock::time_point _dayStart;

    // ========================================================================
    // DATA MEMBERS - Timing Accumulators
    // ========================================================================

    uint32 _updateAccumulator = 0;
    uint32 _dbSyncAccumulator = 0;
    uint32 _statsAccumulator = 0;
    uint32 _replenishAccumulator = 0;

    // ========================================================================
    // DATA MEMBERS - State
    // ========================================================================

    std::atomic<bool> _initialized{false};
    std::atomic<bool> _warmingInProgress{false};
    std::atomic<bool> _shuttingDown{false};
    std::atomic<bool> _warmupPending{false};  // Deferred warmup until world is running

    // ========================================================================
    // DATA MEMBERS - Callbacks
    // ========================================================================

    AssignmentFailedCallback _assignmentFailedCallback;
    OverflowNeededCallback _overflowNeededCallback;
};

} // namespace Playerbot

#define sInstanceBotPool Playerbot::InstanceBotPool::Instance()
