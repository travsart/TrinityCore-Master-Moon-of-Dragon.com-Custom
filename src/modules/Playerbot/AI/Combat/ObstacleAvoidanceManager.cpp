/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ObstacleAvoidanceManager.h"
#include "Player.h"
#include "Unit.h"
#include "GameObject.h"
#include "Map.h"
#include "Log.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Creature.h"
#include "Pet.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

ObstacleAvoidanceManager::ObstacleAvoidanceManager(Player* bot)
    : _bot(bot), _scanRadius(DEFAULT_SCAN_RADIUS), _lookaheadTime(DEFAULT_LOOKAHEAD_TIME),
      _updateInterval(DEFAULT_UPDATE_INTERVAL), _lastUpdate(0),
      _collisionTolerance(DEFAULT_COLLISION_TOLERANCE), _predictiveAvoidance(true),
      _emergencyMode(false), _lastCleanup(0), _lastCacheCleanup(0)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "ObstacleAvoidanceManager: Bot player is null!");
        return;
    }

    TC_LOG_DEBUG("playerbot.obstacle", "ObstacleAvoidanceManager initialized for bot {}", _bot->GetName());
}

void ObstacleAvoidanceManager::UpdateObstacleDetection(const DetectionContext& context)
{
    auto startTime = std::chrono::steady_clock::now();

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _lastUpdate < _updateInterval && !context.emergencyMode)
            return;

        _lastUpdate = currentTime;

        std::vector<ObstacleInfo> detectedObstacles = ScanForObstacles(context);

        for (const ObstacleInfo& obstacle : detectedObstacles)
        {
            auto existingIt = _obstacles.find(obstacle.guid);
            if (existingIt != _obstacles.end())
            {
                UpdateObstacle(obstacle);
            }
            else
            {
                RegisterObstacle(obstacle);
            }
        }

        CleanupExpiredObstacles();

        if (_predictiveAvoidance)
        {
            UpdateObstaclePredictions();
        }

        _metrics.obstaclesDetected += static_cast<uint32>(detectedObstacles.size());

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        TrackPerformance(duration, "UpdateObstacleDetection");

        TC_LOG_TRACE("playerbot.obstacle", "Bot {} detected {} obstacles in {}μs",
                   _bot->GetName(), detectedObstacles.size(), duration.count());
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.obstacle", "Exception in UpdateObstacleDetection for bot {}: {}", _bot->GetName(), e.what());
    }
}

std::vector<CollisionPrediction> ObstacleAvoidanceManager::PredictCollisions(const DetectionContext& context)
{
    std::vector<CollisionPrediction> predictions;
    predictions.reserve(_obstacles.size());

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    for (const auto& [guid, obstacle] : _obstacles)
    {
        if (ShouldIgnoreObstacle(obstacle, context))
            continue;

        CollisionPrediction prediction = PredictCollisionWithObstacle(obstacle, context);
        if (prediction.willCollide)
        {
            predictions.push_back(prediction);
        }
    }

    std::sort(predictions.begin(), predictions.end(),
        [](const CollisionPrediction& a, const CollisionPrediction& b) {
            return a.timeToCollision < b.timeToCollision;
        });

    return predictions;
}

std::vector<AvoidanceManeuver> ObstacleAvoidanceManager::GenerateAvoidanceManeuvers(const CollisionPrediction& collision)
{
    std::vector<AvoidanceManeuver> maneuvers;

    if (!collision.willCollide || !collision.obstacle)
        return maneuvers;

    switch (collision.obstacle->type)
    {
        case ObstacleType::STATIC_TERRAIN:
        case ObstacleType::ENVIRONMENTAL:
            maneuvers.push_back(GenerateDirectAvoidance(collision));
            maneuvers.push_back(GenerateCircumnavigation(collision));
            if (collision.obstacle->height <= 3.0f)
                maneuvers.push_back(GenerateJumpOver(collision));
            break;

        case ObstacleType::DYNAMIC_OBJECT:
        case ObstacleType::UNIT_OBSTACLE:
            maneuvers.push_back(GenerateWaitAndPass(collision));
            maneuvers.push_back(GenerateDirectAvoidance(collision));
            maneuvers.push_back(GenerateCircumnavigation(collision));
            break;

        case ObstacleType::TEMPORARY_HAZARD:
            maneuvers.push_back(GenerateDirectAvoidance(collision));
            maneuvers.push_back(GenerateWaitAndPass(collision));
            maneuvers.push_back(GenerateBacktrack(collision));
            break;

        case ObstacleType::PROJECTILE:
            maneuvers.push_back(GenerateDirectAvoidance(collision));
            if (collision.timeToCollision < 1.0f)
            {
                AvoidanceManeuver emergency;
                emergency.behavior = AvoidanceBehavior::EMERGENCY_STOP;
                emergency.priority = 0;
                emergency.successProbability = 0.9f;
                emergency.description = "Emergency stop for projectile";
                maneuvers.insert(maneuvers.begin(), emergency);
            }
            break;

        default:
            maneuvers.push_back(GenerateDirectAvoidance(collision));
            break;
    }

    if (!maneuvers.empty() && !_bot->GetGroup())
    {
        std::vector<AvoidanceManeuver> formationManeuvers = GenerateFormationAwareAvoidance(collision);
        maneuvers.insert(maneuvers.end(), formationManeuvers.begin(), formationManeuvers.end());
    }

    std::sort(maneuvers.begin(), maneuvers.end());

    return maneuvers;
}

