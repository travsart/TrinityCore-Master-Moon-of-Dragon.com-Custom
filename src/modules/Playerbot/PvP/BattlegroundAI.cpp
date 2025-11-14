/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BattlegroundAI.h"
#include "Player.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "GameObject.h"
#include "World.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

BattlegroundAI* BattlegroundAI::instance()
{
    static BattlegroundAI instance;
    return &instance;
}

BattlegroundAI::BattlegroundAI()
{
    TC_LOG_INFO("playerbot", "BattlegroundAI initialized");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void BattlegroundAI::Initialize()
{
    ::std::lock_guard lock(_mutex);

    TC_LOG_INFO("playerbot", "BattlegroundAI: Initializing battleground strategies...");

    InitializeWSGStrategy();
    InitializeABStrategy();
    InitializeAVStrategy();
    InitializeEOTSStrategy();
    InitializeSiegeStrategy();

    TC_LOG_INFO("playerbot", "BattlegroundAI: Initialization complete");
}

void BattlegroundAI::InitializeWSGStrategy()
{
    FlagBGStrategy wsg;
    wsg.escortFlagCarrier = true;
    wsg.defendFlagRoom = true;
    wsg.killEnemyFC = true;
    wsg.fcEscortCount = 3;
    wsg.flagRoomDefenders = 2;

    // Warsong Gulch flag spawns
    wsg.friendlyFlagSpawn = Position(1540.0f, 1481.0f, 352.0f, 0.0f); // Alliance flag room
    wsg.enemyFlagSpawn = Position(916.0f, 1434.0f, 346.0f, 0.0f);     // Horde flag room

    _flagStrategies[BGType::WARSONG_GULCH] = wsg;

    // Twin Peaks uses same strategy with different coordinates
    FlagBGStrategy tp = wsg;
    tp.friendlyFlagSpawn = Position(2118.0f, 191.0f, 135.0f, 0.0f);
    tp.enemyFlagSpawn = Position(1578.0f, 344.0f, 2.0f, 0.0f);
    _flagStrategies[BGType::TWIN_PEAKS] = tp;
}

void BattlegroundAI::InitializeABStrategy()
{
    BaseBGStrategy ab;
    ab.rotateCaptures = true;
    ab.minDefendersPerBase = 2;

    // Arathi Basin base locations
    ab.baseLocations.push_back(Position(1166.0f, 1200.0f, -56.0f, 0.0f)); // Stables
    ab.baseLocations.push_back(Position(1051.0f, 1152.0f, -56.0f, 0.0f)); // Blacksmith
    ab.baseLocations.push_back(Position(1006.0f, 1447.0f, -65.0f, 0.0f)); // Farm
    ab.baseLocations.push_back(Position(780.0f, 1185.0f, 15.0f, 0.0f));   // Gold Mine
    ab.baseLocations.push_back(Position(1146.0f, 816.0f, -98.0f, 0.0f));  // Lumber Mill

    // Blacksmith is priority (center position)
    ab.priorityBases.push_back(1); // Blacksmith index

    _baseStrategies[BGType::ARATHI_BASIN] = ab;

    // Battle for Gilneas uses similar strategy
    BaseBGStrategy bfg = ab;
    bfg.baseLocations.clear();
    bfg.baseLocations.push_back(Position(1057.0f, 1148.0f, 3.5f, 0.0f)); // Waterworks
    bfg.baseLocations.push_back(Position(980.0f, 1251.0f, 16.8f, 0.0f)); // Lighthouse
    bfg.baseLocations.push_back(Position(887.0f, 1151.0f, 8.0f, 0.0f));  // Mines
    _baseStrategies[BGType::BATTLE_FOR_GILNEAS] = bfg;
}

void BattlegroundAI::InitializeAVStrategy()
{
    AVStrategy av;
    av.captureGraveyards = true;
    av.captureTowers = true;
    av.killBoss = true;
    av.escortNPCs = true;
    av.collectResources = true;

    // Alterac Valley graveyard locations (simplified)
    av.graveyardLocations.push_back(Position(638.0f, -270.0f, 30.0f, 0.0f));  // Stonehearth GY
    av.graveyardLocations.push_back(Position(-202.0f, -112.0f, 79.0f, 0.0f)); // Iceblood GY
    av.graveyardLocations.push_back(Position(-611.0f, -396.0f, 61.0f, 0.0f)); // Frostwolf GY

    // Tower locations
    av.towerLocations.push_back(Position(553.0f, -78.0f, 51.0f, 0.0f));   // Dun Baldar North
    av.towerLocations.push_back(Position(674.0f, -143.0f, 64.0f, 0.0f));  // Dun Baldar South
    av.towerLocations.push_back(Position(-1361.0f, -219.0f, 98.0f, 0.0f)); // Frostwolf East
    av.towerLocations.push_back(Position(-1302.0f, -316.0f, 113.0f, 0.0f)); // Frostwolf West

    // Boss locations
    av.bossLocation = Position(-1370.0f, -219.0f, 98.0f, 0.0f); // Drek'Thar / Vanndar

    _avStrategies[BGType::ALTERAC_VALLEY] = av;
}

void BattlegroundAI::InitializeEOTSStrategy()
{
    EOTSStrategy eots;
    eots.captureBases = true;
    eots.captureFlag = true;
    eots.prioritizeFlagWhenLeading = false;
    eots.flagCarrierEscortCount = 3;

    // Eye of the Storm base locations
    eots.baseLocations.push_back(Position(2050.0f, 1372.0f, 1194.0f, 0.0f)); // Fel Reaver
    eots.baseLocations.push_back(Position(2047.0f, 1749.0f, 1190.0f, 0.0f)); // Blood Elf
    eots.baseLocations.push_back(Position(2283.0f, 1731.0f, 1189.0f, 0.0f)); // Draenei
    eots.baseLocations.push_back(Position(2301.0f, 1386.0f, 1197.0f, 0.0f)); // Mage

    // Flag spawn location (center)
    eots.flagLocation = Position(2174.0f, 1569.0f, 1159.0f, 0.0f);

    _eotsStrategies[BGType::EYE_OF_THE_STORM] = eots;
}

void BattlegroundAI::InitializeSiegeStrategy()
{
    SiegeStrategy siege;
    siege.operateSiegeWeapons = true;
    siege.defendGates = true;
    siege.attackGates = true;
    siege.prioritizeDemolishers = true;

    // Strand of the Ancients gate locations (simplified)
    siege.gateLocations.push_back(Position(1411.0f, 108.0f, 31.0f, 0.0f));  // Green gate
    siege.gateLocations.push_back(Position(1055.0f, -108.0f, 22.0f, 0.0f)); // Yellow gate
    siege.gateLocations.push_back(Position(1431.0f, -219.0f, 30.0f, 0.0f)); // Blue gate
    siege.gateLocations.push_back(Position(1227.0f, -235.0f, 34.0f, 0.0f)); // Red gate

    _siegeStrategies[BGType::STRAND_OF_THE_ANCIENTS] = siege;

    // Isle of Conquest uses similar strategy
    SiegeStrategy ioc = siege;
    ioc.gateLocations.clear();
    ioc.gateLocations.push_back(Position(435.0f, -855.0f, 49.0f, 0.0f));  // Alliance gate
    ioc.gateLocations.push_back(Position(498.0f, -1046.0f, 135.0f, 0.0f)); // Horde gate
    _siegeStrategies[BGType::ISLE_OF_CONQUEST] = ioc;
}

void BattlegroundAI::Update(::Player* player, uint32 diff)
{
    if (!player || !player->IsInWorld())
        return;

    // Check if player is in battleground
    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Throttle updates (500ms for BG responsiveness)
    if (_lastUpdateTimes.count(playerGuid))
    {
        uint32 timeSinceLastUpdate = currentTime - _lastUpdateTimes[playerGuid];
        if (timeSinceLastUpdate < BG_UPDATE_INTERVAL)
            return;
    }

    _lastUpdateTimes[playerGuid] = currentTime;

    ::std::lock_guard lock(_mutex);

    // Auto-assign role if not assigned
    if (!_playerRoles.count(playerGuid))
        AssignRole(player, GetBattlegroundType(player));

    // Execute BG-specific strategy
    BGType bgType = GetBattlegroundType(player);
    switch (bgType)
    {
        case BGType::WARSONG_GULCH:
        case BGType::TWIN_PEAKS:
            ExecuteWSGStrategy(player);
            break;

        case BGType::ARATHI_BASIN:
        case BGType::BATTLE_FOR_GILNEAS:
            ExecuteABStrategy(player);
            break;

        case BGType::ALTERAC_VALLEY:
            ExecuteAVStrategy(player);
            break;

        case BGType::EYE_OF_THE_STORM:
            ExecuteEOTSStrategy(player);
            break;

        case BGType::STRAND_OF_THE_ANCIENTS:
        case BGType::ISLE_OF_CONQUEST:
            ExecuteSiegeStrategy(player);
            break;

        case BGType::TEMPLE_OF_KOTMOGU:
            ExecuteKotmoguStrategy(player);
            break;

        case BGType::SILVERSHARD_MINES:
            ExecuteSilvershardStrategy(player);
            break;

        case BGType::DEEPWIND_GORGE:
            ExecuteDeepwindStrategy(player);
            break;

        default:
            break;
    }

    // Adaptive strategy based on score
    AdjustStrategyBasedOnScore(player);
}

// ============================================================================
// ROLE MANAGEMENT
// ============================================================================

void BattlegroundAI::AssignRole(::Player* player, BGType bgType)
{
    if (!player)
        return;

    ::std::lock_guard lock(_mutex);

    uint32 playerGuid = player->GetGUID().GetCounter();
    BGRole role = BGRole::ATTACKER; // Default

    // Assign role based on class and BG type
    uint8 playerClass = player->GetClass();
    switch (bgType)
    {
        case BGType::WARSONG_GULCH:
        case BGType::TWIN_PEAKS:
            // Assign flag carrier to mobile classes
    if (playerClass == CLASS_DRUID || playerClass == CLASS_MONK ||
                playerClass == CLASS_DEMON_HUNTER)
                role = BGRole::FLAG_CARRIER;
            else if (playerClass == CLASS_PRIEST || playerClass == CLASS_PALADIN ||
                     playerClass == CLASS_SHAMAN)
                role = BGRole::HEALER_SUPPORT;
            else
                role = BGRole::FLAG_DEFENDER;
            break;

        case BGType::ARATHI_BASIN:
        case BGType::BATTLE_FOR_GILNEAS:
            // Healers and tanks defend, DPS capture
    if (playerClass == CLASS_PRIEST || playerClass == CLASS_PALADIN)
                role = BGRole::BASE_DEFENDER;
            else
                role = BGRole::BASE_CAPTURER;
            break;

        case BGType::ALTERAC_VALLEY:
            // Assign varied roles
    if (playerClass == CLASS_WARRIOR || playerClass == CLASS_DEATH_KNIGHT)
                role = BGRole::SIEGE_OPERATOR;
            else
                role = BGRole::ATTACKER;
            break;

        default:
            role = BGRole::ATTACKER;
            break;
    }

    _playerRoles[playerGuid] = role;

    TC_LOG_INFO("playerbot", "BattlegroundAI: Assigned role {} to player {}",
        static_cast<uint32>(role), playerGuid);
}

BGRole BattlegroundAI::GetPlayerRole(::Player* player) const
{
    if (!player)
        return BGRole::ATTACKER;

    ::std::lock_guard lock(_mutex);

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (_playerRoles.count(playerGuid))
        return _playerRoles.at(playerGuid);

    return BGRole::ATTACKER;
}

bool BattlegroundAI::SwitchRole(::Player* player, BGRole newRole)
{
    if (!player)
        return false;

    ::std::lock_guard lock(_mutex);

    BGStrategyProfile profile = GetStrategyProfile(player->GetGUID().GetCounter());
    if (!profile.allowRoleSwitch)
        return false;

    if (!IsRoleAppropriate(player, newRole))
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    _playerRoles[playerGuid] = newRole;

    TC_LOG_INFO("playerbot", "BattlegroundAI: Switched player {} to role {}",
        playerGuid, static_cast<uint32>(newRole));

    return true;
}

bool BattlegroundAI::IsRoleAppropriate(::Player* player, BGRole role) const
{
    if (!player)
        return false;

    uint8 playerClass = player->GetClass();
    switch (role)
    {
        case BGRole::FLAG_CARRIER:
            // Mobile classes are best flag carriers
            return playerClass == CLASS_DRUID || playerClass == CLASS_MONK ||
                   playerClass == CLASS_DEMON_HUNTER || playerClass == CLASS_ROGUE;

        case BGRole::HEALER_SUPPORT:
            // Only healers should have healer support role
            return playerClass == CLASS_PRIEST || playerClass == CLASS_PALADIN ||
                   playerClass == CLASS_SHAMAN || playerClass == CLASS_DRUID ||
                   playerClass == CLASS_MONK || playerClass == CLASS_EVOKER;

        case BGRole::SIEGE_OPERATOR:
            // Melee classes are good for siege weapons
            return playerClass == CLASS_WARRIOR || playerClass == CLASS_DEATH_KNIGHT ||
                   playerClass == CLASS_PALADIN;
        default:
            return true; // All other roles are flexible
    }
}

// ============================================================================
// OBJECTIVE MANAGEMENT
// ============================================================================

::std::vector<BGObjective> BattlegroundAI::GetActiveObjectives(::Player* player) const
{
    if (!player)
        return {};

    ::std::lock_guard lock(_mutex);

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return {};

    uint32 bgInstanceId = bg->GetInstanceID();
    if (_activeObjectives.count(bgInstanceId))
        return _activeObjectives.at(bgInstanceId);

    return {};
}

BGObjective BattlegroundAI::GetPlayerObjective(::Player* player) const
{
    if (!player)
        return BGObjective();

    ::std::lock_guard lock(_mutex);

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (_playerObjectives.count(playerGuid))
        return _playerObjectives.at(playerGuid);

    // No objective assigned - get highest priority objective
    ::std::vector<BGObjective> objectives = GetActiveObjectives(player);
    if (objectives.empty())
        return BGObjective();

    // Sort by priority
    ::std::sort(objectives.begin(), objectives.end(),
        [](BGObjective const& a, BGObjective const& b) {
            return static_cast<uint32>(a.priority) < static_cast<uint32>(b.priority);
        });

    return objectives[0];
}

bool BattlegroundAI::AssignObjective(::Player* player, BGObjective const& objective)
{
    if (!player)
        return false;

    ::std::lock_guard lock(_mutex);

    uint32 playerGuid = player->GetGUID().GetCounter();
    _playerObjectives[playerGuid] = objective;

    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Assigned objective type {} to player {}",
        static_cast<uint32>(objective.type), playerGuid);

    return true;
}

