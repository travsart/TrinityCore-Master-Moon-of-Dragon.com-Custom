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
#include "Player.h"
#include "Unit.h"
#include <unordered_map>
#include <unordered_set>

namespace Playerbot
{

class TC_GAME_API InterruptManager
{
public:
    explicit InterruptManager(Player* bot);
    ~InterruptManager() = default;

    // Interrupt management
    ::Unit* GetHighestPriorityTarget();
    bool ShouldInterrupt(::Unit* target);
    void RecordInterruptAttempt(::Unit* target, bool success);

    // Priority calculation
    float CalculateInterruptPriority(::Unit* target);
    bool IsHighPrioritySpell(uint32 spellId);

private:
    Player* _bot;
    std::unordered_map<ObjectGuid, uint32> _lastInterruptAttempt;
    std::unordered_set<uint32> _highPrioritySpells;
};

} // namespace Playerbot