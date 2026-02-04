/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _ARENABOTMANAGER_H
#define _ARENABOTMANAGER_H

#include "Common.h"
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Battleground.h"
#include "../Core/DI/Interfaces/IArenaBotManager.h"
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
 * @brief Manages automatic bot recruitment for Arena queues
 *
 * This manager monitors human player arena queue joins and automatically populates
 * teams with appropriate bots. It handles:
 * - Detection of human players in arena queue (2v2, 3v3 rated and skirmish)
 * - Selection and queueing of suitable bot teammates
 * - Generation of bot opponents for complete matches
 * - Automatic invitation acceptance for bots
 * - Rating tracking for rated arenas
 *
 * Thread-safe singleton implementation using Meyer's singleton pattern.
 *
 * Note: Solo Shuffle is NOT available in TrinityCore 12.0 (The War Within)
 */
class TC_GAME_API ArenaBotManager final : public IArenaBotManager
{
private:
    ArenaBotManager();
    ~ArenaBotManager();

public:
    // Singleton access
    static ArenaBotManager* instance();

    // Delete copy/move
    ArenaBotManager(ArenaBotManager const&) = delete;
    ArenaBotManager(ArenaBotManager&&) = delete;
    ArenaBotManager& operator=(ArenaBotManager const&) = delete;
    ArenaBotManager& operator=(ArenaBotManager&&) = delete;

    // ============================================================================
    // IArenaBotManager INTERFACE
    // ============================================================================

    void Initialize() override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    void OnPlayerJoinQueue(Player* player, ArenaBracketType bracketType,
                           ArenaQueueMode mode, bool asGroup) override;
    void OnPlayerLeaveQueue(ObjectGuid playerGuid) override;
    void OnInvitationReceived(ObjectGuid playerGuid, uint32 arenaInstanceGuid) override;
    void OnArenaStart(Battleground* bg) override;
    void OnArenaEnd(Battleground* bg, Team winnerTeam) override;

    uint32 PopulateTeammates(ObjectGuid playerGuid, ArenaBracketType bracketType,
                              ArenaQueueMode mode, uint32 teammatesNeeded) override;
    uint32 PopulateOpponents(ArenaBracketType bracketType, ArenaQueueMode mode,
                              uint32 opponentsNeeded) override;

    bool IsBotQueued(ObjectGuid botGuid) const override;
    void GetStatistics(uint32& totalQueued, uint32& totalAssignments) const override;
    void SetEnabled(bool enable) override;
    bool IsEnabled() const override { return _enabled; }
    void CleanupStaleAssignments() override;

    uint8 GetTeamSize(ArenaBracketType bracketType) const override;

    // ============================================================================
    // ADDITIONAL METHODS
    // ============================================================================

    /**
     * @brief Get battleground type for arena bracket
     */
    BattlegroundTypeId GetBGTypeForBracket(ArenaBracketType bracketType) const;

    /**
     * @brief Check if bracket type is skirmish
     */
    bool IsSkirmish(ArenaBracketType bracketType) const;

private:
    // ============================================================================
    // HELPER METHODS
    // ============================================================================

    /**
     * @brief Queue a bot for arena
     */
    bool QueueBot(Player* bot, ArenaBracketType bracketType, ArenaQueueMode mode, bool asTeammate);

    /**
     * @brief Remove a bot from arena queue
     */
    void RemoveBotFromQueue(Player* bot);

    /**
     * @brief Find available bots for arena
     */
    std::vector<Player*> FindAvailableBots(uint8 minLevel, uint8 maxLevel, uint32 count) const;

    /**
     * @brief Find bots suitable for specific roles (healer, DPS)
     */
    std::vector<Player*> FindBotsForTeamComposition(uint32 teamSize, uint8 minLevel, uint8 maxLevel) const;

    /**
     * @brief Register bot assignment
     */
    void RegisterBotAssignment(ObjectGuid humanGuid, ObjectGuid botGuid,
                                ArenaBracketType bracketType, ArenaQueueMode mode, bool isTeammate);

    /**
     * @brief Unregister bot assignment
     */
    void UnregisterBotAssignment(ObjectGuid botGuid);

    /**
     * @brief Unregister all bots for a player
     */
    void UnregisterAllBotsForPlayer(ObjectGuid humanGuid);

    /**
     * @brief Check if bot is available for arena
     */
    bool IsBotAvailable(Player* bot) const;

    /**
     * @brief Check if bot can be healer
     */
    bool CanBeHealer(Player* bot) const;

    /**
     * @brief Check if bot can be DPS
     */
    bool CanBeDPS(Player* bot) const;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    /**
     * @brief Information about a bot queued for arena
     */
    struct BotQueueInfo
    {
        ObjectGuid humanPlayerGuid;
        ArenaBracketType bracketType;
        ArenaQueueMode mode;
        bool isTeammate;            ///< true if teammate, false if opponent
        time_t queueTime;
        uint32 arenaInstanceGuid;

        BotQueueInfo() : bracketType(ArenaBracketType::ARENA_2v2),
                         mode(ArenaQueueMode::SKIRMISH),
                         isTeammate(true), queueTime(0), arenaInstanceGuid(0) {}

        BotQueueInfo(ObjectGuid humanGuid, ArenaBracketType bracket, ArenaQueueMode m, bool teammate)
            : humanPlayerGuid(humanGuid), bracketType(bracket), mode(m),
              isTeammate(teammate), queueTime(time(nullptr)), arenaInstanceGuid(0) {}
    };

    /**
     * @brief Information about a human player with bot assignments
     */
    struct HumanPlayerQueueInfo
    {
        std::vector<ObjectGuid> teammates;
        std::vector<ObjectGuid> opponents;
        ArenaBracketType bracketType;
        ArenaQueueMode mode;
        time_t queueTime;

        HumanPlayerQueueInfo() : bracketType(ArenaBracketType::ARENA_2v2),
                                  mode(ArenaQueueMode::SKIRMISH), queueTime(0) {}

        HumanPlayerQueueInfo(ArenaBracketType bracket, ArenaQueueMode m)
            : bracketType(bracket), mode(m), queueTime(time(nullptr)) {}
    };

    // ============================================================================
    // MEMBER VARIABLES
    // ============================================================================

    mutable OrderedRecursiveMutex<LockOrder::GROUP_MANAGER> _mutex;

    /// Map of bot GUID -> queue information
    std::unordered_map<ObjectGuid, BotQueueInfo> _queuedBots;

    /// Map of human player GUID -> queue information
    std::unordered_map<ObjectGuid, HumanPlayerQueueInfo> _humanPlayers;

    /// Map of arena instance GUID -> set of bot GUIDs
    std::unordered_map<uint32, std::unordered_set<ObjectGuid>> _arenaInstanceBots;

    /// Whether the system is enabled
    std::atomic<bool> _enabled;

    /// Update accumulator
    uint32 _updateAccumulator;

    /// Cleanup interval (5 minutes)
    static constexpr uint32 CLEANUP_INTERVAL = 5 * MINUTE * IN_MILLISECONDS;

    /// Maximum queue time (15 minutes for arena)
    static constexpr time_t MAX_QUEUE_TIME = 15 * MINUTE;

    /// Whether initialized
    bool _initialized;
};

} // namespace Playerbot

#define sArenaBotManager Playerbot::ArenaBotManager::instance()

#endif // _ARENABOTMANAGER_H