bool BattlegroundAI::CompleteObjective(::Player* player, BGObjective const& objective)
{
    if (!player)
        return false;

    ::std::lock_guard lock(_mutex);

    uint32 playerGuid = player->GetGUID().GetCounter();
    // Update metrics based on objective type
    switch (objective.type)
    {
        case BGObjectiveType::CAPTURE_FLAG:
            _playerMetrics[playerGuid].flagCaptures++;
            _globalMetrics.flagCaptures++;
            break;

        case BGObjectiveType::DEFEND_FLAG:
            _playerMetrics[playerGuid].flagReturns++;
            _globalMetrics.flagReturns++;
            break;

        case BGObjectiveType::CAPTURE_BASE:
            _playerMetrics[playerGuid].basesAssaulted++;
            _globalMetrics.basesAssaulted++;
            break;

        case BGObjectiveType::DEFEND_BASE:
            _playerMetrics[playerGuid].basesDefended++;
            _globalMetrics.basesDefended++;
            break;

        default:
            _playerMetrics[playerGuid].objectivesCaptured++;
            _globalMetrics.objectivesCaptured++;
            break;
    }

    // Clear objective
    _playerObjectives.erase(playerGuid);

    TC_LOG_INFO("playerbot", "BattlegroundAI: Player {} completed objective type {}",
        playerGuid, static_cast<uint32>(objective.type));
    return true;
}

