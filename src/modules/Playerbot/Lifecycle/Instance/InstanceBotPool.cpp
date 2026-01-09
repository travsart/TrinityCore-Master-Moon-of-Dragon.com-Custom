/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "InstanceBotPool.h"
#include "InstanceBotOrchestrator.h"
#include "BotCloneEngine.h"
#include "BotTemplateRepository.h"
#include "Config/PlayerbotConfig.h"
#include "Session/BotWorldSessionMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>
#include <random>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

InstanceBotPool* InstanceBotPool::Instance()
{
    static InstanceBotPool instance;
    return &instance;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool InstanceBotPool::Initialize()
{
    if (_initialized.load())
    {
        TC_LOG_WARN("playerbot.pool", "InstanceBotPool already initialized");
        return true;
    }

    TC_LOG_INFO("playerbot.pool", "Initializing Instance Bot Pool...");

    // Load configuration
    LoadConfig();

    if (!_config.enabled)
    {
        TC_LOG_INFO("playerbot.pool", "Instance Bot Pool is disabled in configuration");
        return true;
    }

    // Initialize timing
    _hourStart = std::chrono::system_clock::now();
    _dayStart = _hourStart;

    // Initialize statistics
    _stats.Reset();

    // Load from database if configured
    if (_config.behavior.persistToDatabase)
    {
        LoadFromDatabase();
    }

    _initialized.store(true);

    TC_LOG_INFO("playerbot.pool", "Instance Bot Pool initialized successfully");

    // Warm the pool if configured
    if (_config.behavior.warmOnStartup)
    {
        WarmPool();
    }

    return true;
}

void InstanceBotPool::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("playerbot.pool", "Shutting down Instance Bot Pool...");

    _shuttingDown.store(true);

    // Save to database if configured
    if (_config.behavior.persistToDatabase)
    {
        SyncToDatabase();
    }

    // Clear all slots
    {
        std::unique_lock lock(_slotsMutex);
        _slots.clear();
    }

    // Clear reservations
    {
        std::lock_guard lock(_reservationMutex);
        _reservations.clear();
    }

    // Clear ready index
    {
        std::unique_lock lock(_readyIndexMutex);
        _readyIndex.clear();
    }

    _initialized.store(false);
    _shuttingDown.store(false);

    TC_LOG_INFO("playerbot.pool", "Instance Bot Pool shutdown complete");
}

void InstanceBotPool::Update(uint32 diff)
{
    if (!_initialized.load() || !_config.enabled || _shuttingDown.load())
        return;

    // Main update at configured interval
    _updateAccumulator += diff;
    if (_updateAccumulator >= _config.timing.updateIntervalMs)
    {
        _updateAccumulator = 0;

        // Process cooldown expirations
        ProcessCooldowns();

        // Process reservation timeouts
        ProcessReservations();

        // Retry warming bots that failed initial warmup (async DB commit delay)
        ProcessWarmingRetries();

        // Check hourly reset
        CheckHourlyReset();
    }

    // Statistics update at configured interval
    _statsAccumulator += diff;
    if (_statsAccumulator >= _config.timing.statsIntervalMs)
    {
        _statsAccumulator = 0;
        UpdateStatistics();
    }

    // Pool replenishment at configured interval
    if (_config.behavior.autoReplenish)
    {
        _replenishAccumulator += diff;
        if (_replenishAccumulator >= _config.timing.replenishIntervalMs)
        {
            _replenishAccumulator = 0;
            ReplenishPool();
        }
    }

    // Database sync at configured interval
    if (_config.behavior.persistToDatabase)
    {
        _dbSyncAccumulator += diff;
        if (_dbSyncAccumulator >= _config.timing.dbSyncIntervalMs)
        {
            _dbSyncAccumulator = 0;
            SyncToDatabase();
        }
    }
}

