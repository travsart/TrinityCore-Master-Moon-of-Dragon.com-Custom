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
#include "Position.h"

namespace Playerbot
{

class TC_GAME_API PositionManager
{
public:
    explicit PositionManager(Player* bot);
    ~PositionManager() = default;

    // Position calculation
    Position GetOptimalCastingPosition(::Unit* target, float range);
    Position GetOptimalRangedPosition(::Unit* target, float range);
    Position FindSafePosition(float minDistance);
    Position PredictPosition(::Unit* target, float timeAhead);

    // Position validation
    bool IsPositionSafe(const Position& pos);
    bool IsInOptimalRange(::Unit* target, float desiredRange);

    // Movement management
    void SetOptimalPosition(::Unit* target, float range);
    void MoveToPosition(const Position& pos);

private:
    Player* _bot;
    Position _lastOptimalPosition;
};

} // namespace Playerbot