bool BattlegroundAI::IsObjectiveContested(BGObjective const& objective) const
{
    // Check if enemy players are near objective
    uint32 enemyCount = CountPlayersAtObjective(objective.location, OBJECTIVE_RANGE);
    return enemyCount > 0;
}

// ============================================================================
// WARSONG GULCH / TWIN PEAKS STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteWSGStrategy(::Player* player)
{
    if (!player)
        return;

    BGRole role = GetPlayerRole(player);
    BGType bgType = GetBattlegroundType(player);
    FlagBGStrategy strategy = _flagStrategies[bgType];

    switch (role)
    {
        case BGRole::FLAG_CARRIER:
            // Try to pick up enemy flag
    if (!player->HasAura(23333)) // Not carrying flag
                PickupFlag(player);
            break;

        case BGRole::FLAG_DEFENDER:
            // Defend flag room or return flag if dropped
            DefendFlagRoom(player);
            ReturnFlag(player);
            break;

        case BGRole::HEALER_SUPPORT:
        case BGRole::ATTACKER:
            // Escort friendly FC or kill enemy FC
            {
                ::Player* friendlyFC = FindFriendlyFlagCarrier(player);
                ::Player* enemyFC = FindEnemyFlagCarrier(player);
                if (friendlyFC && strategy.escortFlagCarrier)
                    EscortFlagCarrier(player, friendlyFC);
                else if (enemyFC && strategy.killEnemyFC)
                {
                    // Attack enemy FC
                    player->SetSelection(enemyFC->GetGUID());
                }
            }
            break;

        default:
            break;
    }
}

