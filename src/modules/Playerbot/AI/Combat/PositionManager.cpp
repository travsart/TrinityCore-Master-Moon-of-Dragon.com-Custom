/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PositionManager.h"
#include "Player.h"
#include "Unit.h"
#include "Map.h"
#include "Log.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "PathGenerator.h"
#include "MoveSpline.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace Playerbot
{

PositionManager::PositionManager(Player* bot, BotThreatManager* threatManager)
    : _bot(bot), _threatManager(threatManager), _updateInterval(DEFAULT_UPDATE_INTERVAL),
      _lastUpdate(0), _positionTolerance(POSITION_TOLERANCE), _maxCandidates(MAX_CANDIDATES),
      _lastZoneUpdate(0)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "PositionManager: Bot player is null!");
        return;
    }

    if (!_threatManager)
    {
        TC_LOG_ERROR("playerbot", "PositionManager: ThreatManager is null for bot {}", _bot->GetName());
        return;
    }

    TC_LOG_DEBUG("playerbot.position", "PositionManager initialized for bot {}", _bot->GetName());
}

MovementResult PositionManager::UpdatePosition(const MovementContext& context)
{
    auto startTime = std::chrono::steady_clock::now();
    MovementResult result;

    std::unique_lock<std::shared_mutex> lock(_mutex);

    try
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _lastUpdate < _updateInterval && !context.emergencyMode)
        {
            result.failureReason = "Update interval not reached";
            return result;
        }

        _lastUpdate = currentTime;
        UpdateAoEZones(currentTime);

        Position currentPos = _bot->GetPosition();
        result.currentPosition = currentPos;

        PositionInfo currentPosInfo = EvaluatePosition(currentPos, context);

        if (!context.emergencyMode && currentPosInfo.score >= 80.0f && currentPosInfo.priority >= MovementPriority::OPTIMIZATION)
        {
            result.success = true;
            result.targetPosition = currentPos;
            result.priority = MovementPriority::MAINTENANCE;
            return result;
        }

        if (context.emergencyMode || IsInDangerZone(currentPos))
        {
            return HandleEmergencyMovement(context);
        }

        MovementResult optimalResult = FindOptimalPosition(context);
        if (optimalResult.success)
        {
            float movementDistance = currentPos.GetExactDist(&optimalResult.targetPosition);
            if (movementDistance > _positionTolerance)
            {
                return ExecuteMovement(optimalResult.targetPosition, optimalResult.priority);
            }
        }

        result.success = true;
        result.targetPosition = currentPos;
        result.priority = MovementPriority::MAINTENANCE;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.failureReason = "Exception during position update: " + std::string(e.what());
        TC_LOG_ERROR("playerbot.position", "Exception in UpdatePosition for bot {}: {}", _bot->GetName(), e.what());
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "UpdatePosition");

    return result;
}

MovementResult PositionManager::FindOptimalPosition(const MovementContext& context)
{
    MovementResult result;

    std::vector<Position> candidates = GenerateCandidatePositions(context);
    if (candidates.empty())
    {
        result.failureReason = "No candidate positions generated";
        return result;
    }

    std::vector<PositionInfo> evaluatedPositions = EvaluatePositions(candidates, context);
    if (evaluatedPositions.empty())
    {
        result.failureReason = "No valid positions after evaluation";
        return result;
    }

    std::sort(evaluatedPositions.begin(), evaluatedPositions.end(), std::greater<PositionInfo>());

    PositionInfo bestPosition = evaluatedPositions[0];
    result.success = true;
    result.targetPosition = bestPosition.position;
    result.priority = bestPosition.priority;
    result.estimatedTime = EstimateMovementTime(_bot->GetPosition(), bestPosition.position);
    result.pathDistance = _bot->GetExactDist(&bestPosition.position);

    TC_LOG_DEBUG("playerbot.position", "Found optimal position for bot {} at ({:.2f}, {:.2f}, {:.2f}) with score {:.2f}",
               _bot->GetName(), bestPosition.position.GetPositionX(), bestPosition.position.GetPositionY(),
               bestPosition.position.GetPositionZ(), bestPosition.score);

    return result;
}

