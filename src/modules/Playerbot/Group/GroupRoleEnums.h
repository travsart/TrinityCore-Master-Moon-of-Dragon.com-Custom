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

namespace Playerbot
{

enum class GroupRole : uint8
{
    TANK        = 0,
    HEALER      = 1,
    MELEE_DPS   = 2,
    RANGED_DPS  = 3,
    SUPPORT     = 4,
    UTILITY     = 5,
    NONE        = 6
};

enum class RoleCapability : uint8
{
    PRIMARY     = 0,  // Main specialization role
    SECONDARY   = 1,  // Off-spec capable
    HYBRID      = 2,  // Dual-role capable (e.g., Paladin tank/heal)
    EMERGENCY   = 3,  // Can fill role in emergency
    INCAPABLE   = 4   // Cannot perform this role
};

enum class RoleAssignmentStrategy : uint8
{
    OPTIMAL_ASSIGNMENT         = 0,  // Best possible role distribution
    BALANCED_ASSIGNMENT        = 1,  // Even distribution of capabilities
    FLEXIBLE_ASSIGNMENT        = 2,  // Adapt to group needs
    STRICT_PRIMARY_ONLY        = 3,  // Only assign primary roles
    HYBRID_CLASS_FRIENDLY      = 4,  // Favor hybrid classes
    DUNGEON_CONTENT_FOCUSED    = 5,  // Optimize for dungeon content
    RAID_CONTENT_FOCUSED       = 6,  // Optimize for raid content
    PVP_CONTENT_FOCUSED        = 7   // Optimize for PvP content
};

} // namespace Playerbot
