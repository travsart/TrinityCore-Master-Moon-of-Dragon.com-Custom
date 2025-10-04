/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MovementGenerator.h"
#include "Player.h"
#include "MotionMaster.h"
#include "MovementPackets.h"
#include "MovementDefines.h"
#include "MoveSplineInit.h"
#include "Log.h"
#include "World.h"
#include <chrono>

namespace PlayerBot
{
    MovementGenerator::MovementGenerator(MovementGeneratorType type, MovementPriority priority)
        : _type(type), _priority(priority), _isActive(false), _hasReached(false),
          _needsPath(false), _updateInterval(MovementConstants::UPDATE_INTERVAL_NORMAL),
          _recalcThreshold(MovementConstants::RECALC_THRESHOLD),
          _smoothTransitions(true), _positionCheckTimer(0), _pathRecalcTimer(0)
    {
        _lastUpdate = std::chrono::steady_clock::now();
        _initTime = _lastUpdate;
        _metrics.Reset();
    }

    MovementGenerator::~MovementGenerator()
    {
        // Virtual destructor for proper cleanup
    }

    bool MovementGenerator::CanBeInterrupted(MovementGeneratorType newType,
                                            MovementPriority newPriority) const
    {
        // Higher priority always interrupts
        if (newPriority > _priority)
            return true;

        // Same priority - check specific rules
        if (newPriority == _priority)
        {
            // Combat movements can interrupt non-combat of same priority
            if ((newType == MovementGeneratorType::MOVEMENT_CHASE ||
                 newType == MovementGeneratorType::MOVEMENT_FLEE) &&
                (_type != MovementGeneratorType::MOVEMENT_CHASE &&
                 _type != MovementGeneratorType::MOVEMENT_FLEE))
            {
                return true;
            }
        }

        return false;
    }

    void MovementGenerator::OnInterrupted(Player* bot, MovementGeneratorType interruptType)
    {
        if (!bot)
            return;

        _isActive.store(false);
        StopMovement(bot);
        _state.lastResult = MovementResult::MOVEMENT_CANCELLED;

        TC_LOG_DEBUG("playerbot.movement", "Movement generator [%u] interrupted by [%u] for bot %s",
            static_cast<uint8>(_type), static_cast<uint8>(interruptType), bot->GetName().c_str());
    }

    void MovementGenerator::OnTargetMoved(Player* bot, Position const& newPosition)
    {
        if (!bot || !_isActive.load())
            return;

        float distance = bot->GetExactDist(&newPosition);
        if (distance > _recalcThreshold)
        {
            _needsPath.store(true);
            _state.needsRecalc = true;
            _state.targetPosition = newPosition;

            TC_LOG_DEBUG("playerbot.movement", "Target moved %.2f yards, marking for recalculation",
                distance);
        }
    }

    void MovementGenerator::SendMovementPacket(Player* bot, Position const& position, float speed)
    {
        if (!bot || !bot->IsInWorld())
            return;

        // Use TrinityCore's movement system
        Movement::MoveSplineInit init(bot);
        init.MoveTo(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(), true);

        if (speed > 0.0f)
            init.SetVelocity(speed);

        // Apply smooth transitions if enabled
        if (_smoothTransitions)
        {
            init.SetSmooth();
            init.SetUncompressed();
        }

        // Set appropriate movement flags based on state
        if (_state.currentType == MovementGeneratorType::MOVEMENT_FLEE)
            init.SetBackward();
        else if (_state.currentType == MovementGeneratorType::MOVEMENT_CHASE)
            init.SetRun();

        init.Launch();

        // Update state
        _state.isMoving = true;
        _state.currentSpeed = speed > 0.0f ? speed : bot->GetSpeed(MOVE_RUN);
        _state.targetPosition = position;
    }

    void MovementGenerator::SetFacing(Player* bot, float angle)
    {
        if (!bot || !bot->IsInWorld())
            return;

        // Normalize angle to 0-2PI range
        angle = Position::NormalizeOrientation(angle);

        // Only update if significant change
        float currentAngle = bot->GetOrientation();
        float diff = std::abs(angle - currentAngle);
        if (diff > 0.1f && diff < (2 * M_PI - 0.1f))
        {
            bot->SetFacingTo(angle);

            // Send orientation update packet
            WorldPackets::Movement::MoveSetFacing packet;
            packet.MoverGUID = bot->GetGUID();
            packet.Angle = angle;
            bot->SendMessageToSet(packet.Write(), false);
        }
    }

    void MovementGenerator::StopMovement(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return;

        // Stop spline movement
        bot->StopMoving();

        // Clear movement flags
        bot->RemoveUnitMovementFlag(MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD |
                                   MOVEMENTFLAG_STRAFE_LEFT | MOVEMENTFLAG_STRAFE_RIGHT);

        // Send stop packet
        Movement::MoveSplineInit init(bot);
        init.Stop();
        init.Launch();

        // Update state
        _state.isMoving = false;
        _state.currentSpeed = 0.0f;
        _hasReached.store(true);
    }

