/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_CROWD_CONTROL_MANAGER_H
#define TRINITYCORE_CROWD_CONTROL_MANAGER_H

#include "Define.h"

class Player;
class Unit;

namespace Playerbot
{
    struct CombatMetrics;

    // Stub class for CrowdControlManager - full implementation pending
    class CrowdControlManager
    {
    public:
        explicit CrowdControlManager(Player* bot) : _bot(bot) {}
        ~CrowdControlManager() = default;

        void Update(uint32 diff, const CombatMetrics& metrics) {}
        void Reset() {}

        bool ShouldUseCrowdControl() { return false; }
        Unit* GetPriorityTarget() { return nullptr; }
        uint32 GetRecommendedSpell(Unit* target) { return 0; }

    private:
        Player* _bot;
    };

} // namespace Playerbot

#endif // TRINITYCORE_CROWD_CONTROL_MANAGER_H