MovementResult PositionManager::ExecuteMovement(const Position& targetPos, MovementPriority priority)
{
    MovementResult result;
    result.targetPosition = targetPos;
    result.priority = priority;

    if (!CanReachPosition(targetPos))
    {
        result.failureReason = "Target position unreachable";
        return result;
    }

    Position currentPos = _bot->GetPosition();
    float distance = currentPos.GetExactDist(&targetPos);

    if (distance <= _positionTolerance)
    {
        result.success = true;
        result.pathDistance = 0.0f;
        result.estimatedTime = 0.0f;
        return result;
    }

    std::vector<Position> waypoints = CalculateWaypoints(currentPos, targetPos);
    result.waypoints = waypoints;
    result.pathDistance = 0.0f;

    for (size_t i = 1; i < waypoints.size(); ++i)
    {
        result.pathDistance += waypoints[i-1].GetExactDist(&waypoints[i]);
    }

    result.estimatedTime = EstimateMovementTime(currentPos, targetPos);

    if (priority <= MovementPriority::CRITICAL)
    {
        result.requiresSprint = true;
    }

    float heightDiff = std::abs(targetPos.GetPositionZ() - currentPos.GetPositionZ());
    if (heightDiff > 3.0f)
    {
        result.requiresJump = true;
    }

    _bot->GetMotionMaster()->MovePoint(0, targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ());

    result.success = true;
    _metrics.movementCommands++;

    TC_LOG_DEBUG("playerbot.position", "Bot {} moving to position ({:.2f}, {:.2f}, {:.2f}), distance: {:.2f}",
               _bot->GetName(), targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ(), distance);

    return result;
}

PositionInfo PositionManager::EvaluatePosition(const Position& pos, const MovementContext& context)
{
    auto startTime = std::chrono::steady_clock::now();
    PositionInfo info;
    info.position = pos;
    info.evaluationTime = getMSTime();

    if (!ValidatePosition(pos, context.validationFlags))
    {
        info.score = 0.0f;
        info.priority = MovementPriority::IDLE;
        return info;
    }

    float totalScore = 0.0f;

    totalScore += CalculateDistanceScore(pos, context) * context.weights.distanceWeight;
    totalScore += CalculateSafetyScore(pos, context) * context.weights.safetyWeight;
    totalScore += CalculateLineOfSightScore(pos, context) * context.weights.losWeight;
    totalScore += CalculateAngleScore(pos, context) * context.weights.angleWeight;
    totalScore += CalculateGroupScore(pos, context) * context.weights.groupWeight;
    totalScore += CalculateEscapeScore(pos, context) * context.weights.escapeWeight;

    info.score = std::max(0.0f, totalScore);
    info.distanceToTarget = context.target ? pos.GetExactDist(context.target) : 0.0f;
    info.hasLineOfSight = context.target ? _bot->IsWithinLOSInMap(context.target, pos) : true;
    info.isOptimalRange = (info.distanceToTarget >= context.preferredRange * 0.8f &&
                          info.distanceToTarget <= context.preferredRange * 1.2f);
    info.safetyRating = CalculateSafetyScore(pos, context);
    info.movementCost = CalculateMovementCost(_bot->GetPosition(), pos);

    if (info.score >= 90.0f)
        info.priority = MovementPriority::OPTIMIZATION;
    else if (info.score >= 70.0f)
        info.priority = MovementPriority::TACTICAL;
    else if (info.score >= 50.0f)
        info.priority = MovementPriority::MAINTENANCE;
    else
        info.priority = MovementPriority::IDLE;

    if (IsInDangerZone(pos))
    {
        info.priority = MovementPriority::EMERGENCY;
        info.score *= 0.1f;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "EvaluatePosition");

    _metrics.positionEvaluations++;
    return info;
}

std::vector<PositionInfo> PositionManager::EvaluatePositions(const std::vector<Position>& positions, const MovementContext& context)
{
    std::vector<PositionInfo> results;
    results.reserve(positions.size());

    for (const Position& pos : positions)
    {
        PositionInfo info = EvaluatePosition(pos, context);
        if (info.score > 0.0f)
        {
            results.push_back(info);
        }
    }

    return results;
}

