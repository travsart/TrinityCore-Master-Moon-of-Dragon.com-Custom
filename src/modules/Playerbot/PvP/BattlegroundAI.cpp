/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BattlegroundAI.h"
#include "GameTime.h"
#include "Player.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "GameObject.h"
#include "World.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "../AI/Coordination/Battleground/BattlegroundCoordinatorManager.h"
#include "../AI/Coordination/Battleground/BattlegroundCoordinator.h"
#include "../AI/Coordination/Battleground/BGSpatialQueryCache.h"
#include "../AI/Coordination/Battleground/Scripts/IBGScript.h"
#include "../AI/Coordination/Battleground/Scripts/Domination/TempleOfKotmoguScript.h"
#include "../AI/Coordination/Battleground/Scripts/Domination/TempleOfKotmoguData.h"
#include "../Movement/BotMovementUtil.h"
#include <algorithm>
#include <cmath>
#include <limits>

// WSG/TP Flag aura IDs
constexpr uint32 ALLIANCE_FLAG_AURA = 23333;  // Carrying Horde flag
constexpr uint32 HORDE_FLAG_AURA = 23335;     // Carrying Alliance flag

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
    if (!bg)
        return;

    // Handle both prep phase (WAIT_JOIN) and active phase (IN_PROGRESS)
    BattlegroundStatus status = bg->GetStatus();
    if (status != STATUS_IN_PROGRESS && status != STATUS_WAIT_JOIN)
        return;

    // During prep phase, just wait at spawn - don't execute strategy yet
    if (status == STATUS_WAIT_JOIN)
    {
        // Log once that we're in prep mode
        static std::unordered_map<uint32, bool> prepLogged;
        uint32 guid = player->GetGUID().GetCounter();
        if (!prepLogged[guid])
        {
            TC_LOG_INFO("playerbots.bg",
                "BattlegroundAI: Bot {} in {} prep phase - waiting for gates to open",
                player->GetName(), bg->GetName());
            prepLogged[guid] = true;
        }
        // Don't move or execute strategy during prep - just wait
        return;
    }

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

    // =========================================================================
    // BATTLEGROUND COORDINATOR INTEGRATION
    // =========================================================================
    // Try to get coordinator for strategic decisions. If available, use its
    // role assignment. Otherwise, fallback to local role assignment.

    BattlegroundCoordinator* coordinator = sBGCoordinatorMgr->GetCoordinatorForPlayer(player);
    Playerbot::BGRole assignedRole = BGRole::UNASSIGNED;
    BGObjective* assignment = nullptr;

    if (coordinator)
    {
        // Get the bot's assigned role from coordinator
        assignedRole = coordinator->GetBotRole(player->GetGUID());

        // If bot has no role, it's a late-joiner - register it with the coordinator
        if (assignedRole == BGRole::UNASSIGNED)
        {
            coordinator->AddBot(player);
            assignedRole = coordinator->GetBotRole(player->GetGUID());
        }

        assignment = coordinator->GetAssignment(player->GetGUID());

        TC_LOG_DEBUG("playerbots.bg",
            "BattlegroundAI: Bot {} using coordinator (role: {}, has assignment: {})",
            player->GetName(),
            static_cast<uint32>(assignedRole),
            assignment != nullptr);

        // Store the role for strategy execution
        _playerRoles[playerGuid] = assignedRole;
    }
    else
    {
        // No coordinator - try to create one through UpdateBot
        // This handles late-joining bots or when coordinator wasn't created yet
        sBGCoordinatorMgr->UpdateBot(player, 0);

        // Fall back to local role assignment
        ::std::lock_guard lock(_mutex);
        if (!_playerRoles.count(playerGuid))
            AssignRole(player, GetBattlegroundType(player));
        assignedRole = _playerRoles[playerGuid];
    }

    // =========================================================================
    // EXECUTE BG-SPECIFIC STRATEGY BASED ON ROLE
    // =========================================================================
    // Use the assigned role to drive behavior. The strategy functions
    // will use the role to determine specific actions.

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
        case ObjectiveType::FLAG:
            // For flags, count as captures (defense tracked separately)
            _playerMetrics[playerGuid].flagCaptures++;
            _globalMetrics.flagCaptures++;
            break;

        case ObjectiveType::NODE:
        case ObjectiveType::CONTROL_POINT:
        case ObjectiveType::CAPTURABLE:
            // For bases/nodes, count as assaults
            _playerMetrics[playerGuid].basesAssaulted++;
            _globalMetrics.basesAssaulted++;
            break;

        case ObjectiveType::TOWER:
        case ObjectiveType::GRAVEYARD:
            // Towers and graveyards count as defended
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
    uint32 enemyCount = CountPlayersAtObjective(Position(objective.x, objective.y, objective.z), OBJECTIVE_RANGE);
    return enemyCount > 0;
}

// ============================================================================
// WARSONG GULCH / TWIN PEAKS STRATEGY
// ============================================================================

