/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _BGBOTMANAGER_H
#define _BGBOTMANAGER_H

#include "Common.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Battleground.h"
#include "../Core/DI/Interfaces/IBGBotManager.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>

class Player;
class Group;

namespace Playerbot
{

/**
 * @brief Manages automatic bot recruitment for Battleground queues
 *
 * This manager monitors human player BG queue joins and automatically populates
 * teams with appropriate bots based on faction requirements. It handles:
 * - Detection of human players in BG queue
 * - Selection and queueing of suitable bots (faction-appropriate)
 * - Automatic invitation acceptance for bots
 * - Teleportation to battleground on match start
 * - Tracking of bot assignments to prevent double-queueing
 *
 * Thread-safe singleton implementation using Meyer's singleton pattern.
 */
class TC_GAME_API BGBotManager final : public IBGBotManager
{
private:
    BGBotManager();
    ~BGBotManager();

public:
    // Singleton access (Meyer's singleton - thread-safe in C++11+)
    static BGBotManager* instance();

    // Delete copy/move constructors
    BGBotManager(BGBotManager const&) = delete;
    BGBotManager(BGBotManager&&) = delete;
    BGBotManager& operator=(BGBotManager const&) = delete;
    BGBotManager& operator=(BGBotManager&&) = delete;

    // ============================================================================
    // IBGBotManager INTERFACE
    // ============================================================================

    void Initialize() override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    void OnPlayerJoinQueue(Player* player, BattlegroundTypeId bgTypeId,
                           BattlegroundBracketId bracket, bool asGroup) override;
    void OnPlayerLeaveQueue(ObjectGuid playerGuid) override;
    void OnInvitationReceived(ObjectGuid playerGuid, uint32 bgInstanceGuid) override;
    void OnBattlegroundStart(Battleground* bg) override;
    void OnBattlegroundEnd(Battleground* bg, Team winnerTeam) override;

    uint32 PopulateQueue(ObjectGuid playerGuid, BattlegroundTypeId bgTypeId,
                         BattlegroundBracketId bracket,
                         uint32 neededAlliance, uint32 neededHorde) override;

    bool IsBotQueued(ObjectGuid botGuid) const override;
    void GetStatistics(uint32& totalQueued, uint32& totalAssignments) const override;
    void SetEnabled(bool enable) override;
    bool IsEnabled() const override { return _enabled; }
    void CleanupStaleAssignments() override;

    // ============================================================================
    // ADDITIONAL METHODS
    // ============================================================================

    /**
     * @brief Get the team size for a battleground type
     * @param bgTypeId The battleground type
     * @return Team size (10 for WSG/AB, 15 for AV, etc.)
     */
    uint32 GetBGTeamSize(BattlegroundTypeId bgTypeId) const;

    /**
     * @brief Get the minimum players needed to start
     * @param bgTypeId The battleground type
     * @return Minimum player count
     */
    uint32 GetBGMinPlayers(BattlegroundTypeId bgTypeId) const;

private:
    // ============================================================================
    // HELPER METHODS
    // ============================================================================

    /**
     * @brief Calculate how many bots are needed for each faction
     */
    void CalculateNeededBots(BattlegroundTypeId bgTypeId, Team humanTeam,
                              uint32& allianceNeeded, uint32& hordeNeeded) const;

    /**
     * @brief Queue a bot for battleground
     */
    bool QueueBot(Player* bot, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket);

    /**
     * @brief Remove a bot from BG queue
     */
    void RemoveBotFromQueue(Player* bot);

    /**
     * @brief Find available bots for a faction
     */
    std::vector<Player*> FindAvailableBots(Team team, uint8 minLevel, uint8 maxLevel,
                                            uint32 count) const;

    /**
     * @brief Get level range for bracket
     */
    void GetBracketLevelRange(BattlegroundBracketId bracket, uint8& minLevel, uint8& maxLevel) const;

    /**
     * @brief Register bot assignment
     */
    void RegisterBotAssignment(ObjectGuid humanGuid, ObjectGuid botGuid,
                                BattlegroundTypeId bgTypeId, Team team);

    /**
     * @brief Unregister bot assignment
     */
    void UnregisterBotAssignment(ObjectGuid botGuid);

    /**
     * @brief Unregister all bots for a player
     */
    void UnregisterAllBotsForPlayer(ObjectGuid humanGuid);

    /**
     * @brief Check if bot is available for BG
     */
    bool IsBotAvailable(Player* bot) const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    /**
     * @brief Information about a bot queued for BG
     */
    struct BotQueueInfo
    {
        ObjectGuid humanPlayerGuid;     ///< Human player this bot is associated with
        BattlegroundTypeId bgTypeId;    ///< Battleground type
        Team team;                       ///< Faction
        time_t queueTime;                ///< When queued
        uint32 bgInstanceGuid;           ///< BG instance if invited

        BotQueueInfo() : bgTypeId(BATTLEGROUND_TYPE_NONE), team(TEAM_OTHER),
                         queueTime(0), bgInstanceGuid(0) {}

        BotQueueInfo(ObjectGuid humanGuid, BattlegroundTypeId bgType, Team t)
            : humanPlayerGuid(humanGuid), bgTypeId(bgType), team(t),
              queueTime(time(nullptr)), bgInstanceGuid(0) {}
    };

    /**
     * @brief Information about a human player with bot assignments
     */
    struct HumanPlayerQueueInfo
    {
        std::vector<ObjectGuid> assignedBots;
        BattlegroundTypeId bgTypeId;
        BattlegroundBracketId bracket;
        Team team;
        time_t queueTime;

        HumanPlayerQueueInfo() : bgTypeId(BATTLEGROUND_TYPE_NONE),
                                  bracket(BG_BRACKET_ID_FIRST),
                                  team(TEAM_OTHER), queueTime(0) {}

        HumanPlayerQueueInfo(BattlegroundTypeId bgType, BattlegroundBracketId b, Team t)
            : bgTypeId(bgType), bracket(b), team(t), queueTime(time(nullptr)) {}
    };

    // ============================================================================
    // MEMBER VARIABLES
    // ============================================================================

    mutable OrderedRecursiveMutex<LockOrder::GROUP_MANAGER> _mutex;

    /// Map of bot GUID -> queue information
    std::unordered_map<ObjectGuid, BotQueueInfo> _queuedBots;

    /// Map of human player GUID -> queue information
    std::unordered_map<ObjectGuid, HumanPlayerQueueInfo> _humanPlayers;

    /// Map of BG instance GUID -> set of bot GUIDs
    std::unordered_map<uint32, std::unordered_set<ObjectGuid>> _bgInstanceBots;

    /// Whether the system is enabled
    std::atomic<bool> _enabled;

    /// Update accumulator for periodic cleanup
    uint32 _updateAccumulator;

    /// Cleanup interval (5 minutes)
    static constexpr uint32 CLEANUP_INTERVAL = 5 * MINUTE * IN_MILLISECONDS;

    /// Maximum queue time before considered stale (30 minutes)
    static constexpr time_t MAX_QUEUE_TIME = 30 * MINUTE;

    /// Whether initialized
    bool _initialized;
};

} // namespace Playerbot

#define sBGBotManager Playerbot::BGBotManager::instance()

#endif // _BGBOTMANAGER_H