void InstanceBotPool::LoadConfig()
{
    TC_LOG_INFO("playerbot.pool", "Loading Instance Bot Pool configuration...");

    // Master enable
    _config.enabled = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.Enable", true);

    // Pool sizes - Alliance
    _config.poolSize.allianceTanks = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.Alliance.Tanks", 20);
    _config.poolSize.allianceHealers = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.Alliance.Healers", 30);
    _config.poolSize.allianceDPS = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.Alliance.DPS", 50);

    // Pool sizes - Horde
    _config.poolSize.hordeTanks = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.Horde.Tanks", 20);
    _config.poolSize.hordeHealers = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.Horde.Healers", 30);
    _config.poolSize.hordeDPS = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.Horde.DPS", 50);

    // Overflow settings
    _config.poolSize.maxOverflowBots = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.MaxOverflow", 500);
    _config.poolSize.overflowCreationRate = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.OverflowRate", 10);
    _config.poolSize.maxConcurrentCreations = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.MaxConcurrentCreations", 10);

    // Timing
    _config.timing.cooldownDuration = std::chrono::seconds(
        sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.CooldownSeconds", 300));
    _config.timing.reservationTimeout = std::chrono::milliseconds(
        sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.ReservationTimeoutMs", 60000));
    _config.timing.warmupTimeout = std::chrono::milliseconds(
        sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.WarmupTimeoutMs", 30000));
    _config.timing.updateIntervalMs = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.UpdateIntervalMs", 1000);
    _config.timing.dbSyncIntervalMs = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.DbSyncIntervalMs", 60000);

    // Behavior
    _config.behavior.autoReplenish = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.AutoReplenish", true);
    _config.behavior.persistToDatabase = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.Persist", true);
    _config.behavior.warmOnStartup = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.WarmOnStartup", true);
    _config.behavior.enableJITFactory = sPlayerbotConfig->GetBool("Playerbot.Instance.JIT.Enable", true);
    _config.behavior.jitThresholdPct = sPlayerbotConfig->GetInt("Playerbot.Instance.Pool.JITThresholdPct", 20);

    // Logging
    _config.logging.logAssignments = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.LogAssignments", true);
    _config.logging.logPoolChanges = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.LogChanges", false);
    _config.logging.logReservations = sPlayerbotConfig->GetBool("Playerbot.Instance.Pool.LogReservations", true);

    TC_LOG_INFO("playerbot.pool",
        "Instance Bot Pool config: enabled={}, alliance={}/{}/{}, horde={}/{}/{}, cooldown={}s",
        _config.enabled,
        _config.poolSize.allianceTanks, _config.poolSize.allianceHealers, _config.poolSize.allianceDPS,
        _config.poolSize.hordeTanks, _config.poolSize.hordeHealers, _config.poolSize.hordeDPS,
        _config.timing.cooldownDuration.count());
}

// ============================================================================
// POOL MANAGEMENT
// ============================================================================

void InstanceBotPool::WarmPool()
{
    if (_warmingInProgress.load())
    {
        TC_LOG_DEBUG("playerbot.pool", "Pool warming already in progress");
        return;
    }

    _warmingInProgress.store(true);

    TC_LOG_INFO("playerbot.pool", "Starting pool warming with level bracket distribution...");

    uint32 totalToCreate = _config.poolSize.GetTotalWarmPool();
    uint32 created = 0;

    // Helper lambda to get a representative level for a bracket
    auto getLevelForBracket = [](uint32 bracket) -> uint32 {
        uint32 minLevel, maxLevel;
        PoolLevelConfig::GetLevelRange(bracket, minLevel, maxLevel);
        // Use middle of the bracket for representative level
        return (minLevel + maxLevel) / 2;
    };

    // Helper lambda to create bots for a role distributed across level brackets
    auto createBotsForRole = [&](BotRole role, PoolType poolType, uint32 totalCount) {
        // Distribute bots across all 4 level brackets according to config
        for (uint32 bracket = 0; bracket < 4; ++bracket)
        {
            uint32 countForBracket = static_cast<uint32>(totalCount * _config.levelConfig.bracketDistribution[bracket]);
            // Ensure at least 1 bot per bracket if total count > 4
            if (countForBracket == 0 && totalCount > 4)
                countForBracket = 1;

            uint32 level = getLevelForBracket(bracket);

            TC_LOG_DEBUG("playerbot.pool", "WarmPool: Creating {} {} bots at level {} (bracket {})",
                countForBracket, BotRoleToString(role), level, bracket);

            for (uint32 i = 0; i < countForBracket; ++i)
            {
                if (CreatePoolBot(role, poolType, level) != ObjectGuid::Empty)
                    ++created;
            }
        }
    };

    // Create Alliance bots distributed across level brackets
    TC_LOG_INFO("playerbot.pool", "Creating Alliance pool bots...");
    createBotsForRole(BotRole::Tank, PoolType::PvP_Alliance, _config.poolSize.allianceTanks);
    createBotsForRole(BotRole::Healer, PoolType::PvP_Alliance, _config.poolSize.allianceHealers);
    createBotsForRole(BotRole::DPS, PoolType::PvP_Alliance, _config.poolSize.allianceDPS);

    // Create Horde bots distributed across level brackets
    TC_LOG_INFO("playerbot.pool", "Creating Horde pool bots...");
    createBotsForRole(BotRole::Tank, PoolType::PvP_Horde, _config.poolSize.hordeTanks);
    createBotsForRole(BotRole::Healer, PoolType::PvP_Horde, _config.poolSize.hordeHealers);
    createBotsForRole(BotRole::DPS, PoolType::PvP_Horde, _config.poolSize.hordeDPS);

    _warmingInProgress.store(false);

    TC_LOG_INFO("playerbot.pool", "Pool warming complete: created {} of {} bots across 4 level brackets", created, totalToCreate);
    TC_LOG_INFO("playerbot.pool", "Level bracket distribution: 1-10 ({}%), 10-60 ({}%), 60-70 ({}%), 70-80 ({}%)",
        static_cast<uint32>(_config.levelConfig.bracketDistribution[0] * 100),
        static_cast<uint32>(_config.levelConfig.bracketDistribution[1] * 100),
        static_cast<uint32>(_config.levelConfig.bracketDistribution[2] * 100),
        static_cast<uint32>(_config.levelConfig.bracketDistribution[3] * 100));

    _statsDirty.store(true);
}

void InstanceBotPool::ReplenishPool()
{
    if (_warmingInProgress.load())
        return;

    std::shared_lock lock(_slotsMutex);

    // Count current ready bots per faction/role
    uint32 allianceTanksReady = 0, allianceHealersReady = 0, allianceDPSReady = 0;
    uint32 hordeTanksReady = 0, hordeHealersReady = 0, hordeDPSReady = 0;

    for (auto const& [guid, slot] : _slots)
    {
        if (slot.state != PoolSlotState::Ready)
            continue;

        if (slot.faction == Faction::Alliance)
        {
            switch (slot.role)
            {
                case BotRole::Tank:   ++allianceTanksReady; break;
                case BotRole::Healer: ++allianceHealersReady; break;
                case BotRole::DPS:    ++allianceDPSReady; break;
                default: break;
            }
        }
        else
        {
            switch (slot.role)
            {
                case BotRole::Tank:   ++hordeTanksReady; break;
                case BotRole::Healer: ++hordeHealersReady; break;
                case BotRole::DPS:    ++hordeDPSReady; break;
                default: break;
            }
        }
    }

    lock.unlock();

    // Check if replenishment is needed
    bool needReplenish = false;
    if (allianceTanksReady < _config.behavior.minBotsPerRole ||
        allianceHealersReady < _config.behavior.minBotsPerRole ||
        allianceDPSReady < _config.behavior.minBotsPerRole ||
        hordeTanksReady < _config.behavior.minBotsPerRole ||
        hordeHealersReady < _config.behavior.minBotsPerRole ||
        hordeDPSReady < _config.behavior.minBotsPerRole)
    {
        needReplenish = true;
    }

    if (!needReplenish)
        return;

    if (_config.logging.logPoolChanges)
    {
        TC_LOG_INFO("playerbot.pool", "Pool replenishment needed - Alliance: T={}/H={}/D={}, Horde: T={}/H={}/D={}",
            allianceTanksReady, allianceHealersReady, allianceDPSReady,
            hordeTanksReady, hordeHealersReady, hordeDPSReady);
    }

    // Request overflow bots if JIT factory is enabled
    if (_config.behavior.enableJITFactory && _overflowNeededCallback)
    {
        if (allianceTanksReady < _config.behavior.minBotsPerRole)
            _overflowNeededCallback(BotRole::Tank, Faction::Alliance, 80,
                _config.behavior.minBotsPerRole - allianceTanksReady);
        if (allianceHealersReady < _config.behavior.minBotsPerRole)
            _overflowNeededCallback(BotRole::Healer, Faction::Alliance, 80,
                _config.behavior.minBotsPerRole - allianceHealersReady);
        if (allianceDPSReady < _config.behavior.minBotsPerRole)
            _overflowNeededCallback(BotRole::DPS, Faction::Alliance, 80,
                _config.behavior.minBotsPerRole - allianceDPSReady);

        if (hordeTanksReady < _config.behavior.minBotsPerRole)
            _overflowNeededCallback(BotRole::Tank, Faction::Horde, 80,
                _config.behavior.minBotsPerRole - hordeTanksReady);
        if (hordeHealersReady < _config.behavior.minBotsPerRole)
            _overflowNeededCallback(BotRole::Healer, Faction::Horde, 80,
                _config.behavior.minBotsPerRole - hordeHealersReady);
        if (hordeDPSReady < _config.behavior.minBotsPerRole)
            _overflowNeededCallback(BotRole::DPS, Faction::Horde, 80,
                _config.behavior.minBotsPerRole - hordeDPSReady);
    }
}

uint32 InstanceBotPool::GetAvailableCount(BotRole role, PoolType poolType) const
{
    Faction faction = GetFactionForPoolType(poolType);
    return GetAvailableCount(role, faction);
}

uint32 InstanceBotPool::GetAvailableCount(BotRole role, Faction faction) const
{
    std::shared_lock lock(_slotsMutex);

    uint32 count = 0;
    for (auto const& [guid, slot] : _slots)
    {
        if (slot.state == PoolSlotState::Ready &&
            slot.role == role &&
            slot.faction == faction)
        {
            ++count;
        }
    }
    return count;
}

uint32 InstanceBotPool::GetAvailableCountForLevel(BotRole role, Faction faction,
                                                    uint32 level, uint32 range) const
{
    std::shared_lock lock(_slotsMutex);

    uint32 count = 0;
    for (auto const& [guid, slot] : _slots)
    {
        if (slot.state == PoolSlotState::Ready &&
            slot.role == role &&
            slot.faction == faction &&
            slot.IsInLevelRange(level, range))
        {
            ++count;
        }
    }
    return count;
}

uint32 InstanceBotPool::GetTotalPoolSize() const
{
    std::shared_lock lock(_slotsMutex);
    return static_cast<uint32>(_slots.size());
}

uint32 InstanceBotPool::GetReadyCount() const
{
    std::shared_lock lock(_slotsMutex);

    uint32 count = 0;
    for (auto const& [guid, slot] : _slots)
    {
        if (slot.state == PoolSlotState::Ready)
            ++count;
    }
    return count;
}

uint32 InstanceBotPool::GetAssignedCount() const
{
    std::shared_lock lock(_slotsMutex);

    uint32 count = 0;
    for (auto const& [guid, slot] : _slots)
    {
        if (slot.state == PoolSlotState::Assigned)
            ++count;
    }
    return count;
}

// ============================================================================
// BOT ASSIGNMENT - PvE
// ============================================================================

std::vector<ObjectGuid> InstanceBotPool::AssignForDungeon(
    uint32 dungeonId, uint32 playerLevel, Faction playerFaction,
    uint32 tanksNeeded, uint32 healersNeeded, uint32 dpsNeeded)
{
    auto startTime = std::chrono::steady_clock::now();

    std::vector<ObjectGuid> result;

    // Select tanks
    auto tanks = SelectBots(BotRole::Tank, playerFaction, playerLevel, tanksNeeded,
        _config.levelConfig.normalDungeonMinGS);
    result.insert(result.end(), tanks.begin(), tanks.end());

    // Select healers
    auto healers = SelectBots(BotRole::Healer, playerFaction, playerLevel, healersNeeded,
        _config.levelConfig.normalDungeonMinGS);
    result.insert(result.end(), healers.begin(), healers.end());

    // Select DPS
    auto dps = SelectBots(BotRole::DPS, playerFaction, playerLevel, dpsNeeded,
        _config.levelConfig.normalDungeonMinGS);
    result.insert(result.end(), dps.begin(), dps.end());

    // Check if we got enough
    bool success = (tanks.size() == tanksNeeded &&
                   healers.size() == healersNeeded &&
                   dps.size() == dpsNeeded);

    if (!success)
    {
        // Release any partially selected bots
        ReleaseBots(result);
        result.clear();

        if (_config.logging.logAssignments)
        {
            TC_LOG_WARN("playerbot.pool",
                "Failed to assign bots for dungeon {}: needed T={}/H={}/D={}, got T={}/H={}/D={}",
                dungeonId, tanksNeeded, healersNeeded, dpsNeeded,
                tanks.size(), healers.size(), dps.size());
        }

        if (_assignmentFailedCallback)
        {
            _assignmentFailedCallback(InstanceType::Dungeon, dungeonId,
                "Insufficient bots available");
        }

        // Update failure stats
        std::unique_lock statsLock(_statsMutex);
        ++_stats.activity.failedRequestsThisHour;
        return result;
    }

    // Assign all selected bots
    for (ObjectGuid guid : result)
    {
        AssignBot(guid, 0, dungeonId, InstanceType::Dungeon);
    }

    // Record timing
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::unique_lock statsLock(_statsMutex);
        _stats.timing.RecordAssignment(duration);
        ++_stats.activity.dungeonsFilledThisHour;
        ++_stats.activity.successfulRequestsThisHour;
        _stats.activity.assignmentsThisHour += static_cast<uint32>(result.size());
    }

    if (_config.logging.logAssignments)
    {
        TC_LOG_INFO("playerbot.pool",
            "Assigned {} bots for dungeon {} in {}µs",
            result.size(), dungeonId, duration.count());
    }

    _statsDirty.store(true);
    return result;
}

std::vector<ObjectGuid> InstanceBotPool::AssignForRaid(
    uint32 raidId, uint32 playerLevel, Faction playerFaction,
    uint32 tanksNeeded, uint32 healersNeeded, uint32 dpsNeeded)
{
    auto startTime = std::chrono::steady_clock::now();

    std::vector<ObjectGuid> result;

    // Select tanks
    auto tanks = SelectBots(BotRole::Tank, playerFaction, playerLevel, tanksNeeded,
        _config.levelConfig.normalRaidMinGS);
    result.insert(result.end(), tanks.begin(), tanks.end());

    // Select healers
    auto healers = SelectBots(BotRole::Healer, playerFaction, playerLevel, healersNeeded,
        _config.levelConfig.normalRaidMinGS);
    result.insert(result.end(), healers.begin(), healers.end());

    // Select DPS
    auto dps = SelectBots(BotRole::DPS, playerFaction, playerLevel, dpsNeeded,
        _config.levelConfig.normalRaidMinGS);
    result.insert(result.end(), dps.begin(), dps.end());

    // Check if we got enough (allow partial for raids - progressive filling)
    uint32 totalNeeded = tanksNeeded + healersNeeded + dpsNeeded;
    uint32 totalGot = static_cast<uint32>(result.size());
    float fillPct = totalNeeded > 0 ? (static_cast<float>(totalGot) / totalNeeded) * 100.0f : 100.0f;

    // Assign all selected bots
    for (ObjectGuid guid : result)
    {
        AssignBot(guid, 0, raidId, InstanceType::Raid);
    }

    // Record timing
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::unique_lock statsLock(_statsMutex);
        _stats.timing.RecordAssignment(duration);
        ++_stats.activity.raidsFilledThisHour;
        if (fillPct >= 100.0f)
            ++_stats.activity.successfulRequestsThisHour;
        else
            ++_stats.activity.failedRequestsThisHour;
        _stats.activity.assignmentsThisHour += static_cast<uint32>(result.size());
    }

    if (_config.logging.logAssignments)
    {
        TC_LOG_INFO("playerbot.pool",
            "Assigned {} of {} bots for raid {} ({:.1f}% fill) in {}µs",
            result.size(), totalNeeded, raidId, fillPct, duration.count());
    }

    _statsDirty.store(true);
    return result;
}

// ============================================================================
// BOT ASSIGNMENT - PvP
// ============================================================================

BGAssignment InstanceBotPool::AssignForBattleground(
    uint32 bgTypeId, uint32 bracketLevel,
    uint32 allianceNeeded, uint32 hordeNeeded)
{
    auto startTime = std::chrono::steady_clock::now();

    BGAssignment result;

    // Select Alliance bots (mixed roles)
    uint32 allianceCount = allianceNeeded;
    while (allianceCount > 0)
    {
        // Distribute roles: roughly 15% tanks, 25% healers, 60% DPS
        BotRole role = BotRole::DPS;
        if (allianceCount > allianceNeeded * 0.85f)
            role = BotRole::Tank;
        else if (allianceCount > allianceNeeded * 0.60f)
            role = BotRole::Healer;

        ObjectGuid guid = SelectBestBot(role, Faction::Alliance, bracketLevel, 0);
        if (guid != ObjectGuid::Empty)
        {
            result.allianceBots.push_back(guid);
            --allianceCount;
        }
        else
        {
            // Try any role
            for (uint8 r = 0; r < static_cast<uint8>(BotRole::Max); ++r)
            {
                guid = SelectBestBot(static_cast<BotRole>(r), Faction::Alliance, bracketLevel, 0);
                if (guid != ObjectGuid::Empty)
                {
                    result.allianceBots.push_back(guid);
                    --allianceCount;
                    break;
                }
            }
            if (guid == ObjectGuid::Empty)
                break; // No more Alliance bots available
        }
    }

    // Select Horde bots (mixed roles)
    uint32 hordeCount = hordeNeeded;
    while (hordeCount > 0)
    {
        BotRole role = BotRole::DPS;
        if (hordeCount > hordeNeeded * 0.85f)
            role = BotRole::Tank;
        else if (hordeCount > hordeNeeded * 0.60f)
            role = BotRole::Healer;

        ObjectGuid guid = SelectBestBot(role, Faction::Horde, bracketLevel, 0);
        if (guid != ObjectGuid::Empty)
        {
            result.hordeBots.push_back(guid);
            --hordeCount;
        }
        else
        {
            // Try any role
            for (uint8 r = 0; r < static_cast<uint8>(BotRole::Max); ++r)
            {
                guid = SelectBestBot(static_cast<BotRole>(r), Faction::Horde, bracketLevel, 0);
                if (guid != ObjectGuid::Empty)
                {
                    result.hordeBots.push_back(guid);
                    --hordeCount;
                    break;
                }
            }
            if (guid == ObjectGuid::Empty)
                break;
        }
    }

    // Check success
    result.success = (result.allianceBots.size() >= allianceNeeded &&
                     result.hordeBots.size() >= hordeNeeded);

    // Assign all selected bots
    for (ObjectGuid guid : result.allianceBots)
        AssignBot(guid, 0, bgTypeId, InstanceType::Battleground);
    for (ObjectGuid guid : result.hordeBots)
        AssignBot(guid, 0, bgTypeId, InstanceType::Battleground);

    // Record timing
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::unique_lock statsLock(_statsMutex);
        _stats.timing.RecordAssignment(duration);
        ++_stats.activity.battlegroundsFilledThisHour;
        if (result.success)
            ++_stats.activity.successfulRequestsThisHour;
        else
            ++_stats.activity.failedRequestsThisHour;
        _stats.activity.assignmentsThisHour +=
            static_cast<uint32>(result.allianceBots.size() + result.hordeBots.size());
    }

    if (_config.logging.logAssignments)
    {
        TC_LOG_INFO("playerbot.pool",
            "BG {} assignment: Alliance={}/{}, Horde={}/{}, success={}, {}µs",
            bgTypeId, result.allianceBots.size(), allianceNeeded,
            result.hordeBots.size(), hordeNeeded, result.success, duration.count());
    }

    _statsDirty.store(true);
    return result;
}