bool BattlegroundAI::PickupFlag(::Player* player)
{
    if (!player)
        return false;

    BGType bgType = GetBattlegroundType(player);
    FlagBGStrategy strategy = _flagStrategies[bgType];

    // Move to enemy flag location
    float distance = ::std::sqrt(player->GetExactDistSq(strategy.enemyFlagSpawn)); // Calculate once from squared distance
    if (distance > OBJECTIVE_RANGE)
    {
        // Full implementation: Use PathGenerator to move to flag
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} moving to enemy flag",
            player->GetGUID().GetCounter());
        return false;
    }

    // Interact with flag
    TC_LOG_INFO("playerbot", "BattlegroundAI: Player {} picking up flag",
        player->GetGUID().GetCounter());

    // Full implementation: Interact with flag GameObject
    return true;
}

bool BattlegroundAI::ReturnFlag(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Find dropped friendly flag and interact with it
    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} returning flag",
        player->GetGUID().GetCounter());

    return true;
}

::Player* BattlegroundAI::FindFriendlyFlagCarrier(::Player* player) const
{
    if (!player)
        return nullptr;

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return nullptr;

    // Full implementation: Query BG for flag carrier on player's team
    // Check for flag aura (23333 for Alliance, 23335 for Horde)

    return nullptr;
}