void BattlegroundAI::ExecuteWSGStrategy(::Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    // Get coordinator and script for proper data-driven execution
    BattlegroundCoordinator* coordinator = sBGCoordinatorMgr->GetCoordinatorForPlayer(player);
    Coordination::Battleground::IBGScript* script = coordinator ? coordinator->GetScript() : nullptr;

    BGRole role = GetPlayerRole(player);
    uint32 teamId = player->GetBGTeam();

    // Check if we're carrying a flag
    bool carryingFlag = player->HasAura(ALLIANCE_FLAG_AURA) || player->HasAura(HORDE_FLAG_AURA);

    // Find flag carriers
    ::Player* friendlyFC = FindFriendlyFlagCarrier(player);
    ::Player* enemyFC = FindEnemyFlagCarrier(player);

    // Log current state
    TC_LOG_DEBUG("playerbots.bg", "WSG: {} role={} carryingFlag={} friendlyFC={} enemyFC={}",
        player->GetName(), static_cast<uint32>(role), carryingFlag,
        friendlyFC ? friendlyFC->GetName() : "none",
        enemyFC ? enemyFC->GetName() : "none");

    // =========================================================================
    // PRIORITY 1: If we're carrying flag, run it home!
    // =========================================================================
    if (carryingFlag)
    {
        ExecuteFlagCarrierBehavior(player, coordinator, script);
        return;
    }

    // =========================================================================
    // PRIORITY 2: Execute role-based behavior
    // =========================================================================
    switch (role)
    {
        case BGRole::FLAG_CARRIER:
        case BGRole::FLAG_HUNTER:
            // Go pick up enemy flag
            if (!enemyFC) // Enemy flag not taken, go get it
            {
                ExecuteFlagPickupBehavior(player, coordinator, script);
            }
            else // Enemy has our flag - hunt them!
            {
                ExecuteFlagHunterBehavior(player, enemyFC);
            }
            break;

        case BGRole::FLAG_ESCORT:
            if (friendlyFC)
            {
                ExecuteEscortBehavior(player, friendlyFC, coordinator, script);
            }
            else if (!enemyFC)
            {
                // No one has flags - help pick up enemy flag
                ExecuteFlagPickupBehavior(player, coordinator, script);
            }
            else
            {
                // Our FC not present, enemy has flag - hunt enemy FC
                ExecuteFlagHunterBehavior(player, enemyFC);
            }
            break;

        case BGRole::FLAG_DEFENDER:
        case BGRole::NODE_DEFENDER:
            ExecuteDefenderBehavior(player, coordinator, script);
            break;

        case BGRole::HEALER_SUPPORT:
            // Prioritize healing FC, then escort, then defend
            if (friendlyFC)
                ExecuteEscortBehavior(player, friendlyFC, coordinator, script);
            else
                ExecuteDefenderBehavior(player, coordinator, script);
            break;

        case BGRole::ATTACKER:
        case BGRole::NODE_ATTACKER:
            if (enemyFC)
                ExecuteFlagHunterBehavior(player, enemyFC);
            else if (!friendlyFC) // No one has enemy flag, go get it
                ExecuteFlagPickupBehavior(player, coordinator, script);
            else // Our FC has flag - escort
                ExecuteEscortBehavior(player, friendlyFC, coordinator, script);
            break;

        default:
            // UNASSIGNED or unknown role - be useful: hunt enemy FC or help capture
            if (enemyFC)
                ExecuteFlagHunterBehavior(player, enemyFC);
            else
                ExecuteFlagPickupBehavior(player, coordinator, script);
            break;
    }
}

bool BattlegroundAI::PickupFlag(::Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    BGType bgType = GetBattlegroundType(player);
    if (!_flagStrategies.count(bgType))
        return false;

    FlagBGStrategy& strategy = _flagStrategies[bgType];

    // Determine which flag we need to pick up based on faction
    Position targetFlag = (player->GetBGTeam() == ALLIANCE)
        ? strategy.enemyFlagSpawn   // Alliance picks up Horde flag
        : strategy.friendlyFlagSpawn; // Horde picks up Alliance flag (positions are from Alliance perspective)

    // Correct for faction - in WSG data, friendlyFlagSpawn is Alliance base, enemyFlagSpawn is Horde base
    // Alliance wants to go to Horde base (enemyFlagSpawn) to pick up their flag
    // Horde wants to go to Alliance base (friendlyFlagSpawn) to pick up their flag
    if (player->GetBGTeam() == HORDE)
        targetFlag = strategy.friendlyFlagSpawn;
    else
        targetFlag = strategy.enemyFlagSpawn;

    float distance = player->GetExactDist(&targetFlag);

    if (distance > OBJECTIVE_RANGE)
    {
        // Move to enemy flag location
        if (BotMovementUtil::MoveToPosition(player, targetFlag))
        {
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} moving to enemy flag (dist: {:.1f})",
                player->GetName(), distance);
        }
        return false;
    }

    // We're at the flag - try to interact with it
    TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} at enemy flag, attempting pickup",
        player->GetName());

    // Find the flag GameObject and interact with it
    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return false;

    // Search for nearby flag GameObjects
    std::list<GameObject*> flagList;
    player->GetGameObjectListWithEntryInGrid(flagList, 0, 10.0f); // Get all GOs nearby

    for (GameObject* go : flagList)
    {
        if (!go || !go->IsWithinDistInMap(player, OBJECTIVE_RANGE))
            continue;

        // Check if this is a capturable flag (has GAMEOBJECT_TYPE_FLAGSTAND or similar)
        GameObjectTemplate const* goInfo = go->GetGOInfo();
        if (goInfo && (goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND ||
                       goInfo->type == GAMEOBJECT_TYPE_GOOBER))
        {
            // Try to use the flag
            go->Use(player);
            TC_LOG_INFO("playerbots.bg", "BattlegroundAI: {} picked up flag!",
                player->GetName());
            return true;
        }
    }

    return false;
}

bool BattlegroundAI::ReturnFlag(::Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return false;

    // Search for dropped flag GameObject near our flag room
    BGType bgType = GetBattlegroundType(player);
    if (!_flagStrategies.count(bgType))
        return false;

    FlagBGStrategy& strategy = _flagStrategies[bgType];

    // Our flag room (where dropped friendly flags appear)
    Position flagRoom = (player->GetBGTeam() == ALLIANCE)
        ? strategy.friendlyFlagSpawn
        : strategy.enemyFlagSpawn;

    // Search for flag GameObjects in a large radius (flags can be dropped anywhere)
    std::list<GameObject*> goList;
    player->GetGameObjectListWithEntryInGrid(goList, 0, 50.0f);

    GameObject* droppedFlag = nullptr;
    float closestDist = 100.0f;

    for (GameObject* go : goList)
    {
        if (!go)
            continue;

        GameObjectTemplate const* goInfo = go->GetGOInfo();
        if (!goInfo)
            continue;

        // Look for dropped flag types
        if (goInfo->type == GAMEOBJECT_TYPE_FLAGDROP)
        {
            float dist = player->GetExactDist(go);
            if (dist < closestDist)
            {
                closestDist = dist;
                droppedFlag = go;
            }
        }
    }

    if (droppedFlag)
    {
        float dist = player->GetExactDist(droppedFlag);
        if (dist > OBJECTIVE_RANGE)
        {
            // Move to the dropped flag
            Position flagPos;
            flagPos.Relocate(droppedFlag->GetPositionX(), droppedFlag->GetPositionY(), droppedFlag->GetPositionZ());
            if (BotMovementUtil::MoveToPosition(player, flagPos))
            {
                TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} moving to return dropped flag (dist: {:.1f})",
                    player->GetName(), dist);
            }
            return true;
        }

        // We're at the flag - interact to return it
        droppedFlag->Use(player);
        TC_LOG_INFO("playerbots.bg", "BattlegroundAI: {} returned dropped flag!",
            player->GetName());
        return true;
    }

    return false;
}