ArenaAssignment InstanceBotPool::AssignForArena(
    uint32 arenaType, uint32 bracketLevel, Faction playerFaction,
    uint32 teammatesNeeded, uint32 opponentsNeeded)
{
    auto startTime = std::chrono::steady_clock::now();

    ArenaAssignment result;
    Faction opponentFaction = (playerFaction == Faction::Alliance) ? Faction::Horde : Faction::Alliance;

    // Select teammates (same faction as player)
    auto teammates = SelectBots(BotRole::DPS, playerFaction, bracketLevel, teammatesNeeded, 0);
    result.teammates = teammates;

    // Select opponents (opposite faction)
    auto opponents = SelectBots(BotRole::DPS, opponentFaction, bracketLevel, opponentsNeeded, 0);
    result.opponents = opponents;

    result.success = (teammates.size() == teammatesNeeded &&
                     opponents.size() == opponentsNeeded);

    // Assign all selected bots
    for (ObjectGuid guid : result.teammates)
        AssignBot(guid, 0, arenaType, InstanceType::Arena);
    for (ObjectGuid guid : result.opponents)
        AssignBot(guid, 0, arenaType, InstanceType::Arena);

    // Record timing
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::unique_lock statsLock(_statsMutex);
        _stats.timing.RecordAssignment(duration);
        ++_stats.activity.arenasFilledThisHour;
        if (result.success)
            ++_stats.activity.successfulRequestsThisHour;
        else
            ++_stats.activity.failedRequestsThisHour;
    }

    if (_config.logging.logAssignments)
    {
        TC_LOG_INFO("playerbot.pool",
            "Arena {} assignment: teammates={}/{}, opponents={}/{}, success={}, {}µs",
            arenaType, teammates.size(), teammatesNeeded,
            opponents.size(), opponentsNeeded, result.success, duration.count());
    }

    _statsDirty.store(true);
    return result;
}