::Player* BattlegroundAI::FindEnemyFlagCarrier(::Player* player) const
{
    if (!player)
        return nullptr;

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return nullptr;

    // Full implementation: Query BG for flag carrier on enemy team
    return nullptr;
}

bool BattlegroundAI::EscortFlagCarrier(::Player* player, ::Player* fc)
{
    if (!player || !fc)
        return false;

    // Follow flag carrier
    float distance = ::std::sqrt(player->GetExactDistSq(fc)); // Calculate once from squared distance
    if (distance > 15.0f)
    {
        // Full implementation: Move closer to FC
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} escorting FC {}",
            player->GetGUID().GetCounter(), fc->GetGUID().GetCounter());
    }

    // Defend FC from attackers
    // Full implementation: Find enemies attacking FC and engage

    return true;
}

bool BattlegroundAI::DefendFlagRoom(::Player* player)
{
    if (!player)
        return false;

    BGType bgType = GetBattlegroundType(player);
    FlagBGStrategy strategy = _flagStrategies[bgType];

    // Move to friendly flag room
    float distance = ::std::sqrt(player->GetExactDistSq(strategy.friendlyFlagSpawn)); // Calculate once from squared distance
    if (distance > strategy.friendlyFlagSpawn.GetExactDist(player))
    {
        // Full implementation: Move to flag room
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} moving to flag room",
            player->GetGUID().GetCounter());
    }

    // Engage enemies in flag room
    // Full implementation: Find and attack enemies near flag

    return true;
}

// ============================================================================
// ARATHI BASIN / BATTLE FOR GILNEAS STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteABStrategy(::Player* player)
{
    if (!player)
        return;

    BGRole role = GetPlayerRole(player);
    BGType bgType = GetBattlegroundType(player);
    BaseBGStrategy strategy = _baseStrategies[bgType];

    switch (role)
    {
        case BGRole::BASE_CAPTURER:
            {
                // Find best base to capture
                Position bestBase = FindBestBaseToCapture(player);
                if (bestBase.GetPositionX() != 0.0f)
                    CaptureBase(player, bestBase);
            }
            break;

        case BGRole::BASE_DEFENDER:
            {
                // Defend captured bases
                ::std::vector<Position> capturedBases = GetCapturedBases(player);
                if (!capturedBases.empty())
                {
                    // Find base under attack
    for (Position const& base : capturedBases)
                    {
                        if (IsBaseUnderAttack(base))
                        {
                            DefendBase(player, base);
                            break;
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
}

bool BattlegroundAI::CaptureBase(::Player* player, Position const& baseLocation)
{
    if (!player)
        return false;

    // Move to base
    float distance = ::std::sqrt(player->GetExactDistSq(baseLocation)); // Calculate once from squared distance
    if (distance > OBJECTIVE_RANGE)
    {
        // Full implementation: Move to base
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} moving to capture base",
            player->GetGUID().GetCounter());
        return false;
    }

    // Interact with base flag
    TC_LOG_INFO("playerbot", "BattlegroundAI: Player {} capturing base",
        player->GetGUID().GetCounter());

    // Full implementation: Interact with base GameObject
    return true;
}

bool BattlegroundAI::DefendBase(::Player* player, Position const& baseLocation)
{
    if (!player)
        return false;

    // Move to base
    float distance = ::std::sqrt(player->GetExactDistSq(baseLocation)); // Calculate once from squared distance
    if (distance > 30.0f)
    {
        // Full implementation: Move to base
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} moving to defend base",
            player->GetGUID().GetCounter());
    }

    // Engage enemies near base
    // Full implementation: Find and attack enemies

    return true;
}

Position BattlegroundAI::FindBestBaseToCapture(::Player* player) const
{
    if (!player)
        return Position();

    BGType bgType = GetBattlegroundType(player);
    if (!_baseStrategies.count(bgType))
        return Position();

    BaseBGStrategy strategy = _baseStrategies.at(bgType);

    // Check priority bases first
    for (uint32 priorityIndex : strategy.priorityBases)
    {
        if (priorityIndex < strategy.baseLocations.size())
        {
            Position base = strategy.baseLocations[priorityIndex];
            // Full implementation: Check if base is neutral or enemy-controlled
            return base;
        }
    }

    // Find closest neutral/enemy base
    Position closestBase;
    float closestDistanceSq = 9999.0f * 9999.0f; // Squared distance
    for (Position const& base : strategy.baseLocations)
    {
        float distanceSq = player->GetExactDistSq(base);
        if (distanceSq < closestDistanceSq)
        {
            closestDistanceSq = distanceSq;
            closestBase = base;
        }
    }

    return closestBase;
}

::std::vector<Position> BattlegroundAI::GetCapturedBases(::Player* player) const
{
    ::std::vector<Position> capturedBases;

    if (!player)
        return capturedBases;

    // Full implementation: Query BG for bases controlled by player's team
    return capturedBases;
}

bool BattlegroundAI::IsBaseUnderAttack(Position const& baseLocation) const
{
    // Check if enemy players are near base
    uint32 enemyCount = CountPlayersAtObjective(baseLocation, 30.0f);
    return enemyCount > 0;
}

// ============================================================================
// ALTERAC VALLEY STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteAVStrategy(::Player* player)
{
    if (!player)
        return;

    BGRole role = GetPlayerRole(player);

    switch (role)
    {
        case BGRole::SIEGE_OPERATOR:
            OperateSiegeWeapon(player);
            break;

        case BGRole::ATTACKER:
            CaptureGraveyard(player);
            CaptureTower(player);
            break;

        case BGRole::DEFENDER:
            // Defend key objectives
            break;

        default:
            break;
    }
}

bool BattlegroundAI::CaptureGraveyard(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} capturing graveyard",
        player->GetGUID().GetCounter());

    // Full implementation: Find nearest neutral/enemy graveyard and capture
    return true;
}