::Player* BattlegroundAI::FindFriendlyFlagCarrier(::Player* player) const
{
    if (!player || !player->IsInWorld())
        return nullptr;

    // OPTIMIZATION: Use O(1) cached lookup from BattlegroundCoordinator
    // instead of O(n) iteration over all BG players (80x faster for 40v40)
    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

    if (coordinator)
    {
        ObjectGuid fcGuid = coordinator->GetCachedFriendlyFC();
        if (!fcGuid.IsEmpty())
        {
            ::Player* fc = ObjectAccessor::FindPlayer(fcGuid);
            if (fc && fc->IsInWorld() && fc->IsAlive())
                return fc;
        }
        // Cache returned empty - no friendly FC
        return nullptr;
    }

    // FALLBACK: Legacy O(n) implementation if no coordinator
    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return nullptr;

    uint32 teamId = player->GetBGTeam();
    // Friendly FC has the enemy's flag:
    // - Alliance FC carries Horde flag (ALLIANCE_FLAG_AURA = 23333)
    // - Horde FC carries Alliance flag (HORDE_FLAG_AURA = 23335)
    uint32 flagAura = (teamId == ALLIANCE) ? ALLIANCE_FLAG_AURA : HORDE_FLAG_AURA;

    // Iterate through all players in the BG on our team
    for (auto const& itr : bg->GetPlayers())
    {
        ::Player* bgPlayer = ObjectAccessor::FindPlayer(itr.first);
        if (!bgPlayer || !bgPlayer->IsInWorld() || !bgPlayer->IsAlive())
            continue;

        // Check if on same team and has flag
        if (bgPlayer->GetBGTeam() == teamId && bgPlayer->HasAura(flagAura))
        {
            return bgPlayer;
        }
    }

    return nullptr;
}

::Player* BattlegroundAI::FindEnemyFlagCarrier(::Player* player) const
{
    if (!player || !player->IsInWorld())
        return nullptr;

    // OPTIMIZATION: Use O(1) cached lookup from BattlegroundCoordinator
    // instead of O(n) iteration over all BG players (80x faster for 40v40)
    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

    if (coordinator)
    {
        ObjectGuid fcGuid = coordinator->GetCachedEnemyFC();
        if (!fcGuid.IsEmpty())
        {
            ::Player* fc = ObjectAccessor::FindPlayer(fcGuid);
            if (fc && fc->IsInWorld() && fc->IsAlive())
                return fc;
        }
        // Cache returned empty - no enemy FC
        return nullptr;
    }

    // FALLBACK: Legacy O(n) implementation if no coordinator
    ::Battleground* bg = GetPlayerBattleground(player);
    if (!bg)
        return nullptr;

    uint32 teamId = player->GetBGTeam();
    uint32 enemyTeam = (teamId == ALLIANCE) ? HORDE : ALLIANCE;
    // Enemy FC has our flag:
    // - Enemy Alliance FC carries Horde flag (ALLIANCE_FLAG_AURA = 23333)
    // - Enemy Horde FC carries Alliance flag (HORDE_FLAG_AURA = 23335)
    uint32 flagAura = (enemyTeam == ALLIANCE) ? ALLIANCE_FLAG_AURA : HORDE_FLAG_AURA;

    // Iterate through all players in the BG on enemy team
    for (auto const& itr : bg->GetPlayers())
    {
        ::Player* bgPlayer = ObjectAccessor::FindPlayer(itr.first);
        if (!bgPlayer || !bgPlayer->IsInWorld() || !bgPlayer->IsAlive())
            continue;

        // Check if on enemy team and has flag
        if (bgPlayer->GetBGTeam() == enemyTeam && bgPlayer->HasAura(flagAura))
        {
            return bgPlayer;
        }
    }

    return nullptr;
}

bool BattlegroundAI::EscortFlagCarrier(::Player* player, ::Player* fc)
{
    if (!player || !fc || !player->IsInWorld() || !fc->IsInWorld())
        return false;

    float distance = player->GetExactDist(fc);
    constexpr float ESCORT_DISTANCE = 8.0f;  // Stay close to FC
    constexpr float MAX_ESCORT_DISTANCE = 30.0f;  // Don't chase too far

    if (distance > MAX_ESCORT_DISTANCE)
    {
        // FC is too far, move to them
        if (BotMovementUtil::MoveToPosition(player, fc->GetPosition()))
        {
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} chasing FC {} (dist: {:.1f})",
                player->GetName(), fc->GetName(), distance);
        }
        return true;
    }
    else if (distance > ESCORT_DISTANCE)
    {
        // Move closer to FC
        Position escortPos;
        // Position ourselves between FC and where they're going (or just near them)
        float angle = fc->GetOrientation() + M_PI; // Behind the FC
        escortPos.Relocate(
            fc->GetPositionX() + ESCORT_DISTANCE * 0.5f * cos(angle),
            fc->GetPositionY() + ESCORT_DISTANCE * 0.5f * sin(angle),
            fc->GetPositionZ()
        );
        BotMovementUtil::CorrectPositionToGround(player, escortPos);

        if (BotMovementUtil::MoveToPosition(player, escortPos))
        {
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} moving to escort FC {} (dist: {:.1f})",
                player->GetName(), fc->GetName(), distance);
        }
    }

    // Check if FC is under attack and help them
    if (fc->IsInCombat())
    {
        // Find enemies attacking the FC
        Unit* attacker = nullptr;
        for (auto const& threatRef : fc->GetThreatManager().GetSortedThreatList())
        {
            Unit* target = threatRef->GetVictim();
            if (target && target->IsAlive() && target->IsHostileTo(player))
            {
                attacker = target;
                break;
            }
        }

        // If FC has no threat list entries, check who is targeting them
        if (!attacker)
        {
            // OPTIMIZATION: Use spatial cache instead of GetPlayerListInGrid
            // O(cells) instead of O(n) - 20x faster for 40v40
            BattlegroundCoordinator* coordinator =
                sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

            if (coordinator)
            {
                auto nearbyEnemies = coordinator->QueryNearbyEnemies(player->GetPosition(), 30.0f);
                for (auto const* snapshot : nearbyEnemies)
                {
                    if (snapshot && snapshot->isAlive && snapshot->targetGuid == fc->GetGUID())
                    {
                        ::Player* enemy = ObjectAccessor::FindPlayer(snapshot->guid);
                        if (enemy && enemy->IsAlive())
                        {
                            attacker = enemy;
                            break;
                        }
                    }
                }
            }
            else
            {
                // Fallback to legacy method
                std::list<Player*> nearbyPlayers;
                player->GetPlayerListInGrid(nearbyPlayers, 30.0f);
                for (::Player* nearby : nearbyPlayers)
                {
                    if (nearby && nearby->IsAlive() && nearby->IsHostileTo(player) &&
                        nearby->GetTarget() == fc->GetGUID())
                    {
                        attacker = nearby;
                        break;
                    }
                }
            }
        }

        if (attacker && attacker->IsAlive())
        {
            // Set target to attack the FC's attacker
            player->SetSelection(attacker->GetGUID());
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} targeting {} who is attacking FC",
                player->GetName(), attacker->GetName());
        }
    }

    return true;
}