std::vector<Position> PositionManager::GenerateCandidatePositions(const MovementContext& context)
{
    std::vector<Position> candidates;

    if (!context.target)
    {
        return candidates;
    }

    Position targetPos = context.target->GetPosition();
    float minRange = 2.0f;
    float maxRange = context.maxRange;

    switch (context.desiredType)
    {
        case PositionType::MELEE_RANGE:
            candidates = GenerateCircularPositions(targetPos, 4.0f, 8);
            break;

        case PositionType::RANGED_DPS:
            {
                std::vector<Position> innerRing = GenerateCircularPositions(targetPos, context.preferredRange * 0.8f, 12);
                std::vector<Position> outerRing = GenerateCircularPositions(targetPos, context.preferredRange * 1.2f, 12);
                candidates.insert(candidates.end(), innerRing.begin(), innerRing.end());
                candidates.insert(candidates.end(), outerRing.begin(), outerRing.end());
            }
            break;

        case PositionType::HEALING:
            {
                if (!context.groupMembers.empty())
                {
                    Position groupCenter = PositionUtils::CalculateGroupCenter(context.groupMembers);
                    candidates = GenerateCircularPositions(groupCenter, 20.0f, 16);
                }
                else
                {
                    candidates = GenerateCircularPositions(targetPos, 25.0f, 12);
                }
            }
            break;

        case PositionType::KITING:
            {
                Position currentPos = _bot->GetPosition();
                float angle = PositionUtils::CalculateAngleBetween(targetPos, currentPos);
                candidates = GenerateArcPositions(targetPos, context.preferredRange, angle - M_PI/3, angle + M_PI/3, 8);
            }
            break;

        case PositionType::FLANKING:
            {
                float targetAngle = context.target->GetOrientation();
                float leftFlankAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI/2);
                float rightFlankAngle = PositionUtils::NormalizeAngle(targetAngle - M_PI/2);

                candidates.push_back(PositionUtils::CalculatePositionAtAngle(targetPos, 6.0f, leftFlankAngle));
                candidates.push_back(PositionUtils::CalculatePositionAtAngle(targetPos, 6.0f, rightFlankAngle));
            }
            break;

        case PositionType::TANKING:
            {
                float targetAngle = context.target->GetOrientation();
                float frontAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
                candidates = GenerateArcPositions(targetPos, 5.0f, frontAngle - M_PI/6, frontAngle + M_PI/6, 6);
            }
            break;

        default:
            candidates = GenerateCircularPositions(targetPos, context.preferredRange, 12);
            break;
    }

    if (candidates.size() > _maxCandidates)
    {
        candidates.resize(_maxCandidates);
    }

    return candidates;
}

Position PositionManager::FindMeleePosition(Unit* target, bool preferBehind)
{
    if (!target)
        return _bot->GetPosition();

    Position targetPos = target->GetPosition();
    float targetAngle = target->GetOrientation();

    if (preferBehind)
    {
        float behindAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
        return PositionUtils::CalculatePositionAtAngle(targetPos, 3.5f, behindAngle);
    }
    else
    {
        std::vector<Position> candidates = GenerateCircularPositions(targetPos, 4.0f, 8);

        MovementContext context;
        context.bot = _bot;
        context.target = target;
        context.desiredType = PositionType::MELEE_RANGE;
        context.validationFlags = PositionValidation::COMBAT;

        PositionInfo bestPosition;
        for (const Position& pos : candidates)
        {
            PositionInfo info = EvaluatePosition(pos, context);
            if (info.score > bestPosition.score)
            {
                bestPosition = info;
            }
        }

        return bestPosition.position;
    }
}

Position PositionManager::FindRangedPosition(Unit* target, float preferredRange)
{
    if (!target)
        return _bot->GetPosition();

    Position targetPos = target->GetPosition();
    std::vector<Position> candidates = GenerateCircularPositions(targetPos, preferredRange, 16);

    MovementContext context;
    context.bot = _bot;
    context.target = target;
    context.desiredType = PositionType::RANGED_DPS;
    context.preferredRange = preferredRange;
    context.validationFlags = PositionValidation::SAFE;

    PositionInfo bestPosition;
    for (const Position& pos : candidates)
    {
        PositionInfo info = EvaluatePosition(pos, context);
        if (info.score > bestPosition.score)
        {
            bestPosition = info;
        }
    }

    return bestPosition.position;
}