bool ObstacleAvoidanceManager::ExecuteAvoidanceManeuver(const AvoidanceManeuver& maneuver)
{
    if (maneuver.waypoints.empty())
        return false;

    try
    {
        switch (maneuver.behavior)
        {
            case AvoidanceBehavior::EMERGENCY_STOP:
                ExecuteEmergencyStop();
                _metrics.emergencyStops++;
                break;

            case AvoidanceBehavior::DIRECT_AVOIDANCE:
            case AvoidanceBehavior::CIRCUMNAVIGATE:
            case AvoidanceBehavior::FIND_ALTERNATIVE:
                {
                    Position targetPos = maneuver.waypoints.back();
                    _bot->GetMotionMaster()->MovePoint(0, targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ());
                }
                break;

            case AvoidanceBehavior::WAIT_AND_PASS:
                _bot->GetMotionMaster()->Clear();
                break;

            case AvoidanceBehavior::JUMP_OVER:
                if (maneuver.requiresJump)
                {
                    Position jumpTarget = maneuver.waypoints.back();
                    _bot->GetMotionMaster()->MoveJump(jumpTarget.GetPositionX(), jumpTarget.GetPositionY(), jumpTarget.GetPositionZ(), 10.0f, 10.0f);
                }
                break;

            case AvoidanceBehavior::BACKTRACK:
                if (maneuver.waypoints.size() >= 2)
                {
                    Position backtrackPos = maneuver.waypoints[0];
                    _bot->GetMotionMaster()->MovePoint(0, backtrackPos.GetPositionX(), backtrackPos.GetPositionY(), backtrackPos.GetPositionZ());
                }
                break;

            default:
                return false;
        }

        _metrics.avoidanceManeuvers++;
        _metrics.collisionsPrevented++;

        TC_LOG_DEBUG("playerbot.obstacle", "Bot {} executed avoidance maneuver: {}",
                   _bot->GetName(), maneuver.description);

        return true;
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.obstacle", "Exception executing avoidance maneuver for bot {}: {}", _bot->GetName(), e.what());
        return false;
    }
}

std::vector<ObstacleInfo> ObstacleAvoidanceManager::ScanForObstacles(const DetectionContext& context)
{
    std::vector<ObstacleInfo> obstacles;

    if (context.flags & DetectionFlags::TERRAIN)
        ScanTerrain(context, obstacles);

    if (context.flags & DetectionFlags::UNITS)
        ScanUnits(context, obstacles);

    if (context.flags & DetectionFlags::OBJECTS)
        ScanGameObjects(context, obstacles);

    if (context.flags & DetectionFlags::HAZARDS)
        ScanEnvironmentalHazards(context, obstacles);

    return obstacles;
}