bool BattlegroundAI::DefendFlagRoom(::Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    BGType bgType = GetBattlegroundType(player);
    if (!_flagStrategies.count(bgType))
        return false;

    FlagBGStrategy& strategy = _flagStrategies[bgType];

    // Our flag room depends on faction (positions are from Alliance perspective)
    Position flagRoom = (player->GetBGTeam() == ALLIANCE)
        ? strategy.friendlyFlagSpawn  // Alliance defends Alliance base
        : strategy.enemyFlagSpawn;    // Horde defends Horde base

    float distance = player->GetExactDist(&flagRoom);
    constexpr float DEFENSE_RADIUS = 25.0f;

    // Move to flag room if too far
    if (distance > DEFENSE_RADIUS)
    {
        if (BotMovementUtil::MoveToPosition(player, flagRoom))
        {
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} moving to defend flag room (dist: {:.1f})",
                player->GetName(), distance);
        }
        return true;
    }

    // We're in the flag room - look for enemies using spatial cache
    // OPTIMIZATION: O(cells) instead of O(n) - 20x faster for 40v40 BGs
    ::Player* closestEnemy = nullptr;
    float closestDist = DEFENSE_RADIUS + 1.0f;

    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

    if (coordinator)
    {
        // Use optimized spatial cache query - O(cells) complexity
        auto const* nearestSnapshot = coordinator->GetNearestEnemy(
            player->GetPosition(), DEFENSE_RADIUS, &closestDist);

        if (nearestSnapshot)
        {
            closestEnemy = ObjectAccessor::FindPlayer(nearestSnapshot->guid);
        }
    }
    else
    {
        // Fallback to legacy O(n) method if coordinator not available
        std::list<Player*> nearbyPlayers;
        player->GetPlayerListInGrid(nearbyPlayers, DEFENSE_RADIUS);

        for (::Player* nearby : nearbyPlayers)
        {
            if (!nearby || !nearby->IsAlive() || !nearby->IsHostileTo(player))
                continue;

            float dist = player->GetExactDist(nearby);
            if (dist < closestDist)
            {
                closestDist = dist;
                closestEnemy = nearby;
            }
        }
    }

    if (closestEnemy && closestEnemy->IsAlive())
    {
        // Target the closest enemy in our flag room
        player->SetSelection(closestEnemy->GetGUID());
        TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} targeting enemy {} in flag room (cache)",
            player->GetName(), closestEnemy->GetName());
        return true;
    }

    // No enemies - patrol near the flag
    if (!BotMovementUtil::IsMoving(player))
    {
        // Small random movement around flag room
        Position patrolPos;
        float angle = frand(0, 2 * M_PI);
        float dist = frand(3.0f, 10.0f);
        patrolPos.Relocate(
            flagRoom.GetPositionX() + dist * cos(angle),
            flagRoom.GetPositionY() + dist * sin(angle),
            flagRoom.GetPositionZ()
        );
        BotMovementUtil::CorrectPositionToGround(player, patrolPos);
        BotMovementUtil::MoveToPosition(player, patrolPos);
    }

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
    if (!player || !player->IsInWorld())
        return false;

    float distance = player->GetExactDist(&baseLocation);

    // Move to base if too far
    if (distance > OBJECTIVE_RANGE)
    {
        if (BotMovementUtil::MoveToPosition(player, baseLocation))
        {
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} moving to capture base (dist: {:.1f})",
                player->GetName(), distance);
        }
        return false;
    }

    // We're at the base - try to interact with the flag/banner
    TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} at base, attempting capture",
        player->GetName());

    // Find the base flag GameObject
    std::list<GameObject*> goList;
    player->GetGameObjectListWithEntryInGrid(goList, 0, OBJECTIVE_RANGE);

    for (GameObject* go : goList)
    {
        if (!go || !go->IsWithinDistInMap(player, OBJECTIVE_RANGE))
            continue;

        GameObjectTemplate const* goInfo = go->GetGOInfo();
        if (goInfo && (goInfo->type == GAMEOBJECT_TYPE_GOOBER ||
                       goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND ||
                       goInfo->type == GAMEOBJECT_TYPE_CAPTURE_POINT))
        {
            // Check if we can interact with it
            go->Use(player);
            TC_LOG_INFO("playerbots.bg", "BattlegroundAI: {} interacting with base capture point",
                player->GetName());
            return true;
        }
    }

    return false;
}

