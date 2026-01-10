/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BGBotManager.h"
#include "../Core/PlayerBotHooks.h"
#include "../Core/PlayerBotHelpers.h"
#include "../Session/BotWorldSessionMgr.h"
#include "Player.h"
#include "Group.h"
#include "GroupMgr.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "World.h"
#include "WorldSession.h"
#include "GameTime.h"
#include "DB2Stores.h"

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

BGBotManager::BGBotManager()
    : _enabled(false)
    , _updateAccumulator(0)
    , _initialized(false)
{
}

BGBotManager::~BGBotManager()
{
    Shutdown();
}

BGBotManager* BGBotManager::instance()
{
    static BGBotManager instance;
    return &instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void BGBotManager::Initialize()
{
    std::lock_guard lock(_mutex);

    if (_initialized)
    {
        TC_LOG_WARN("module.playerbot.bg", "BGBotManager::Initialize - Already initialized");
        return;
    }

    TC_LOG_INFO("module.playerbot.bg", "Initializing Battleground Bot Manager...");

    _queuedBots.clear();
    _humanPlayers.clear();
    _bgInstanceBots.clear();
    _updateAccumulator = 0;

    _enabled = true;
    _initialized = true;

    TC_LOG_INFO("module.playerbot.bg", "Battleground Bot Manager initialized (Enabled: {})", _enabled.load());
}

void BGBotManager::Shutdown()
{
    std::lock_guard lock(_mutex);

    if (!_initialized)
        return;

    TC_LOG_INFO("module.playerbot.bg", "Shutting down Battleground Bot Manager...");

    // Remove all bots from queues
    for (auto const& [botGuid, queueInfo] : _queuedBots)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }
    }

    _queuedBots.clear();
    _humanPlayers.clear();
    _bgInstanceBots.clear();

    _initialized = false;
    _enabled = false;

    TC_LOG_INFO("module.playerbot.bg", "Battleground Bot Manager shut down");
}

void BGBotManager::Update(uint32 diff)
{
    if (!_enabled || !_initialized)
        return;

    _updateAccumulator += diff;

    if (_updateAccumulator >= CLEANUP_INTERVAL)
    {
        CleanupStaleAssignments();
        _updateAccumulator = 0;
    }
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

void BGBotManager::OnPlayerJoinQueue(Player* player, BattlegroundTypeId bgTypeId,
                                      BattlegroundBracketId bracket, bool asGroup)
{
    if (!_enabled || !_initialized)
        return;

    // Only process human players
    if (PlayerBotHooks::IsPlayerBot(player))
        return;

    ObjectGuid playerGuid = player->GetGUID();
    Team playerTeam = player->GetTeam();

    TC_LOG_INFO("module.playerbot.bg", "BGBotManager::OnPlayerJoinQueue - Player {} joined BG queue (Type: {}, Bracket: {}, Team: {})",
                player->GetName(), static_cast<uint32>(bgTypeId), static_cast<uint32>(bracket),
                playerTeam == ALLIANCE ? "Alliance" : "Horde");

    // Calculate needed bots for both factions
    uint32 allianceNeeded = 0, hordeNeeded = 0;
    CalculateNeededBots(bgTypeId, playerTeam, allianceNeeded, hordeNeeded);

    uint32 totalNeeded = allianceNeeded + hordeNeeded;
    if (totalNeeded == 0)
    {
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::OnPlayerJoinQueue - No bots needed");
        return;
    }

    TC_LOG_INFO("module.playerbot.bg", "BGBotManager::OnPlayerJoinQueue - Need {} Alliance, {} Horde bots",
                allianceNeeded, hordeNeeded);

    // Populate queue with bots
    uint32 botsQueued = PopulateQueue(playerGuid, bgTypeId, bracket, allianceNeeded, hordeNeeded);

    TC_LOG_INFO("module.playerbot.bg", "BGBotManager::OnPlayerJoinQueue - Queued {} bots for player {}",
                botsQueued, player->GetName());
}

void BGBotManager::OnPlayerLeaveQueue(ObjectGuid playerGuid)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    // Check if this is a human player with assigned bots
    auto humanItr = _humanPlayers.find(playerGuid);
    if (humanItr != _humanPlayers.end())
    {
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::OnPlayerLeaveQueue - Human player left, removing {} bots",
                     humanItr->second.assignedBots.size());

        for (ObjectGuid botGuid : humanItr->second.assignedBots)
        {
            if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
            {
                RemoveBotFromQueue(bot);
            }
            _queuedBots.erase(botGuid);
        }

        _humanPlayers.erase(humanItr);
    }
    else
    {
        // Check if this is a bot
        auto botItr = _queuedBots.find(playerGuid);
        if (botItr != _queuedBots.end())
        {
            ObjectGuid humanGuid = botItr->second.humanPlayerGuid;
            auto humanPlayerItr = _humanPlayers.find(humanGuid);
            if (humanPlayerItr != _humanPlayers.end())
            {
                auto& bots = humanPlayerItr->second.assignedBots;
                bots.erase(std::remove(bots.begin(), bots.end(), playerGuid), bots.end());

                if (bots.empty())
                    _humanPlayers.erase(humanPlayerItr);
            }

            _queuedBots.erase(botItr);
        }
    }
}

