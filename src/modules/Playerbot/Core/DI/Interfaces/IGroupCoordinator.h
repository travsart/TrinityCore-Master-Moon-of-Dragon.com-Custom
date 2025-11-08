/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Define.h"
#include <cstdint>

class Player;
class Group;

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
};

} // namespace Playerbot