std::vector<ObstacleInfo> ObstacleAvoidanceManager::DetectUnitObstacles(const DetectionContext& context)
{
    std::vector<ObstacleInfo> unitObstacles;

    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, context.scanRadius);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, context.scanRadius);

    for (Unit* unit : nearbyUnits)
    {
        if (!unit || unit == _bot || !unit->IsInWorld())
            continue;

        if (unit->IsAlive() && !unit->HasUnitState(UNIT_STATE_UNATTACKABLE))
        {
            ObstacleInfo obstacle;
            obstacle.guid = unit->GetGUID();
            obstacle.object = unit;
            obstacle.position = unit->GetPosition();
            obstacle.type = ObstacleType::UNIT_OBSTACLE;
            obstacle.radius = CalculateObstacleRadius(unit, ObstacleType::UNIT_OBSTACLE);
            obstacle.height = unit->GetCollisionHeight();
            obstacle.isMoving = unit->IsMoving();
            obstacle.priority = AssessObstaclePriority(obstacle, context);
            obstacle.name = unit->GetName();
            obstacle.firstDetected = getMSTime();
            obstacle.lastSeen = obstacle.firstDetected;

            if (obstacle.isMoving)
            {
                obstacle.velocity.m_positionX = unit->GetSpeedXY() * std::cos(unit->GetOrientation());
                obstacle.velocity.m_positionY = unit->GetSpeedXY() * std::sin(unit->GetOrientation());
                obstacle.velocity.m_positionZ = 0.0f;
            }

            obstacle.predictedPosition = PredictObstaclePosition(obstacle, context.lookaheadTime);
            obstacle.avoidanceRadius = ObstacleUtils::CalculateAvoidanceRadius(obstacle.radius, GetBotRadius());

            unitObstacles.push_back(obstacle);
        }
    }

    return unitObstacles;
}

CollisionPrediction ObstacleAvoidanceManager::PredictCollisionWithObstacle(const ObstacleInfo& obstacle, const DetectionContext& context)
{
    CollisionPrediction prediction;
    prediction.obstacle = const_cast<ObstacleInfo*>(&obstacle);

    Position botPos = context.currentPosition;
    Position botVel = context.velocity;
    Position obstaclePos = obstacle.position;
    Position obstacleVel = obstacle.velocity;

    if (obstacle.isMoving)
    {
        float timeToCollision = CalculateTimeToCollision(obstacle, context);
        if (timeToCollision > 0.0f && timeToCollision <= context.lookaheadTime)
        {
            prediction.willCollide = true;
            prediction.timeToCollision = timeToCollision;
            prediction.collisionPoint = ObstacleUtils::PredictPosition(botPos, botVel, timeToCollision);

            if (timeToCollision <= 1.0f)
                prediction.collisionType = CollisionType::IMMINENT;
            else if (timeToCollision <= 3.0f)
                prediction.collisionType = CollisionType::NEAR;
            else
                prediction.collisionType = CollisionType::DISTANT;

            prediction.collisionSeverity = 1.0f - (timeToCollision / context.lookaheadTime);
        }
    }
    else
    {
        float distanceToObstacle = botPos.GetExactDist(&obstaclePos);
        float combinedRadius = obstacle.radius + GetBotRadius() + _collisionTolerance;

        if (distanceToObstacle <= combinedRadius)
        {
            prediction.willCollide = true;
            prediction.timeToCollision = 0.0f;
            prediction.collisionType = CollisionType::IMMINENT;
            prediction.collisionPoint = obstaclePos;
            prediction.collisionSeverity = 1.0f;
        }
        else
        {
            Position direction = context.targetPosition;
            direction.m_positionX -= botPos.GetPositionX();
            direction.m_positionY -= botPos.GetPositionY();
            direction.m_positionZ -= botPos.GetPositionZ();

            float length = std::sqrt(direction.m_positionX * direction.m_positionX +
                                   direction.m_positionY * direction.m_positionY +
                                   direction.m_positionZ * direction.m_positionZ);

            if (length > 0.0f)
            {
                direction.m_positionX /= length;
                direction.m_positionY /= length;
                direction.m_positionZ /= length;

                float distanceToPath = ObstacleUtils::DistancePointToLine(obstaclePos, botPos, context.targetPosition);
                if (distanceToPath <= combinedRadius)
                {
                    prediction.willCollide = true;
                    prediction.timeToCollision = distanceToObstacle / GetBotSpeed();
                    prediction.collisionType = CollisionType::POTENTIAL;
                    prediction.collisionPoint = ObstacleUtils::ClosestPointOnLine(obstaclePos, botPos, context.targetPosition);
                    prediction.collisionSeverity = 0.5f;
                }
            }
        }
    }

    if (prediction.willCollide)
    {
        switch (obstacle.type)
        {
            case ObstacleType::STATIC_TERRAIN:
                prediction.recommendedAction = AvoidanceBehavior::CIRCUMNAVIGATE;
                break;
            case ObstacleType::UNIT_OBSTACLE:
                prediction.recommendedAction = obstacle.isMoving ? AvoidanceBehavior::WAIT_AND_PASS : AvoidanceBehavior::DIRECT_AVOIDANCE;
                break;
            case ObstacleType::TEMPORARY_HAZARD:
                prediction.recommendedAction = AvoidanceBehavior::DIRECT_AVOIDANCE;
                break;
            case ObstacleType::PROJECTILE:
                prediction.recommendedAction = prediction.timeToCollision < 1.0f ? AvoidanceBehavior::EMERGENCY_STOP : AvoidanceBehavior::DIRECT_AVOIDANCE;
                break;
            default:
                prediction.recommendedAction = AvoidanceBehavior::DIRECT_AVOIDANCE;
                break;
        }
    }

    return prediction;
}