// ============================================================================
// BOT RELEASE
// ============================================================================

void InstanceBotPool::ReleaseBots(std::vector<ObjectGuid> const& bots)
{
    for (ObjectGuid guid : bots)
        ReleaseBot(guid, true);
}

void InstanceBotPool::ReleaseBot(ObjectGuid botGuid, bool success)
{
    std::unique_lock lock(_slotsMutex);

    auto it = _slots.find(botGuid);
    if (it == _slots.end())
        return;

    if (it->second.state != PoolSlotState::Assigned)
        return;

    it->second.ReleaseFromInstance(success);

    ++_stats.activity.releasesThisHour;
    ++_stats.activity.releasesToday;

    if (_config.logging.logAssignments)
    {
        TC_LOG_DEBUG("playerbot.pool", "Released bot {} from instance (success={})",
            botGuid.ToString(), success);
    }

    _statsDirty.store(true);
}

void InstanceBotPool::ReleaseBotsFromInstance(uint32 instanceId)
{
    std::unique_lock lock(_slotsMutex);

    for (auto& [guid, slot] : _slots)
    {
        if (slot.state == PoolSlotState::Assigned &&
            slot.currentInstanceId == instanceId)
        {
            slot.ReleaseFromInstance(true);
            ++_stats.activity.releasesThisHour;
        }
    }

    _statsDirty.store(true);
}

// ============================================================================
// RESERVATION SYSTEM
// ============================================================================