bool BattlegroundAI::DefendBase(::Player* player, Position const& baseLocation)
{
    if (!player || !player->IsInWorld())
        return false;

    constexpr float DEFENSE_RADIUS = 30.0f;
    float distance = player->GetExactDist(&baseLocation);

    // Move to base if too far
    if (distance > DEFENSE_RADIUS)
    {
        if (BotMovementUtil::MoveToPosition(player, baseLocation))
        {
            TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} moving to defend base (dist: {:.1f})",
                player->GetName(), distance);
        }
        return true;
    }

    // We're at the base - look for enemies using spatial cache
    // OPTIMIZATION: O(cells) instead of O(n) - 20x faster for 40v40 BGs
    ::Player* closestEnemy = nullptr;
    float closestDist = DEFENSE_RADIUS + 1.0f;

    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

    if (coordinator)
    {
        // Use optimized spatial cache query - O(cells) complexity
        auto const* nearestSnapshot = coordinator->GetNearestEnemy(
            player->GetPosition(), DEFENSE_RADIUS, &closestDist);

        if (nearestSnapshot)
        {
            closestEnemy = ObjectAccessor::FindPlayer(nearestSnapshot->guid);
        }
    }
    else
    {
        // Fallback to legacy O(n) method if coordinator not available
        std::list<Player*> nearbyPlayers;
        player->GetPlayerListInGrid(nearbyPlayers, DEFENSE_RADIUS);

        for (::Player* nearby : nearbyPlayers)
        {
            if (!nearby || !nearby->IsAlive() || !nearby->IsHostileTo(player))
                continue;

            float dist = player->GetExactDist(nearby);
            if (dist < closestDist)
            {
                closestDist = dist;
                closestEnemy = nearby;
            }
        }
    }

    if (closestEnemy && closestEnemy->IsAlive())
    {
        // Target the closest enemy near base
        player->SetSelection(closestEnemy->GetGUID());
        TC_LOG_DEBUG("playerbots.bg", "BattlegroundAI: {} targeting enemy {} near base (cache)",
            player->GetName(), closestEnemy->GetName());
        return true;
    }

    // No enemies - patrol near the base
    if (!BotMovementUtil::IsMoving(player))
    {
        Position patrolPos;
        float angle = frand(0, 2 * M_PI);
        float dist = frand(5.0f, 15.0f);
        patrolPos.Relocate(
            baseLocation.GetPositionX() + dist * cos(angle),
            baseLocation.GetPositionY() + dist * sin(angle),
            baseLocation.GetPositionZ()
        );
        BotMovementUtil::CorrectPositionToGround(player, patrolPos);
        BotMovementUtil::MoveToPosition(player, patrolPos);
    }

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
// Runtime behavior has been moved to TempleOfKotmoguScript (lighthouse pattern).
// This is a thin delegation wrapper.

void BattlegroundAI::ExecuteKotmoguStrategy(::Player* player)
{
    if (!player || !player->IsInWorld() || !player->IsAlive())
        return;

    BattlegroundCoordinator* coordinator = sBGCoordinatorMgr->GetCoordinatorForPlayer(player);
    if (coordinator)
    {
        auto* script = coordinator->GetScript();
        if (script && script->GetBGType() == BGType::TEMPLE_OF_KOTMOGU)
        {
            if (script->ExecuteStrategy(player))
                return;
        }
    }

    TC_LOG_DEBUG("playerbots.bg", "[TOK] {} no coordinator/script available, idle",
        player->GetName());
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
    uint32 playersAtObjective = CountPlayersAtObjective(Position(objective.x, objective.y, objective.z), 20.0f);
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
    // OPTIMIZATION: Use spatial cache for O(cells) complexity instead of O(n)
    ::std::vector<::Player*> teammates;

    if (!player || !player->IsInWorld())
        return teammates;

    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

    if (coordinator)
    {
        // Use optimized spatial cache query - O(cells) complexity
        auto allySnapshots = coordinator->QueryNearbyAllies(player->GetPosition(), range);

        for (auto const* snapshot : allySnapshots)
        {
            if (!snapshot || !snapshot->isAlive)
                continue;

            // Skip self
            if (snapshot->guid == player->GetGUID())
                continue;

            ::Player* ally = ObjectAccessor::FindPlayer(snapshot->guid);
            if (ally && ally->IsInWorld() && ally->IsAlive())
            {
                teammates.push_back(ally);
            }
        }

        TC_LOG_DEBUG("playerbots.bg", "GetNearbyTeammates (cached): {} found {} allies within {:.1f}",
            player->GetName(), teammates.size(), range);
    }
    else
    {
        // Fallback to legacy O(n) method
        std::list<Player*> nearbyPlayers;
        player->GetPlayerListInGrid(nearbyPlayers, range);

        for (::Player* nearby : nearbyPlayers)
        {
            if (!nearby || nearby == player || !nearby->IsAlive())
                continue;

            // Check if on same team
            if (nearby->GetBGTeam() == player->GetBGTeam())
            {
                teammates.push_back(nearby);
            }
        }

        TC_LOG_DEBUG("playerbots.bg", "GetNearbyTeammates (fallback): {} found {} allies",
            player->GetName(), teammates.size());
    }

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

    float distance = ::std::sqrt(player->GetExactDistSq(Position(objective.x, objective.y, objective.z))); // Calculate once from squared distance
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

    return IsObjectiveInRange(player, Position(objective.x, objective.y, objective.z), OBJECTIVE_RANGE);
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

BGMetrics const& BattlegroundAI::GetPlayerMetrics(uint32 playerGuid) const
{
    ::std::lock_guard lock(_mutex);

    if (!_playerMetrics.count(playerGuid))
    {
        static BGMetrics emptyMetrics;
        return emptyMetrics;
    }

    return _playerMetrics.at(playerGuid);
}

BGMetrics const& BattlegroundAI::GetGlobalMetrics() const
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

    // CRITICAL FIX: BGType enum values ARE map IDs, so use GetMapId() directly
    // The BGType enum in BGState.h uses map IDs:
    //   WARSONG_GULCH = 489, TEMPLE_OF_KOTMOGU = 998, etc.
    // This allows direct casting from the battleground's map ID
    uint32 mapId = bg->GetMapId();

    // Validate the map ID corresponds to a known BG type
    switch (mapId)
    {
        case 489:   // Warsong Gulch
            return BGType::WARSONG_GULCH;
        case 529:   // Arathi Basin
            return BGType::ARATHI_BASIN;
        case 30:    // Alterac Valley
            return BGType::ALTERAC_VALLEY;
        case 566:   // Eye of the Storm
            return BGType::EYE_OF_THE_STORM;
        case 607:   // Strand of the Ancients
            return BGType::STRAND_OF_THE_ANCIENTS;
        case 628:   // Isle of Conquest
            return BGType::ISLE_OF_CONQUEST;
        case 726:   // Twin Peaks
            return BGType::TWIN_PEAKS;
        case 761:   // Battle for Gilneas
            return BGType::BATTLE_FOR_GILNEAS;
        case 727:   // Silvershard Mines
            return BGType::SILVERSHARD_MINES;
        case 998:   // Temple of Kotmogu
            return BGType::TEMPLE_OF_KOTMOGU;
        case 1105:  // Deepwind Gorge
            return BGType::DEEPWIND_GORGE;
        case 1803:  // Seething Shore
            return BGType::SEETHING_SHORE;
        case 1191:  // Ashran
            return BGType::ASHRAN;
        default:
            TC_LOG_WARN("playerbots.bg",
                "BattlegroundAI: Unknown BG map {} for bot {} - defaulting to WSG strategy",
                mapId, player->GetName());
            return BGType::WARSONG_GULCH;
    }
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
    // OPTIMIZATION: Use spatial cache for O(cells) complexity instead of O(n)
    // Try to get any active coordinator for this position
    // Note: This method is called without player context, so we check all coordinators
    auto const& coordinators = sBGCoordinatorMgr->GetAllCoordinators();

    // Get any active coordinator (we need one that covers the same BG instance)
    // The spatial cache maintains player positions across the BG
    for (auto const& pair : coordinators)
    {
        BattlegroundCoordinator* coordinator = pair.second.get();
        if (!coordinator || !coordinator->IsActive())
            continue;

        // Use spatial cache query for enemy count at position
        auto const* spatialCache = coordinator->GetSpatialCache();
        if (spatialCache)
        {
            // Count enemies at the objective position
            uint32 count = coordinator->CountEnemiesInRadius(objLocation, range);
            if (count > 0)
            {
                TC_LOG_DEBUG("playerbots.bg",
                    "CountPlayersAtObjective: {} enemies at ({:.1f}, {:.1f}) within {:.1f}",
                    count, objLocation.GetPositionX(), objLocation.GetPositionY(), range);
            }
            return count;
        }
    }

    // No coordinator available - return 0 (defensive)
    TC_LOG_DEBUG("playerbots.bg", "CountPlayersAtObjective: No coordinator available, returning 0");
    return 0;
}