AvoidanceManeuver ObstacleAvoidanceManager::GenerateDirectAvoidance(const CollisionPrediction& collision)
{
    AvoidanceManeuver maneuver;
    maneuver.behavior = AvoidanceBehavior::DIRECT_AVOIDANCE;
    maneuver.priority = static_cast<uint32>(collision.obstacle->priority);

    Position botPos = _bot->GetPosition();
    Position obstaclePos = collision.obstacle->position;
    float avoidanceRadius = collision.obstacle->avoidanceRadius;

    float angle = std::atan2(obstaclePos.GetPositionY() - botPos.GetPositionY(),
                           obstaclePos.GetPositionX() - botPos.GetPositionX());

    float leftAngle = angle + M_PI/2;
    float rightAngle = angle - M_PI/2;

    Position leftAvoidance;
    leftAvoidance.m_positionX = obstaclePos.GetPositionX() + avoidanceRadius * std::cos(leftAngle);
    leftAvoidance.m_positionY = obstaclePos.GetPositionY() + avoidanceRadius * std::sin(leftAngle);
    leftAvoidance.m_positionZ = obstaclePos.GetPositionZ();

    Position rightAvoidance;
    rightAvoidance.m_positionX = obstaclePos.GetPositionX() + avoidanceRadius * std::cos(rightAngle);
    rightAvoidance.m_positionY = obstaclePos.GetPositionY() + avoidanceRadius * std::sin(rightAngle);
    rightAvoidance.m_positionZ = obstaclePos.GetPositionZ();

    float leftDistance = botPos.GetExactDist(&leftAvoidance);
    float rightDistance = botPos.GetExactDist(&rightAvoidance);

    Position chosenAvoidance = (leftDistance <= rightDistance) ? leftAvoidance : rightAvoidance;

    maneuver.waypoints.push_back(botPos);
    maneuver.waypoints.push_back(chosenAvoidance);
    maneuver.executionTime = leftDistance / GetBotSpeed();
    maneuver.successProbability = 0.8f;
    maneuver.energyCost = leftDistance * 0.1f;
    maneuver.description = "Direct avoidance around obstacle";

    return maneuver;
}

AvoidanceManeuver ObstacleAvoidanceManager::GenerateCircumnavigation(const CollisionPrediction& collision)
{
    AvoidanceManeuver maneuver;
    maneuver.behavior = AvoidanceBehavior::CIRCUMNAVIGATE;
    maneuver.priority = static_cast<uint32>(collision.obstacle->priority) + 1;

    Position botPos = _bot->GetPosition();
    Position obstaclePos = collision.obstacle->position;
    float radius = collision.obstacle->avoidanceRadius * 1.5f;

    std::vector<Position> circumWaypoints;
    circumWaypoints.push_back(botPos);

    for (int i = 1; i <= 4; ++i)
    {
        float angle = (2.0f * M_PI * i) / 4.0f;
        Position waypoint;
        waypoint.m_positionX = obstaclePos.GetPositionX() + radius * std::cos(angle);
        waypoint.m_positionY = obstaclePos.GetPositionY() + radius * std::sin(angle);
        waypoint.m_positionZ = obstaclePos.GetPositionZ();
        circumWaypoints.push_back(waypoint);
    }

    maneuver.waypoints = circumWaypoints;
    maneuver.executionTime = (radius * M_PI * 2) / GetBotSpeed();
    maneuver.successProbability = 0.9f;
    maneuver.energyCost = maneuver.executionTime * 0.2f;
    maneuver.description = "Circumnavigate around obstacle";

    return maneuver;
}