bool BattlegroundAI::CaptureTower(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} capturing tower",
        player->GetGUID().GetCounter());

    // Full implementation: Find nearest neutral/enemy tower and capture
    return true;
}

bool BattlegroundAI::KillBoss(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_INFO("playerbot", "BattlegroundAI: Player {} attacking boss",
        player->GetGUID().GetCounter());

    // Full implementation: Move to boss and attack
    return true;
}

bool BattlegroundAI::EscortNPC(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Find and escort Wing Commander NPCs
    return true;
}

// ============================================================================
// EYE OF THE STORM STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteEOTSStrategy(::Player* player)
{
    if (!player)
        return;

    BGRole role = GetPlayerRole(player);
    BGType bgType = BGType::EYE_OF_THE_STORM;
    EOTSStrategy strategy = _eotsStrategies[bgType];

    // Check if team is winning
    bool isWinning = IsTeamWinning(player);

    if (isWinning && strategy.prioritizeFlagWhenLeading)
    {
        // Focus on flag when winning
        CaptureFlagEOTS(player);
    }
    else
    {
        // Balance between bases and flag
    switch (role)
        {
            case BGRole::BASE_CAPTURER:
                CaptureBaseEOTS(player);
                break;

            case BGRole::FLAG_CARRIER:
                CaptureFlagEOTS(player);
                break;

            default:
                // Assist with both objectives
                float distanceSq = player->GetExactDistSq(strategy.flagLocation);
                if (distanceSq < (30.0f * 30.0f)) // 900.0f
                    CaptureFlagEOTS(player);
                else
                    CaptureBaseEOTS(player);
                break;
        }
    }
}

bool BattlegroundAI::CaptureFlagEOTS(::Player* player)
{
    if (!player)
        return false;

    BGType bgType = BGType::EYE_OF_THE_STORM;
    EOTSStrategy strategy = _eotsStrategies[bgType];

    // Move to flag location
    float distance = ::std::sqrt(player->GetExactDistSq(strategy.flagLocation)); // Calculate once from squared distance
    if (distance > OBJECTIVE_RANGE)
    {
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} moving to EOTS flag",
            player->GetGUID().GetCounter());
        // Full implementation: Move to flag
        return false;
    }

    // Pick up flag and return to base
    TC_LOG_INFO("playerbot", "BattlegroundAI: Player {} capturing EOTS flag",
        player->GetGUID().GetCounter());

    return true;
}

bool BattlegroundAI::CaptureBaseEOTS(::Player* player)
{
    if (!player)
        return false;

    // Similar to AB strategy - find and capture bases
    Position bestBase = FindBestBaseToCapture(player);
    if (bestBase.GetPositionX() != 0.0f)
        return CaptureBase(player, bestBase);

    return false;
}

// ============================================================================
// SIEGE STRATEGY (SOTA / IOC)
// ============================================================================

