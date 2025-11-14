/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <vector>
#include <ctime>

class Player;

namespace Playerbot
{

/**
 * @brief Interface for LFG bot selection and matching
 *
 * Defines the contract for selecting appropriate bots to fill LFG queues
 * based on role requirements, level range, gear quality, and availability.
 */
class TC_GAME_API ILFGBotSelector
{
public:
    virtual ~ILFGBotSelector() = default;

    /**
     * @brief Find available tank bots within level range
     *
     * @param minLevel Minimum level requirement
     * @param maxLevel Maximum level requirement
     * @param count Number of tanks needed
     * @return Vector of tank bot Player pointers (may be fewer than requested)
     */
    virtual ::std::vector<Player*> FindTanks(uint8 minLevel, uint8 maxLevel, uint32 count) = 0;

    /**
     * @brief Find available healer bots within level range
     *
     * @param minLevel Minimum level requirement
     * @param maxLevel Maximum level requirement
     * @param count Number of healers needed
     * @return Vector of healer bot Player pointers (may be fewer than requested)
     */
    virtual ::std::vector<Player*> FindHealers(uint8 minLevel, uint8 maxLevel, uint32 count) = 0;

    /**
     * @brief Find available DPS bots within level range
     *
     * @param minLevel Minimum level requirement
     * @param maxLevel Maximum level requirement
     * @param count Number of DPS needed
     * @return Vector of DPS bot Player pointers (may be fewer than requested)
     */
    virtual ::std::vector<Player*> FindDPS(uint8 minLevel, uint8 maxLevel, uint32 count) = 0;

    /**
     * @brief Check if a bot is available for LFG queueing
     *
     * Checks:
     * - Not already in a group
     * - Not already in LFG queue
     * - Not in a dungeon/raid/battleground
     * - No deserter debuff
     * - Not on cooldown
     * - Online and active
     *
     * @param bot The bot to check
     * @return true if available, false otherwise
     */
    virtual bool IsBotAvailable(Player* bot) = 0;

    /**
     * @brief Calculate priority score for a bot to fill a specific role
     *
     * Higher scores indicate better suitability. Scoring factors:
     * - Level match to desired level (+100 for exact, -10 per level difference)
     * - Gear quality/item level (+0 to +300)
     * - Role proficiency (+500 if primary role)
     * - Recent activity (-50 if used in last hour, +100 if unused)
     * - Geographic proximity (+50 if same continent)
     *
     * @param bot The bot to score
     * @param desiredRole The role needed (PLAYER_ROLE_TANK/HEALER/DAMAGE)
     * @param desiredLevel The ideal level for the dungeon
     * @return Priority score (higher is better, 0-2000+)
     */
    virtual uint32 CalculateBotPriority(Player* bot, uint8 desiredRole, uint8 desiredLevel) = 0;

    /**
     * @brief Set the last queue time for a bot
     *
     * Used to track bot usage and prefer less recently used bots.
     *
     * @param botGuid The bot's GUID
     * @param queueTime The time the bot was queued (timestamp)
     */
    virtual void SetLastQueueTime(ObjectGuid botGuid, time_t queueTime) = 0;

    /**
     * @brief Get the last queue time for a bot
     *
     * @param botGuid The bot's GUID
     * @return Last queue timestamp, or 0 if never queued
     */
    virtual time_t GetLastQueueTime(ObjectGuid botGuid) = 0;

    /**
     * @brief Clear tracking data for a bot
     *
     * @param botGuid The bot's GUID
     */
    virtual void ClearBotTracking(ObjectGuid botGuid) = 0;

    /**
     * @brief Clear all tracking data
     */
    virtual void ClearAllTracking() = 0;
};

} // namespace Playerbot