AvoidanceManeuver ObstacleAvoidanceManager::GenerateWaitAndPass(const CollisionPrediction& collision)
{
    AvoidanceManeuver maneuver;
    maneuver.behavior = AvoidanceBehavior::WAIT_AND_PASS;
    maneuver.priority = static_cast<uint32>(collision.obstacle->priority) + 2;

    float estimatedClearanceTime = EstimateObstacleClearanceTime(*collision.obstacle);

    maneuver.waypoints.push_back(_bot->GetPosition());
    maneuver.executionTime = estimatedClearanceTime;
    maneuver.successProbability = collision.obstacle->isMoving ? 0.7f : 0.3f;
    maneuver.energyCost = 0.0f;
    maneuver.description = "Wait for obstacle to pass";

    return maneuver;
}

AvoidanceManeuver ObstacleAvoidanceManager::GenerateJumpOver(const CollisionPrediction& collision)
{
    AvoidanceManeuver maneuver;
    maneuver.behavior = AvoidanceBehavior::JUMP_OVER;
    maneuver.priority = static_cast<uint32>(collision.obstacle->priority);

    if (collision.obstacle->height > 3.0f)
    {
        maneuver.successProbability = 0.0f;
        return maneuver;
    }

    Position botPos = _bot->GetPosition();
    Position obstaclePos = collision.obstacle->position;

    float jumpDistance = collision.obstacle->radius * 2.0f + 2.0f;
    float angle = std::atan2(obstaclePos.GetPositionY() - botPos.GetPositionY(),
                           obstaclePos.GetPositionX() - botPos.GetPositionX());

    Position jumpTarget;
    jumpTarget.m_positionX = obstaclePos.GetPositionX() + jumpDistance * std::cos(angle);
    jumpTarget.m_positionY = obstaclePos.GetPositionY() + jumpDistance * std::sin(angle);
    jumpTarget.m_positionZ = obstaclePos.GetPositionZ();

    maneuver.waypoints.push_back(botPos);
    maneuver.waypoints.push_back(jumpTarget);
    maneuver.requiresJump = true;
    maneuver.executionTime = 2.0f;
    maneuver.successProbability = 0.6f;
    maneuver.energyCost = 5.0f;
    maneuver.description = "Jump over obstacle";

    return maneuver;
}

bool ObstacleAvoidanceManager::RequiresImmediateAvoidance()
{
    DetectionContext context;
    context.bot = _bot;
    context.currentPosition = _bot->GetPosition();
    context.scanRadius = 5.0f;
    context.lookaheadTime = 1.0f;
    context.flags = DetectionFlags::BASIC;
    context.emergencyMode = true;

    std::vector<CollisionPrediction> predictions = PredictCollisions(context);

    for (const CollisionPrediction& prediction : predictions)
    {
        if (prediction.collisionType == CollisionType::IMMINENT && prediction.timeToCollision <= 1.0f)
        {
            return true;
        }
    }

    return false;
}

void ObstacleAvoidanceManager::ExecuteEmergencyStop()
{
    _bot->GetMotionMaster()->Clear();
    _bot->StopMoving();

    TC_LOG_DEBUG("playerbot.obstacle", "Bot {} executed emergency stop", _bot->GetName());
}

bool ObstacleAvoidanceManager::CanSafelyProceed(const Position& nextPosition)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    for (const auto& [guid, obstacle] : _obstacles)
    {
        if (obstacle.priority == ObstaclePriority::CRITICAL)
        {
            float distance = nextPosition.GetExactDist(&obstacle.position);
            float safeDistance = obstacle.radius + GetBotRadius() + _collisionTolerance;

            if (distance <= safeDistance)
                return false;
        }
    }

    return true;
}

ObstacleType ObstacleAvoidanceManager::ClassifyObstacle(WorldObject* object)
{
    if (!object)
        return ObstacleType::STATIC_TERRAIN;

    if (Unit* unit = object->ToUnit())
    {
        return ObstacleType::UNIT_OBSTACLE;
    }
    else if (GameObject* gameObj = object->ToGameObject())
    {
        switch (gameObj->GetGoType())
        {
            case GAMEOBJECT_TYPE_DOOR:
                return ObstacleType::DYNAMIC_OBJECT;
            case GAMEOBJECT_TYPE_TRAP:
                return ObstacleType::TEMPORARY_HAZARD;
            default:
                return ObstacleType::STATIC_TERRAIN;
        }
    }

    return ObstacleType::STATIC_TERRAIN;
}

