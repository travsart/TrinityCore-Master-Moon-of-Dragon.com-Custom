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
#include "../Core/Diagnostics/BotOperationTracker.h"
#include "../Session/BotWorldSessionMgr.h"
#include "../AI/Coordination/Battleground/BattlegroundCoordinatorManager.h"
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
#include "../Lifecycle/Instance/QueueStatePoller.h"

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

BGBotManager::BGBotManager()
    : _enabled(false)
    , _updateAccumulator(0)
    , _invitationCheckAccumulator(0)
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
    _invitationCheckAccumulator = 0;

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
    _invitationCheckAccumulator += diff;

    // Periodic cleanup (every 5 minutes)
    if (_updateAccumulator >= CLEANUP_INTERVAL)
    {
        CleanupStaleAssignments();
        _updateAccumulator = 0;
    }

    // Frequent invitation check (every 1 second)
    // This is necessary because the core BG system doesn't notify us when bots are invited
    if (_invitationCheckAccumulator >= INVITATION_CHECK_INTERVAL)
    {
        ProcessPendingInvitations();
        _invitationCheckAccumulator = 0;
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

    // Trigger immediate poll to detect any remaining shortages
    // This allows the JIT system to create additional bots if needed
    sQueueStatePoller->PollBGQueues();
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
    BattlegroundTypeId bgTypeId = bg->GetTypeID();

    TC_LOG_INFO("module.playerbot.bg", "BGBotManager::OnBattlegroundStart - BG instance {} ({}) started",
                 bgInstanceGuid, bg->GetName());

    // =========================================================================
    // 1. Initialize the BattlegroundCoordinator for this BG
    // =========================================================================
    sBGCoordinatorMgr->OnBattlegroundStart(bg);

    // =========================================================================
    // 2. Check team population and fill empty slots
    // =========================================================================
    uint32 targetTeamSize = GetBGTeamSize(bgTypeId);

    // Count players per team
    uint32 allianceCount = 0;
    uint32 hordeCount = 0;

    for (auto const& itr : bg->GetPlayers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(itr.first))
        {
            if (player->GetBGTeam() == ALLIANCE)
                ++allianceCount;
            else
                ++hordeCount;
        }
    }

    TC_LOG_INFO("module.playerbot.bg",
        "BGBotManager::OnBattlegroundStart - Current population: Alliance {}/{}, Horde {}/{}",
        allianceCount, targetTeamSize, hordeCount, targetTeamSize);

    // Calculate missing slots
    uint32 allianceNeeded = (allianceCount < targetTeamSize) ? (targetTeamSize - allianceCount) : 0;
    uint32 hordeNeeded = (hordeCount < targetTeamSize) ? (targetTeamSize - hordeCount) : 0;

    if (allianceNeeded == 0 && hordeNeeded == 0)
    {
        TC_LOG_DEBUG("module.playerbot.bg",
            "BGBotManager::OnBattlegroundStart - Teams are full, no bots needed");
        return;
    }

    TC_LOG_INFO("module.playerbot.bg",
        "BGBotManager::OnBattlegroundStart - Need to fill {} Alliance, {} Horde slots",
        allianceNeeded, hordeNeeded);

    // Get level range for this BG
    BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    uint8 minLevel = 10, maxLevel = 80;

    if (bgTemplate && !bgTemplate->MapIDs.empty())
    {
        // Use the BG's actual level range
        PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(
            bgTemplate->MapIDs.front(), 80); // Use max level to get the bracket
        if (bracketEntry)
        {
            minLevel = bracketEntry->MinLevel;
            maxLevel = bracketEntry->MaxLevel;
        }
    }

    uint32 botsAdded = 0;

    // Queue additional Alliance bots
    if (allianceNeeded > 0)
    {
        std::vector<Player*> allianceBots = FindAvailableBots(ALLIANCE, minLevel, maxLevel, allianceNeeded);
        for (Player* bot : allianceBots)
        {
            if (AddBotDirectlyToBG(bot, bg, ALLIANCE))
            {
                ++botsAdded;
                TC_LOG_DEBUG("module.playerbot.bg",
                    "BGBotManager::OnBattlegroundStart - Added Alliance bot {} to BG",
                    bot->GetName());
            }
        }
    }

    // Queue additional Horde bots
    if (hordeNeeded > 0)
    {
        std::vector<Player*> hordeBots = FindAvailableBots(HORDE, minLevel, maxLevel, hordeNeeded);
        for (Player* bot : hordeBots)
        {
            if (AddBotDirectlyToBG(bot, bg, HORDE))
            {
                ++botsAdded;
                TC_LOG_DEBUG("module.playerbot.bg",
                    "BGBotManager::OnBattlegroundStart - Added Horde bot {} to BG",
                    bot->GetName());
            }
        }
    }

    if (botsAdded > 0)
    {
        TC_LOG_INFO("module.playerbot.bg",
            "BGBotManager::OnBattlegroundStart - Added {} bots to fill empty slots",
            botsAdded);

        // Notify coordinator that new bots were added
        sBGCoordinatorMgr->OnBattlegroundStart(bg);
    }
}

