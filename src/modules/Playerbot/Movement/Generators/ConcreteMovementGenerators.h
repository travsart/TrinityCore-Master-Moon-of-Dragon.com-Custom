/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_PLAYERBOT_CONCRETE_MOVEMENT_GENERATORS_H
#define TRINITY_PLAYERBOT_CONCRETE_MOVEMENT_GENERATORS_H

#include "../Core/MovementGenerator.h"
#include "Player.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectDefines.h"

namespace Playerbot
{
    /**
     * @class PointMovementGenerator
     * @brief Moves bot to a specific point
     */
    class PointMovementGenerator : public MovementGenerator
    {
    public:
        PointMovementGenerator(Position const& destination, MovementPriority priority = MovementPriority::PRIORITY_NORMAL)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_POINT, priority)
            , _destination(destination)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld())
                return false;

            _isActive.store(true);
            _hasReached.store(false);
            _initTime = std::chrono::steady_clock::now();
            _lastPosition = bot->GetPosition();

            TC_LOG_DEBUG("playerbot.movement", "PointMovementGenerator: Initialized for bot %s to position (%.2f, %.2f, %.2f)",
                bot->GetName().c_str(), _destination.GetPositionX(), _destination.GetPositionY(), _destination.GetPositionZ());

            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _hasReached.store(false);
            _needsPath.store(true);
            _lastPosition = bot->GetPosition();
            _state.Reset();

            TC_LOG_DEBUG("playerbot.movement", "PointMovementGenerator: Reset for bot %s", bot->GetName().c_str());
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load())
                return MovementResult::MOVEMENT_FAILED;

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            // Check if reached destination
            float distance = bot->GetExactDist(&_destination);
            if (distance < MovementConstants::REACHED_THRESHOLD)
            {
                _hasReached.store(true);
                _state.isMoving = false;
                StopMovement(bot);
                return MovementResult::MOVEMENT_SUCCESS;
            }

            // Check if stuck
            if (IsStuck(bot, bot->GetPosition()))
            {
                HandleStuck(bot);
                return MovementResult::MOVEMENT_STUCK;
            }

            // Send movement command
            Movement::MoveSplineInit init(bot);
            init.MoveTo(_destination.GetPositionX(), _destination.GetPositionY(), _destination.GetPositionZ());
            init.SetWalk(false);
            init.Launch();

            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_POINT;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;

            if (!interrupted && !_hasReached.load())
            {
                StopMovement(bot);
            }

            TC_LOG_DEBUG("playerbot.movement", "PointMovementGenerator: Finalized for bot %s (interrupted: %s)",
                bot->GetName().c_str(), interrupted ? "yes" : "no");
        }

    private:
        Position _destination;
    };

    /**
     * @class FollowMovementGenerator
     * @brief Makes bot follow a unit
     */
    class FollowMovementGenerator : public MovementGenerator
    {
    public:
        FollowMovementGenerator(ObjectGuid targetGuid, float minDist, float maxDist, float angle = 0.0f,
                               MovementPriority priority = MovementPriority::PRIORITY_NORMAL)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_FOLLOW, priority)
            , _targetGuid(targetGuid)
            , _minDistance(minDist)
            , _maxDistance(maxDist)
            , _angle(angle)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld())
                return false;

            _isActive.store(true);
            _initTime = std::chrono::steady_clock::now();
            _lastPosition = bot->GetPosition();

            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _needsPath.store(true);
            _lastPosition = bot->GetPosition();
            _state.Reset();
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load())
                return MovementResult::MOVEMENT_FAILED;

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            // Get target
            Unit* target = ObjectAccessor::GetUnit(*bot, _targetGuid);
            if (!target || !target->IsInWorld())
            {
                return MovementResult::MOVEMENT_FAILED;
            }

            // Check distance
            float distance = bot->GetExactDist(target);
            if (distance >= _minDistance && distance <= _maxDistance)
            {
                // Within acceptable range
                _state.isMoving = false;
                return MovementResult::MOVEMENT_SUCCESS;
            }

            // Calculate follow position
            Position followPos = target->GetPosition();
            if (_angle != 0.0f)
            {
                float targetAngle = target->GetOrientation() + _angle;
                followPos.m_positionX += _minDistance * cos(targetAngle);
                followPos.m_positionY += _minDistance * sin(targetAngle);
            }

            // Move to follow position
            Movement::MoveSplineInit init(bot);
            init.MoveTo(followPos.GetPositionX(), followPos.GetPositionY(), followPos.GetPositionZ());
            init.SetWalk(false);
            init.Launch();

            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_FOLLOW;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;
            StopMovement(bot);
        }

        void OnTargetMoved(Player* bot, Position const& newPosition) override
        {
            _needsPath.store(true);
        }

    private:
        ObjectGuid _targetGuid;
        float _minDistance;
        float _maxDistance;
        float _angle;
    };

    /**
     * @class FleeMovementGenerator
     * @brief Makes bot flee from a threat
     */
    class FleeMovementGenerator : public MovementGenerator
    {
    public:
        FleeMovementGenerator(ObjectGuid threatGuid, float distance,
                             MovementPriority priority = MovementPriority::PRIORITY_FLEE)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_FLEE, priority)
            , _threatGuid(threatGuid)
            , _fleeDistance(distance)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld())
                return false;

            _isActive.store(true);
            _initTime = std::chrono::steady_clock::now();
            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _needsPath.store(true);
            _state.Reset();
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load())
                return MovementResult::MOVEMENT_FAILED;

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            Unit* threat = ObjectAccessor::GetUnit(*bot, _threatGuid);
            if (!threat)
                return MovementResult::MOVEMENT_FAILED;

            float distance = bot->GetExactDist(threat);
            if (distance >= _fleeDistance)
            {
                _hasReached.store(true);
                _state.isMoving = false;
                return MovementResult::MOVEMENT_SUCCESS;
            }

            // Calculate flee position (opposite direction from threat)
            float angle = bot->GetAbsoluteAngle(threat) + M_PI; // Opposite direction
            Position fleePos;
            bot->GetNearPoint(bot, fleePos.m_positionX, fleePos.m_positionY, fleePos.m_positionZ,
                             _fleeDistance, angle);

            // Move away
            Movement::MoveSplineInit init(bot);
            init.MoveTo(fleePos.GetPositionX(), fleePos.GetPositionY(), fleePos.GetPositionZ());
            init.SetWalk(false);
            init.Launch();

            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_FLEE;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;
            StopMovement(bot);
        }

    private:
        ObjectGuid _threatGuid;
        float _fleeDistance;
    };

    /**
     * @class ChaseMovementGenerator
     * @brief Makes bot chase a target
     */
    class ChaseMovementGenerator : public MovementGenerator
    {
    public:
        ChaseMovementGenerator(ObjectGuid targetGuid, float range = 0.0f, float angle = 0.0f,
                              MovementPriority priority = MovementPriority::PRIORITY_COMBAT)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_CHASE, priority)
            , _targetGuid(targetGuid)
            , _range(range)
            , _angle(angle)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld())
                return false;

            _isActive.store(true);
            _initTime = std::chrono::steady_clock::now();
            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _needsPath.store(true);
            _state.Reset();
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load())
                return MovementResult::MOVEMENT_FAILED;

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            Unit* target = ObjectAccessor::GetUnit(*bot, _targetGuid);
            if (!target)
                return MovementResult::MOVEMENT_FAILED;

            float distance = bot->GetExactDist(target);
            float effectiveRange = _range > 0.0f ? _range : bot->GetCombatReach() + target->GetCombatReach();

            if (distance <= effectiveRange)
            {
                _state.isMoving = false;
                StopMovement(bot);
                return MovementResult::MOVEMENT_SUCCESS;
            }

            // Move to chase position
            bot->GetMotionMaster()->MoveChase(target, effectiveRange, _angle);
            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_CHASE;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;
        }

    private:
        ObjectGuid _targetGuid;
        float _range;
        float _angle;
    };

    /**
     * @class RandomMovementGenerator
     * @brief Makes bot wander around randomly
     */
    class RandomMovementGenerator : public MovementGenerator
    {
    public:
        RandomMovementGenerator(float radius, uint32 duration = 0,
                               MovementPriority priority = MovementPriority::PRIORITY_NORMAL)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_RANDOM, priority)
            , _radius(radius)
            , _duration(duration)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld())
                return false;

            _isActive.store(true);
            _initTime = std::chrono::steady_clock::now();
            _centerPosition = bot->GetPosition();
            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _needsPath.store(true);
            _state.Reset();
            _centerPosition = bot->GetPosition();
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load())
                return MovementResult::MOVEMENT_FAILED;

            // Check duration
            if (_duration > 0)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - _initTime).count();
                if (elapsed >= _duration)
                {
                    return MovementResult::MOVEMENT_SUCCESS;
                }
            }

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            // Generate random position within radius
            float angle = frand(0.0f, 2.0f * M_PI);
            float distance = frand(0.0f, _radius);
            Position randomPos;
            randomPos.m_positionX = _centerPosition.GetPositionX() + distance * cos(angle);
            randomPos.m_positionY = _centerPosition.GetPositionY() + distance * sin(angle);
            randomPos.m_positionZ = _centerPosition.GetPositionZ();

            // Move to random position
            Movement::MoveSplineInit init(bot);
            init.MoveTo(randomPos.GetPositionX(), randomPos.GetPositionY(), randomPos.GetPositionZ());
            init.SetWalk(true);
            init.Launch();

            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_RANDOM;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;
            StopMovement(bot);
        }

    private:
        float _radius;
        uint32 _duration;
        Position _centerPosition;
    };

    /**
     * @class FormationMovementGenerator
     * @brief Moves bot in formation with leader
     */
    class FormationMovementGenerator : public MovementGenerator
    {
    public:
        FormationMovementGenerator(ObjectGuid leaderGuid, FormationPosition const& formationPos,
                                  MovementPriority priority = MovementPriority::PRIORITY_NORMAL)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_FORMATION, priority)
            , _leaderGuid(leaderGuid)
            , _formationPosition(formationPos)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld())
                return false;

            _isActive.store(true);
            _initTime = std::chrono::steady_clock::now();
            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _needsPath.store(true);
            _state.Reset();
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load())
                return MovementResult::MOVEMENT_FAILED;

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            Unit* leader = ObjectAccessor::GetUnit(*bot, _leaderGuid);
            if (!leader)
                return MovementResult::MOVEMENT_FAILED;

            // Calculate formation position
            Position leaderPos = leader->GetPosition();
            float angle = leaderPos.GetOrientation() + _formationPosition.followAngle;
            float x = leaderPos.GetPositionX() + _formationPosition.followDistance * cos(angle);
            float y = leaderPos.GetPositionY() + _formationPosition.followDistance * sin(angle);
            float z = leaderPos.GetPositionZ();

            Position formationPos(x, y, z, angle);

            // Check if in position
            if (bot->GetExactDist(&formationPos) < MovementConstants::REACHED_THRESHOLD)
            {
                _state.isMoving = false;
                return MovementResult::MOVEMENT_SUCCESS;
            }

            // Move to formation position
            Movement::MoveSplineInit init(bot);
            init.MoveTo(formationPos.GetPositionX(), formationPos.GetPositionY(), formationPos.GetPositionZ());
            init.SetWalk(false);
            init.Launch();

            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_FORMATION;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;
        }

    private:
        ObjectGuid _leaderGuid;
        FormationPosition _formationPosition;
    };

    /**
     * @class PatrolMovementGenerator
     * @brief Makes bot patrol along waypoints
     */
    class PatrolMovementGenerator : public MovementGenerator
    {
    public:
        PatrolMovementGenerator(std::vector<Position> const& waypoints, bool cyclic = true,
                               MovementPriority priority = MovementPriority::PRIORITY_NORMAL)
            : MovementGenerator(MovementGeneratorType::MOVEMENT_PATROL, priority)
            , _waypoints(waypoints)
            , _cyclic(cyclic)
            , _currentWaypoint(0)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot || !bot->IsInWorld() || _waypoints.empty())
                return false;

            _isActive.store(true);
            _initTime = std::chrono::steady_clock::now();
            _currentWaypoint = 0;
            return true;
        }

        void Reset(Player* bot) override
        {
            if (!bot)
                return;

            _needsPath.store(true);
            _state.Reset();
            _currentWaypoint = 0;
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            if (!bot || !_isActive.load() || _waypoints.empty())
                return MovementResult::MOVEMENT_FAILED;

            if (!ShouldUpdate())
                return MovementResult::MOVEMENT_IN_PROGRESS;

            // Check if reached current waypoint
            Position const& targetWP = _waypoints[_currentWaypoint];
            if (bot->GetExactDist(&targetWP) < MovementConstants::REACHED_THRESHOLD)
            {
                // Move to next waypoint
                _currentWaypoint++;
                if (_currentWaypoint >= _waypoints.size())
                {
                    if (_cyclic)
                    {
                        _currentWaypoint = 0;
                    }
                    else
                    {
                        _hasReached.store(true);
                        _state.isMoving = false;
                        return MovementResult::MOVEMENT_SUCCESS;
                    }
                }
            }

            // Move to current waypoint
            Position const& wp = _waypoints[_currentWaypoint];
            Movement::MoveSplineInit init(bot);
            init.MoveTo(wp.GetPositionX(), wp.GetPositionY(), wp.GetPositionZ());
            init.SetWalk(false);
            init.Launch();

            _state.isMoving = true;
            _state.currentType = MovementGeneratorType::MOVEMENT_PATROL;

            return MovementResult::MOVEMENT_IN_PROGRESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            if (!bot)
                return;

            _isActive.store(false);
            _state.isMoving = false;
            StopMovement(bot);
        }

    private:
        std::vector<Position> _waypoints;
        bool _cyclic;
        uint32 _currentWaypoint;
    };

    /**
     * @class IdleMovementGenerator
     * @brief Default generator when bot is not moving
     */
    class IdleMovementGenerator : public MovementGenerator
    {
    public:
        IdleMovementGenerator()
            : MovementGenerator(MovementGeneratorType::MOVEMENT_IDLE, MovementPriority::PRIORITY_NONE)
        {
        }

        bool Initialize(Player* bot) override
        {
            if (!bot)
                return false;

            _isActive.store(true);
            _state.isMoving = false;
            return true;
        }

        void Reset(Player* bot) override
        {
            _state.Reset();
            _state.isMoving = false;
        }

        MovementResult Update(Player* bot, uint32 diff) override
        {
            // Idle generator does nothing
            return MovementResult::MOVEMENT_SUCCESS;
        }

        void Finalize(Player* bot, bool interrupted) override
        {
            _isActive.store(false);
        }
    };
}

#endif // TRINITY_PLAYERBOT_CONCRETE_MOVEMENT_GENERATORS_H