ObstaclePriority ObstacleAvoidanceManager::AssessObstaclePriority(const ObstacleInfo& obstacle, const DetectionContext& context)
{
    switch (obstacle.type)
    {
        case ObstacleType::PROJECTILE:
            return ObstaclePriority::CRITICAL;

        case ObstacleType::TEMPORARY_HAZARD:
            return ObstaclePriority::HIGH;

        case ObstacleType::UNIT_OBSTACLE:
            if (Unit* unit = dynamic_cast<Unit*>(obstacle.object))
            {
                if (_bot->IsHostileTo(unit))
                    return ObstaclePriority::HIGH;
                else
                    return ObstaclePriority::MODERATE;
            }
            return ObstaclePriority::MODERATE;

        case ObstacleType::DYNAMIC_OBJECT:
            return ObstaclePriority::MODERATE;

        case ObstacleType::STATIC_TERRAIN:
        case ObstacleType::ENVIRONMENTAL:
        default:
            return ObstaclePriority::LOW;
    }
}

float ObstacleAvoidanceManager::CalculateObstacleRadius(WorldObject* object, ObstacleType type)
{
    if (!object)
        return 1.0f;

    if (Unit* unit = object->ToUnit())
    {
        return unit->GetCombatReach();
    }
    else if (GameObject* gameObj = object->ToGameObject())
    {
        return std::max(gameObj->GetDisplayScale(), 1.0f);
    }

    return 1.0f;
}

void ObstacleAvoidanceManager::RegisterObstacle(const ObstacleInfo& obstacle)
{
    if (_obstacles.size() >= MAX_OBSTACLES)
    {
        CleanupExpiredObstacles();
    }

    _obstacles[obstacle.guid] = obstacle;

    TC_LOG_TRACE("playerbot.obstacle", "Bot {} registered obstacle {} at ({:.2f}, {:.2f})",
               _bot->GetName(), obstacle.name, obstacle.position.GetPositionX(), obstacle.position.GetPositionY());
}

void ObstacleAvoidanceManager::UpdateObstacle(const ObstacleInfo& obstacle)
{
    auto it = _obstacles.find(obstacle.guid);
    if (it != _obstacles.end())
    {
        it->second.position = obstacle.position;
        it->second.velocity = obstacle.velocity;
        it->second.lastSeen = getMSTime();
        it->second.predictedPosition = PredictObstaclePosition(it->second, _lookaheadTime);
    }
}

void ObstacleAvoidanceManager::CleanupExpiredObstacles()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastCleanup < CLEANUP_INTERVAL)
        return;

    auto it = _obstacles.begin();
    while (it != _obstacles.end())
    {
        if (it->second.isTemporary && currentTime > it->second.expirationTime)
        {
            it = _obstacles.erase(it);
        }
        else if (currentTime - it->second.lastSeen > 10000)
        {
            it = _obstacles.erase(it);
        }
        else
        {
            ++it;
        }
    }

    _lastCleanup = currentTime;
}

float ObstacleAvoidanceManager::CalculateTimeToCollision(const ObstacleInfo& obstacle, const DetectionContext& context)
{
    if (!obstacle.isMoving)
        return -1.0f;

    Position relativePos;
    relativePos.m_positionX = obstacle.position.GetPositionX() - context.currentPosition.GetPositionX();
    relativePos.m_positionY = obstacle.position.GetPositionY() - context.currentPosition.GetPositionY();
    relativePos.m_positionZ = obstacle.position.GetPositionZ() - context.currentPosition.GetPositionZ();

    Position relativeVel;
    relativeVel.m_positionX = obstacle.velocity.m_positionX - context.velocity.m_positionX;
    relativeVel.m_positionY = obstacle.velocity.m_positionY - context.velocity.m_positionY;
    relativeVel.m_positionZ = obstacle.velocity.m_positionZ - context.velocity.m_positionZ;

    float a = relativeVel.m_positionX * relativeVel.m_positionX +
              relativeVel.m_positionY * relativeVel.m_positionY +
              relativeVel.m_positionZ * relativeVel.m_positionZ;

    float b = 2.0f * (relativePos.m_positionX * relativeVel.m_positionX +
                      relativePos.m_positionY * relativeVel.m_positionY +
                      relativePos.m_positionZ * relativeVel.m_positionZ);

    float combinedRadius = obstacle.radius + GetBotRadius() + _collisionTolerance;
    float c = relativePos.m_positionX * relativePos.m_positionX +
              relativePos.m_positionY * relativePos.m_positionY +
              relativePos.m_positionZ * relativePos.m_positionZ -
              combinedRadius * combinedRadius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f || a == 0.0f)
        return -1.0f;

    float t1 = (-b - std::sqrt(discriminant)) / (2.0f * a);
    float t2 = (-b + std::sqrt(discriminant)) / (2.0f * a);

    if (t1 > 0.0f)
        return t1;
    else if (t2 > 0.0f)
        return t2;
    else
        return -1.0f;
}

