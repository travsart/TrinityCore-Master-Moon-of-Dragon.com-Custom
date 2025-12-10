/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _IBGBOTMANAGER_H
#define _IBGBOTMANAGER_H

#include "ObjectGuid.h"
#include "Battleground.h"
#include <cstdint>

class Player;

namespace Playerbot
{

/**
 * @brief Interface for Battleground Bot Manager
 *
 * Manages automatic bot recruitment for battleground queues.
 * Detects human player queue joins and populates with appropriate bots
 * to enable shorter queue times for single-player experience.
 */
class IBGBotManager
{
public:
    virtual ~IBGBotManager() = default;

    /**
     * @brief Initialize the BG Bot Manager
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
     * @brief Called when a human player joins a BG queue
     * @param player The human player
     * @param bgTypeId The battleground type ID
     * @param bracket The bracket ID
     * @param asGroup Whether queued as group
     */
    virtual void OnPlayerJoinQueue(Player* player, BattlegroundTypeId bgTypeId,
                                    BattlegroundBracketId bracket, bool asGroup) = 0;

    /**
     * @brief Called when a player leaves the BG queue
     * @param playerGuid GUID of the player
     */
    virtual void OnPlayerLeaveQueue(ObjectGuid playerGuid) = 0;

    /**
     * @brief Called when a BG invitation is received
     * @param playerGuid The player receiving invitation
     * @param bgInstanceGuid The BG instance GUID
     */
    virtual void OnInvitationReceived(ObjectGuid playerGuid, uint32 bgInstanceGuid) = 0;

    /**
     * @brief Called when a BG match starts
     * @param bg The battleground instance
     */
    virtual void OnBattlegroundStart(Battleground* bg) = 0;

    /**
     * @brief Called when a BG match ends
     * @param bg The battleground instance
     * @param winnerTeam The winning team
     */
    virtual void OnBattlegroundEnd(Battleground* bg, Team winnerTeam) = 0;

    /**
     * @brief Manually populate queue with bots
     * @param playerGuid The player to populate for
     * @param bgTypeId The battleground type
     * @param bracket The bracket
     * @param neededAlliance Number of Alliance bots needed
     * @param neededHorde Number of Horde bots needed
     * @return Number of bots queued
     */
    virtual uint32 PopulateQueue(ObjectGuid playerGuid, BattlegroundTypeId bgTypeId,
                                  BattlegroundBracketId bracket,
                                  uint32 neededAlliance, uint32 neededHorde) = 0;

    /**
     * @brief Check if a bot is currently queued for BG
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
};

} // namespace Playerbot

#endif // _IBGBOTMANAGER_H