void BGBotManager::OnInvitationReceived(ObjectGuid playerGuid, uint32 bgInstanceGuid)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    auto itr = _queuedBots.find(playerGuid);
    if (itr == _queuedBots.end())
        return;

    // Update bot's BG instance
    itr->second.bgInstanceGuid = bgInstanceGuid;
    _bgInstanceBots[bgInstanceGuid].insert(playerGuid);

    // Auto-accept the invitation for bot
    if (Player* bot = ObjectAccessor::FindPlayer(playerGuid))
    {
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::OnInvitationReceived - Bot {} accepting BG invitation",
                     bot->GetName());

        // Use BattlegroundMgr to handle the acceptance
        // The bot will be teleported when the BG starts
        if (Battleground* bg = sBattlegroundMgr->GetBattleground(bgInstanceGuid, itr->second.bgTypeId))
        {
            // Construct the queue type ID for AddPlayer
            BattlegroundQueueTypeId queueTypeId = BattlegroundMgr::BGQueueTypeId(
                static_cast<uint16>(itr->second.bgTypeId),
                BattlegroundQueueIdType::Battleground,
                false,  // Not rated
                0       // TeamSize (0 for regular BG)
            );
            bg->AddPlayer(bot, queueTypeId);
        }
    }
}

void BGBotManager::OnBattlegroundStart(Battleground* bg)
{
    if (!_enabled || !_initialized || !bg)
        return;

    std::lock_guard lock(_mutex);

    uint32 bgInstanceGuid = bg->GetInstanceID();

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::OnBattlegroundStart - BG instance {} started",
                 bgInstanceGuid);

    // All bots in this BG should already be added via OnInvitationReceived
    // The BG system handles teleportation
}

void BGBotManager::OnBattlegroundEnd(Battleground* bg, Team winnerTeam)
{
    if (!_enabled || !_initialized || !bg)
        return;

    std::lock_guard lock(_mutex);

    uint32 bgInstanceGuid = bg->GetInstanceID();

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::OnBattlegroundEnd - BG instance {} ended, winner: {}",
                 bgInstanceGuid, winnerTeam == ALLIANCE ? "Alliance" : "Horde");

    // Cleanup bots from this BG instance
    auto itr = _bgInstanceBots.find(bgInstanceGuid);
    if (itr != _bgInstanceBots.end())
    {
        for (ObjectGuid botGuid : itr->second)
        {
            UnregisterBotAssignment(botGuid);
        }
        _bgInstanceBots.erase(itr);
    }
}