Position ObstacleAvoidanceManager::PredictObstaclePosition(const ObstacleInfo& obstacle, float timeAhead)
{
    Position predicted = obstacle.position;

    if (obstacle.isMoving)
    {
        predicted.m_positionX += obstacle.velocity.m_positionX * timeAhead;
        predicted.m_positionY += obstacle.velocity.m_positionY * timeAhead;
        predicted.m_positionZ += obstacle.velocity.m_positionZ * timeAhead;
    }

    return predicted;
}

void ObstacleAvoidanceManager::ScanTerrain(const DetectionContext& context, std::vector<ObstacleInfo>& obstacles)
{
    // Terrain obstacles are typically handled by the pathfinding system
    // This method could scan for specific terrain features if needed
}

void ObstacleAvoidanceManager::ScanUnits(const DetectionContext& context, std::vector<ObstacleInfo>& obstacles)
{
    std::vector<ObstacleInfo> unitObstacles = DetectUnitObstacles(context);
    obstacles.insert(obstacles.end(), unitObstacles.begin(), unitObstacles.end());
}

void ObstacleAvoidanceManager::ScanGameObjects(const DetectionContext& context, std::vector<ObstacleInfo>& obstacles)
{
    std::list<GameObject*> nearbyObjects;
    Trinity::AllGameObjectsInRange check(_bot, context.scanRadius);
    Trinity::GameObjectSearcher<Trinity::AllGameObjectsInRange> searcher(_bot, nearbyObjects, check);
    Cell::VisitAllObjects(_bot, searcher, context.scanRadius);

    for (GameObject* obj : nearbyObjects)
    {
        if (!obj || !obj->IsInWorld())
            continue;

        ObstacleInfo obstacle;
        obstacle.guid = obj->GetGUID();
        obstacle.object = obj;
        obstacle.position = obj->GetPosition();
        obstacle.type = ClassifyObstacle(obj);
        obstacle.radius = CalculateObstacleRadius(obj, obstacle.type);
        obstacle.height = obj->GetDisplayScale() * 2.0f;
        obstacle.isMoving = false;
        obstacle.priority = AssessObstaclePriority(obstacle, context);
        obstacle.name = obj->GetName();
        obstacle.firstDetected = getMSTime();
        obstacle.lastSeen = obstacle.firstDetected;
        obstacle.avoidanceRadius = ObstacleUtils::CalculateAvoidanceRadius(obstacle.radius, GetBotRadius());

        obstacles.push_back(obstacle);
    }
}

void ObstacleAvoidanceManager::ScanEnvironmentalHazards(const DetectionContext& context, std::vector<ObstacleInfo>& obstacles)
{
    // Environmental hazards would be detected through spell effects, ground conditions, etc.
    // This is a placeholder for environmental hazard detection
}

float ObstacleAvoidanceManager::EstimateObstacleClearanceTime(const ObstacleInfo& obstacle)
{
    if (!obstacle.isMoving)
        return 10.0f;

    float speed = std::sqrt(obstacle.velocity.m_positionX * obstacle.velocity.m_positionX +
                          obstacle.velocity.m_positionY * obstacle.velocity.m_positionY);

    if (speed <= 0.1f)
        return 10.0f;

    return (obstacle.radius * 2.0f) / speed;
}