uint32 InstanceBotPool::CreateReservation(
    InstanceType type, uint32 contentId, uint32 playerLevel, Faction playerFaction,
    uint32 tanksNeeded, uint32 healersNeeded, uint32 dpsNeeded,
    uint32 allianceNeeded, uint32 hordeNeeded)
{
    uint32 reservationId = _nextReservationId.fetch_add(1);

    Reservation reservation;
    reservation.reservationId = reservationId;
    reservation.instanceType = type;
    reservation.contentId = contentId;
    reservation.playerLevel = playerLevel;
    reservation.playerFaction = playerFaction;
    reservation.tanksNeeded = tanksNeeded;
    reservation.healersNeeded = healersNeeded;
    reservation.dpsNeeded = dpsNeeded;
    reservation.allianceNeeded = allianceNeeded;
    reservation.hordeNeeded = hordeNeeded;
    reservation.createdAt = std::chrono::steady_clock::now();
    reservation.deadline = reservation.createdAt + _config.timing.reservationTimeout;

    // Reserve bots
    if (RequiresBothFactions(type))
    {
        // PvP - reserve from both factions
        auto allianceBots = SelectBots(BotRole::DPS, Faction::Alliance, playerLevel, allianceNeeded, 0);
        auto hordeBots = SelectBots(BotRole::DPS, Faction::Horde, playerLevel, hordeNeeded, 0);

        std::unique_lock lock(_slotsMutex);
        for (ObjectGuid guid : allianceBots)
        {
            auto it = _slots.find(guid);
            if (it != _slots.end() && it->second.Reserve(reservationId))
            {
                reservation.reservedBots.push_back(guid);
                ++reservation.allianceFulfilled;
            }
        }
        for (ObjectGuid guid : hordeBots)
        {
            auto it = _slots.find(guid);
            if (it != _slots.end() && it->second.Reserve(reservationId))
            {
                reservation.reservedBots.push_back(guid);
                ++reservation.hordeFulfilled;
            }
        }
    }
    else
    {
        // PvE - reserve by role
        auto tanks = SelectBots(BotRole::Tank, playerFaction, playerLevel, tanksNeeded, 0);
        auto healers = SelectBots(BotRole::Healer, playerFaction, playerLevel, healersNeeded, 0);
        auto dps = SelectBots(BotRole::DPS, playerFaction, playerLevel, dpsNeeded, 0);

        std::unique_lock lock(_slotsMutex);
        for (ObjectGuid guid : tanks)
        {
            auto it = _slots.find(guid);
            if (it != _slots.end() && it->second.Reserve(reservationId))
            {
                reservation.reservedBots.push_back(guid);
                ++reservation.tanksFulfilled;
            }
        }
        for (ObjectGuid guid : healers)
        {
            auto it = _slots.find(guid);
            if (it != _slots.end() && it->second.Reserve(reservationId))
            {
                reservation.reservedBots.push_back(guid);
                ++reservation.healersFulfilled;
            }
        }
        for (ObjectGuid guid : dps)
        {
            auto it = _slots.find(guid);
            if (it != _slots.end() && it->second.Reserve(reservationId))
            {
                reservation.reservedBots.push_back(guid);
                ++reservation.dpsFulfilled;
            }
        }
    }

    // Store reservation
    {
        std::lock_guard lock(_reservationMutex);
        _reservations[reservationId] = std::move(reservation);
    }

    ++_stats.activity.reservationsThisHour;

    if (_config.logging.logReservations)
    {
        TC_LOG_INFO("playerbot.pool",
            "Created reservation {} for {} content {}: {:.1f}% fulfilled",
            reservationId, InstanceTypeToString(type), contentId,
            GetReservationFulfillment(reservationId));
    }

    _statsDirty.store(true);
    return reservationId;
}

bool InstanceBotPool::CanFulfillReservation(uint32 reservationId) const
{
    std::lock_guard lock(_reservationMutex);

    auto it = _reservations.find(reservationId);
    if (it == _reservations.end())
        return false;

    return it->second.IsFulfilled();
}

float InstanceBotPool::GetReservationFulfillment(uint32 reservationId) const
{
    std::lock_guard lock(_reservationMutex);

    auto it = _reservations.find(reservationId);
    if (it == _reservations.end())
        return 0.0f;

    return it->second.GetFulfillmentPct();
}

std::vector<ObjectGuid> InstanceBotPool::FulfillReservation(uint32 reservationId)
{
    std::vector<ObjectGuid> result;

    std::lock_guard resLock(_reservationMutex);

    auto it = _reservations.find(reservationId);
    if (it == _reservations.end())
        return result;

    Reservation& reservation = it->second;

    // Transition all reserved bots to assigned
    std::unique_lock slotLock(_slotsMutex);
    for (ObjectGuid guid : reservation.reservedBots)
    {
        auto slotIt = _slots.find(guid);
        if (slotIt != _slots.end())
        {
            if (slotIt->second.FulfillReservation(0, reservation.contentId, reservation.instanceType))
            {
                result.push_back(guid);
            }
        }
    }
    slotLock.unlock();

    // Remove reservation
    _reservations.erase(it);

    if (_config.logging.logReservations)
    {
        TC_LOG_INFO("playerbot.pool", "Fulfilled reservation {}: {} bots assigned",
            reservationId, result.size());
    }

    _statsDirty.store(true);
    return result;
}

void InstanceBotPool::CancelReservation(uint32 reservationId)
{
    std::lock_guard resLock(_reservationMutex);

    auto it = _reservations.find(reservationId);
    if (it == _reservations.end())
        return;

    // Return reserved bots to ready state
    std::unique_lock slotLock(_slotsMutex);
    for (ObjectGuid guid : it->second.reservedBots)
    {
        auto slotIt = _slots.find(guid);
        if (slotIt != _slots.end())
        {
            slotIt->second.CancelReservation();
        }
    }
    slotLock.unlock();

    _reservations.erase(it);
    ++_stats.activity.cancellationsThisHour;

    if (_config.logging.logReservations)
    {
        TC_LOG_INFO("playerbot.pool", "Cancelled reservation {}", reservationId);
    }

    _statsDirty.store(true);
}

std::chrono::milliseconds InstanceBotPool::GetEstimatedWaitTime(
    InstanceType type, uint32 /*contentId*/,
    uint32 tanksNeeded, uint32 healersNeeded, uint32 dpsNeeded) const
{
    // Check current availability
    uint32 availableTanks = GetReadyCount(); // Simplified - would need role breakdown
    uint32 totalNeeded = tanksNeeded + healersNeeded + dpsNeeded;

    if (GetReadyCount() >= totalNeeded)
        return std::chrono::milliseconds(0); // Instant

    // Estimate based on JIT factory speed
    uint32 deficit = totalNeeded - GetReadyCount();
    uint32 creationRate = _config.poolSize.overflowCreationRate;
    if (creationRate == 0)
        creationRate = 1;

    uint32 estimatedSeconds = deficit / creationRate;

    return std::chrono::seconds(estimatedSeconds);
}

