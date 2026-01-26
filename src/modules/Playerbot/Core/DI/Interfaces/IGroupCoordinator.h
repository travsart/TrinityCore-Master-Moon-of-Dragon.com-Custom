/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <cstdint>

class Player;
class Group;
class Unit;

namespace Playerbot
{

/**
 * @brief Interface for Group Coordination
 *
 * Abstracts group management operations to enable dependency injection.
 * Handles group formation, role assignment, and coordination.
 */
class TC_GAME_API IGroupCoordinator
{
public:
    virtual ~IGroupCoordinator() = default;

    /**
     * @brief Initialize coordinator
     */
    virtual void Initialize() = 0;

    /**
     * @brief Update coordinator logic
     *
     * @param diff Time elapsed since last update (milliseconds)
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Reset coordinator state
     */
    virtual void Reset() = 0;

    /**
     * @brief Shutdown coordinator
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Join a group
     *
     * @param group Group to join
     * @return True if joined successfully, false otherwise
     */
    virtual bool JoinGroup(Group* group) = 0;

    /**
     * @brief Leave current group
     *
     * @return True if left successfully, false otherwise
     */
    virtual bool LeaveGroup() = 0;

    /**
     * @brief Get current group
     *
     * @return Pointer to group, or nullptr if not in group
     */
    virtual Group* GetGroup() const = 0;

    /**
     * @brief Check if in a group
     *
     * @return True if in group, false otherwise
     */
    virtual bool IsInGroup() const = 0;

    /**
     * @brief Check if in a raid
     *
     * @return True if in raid, false otherwise
     */
    virtual bool IsInRaid() const = 0;

    /**
     * @brief Get raid size
     *
     * @return Number of players in raid, or 0 if not in raid
     */
    virtual uint32 GetRaidSize() const = 0;

    /**
     * @brief Get group size
     *
     * @return Number of players in group, or 0 if not in group
     */
    virtual uint32 GetGroupSize() const = 0;

    /**
     * @brief Get group leader
     *
     * @return Pointer to leader player, or nullptr if not in group
     */
    virtual Player* GetLeader() const = 0;

    // ========================================================================
    // Role queries - Used by coordinators to determine bot's role in group
    // ========================================================================

    /**
     * @brief Check if bot is assigned as tank
     *
     * @return True if tank role, false otherwise
     */
    virtual bool IsTank() const = 0;

    /**
     * @brief Check if bot is assigned as healer
     *
     * @return True if healer role, false otherwise
     */
    virtual bool IsHealer() const = 0;

    /**
     * @brief Check if bot is assigned as DPS
     *
     * @return True if DPS role, false otherwise
     */
    virtual bool IsDPS() const = 0;

    // ========================================================================
    // Combat queries - Used by strategies to coordinate group combat
    // ========================================================================

    /**
     * @brief Check if group is in combat
     *
     * @return True if any group member is in combat, false otherwise
     */
    virtual bool IsInCombat() const = 0;

    /**
     * @brief Get current group target (focus target for coordinated attacks)
     *
     * @return Pointer to target unit, or nullptr if no group target
     */
    virtual Unit* GetGroupTarget() const = 0;
};

} // namespace Playerbot