    bool MovementGenerator::ShouldUpdate()
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastUpdate).count();

        if (elapsed >= _updateInterval)
        {
            _lastUpdate = now;
            return true;
        }

        return false;
    }

    float MovementGenerator::GetDistanceToTarget(Player* bot, Position const& target) const
    {
        if (!bot)
            return 0.0f;

        return bot->GetExactDist(&target);
    }

    float MovementGenerator::GetDistanceToTarget(Player* bot, Unit* target) const
    {
        if (!bot || !target)
            return 0.0f;

        return bot->GetExactDist(target);
    }

    bool MovementGenerator::IsStuck(Player* bot, Position const& currentPos)
    {
        if (!bot || !_state.isMoving)
            return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - _state.lastStuckCheck).count();

        if (elapsed < MovementConstants::STUCK_CHECK_INTERVAL)
            return false;

        _state.lastStuckCheck = now;

        // Check if position hasn't changed significantly
        float distance = _lastPosition.GetExactDist(&currentPos);
        if (distance < MovementConstants::STUCK_THRESHOLD && _state.isMoving)
        {
            _state.stuckCounter++;
            TC_LOG_DEBUG("playerbot.movement", "Potential stuck detected for bot %s (counter: %u)",
                bot->GetName().c_str(), _state.stuckCounter);

            if (_state.stuckCounter >= MovementConstants::MAX_STUCK_COUNTER)
            {
                _metrics.stuckDetections++;
                return true;
            }
        }
        else
        {
            // Reset counter if we've moved
            if (_state.stuckCounter > 0)
                _state.stuckCounter = 0;
        }

        _lastPosition = currentPos;
        return false;
    }

    void MovementGenerator::HandleStuck(Player* bot)
    {
        if (!bot)
            return;

        TC_LOG_DEBUG("playerbot.movement", "Handling stuck situation for bot %s", bot->GetName().c_str());

        // Stop current movement
        StopMovement(bot);

        // Try to move in a random direction to unstuck
        float angle = frand(0.0f, 2 * M_PI);
        float distance = 5.0f;

        Position unstuckPos;
        bot->GetNearPosition(unstuckPos, distance, angle);

        // Validate the unstuck position
        if (ValidateDestination(bot, unstuckPos))
        {
            SendMovementPacket(bot, unstuckPos, bot->GetSpeed(MOVE_RUN));
        }

        // Reset stuck counter and mark for path recalculation
        _state.stuckCounter = 0;
        _state.needsRecalc = true;
        _needsPath.store(true);
        _metrics.recalculations++;
    }

    void MovementGenerator::UpdateMetrics(uint32 cpuMicros, uint32 pathNodes)
    {
        _metrics.totalCpuMicros += cpuMicros;
        _metrics.totalPathNodes += pathNodes;

        // Update CPU throttling if needed
        float cpuPercent = static_cast<float>(cpuMicros) / 1000000.0f * 100.0f;
        if (cpuPercent > MovementConstants::CPU_THROTTLE_THRESHOLD)
        {
            // Increase update interval to reduce CPU usage
            _updateInterval = std::min(_updateInterval * 2,
                MovementConstants::UPDATE_INTERVAL_FAR);

            TC_LOG_DEBUG("playerbot.movement", "CPU throttling activated, new interval: %u ms",
                _updateInterval);
        }
    }

    MotionMaster* MovementGenerator::GetMotionMaster(Player* bot) const
    {
        if (!bot)
            return nullptr;

        return bot->GetMotionMaster();
    }

    bool MovementGenerator::ValidateDestination(Player* bot, Position const& destination) const
    {
        if (!bot || !bot->GetMap())
            return false;

        // Check if position is valid in the map
        if (!bot->GetMap()->IsInLineOfSight(bot->GetPositionX(), bot->GetPositionY(),
            bot->GetPositionZ() + 2.0f, destination.GetPositionX(),
            destination.GetPositionY(), destination.GetPositionZ() + 2.0f,
            bot->GetPhaseShift(), LINEOFSIGHT_ALL_CHECKS))
        {
            TC_LOG_DEBUG("playerbot.movement", "Destination failed LOS check");
            return false;
        }

        // Check if destination is in valid terrain (not in void or unreachable)
        float groundZ = bot->GetMap()->GetHeight(bot->GetPhaseShift(),
            destination.GetPositionX(), destination.GetPositionY(),
            destination.GetPositionZ() + 2.0f, true, 100.0f);

        if (groundZ < -500.0f) // Void/invalid terrain
        {
            TC_LOG_DEBUG("playerbot.movement", "Destination in void/invalid terrain");
            return false;
        }

        // Check if destination is too far below or above ground
        float heightDiff = std::abs(destination.GetPositionZ() - groundZ);
        if (heightDiff > 50.0f && !bot->CanFly())
        {
            TC_LOG_DEBUG("playerbot.movement", "Destination height difference too large: %.2f",
                heightDiff);
            return false;
        }

        return true;
    }

    void MovementGenerator::ApplyMovementFlags(Player* bot, TerrainType terrain)
    {
        if (!bot)
            return;

        // Clear existing movement type flags
        bot->RemoveUnitMovementFlag(MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_FLYING);

        // Apply appropriate flags based on terrain
        if (terrain & TerrainType::TERRAIN_WATER)
        {
            if (bot->CanSwim())
                bot->AddUnitMovementFlag(MOVEMENTFLAG_SWIMMING);
        }
        else if (terrain & TerrainType::TERRAIN_AIR)
        {
            if (bot->CanFly())
                bot->AddUnitMovementFlag(MOVEMENTFLAG_FLYING);
        }

        // Handle special terrain types
        if (terrain & TerrainType::TERRAIN_LAVA)
        {
            // Increase movement speed if has fire resistance
            // This would require checking auras/resistances
        }
        else if (terrain & TerrainType::TERRAIN_SLIME)
        {
            // Apply slow effect if not immune
            // This would require checking immunities
        }
    }
}