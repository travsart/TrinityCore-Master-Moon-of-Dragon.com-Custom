/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BotMovementController.h"
#include "Unit.h"
#include "Position.h"

BotMovementController::BotMovementController(Unit* owner)
    : _owner(owner)
    , _totalTimePassed(0)
{
    if (_owner)
        RecordPosition();
}

BotMovementController::~BotMovementController()
{
}

void BotMovementController::Update(uint32 diff)
{
    _totalTimePassed += diff;
}

Position const* BotMovementController::GetLastPosition() const
{
    if (_positionHistory.empty())
        return nullptr;
    
    return &_positionHistory.back().pos;
}

void BotMovementController::RecordPosition()
{
    if (!_owner)
        return;
    
    Position currentPos = _owner->GetPosition();
    _positionHistory.emplace_back(currentPos, _totalTimePassed);
    
    if (_positionHistory.size() > MAX_POSITION_HISTORY)
        _positionHistory.pop_front();
}