Position PositionManager::FindKitingPosition(Unit* threat, float minDistance)
{
    if (!threat)
        return _bot->GetPosition();

    Position currentPos = _bot->GetPosition();
    Position threatPos = threat->GetPosition();

    float currentDistance = currentPos.GetExactDist(&threatPos);
    if (currentDistance >= minDistance * 1.2f)
    {
        return currentPos;
    }

    float escapeAngle = PositionUtils::CalculateAngleBetween(threatPos, currentPos);

    std::vector<Position> escapePositions;
    for (int i = -2; i <= 2; ++i)
    {
        float angle = PositionUtils::NormalizeAngle(escapeAngle + (i * M_PI/6));
        Position escapePos = PositionUtils::CalculatePositionAtAngle(threatPos, minDistance * 1.5f, angle);
        escapePositions.push_back(escapePos);
    }

    MovementContext context;
    context.bot = _bot;
    context.target = threat;
    context.desiredType = PositionType::KITING;
    context.preferredRange = minDistance * 1.5f;
    context.validationFlags = PositionValidation::BASIC;

    PositionInfo bestPosition;
    for (const Position& pos : escapePositions)
    {
        PositionInfo info = EvaluatePosition(pos, context);
        if (info.score > bestPosition.score)
        {
            bestPosition = info;
        }
    }

    return bestPosition.position;
}

bool PositionManager::IsPositionSafe(const Position& pos, const MovementContext& context)
{
    if (IsInDangerZone(pos))
        return false;

    for (Unit* enemy : context.nearbyEnemies)
    {
        if (!enemy)
            continue;

        float distance = pos.GetExactDist(enemy);
        if (distance < 5.0f && enemy->IsHostileTo(_bot))
        {
            return false;
        }
    }

    return ValidatePosition(pos, PositionValidation::SAFE);
}

bool PositionManager::IsInDangerZone(const Position& pos)
{
    uint32 currentTime = getMSTime();

    for (const AoEZone& zone : _activeZones)
    {
        if (zone.IsPositionInDanger(pos, currentTime))
        {
            return true;
        }
    }

    return false;
}

Position PositionManager::FindSafePosition(const Position& fromPos, float minDistance)
{
    std::vector<Position> safePositions = GenerateCircularPositions(fromPos, minDistance, 16);

    for (const Position& pos : safePositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }

    std::vector<Position> farPositions = GenerateCircularPositions(fromPos, minDistance * 2.0f, 16);
    for (const Position& pos : farPositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }

    return fromPos;
}

void PositionManager::RegisterAoEZone(const AoEZone& zone)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _activeZones.push_back(zone);

    TC_LOG_DEBUG("playerbot.position", "Registered AoE zone for bot {} at ({:.2f}, {:.2f}) radius {:.2f}",
               _bot->GetName(), zone.center.GetPositionX(), zone.center.GetPositionY(), zone.radius);
}

void PositionManager::UpdateAoEZones(uint32 currentTime)
{
    if (currentTime - _lastZoneUpdate < 1000)  // Update every second
        return;

    ClearExpiredZones(currentTime);
    _lastZoneUpdate = currentTime;
}

void PositionManager::ClearExpiredZones(uint32 currentTime)
{
    _activeZones.erase(
        std::remove_if(_activeZones.begin(), _activeZones.end(),
            [currentTime](const AoEZone& zone) {
                return !zone.isActive || currentTime > zone.startTime + zone.duration;
            }),
        _activeZones.end()
    );
}

bool PositionManager::ValidatePosition(const Position& pos, PositionValidation flags)
{
    if ((flags & PositionValidation::WALKABLE) && !IsWalkablePosition(pos))
        return false;

    if ((flags & PositionValidation::STABLE_GROUND))
    {
        Map* map = _bot->GetMap();
        if (!map || !PositionUtils::IsPositionOnGround(pos, map))
            return false;
    }

    if ((flags & PositionValidation::NO_OBSTACLES))
    {
        if (!PositionUtils::CanWalkStraightLine(_bot->GetPosition(), pos, _bot->GetMap()))
            return false;
    }

    if ((flags & PositionValidation::AVOID_AOE) && IsInDangerZone(pos))
        return false;

    return true;
}

bool PositionManager::IsWalkablePosition(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return map->IsInWater(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()) == false &&
           PositionUtils::IsPositionOnGround(pos, map);
}

float PositionManager::CalculateMovementCost(const Position& from, const Position& to)
{
    float distance = from.GetExactDist(&to);
    float heightDiff = std::abs(to.GetPositionZ() - from.GetPositionZ());

    float cost = distance;
    if (heightDiff > 2.0f)
        cost += heightDiff * 2.0f;  // Penalty for elevation changes

    if (!PositionUtils::CanWalkStraightLine(from, to, _bot->GetMap()))
        cost *= 1.5f;  // Penalty for indirect paths

    return cost;
}

