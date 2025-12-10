/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _IARENABOTMANAGER_H
#define _IARENABOTMANAGER_H

#include "ObjectGuid.h"
#include "Battleground.h"
#include <cstdint>

class Player;

namespace Playerbot
{

/**
 * @brief Arena bracket types
 */
enum class ArenaBracketType : uint8
{
    ARENA_2v2 = 0,
    ARENA_3v3 = 1,
    ARENA_5v5 = 2,      // Note: Removed in 11.2 but kept for compatibility
    SKIRMISH_2v2 = 10,
    SKIRMISH_3v3 = 11
};

/**
 * @brief Arena queue mode
 */
enum class ArenaQueueMode : uint8
{
    RATED = 0,
    SKIRMISH = 1
};

/**
 * @brief Interface for Arena Bot Manager
 *
 * Manages automatic bot recruitment for arena queues.
 * Supports both rated arenas (2v2/3v3) and skirmishes.
 * Enables single-player arena experience with AI teammates/opponents.
 */
class IArenaBotManager
{
public:
    virtual ~IArenaBotManager() = default;

    /**
     * @brief Initialize the Arena Bot Manager
     */
    virtual void Initialize() = 0;

    /**
     * @brief Shutdown and cleanup
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Update manager state
     * @param diff Time elapsed since last update in milliseconds
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Called when a human player joins an arena queue
     * @param player The human player
     * @param bracketType The arena bracket (2v2/3v3)
     * @param mode Rated or skirmish
     * @param asGroup Whether queued as group
     */
    virtual void OnPlayerJoinQueue(Player* player, ArenaBracketType bracketType,
                                    ArenaQueueMode mode, bool asGroup) = 0;

    /**
     * @brief Called when a player leaves the arena queue
     * @param playerGuid GUID of the player
     */
    virtual void OnPlayerLeaveQueue(ObjectGuid playerGuid) = 0;

    /**
     * @brief Called when an arena invitation is received
     * @param playerGuid The player receiving invitation
     * @param arenaInstanceGuid The arena instance GUID
     */
    virtual void OnInvitationReceived(ObjectGuid playerGuid, uint32 arenaInstanceGuid) = 0;

    /**
     * @brief Called when an arena match starts
     * @param bg The arena (battleground) instance
     */
    virtual void OnArenaStart(Battleground* bg) = 0;

    /**
     * @brief Called when an arena match ends
     * @param bg The arena instance
     * @param winnerTeam The winning team
     */
    virtual void OnArenaEnd(Battleground* bg, Team winnerTeam) = 0;

    /**
     * @brief Populate arena queue with bot teammates
     * @param playerGuid The human player
     * @param bracketType The bracket type
     * @param mode Queue mode
     * @param teammatesNeeded Number of bot teammates needed
     * @return Number of bots queued
     */
    virtual uint32 PopulateTeammates(ObjectGuid playerGuid, ArenaBracketType bracketType,
                                      ArenaQueueMode mode, uint32 teammatesNeeded) = 0;

    /**
     * @brief Populate arena queue with bot opponents
     * @param bracketType The bracket type
     * @param mode Queue mode
     * @param opponentsNeeded Number of opponents needed
     * @return Number of bots queued
     */
    virtual uint32 PopulateOpponents(ArenaBracketType bracketType, ArenaQueueMode mode,
                                      uint32 opponentsNeeded) = 0;

    /**
     * @brief Check if a bot is currently queued for arena
     * @param botGuid The bot's GUID
     * @return true if queued
     */
    virtual bool IsBotQueued(ObjectGuid botGuid) const = 0;

    /**
     * @brief Get statistics
     * @param totalQueued Output: Total bots queued
     * @param totalAssignments Output: Total active assignments
     */
    virtual void GetStatistics(uint32& totalQueued, uint32& totalAssignments) const = 0;

    /**
     * @brief Enable or disable the system
     * @param enable true to enable
     */
    virtual void SetEnabled(bool enable) = 0;

    /**
     * @brief Check if system is enabled
     * @return true if enabled
     */
    virtual bool IsEnabled() const = 0;

    /**
     * @brief Cleanup stale assignments
     */
    virtual void CleanupStaleAssignments() = 0;

    /**
     * @brief Get team size for bracket
     * @param bracketType The bracket type
     * @return Team size (2, 3, or 5)
     */
    virtual uint8 GetTeamSize(ArenaBracketType bracketType) const = 0;
};

} // namespace Playerbot

#endif // _IARENABOTMANAGER_H
