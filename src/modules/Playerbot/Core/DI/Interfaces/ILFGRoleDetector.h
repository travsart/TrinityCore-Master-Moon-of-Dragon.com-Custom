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

class Player;

/**
 * @brief Interface for LFG role detection system
 *
 * Automatically detects player roles based on spec, gear, and class.
 */
class TC_GAME_API ILFGRoleDetector
{
public:
    virtual ~ILFGRoleDetector() = default;

    // Core role detection
    virtual uint8 DetectPlayerRole(Player* player) = 0;
    virtual uint8 DetectBotRole(Player* bot) = 0;
    virtual bool CanPerformRole(Player* player, uint8 role) = 0;
    virtual uint8 GetBestRoleForPlayer(Player* player) = 0;
    virtual uint8 GetAllPerformableRoles(Player* player) = 0;
    virtual uint8 GetRoleFromSpecialization(Player* player, uint32 specId) = 0;
};

#endif // ILFGRoleDetector_h