// ============================================================================
// QUEUE POPULATION
// ============================================================================

uint32 BGBotManager::PopulateQueue(ObjectGuid playerGuid, BattlegroundTypeId bgTypeId,
                                    BattlegroundBracketId bracket,
                                    uint32 neededAlliance, uint32 neededHorde)
{
    std::lock_guard lock(_mutex);

    Player* humanPlayer = ObjectAccessor::FindPlayer(playerGuid);
    if (!humanPlayer)
    {
        TC_LOG_ERROR("module.playerbot.bg", "BGBotManager::PopulateQueue - Player {} not found", playerGuid.ToString());
        return 0;
    }

    // Get level range from DB2 data (proper approach using PVPDifficultyEntry)
    uint8 minLevel = 10, maxLevel = 80;  // Default fallback

    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (bgTemplate && !bgTemplate->MapIDs.empty())
    {
        PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketById(
            bgTemplate->MapIDs.front(), bracket);
        if (bracketEntry)
        {
            minLevel = bracketEntry->MinLevel;
            maxLevel = bracketEntry->MaxLevel;
            TC_LOG_DEBUG("module.playerbot.bg",
                "BGBotManager::PopulateQueue - Using DB2 level range for bracket {}: {}-{}",
                static_cast<uint32>(bracket), minLevel, maxLevel);
        }
        else
        {
            TC_LOG_WARN("module.playerbot.bg",
                "BGBotManager::PopulateQueue - No PVPDifficultyEntry for bracket {} on map {}, using fallback",
                static_cast<uint32>(bracket), bgTemplate->MapIDs.front());
        }
    }
    else
    {
        TC_LOG_WARN("module.playerbot.bg",
            "BGBotManager::PopulateQueue - No BG template for type {}, using fallback level range",
            static_cast<uint32>(bgTypeId));
    }

    TC_LOG_INFO("module.playerbot.bg",
        "BGBotManager::PopulateQueue - Looking for bots level {}-{} for bracket {}",
        minLevel, maxLevel, static_cast<uint32>(bracket));

    uint32 botsQueued = 0;

    // Queue Alliance bots
    if (neededAlliance > 0)
    {
        std::vector<Player*> allianceBots = FindAvailableBots(ALLIANCE, minLevel, maxLevel, neededAlliance);
        for (Player* bot : allianceBots)
        {
            if (QueueBot(bot, bgTypeId, bracket))
            {
                RegisterBotAssignment(playerGuid, bot->GetGUID(), bgTypeId, ALLIANCE);
                ++botsQueued;
                TC_LOG_DEBUG("module.playerbot.bg", "Queued Alliance bot {} for BG", bot->GetName());
            }
        }
    }

    // Queue Horde bots
    if (neededHorde > 0)
    {
        std::vector<Player*> hordeBots = FindAvailableBots(HORDE, minLevel, maxLevel, neededHorde);
        for (Player* bot : hordeBots)
        {
            if (QueueBot(bot, bgTypeId, bracket))
            {
                RegisterBotAssignment(playerGuid, bot->GetGUID(), bgTypeId, HORDE);
                ++botsQueued;
                TC_LOG_DEBUG("module.playerbot.bg", "Queued Horde bot {} for BG", bot->GetName());
            }
        }
    }

    // Track human player
    if (botsQueued > 0)
    {
        auto& humanInfo = _humanPlayers[playerGuid];
        humanInfo.bgTypeId = bgTypeId;
        humanInfo.bracket = bracket;
        humanInfo.team = humanPlayer->GetTeam();
    }

    return botsQueued;
}

bool BGBotManager::IsBotQueued(ObjectGuid botGuid) const
{
    std::lock_guard lock(_mutex);
    return _queuedBots.find(botGuid) != _queuedBots.end();
}