// ============================================================================
// QUERIES
// ============================================================================

InstanceBotSlot const* InstanceBotPool::GetSlot(ObjectGuid botGuid) const
{
    std::shared_lock lock(_slotsMutex);

    auto it = _slots.find(botGuid);
    if (it == _slots.end())
        return nullptr;

    return &it->second;
}

bool InstanceBotPool::IsPoolBot(ObjectGuid botGuid) const
{
    std::shared_lock lock(_slotsMutex);
    return _slots.find(botGuid) != _slots.end();
}

uint32 InstanceBotPool::GetBotInstanceId(ObjectGuid botGuid) const
{
    std::shared_lock lock(_slotsMutex);

    auto it = _slots.find(botGuid);
    if (it == _slots.end())
        return 0;

    return it->second.currentInstanceId;
}

bool InstanceBotPool::CanProvideBotsFor(
    InstanceType /*type*/,
    uint32 tanksNeeded, uint32 healersNeeded, uint32 dpsNeeded) const
{
    // Simple check - could be more sophisticated with role breakdown
    uint32 totalNeeded = tanksNeeded + healersNeeded + dpsNeeded;
    return GetReadyCount() >= totalNeeded;
}

// ============================================================================
// STATISTICS
// ============================================================================

PoolStatistics InstanceBotPool::GetStatistics() const
{
    std::shared_lock lock(_statsMutex);
    return _stats;
}

