/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Battleground Bot Integration Script - Module-Only Approach
 *
 * This script integrates the BGBotManager with TrinityCore's Battleground system
 * using a polling approach that requires NO core file modifications.
 *
 * Design:
 * - Uses WorldScript::OnUpdate to periodically check BG queue state
 * - Detects human players who have joined BG queues
 * - Triggers bot recruitment via BGBotManager::OnPlayerJoinQueue
 * - Monitors invitations for automatic bot acceptance
 * - Tracks processed players to avoid duplicate bot additions
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "GameTime.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "DB2Stores.h"
#include "../Session/BotWorldSessionMgr.h"
#include "../Core/PlayerBotHooks.h"
#include "../PvP/BGBotManager.h"
#include "../Lifecycle/Instance/InstanceBotHooks.h"
#include "Log.h"
#include <unordered_set>
#include <unordered_map>

namespace Playerbot
{

/**
 * @brief Battleground Bot Integration using polling approach
 *
 * This WorldScript polls the BG system periodically to detect human players
 * who have joined the queue, then triggers bot recruitment to fill both teams.
 */
class PlayerbotBGScript : public WorldScript
{
public:
    PlayerbotBGScript() : WorldScript("PlayerbotBGScript") {}

    void OnUpdate(uint32 diff) override
    {
        // Throttle polling to every 1 second
        _updateAccumulator += diff;
        if (_updateAccumulator < BG_POLL_INTERVAL)
            return;
        _updateAccumulator = 0;

        // Skip if BG bot manager is not enabled
        if (!sBGBotManager || !sBGBotManager->IsEnabled())
            return;

        // Update BGBotManager
        sBGBotManager->Update(diff);

        // Poll for new queued players
        PollQueuedPlayers();

        // Cleanup stale tracking data
        CleanupStaleData();
    }

    void OnStartup() override
    {
        TC_LOG_INFO("module.playerbot.bg", "PlayerbotBGScript: Initializing BG bot integration...");

        if (sBGBotManager)
        {
            sBGBotManager->Initialize();
            TC_LOG_INFO("module.playerbot.bg", "PlayerbotBGScript: BGBotManager initialized");
        }
        else
        {
            TC_LOG_ERROR("module.playerbot.bg", "PlayerbotBGScript: BGBotManager not available!");
        }
    }

    void OnShutdownInitiate(ShutdownExitCode /*code*/, ShutdownMask /*mask*/) override
    {
        TC_LOG_INFO("module.playerbot.bg", "PlayerbotBGScript: Shutting down BG bot integration...");

        if (sBGBotManager)
        {
            sBGBotManager->Shutdown();
        }

        _processedPlayers.clear();
        _lastQueueState.clear();
    }

private:
    /**
     * @brief Poll all online players to detect new BG queue joins
     */
    void PollQueuedPlayers()
    {
        SessionMap const& sessions = sWorld->GetAllSessions();

        for (auto const& pair : sessions)
        {
            WorldSession* session = pair.second;
            if (!session)
                continue;

            Player* player = session->GetPlayer();
            if (!player || !player->IsInWorld())
                continue;

            // Skip bots - only process human players
            if (IsPlayerBotCheck(player))
                continue;

            ObjectGuid playerGuid = player->GetGUID();

            // Check if player is in BG queue
            bool inQueue = false;
            BattlegroundQueueTypeId currentQueueType = BATTLEGROUND_QUEUE_NONE;
            BattlegroundTypeId bgTypeId = BATTLEGROUND_TYPE_NONE;

            for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
            {
                BattlegroundQueueTypeId queueTypeId = player->GetBattlegroundQueueTypeId(i);
                if (queueTypeId != BATTLEGROUND_QUEUE_NONE)
                {
                    // Check if this is a regular BG (not arena)
                    // In TrinityCore 11.2, BattlemasterListId maps to BattlegroundTypeId
                    BattlegroundTypeId type = BattlegroundTypeId(queueTypeId.BattlemasterListId);
                    if (type != BATTLEGROUND_AA) // Not arena
                    {
                        inQueue = true;
                        currentQueueType = queueTypeId;
                        bgTypeId = type;
                        break;
                    }
                }
            }

            // Track state changes
            auto lastStateIt = _lastQueueState.find(playerGuid);
            bool wasInQueue = (lastStateIt != _lastQueueState.end()) ? lastStateIt->second : false;

            _lastQueueState[playerGuid] = inQueue;

            // Handle state transitions
            if (inQueue && !wasInQueue)
            {
                // Player just joined the queue
                HandlePlayerJoinedQueue(player, bgTypeId, currentQueueType);
            }
            else if (!inQueue && wasInQueue)
            {
                // Player left the queue
                HandlePlayerLeftQueue(player);
            }
        }
    }