void BGBotManager::GetStatistics(uint32& totalQueued, uint32& totalAssignments) const
{
    std::lock_guard lock(_mutex);
    totalQueued = static_cast<uint32>(_queuedBots.size());
    totalAssignments = static_cast<uint32>(_humanPlayers.size());
}

void BGBotManager::SetEnabled(bool enable)
{
    _enabled = enable;
    TC_LOG_INFO("module.playerbot.bg", "Battleground Bot Manager {}", enable ? "enabled" : "disabled");

    if (!enable)
    {
        std::lock_guard lock(_mutex);
        for (auto const& [botGuid, queueInfo] : _queuedBots)
        {
            if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
            {
                RemoveBotFromQueue(bot);
            }
        }
        _queuedBots.clear();
        _humanPlayers.clear();
        _bgInstanceBots.clear();
    }
}

void BGBotManager::CleanupStaleAssignments()
{
    std::lock_guard lock(_mutex);

    time_t currentTime = time(nullptr);
    std::vector<ObjectGuid> staleBots;
    std::vector<ObjectGuid> staleHumans;

    // Find stale bots
    for (auto const& [botGuid, queueInfo] : _queuedBots)
    {
        if ((currentTime - queueInfo.queueTime) > MAX_QUEUE_TIME)
        {
            staleBots.push_back(botGuid);
            continue;
        }

        Player* bot = ObjectAccessor::FindPlayer(botGuid);
        if (!bot)
        {
            staleBots.push_back(botGuid);
        }
    }

    // Find stale humans
    for (auto const& [humanGuid, playerInfo] : _humanPlayers)
    {
        Player* human = ObjectAccessor::FindPlayer(humanGuid);
        if (!human)
        {
            staleHumans.push_back(humanGuid);
        }
    }

    // Remove stale bots
    for (ObjectGuid botGuid : staleBots)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }
        UnregisterBotAssignment(botGuid);
    }

    // Remove stale humans
    for (ObjectGuid humanGuid : staleHumans)
    {
        UnregisterAllBotsForPlayer(humanGuid);
    }

    if (!staleBots.empty() || !staleHumans.empty())
    {
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::CleanupStaleAssignments - Removed {} bots, {} humans",
                     staleBots.size(), staleHumans.size());
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

uint32 BGBotManager::GetBGTeamSize(BattlegroundTypeId bgTypeId) const
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_WS:  // Warsong Gulch
        case BATTLEGROUND_TP:  // Twin Peaks
            return 10;
        case BATTLEGROUND_AB:  // Arathi Basin
        case BATTLEGROUND_BFG: // Battle for Gilneas
            return 15;
        case BATTLEGROUND_AV:  // Alterac Valley
            return 40;
        case BATTLEGROUND_EY:  // Eye of the Storm
            return 15;
        case BATTLEGROUND_SA:  // Strand of the Ancients
            return 15;
        case BATTLEGROUND_IC:  // Isle of Conquest
            return 40;
        case BATTLEGROUND_RB:  // Random BG
            return 15; // Default to medium size
        default:
            return 10;
    }
}

uint32 BGBotManager::GetBGMinPlayers(BattlegroundTypeId bgTypeId) const
{
    // TrinityCore 11.2 uses lower minimums for testing/single-player
    // In production this would query BattlegroundTemplate
    switch (bgTypeId)
    {
        case BATTLEGROUND_WS:
        case BATTLEGROUND_TP:
            return 5; // 5v5 minimum
        case BATTLEGROUND_AB:
        case BATTLEGROUND_BFG:
        case BATTLEGROUND_EY:
            return 8;
        case BATTLEGROUND_AV:
        case BATTLEGROUND_IC:
            return 20;
        default:
            return 5;
    }
}

