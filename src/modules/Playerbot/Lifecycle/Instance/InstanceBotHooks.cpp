/*
 * Copyright (C) 2008-2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "InstanceBotHooks.h"
#include "InstanceBotOrchestrator.h"
#include "InstanceBotPool.h"
#include "ContentRequirements.h"
#include "JITBotFactory.h"
#include "Player.h"
#include "Group.h"
#include "Battleground.h"
#include "ObjectAccessor.h"
#include "Log.h"

namespace Playerbot
{

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

std::atomic<bool> InstanceBotHooks::_initialized{false};
std::atomic<bool> InstanceBotHooks::_enabled{false};
std::mutex InstanceBotHooks::_callbackMutex;
std::unordered_map<ObjectGuid, InstanceBotHooks::BotAssignmentCallback> InstanceBotHooks::_dungeonCallbacks;
std::unordered_map<uint64, InstanceBotHooks::PvPBotAssignmentCallback> InstanceBotHooks::_bgCallbacks;

// ============================================================================
// INITIALIZATION
// ============================================================================

bool InstanceBotHooks::Initialize()
{
    if (_initialized.exchange(true))
    {
        TC_LOG_WARN("playerbots.instance", "InstanceBotHooks::Initialize called multiple times");
        return true;
    }

    TC_LOG_INFO("playerbots.instance", "Initializing Instance Bot Hooks...");

    // Check if orchestrator is available and enabled
    if (sInstanceBotOrchestrator)
    {
        _enabled.store(sInstanceBotOrchestrator->GetConfig().enabled);
    }
    else
    {
        TC_LOG_ERROR("playerbots.instance", "InstanceBotOrchestrator not available for hooks");
        _enabled.store(false);
        _initialized.store(false);
        return false;
    }

    TC_LOG_INFO("playerbots.instance", "Instance Bot Hooks initialized (enabled: {})",
        _enabled.load() ? "yes" : "no");

    return true;
}

void InstanceBotHooks::Shutdown()
{
    if (!_initialized.exchange(false))
        return;

    TC_LOG_INFO("playerbots.instance", "Shutting down Instance Bot Hooks...");

    _enabled.store(false);

    // Clear all pending callbacks
    {
        std::lock_guard<std::mutex> lock(_callbackMutex);
        _dungeonCallbacks.clear();
        _bgCallbacks.clear();
    }

    TC_LOG_INFO("playerbots.instance", "Instance Bot Hooks shutdown complete");
}

bool InstanceBotHooks::IsEnabled()
{
    return _enabled.load() && _initialized.load();
}

// ============================================================================
// LFG HOOKS
// ============================================================================

void InstanceBotHooks::OnPlayerJoinLfg(
    Player* player,
    std::set<uint32> const& dungeons,
    uint8 roles)
{
    if (!IsEnabled() || !player || dungeons.empty())
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerJoinLfg: Player {} queued for {} dungeons with role {}",
        player->GetName(), dungeons.size(), roles);

    // Get the first dungeon (typically only one for random)
    uint32 dungeonId = *dungeons.begin();

    // Create dungeon request for orchestrator
    DungeonRequest request;
    request.playerGuid = player->GetGUID();
    request.dungeonId = dungeonId;
    request.playerRole = roles;

    // Set up callbacks
    request.onBotsReady = [playerGuid = player->GetGUID()](std::vector<ObjectGuid> const& bots)
    {
        TC_LOG_DEBUG("playerbots.instance", "Dungeon bots ready for player {}: {} bots assigned",
            playerGuid.ToString(), bots.size());

        // Notify any registered callbacks
        std::lock_guard<std::mutex> lock(_callbackMutex);
        auto it = _dungeonCallbacks.find(playerGuid);
        if (it != _dungeonCallbacks.end())
        {
            it->second(true, bots);
            _dungeonCallbacks.erase(it);
        }
    };

    request.onFailed = [playerGuid = player->GetGUID()](std::string const& error)
    {
        TC_LOG_WARN("playerbots.instance", "Failed to get dungeon bots for player {}: {}",
            playerGuid.ToString(), error);

        // Notify any registered callbacks
        std::lock_guard<std::mutex> lock(_callbackMutex);
        auto it = _dungeonCallbacks.find(playerGuid);
        if (it != _dungeonCallbacks.end())
        {
            it->second(false, {});
            _dungeonCallbacks.erase(it);
        }
    };

    // Submit request to orchestrator
    uint32 requestId = sInstanceBotOrchestrator->RequestDungeonBots(request);
    TC_LOG_DEBUG("playerbots.instance", "Dungeon bot request {} submitted for player {}",
        requestId, player->GetName());
}

void InstanceBotHooks::OnPlayerLeaveLfg(Player* player)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerLeaveLfg: Player {} left LFG queue",
        player->GetName());

    // Cancel any pending requests for this player
    // The orchestrator tracks requests by player GUID
    sInstanceBotOrchestrator->CancelRequestsForPlayer(player->GetGUID());

    // Clear any registered callbacks
    {
        std::lock_guard<std::mutex> lock(_callbackMutex);
        _dungeonCallbacks.erase(player->GetGUID());
    }
}

void InstanceBotHooks::OnLfgGroupFormed(
    ObjectGuid groupGuid,
    uint32 dungeonId,
    std::vector<ObjectGuid> const& players)
{
    if (!IsEnabled())
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnLfgGroupFormed: Group {} formed for dungeon {} with {} players",
        groupGuid.ToString(), dungeonId, players.size());

    // Get content requirements
    auto const* requirement = sContentRequirementDb->GetDungeonRequirement(dungeonId);
    if (!requirement)
    {
        TC_LOG_WARN("playerbots.instance", "No content requirement found for dungeon {}", dungeonId);
        return;
    }

    // Calculate how many bots we need
    uint32 maxPlayers = requirement->maxPlayers;
    uint32 currentPlayers = static_cast<uint32>(players.size());

    if (currentPlayers >= maxPlayers)
    {
        TC_LOG_DEBUG("playerbots.instance", "Group {} already full ({}/{})",
            groupGuid.ToString(), currentPlayers, maxPlayers);
        return;
    }

    // Find first human player for faction/level reference
    Player* refPlayer = nullptr;
    for (ObjectGuid const& guid : players)
    {
        if (Player* player = ObjectAccessor::FindPlayer(guid))
        {
            if (!sInstanceBotPool->IsPoolBot(guid))
            {
                refPlayer = player;
                break;
            }
        }
    }

    if (!refPlayer)
    {
        TC_LOG_WARN("playerbots.instance", "No human player found in LFG group {}", groupGuid.ToString());
        return;
    }

    // The group should already have bots from the initial queue request
    // This hook is for verification and late additions
    TC_LOG_DEBUG("playerbots.instance", "LFG group {} ready with {}/{} players",
        groupGuid.ToString(), currentPlayers, maxPlayers);
}

void InstanceBotHooks::OnLfgProposalAccepted(
    ObjectGuid groupGuid,
    uint32 dungeonId)
{
    if (!IsEnabled())
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnLfgProposalAccepted: Group {} accepted for dungeon {}",
        groupGuid.ToString(), dungeonId);

    // Mark assigned bots as entering instance
    sInstanceBotOrchestrator->OnInstanceCreated(
        groupGuid.GetCounter(),
        InstanceType::Dungeon,
        dungeonId
    );
}

// ============================================================================
// BATTLEGROUND HOOKS
// ============================================================================

void InstanceBotHooks::OnPlayerJoinBattleground(
    Player* player,
    uint32 bgTypeId,
    uint32 bracketId,
    bool asGroup)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerJoinBattleground: Player {} queued for BG {} bracket {} (group: {})",
        player->GetName(), bgTypeId, bracketId, asGroup ? "yes" : "no");

    // Get content requirements
    auto const* requirement = sContentRequirementDb->GetBattlegroundRequirement(bgTypeId);
    if (!requirement)
    {
        TC_LOG_WARN("playerbots.instance", "No content requirement found for battleground {}", bgTypeId);
        return;
    }

    // For large BGs (AV, IoC), start JIT creation immediately
    if (requirement->requiresBothFactions && requirement->playersPerFaction >= 20)
    {
        TC_LOG_INFO("playerbots.instance", "Large battleground {} detected - preparing JIT bots for {}/faction",
            bgTypeId, requirement->playersPerFaction);

        // Create battleground request for orchestrator
        BattlegroundRequest request;
        request.bgTypeId = bgTypeId;
        request.bracketLevel = bracketId;
        request.currentAlliancePlayers = player->GetTeam() == ALLIANCE ? 1 : 0;
        request.currentHordePlayers = player->GetTeam() == HORDE ? 1 : 0;
        request.playerFaction = player->GetTeam() == ALLIANCE ? Faction::Alliance : Faction::Horde;

        request.onBotsReady = [bgTypeId, bracketId](
            std::vector<ObjectGuid> const& alliance,
            std::vector<ObjectGuid> const& horde)
        {
            TC_LOG_INFO("playerbots.instance", "BG {} bots ready: {} Alliance, {} Horde",
                bgTypeId, alliance.size(), horde.size());

            // Notify any registered callbacks
            std::lock_guard<std::mutex> lock(_callbackMutex);
            uint64 key = MakeBGCallbackKey(bgTypeId, bracketId);
            auto it = _bgCallbacks.find(key);
            if (it != _bgCallbacks.end())
            {
                it->second(true, alliance, horde);
                _bgCallbacks.erase(it);
            }
        };

        request.onFailed = [bgTypeId, bracketId](std::string const& error)
        {
            TC_LOG_WARN("playerbots.instance", "Failed to get BG {} bots: {}", bgTypeId, error);

            std::lock_guard<std::mutex> lock(_callbackMutex);
            uint64 key = MakeBGCallbackKey(bgTypeId, bracketId);
            auto it = _bgCallbacks.find(key);
            if (it != _bgCallbacks.end())
            {
                it->second(false, {}, {});
                _bgCallbacks.erase(it);
            }
        };

        sInstanceBotOrchestrator->RequestBattlegroundBots(request);
    }
}

void InstanceBotHooks::OnPlayerLeaveBattlegroundQueue(
    Player* player,
    uint32 bgTypeId)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerLeaveBattlegroundQueue: Player {} left BG {} queue",
        player->GetName(), bgTypeId);

    // Don't cancel BG bot creation - other players may still be queued
    // The queue system handles this naturally
}

bool InstanceBotHooks::OnBattlegroundQueueUpdate(
    uint32 bgTypeId,
    uint32 bracketId,
    uint32 allianceInQueue,
    uint32 hordeInQueue,
    uint32 minPlayersPerTeam,
    uint32 maxPlayersPerTeam)
{
    if (!IsEnabled())
        return false;

    // Only process if at least one human is queued
    if (allianceInQueue == 0 && hordeInQueue == 0)
        return false;

    auto const* requirement = sContentRequirementDb->GetBattlegroundRequirement(bgTypeId);
    if (!requirement)
        return false;

    // Check if we should fill with bots
    uint32 totalQueued = allianceInQueue + hordeInQueue;

    // For PvP BGs requiring both factions
    if (requirement->requiresBothFactions)
    {
        // Check if we can start the BG with bots
        bool allianceReady = allianceInQueue >= 1;  // At least 1 human
        bool hordeReady = hordeInQueue >= 1 || allianceInQueue >= 1;  // Can fill with bots

        if (allianceReady || hordeReady)
        {
            uint32 neededAlliance = maxPlayersPerTeam - allianceInQueue;
            uint32 neededHorde = maxPlayersPerTeam - hordeInQueue;

            TC_LOG_DEBUG("playerbots.instance",
                "BG {} queue update: Alliance {}/{}, Horde {}/{} - need {} Alliance bots, {} Horde bots",
                bgTypeId, allianceInQueue, maxPlayersPerTeam, hordeInQueue, maxPlayersPerTeam,
                neededAlliance, neededHorde);

            // Check if orchestrator can provide bots
            return sInstanceBotOrchestrator->CanProvideBotsFor(InstanceType::Battleground, bgTypeId);
        }
    }

    return false;
}

void InstanceBotHooks::OnBattlegroundStarting(
    Battleground* bg,
    uint32 allianceCount,
    uint32 hordeCount)
{
    if (!IsEnabled() || !bg)
        return;

    uint32 bgTypeId = bg->GetTypeID();
    uint32 instanceId = bg->GetInstanceID();

    TC_LOG_DEBUG("playerbots.instance",
        "OnBattlegroundStarting: BG {} instance {} starting with {} Alliance, {} Horde",
        bgTypeId, instanceId, allianceCount, hordeCount);

    // Register this instance with the orchestrator
    sInstanceBotOrchestrator->OnInstanceCreated(
        instanceId,
        InstanceType::Battleground,
        bgTypeId
    );

    // Get content requirements
    auto const* requirement = sContentRequirementDb->GetBattlegroundRequirement(bgTypeId);
    if (!requirement)
        return;

    uint32 maxPerTeam = requirement->playersPerFaction;
    uint32 neededAlliance = maxPerTeam > allianceCount ? maxPerTeam - allianceCount : 0;
    uint32 neededHorde = maxPerTeam > hordeCount ? maxPerTeam - hordeCount : 0;

    if (neededAlliance == 0 && neededHorde == 0)
    {
        TC_LOG_DEBUG("playerbots.instance", "BG {} fully staffed, no bots needed", instanceId);
        return;
    }

    TC_LOG_INFO("playerbots.instance", "BG {} needs {} Alliance bots, {} Horde bots",
        instanceId, neededAlliance, neededHorde);

    // Request bots from orchestrator (already should be reserved)
    BattlegroundRequest request;
    request.bgTypeId = bgTypeId;
    request.bracketLevel = bg->GetBracketId();
    request.currentAlliancePlayers = allianceCount;
    request.currentHordePlayers = hordeCount;
    request.playerFaction = allianceCount > 0 ? Faction::Alliance : Faction::Horde;

    request.onBotsReady = [instanceId, bgTypeId](
        std::vector<ObjectGuid> const& alliance,
        std::vector<ObjectGuid> const& horde)
    {
        TC_LOG_INFO("playerbots.instance",
            "BG {} instance {} - adding {} Alliance bots, {} Horde bots",
            bgTypeId, instanceId, alliance.size(), horde.size());

        // Bots are added to the BG through the normal queue mechanism
        // The orchestrator handles teleporting bots to the BG
    };

    request.onFailed = [instanceId, bgTypeId](std::string const& error)
    {
        TC_LOG_ERROR("playerbots.instance",
            "Failed to fill BG {} instance {}: {}",
            bgTypeId, instanceId, error);
    };

    sInstanceBotOrchestrator->RequestBattlegroundBots(request);
}

void InstanceBotHooks::OnBattlegroundEnded(
    Battleground* bg,
    uint32 winnerTeam)
{
    if (!IsEnabled() || !bg)
        return;

    uint32 instanceId = bg->GetInstanceID();
    uint32 bgTypeId = bg->GetTypeID();

    TC_LOG_DEBUG("playerbots.instance", "OnBattlegroundEnded: BG {} instance {} ended, winner: {}",
        bgTypeId, instanceId, winnerTeam == ALLIANCE ? "Alliance" : "Horde");

    // Release all bots from this BG back to pool
    sInstanceBotOrchestrator->OnInstanceEnded(instanceId);

    // Get bots in this instance for recycling
    std::vector<ObjectGuid> bots = sInstanceBotOrchestrator->GetBotsInInstance(instanceId);
    if (!bots.empty())
    {
        sJITBotFactory->RecycleBots(bots);
    }

    TC_LOG_DEBUG("playerbots.instance", "Released {} bots from BG {} instance {}",
        bots.size(), bgTypeId, instanceId);
}

void InstanceBotHooks::OnPlayerLeftBattleground(
    Player* player,
    Battleground* bg)
{
    if (!IsEnabled() || !player || !bg)
        return;

    // If this was the last human player, end the BG
    // (Let the core handle this - we just track for logging)
    TC_LOG_DEBUG("playerbots.instance", "Player {} left BG {} instance {}",
        player->GetName(), bg->GetTypeID(), bg->GetInstanceID());
}

// ============================================================================
// ARENA HOOKS
// ============================================================================

void InstanceBotHooks::OnPlayerJoinArena(
    Player* player,
    uint32 arenaType,
    uint32 bracketId,
    bool isRated,
    std::vector<ObjectGuid> const& teamMembers)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerJoinArena: Player {} queued for {}v{} arena (rated: {})",
        player->GetName(), arenaType, arenaType, isRated ? "yes" : "no");

    // Create arena request
    ArenaRequest request;
    request.arenaType = arenaType;
    request.bracketLevel = bracketId;
    request.playerGuid = player->GetGUID();
    request.playerFaction = player->GetTeam() == ALLIANCE ? Faction::Alliance : Faction::Horde;
    request.existingTeammates = teamMembers;
    request.needOpponents = true;

    request.onBotsReady = [arenaType, playerName = player->GetName()](
        std::vector<ObjectGuid> const& teammates,
        std::vector<ObjectGuid> const& opponents)
    {
        TC_LOG_DEBUG("playerbots.instance", "Arena {} bots ready for {}: {} teammates, {} opponents",
            arenaType, playerName, teammates.size(), opponents.size());
    };

    request.onFailed = [arenaType, playerName = player->GetName()](std::string const& error)
    {
        TC_LOG_WARN("playerbots.instance", "Failed to get arena {} bots for {}: {}",
            arenaType, playerName, error);
    };

    sInstanceBotOrchestrator->RequestArenaBots(request);
}

void InstanceBotHooks::OnPlayerLeaveArenaQueue(
    Player* player,
    uint32 arenaType)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerLeaveArenaQueue: Player {} left arena {} queue",
        player->GetName(), arenaType);

    sInstanceBotOrchestrator->CancelRequestsForPlayer(player->GetGUID());
}

bool InstanceBotHooks::OnArenaMatchPreparing(
    uint32 arenaType,
    uint32 bracketId,
    std::vector<ObjectGuid>& team1Players,
    std::vector<ObjectGuid>& team2Players,
    uint32 team1NeedsPlayers,
    uint32 team2NeedsPlayers)
{
    if (!IsEnabled())
        return false;

    TC_LOG_DEBUG("playerbots.instance",
        "OnArenaMatchPreparing: {}v{} - Team1 needs {}, Team2 needs {}",
        arenaType, arenaType, team1NeedsPlayers, team2NeedsPlayers);

    if (team1NeedsPlayers == 0 && team2NeedsPlayers == 0)
        return true;  // Match is ready

    // Get arena requirements
    auto const* requirement = sContentRequirementDb->GetArenaRequirement(arenaType);
    if (!requirement)
        return false;

    // Determine factions (for mixed arena, use Alliance for team1, Horde for team2)
    Faction team1Faction = Faction::Alliance;
    Faction team2Faction = Faction::Horde;

    // Try to get bots from pool
    if (team1NeedsPlayers > 0)
    {
        ArenaAssignment assignment = sInstanceBotPool->AssignForArena(
            arenaType,
            bracketId,
            team1Faction,
            team1NeedsPlayers,
            0  // No opponents from this call
        );

        for (ObjectGuid const& guid : assignment.teammates)
        {
            team1Players.push_back(guid);
        }
    }

    if (team2NeedsPlayers > 0)
    {
        ArenaAssignment assignment = sInstanceBotPool->AssignForArena(
            arenaType,
            bracketId,
            team2Faction,
            team2NeedsPlayers,
            0
        );

        for (ObjectGuid const& guid : assignment.teammates)
        {
            team2Players.push_back(guid);
        }
    }

    // Check if we now have enough
    bool team1Ready = team1Players.size() >= arenaType;
    bool team2Ready = team2Players.size() >= arenaType;

    TC_LOG_DEBUG("playerbots.instance", "Arena match prep: Team1 {}/{}, Team2 {}/{}",
        team1Players.size(), arenaType, team2Players.size(), arenaType);

    return team1Ready && team2Ready;
}

void InstanceBotHooks::OnArenaMatchEnded(
    uint32 arenaType,
    uint32 winnerTeam,
    std::vector<ObjectGuid> const& team1Players,
    std::vector<ObjectGuid> const& team2Players)
{
    if (!IsEnabled())
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnArenaMatchEnded: {}v{} - Winner: Team {}",
        arenaType, arenaType, winnerTeam + 1);

    // Release bots back to pool
    for (ObjectGuid const& guid : team1Players)
    {
        if (sInstanceBotPool->IsPoolBot(guid))
        {
            sInstanceBotPool->ReleaseBots({guid});
        }
    }

    for (ObjectGuid const& guid : team2Players)
    {
        if (sInstanceBotPool->IsPoolBot(guid))
        {
            sInstanceBotPool->ReleaseBots({guid});
        }
    }
}

// ============================================================================
// RAID/INSTANCE HOOKS
// ============================================================================

void InstanceBotHooks::OnPlayerEnterInstance(
    Player* player,
    uint32 mapId,
    uint32 instanceId,
    bool isRaid)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerEnterInstance: Player {} entered {} instance {} (map {})",
        player->GetName(), isRaid ? "raid" : "dungeon", instanceId, mapId);

    // Track the instance
    sInstanceBotOrchestrator->OnInstanceCreated(
        instanceId,
        isRaid ? InstanceType::Raid : InstanceType::Dungeon,
        mapId
    );
}

void InstanceBotHooks::OnPlayerLeaveInstance(
    Player* player,
    uint32 mapId,
    uint32 instanceId)
{
    if (!IsEnabled() || !player)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnPlayerLeaveInstance: Player {} left instance {} (map {})",
        player->GetName(), instanceId, mapId);

    sInstanceBotOrchestrator->OnPlayerLeftInstance(player->GetGUID(), instanceId);
}

void InstanceBotHooks::OnInstanceReset(
    uint32 mapId,
    uint32 instanceId)
{
    if (!IsEnabled())
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnInstanceReset: Instance {} (map {}) reset",
        instanceId, mapId);

    // Release all bots from this instance
    sInstanceBotOrchestrator->OnInstanceEnded(instanceId);
}

void InstanceBotHooks::OnRaidNeedsBots(
    Player* leader,
    uint32 raidId,
    std::vector<ObjectGuid> const& currentMembers,
    std::map<ObjectGuid, uint8> const& memberRoles)
{
    if (!IsEnabled() || !leader)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnRaidNeedsBots: Leader {} needs bots for raid {} ({} current members)",
        leader->GetName(), raidId, currentMembers.size());

    // Create raid request
    RaidRequest request;
    request.leaderGuid = leader->GetGUID();
    request.raidId = raidId;
    request.currentGroupMembers = currentMembers;
    request.memberRoles = memberRoles;

    request.onBotsReady = [raidId, leaderName = leader->GetName()](std::vector<ObjectGuid> const& bots)
    {
        TC_LOG_INFO("playerbots.instance", "Raid {} bots ready for {}: {} bots assigned",
            raidId, leaderName, bots.size());
    };

    request.onFailed = [raidId, leaderName = leader->GetName()](std::string const& error)
    {
        TC_LOG_WARN("playerbots.instance", "Failed to get raid {} bots for {}: {}",
            raidId, leaderName, error);
    };

    sInstanceBotOrchestrator->RequestRaidBots(request);
}

// ============================================================================
// GROUP HOOKS
// ============================================================================

void InstanceBotHooks::OnGroupDisbanded(ObjectGuid groupGuid)
{
    if (!IsEnabled())
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnGroupDisbanded: Group {} disbanded",
        groupGuid.ToString());

    // Release any bots assigned to this group
    // The instance cleanup will handle the bot release
}

void InstanceBotHooks::OnGroupLeaderChanged(
    Group* group,
    Player* newLeader)
{
    if (!IsEnabled() || !group || !newLeader)
        return;

    TC_LOG_DEBUG("playerbots.instance", "OnGroupLeaderChanged: Group {} new leader {}",
        group->GetGUID().ToString(), newLeader->GetName());

    // Update bot master reference if needed
    // Bots should follow the new leader's commands
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool InstanceBotHooks::IsPoolBot(ObjectGuid guid)
{
    if (!IsEnabled())
        return false;

    return sInstanceBotPool->IsPoolBot(guid);
}

uint32 InstanceBotHooks::GetEstimatedWaitTime(
    uint8 contentType,
    uint32 contentId)
{
    if (!IsEnabled())
        return 0;

    InstanceType type = static_cast<InstanceType>(contentType);
    auto waitTime = sInstanceBotOrchestrator->GetEstimatedWaitTime(type, contentId, 1);

    return static_cast<uint32>(waitTime.count());
}

void InstanceBotHooks::GetPoolStats(
    uint32& outReady,
    uint32& outAssigned,
    uint32& outTotal)
{
    if (!IsEnabled())
    {
        outReady = 0;
        outAssigned = 0;
        outTotal = 0;
        return;
    }

    PoolStatistics stats = sInstanceBotPool->GetStatistics();
    outReady = stats.slotStats.readySlots;
    outAssigned = stats.slotStats.assignedSlots;
    outTotal = stats.slotStats.GetTotal();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void InstanceBotHooks::RegisterDungeonCallback(
    ObjectGuid playerGuid,
    BotAssignmentCallback callback)
{
    if (!IsEnabled())
        return;

    std::lock_guard<std::mutex> lock(_callbackMutex);
    _dungeonCallbacks[playerGuid] = std::move(callback);
}

void InstanceBotHooks::RegisterBattlegroundCallback(
    uint32 bgTypeId,
    uint32 bracketId,
    PvPBotAssignmentCallback callback)
{
    if (!IsEnabled())
        return;

    std::lock_guard<std::mutex> lock(_callbackMutex);
    uint64 key = MakeBGCallbackKey(bgTypeId, bracketId);
    _bgCallbacks[key] = std::move(callback);
}

} // namespace Playerbot