    /**
     * @brief Handle a player joining the BG queue
     */
    void HandlePlayerJoinedQueue(Player* player, BattlegroundTypeId bgTypeId, BattlegroundQueueTypeId queueTypeId)
    {
        ObjectGuid playerGuid = player->GetGUID();

        // Check if already processed
        if (_processedPlayers.count(playerGuid))
        {
            TC_LOG_DEBUG("module.playerbot.bg",
                "PlayerbotBGScript: Player {} already processed, skipping", player->GetName());
            return;
        }

        TC_LOG_INFO("module.playerbot.bg",
            "PlayerbotBGScript: Detected player {} (level {}) joined BG queue (Type: {})",
            player->GetName(), player->GetLevel(), static_cast<uint32>(bgTypeId));

        // Get bracket from player level using BG template map
        BattlegroundBracketId bracket = BG_BRACKET_ID_LAST;  // Default fallback

        // Get the BG template to find the map ID for bracket lookup
        BattlegroundTemplate const* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
        if (bgTemplate && !bgTemplate->MapIDs.empty())
        {
            // Use DB2Manager to get the correct bracket for player's level
            PVPDifficultyEntry const* bracketEntry = DB2Manager::GetBattlegroundBracketByLevel(
                bgTemplate->MapIDs.front(), player->GetLevel());

            if (bracketEntry)
            {
                bracket = BattlegroundBracketId(bracketEntry->RangeIndex);
                TC_LOG_INFO("module.playerbot.bg",
                    "PlayerbotBGScript: Player {} level {} -> Bracket {} (range {}-{})",
                    player->GetName(), player->GetLevel(), static_cast<uint32>(bracket),
                    bracketEntry->MinLevel, bracketEntry->MaxLevel);
            }
            else
            {
                TC_LOG_WARN("module.playerbot.bg",
                    "PlayerbotBGScript: No bracket found for level {}, using max bracket",
                    player->GetLevel());
            }
        }
        else
        {
            TC_LOG_WARN("module.playerbot.bg",
                "PlayerbotBGScript: No BG template for type {}, using max bracket",
                static_cast<uint32>(bgTypeId));
        }

        // HYBRID APPROACH: Use both new and old systems
        //
        // Step 1: Trigger the new Instance Bot System to create/reserve bots
        // This will create bots if needed and add them to the BG queue
        if (InstanceBotHooks::IsEnabled())
        {
            TC_LOG_INFO("module.playerbot.bg",
                "PlayerbotBGScript: Triggering Instance Bot System for player {} (BG Type: {}, Bracket: {})",
                player->GetName(), static_cast<uint32>(bgTypeId), static_cast<uint32>(bracket));

            InstanceBotHooks::OnPlayerJoinBattleground(
                player,
                static_cast<uint32>(bgTypeId),
                static_cast<uint32>(bracket),
                player->GetGroup() != nullptr
            );
        }

        // Step 2: Use BGBotManager to add CURRENTLY ONLINE bots to queue
        // This provides immediate bot coverage while Instance Bot System creates new bots
        sBGBotManager->OnPlayerJoinQueue(player, bgTypeId, bracket, player->GetGroup() != nullptr);

        // Mark as processed
        _processedPlayers.insert(playerGuid);
        _processedPlayerTimes[playerGuid] = GameTime::GetGameTimeMS();

        TC_LOG_INFO("module.playerbot.bg",
            "PlayerbotBGScript: Triggered bot recruitment for player {} (BG Type: {}, Bracket: {})",
            player->GetName(), static_cast<uint32>(bgTypeId), static_cast<uint32>(bracket));
    }

    /**
     * @brief Handle a player leaving the BG queue
     */
    void HandlePlayerLeftQueue(Player* player, BattlegroundTypeId bgTypeId = BATTLEGROUND_TYPE_NONE)
    {
        ObjectGuid playerGuid = player->GetGUID();

        TC_LOG_INFO("module.playerbot.bg",
            "PlayerbotBGScript: Player {} left BG queue", player->GetName());

        // Notify Instance Bot Hooks (new system)
        if (InstanceBotHooks::IsEnabled())
        {
            InstanceBotHooks::OnPlayerLeaveBattlegroundQueue(player, static_cast<uint32>(bgTypeId));
        }

        // Notify BGBotManager (old system)
        sBGBotManager->OnPlayerLeaveQueue(playerGuid);

        // Remove from processed set
        _processedPlayers.erase(playerGuid);
        _processedPlayerTimes.erase(playerGuid);
    }

    /**
     * @brief Check if a player is a bot
     */
    bool IsPlayerBotCheck(Player* player) const
    {
        if (!player)
            return false;

        return PlayerBotHooks::IsPlayerBot(player);
    }

    /**
     * @brief Cleanup stale tracking data
     */
    void CleanupStaleData()
    {
        uint32 now = GameTime::GetGameTimeMS();

        if (now - _lastCleanupTime < CLEANUP_INTERVAL)
            return;
        _lastCleanupTime = now;

        constexpr uint32 STALE_THRESHOLD = 30 * MINUTE * IN_MILLISECONDS;

        std::vector<ObjectGuid> toRemove;
        for (auto const& pair : _processedPlayerTimes)
        {
            if (now - pair.second > STALE_THRESHOLD)
            {
                toRemove.push_back(pair.first);
            }
        }

        for (ObjectGuid const& guid : toRemove)
        {
            _processedPlayers.erase(guid);
            _processedPlayerTimes.erase(guid);
            _lastQueueState.erase(guid);
        }

        if (!toRemove.empty())
        {
            TC_LOG_DEBUG("module.playerbot.bg",
                "PlayerbotBGScript: Cleaned up {} stale player entries", toRemove.size());
        }
    }

    // Configuration
    // RELIABILITY FIX: Reduced from 1000ms to 250ms for faster BG queue detection
    // The previous 1000ms delay meant up to 1 second before detecting player queue join,
    // which combined with JIT bot creation time caused the first BG start to fail.
    static constexpr uint32 BG_POLL_INTERVAL = 250; // 250ms for responsive detection
    static constexpr uint32 CLEANUP_INTERVAL = 5 * MINUTE * IN_MILLISECONDS;

    // State tracking
    uint32 _updateAccumulator = 0;
    uint32 _lastCleanupTime = 0;

    // Processed players
    std::unordered_set<ObjectGuid> _processedPlayers;
    std::unordered_map<ObjectGuid, uint32> _processedPlayerTimes;

    // Last known queue state
    std::unordered_map<ObjectGuid, bool> _lastQueueState;
};

void AddSC_PlayerbotBGScript()
{
    new PlayerbotBGScript();
}

} // namespace Playerbot