uint32 BattlegroundAI::CountPlayersAtObjective(Position const& objLocation, float range,
    ::Player* contextPlayer) const
{
    // OPTIMIZATION: Use spatial cache with explicit player context for O(cells) complexity
    if (!contextPlayer)
        return CountPlayersAtObjective(objLocation, range);

    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(contextPlayer);

    if (coordinator)
    {
        // Use optimized spatial cache query
        uint32 enemyCount = coordinator->CountEnemiesInRadius(objLocation, range);
        TC_LOG_DEBUG("playerbots.bg",
            "CountPlayersAtObjective (cached): {} enemies at ({:.1f}, {:.1f})",
            enemyCount, objLocation.GetPositionX(), objLocation.GetPositionY());
        return enemyCount;
    }

    // Fallback - no coordinator
    return 0;
}

::std::vector<::Player*> BattlegroundAI::GetPlayersAtObjective(Position const& objLocation,
    float range) const
{
    // OPTIMIZATION: Use spatial cache for O(cells) complexity instead of O(n)
    ::std::vector<::Player*> players;

    auto const& coordinators = sBGCoordinatorMgr->GetAllCoordinators();

    // Get any active coordinator
    for (auto const& pair : coordinators)
    {
        BattlegroundCoordinator* coordinator = pair.second.get();
        if (!coordinator || !coordinator->IsActive())
            continue;

        // Query nearby enemies from spatial cache
        auto enemySnapshots = coordinator->QueryNearbyEnemies(objLocation, range);

        for (auto const* snapshot : enemySnapshots)
        {
            if (snapshot && snapshot->isAlive)
            {
                ::Player* enemy = ObjectAccessor::FindPlayer(snapshot->guid);
                if (enemy && enemy->IsInWorld() && enemy->IsAlive())
                {
                    players.push_back(enemy);
                }
            }
        }

        // Only check one coordinator (they share the same BG instance data)
        break;
    }

    return players;
}

::std::vector<::Player*> BattlegroundAI::GetPlayersAtObjective(Position const& objLocation,
    float range, ::Player* contextPlayer) const
{
    // OPTIMIZATION: Use spatial cache with explicit player context
    ::std::vector<::Player*> players;

    if (!contextPlayer)
        return GetPlayersAtObjective(objLocation, range);

    BattlegroundCoordinator* coordinator =
        sBGCoordinatorMgr->GetCoordinatorForPlayer(contextPlayer);

    if (coordinator)
    {
        // Query nearby enemies from spatial cache - O(cells) instead of O(n)
        auto enemySnapshots = coordinator->QueryNearbyEnemies(objLocation, range);

        for (auto const* snapshot : enemySnapshots)
        {
            if (snapshot && snapshot->isAlive)
            {
                ::Player* enemy = ObjectAccessor::FindPlayer(snapshot->guid);
                if (enemy && enemy->IsInWorld() && enemy->IsAlive())
                {
                    players.push_back(enemy);
                }
            }
        }
    }

    return players;
}

// ============================================================================
// CTF BEHAVIOR EXECUTION (uses script data)
// ============================================================================