void BGBotManager::CalculateNeededBots(BattlegroundTypeId bgTypeId, Team humanTeam,
                                        uint32& allianceNeeded, uint32& hordeNeeded) const
{
    uint32 teamSize = GetBGTeamSize(bgTypeId);

    // Human is on one team, need bots for both
    // Assume human already counts as 1 for their team
    if (humanTeam == ALLIANCE)
    {
        allianceNeeded = teamSize - 1;  // -1 for human
        hordeNeeded = teamSize;
    }
    else
    {
        allianceNeeded = teamSize;
        hordeNeeded = teamSize - 1;     // -1 for human
    }
}

bool BGBotManager::QueueBot(Player* bot, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket)
{
    if (!bot || !IsBotAvailable(bot))
        return false;

    // Get the BG template to find the map ID
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bgTemplate || bgTemplate->MapIDs.empty())
    {
        TC_LOG_ERROR("module.playerbot.bg", "BGBotManager::QueueBot - No template for BG type {}", static_cast<uint32>(bgTypeId));
        return false;
    }

    // Get the bracket entry for this bot's level
    PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(
        bgTemplate->MapIDs.front(), bot->GetLevel());
    if (!bracketEntry)
    {
        TC_LOG_ERROR("module.playerbot.bg", "BGBotManager::QueueBot - No bracket entry for bot {} at level {}",
                     bot->GetName(), bot->GetLevel());
        return false;
    }

    // In 11.2, BGQueueTypeId takes 4 params: (battlemasterListId, type, rated, teamSize)
    // For regular BGs, teamSize is 0
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(
        static_cast<uint16>(bgTypeId),
        BattlegroundQueueIdType::Battleground,
        false,  // Not rated
        0       // TeamSize (0 for regular BG)
    );

    // AddGroup takes 7 params: (leader, group, team, bracketEntry, isPremade, ArenaRating, MatchmakerRating)
    GroupQueueInfo* ginfo = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId).AddGroup(
        bot,
        nullptr,        // No group
        bot->GetTeam(),
        bracketEntry,
        false,          // Not premade
        0,              // Arena rating
        0               // Matchmaker rating
    );

    if (ginfo)
    {
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::QueueBot - Bot {} queued for BG type {}",
                     bot->GetName(), static_cast<uint32>(bgTypeId));
        return true;
    }

    return false;
}

void BGBotManager::RemoveBotFromQueue(Player* bot)
{
    if (!bot)
        return;

    // Remove from all BG queues
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattlegroundQueueTypeId bgQueueTypeId = bot->GetBattlegroundQueueTypeId(i);
        if (bgQueueTypeId != BATTLEGROUND_QUEUE_NONE)
        {
            sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId).RemovePlayer(bot->GetGUID(), false);
        }
    }

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::RemoveBotFromQueue - Bot {} removed from queue",
                 bot->GetName());
}

std::vector<Player*> BGBotManager::FindAvailableBots(Team team, uint8 minLevel, uint8 maxLevel,
                                                       uint32 count) const
{
    std::vector<Player*> result;
    result.reserve(count);

    // Diagnostic counters
    uint32 totalBots = 0;
    uint32 wrongFaction = 0;
    uint32 wrongLevel = 0;
    uint32 unavailable = 0;
    uint32 alreadyQueued = 0;

    // Iterate through all online sessions and find bots
    for (auto const& [accountId, session] : sWorld->GetAllSessions())
    {
        if (!session)
            continue;

        Player* player = session->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        // Check if this is a bot
        if (!PlayerBotHooks::IsPlayerBot(player))
            continue;

        ++totalBots;

        // Check faction
        if (player->GetTeam() != team)
        {
            ++wrongFaction;
            continue;
        }

        // Check level
        uint8 level = player->GetLevel();
        if (level < minLevel || level > maxLevel)
        {
            ++wrongLevel;
            continue;
        }

        // Check availability
        if (!IsBotAvailable(player))
        {
            ++unavailable;
            continue;
        }

        // Check if already queued
        if (IsBotQueued(player->GetGUID()))
        {
            ++alreadyQueued;
            continue;
        }

        result.push_back(player);

        if (result.size() >= count)
            break;
    }

    // Log diagnostics if we didn't find enough bots
    if (result.size() < count)
    {
        TC_LOG_WARN("module.playerbot.bg",
            "BGBotManager::FindAvailableBots - Found only {}/{} bots for {} (level {}-{}). "
            "Stats: totalBots={}, wrongFaction={}, wrongLevel={}, unavailable={}, alreadyQueued={}",
            result.size(), count,
            team == ALLIANCE ? "Alliance" : "Horde",
            minLevel, maxLevel,
            totalBots, wrongFaction, wrongLevel, unavailable, alreadyQueued);
    }

    return result;
}