void BGBotManager::OnBattlegroundEnd(Battleground* bg, Team winnerTeam)
{
    if (!_enabled || !_initialized || !bg)
        return;

    std::lock_guard lock(_mutex);

    uint32 bgInstanceGuid = bg->GetInstanceID();

    TC_LOG_INFO("module.playerbot.bg", "BGBotManager::OnBattlegroundEnd - BG instance {} ended, winner: {}",
                 bgInstanceGuid, winnerTeam == ALLIANCE ? "Alliance" : "Horde");

    // Notify coordinator that BG is ending
    sBGCoordinatorMgr->OnBattlegroundEnd(bg);

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
    // TrinityCore 12.0 uses lower minimums for testing/single-player
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

bool BGBotManager::QueueBotForBG(Player* bot, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket)
{
    // Public wrapper for JIT bot integration
    // The bot must already be logged in and in world before calling this
    if (!bot || !bot->IsInWorld())
    {
        TC_LOG_ERROR("module.playerbot.bg", "QueueBotForBG: Bot is null or not in world");
        return false;
    }

    if (!_enabled || !_initialized)
    {
        TC_LOG_WARN("module.playerbot.bg", "QueueBotForBG: BGBotManager not enabled/initialized");
        return false;
    }

    return QueueBot(bot, bgTypeId, bracket);
}

bool BGBotManager::QueueBotForBGWithTracking(Player* bot, BattlegroundTypeId bgTypeId,
                                              BattlegroundBracketId bracket, ObjectGuid humanPlayerGuid)
{
    // Extended version that registers bot in _queuedBots for proper invitation handling
    if (!bot || !bot->IsInWorld())
    {
        TC_LOG_ERROR("module.playerbot.bg", "QueueBotForBGWithTracking: Bot is null or not in world");
        BOT_TRACK_BG_ERROR(
            BGQueueErrorCode::BOT_UNAVAILABLE,
            "Bot is null or not in world for BG queue",
            ObjectGuid::Empty,
            humanPlayerGuid,
            static_cast<uint32>(bgTypeId));
        return false;
    }

    if (!_enabled || !_initialized)
    {
        TC_LOG_WARN("module.playerbot.bg", "QueueBotForBGWithTracking: BGBotManager not enabled/initialized");
        BOT_TRACK_BG_ERROR(
            BGQueueErrorCode::BOT_UNAVAILABLE,
            "BGBotManager not enabled/initialized",
            bot->GetGUID(),
            humanPlayerGuid,
            static_cast<uint32>(bgTypeId));
        return false;
    }

    // Queue the bot in the BG queue
    if (!QueueBot(bot, bgTypeId, bracket))
    {
        TC_LOG_WARN("module.playerbot.bg", "QueueBotForBGWithTracking: Failed to queue bot {} for BG {}",
            bot->GetName(), static_cast<uint32>(bgTypeId));
        // Error already tracked in QueueBot
        return false;
    }

    // CRITICAL: Register the bot in _queuedBots so OnInvitationReceived will process it
    std::lock_guard lock(_mutex);
    RegisterBotAssignment(humanPlayerGuid, bot->GetGUID(), bgTypeId, bot->GetTeam());

    TC_LOG_INFO("module.playerbot.bg",
        "QueueBotForBGWithTracking: Bot {} queued and registered for BG {} (tracking human {})",
        bot->GetName(), static_cast<uint32>(bgTypeId), humanPlayerGuid.ToString());

    // Track success
    BOT_TRACK_SUCCESS(BotOperationCategory::BG_QUEUE, "BGBotManager::QueueBotForBGWithTracking", bot->GetGUID());

    return true;
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
    if (!bgTemplate)
    {
        TC_LOG_ERROR("module.playerbot.bg", "BGBotManager::QueueBot - No template for BG type {}", static_cast<uint32>(bgTypeId));
        BOT_TRACK_BG_ERROR(
            BGQueueErrorCode::BG_TEMPLATE_NOT_FOUND,
            fmt::format("No BG template for type {}", static_cast<uint32>(bgTypeId)),
            bot ? bot->GetGUID() : ObjectGuid::Empty,
            ObjectGuid::Empty,
            static_cast<uint32>(bgTypeId));
        return false;
    }

    // CRITICAL FIX: For Random BG and other meta-queues, MapIDs might be empty
    // We need to get bracket from PvpDifficulty directly if no map is available
    PVPDifficultyEntry const* bracketEntry = nullptr;

    if (!bgTemplate->MapIDs.empty())
    {
        // Normal BG with dedicated map
        bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(
            bgTemplate->MapIDs.front(), bot->GetLevel());
    }
    else
    {
        // Random BG or meta-queue - use any available BG's bracket for this level
        // Get bracket from WSG (map 489) as a fallback since it covers all levels
        bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(489, bot->GetLevel());
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::QueueBot - Using fallback bracket for meta-queue BG type {} (bot level {})",
                     static_cast<uint32>(bgTypeId), bot->GetLevel());
    }

    if (!bracketEntry)
    {
        TC_LOG_ERROR("module.playerbot.bg", "BGBotManager::QueueBot - No bracket entry for bot {} at level {}",
                     bot->GetName(), bot->GetLevel());
        BOT_TRACK_BG_ERROR(
            BGQueueErrorCode::BRACKET_NOT_FOUND,
            fmt::format("No PVP bracket for bot {} at level {}", bot->GetName(), bot->GetLevel()),
            bot->GetGUID(),
            ObjectGuid::Empty,
            static_cast<uint32>(bgTypeId));
        return false;
    }

    // In 12.0, BGQueueTypeId takes 4 params: (battlemasterListId, type, rated, teamSize)
    // For regular BGs, teamSize is 0
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(
        static_cast<uint16>(bgTypeId),
        BattlegroundQueueIdType::Battleground,
        false,  // Not rated
        0       // TeamSize (0 for regular BG)
    );

    // CRITICAL FIX: Check if bot is already in this queue (prevents duplicate adds)
    if (bot->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
    {
        TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::QueueBot - Bot {} already in queue for BG type {}",
                     bot->GetName(), static_cast<uint32>(bgTypeId));
        return true;  // Already queued, consider it success
    }

    // Check if bot has free queue slots
    if (!bot->HasFreeBattlegroundQueueId())
    {
        TC_LOG_WARN("module.playerbot.bg", "BGBotManager::QueueBot - Bot {} has no free BG queue slots",
                    bot->GetName());
        BOT_TRACK_BG_ERROR(
            BGQueueErrorCode::BOT_QUEUE_FULL,
            fmt::format("Bot {} has no free BG queue slots", bot->GetName()),
            bot->GetGUID(),
            ObjectGuid::Empty,
            static_cast<uint32>(bgTypeId));
        return false;
    }

    // Get the BG queue
    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

    // AddGroup takes 7 params: (leader, group, team, bracketEntry, isPremade, ArenaRating, MatchmakerRating)
    GroupQueueInfo* ginfo = bgQueue.AddGroup(
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
        // CRITICAL FIX: Update bot's player state to know it's in the queue
        // This is required for the BG system to properly track the bot
        uint32 queueSlot = bot->AddBattlegroundQueueId(bgQueueTypeId);

        TC_LOG_INFO("module.playerbot.bg", "BGBotManager::QueueBot - Bot {} queued for BG type {} in slot {} (bracket {})",
                     bot->GetName(), static_cast<uint32>(bgTypeId), queueSlot, static_cast<uint32>(bracketEntry->GetBracketId()));

        // CRITICAL FIX: Schedule queue update to trigger match-making
        // Without this, the queue won't be processed to start the BG
        sBattlegroundMgr->ScheduleQueueUpdate(0, bgQueueTypeId, bracketEntry->GetBracketId());

        return true;
    }

    TC_LOG_ERROR("module.playerbot.bg", "BGBotManager::QueueBot - AddGroup failed for bot {} (BG type {})",
                 bot->GetName(), static_cast<uint32>(bgTypeId));
    BOT_TRACK_BG_ERROR(
        BGQueueErrorCode::ADD_GROUP_FAILED,
        fmt::format("BG queue AddGroup failed for bot {} (BG type {})", bot->GetName(), static_cast<uint32>(bgTypeId)),
        bot->GetGUID(),
        ObjectGuid::Empty,
        static_cast<uint32>(bgTypeId));
    return false;
}