bool ObstacleAvoidanceManager::ShouldIgnoreObstacle(const ObstacleInfo& obstacle, const DetectionContext& context)
{
    if (obstacle.priority == ObstaclePriority::IGNORE)
        return true;

    if (!IsInScanRange(obstacle.position, context))
        return true;

    if (obstacle.isTemporary && getMSTime() > obstacle.expirationTime)
        return true;

    return false;
}

float ObstacleAvoidanceManager::GetBotRadius() const
{
    return _bot->GetCombatReach();
}

float ObstacleAvoidanceManager::GetBotSpeed() const
{
    return _bot->GetSpeed(MOVE_RUN);
}

bool ObstacleAvoidanceManager::IsInScanRange(const Position& pos, const DetectionContext& context)
{
    return context.currentPosition.GetExactDist(&pos) <= context.scanRadius;
}

void ObstacleAvoidanceManager::TrackPerformance(std::chrono::microseconds duration, const std::string& operation)
{
    if (duration > _metrics.maxDetectionTime)
        _metrics.maxDetectionTime = duration;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        _metrics.averageDetectionTime = std::chrono::microseconds(
            static_cast<uint64_t>(_metrics.averageDetectionTime.count() * 0.9 + duration.count() * 0.1)
        );
        _metrics.lastUpdate = currentTime;
    }
}

// ObstacleUtils implementation
bool ObstacleUtils::IsPointInCircle(const Position& point, const Position& center, float radius)
{
    return point.GetExactDist(&center) <= radius;
}

float ObstacleUtils::DistancePointToLine(const Position& point, const Position& lineStart, const Position& lineEnd)
{
    float A = point.GetPositionX() - lineStart.GetPositionX();
    float B = point.GetPositionY() - lineStart.GetPositionY();
    float C = lineEnd.GetPositionX() - lineStart.GetPositionX();
    float D = lineEnd.GetPositionY() - lineStart.GetPositionY();

    float dot = A * C + B * D;
    float lenSq = C * C + D * D;

    if (lenSq == 0.0f)
        return point.GetExactDist(&lineStart);

    float param = dot / lenSq;

    float xx, yy;

    if (param < 0.0f)
    {
        xx = lineStart.GetPositionX();
        yy = lineStart.GetPositionY();
    }
    else if (param > 1.0f)
    {
        xx = lineEnd.GetPositionX();
        yy = lineEnd.GetPositionY();
    }
    else
    {
        xx = lineStart.GetPositionX() + param * C;
        yy = lineStart.GetPositionY() + param * D;
    }

    float dx = point.GetPositionX() - xx;
    float dy = point.GetPositionY() - yy;

    return std::sqrt(dx * dx + dy * dy);
}

Position ObstacleUtils::ClosestPointOnLine(const Position& point, const Position& lineStart, const Position& lineEnd)
{
    float A = point.GetPositionX() - lineStart.GetPositionX();
    float B = point.GetPositionY() - lineStart.GetPositionY();
    float C = lineEnd.GetPositionX() - lineStart.GetPositionX();
    float D = lineEnd.GetPositionY() - lineStart.GetPositionY();

    float dot = A * C + B * D;
    float lenSq = C * C + D * D;

    if (lenSq == 0.0f)
        return lineStart;

    float param = std::max(0.0f, std::min(1.0f, dot / lenSq));

    Position closest;
    closest.m_positionX = lineStart.GetPositionX() + param * C;
    closest.m_positionY = lineStart.GetPositionY() + param * D;
    closest.m_positionZ = lineStart.GetPositionZ() + param * (lineEnd.GetPositionZ() - lineStart.GetPositionZ());

    return closest;
}

float ObstacleUtils::CalculateAvoidanceRadius(float obstacleRadius, float botRadius, float safetyMargin)
{
    return obstacleRadius + botRadius + safetyMargin;
}

Position ObstacleUtils::PredictPosition(const Position& current, const Position& velocity, float timeAhead)
{
    Position predicted;
    predicted.m_positionX = current.GetPositionX() + velocity.m_positionX * timeAhead;
    predicted.m_positionY = current.GetPositionY() + velocity.m_positionY * timeAhead;
    predicted.m_positionZ = current.GetPositionZ() + velocity.m_positionZ * timeAhead;
    return predicted;
}

bool ObstacleUtils::DoCirclesIntersect(const Position& center1, float radius1, const Position& center2, float radius2)
{
    float distance = center1.GetExactDist(&center2);
    return distance <= (radius1 + radius2);
}

} // namespace Playerbot