void BGBotManager::GetBracketLevelRange(BattlegroundBracketId bracket, uint8& minLevel, uint8& maxLevel) const
{
    // TrinityCore 11.2 uses level scaling, but we still need ranges for bot selection
    // These are approximate ranges based on bracket IDs
    switch (bracket)
    {
        case BG_BRACKET_ID_FIRST:
            minLevel = 10;
            maxLevel = 19;
            break;
        case BG_BRACKET_ID_LAST:
        default:
            // Max level bracket (11.2 = level 80 cap for The War Within Season 1)
            minLevel = 70;
            maxLevel = 80;
            break;
    }
}

void BGBotManager::RegisterBotAssignment(ObjectGuid humanGuid, ObjectGuid botGuid,
                                          BattlegroundTypeId bgTypeId, Team team)
{
    _queuedBots[botGuid] = BotQueueInfo(humanGuid, bgTypeId, team);

    auto& humanInfo = _humanPlayers[humanGuid];
    humanInfo.assignedBots.push_back(botGuid);

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::RegisterBotAssignment - Bot {} assigned to human {} for BG {}",
                 botGuid.ToString(), humanGuid.ToString(), static_cast<uint32>(bgTypeId));
}

void BGBotManager::UnregisterBotAssignment(ObjectGuid botGuid)
{
    auto itr = _queuedBots.find(botGuid);
    if (itr == _queuedBots.end())
        return;

    ObjectGuid humanGuid = itr->second.humanPlayerGuid;
    _queuedBots.erase(itr);

    auto humanItr = _humanPlayers.find(humanGuid);
    if (humanItr != _humanPlayers.end())
    {
        auto& bots = humanItr->second.assignedBots;
        bots.erase(std::remove(bots.begin(), bots.end(), botGuid), bots.end());

        if (bots.empty())
            _humanPlayers.erase(humanItr);
    }

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::UnregisterBotAssignment - Bot {} unregistered", botGuid.ToString());
}

void BGBotManager::UnregisterAllBotsForPlayer(ObjectGuid humanGuid)
{
    auto itr = _humanPlayers.find(humanGuid);
    if (itr == _humanPlayers.end())
        return;

    for (ObjectGuid botGuid : itr->second.assignedBots)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }
        _queuedBots.erase(botGuid);
    }

    _humanPlayers.erase(itr);

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::UnregisterAllBotsForPlayer - All bots unregistered for {}",
                 humanGuid.ToString());
}

bool BGBotManager::IsBotAvailable(Player* bot) const
{
    if (!bot || !bot->IsInWorld())
        return false;

    // Not available if in group
    if (bot->GetGroup())
        return false;

    // Not available if in BG
    if (bot->InBattleground())
        return false;

    // Not available if in arena
    if (bot->InArena())
        return false;

    // Not available if in LFG queue
    // Check via player battleground queue slots
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        if (bot->GetBattlegroundQueueTypeId(i) != BATTLEGROUND_QUEUE_NONE)
            return false;
    }

    // Not available if dead
    if (bot->isDead())
        return false;

    // Not available if deserter
    if (bot->HasAura(26013)) // Deserter aura ID
        return false;

    return true;
}

} // namespace Playerbot