MovementResult PositionManager::HandleEmergencyMovement(const MovementContext& context)
{
    _metrics.emergencyMoves++;

    Position emergencyPos = FindEmergencyEscapePosition();

    MovementResult result;
    result.priority = MovementPriority::EMERGENCY;
    result.requiresSprint = true;

    return ExecuteMovement(emergencyPos, MovementPriority::EMERGENCY);
}

Position PositionManager::FindEmergencyEscapePosition()
{
    Position currentPos = _bot->GetPosition();
    std::vector<Position> escapePositions = GenerateCircularPositions(currentPos, EMERGENCY_DISTANCE, 12);

    for (const Position& pos : escapePositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }

    return currentPos;
}

float PositionManager::CalculateDistanceScore(const Position& pos, const MovementContext& context)
{
    if (!context.target)
        return 50.0f;

    float distance = pos.GetExactDist(context.target);
    float optimalDistance = context.preferredRange;

    if (distance <= optimalDistance * 1.2f && distance >= optimalDistance * 0.8f)
        return 100.0f;

    float deviation = std::abs(distance - optimalDistance) / optimalDistance;
    return std::max(0.0f, 100.0f - (deviation * 100.0f));
}

float PositionManager::CalculateSafetyScore(const Position& pos, const MovementContext& context)
{
    float score = 100.0f;

    if (IsInDangerZone(pos))
        score -= 80.0f;

    for (Unit* enemy : context.nearbyEnemies)
    {
        if (!enemy)
            continue;

        float distance = pos.GetExactDist(enemy);
        if (distance < 5.0f)
            score -= 30.0f;
        else if (distance < 10.0f)
            score -= 15.0f;
    }

    return std::max(0.0f, score);
}

float PositionManager::CalculateLineOfSightScore(const Position& pos, const MovementContext& context)
{
    if (!context.target)
        return 100.0f;

    if (_bot->IsWithinLOSInMap(context.target, pos))
        return 100.0f;
    else
        return 0.0f;
}

float PositionManager::CalculateAngleScore(const Position& pos, const MovementContext& context)
{
    if (!context.target)
        return 50.0f;

    float score = 50.0f;
    Position targetPos = context.target->GetPosition();
    float targetAngle = context.target->GetOrientation();
    float positionAngle = PositionUtils::CalculateAngleBetween(targetPos, pos);

    switch (context.desiredType)
    {
        case PositionType::MELEE_RANGE:
        case PositionType::FLANKING:
            {
                float behindAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
                float angleDiff = std::abs(PositionUtils::NormalizeAngle(positionAngle - behindAngle));
                if (angleDiff < M_PI/6)  // Within 30 degrees behind
                    score += 30.0f;
            }
            break;

        case PositionType::TANKING:
            {
                float frontAngle = targetAngle;
                float angleDiff = std::abs(PositionUtils::NormalizeAngle(positionAngle - frontAngle));
                if (angleDiff < M_PI/6)  // Within 30 degrees in front
                    score += 30.0f;
            }
            break;

        default:
            score = 50.0f;
            break;
    }

    return score;
}

float PositionManager::CalculateGroupScore(const Position& pos, const MovementContext& context)
{
    if (context.groupMembers.empty())
        return 50.0f;

    Position groupCenter = PositionUtils::CalculateGroupCenter(context.groupMembers);
    float distanceToGroup = pos.GetExactDist(&groupCenter);

    float optimalDistance = GetOptimalGroupDistance(context.botRole);

    if (distanceToGroup <= optimalDistance * 1.2f && distanceToGroup >= optimalDistance * 0.8f)
        return 100.0f;

    float deviation = std::abs(distanceToGroup - optimalDistance) / optimalDistance;
    return std::max(0.0f, 100.0f - (deviation * 50.0f));
}

float PositionManager::GetOptimalGroupDistance(ThreatRole role)
{
    switch (role)
    {
        case ThreatRole::TANK:
            return 8.0f;
        case ThreatRole::HEALER:
            return 25.0f;
        case ThreatRole::DPS:
            return 15.0f;
        case ThreatRole::SUPPORT:
            return 20.0f;
        default:
            return 15.0f;
    }
}

std::vector<Position> PositionManager::GenerateCircularPositions(const Position& center, float radius, uint32 count)
{
    std::vector<Position> positions;
    positions.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        float angle = (2.0f * M_PI * i) / count;
        Position pos = PositionUtils::CalculatePositionAtAngle(center, radius, angle);
        positions.push_back(pos);
    }

    return positions;
}