void BGBotManager::RemoveBotFromQueue(Player* bot)
{
    if (!bot)
        return;

    // Remove from all BG queues - iterate backwards since we're modifying the array
    for (int8 i = PLAYER_MAX_BATTLEGROUND_QUEUES - 1; i >= 0; --i)
    {
        BattlegroundQueueTypeId bgQueueTypeId = bot->GetBattlegroundQueueTypeId(i);
        if (bgQueueTypeId != BATTLEGROUND_QUEUE_NONE)
        {
            TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::RemoveBotFromQueue - Removing bot {} from queue slot {} (BG type {})",
                         bot->GetName(), i, bgQueueTypeId.BattlemasterListId);

            // Remove from the queue system
            sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId).RemovePlayer(bot->GetGUID(), false);

            // Clear the bot's queue slot
            bot->RemoveBattlegroundQueueId(bgQueueTypeId);
        }
    }

    TC_LOG_DEBUG("module.playerbot.bg", "BGBotManager::RemoveBotFromQueue - Bot {} removed from all BG queues",
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

    // CRITICAL FIX: Use BotWorldSessionMgr to get bots, not sWorld->GetAllSessions()
    // Bot sessions are stored in BotWorldSessionMgr::_botSessions, NOT in World's session map
    std::vector<Player*> allBots = sBotWorldSessionMgr->GetAllBotPlayers();

    TC_LOG_DEBUG("module.playerbot.bg",
        "BGBotManager::FindAvailableBots - Got {} bots from BotWorldSessionMgr", allBots.size());

    for (Player* player : allBots)
    {
        if (!player || !player->IsInWorld())
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
    // TrinityCore 12.0 uses level scaling, but we still need ranges for bot selection
    // These are approximate ranges based on bracket IDs
    switch (bracket)
    {
        case BG_BRACKET_ID_FIRST:
            minLevel = 10;
            maxLevel = 19;
            break;
        case BG_BRACKET_ID_LAST:
        default:
            // Max level bracket (12.0 = level 80 cap for The War Within Season 1)
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

// ============================================================================
// INVITATION PROCESSING
// ============================================================================

void BGBotManager::ProcessPendingInvitations()
{
    std::lock_guard lock(_mutex);

    if (_queuedBots.empty())
        return;

    // Collect bots that need to accept invitations
    // We collect first to avoid modifying _queuedBots while iterating
    std::vector<std::pair<ObjectGuid, BotQueueInfo>> botsToAccept;

    for (auto const& [botGuid, queueInfo] : _queuedBots)
    {
        // Skip bots that already have a BG instance assigned (already accepted)
        if (queueInfo.bgInstanceGuid != 0)
            continue;

        Player* bot = ObjectAccessor::FindPlayer(botGuid);
        if (!bot || !bot->IsInWorld())
            continue;

        // Check all BG queue slots for pending invitations
        for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            BattlegroundQueueTypeId bgQueueTypeId = bot->GetBattlegroundQueueTypeId(i);
            if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
                continue;

            // Check if bot is invited for this queue type
            if (bot->IsInvitedForBattlegroundQueueType(bgQueueTypeId))
            {
                botsToAccept.push_back({botGuid, queueInfo});
                break;  // Only need to find one invitation
            }
        }
    }

    // Now process the bots that need to accept invitations
    for (auto const& [botGuid, queueInfo] : botsToAccept)
    {
        Player* bot = ObjectAccessor::FindPlayer(botGuid);
        if (!bot || !bot->IsInWorld())
            continue;

        // Find the queue slot with the invitation
        for (uint8 queueSlot = 0; queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++queueSlot)
        {
            BattlegroundQueueTypeId bgQueueTypeId = bot->GetBattlegroundQueueTypeId(queueSlot);
            if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
                continue;

            if (!bot->IsInvitedForBattlegroundQueueType(bgQueueTypeId))
                continue;

            // Get queue info to find the BG instance
            BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

            GroupQueueInfo ginfo;
            if (!bgQueue.GetPlayerGroupInfoData(bot->GetGUID(), &ginfo))
            {
                TC_LOG_WARN("module.playerbot.bg",
                    "ProcessPendingInvitations - Bot {} has invitation but no GroupQueueInfo",
                    bot->GetName());
                continue;
            }

            if (!ginfo.IsInvitedToBGInstanceGUID)
            {
                TC_LOG_WARN("module.playerbot.bg",
                    "ProcessPendingInvitations - Bot {} IsInvited=true but IsInvitedToBGInstanceGUID=0",
                    bot->GetName());
                continue;
            }

            BattlegroundTypeId bgTypeId = BattlegroundTypeId(bgQueueTypeId.BattlemasterListId);
            Battleground* bg = sBattlegroundMgr->GetBattleground(
                ginfo.IsInvitedToBGInstanceGUID,
                bgTypeId == BATTLEGROUND_AA ? BATTLEGROUND_TYPE_NONE : bgTypeId);

            if (!bg)
            {
                TC_LOG_WARN("module.playerbot.bg",
                    "ProcessPendingInvitations - Bot {} invited to BG instance {} but BG not found",
                    bot->GetName(), ginfo.IsInvitedToBGInstanceGUID);
                continue;
            }

            TC_LOG_INFO("module.playerbot.bg",
                "ProcessPendingInvitations - Bot {} auto-accepting BG {} (instance {}, type {})",
                bot->GetName(), bg->GetName(), bg->GetInstanceID(), static_cast<uint32>(bgTypeId));

            // =================================================================
            // AUTO-ACCEPT LOGIC (mirrors HandleBattleFieldPortOpcode)
            // =================================================================

            // Check for Freeze debuff
            if (bot->HasAura(9454))
            {
                TC_LOG_DEBUG("module.playerbot.bg",
                    "ProcessPendingInvitations - Bot {} has Freeze aura, skipping",
                    bot->GetName());
                continue;
            }

            // Set battleground entry point (for return after BG ends)
            if (!bot->InBattleground())
                bot->SetBattlegroundEntryPoint();

            // Resurrect if dead
            if (!bot->IsAlive())
            {
                bot->ResurrectPlayer(1.0f);
                bot->SpawnCorpseBones();
            }

            // Stop taxi flight
            bot->FinishTaxiFlight();

            // Remove from queue
            bgQueue.RemovePlayer(bot->GetGUID(), false);

            // If bot was in another BG, remove from it
            if (Battleground* currentBg = bot->GetBattleground())
                currentBg->RemovePlayerAtLeave(bot->GetGUID(), false, true);

            // Set destination BG
            bot->SetBattlegroundId(bg->GetInstanceID(), bg->GetTypeID(), bgQueueTypeId);
            bot->SetBGTeam(ginfo.Team);

            // Update our tracking
            auto itr = _queuedBots.find(botGuid);
            if (itr != _queuedBots.end())
            {
                itr->second.bgInstanceGuid = bg->GetInstanceID();
            }
            _bgInstanceBots[bg->GetInstanceID()].insert(botGuid);

            // Teleport to battleground
            BattlegroundMgr::SendToBattleground(bot, bg);

            TC_LOG_INFO("module.playerbot.bg",
                "ProcessPendingInvitations - Bot {} teleporting to BG {} (Team: {})",
                bot->GetName(), bg->GetName(),
                ginfo.Team == ALLIANCE ? "Alliance" : "Horde");

            break;  // Only process one queue slot per bot per update
        }
    }
}

bool BGBotManager::AddBotDirectlyToBG(Player* bot, Battleground* bg, Team team)
{
    if (!bot || !bg || !bot->IsInWorld())
        return false;

    // Check if bot is already in a BG
    if (bot->InBattleground())
        return false;

    // Check for deserter
    if (bot->HasAura(26013))
        return false;

    TC_LOG_DEBUG("module.playerbot.bg",
        "BGBotManager::AddBotDirectlyToBG - Adding bot {} to BG {} as {}",
        bot->GetName(), bg->GetName(),
        team == ALLIANCE ? "Alliance" : "Horde");

    // Set battleground entry point (for return after BG ends)
    bot->SetBattlegroundEntryPoint();

    // Resurrect if dead
    if (!bot->IsAlive())
    {
        bot->ResurrectPlayer(1.0f);
        bot->SpawnCorpseBones();
    }

    // Stop taxi flight
    bot->FinishTaxiFlight();

    // Remove from any existing BG queues
    for (int8 i = PLAYER_MAX_BATTLEGROUND_QUEUES - 1; i >= 0; --i)
    {
        BattlegroundQueueTypeId bgQueueTypeId = bot->GetBattlegroundQueueTypeId(i);
        if (bgQueueTypeId != BATTLEGROUND_QUEUE_NONE)
        {
            sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId).RemovePlayer(bot->GetGUID(), false);
            bot->RemoveBattlegroundQueueId(bgQueueTypeId);
        }
    }

    // Create queue type for this BG
    BattlegroundQueueTypeId queueTypeId = BattlegroundMgr::BGQueueTypeId(
        static_cast<uint16>(bg->GetTypeID()),
        BattlegroundQueueIdType::Battleground,
        false,  // Not rated
        0       // TeamSize
    );

    // Set destination BG data (required for teleport and AddPlayer)
    // Note: SetBattlegroundId() sets m_bgData.bgInstanceID, bgTypeID, AND queueId
    bot->SetBattlegroundId(bg->GetInstanceID(), bg->GetTypeID(), queueTypeId);
    bot->SetBGTeam(team);

    // Track the bot
    _bgInstanceBots[bg->GetInstanceID()].insert(bot->GetGUID());

    // Teleport bot to the battleground
    // IMPORTANT: Do NOT call bg->AddPlayer() here! It will be called automatically
    // in HandleMoveWorldPortAck when the teleport completes. Calling it before
    // teleport causes a crash in Map::SendObjectUpdates because the player is
    // removed from the current map while still in the update queue.
    BattlegroundMgr::SendToBattleground(bot, bg);

    TC_LOG_INFO("module.playerbot.bg",
        "BGBotManager::AddBotDirectlyToBG - Bot {} teleporting to BG {} (Team: {})",
        bot->GetName(), bg->GetName(),
        team == ALLIANCE ? "Alliance" : "Horde");

    return true;
}

} // namespace Playerbot