void BattlegroundAI::ExecuteSiegeStrategy(::Player* player)
{
    if (!player)
        return;

    BGRole role = GetPlayerRole(player);

    switch (role)
    {
        case BGRole::SIEGE_OPERATOR:
            OperateSiegeWeapon(player);
            break;

        case BGRole::ATTACKER:
            AttackGate(player);
            break;

        case BGRole::DEFENDER:
            DefendGate(player);
            break;

        default:
            break;
    }
}

bool BattlegroundAI::OperateSiegeWeapon(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} operating siege weapon",
        player->GetGUID().GetCounter());

    // Full implementation: Find demolisher/catapult and use it
    return true;
}

bool BattlegroundAI::AttackGate(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} attacking gate",
        player->GetGUID().GetCounter());

    // Full implementation: Attack nearest enemy gate
    return true;
}

bool BattlegroundAI::DefendGate(::Player* player)
{
    if (!player)
        return false;

    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} defending gate",
        player->GetGUID().GetCounter());

    // Full implementation: Defend friendly gates from attackers
    return true;
}

// ============================================================================
// TEMPLE OF KOTMOGU STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteKotmoguStrategy(::Player* player)
{
    if (!player)
        return;

    // Pick up orb or defend orb carrier
    PickupOrb(player);
    DefendOrbCarrier(player);
}

bool BattlegroundAI::PickupOrb(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Find and pick up orb
    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} picking up orb",
        player->GetGUID().GetCounter());

    return true;
}

bool BattlegroundAI::DefendOrbCarrier(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Find friendly orb carrier and defend
    return true;
}

// ============================================================================
// SILVERSHARD MINES STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteSilvershardStrategy(::Player* player)
{
    if (!player)
        return;

    // Capture or defend carts
    CaptureCart(player);
    DefendCart(player);
}

bool BattlegroundAI::CaptureCart(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Find and capture mine cart
    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} capturing cart",
        player->GetGUID().GetCounter());

    return true;
}

bool BattlegroundAI::DefendCart(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Defend friendly cart
    return true;
}

// ============================================================================
// DEEPWIND GORGE STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteDeepwindStrategy(::Player* player)
{
    if (!player)
        return;

    // Capture or defend mines
    CaptureMine(player);
    DefendMine(player);
}

bool BattlegroundAI::CaptureMine(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Capture neutral/enemy mine
    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} capturing mine",
        player->GetGUID().GetCounter());

    return true;
}

bool BattlegroundAI::DefendMine(::Player* player)
{
    if (!player)
        return false;

    // Full implementation: Defend friendly mine
    return true;
}

// ============================================================================
// TEAM COORDINATION
// ============================================================================

bool BattlegroundAI::GroupUpForObjective(::Player* player, BGObjective const& objective)
{
    if (!player)
        return false;

    BGStrategyProfile profile = GetStrategyProfile(player->GetGUID().GetCounter());
    if (!profile.groupUpForObjectives)
        return false;

    // Check if enough players at objective
    uint32 playersAtObjective = CountPlayersAtObjective(objective.location, 20.0f);
    if (playersAtObjective < profile.minPlayersForAttack)
    {
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} waiting for group at objective",
            player->GetGUID().GetCounter());
        return false;
    }

    return true;
}

::std::vector<::Player*> BattlegroundAI::GetNearbyTeammates(::Player* player, float range) const
{
    ::std::vector<::Player*> teammates;

    if (!player)
        return teammates;

    // Full implementation: Find friendly players in range
    return teammates;
}

bool BattlegroundAI::CallForBackup(::Player* player, Position const& location)
{
    if (!player)
        return false;

    ::std::lock_guard lock(_mutex);

    uint32 playerGuid = player->GetGUID().GetCounter();
    _backupCalls[playerGuid] = {location, GameTime::GetGameTimeMS()};

    TC_LOG_INFO("playerbot", "BattlegroundAI: Player {} calling for backup",
        playerGuid);

    return true;
}

bool BattlegroundAI::RespondToBackupCall(::Player* player, Position const& location)
{
    if (!player)
        return false;

    // Move to backup location
    float distance = ::std::sqrt(player->GetExactDistSq(location)); // Calculate once from squared distance
    if (distance > 5.0f)
    {
        TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} responding to backup call",
            player->GetGUID().GetCounter());
        // Full implementation: Move to location
    }

    return true;
}

// ============================================================================
// POSITIONING
// ============================================================================

bool BattlegroundAI::MoveToObjective(::Player* player, BGObjective const& objective)
{
    if (!player)
        return false;

    float distance = ::std::sqrt(player->GetExactDistSq(objective.location)); // Calculate once from squared distance
    if (distance <= OBJECTIVE_RANGE)
        return true;

    // Full implementation: Use PathGenerator to move to objective
    TC_LOG_DEBUG("playerbot", "BattlegroundAI: Player {} moving to objective",
        player->GetGUID().GetCounter());

    return false;
}