std::vector<Position> PositionManager::GenerateArcPositions(const Position& center, float radius, float startAngle, float endAngle, uint32 count)
{
    std::vector<Position> positions;
    positions.reserve(count);

    float angleStep = (endAngle - startAngle) / (count - 1);

    for (uint32 i = 0; i < count; ++i)
    {
        float angle = startAngle + (angleStep * i);
        Position pos = PositionUtils::CalculatePositionAtAngle(center, radius, angle);
        positions.push_back(pos);
    }

    return positions;
}

float PositionManager::EstimateMovementTime(const Position& from, const Position& to)
{
    float distance = from.GetExactDist(&to);
    float baseSpeed = _bot->GetSpeed(MOVE_RUN);

    return distance / baseSpeed;
}

bool PositionManager::CanReachPosition(const Position& pos)
{
    return ValidatePosition(pos, PositionValidation::WALKABLE | PositionValidation::NO_OBSTACLES);
}

std::vector<Position> PositionManager::CalculateWaypoints(const Position& from, const Position& to)
{
    std::vector<Position> waypoints;
    waypoints.push_back(from);
    waypoints.push_back(to);

    return waypoints;
}

void PositionManager::TrackPerformance(std::chrono::microseconds duration, const std::string& operation)
{
    if (duration > _metrics.maxEvaluationTime)
        _metrics.maxEvaluationTime = duration;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        uint32 totalEvaluations = _metrics.positionEvaluations.load();
        if (totalEvaluations > 0)
        {
            _metrics.averageEvaluationTime = std::chrono::microseconds(
                static_cast<uint64_t>(_metrics.averageEvaluationTime.count() * 0.9 + duration.count() * 0.1)
            );
        }
        _metrics.lastUpdate = currentTime;
    }
}

// PositionUtils implementation
Position PositionUtils::CalculatePositionAtAngle(const Position& center, float distance, float angle)
{
    Position result;
    result.m_positionX = center.GetPositionX() + distance * std::cos(angle);
    result.m_positionY = center.GetPositionY() + distance * std::sin(angle);
    result.m_positionZ = center.GetPositionZ();
    result.SetOrientation(angle);

    return result;
}

float PositionUtils::CalculateAngleBetween(const Position& from, const Position& to)
{
    float dx = to.GetPositionX() - from.GetPositionX();
    float dy = to.GetPositionY() - from.GetPositionY();

    return std::atan2(dy, dx);
}

float PositionUtils::NormalizeAngle(float angle)
{
    while (angle > M_PI)
        angle -= 2.0f * M_PI;
    while (angle < -M_PI)
        angle += 2.0f * M_PI;

    return angle;
}

bool PositionUtils::IsInMeleeRange(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    float distance = bot->GetDistance(target);
    return distance <= 5.0f;
}

bool PositionUtils::IsInOptimalRange(Player* bot, Unit* target, PositionType type)
{
    if (!bot || !target)
        return false;

    float distance = bot->GetDistance(target);

    switch (type)
    {
        case PositionType::MELEE_RANGE:
            return distance <= 5.0f;
        case PositionType::RANGED_DPS:
            return distance >= 20.0f && distance <= 40.0f;
        case PositionType::HEALING:
            return distance <= 40.0f;
        case PositionType::KITING:
            return distance >= 15.0f;
        default:
            return true;
    }
}

Position PositionUtils::CalculateGroupCenter(const std::vector<Player*>& players)
{
    if (players.empty())
        return Position();

    float totalX = 0.0f, totalY = 0.0f, totalZ = 0.0f;

    for (Player* player : players)
    {
        if (player)
        {
            totalX += player->GetPositionX();
            totalY += player->GetPositionY();
            totalZ += player->GetPositionZ();
        }
    }

    Position center;
    center.m_positionX = totalX / players.size();
    center.m_positionY = totalY / players.size();
    center.m_positionZ = totalZ / players.size();

    return center;
}

bool PositionUtils::IsPositionOnGround(const Position& pos, Map* map)
{
    if (!map)
        return false;

    float groundZ = map->GetHeight(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
    return std::abs(pos.GetPositionZ() - groundZ) <= 2.0f;
}

bool PositionUtils::CanWalkStraightLine(const Position& from, const Position& to, Map* map)
{
    if (!map)
        return false;

    return !map->isInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                                to.GetPositionX(), to.GetPositionY(), to.GetPositionZ());
}

} // namespace Playerbot