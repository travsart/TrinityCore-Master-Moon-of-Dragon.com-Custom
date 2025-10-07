/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_DEFENSIVE_MANAGER_H
#define TRINITYCORE_DEFENSIVE_MANAGER_H

#include "Define.h"

class Player;

namespace Playerbot
{
    struct CombatMetrics;

    // Stub class for DefensiveManager - full implementation pending
    class DefensiveManager
    {
    public:
        explicit DefensiveManager(Player* bot) : _bot(bot) {}
        ~DefensiveManager() = default;

        void Update(uint32 diff, const CombatMetrics& metrics) {}
        void Reset() {}

        bool NeedsDefensive() { return false; }
        bool NeedsEmergencyDefensive() { return false; }
        uint32 GetRecommendedDefensive() { return 0; }
        uint32 UseEmergencyDefensive() { return 0; }

    private:
        Player* _bot;
    };

} // namespace Playerbot

#endif // TRINITYCORE_DEFENSIVE_MANAGER_H