bool BattlegroundAI::TakeDefensivePosition(::Player* player, Position const& location)
{
    if (!player)
        return false;

    // Full implementation: Move to defensive position near location
    return true;
}

bool BattlegroundAI::IsAtObjective(::Player* player, BGObjective const& objective) const
{
    if (!player)
        return false;

    return IsObjectiveInRange(player, objective.location, OBJECTIVE_RANGE);
}

// ============================================================================
// ADAPTIVE STRATEGY
// ============================================================================

void BattlegroundAI::AdjustStrategyBasedOnScore(::Player* player)
{
    if (!player)
        return;

    if (IsTeamWinning(player))
        SwitchToDefensiveStrategy(player);
    else
        SwitchToAggressiveStrategy(player);
}

bool BattlegroundAI::IsTeamWinning(::Player* player) const
{
    if (!player)
        return false;

    uint32 teamScore = GetTeamScore(player);
    uint32 enemyScore = GetEnemyTeamScore(player);

    return teamScore > enemyScore;
}

void BattlegroundAI::SwitchToDefensiveStrategy(::Player* player)
{
    if (!player)
        return;

    // Switch to defensive roles
    BGRole currentRole = GetPlayerRole(player);
    if (currentRole == BGRole::ATTACKER || currentRole == BGRole::BASE_CAPTURER)
    {
        SwitchRole(player, BGRole::DEFENDER);
    }
}

void BattlegroundAI::SwitchToAggressiveStrategy(::Player* player)
{
    if (!player)
        return;

    // Switch to offensive roles
    BGRole currentRole = GetPlayerRole(player);
    if (currentRole == BGRole::DEFENDER || currentRole == BGRole::BASE_DEFENDER)
    {
        SwitchRole(player, BGRole::ATTACKER);
    }
}

// ============================================================================
// PROFILES
// ============================================================================

void BattlegroundAI::SetStrategyProfile(uint32 playerGuid, BGStrategyProfile const& profile)
{
    ::std::lock_guard lock(_mutex);
    _playerProfiles[playerGuid] = profile;
}

BGStrategyProfile BattlegroundAI::GetStrategyProfile(uint32 playerGuid) const
{
    ::std::lock_guard lock(_mutex);

    if (_playerProfiles.count(playerGuid))
        return _playerProfiles.at(playerGuid);

    return BGStrategyProfile(); // Default
}

// ============================================================================
// METRICS
// ============================================================================

BattlegroundAI::BGMetrics const& BattlegroundAI::GetPlayerMetrics(uint32 playerGuid) const
{
    ::std::lock_guard lock(_mutex);

    if (!_playerMetrics.count(playerGuid))
    {
        static BGMetrics emptyMetrics;
        return emptyMetrics;
    }

    return _playerMetrics.at(playerGuid);
}

BattlegroundAI::BGMetrics const& BattlegroundAI::GetGlobalMetrics() const
{
    return _globalMetrics;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

BGType BattlegroundAI::GetBattlegroundType(::Player* player) const
{
    if (!player)
        return BGType::WARSONG_GULCH;

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return BGType::WARSONG_GULCH;

    // Map BG type ID to BGType enum
    // Full implementation: Query bg->GetTypeID()

    return BGType::WARSONG_GULCH; // Placeholder
}

::Battleground* BattlegroundAI::GetPlayerBattleground(::Player* player) const
{
    if (!player)
        return nullptr;

    return player->GetBattleground();
}

uint32 BattlegroundAI::GetTeamScore(::Player* player) const
{
    if (!player)
        return 0;

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return 0;

    // Full implementation: Query bg->GetTeamScore(player->GetBGTeam())
    return 0;
}

uint32 BattlegroundAI::GetEnemyTeamScore(::Player* player) const
{
    if (!player)
        return 0;

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return 0;

    // Full implementation: Query bg->GetTeamScore(enemy team)
    return 0;
}

bool BattlegroundAI::IsObjectiveInRange(::Player* player, Position const& objLocation,
    float range) const
{
    if (!player)
        return false;

    float rangeSq = range * range;
    return player->GetExactDistSq(objLocation) <= rangeSq;
}

uint32 BattlegroundAI::CountPlayersAtObjective(Position const& objLocation, float range) const
{
    // Full implementation: Count players in range
    return 0;
}

::std::vector<::Player*> BattlegroundAI::GetPlayersAtObjective(Position const& objLocation,
    float range) const
{
    // Full implementation: Find all players in range
    return {};
}

} // namespace Playerbot
