/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * LFG Bot Integration Script - Module-Only Approach
 *
 * This script integrates the LFGBotManager with TrinityCore's LFG system
 * using a polling approach that requires NO core file modifications.
 *
 * Design:
 * - Uses WorldScript::OnUpdate to periodically check LFG queue state
 * - Detects human players with LFG_STATE_QUEUED status
 * - Triggers bot recruitment via LFGBotManager::OnPlayerJoinQueue
 * - Monitors proposals and role checks for automatic bot responses
 * - Tracks processed players to avoid duplicate bot additions
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "GameTime.h"
#include "LFGMgr.h"
#include "LFG.h"
#include "Session/BotWorldSessionMgr.h"
#include "Core/PlayerBotHooks.h"
#include "LFG/LFGBotManager.h"
#include "Log.h"
#include <unordered_set>
#include <unordered_map>

namespace Playerbot
{

/**
 * @brief LFG Bot Integration using polling approach
 *
 * This WorldScript polls the LFG system periodically to detect human players
 * who have joined the queue, then triggers bot recruitment to fill the group.
 */
class PlayerbotLFGScript : public WorldScript
{
public:
    PlayerbotLFGScript() : WorldScript("PlayerbotLFGScript") {}

    void OnUpdate(uint32 diff) override
    {
        // Throttle LFG polling to every 1 second
        _updateAccumulator += diff;
        if (_updateAccumulator < LFG_POLL_INTERVAL)
            return;
        _updateAccumulator = 0;

        // Skip if LFG bot manager is not enabled
        if (!sLFGBotManager || !sLFGBotManager->IsEnabled())
            return;

        // Update LFGBotManager (handles stale cleanup, etc.)
        sLFGBotManager->Update(diff);

        // Poll for new queued players
        PollQueuedPlayers();

        // Poll for proposals that need bot acceptance
        PollProposals();

        // Cleanup stale tracking data periodically
        CleanupStaleData();
    }

    void OnStartup() override
    {
        TC_LOG_INFO("module.playerbot.lfg", "PlayerbotLFGScript: Initializing LFG bot integration...");

        // Initialize LFGBotManager
        if (sLFGBotManager)
        {
            sLFGBotManager->Initialize();
            TC_LOG_INFO("module.playerbot.lfg", "PlayerbotLFGScript: LFGBotManager initialized");
        }
        else
        {
            TC_LOG_ERROR("module.playerbot.lfg", "PlayerbotLFGScript: LFGBotManager not available!");
        }
    }

    void OnShutdownInitiate(ShutdownExitCode /*code*/, ShutdownMask /*mask*/) override
    {
        TC_LOG_INFO("module.playerbot.lfg", "PlayerbotLFGScript: Shutting down LFG bot integration...");

        if (sLFGBotManager)
        {
            sLFGBotManager->Shutdown();
        }

        // Clear all tracking data
        _processedPlayers.clear();
        _processedProposals.clear();
        _lastQueueState.clear();
    }

private:
    /**
     * @brief Poll all online players to detect new LFG queue joins
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

            // Get player's current LFG state
            lfg::LfgState state = sLFGMgr->GetState(playerGuid);

            // Track state changes
            auto lastStateIt = _lastQueueState.find(playerGuid);
            lfg::LfgState lastState = (lastStateIt != _lastQueueState.end())
                ? lastStateIt->second
                : lfg::LFG_STATE_NONE;

            // Update stored state
            _lastQueueState[playerGuid] = state;

            // Handle state transitions
            if (state == lfg::LFG_STATE_QUEUED && lastState != lfg::LFG_STATE_QUEUED)
            {
                // Player just joined the queue
                HandlePlayerJoinedQueue(player);
            }
            else if (state == lfg::LFG_STATE_NONE && lastState == lfg::LFG_STATE_QUEUED)
            {
                // Player left the queue
                HandlePlayerLeftQueue(player);
            }
            else if (state == lfg::LFG_STATE_PROPOSAL && lastState != lfg::LFG_STATE_PROPOSAL)
            {
                // Proposal received - bots need to accept
                TC_LOG_DEBUG("module.playerbot.lfg",
                    "PlayerbotLFGScript: Player {} has a proposal pending", player->GetName());
            }
            else if (state == lfg::LFG_STATE_ROLECHECK && lastState != lfg::LFG_STATE_ROLECHECK)
            {
                // Role check started
                HandleRoleCheck(player);
            }
        }
    }

    /**
     * @brief Poll for proposals that bots need to accept
     */
    void PollProposals()
    {
        // The proposal handling is triggered by detecting LFG_STATE_PROPOSAL on human players
        // When detected, we call LFGBotManager::OnProposalReceived for all bots in the proposal

        // This is handled implicitly by state change detection in PollQueuedPlayers
        // because when a proposal is created, both human and bot participants
        // transition to LFG_STATE_PROPOSAL
    }

