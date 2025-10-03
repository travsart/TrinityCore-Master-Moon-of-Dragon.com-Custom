/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RogueAI.h"
#include "Player.h"
#include "Unit.h"
#include <cmath>

namespace Playerbot
{

Position RogueCombatPositioning::CalculateOptimalPosition(Unit* target, RogueSpec spec)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    // Simple behind-target positioning
    Position pos = _bot->GetPosition();
    float angle = target->GetOrientation() + M_PI; // Behind target
    pos.m_positionX = target->GetPositionX() + cos(angle) * 3.0f;
    pos.m_positionY = target->GetPositionY() + sin(angle) * 3.0f;
    pos.m_positionZ = target->GetPositionZ();
    return pos;
}

float RogueCombatPositioning::GetOptimalRange(RogueSpec spec) const
{
    return 5.0f; // Melee range
}

} // namespace Playerbot