void InstanceBotPool::PrintStatusReport() const
{
    PoolStatistics stats = GetStatistics();

    TC_LOG_INFO("playerbot.pool", "=== Instance Bot Pool Status ===");
    TC_LOG_INFO("playerbot.pool", "Total: {} | Ready: {} | Assigned: {} | Cooldown: {}",
        stats.slotStats.GetTotal(), stats.slotStats.readySlots,
        stats.slotStats.assignedSlots, stats.slotStats.cooldownSlots);
    TC_LOG_INFO("playerbot.pool", "Utilization: {:.1f}% | Availability: {:.1f}%",
        stats.GetUtilization(), stats.GetAvailability());
    TC_LOG_INFO("playerbot.pool", "Hourly: {} assignments, {} releases, {} failures",
        stats.activity.assignmentsThisHour, stats.activity.releasesThisHour,
        stats.activity.failedRequestsThisHour);
    TC_LOG_INFO("playerbot.pool", "Avg assignment time: {}µs",
        stats.timing.avgAssignmentTime.count());
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void InstanceBotPool::SetConfig(InstanceBotPoolConfig const& config)
{
    _config = config;
}

void InstanceBotPool::SetAssignmentFailedCallback(AssignmentFailedCallback callback)
{
    _assignmentFailedCallback = std::move(callback);
}

void InstanceBotPool::SetOverflowNeededCallback(OverflowNeededCallback callback)
{
    _overflowNeededCallback = std::move(callback);
}

// ============================================================================
// INTERNAL METHODS - Bot Creation
// ============================================================================

ObjectGuid InstanceBotPool::CreatePoolBot(BotRole role, PoolType poolType, uint32 level)
{
    // Use BotCloneEngine to create an actual bot character
    Faction faction = GetFactionForPoolType(poolType);

    // Clone a bot using the template repository and clone engine
    CloneResult result = sBotCloneEngine->Clone(role, faction, level, 0);

    if (!result.success)
    {
        TC_LOG_WARN("playerbot.pool", "InstanceBotPool::CreatePoolBot - Failed to create bot: {}",
            result.errorMessage);
        return ObjectGuid::Empty;
    }

    TC_LOG_INFO("playerbot.pool", "InstanceBotPool::CreatePoolBot - Created bot {} ({}), Level {}, Role {}",
        result.botName, result.botGuid.ToString(), level, BotRoleToString(role));

    // Create slot for the newly created bot
    InstanceBotSlot slot;
    slot.Initialize(result.botGuid, result.accountId, result.botName, poolType, role);
    slot.level = level;
    slot.faction = faction;
    slot.gearScore = result.gearScore;
    slot.playerClass = result.playerClass;
    slot.specId = result.specId;
    slot.ForceState(PoolSlotState::Warming); // Bot needs to be logged in

    {
        std::unique_lock lock(_slotsMutex);
        _slots[result.botGuid] = std::move(slot);
    }

    // Queue the bot for login via BotWorldSessionMgr
    // The bot will be marked Ready once login is complete
    if (!WarmUpBot(result.botGuid))
    {
        TC_LOG_WARN("playerbot.pool", "InstanceBotPool::CreatePoolBot - Failed to queue bot {} for warmup",
            result.botGuid.ToString());
        // Still return the GUID - bot exists in database, can be warmed later
    }

    _statsDirty.store(true);
    return result.botGuid;
}

bool InstanceBotPool::WarmUpBot(ObjectGuid botGuid)
{
    // Log the bot into the world via BotWorldSessionMgr
    // This queues the bot for rate-limited spawning
    if (!sBotWorldSessionMgr || !sBotWorldSessionMgr->IsEnabled())
    {
        TC_LOG_WARN("playerbot.pool", "InstanceBotPool::WarmUpBot - BotWorldSessionMgr not available");
        return false;
    }

    // Get account ID from slot
    uint32 accountId = 0;
    {
        std::shared_lock lock(_slotsMutex);
        auto it = _slots.find(botGuid);
        if (it != _slots.end())
            accountId = it->second.accountId;
    }

    if (accountId == 0)
    {
        TC_LOG_WARN("playerbot.pool", "InstanceBotPool::WarmUpBot - No account ID for bot {}",
            botGuid.ToString());
        return false;
    }

    // Queue bot for login - BotWorldSessionMgr handles rate limiting
    // Pass bypassLimit=true to allow pool bots to exceed MaxBots limit
    // (level distribution system will balance totals over time)
    bool queued = sBotWorldSessionMgr->AddPlayerBot(botGuid, accountId, true /* bypassLimit */);

    if (queued)
    {
        TC_LOG_DEBUG("playerbot.pool", "InstanceBotPool::WarmUpBot - Queued bot {} for login",
            botGuid.ToString());
    }
    else
    {
        TC_LOG_WARN("playerbot.pool", "InstanceBotPool::WarmUpBot - Failed to queue bot {} for login",
            botGuid.ToString());
    }

    return queued;
}

void InstanceBotPool::OnBotWarmupComplete(ObjectGuid botGuid, bool success)
{
    std::unique_lock lock(_slotsMutex);

    auto it = _slots.find(botGuid);
    if (it == _slots.end())
        return;

    if (success)
    {
        it->second.TransitionTo(PoolSlotState::Ready);
        ++_stats.activity.warmupsThisHour;
    }
    else
    {
        it->second.ForceState(PoolSlotState::Maintenance);
    }

    _statsDirty.store(true);
}

// ============================================================================
// INTERNAL METHODS - Bot Selection
// ============================================================================

ObjectGuid InstanceBotPool::SelectBestBot(BotRole role, Faction faction,
                                           uint32 level, uint32 minGearScore)
{
    std::unique_lock lock(_slotsMutex);

    ObjectGuid bestGuid = ObjectGuid::Empty;
    float bestScore = -1.0f;

    for (auto& [guid, slot] : _slots)
    {
        if (slot.state != PoolSlotState::Ready)
            continue;
        if (slot.role != role)
            continue;
        if (slot.faction != faction)
            continue;
        if (!slot.IsInLevelRange(level, 10))
            continue;
        if (slot.gearScore < minGearScore)
            continue;

        float score = slot.CalculateAssignmentScore(level, minGearScore);
        if (score > bestScore)
        {
            bestScore = score;
            bestGuid = guid;
        }
    }

    return bestGuid;
}

std::vector<ObjectGuid> InstanceBotPool::SelectBots(BotRole role, Faction faction,
                                                     uint32 level, uint32 count,
                                                     uint32 minGearScore)
{
    std::vector<ObjectGuid> result;
    result.reserve(count);

    std::unique_lock lock(_slotsMutex);

    // Collect candidates
    std::vector<std::pair<ObjectGuid, float>> candidates;

    for (auto& [guid, slot] : _slots)
    {
        if (slot.state != PoolSlotState::Ready)
            continue;
        if (slot.role != role)
            continue;
        if (slot.faction != faction)
            continue;
        if (!slot.IsInLevelRange(level, 10))
            continue;
        if (slot.gearScore < minGearScore)
            continue;

        float score = slot.CalculateAssignmentScore(level, minGearScore);
        candidates.emplace_back(guid, score);
    }

    // Sort by score (descending)
    std::sort(candidates.begin(), candidates.end(),
        [](auto const& a, auto const& b) { return a.second > b.second; });

    // Take top N
    for (size_t i = 0; i < count && i < candidates.size(); ++i)
    {
        result.push_back(candidates[i].first);
    }

    return result;
}

bool InstanceBotPool::AssignBot(ObjectGuid botGuid, uint32 instanceId,
                                 uint32 contentId, InstanceType type)
{
    std::unique_lock lock(_slotsMutex);

    auto it = _slots.find(botGuid);
    if (it == _slots.end())
        return false;

    return it->second.AssignToInstance(instanceId, contentId, type);
}

// ============================================================================
// INTERNAL METHODS - Pool Maintenance
// ============================================================================

void InstanceBotPool::ProcessWarmingRetries()
{
    // Retry warmup for bots stuck in Warming state
    // This handles the async database commit delay - character might not be
    // queryable immediately after creation
    std::vector<ObjectGuid> botsToWarm;

    {
        std::shared_lock lock(_slotsMutex);
        for (auto const& [guid, slot] : _slots)
        {
            if (slot.state == PoolSlotState::Warming)
            {
                // Retry after 3 seconds to allow async DB commit to complete
                // (async commits can take longer during pool warming with many bots)
                auto timeSinceStateChange = slot.TimeSinceStateChange();
                if (timeSinceStateChange >= std::chrono::seconds(3))
                {
                    botsToWarm.push_back(guid);
                }
            }
        }
    }

    // Log how many bots need retry
    if (!botsToWarm.empty())
    {
        TC_LOG_INFO("playerbot.pool", "ProcessWarmingRetries - Retrying warmup for {} bots",
            botsToWarm.size());
    }

    // Try to warm each bot (outside the lock to avoid deadlock)
    uint32 successCount = 0;
    for (ObjectGuid const& guid : botsToWarm)
    {
        if (WarmUpBot(guid))
        {
            ++successCount;
            TC_LOG_DEBUG("playerbot.pool", "ProcessWarmingRetries - Successfully queued bot {} for warmup",
                guid.ToString());
        }
        else
        {
            TC_LOG_DEBUG("playerbot.pool", "ProcessWarmingRetries - Failed to queue bot {} (will retry later)",
                guid.ToString());
        }
    }

    if (!botsToWarm.empty())
    {
        TC_LOG_INFO("playerbot.pool", "ProcessWarmingRetries - Queued {}/{} bots for warmup",
            successCount, botsToWarm.size());
    }
}

void InstanceBotPool::ProcessCooldowns()
{
    std::unique_lock lock(_slotsMutex);

    auto cooldownDuration = _config.timing.cooldownDuration;

    for (auto& [guid, slot] : _slots)
    {
        if (slot.state == PoolSlotState::Cooldown)
        {
            if (slot.IsCooldownExpired(cooldownDuration))
            {
                slot.TransitionTo(PoolSlotState::Ready);
                ++_stats.activity.cooldownsExpiredThisHour;

                if (_config.logging.logCooldowns)
                {
                    TC_LOG_DEBUG("playerbot.pool", "Bot {} cooldown expired, now ready",
                        guid.ToString());
                }
            }
        }
    }

    _statsDirty.store(true);
}

void InstanceBotPool::ProcessReservations()
{
    std::lock_guard lock(_reservationMutex);

    auto now = std::chrono::steady_clock::now();
    std::vector<uint32> expiredReservations;

    for (auto const& [id, reservation] : _reservations)
    {
        if (reservation.IsExpired())
        {
            expiredReservations.push_back(id);
        }
    }

    // Can't call CancelReservation while holding lock, so collect and process
    for (uint32 id : expiredReservations)
    {
        // Return reserved bots to ready state
        auto it = _reservations.find(id);
        if (it != _reservations.end())
        {
            std::unique_lock slotLock(_slotsMutex);
            for (ObjectGuid guid : it->second.reservedBots)
            {
                auto slotIt = _slots.find(guid);
                if (slotIt != _slots.end())
                {
                    slotIt->second.CancelReservation();
                }
            }
            slotLock.unlock();

            if (_config.logging.logReservations)
            {
                TC_LOG_INFO("playerbot.pool", "Reservation {} expired and cancelled", id);
            }

            _reservations.erase(it);
            ++_stats.activity.timeoutRequestsThisHour;
        }
    }

    _statsDirty.store(true);
}

void InstanceBotPool::UpdateStatistics()
{
    if (!_statsDirty.load())
        return;

    std::shared_lock slotLock(_slotsMutex);
    std::unique_lock statsLock(_statsMutex);

    // Reset slot stats
    _stats.slotStats.Reset();

    // Count slots by state
    for (auto const& [guid, slot] : _slots)
    {
        switch (slot.state)
        {
            case PoolSlotState::Empty:       ++_stats.slotStats.emptySlots; break;
            case PoolSlotState::Creating:    ++_stats.slotStats.creatingSlots; break;
            case PoolSlotState::Warming:     ++_stats.slotStats.warmingSlots; break;
            case PoolSlotState::Ready:       ++_stats.slotStats.readySlots; break;
            case PoolSlotState::Reserved:    ++_stats.slotStats.reservedSlots; break;
            case PoolSlotState::Assigned:    ++_stats.slotStats.assignedSlots; break;
            case PoolSlotState::Cooldown:    ++_stats.slotStats.cooldownSlots; break;
            case PoolSlotState::Maintenance: ++_stats.slotStats.maintenanceSlots; break;
            default: break;
        }

        // Update role stats
        auto roleIdx = static_cast<size_t>(slot.role);
        if (roleIdx < _stats.roleStats.size())
        {
            ++_stats.roleStats[roleIdx].totalSlots;
            if (slot.state == PoolSlotState::Ready)
                ++_stats.roleStats[roleIdx].readySlots;
            else if (slot.state == PoolSlotState::Assigned)
                ++_stats.roleStats[roleIdx].assignedSlots;
        }

        // Update faction stats
        auto factionIdx = static_cast<size_t>(slot.faction);
        if (factionIdx < _stats.factionStats.size())
        {
            ++_stats.factionStats[factionIdx].totalSlots;
            if (slot.state == PoolSlotState::Ready)
                ++_stats.factionStats[factionIdx].readySlots;
            else if (slot.state == PoolSlotState::Assigned)
                ++_stats.factionStats[factionIdx].assignedSlots;
        }
    }

    _stats.timestamp = std::chrono::system_clock::now();
    _statsDirty.store(false);
}

void InstanceBotPool::CheckHourlyReset()
{
    auto now = std::chrono::system_clock::now();
    auto hoursSinceStart = std::chrono::duration_cast<std::chrono::hours>(now - _hourStart);

    if (hoursSinceStart >= std::chrono::hours(1))
    {
        std::unique_lock lock(_statsMutex);
        _stats.ResetHourly();
        _hourStart = now;

        if (_config.logging.logDetailedStats)
        {
            TC_LOG_INFO("playerbot.pool", "Hourly statistics reset");
        }
    }

    auto daysSinceStart = std::chrono::duration_cast<std::chrono::hours>(now - _dayStart);
    if (daysSinceStart >= std::chrono::hours(24))
    {
        std::unique_lock lock(_statsMutex);
        _stats.ResetDaily();
        _dayStart = now;

        if (_config.logging.logDetailedStats)
        {
            TC_LOG_INFO("playerbot.pool", "Daily statistics reset");
        }
    }
}

// ============================================================================
// INTERNAL METHODS - Database
// ============================================================================

void InstanceBotPool::SyncToDatabase()
{
    // TODO: Implement database persistence
    // This would save pool state to playerbot_instance_pool table
}

void InstanceBotPool::LoadFromDatabase()
{
    // TODO: Implement database loading
    // This would restore pool state from playerbot_instance_pool table
}

// ============================================================================
// STRING UTILITIES
// ============================================================================

std::string PoolStatistics::ToSummaryString() const
{
    return fmt::format(
        "Pool: {} total, {} ready ({:.1f}%), {} assigned ({:.1f}%)",
        slotStats.GetTotal(),
        slotStats.readySlots,
        slotStats.GetAvailabilityPct(),
        slotStats.assignedSlots,
        slotStats.GetUtilizationPct());
}

std::string PoolStatistics::ToDetailedString() const
{
    return fmt::format(
        "Pool Statistics:\n"
        "  Slots: total={}, empty={}, creating={}, warming={}, ready={}, reserved={}, assigned={}, cooldown={}, maintenance={}\n"
        "  Activity: assignments={}, releases={}, jit={}, reservations={}, cancels={}\n"
        "  Timing: avg_assign={}µs, avg_warmup={}ms, peak_assign={}µs\n"
        "  Success: {:.1f}%",
        slotStats.GetTotal(),
        slotStats.emptySlots, slotStats.creatingSlots, slotStats.warmingSlots,
        slotStats.readySlots, slotStats.reservedSlots, slotStats.assignedSlots,
        slotStats.cooldownSlots, slotStats.maintenanceSlots,
        activity.assignmentsThisHour, activity.releasesThisHour, activity.jitCreationsThisHour,
        activity.reservationsThisHour, activity.cancellationsThisHour,
        timing.avgAssignmentTime.count(), timing.avgWarmupTime.count(), timing.peakAssignmentTime.count(),
        activity.GetSuccessRatePct());
}

void PoolStatistics::PrintToLog() const
{
    TC_LOG_INFO("playerbot.pool", "{}", ToDetailedString());
}

// ============================================================================
// CONFIGURATION LOADING
// ============================================================================

void InstanceBotPoolConfig::LoadFromConfig()
{
    // Delegate to pool's LoadConfig
}

bool InstanceBotPoolConfig::Validate() const
{
    // Validate pool sizes are reasonable
    if (poolSize.GetTotalWarmPool() > 10000)
    {
        TC_LOG_WARN("playerbot.pool", "Total pool size {} exceeds recommended maximum of 10000",
            poolSize.GetTotalWarmPool());
    }

    // Validate timing
    if (timing.cooldownDuration < std::chrono::seconds(30))
    {
        TC_LOG_WARN("playerbot.pool", "Cooldown duration {}s is very short, may cause thrashing",
            timing.cooldownDuration.count());
    }

    return true;
}

InstanceBotPoolConfig InstanceBotPoolConfig::GetDefault()
{
    return InstanceBotPoolConfig();
}

void InstanceBotPoolConfig::PrintToLog() const
{
    TC_LOG_INFO("playerbot.pool", "Instance Bot Pool Configuration:");
    TC_LOG_INFO("playerbot.pool", "  Enabled: {}", enabled);
    TC_LOG_INFO("playerbot.pool", "  Alliance: T={}, H={}, D={}",
        poolSize.allianceTanks, poolSize.allianceHealers, poolSize.allianceDPS);
    TC_LOG_INFO("playerbot.pool", "  Horde: T={}, H={}, D={}",
        poolSize.hordeTanks, poolSize.hordeHealers, poolSize.hordeDPS);
    TC_LOG_INFO("playerbot.pool", "  Cooldown: {}s", timing.cooldownDuration.count());
}

void JITFactoryConfig::LoadFromConfig()
{
    enabled = sPlayerbotConfig->GetBool("Playerbot.Instance.JIT.Enable", true);
    maxConcurrentCreations = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.MaxConcurrentCreations", 10);
    recycleTimeoutMinutes = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.RecycleTimeoutMinutes", 5);
    maxRecycledBots = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.MaxRecycledBots", 100);
}

void InstanceOrchestratorConfig::LoadFromConfig()
{
    enabled = sPlayerbotConfig->GetBool("Playerbot.Instance.Orchestrator.Enable", true);
    dungeonTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.Orchestrator.DungeonTimeoutMs", 30000);
    raidTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.Orchestrator.RaidTimeoutMs", 60000);
    bgTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.Orchestrator.BattlegroundTimeoutMs", 120000);
    arenaTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.Orchestrator.ArenaTimeoutMs", 15000);
    useOverflowThresholdPct = sPlayerbotConfig->GetInt("Playerbot.Instance.Orchestrator.OverflowThresholdPct", 80);
}

} // namespace Playerbot