void BattlegroundAI::ExecuteFlagCarrierBehavior(::Player* player,
    BattlegroundCoordinator* coordinator,
    Coordination::Battleground::IBGScript* script)
{
    if (!player || !player->IsInWorld())
        return;

    uint32 teamId = player->GetBGTeam();

    // Get our flag room position from script (where we capture)
    Position capturePoint;
    if (script)
    {
        auto flagRoomPositions = script->GetFlagRoomPositions(teamId);
        if (!flagRoomPositions.empty())
        {
            capturePoint.Relocate(
                flagRoomPositions[0].GetPositionX(),
                flagRoomPositions[0].GetPositionY(),
                flagRoomPositions[0].GetPositionZ()
            );
        }
    }

    // Fallback to hardcoded positions if script unavailable
    if (capturePoint.GetPositionX() == 0.0f)
    {
        BGType bgType = GetBattlegroundType(player);
        if (_flagStrategies.count(bgType))
        {
            capturePoint = (teamId == ALLIANCE)
                ? _flagStrategies[bgType].friendlyFlagSpawn  // Alliance captures at Alliance base
                : _flagStrategies[bgType].enemyFlagSpawn;    // Horde captures at Horde base
        }
    }

    float distance = player->GetExactDist(&capturePoint);

    TC_LOG_DEBUG("playerbots.bg", "WSG FC: {} running flag home (dist: {:.1f})",
        player->GetName(), distance);

    if (distance > OBJECTIVE_RANGE)
    {
        // Run home!
        BotMovementUtil::MoveToPosition(player, capturePoint);
    }
    else
    {
        // At capture point - check if our flag is here to capture
        // If our flag is at base, we auto-capture when touching the flag stand
        TC_LOG_DEBUG("playerbots.bg", "WSG FC: {} at capture point, waiting for our flag",
            player->GetName());

        // Try to find flag stand and interact
        std::list<GameObject*> goList;
        player->GetGameObjectListWithEntryInGrid(goList, 0, OBJECTIVE_RANGE);
        for (GameObject* go : goList)
        {
            if (!go)
                continue;
            GameObjectTemplate const* goInfo = go->GetGOInfo();
            if (goInfo && goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND)
            {
                go->Use(player);
                TC_LOG_INFO("playerbots.bg", "WSG: {} captured flag!", player->GetName());
                break;
            }
        }
    }
}

void BattlegroundAI::ExecuteFlagPickupBehavior(::Player* player,
    BattlegroundCoordinator* coordinator,
    Coordination::Battleground::IBGScript* script)
{
    if (!player || !player->IsInWorld())
        return;

    uint32 teamId = player->GetBGTeam();

    // Get enemy flag position from script
    Position enemyFlagPos;
    if (script)
    {
        auto objectives = script->GetObjectiveData();
        for (const auto& obj : objectives)
        {
            if (obj.type == ObjectiveType::FLAG)
            {
                // Enemy flag: Alliance wants Horde flag (usually id 2), Horde wants Alliance flag (id 1)
                bool isEnemyFlag = (teamId == ALLIANCE && obj.name.find("Horde") != std::string::npos) ||
                                   (teamId == HORDE && obj.name.find("Alliance") != std::string::npos);
                if (isEnemyFlag)
                {
                    enemyFlagPos.Relocate(obj.x, obj.y, obj.z);
                    break;
                }
            }
        }
    }

    // Fallback to hardcoded
    if (enemyFlagPos.GetPositionX() == 0.0f)
    {
        BGType bgType = GetBattlegroundType(player);
        if (_flagStrategies.count(bgType))
        {
            // Alliance goes to Horde base (enemy spawn), Horde goes to Alliance base
            enemyFlagPos = (teamId == ALLIANCE)
                ? _flagStrategies[bgType].enemyFlagSpawn
                : _flagStrategies[bgType].friendlyFlagSpawn;
        }
    }

    float distance = player->GetExactDist(&enemyFlagPos);

    TC_LOG_DEBUG("playerbots.bg", "WSG: {} going to pick up enemy flag (dist: {:.1f})",
        player->GetName(), distance);

    if (distance > OBJECTIVE_RANGE)
    {
        BotMovementUtil::MoveToPosition(player, enemyFlagPos);
    }
    else
    {
        // Try to interact with flag
        std::list<GameObject*> goList;
        player->GetGameObjectListWithEntryInGrid(goList, 0, OBJECTIVE_RANGE);
        for (GameObject* go : goList)
        {
            if (!go)
                continue;
            GameObjectTemplate const* goInfo = go->GetGOInfo();
            if (goInfo && (goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND ||
                           goInfo->type == GAMEOBJECT_TYPE_GOOBER))
            {
                go->Use(player);
                TC_LOG_INFO("playerbots.bg", "WSG: {} picked up flag!", player->GetName());
                break;
            }
        }
    }
}

void BattlegroundAI::ExecuteFlagHunterBehavior(::Player* player, ::Player* enemyFC)
{
    if (!player || !player->IsInWorld() || !enemyFC || !enemyFC->IsInWorld())
        return;

    float distance = player->GetExactDist(enemyFC);

    TC_LOG_DEBUG("playerbots.bg", "WSG: {} hunting enemy FC {} (dist: {:.1f})",
        player->GetName(), enemyFC->GetName(), distance);

    // Chase and attack the enemy FC
    if (distance > 30.0f)
    {
        // Too far - move closer
        BotMovementUtil::MoveToPosition(player, enemyFC->GetPosition());
    }
    else
    {
        // In range - target them for combat
        player->SetSelection(enemyFC->GetGUID());

        // If close enough, start chase
        if (distance > 5.0f)
        {
            BotMovementUtil::ChaseTarget(player, enemyFC, 5.0f);
        }
    }
}