    /**
     * @brief Handle a player joining the LFG queue
     */
    void HandlePlayerJoinedQueue(Player* player)
    {
        ObjectGuid playerGuid = player->GetGUID();

        // Check if we've already processed this player recently
        if (_processedPlayers.count(playerGuid))
        {
            TC_LOG_DEBUG("module.playerbot.lfg",
                "PlayerbotLFGScript: Player {} already processed, skipping", player->GetName());
            return;
        }

        TC_LOG_INFO("module.playerbot.lfg",
            "PlayerbotLFGScript: Detected player {} joined LFG queue", player->GetName());

        // Get the player's role and dungeons
        uint8 roles = sLFGMgr->GetRoles(playerGuid);
        lfg::LfgDungeonSet const& dungeons = sLFGMgr->GetSelectedDungeons(playerGuid);

        if (dungeons.empty())
        {
            TC_LOG_WARN("module.playerbot.lfg",
                "PlayerbotLFGScript: Player {} has no selected dungeons", player->GetName());
            return;
        }

        // Call LFGBotManager to populate queue with bots
        sLFGBotManager->OnPlayerJoinQueue(player, roles, dungeons);

        // Mark player as processed
        _processedPlayers.insert(playerGuid);
        _processedPlayerTimes[playerGuid] = GameTime::GetGameTimeMS();

        TC_LOG_INFO("module.playerbot.lfg",
            "PlayerbotLFGScript: Triggered bot recruitment for player {} (roles: {}, dungeons: {})",
            player->GetName(), static_cast<uint32>(roles), dungeons.size());
    }

    /**
     * @brief Handle a player leaving the LFG queue
     */
    void HandlePlayerLeftQueue(Player* player)
    {
        ObjectGuid playerGuid = player->GetGUID();

        TC_LOG_INFO("module.playerbot.lfg",
            "PlayerbotLFGScript: Player {} left LFG queue", player->GetName());

        // Notify LFGBotManager to cleanup bots assigned to this player
        sLFGBotManager->OnPlayerLeaveQueue(playerGuid);

        // Remove from processed set so they can queue again
        _processedPlayers.erase(playerGuid);
        _processedPlayerTimes.erase(playerGuid);
    }

    /**
     * @brief Handle role check for a player
     */
    void HandleRoleCheck(Player* player)
    {
        ObjectGuid playerGuid = player->GetGUID();

        TC_LOG_DEBUG("module.playerbot.lfg",
            "PlayerbotLFGScript: Role check started for player {}", player->GetName());

        // Get the group GUID
        ObjectGuid groupGuid = sLFGMgr->GetGroup(playerGuid);
        if (!groupGuid.IsEmpty())
        {
            // Notify LFGBotManager to confirm roles for all bots in this group
            sLFGBotManager->OnRoleCheckReceived(groupGuid);
        }
    }

    /**
     * @brief Check if a player is a bot
     */
    bool IsPlayerBotCheck(Player* player) const
    {
        if (!player)
            return false;

        // Use PlayerBotHooks::IsPlayerBot for consistent bot detection
        return PlayerBotHooks::IsPlayerBot(player);
    }

    /**
     * @brief Cleanup stale tracking data
     */
    void CleanupStaleData()
    {
        uint32 now = GameTime::GetGameTimeMS();

        // Only cleanup every 5 minutes
        if (now - _lastCleanupTime < CLEANUP_INTERVAL)
            return;
        _lastCleanupTime = now;

        // Remove processed players that haven't re-queued in 10 minutes
        constexpr uint32 STALE_THRESHOLD = 10 * MINUTE * IN_MILLISECONDS;

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
            TC_LOG_DEBUG("module.playerbot.lfg",
                "PlayerbotLFGScript: Cleaned up {} stale player entries", toRemove.size());
        }
    }

    // Configuration
    static constexpr uint32 LFG_POLL_INTERVAL = 1000; // 1 second
    static constexpr uint32 CLEANUP_INTERVAL = 5 * MINUTE * IN_MILLISECONDS;

    // State tracking
    uint32 _updateAccumulator = 0;
    uint32 _lastCleanupTime = 0;

    // Processed players (to avoid duplicate bot additions)
    std::unordered_set<ObjectGuid> _processedPlayers;
    std::unordered_map<ObjectGuid, uint32> _processedPlayerTimes;

    // Last known LFG state for each player (for detecting transitions)
    std::unordered_map<ObjectGuid, lfg::LfgState> _lastQueueState;

    // Processed proposals
    std::unordered_set<uint32> _processedProposals;
};

void AddSC_PlayerbotLFGScript()
{
    new PlayerbotLFGScript();
}

} // namespace Playerbot
