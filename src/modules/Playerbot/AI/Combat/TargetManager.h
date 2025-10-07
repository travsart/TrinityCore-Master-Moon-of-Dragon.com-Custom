/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_TARGET_MANAGER_H
#define TRINITYCORE_TARGET_MANAGER_H

#include "Define.h"

class Player;
class Unit;

namespace Playerbot
{
    struct CombatMetrics;

    // Stub class for TargetManager - full implementation pending
    class TargetManager
    {
    public:
        explicit TargetManager(Player* bot) : _bot(bot) {}
        ~TargetManager() = default;

        void Update(uint32 diff, const CombatMetrics& metrics) {}
        void Reset() {}

        Unit* GetPriorityTarget() { return nullptr; }
        bool ShouldSwitchTarget() { return false; }
        bool IsHighPriorityTarget(Unit* target) { return false; }

    private:
        Player* _bot;
    };

} // namespace Playerbot

#endif // TRINITYCORE_TARGET_MANAGER_H