void BattlegroundAI::ExecuteEscortBehavior(::Player* player, ::Player* friendlyFC,
    BattlegroundCoordinator* coordinator,
    Coordination::Battleground::IBGScript* script)
{
    if (!player || !player->IsInWorld() || !friendlyFC || !friendlyFC->IsInWorld())
        return;

    float distance = player->GetExactDist(friendlyFC);
    constexpr float ESCORT_DISTANCE = 8.0f;
    constexpr float MAX_ESCORT_DISTANCE = 40.0f;

    TC_LOG_DEBUG("playerbots.bg", "WSG: {} escorting FC {} (dist: {:.1f})",
        player->GetName(), friendlyFC->GetName(), distance);

    Position escortPos;

    // Try to get formation position from script
    if (script && distance < MAX_ESCORT_DISTANCE)
    {
        auto formation = script->GetEscortFormation(friendlyFC->GetPosition(), 4);
        if (!formation.empty())
        {
            // Pick a position based on our guid (simple distribution)
            uint32 idx = player->GetGUID().GetCounter() % formation.size();
            escortPos = formation[idx];
            BotMovementUtil::CorrectPositionToGround(player, escortPos);
        }
    }

    // Fallback to simple following
    if (escortPos.GetPositionX() == 0.0f || distance > MAX_ESCORT_DISTANCE)
    {
        escortPos = friendlyFC->GetPosition();
        // Offset slightly behind FC
        float angle = friendlyFC->GetOrientation() + M_PI;
        escortPos.Relocate(
            friendlyFC->GetPositionX() + ESCORT_DISTANCE * 0.7f * cos(angle),
            friendlyFC->GetPositionY() + ESCORT_DISTANCE * 0.7f * sin(angle),
            friendlyFC->GetPositionZ()
        );
        BotMovementUtil::CorrectPositionToGround(player, escortPos);
    }

    // Move to escort position
    if (distance > ESCORT_DISTANCE * 1.5f || !BotMovementUtil::IsMoving(player))
    {
        BotMovementUtil::MoveToPosition(player, escortPos);
    }

    // Help kill anyone attacking the FC using spatial cache
    // OPTIMIZATION: O(cells) instead of O(n) - 20x faster for 40v40 BGs
    if (friendlyFC->IsInCombat())
    {
        BattlegroundCoordinator* coord =
            sBGCoordinatorMgr->GetCoordinatorForPlayer(player);

        if (coord)
        {
            // Use optimized spatial cache query - O(cells) complexity
            auto nearbyEnemies = coord->QueryNearbyEnemies(friendlyFC->GetPosition(), 20.0f);
            for (auto const* snapshot : nearbyEnemies)
            {
                if (snapshot && snapshot->isAlive)
                {
                    ::Player* enemy = ObjectAccessor::FindPlayer(snapshot->guid);
                    if (enemy && enemy->IsAlive())
                    {
                        player->SetSelection(enemy->GetGUID());
                        TC_LOG_DEBUG("playerbots.bg", "WSG: {} targeting {} attacking FC (cache)",
                            player->GetName(), enemy->GetName());
                        break;
                    }
                }
            }
        }
        else
        {
            // Fallback to legacy O(n) method
            std::list<Player*> nearbyPlayers;
            friendlyFC->GetPlayerListInGrid(nearbyPlayers, 20.0f);
            for (::Player* nearby : nearbyPlayers)
            {
                if (nearby && nearby->IsAlive() && nearby->IsHostileTo(player))
                {
                    player->SetSelection(nearby->GetGUID());
                    TC_LOG_DEBUG("playerbots.bg", "WSG: {} targeting {} attacking FC",
                        player->GetName(), nearby->GetName());
                    break;
                }
            }
        }
    }
}

void BattlegroundAI::ExecuteDefenderBehavior(::Player* player,
    BattlegroundCoordinator* coordinator,
    Coordination::Battleground::IBGScript* script)
{
    if (!player || !player->IsInWorld())
        return;

    uint32 teamId = player->GetBGTeam();

    // Get our flag room position from script
    Position flagRoomPos;
    if (script)
    {
        auto positions = script->GetFlagRoomPositions(teamId);
        if (!positions.empty())
        {
            // Pick a defensive position
            uint32 idx = player->GetGUID().GetCounter() % positions.size();
            flagRoomPos.Relocate(
                positions[idx].GetPositionX(),
                positions[idx].GetPositionY(),
                positions[idx].GetPositionZ()
            );
        }
    }

    // Fallback
    if (flagRoomPos.GetPositionX() == 0.0f)
    {
        BGType bgType = GetBattlegroundType(player);
        if (_flagStrategies.count(bgType))
        {
            flagRoomPos = (teamId == ALLIANCE)
                ? _flagStrategies[bgType].friendlyFlagSpawn
                : _flagStrategies[bgType].enemyFlagSpawn;
        }
    }

    float distance = player->GetExactDist(&flagRoomPos);
    constexpr float DEFENSE_RADIUS = 25.0f;

    TC_LOG_DEBUG("playerbots.bg", "WSG: {} defending flag room (dist: {:.1f})",
        player->GetName(), distance);

    // Move to flag room if too far
    if (distance > DEFENSE_RADIUS)
    {
        BotMovementUtil::MoveToPosition(player, flagRoomPos);
        return;
    }

    // Look for enemies in flag room using spatial cache
    // OPTIMIZATION: O(cells) instead of O(n) - 20x faster for 40v40 BGs
    ::Player* closestEnemy = nullptr;
    float closestDist = DEFENSE_RADIUS + 1.0f;

    // coordinator is passed as parameter to this method
    if (coordinator)
    {
        // Use optimized spatial cache query - O(cells) complexity
        auto const* nearestSnapshot = coordinator->GetNearestEnemy(
            player->GetPosition(), DEFENSE_RADIUS, &closestDist);

        if (nearestSnapshot)
        {
            closestEnemy = ObjectAccessor::FindPlayer(nearestSnapshot->guid);
        }
    }
    else
    {
        // Fallback to legacy O(n) method if coordinator not available
        std::list<Player*> nearbyPlayers;
        player->GetPlayerListInGrid(nearbyPlayers, DEFENSE_RADIUS);

        for (::Player* nearby : nearbyPlayers)
        {
            if (!nearby || !nearby->IsAlive() || !nearby->IsHostileTo(player))
                continue;

            float dist = player->GetExactDist(nearby);
            if (dist < closestDist)
            {
                closestDist = dist;
                closestEnemy = nearby;
            }
        }
    }

    if (closestEnemy && closestEnemy->IsAlive())
    {
        player->SetSelection(closestEnemy->GetGUID());
        TC_LOG_DEBUG("playerbots.bg", "WSG: {} targeting enemy {} in flag room (cache)",
            player->GetName(), closestEnemy->GetName());

        // Chase if too far
        if (closestDist > 5.0f)
        {
            BotMovementUtil::ChaseTarget(player, closestEnemy, 5.0f);
        }
    }
    else if (!BotMovementUtil::IsMoving(player))
    {
        // No enemies - patrol around flag room
        Position patrolPos;
        float angle = frand(0, 2 * M_PI);
        float dist = frand(5.0f, 12.0f);
        patrolPos.Relocate(
            flagRoomPos.GetPositionX() + dist * cos(angle),
            flagRoomPos.GetPositionY() + dist * sin(angle),
            flagRoomPos.GetPositionZ()
        );
        BotMovementUtil::CorrectPositionToGround(player, patrolPos);
        BotMovementUtil::MoveToPosition(player, patrolPos);
    }

    // Also try to return dropped flags
    ReturnFlag(player);
}

} // namespace Playerbot
