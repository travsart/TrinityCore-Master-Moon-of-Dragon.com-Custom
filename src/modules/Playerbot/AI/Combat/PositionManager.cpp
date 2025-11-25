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
#include "PhasingHandler.h"
#include "MotionMaster.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "PhaseShift.h"
#include "SharedDefines.h"
#include "../../Spatial/SpatialGridManager.h"
#include "../../Spatial/SpatialGridQueryHelpers.h"  // PHASE 5B: Thread-safe helpers
#include "ObjectAccessor.h"

namespace Playerbot
{

PositionManager::PositionManager(Player* bot, BotThreatManager* threatManager)
    : _bot(bot), _threatManager(threatManager), _updateInterval(DEFAULT_UPDATE_INTERVAL),
      _lastUpdate(0), _positionTolerance(POSITION_TOLERANCE), _maxCandidates(MAX_CANDIDATES),
      _lastZoneUpdate(0), _lastMovePointTime(0)
{

    if (!_threatManager)
    {
        TC_LOG_ERROR("playerbot", "PositionManager: ThreatManager is null for bot {}", _bot->GetName());
        return;
    }

    TC_LOG_DEBUG("playerbot.position", "PositionManager initialized for bot {}", _bot->GetName());
}

PositionMovementResult PositionManager::UpdatePosition(const MovementContext& context)
{
    auto startTime = ::std::chrono::steady_clock::now();
    PositionMovementResult result;
    // No lock needed - position data is per-bot instance data

    try
    {
        // FIX #3: SPELL CASTING COORDINATION - Don't move while casting
    if (_bot->IsNonMeleeSpellCast(false))
        {
            result.failureReason = "Bot is casting, movement would interrupt spell";
            TC_LOG_DEBUG("playerbot.position", "⏸ Bot {} - Movement blocked, currently casting",
                         _bot->GetName());
            return result;
        }

        uint32 currentTime = GameTime::GetGameTimeMS();
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

        if (!context.emergencyMode && currentPosInfo.score >= 80.0f && currentPosInfo.priority >= MovementPriority::PRIORITY_COMBAT)
        {
            result.success = true;
            result.targetPosition = currentPos;
            result.priority = MovementPriority::PRIORITY_NORMAL;
            return result;
        }

        if (context.emergencyMode || IsInDangerZone(currentPos))
        {
            return HandleEmergencyMovement(context);
        }

        PositionMovementResult optimalResult = FindOptimalPosition(context);
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
        result.priority = MovementPriority::PRIORITY_NORMAL;
    }
    catch (const ::std::exception& e)
    {
        result.success = false;
        result.failureReason = "Exception during position update: " + ::std::string(e.what());
        TC_LOG_ERROR("playerbot.position", "Exception in UpdatePosition for bot {}: {}", _bot->GetName(), e.what());
    }

    auto endTime = ::std::chrono::steady_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "UpdatePosition");

    return result;
}

PositionMovementResult PositionManager::FindOptimalPosition(const MovementContext& context)
{
    PositionMovementResult result;
    ::std::vector<Position> candidates = GenerateCandidatePositions(context);
    if (candidates.empty())
    {
        result.failureReason = "No candidate positions generated";
        return result;
    }

    ::std::vector<PositionInfo> evaluatedPositions = EvaluatePositions(candidates, context);
    if (evaluatedPositions.empty())
    {
        result.failureReason = "No valid positions after evaluation";
        return result;
    }

    ::std::sort(evaluatedPositions.begin(), evaluatedPositions.end(), ::std::greater<PositionInfo>());

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

PositionMovementResult PositionManager::ExecuteMovement(const Position& targetPos, MovementPriority priority)
{
    PositionMovementResult result;
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

    ::std::vector<Position> waypoints = CalculateWaypoints(currentPos, targetPos);
    result.waypoints = waypoints;
    result.pathDistance = 0.0f;

    for (size_t i = 1; i < waypoints.size(); ++i)
    {
        result.pathDistance += waypoints[i-1].GetExactDist(&waypoints[i]);
    }

    result.estimatedTime = EstimateMovementTime(currentPos, targetPos);

    if (priority <= MovementPriority::PRIORITY_CRITICAL)
    {
        result.requiresSprint = true;
    }

    float heightDiff = ::std::abs(targetPos.GetPositionZ() - currentPos.GetPositionZ());
    if (heightDiff > 3.0f)
    {
        result.requiresJump = true;
    }

    // FIX #1: DUPLICATE MOVEMENT PREVENTION - Check if already moving to same position
    // This prevents the infinite movement cancellation bug (60+ MovePoint calls/second)
    uint32 currentTime = GameTime::GetGameTimeMS();
    float distanceToLastTarget = _lastTargetPosition.GetExactDist(&targetPos);

    if (distanceToLastTarget < _positionTolerance && (currentTime - _lastMovePointTime) < 500)
    {
        // Already moving to same destination - don't re-issue command within 500ms
        result.success = true;
        result.failureReason = "Already moving to target position";
        TC_LOG_DEBUG("playerbot.position", "⏭ Bot {} - Duplicate movement prevented, already moving to ({:.2f}, {:.2f}, {:.2f})",
                     _bot->GetName(), targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ());

        return result;
    }

    // Issue new movement command
    _bot->GetMotionMaster()->MovePoint(0, targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ());

    // FIX #5: SPEED CONTROL SYSTEM - Apply sprint for critical/emergency movement
    if (result.requiresSprint)
    {
        // Increase movement speed for urgent repositioning
        // Note: TrinityCore handles speed through auras/spells, not direct modification
        // This is a marker for future sprint ability integration
        TC_LOG_DEBUG("playerbot.position", " Bot {} - Sprint required for urgent movement (priority: {})",
                     _bot->GetName(), static_cast<uint8>(priority));

        // Future: Trigger sprint ability here if available
        // For now, just log the requirement - ClassAI should handle sprint abilities
    }

    // Update tracking state
    _lastTargetPosition = targetPos;
    _lastMovePointTime = currentTime;

    result.success = true;
    _metrics.movementCommands++;

    TC_LOG_DEBUG("playerbot.position", "Bot {} moving to position ({:.2f}, {:.2f}, {:.2f}), distance: {:.2f}",
               _bot->GetName(), targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ(), distance);

    return result;
}

PositionInfo PositionManager::EvaluatePosition(const Position& pos, const MovementContext& context)
{
    auto startTime = ::std::chrono::steady_clock::now();
    PositionInfo info;
    info.position = pos;
    info.evaluationTime = GameTime::GetGameTimeMS();

    if (!ValidatePosition(pos, context.validationFlags))
    {
        info.score = 0.0f;
        info.priority = MovementPriority::PRIORITY_NONE;
        return info;
    }

    float totalScore = 0.0f;

    totalScore += CalculateDistanceScore(pos, context) * context.weights.distanceWeight;
    totalScore += CalculateSafetyScore(pos, context) * context.weights.safetyWeight;
    totalScore += CalculateLineOfSightScore(pos, context) * context.weights.losWeight;
    totalScore += CalculateAngleScore(pos, context) * context.weights.angleWeight;
    totalScore += CalculateGroupScore(pos, context) * context.weights.groupWeight;
    totalScore += CalculateEscapeScore(pos, context) * context.weights.escapeWeight;

    info.score = ::std::max(0.0f, totalScore);
    info.distanceToTarget = context.target ? pos.GetExactDist(context.target) : 0.0f;
    info.hasLineOfSight = context.target ? _bot->IsWithinLOSInMap(context.target) : true;
    info.isOptimalRange = (info.distanceToTarget >= context.preferredRange * 0.8f &&
                          info.distanceToTarget <= context.preferredRange * 1.2f);
    info.safetyRating = CalculateSafetyScore(pos, context);
    info.movementCost = CalculateMovementCost(_bot->GetPosition(), pos);
    if (info.score >= 90.0f)
        info.priority = MovementPriority::PRIORITY_COMBAT;
    else if (info.score >= 70.0f)
        info.priority = MovementPriority::PRIORITY_FLEE;
    else if (info.score >= 50.0f)
        info.priority = MovementPriority::PRIORITY_NORMAL;
    else
        info.priority = MovementPriority::PRIORITY_NONE;

    if (IsInDangerZone(pos))
    {
        info.priority = MovementPriority::PRIORITY_CRITICAL;
        info.score *= 0.1f;
    }

    auto endTime = ::std::chrono::steady_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "EvaluatePosition");

    _metrics.positionEvaluations++;
    return info;
}

::std::vector<PositionInfo> PositionManager::EvaluatePositions(const ::std::vector<Position>& positions, const MovementContext& context)
{
    ::std::vector<PositionInfo> results;
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

::std::vector<Position> PositionManager::GenerateCandidatePositions(const MovementContext& context)
{
    ::std::vector<Position> candidates;

    if (!context.target)
    {
        return candidates;
    }

    Position targetPos = context.target->GetPosition();
    // Variables for potential position calculations
    (void)context.maxRange; // Silence unused warning
    switch (context.desiredType)
    {
        case PositionType::MELEE_COMBAT:
            candidates = GenerateCircularPositions(targetPos, 4.0f, 8);
            break;

        case PositionType::RANGED_DPS:
            {
                ::std::vector<Position> innerRing = GenerateCircularPositions(targetPos, context.preferredRange * 0.8f, 12);
                ::std::vector<Position> outerRing = GenerateCircularPositions(targetPos, context.preferredRange * 1.2f, 12);
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
        ::std::vector<Position> candidates = GenerateCircularPositions(targetPos, 4.0f, 8);

        MovementContext context;
        context.bot = _bot;
        context.target = target;
        context.desiredType = PositionType::MELEE_COMBAT;
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
    ::std::vector<Position> candidates = GenerateCircularPositions(targetPos, preferredRange, 16);
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

    ::std::vector<Position> escapePositions;
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

Position PositionManager::FindTankPosition(Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    // Tank should be in front of the target, facing it away from group
    float angle = target->GetOrientation();
    // Position slightly to the side to avoid frontal cone attacks
    angle += 0.2f; // Slight offset

    // Tank positioning distance (5 yards - melee range)
    float distance = 5.0f;

    Position tankPos = PositionUtils::CalculatePositionAtAngle(target->GetPosition(), distance, angle);

    // Validate position is reachable
    if (!ValidatePosition(tankPos, PositionValidation::BASIC))
    {
        // Try alternative angles if primary position invalid
        for (float offsetAngle : {-0.2f, 0.4f, -0.4f})
        {
            Position altPos = PositionUtils::CalculatePositionAtAngle(target->GetPosition(), distance, angle + offsetAngle);
            if (ValidatePosition(altPos, PositionValidation::BASIC))
                return altPos;
        }
    }

    return tankPos;
}

Position PositionManager::FindHealerPosition(const std::vector<Player*>& groupMembers)
{
    if (!_bot)
        return Position();

    // Healers should be at medium range (18 yards), central to the group
    float healerDistance = 18.0f;

    // Find the group center
    Position groupCenter;
    if (!groupMembers.empty())
    {
        float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
        uint32 count = 0;

        for (Player* member : groupMembers)
        {
            if (member && member != _bot)
            {
                sumX += member->GetPositionX();
                sumY += member->GetPositionY();
                sumZ += member->GetPositionZ();
                ++count;
            }
        }

        if (count > 0)
        {
            groupCenter.Relocate(sumX / count, sumY / count, sumZ / count);
        }
        else
        {
            groupCenter = _bot->GetPosition();
        }
    }
    else
    {
        groupCenter = _bot->GetPosition();
    }

    // Use spatial grid to find optimal position with LOS to most allies
    Map* map = _bot->GetMap();
    Position bestPos = groupCenter;
    int maxVisibleAllies = 0;

    if (map)
    {
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
        }

        if (spatialGrid)
        {
            // Query nearby players (lock-free!)
            std::vector<DoubleBufferedSpatialGrid::PlayerSnapshot> nearbyPlayers =
                spatialGrid->QueryNearbyPlayers(_bot->GetPosition(), 40.0f);

            // Test different positions around the group center
            for (float testAngle = 0; testAngle < 2 * M_PI; testAngle += static_cast<float>(M_PI / 4))
            {
                Position testPos = PositionUtils::CalculatePositionAtAngle(groupCenter, healerDistance, testAngle);

                if (!ValidatePosition(testPos, PositionValidation::BASIC))
                    continue;

                int visibleAllies = 0;

                // Count visible allies using snapshot positions
                for (auto const& snapshot : nearbyPlayers)
                {
                    if (snapshot.guid == _bot->GetGUID())
                        continue; // Skip self

                    // Simple distance check for LOS proxy
                    float dx = testPos.GetPositionX() - snapshot.position.GetPositionX();
                    float dy = testPos.GetPositionY() - snapshot.position.GetPositionY();
                    float dz = testPos.GetPositionZ() - snapshot.position.GetPositionZ();
                    float distSq = dx * dx + dy * dy + dz * dz;

                    // Healers need to be within 40 yards of allies
                    if (distSq < 40.0f * 40.0f)
                        ++visibleAllies;
                }

                if (visibleAllies > maxVisibleAllies)
                {
                    maxVisibleAllies = visibleAllies;
                    bestPos = testPos;
                }
            }
        }
    }

    return bestPos;
}

Position PositionManager::FindDpsPosition(Unit* target, PositionType type)
{
    if (!target)
        return _bot->GetPosition();

    // Route to appropriate DPS positioning based on type
    switch (type)
    {
        case PositionType::MELEE_COMBAT:
        case PositionType::FLANKING:  // Flanking positioning for melee DPS (behind target)
            return FindMeleePosition(target, true); // Prefer behind for DPS

        case PositionType::RANGED_DPS:  // Handles both ranged DPS and casters
            return FindRangedPosition(target, 25.0f); // Standard ranged distance

        default:
            // Default to ranged for unknown types
            return FindRangedPosition(target, 20.0f);
    }
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
    uint32 currentTime = GameTime::GetGameTimeMS();

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
    ::std::vector<Position> safePositions = GenerateCircularPositions(fromPos, minDistance, 16);

    for (const Position& pos : safePositions)
    {
        if (!IsInDangerZone(pos) && ValidatePosition(pos, PositionValidation::BASIC))
        {
            return pos;
        }
    }
    ::std::vector<Position> farPositions = GenerateCircularPositions(fromPos, minDistance * 2.0f, 16);
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
    // No lock needed - position data is per-bot instance data
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
        ::std::remove_if(_activeZones.begin(), _activeZones.end(),
            [currentTime](const AoEZone& zone) {
                return !zone.isActive || currentTime > zone.startTime + zone.duration;
            }),
        _activeZones.end()
    );
}

bool PositionManager::ValidatePosition(const Position& pos, PositionValidation flags)
{
    if ((static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::WALKABLE)) && !IsWalkablePosition(pos))
        return false;

    if (static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::STABLE_GROUND))
    {
        Map* map = _bot->GetMap();
        if (!map || !PositionUtils::IsPositionOnGround(pos, map))
            return false;
    }

    if (static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::NO_OBSTACLES))
    {
        if (!PositionUtils::CanWalkStraightLine(_bot->GetPosition(), pos, _bot->GetMap()))
            return false;
    }

    if ((static_cast<uint32>(flags) & static_cast<uint32>(PositionValidation::AVOID_AOE)) && IsInDangerZone(pos))
        return false;

    return true;
}

bool PositionManager::IsWalkablePosition(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return map->IsInWater(_bot->GetPhaseShift(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()) == false &&
           PositionUtils::IsPositionOnGround(pos, map);
}

float PositionManager::CalculateMovementCost(const Position& from, const Position& to)
{
    float distance = from.GetExactDist(&to);
    float heightDiff = ::std::abs(to.GetPositionZ() - from.GetPositionZ());

    float cost = distance;
    if (heightDiff > 2.0f)
        cost += heightDiff * 2.0f;  // Penalty for elevation changes
    if (!PositionUtils::CanWalkStraightLine(from, to, _bot->GetMap()))
        cost *= 1.5f;  // Penalty for indirect paths

    return cost;
}

PositionMovementResult PositionManager::HandleEmergencyMovement(const MovementContext& /* context */)
{
    _metrics.emergencyMoves++;

    Position emergencyPos = FindEmergencyEscapePosition();

    PositionMovementResult result;
    result.priority = MovementPriority::PRIORITY_CRITICAL;
    result.requiresSprint = true;

    return ExecuteMovement(emergencyPos, MovementPriority::PRIORITY_CRITICAL);
}

Position PositionManager::FindEmergencyEscapePosition()
{
    Position currentPos = _bot->GetPosition();
    ::std::vector<Position> escapePositions = GenerateCircularPositions(currentPos, EMERGENCY_DISTANCE, 12);

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

    float deviation = ::std::abs(distance - optimalDistance) / optimalDistance;
    return ::std::max(0.0f, 100.0f - (deviation * 100.0f));
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

    return ::std::max(0.0f, score);
}

float PositionManager::CalculateLineOfSightScore(const Position& /* pos */, const MovementContext& context)
{
    if (!context.target)
        return 100.0f;

    if (_bot->IsWithinLOSInMap(context.target))
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
        case PositionType::MELEE_COMBAT:
        case PositionType::FLANKING:
            {
                float behindAngle = PositionUtils::NormalizeAngle(targetAngle + M_PI);
                float angleDiff = ::std::abs(PositionUtils::NormalizeAngle(positionAngle - behindAngle));
                if (angleDiff < M_PI/6)  // Within 30 degrees behind
                    score += 30.0f;
            }
            break;

        case PositionType::TANKING:
            {
                float frontAngle = targetAngle;
                float angleDiff = ::std::abs(PositionUtils::NormalizeAngle(positionAngle - frontAngle));
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

    float deviation = ::std::abs(distanceToGroup - optimalDistance) / optimalDistance;
    return ::std::max(0.0f, 100.0f - (deviation * 50.0f));
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

::std::vector<Position> PositionManager::GenerateCircularPositions(const Position& center, float radius, uint32 count)
{
    ::std::vector<Position> positions;
    positions.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        float angle = (2.0f * M_PI * i) / count;
        Position pos = PositionUtils::CalculatePositionAtAngle(center, radius, angle);
        positions.push_back(pos);
    }
    return positions;
}

::std::vector<Position> PositionManager::GenerateArcPositions(const Position& center, float radius, float startAngle, float endAngle, uint32 count)
{
    ::std::vector<Position> positions;
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
    return ValidatePosition(pos, static_cast<PositionValidation>(
        static_cast<uint32>(PositionValidation::WALKABLE) |
        static_cast<uint32>(PositionValidation::NO_OBSTACLES)));
}

::std::vector<Position> PositionManager::CalculateWaypoints(const Position& from, const Position& to)
{
    ::std::vector<Position> waypoints;
    waypoints.push_back(from);
    waypoints.push_back(to);

    return waypoints;
}

void PositionManager::TrackPerformance(::std::chrono::microseconds duration, const ::std::string& /* operation */)
{
    if (duration > _metrics.maxEvaluationTime)
        _metrics.maxEvaluationTime = duration;

    auto currentTime = ::std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = ::std::chrono::duration_cast<::std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        uint32 totalEvaluations = _metrics.positionEvaluations.load();
        if (totalEvaluations > 0)
        {
            _metrics.averageEvaluationTime = ::std::chrono::microseconds(
                static_cast<uint64_t>(_metrics.averageEvaluationTime.count() * 0.9 + duration.count() * 0.1)
            );
        }
        _metrics.lastUpdate = currentTime;
    }
}

float PositionManager::CalculateEscapeScore(const Position& pos, const MovementContext& context)
{
    if (!_bot)
        return 0.0f;

    float score = 0.0f;

    // Base score for distance from current position
    float currentDistance = _bot->GetPosition().GetExactDist(&pos);
    score += ::std::min(currentDistance * 10.0f, 50.0f); // Max 50 points for distance

    // Lock-free spatial grid query for nearby enemies
    Map* map = _bot->GetMap();
    if (map)
    {
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            // Create grid on demand
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
        }

        if (spatialGrid)
        {
            // PHASE 5B: Thread-safe spatial grid query (replaces QueryNearbyCreatureGuids + ObjectAccessor)
            auto hostileSnapshots = SpatialGridQueryHelpers::FindHostileCreaturesInRange(_bot, 30.0f, true);

            // Check distance from nearby enemies
            float minEnemyDistance = 1000.0f;

            // Use snapshots for position calculations (lock-free!)
    for (auto const* snapshot : hostileSnapshots)
            {
                if (!snapshot)
                    continue;

                // Validate with Unit* for IsHostileTo check
                /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ Unit* enemy = ObjectAccessor::GetUnit(*_bot, snapshot->guid);
                if (!enemy || !_bot->IsHostileTo(enemy))
                    continue;

                float enemyDistance = pos.GetExactDist(snapshot->position);
                minEnemyDistance = ::std::min(minEnemyDistance, enemyDistance);

                // Higher score for positions farther from enemies
    if (enemyDistance > 0.0f)
                    score += ::std::min(enemyDistance * 5.0f, 30.0f); // Max 30 points per enemy
            }

            // Bonus for getting to safe range
    if (minEnemyDistance > 15.0f)
                score += 20.0f;
        }
    }

    // Check for line of sight to group members (we want to maintain LOS)
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && member != _bot && member->IsInWorld())
            {
                if (_bot->IsWithinLOSInMap(member))
                    score += 5.0f;
            }
        }
    }

    // Penalty for positions in AoE zones
    // TODO: Implement IsInAoEZone function
    // if (IsInAoEZone(pos))
    //     score -= 40.0f;

    // Penalty for positions too close to walls/obstacles
    MovementContext tempContext;
    if (!IsPositionSafe(pos, tempContext))
        score -= 20.0f;

    return ::std::max(score, 0.0f);
}

// PositionUtils implementation
Position PositionUtils::CalculatePositionAtAngle(const Position& center, float distance, float angle)
{
    Position result;
    result.m_positionX = center.GetPositionX() + distance * ::std::cos(angle);
    result.m_positionY = center.GetPositionY() + distance * ::std::sin(angle);
    result.m_positionZ = center.GetPositionZ();
    result.SetOrientation(angle);

    return result;
}

float PositionUtils::CalculateAngleBetween(const Position& from, const Position& to)
{
    float dx = to.GetPositionX() - from.GetPositionX();
    float dy = to.GetPositionY() - from.GetPositionY();

    return ::std::atan2(dy, dx);
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

    return bot->GetExactDistSq(target) <= (5.0f * 5.0f); // 25.0f
}

bool PositionUtils::IsInOptimalRange(Player* bot, Unit* target, PositionType type)
{
    if (!bot || !target)
        return false;

    float distSq = bot->GetExactDistSq(target);

    switch (type)
    {
        case PositionType::MELEE_COMBAT:
            return distSq <= (5.0f * 5.0f); // 25.0f
        case PositionType::RANGED_DPS:
            return distSq >= (20.0f * 20.0f) && distSq <= (40.0f * 40.0f); // 400.0f - 1600.0f
        case PositionType::HEALING:
            return distSq <= (40.0f * 40.0f); // 1600.0f
        case PositionType::KITING:
            return distSq >= (15.0f * 15.0f); // 225.0f
        default:
            return true;
    }
}

Position PositionUtils::CalculateGroupCenter(const ::std::vector<Player*>& players)
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

    float groundZ = map->GetHeight(PhasingHandler::GetEmptyPhaseShift(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
    return ::std::abs(pos.GetPositionZ() - groundZ) <= 2.0f;
}

Position PositionManager::PredictTargetPosition(Unit* target, float timeAhead)
{
    if (!target)
        return Position();

    // Get current position and movement info
    Position currentPos = target->GetPosition();

    // Calculate predicted position based on movement speed
    // Simple prediction: if target has movement speed, predict forward movement

    // Calculate predicted position based on current velocity
    float speed = target->GetSpeed(MOVE_RUN); // Get movement speed
    if (speed <= 0.0f)
        return currentPos;

    // Get current facing direction
    float orientation = target->GetOrientation();

    // Calculate movement distance
    float distance = speed * timeAhead;

    // Project position forward based on current facing direction
    Position predictedPos;
    predictedPos.m_positionX = currentPos.GetPositionX() + distance * ::std::cos(orientation);
    predictedPos.m_positionY = currentPos.GetPositionY() + distance * ::std::sin(orientation);
    predictedPos.m_positionZ = currentPos.GetPositionZ();
    predictedPos.SetOrientation(orientation);

    // Validate the predicted position (check if walkable)
    if (_bot && _bot->GetMap())
    {
        float groundZ = _bot->GetMap()->GetHeight(PhasingHandler::GetEmptyPhaseShift(),
                                                 predictedPos.GetPositionX(),
                                                 predictedPos.GetPositionY(),
                                                 predictedPos.GetPositionZ());

        // If ground height is reasonable, use it
    if (::std::abs(groundZ - predictedPos.GetPositionZ()) <= 10.0f)
            predictedPos.m_positionZ = groundZ;
    }

    return predictedPos;
}

bool PositionUtils::CanWalkStraightLine(const Position& from, const Position& to, Map* map)
{
    if (!map)
        return false;

    // Use TrinityCore API properly with PhaseShift
    // We need a PhaseShift - for now, create a default one or get from a WorldObject
    PhaseShift phaseShift;
    return map->isInLineOfSight(phaseShift, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                               to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(),
                               LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing);
}

} // namespace Playerbot/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Implementation of 13 missing PositionManager methods with enterprise-grade quality
 * These methods are ready to be added to the main PositionManager.cpp file
 */

#include "PositionManager.h"
#include "Player.h"
#include "Unit.h"
#include "Map.h"
#include "Log.h"
#include "Group.h"
#include "PathGenerator.h"
#include "PhasingHandler.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "../../Spatial/SpatialGridManager.h"
#include "../../Spatial/SpatialGridQueryHelpers.h"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace Playerbot
{

// ========================================================================================
// Method 1: CalculateStrafePosition
// Calculate strafe position for circle-strafing around a target
// ========================================================================================
Position PositionManager::CalculateStrafePosition(Unit* target, bool strafeLeft)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    auto startTime = ::std::chrono::steady_clock::now();

    Position currentPos = _bot->GetPosition();
    Position targetPos = target->GetPosition();

    // Calculate current angle from target to bot
    float currentAngle = PositionUtils::CalculateAngleBetween(targetPos, currentPos);
    float currentDistance = currentPos.GetExactDist(&targetPos);

    // Maintain optimal combat distance while strafing
    float optimalDistance = currentDistance;
    if (optimalDistance < 3.0f)
        optimalDistance = 4.0f;  // Minimum strafe distance
    else if (optimalDistance > 8.0f)
        optimalDistance = 7.0f;  // Maximum strafe distance for melee

    // Calculate strafe angle (15-30 degrees based on movement speed)
    float strafeAngleStep = M_PI / 12.0f;  // 15 degrees default

    // Adjust strafe angle based on target's turn rate to maintain position
    float targetTurnRate = 0.0f;
    if (target->IsMoving())
    {
        // Increase strafe angle if target is turning quickly
        strafeAngleStep = M_PI / 8.0f;  // 22.5 degrees for moving targets
    }

    // Calculate new position angle
    float newAngle = strafeLeft ?
        PositionUtils::NormalizeAngle(currentAngle - strafeAngleStep) :
        PositionUtils::NormalizeAngle(currentAngle + strafeAngleStep);

    // Generate strafe position
    Position strafePos = PositionUtils::CalculatePositionAtAngle(targetPos, optimalDistance, newAngle);

    // Validate the strafe position
    if (!ValidatePosition(strafePos, PositionValidation::BASIC))
    {
        // Try alternative strafe with smaller angle
        strafeAngleStep *= 0.5f;
        newAngle = strafeLeft ?
            PositionUtils::NormalizeAngle(currentAngle - strafeAngleStep) :
            PositionUtils::NormalizeAngle(currentAngle + strafeAngleStep);
        strafePos = PositionUtils::CalculatePositionAtAngle(targetPos, optimalDistance, newAngle);

        // If still invalid, return current position
        if (!ValidatePosition(strafePos, PositionValidation::BASIC))
        {
            TC_LOG_DEBUG("playerbot.position", "Bot {} - Unable to find valid strafe position", _bot->GetName());
            return currentPos;
        }
    }

    // Adjust Z coordinate for terrain
    Map* map = _bot->GetMap();
    if (map)
    {
        float groundZ = map->GetHeight(_bot->GetPhaseShift(),
                                      strafePos.GetPositionX(),
                                      strafePos.GetPositionY(),
                                      strafePos.GetPositionZ());
        strafePos.m_positionZ = groundZ + 0.5f;
    }

    auto endTime = ::std::chrono::steady_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "CalculateStrafePosition");

    TC_LOG_DEBUG("playerbot.position", "Bot {} calculated {} strafe to ({:.2f}, {:.2f}, {:.2f})",
                 _bot->GetName(), strafeLeft ? "left" : "right",
                 strafePos.GetPositionX(), strafePos.GetPositionY(), strafePos.GetPositionZ());

    return strafePos;
}

// ========================================================================================
// Method 2: FindEscapePosition
// Find optimal escape position when threatened by multiple enemies
// ========================================================================================
Position PositionManager::FindEscapePosition(const ::std::vector<Unit*>& threats)
{
    if (!_bot || threats.empty())
        return _bot->GetPosition();

    auto startTime = ::std::chrono::steady_clock::now();
    Position currentPos = _bot->GetPosition();

    // Calculate threat vector (average direction of all threats)
    float threatVectorX = 0.0f;
    float threatVectorY = 0.0f;
    float totalThreatWeight = 0.0f;

    for (Unit* threat : threats)
    {
        if (!threat)
            continue;

        Position threatPos = threat->GetPosition();
        float distance = currentPos.GetExactDist(&threatPos);

        // Weight closer threats more heavily
        float weight = 1.0f;
        if (distance < 10.0f)
            weight = 3.0f;
        else if (distance < 20.0f)
            weight = 2.0f;

        // Add threat health/damage potential
        if (threat->GetHealth() > 0)
            weight *= (static_cast<float>(threat->GetHealth()) / static_cast<float>(threat->GetMaxHealth()));

        float dx = currentPos.GetPositionX() - threatPos.GetPositionX();
        float dy = currentPos.GetPositionY() - threatPos.GetPositionY();

        // Normalize direction vector
        float length = ::std::sqrt(dx * dx + dy * dy);
        if (length > 0.0f)
        {
            dx /= length;
            dy /= length;
        }

        threatVectorX += dx * weight;
        threatVectorY += dy * weight;
        totalThreatWeight += weight;
    }

    // Calculate escape direction (opposite of threat vector)
    if (totalThreatWeight > 0.0f)
    {
        threatVectorX /= totalThreatWeight;
        threatVectorY /= totalThreatWeight;
    }

    float escapeAngle = ::std::atan2(threatVectorY, threatVectorX);

    // Generate escape positions at various distances
    ::std::vector<PositionInfo> escapePositions;
    MovementContext escapeContext;
    escapeContext.bot = _bot;
    escapeContext.emergencyMode = true;
    escapeContext.validationFlags = PositionValidation::BASIC;

    // Try multiple escape distances
    for (float distance : {15.0f, 20.0f, 25.0f, 30.0f})
    {
        // Primary escape direction
        Position escapePos = PositionUtils::CalculatePositionAtAngle(currentPos, distance, escapeAngle);

        // Also try slight variations for better path options
        for (float angleOffset : {0.0f, -M_PI/6, M_PI/6})
        {
            float testAngle = PositionUtils::NormalizeAngle(escapeAngle + angleOffset);
            Position testPos = PositionUtils::CalculatePositionAtAngle(currentPos, distance, testAngle);

            if (ValidatePosition(testPos, escapeContext.validationFlags))
            {
                PositionInfo info = EvaluatePosition(testPos, escapeContext);

                // Calculate minimum distance to all threats
                float minThreatDistance = 1000.0f;
                for (Unit* threat : threats)
                {
                    if (threat)
                    {
                        float threatDist = testPos.GetExactDist(threat);
                        minThreatDistance = ::std::min(minThreatDistance, threatDist);
                    }
                }

                // Bonus score for distance from threats
                info.score += minThreatDistance * 2.0f;

                escapePositions.push_back(info);
            }
        }
    }

    // Sort by score and pick best escape position
    if (!escapePositions.empty())
    {
        ::std::sort(escapePositions.begin(), escapePositions.end(), ::std::greater<PositionInfo>());

        auto endTime = ::std::chrono::steady_clock::now();
        auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
        TrackPerformance(duration, "FindEscapePosition");

        Position bestEscape = escapePositions[0].position;
        TC_LOG_DEBUG("playerbot.position", "Bot {} found escape position at ({:.2f}, {:.2f}, {:.2f}) from {} threats",
                     _bot->GetName(), bestEscape.GetPositionX(), bestEscape.GetPositionY(),
                     bestEscape.GetPositionZ(), threats.size());

        return bestEscape;
    }

    // Fallback: move directly away from nearest threat
    Unit* nearestThreat = threats[0];
    float nearestDistance = currentPos.GetExactDist(nearestThreat);

    for (Unit* threat : threats)
    {
        if (threat)
        {
            float dist = currentPos.GetExactDist(threat);
            if (dist < nearestDistance)
            {
                nearestThreat = threat;
                nearestDistance = dist;
            }
        }
    }

    float fallbackAngle = PositionUtils::CalculateAngleBetween(nearestThreat->GetPosition(), currentPos);
    Position fallbackPos = PositionUtils::CalculatePositionAtAngle(currentPos, EMERGENCY_DISTANCE, fallbackAngle);

    TC_LOG_DEBUG("playerbot.position", "Bot {} using fallback escape position", _bot->GetName());
    return fallbackPos;
}

// ========================================================================================
// Method 3: FindFormationPosition
// Find position based on group formation type
// ========================================================================================
Position PositionManager::FindFormationPosition(const ::std::vector<Player*>& groupMembers, PositionType formationType)
{
    if (!_bot || groupMembers.empty())
        return _bot->GetPosition();

    auto startTime = ::std::chrono::steady_clock::now();

    // Find formation leader (usually tank or highest level)
    Player* leader = nullptr;
    uint32 highestLevel = 0;

    for (Player* member : groupMembers)
    {
        if (!member || member == _bot)
            continue;

        if (member->GetLevel() > highestLevel)
        {
            leader = member;
            highestLevel = member->GetLevel();
        }
    }

    if (!leader)
        leader = groupMembers[0];

    Position leaderPos = leader->GetPosition();
    float leaderOrientation = leader->GetOrientation();
    Position formationPos = _bot->GetPosition();

    // Determine bot's position in formation based on role
    ThreatRole botRole = _threatManager ? _threatManager->GetRole() : ThreatRole::DPS;

    switch (formationType)
    {
        case PositionType::FORMATION_LINE:
        {
            // Line formation perpendicular to leader's facing
            uint32 positionIndex = 0;
            for (size_t i = 0; i < groupMembers.size(); ++i)
            {
                if (groupMembers[i] == _bot)
                {
                    positionIndex = i;
                    break;
                }
            }

            float spacing = 5.0f;
            float offset = (positionIndex - groupMembers.size() / 2.0f) * spacing;
            float perpAngle = PositionUtils::NormalizeAngle(leaderOrientation + M_PI/2);

            formationPos.m_positionX = leaderPos.GetPositionX() + offset * ::std::cos(perpAngle);
            formationPos.m_positionY = leaderPos.GetPositionY() + offset * ::std::sin(perpAngle);
            formationPos.m_positionZ = leaderPos.GetPositionZ();
            break;
        }

        case PositionType::FORMATION_ARROW:
        {
            // V-formation with leader at point
            float baseDistance = 8.0f;
            float spreadAngle = M_PI / 6;  // 30 degrees

            switch (botRole)
            {
                case ThreatRole::TANK:
                    // Tanks at the front
                    formationPos = leaderPos;
                    break;

                case ThreatRole::HEALER:
                    // Healers in the middle-back
                    formationPos = PositionUtils::CalculatePositionAtAngle(
                        leaderPos, baseDistance * 1.5f,
                        PositionUtils::NormalizeAngle(leaderOrientation + M_PI));
                    break;

                case ThreatRole::DPS:
                {
                    // DPS on the wings
                    bool leftWing = (_bot->GetGUID().GetCounter() % 2) == 0;
                    float wingAngle = leftWing ?
                        PositionUtils::NormalizeAngle(leaderOrientation + M_PI - spreadAngle) :
                        PositionUtils::NormalizeAngle(leaderOrientation + M_PI + spreadAngle);
                    formationPos = PositionUtils::CalculatePositionAtAngle(leaderPos, baseDistance, wingAngle);
                    break;
                }

                default:
                    formationPos = PositionUtils::CalculatePositionAtAngle(
                        leaderPos, baseDistance,
                        PositionUtils::NormalizeAngle(leaderOrientation + M_PI));
                    break;
            }
            break;
        }

        case PositionType::FORMATION_CIRCLE:
        {
            // Defensive circle formation
            uint32 memberCount = groupMembers.size();
            uint32 myIndex = 0;

            for (uint32 i = 0; i < memberCount; ++i)
            {
                if (groupMembers[i] == _bot)
                {
                    myIndex = i;
                    break;
                }
            }

            float radius = 10.0f + (memberCount * 1.5f);  // Scale with group size
            float angleStep = (2.0f * M_PI) / memberCount;
            float myAngle = angleStep * myIndex;

            Position groupCenter = PositionUtils::CalculateGroupCenter(groupMembers);
            formationPos = PositionUtils::CalculatePositionAtAngle(groupCenter, radius, myAngle);
            break;
        }

        case PositionType::FORMATION_SPREAD:
        {
            // Spread formation for avoiding AoE
            float spreadDistance = 8.0f;

            // Find position with maximum distance from other members
            ::std::vector<Position> candidates = GenerateCircularPositions(leaderPos, 15.0f, 16);
            float bestScore = 0.0f;

            for (const Position& candidate : candidates)
            {
                if (!ValidatePosition(candidate, PositionValidation::BASIC))
                    continue;

                float minDistance = 1000.0f;
                for (Player* member : groupMembers)
                {
                    if (member && member != _bot)
                    {
                        float dist = candidate.GetExactDist(member);
                        minDistance = ::std::min(minDistance, dist);
                    }
                }

                if (minDistance > spreadDistance && minDistance < 30.0f)
                {
                    float score = minDistance;
                    if (score > bestScore)
                    {
                        bestScore = score;
                        formationPos = candidate;
                    }
                }
            }
            break;
        }

        default:
            // Default: maintain relative position to leader
            formationPos = PositionUtils::CalculatePositionAtAngle(
                leaderPos, 10.0f,
                PositionUtils::NormalizeAngle(leaderOrientation + M_PI));
            break;
    }

    // Validate and adjust formation position
    if (!ValidatePosition(formationPos, PositionValidation::BASIC))
    {
        // Find nearest valid position
        formationPos = PositionUtils::GetNearestWalkablePosition(formationPos, _bot->GetMap(), 10.0f);
    }

    // Adjust Z coordinate for terrain
    Map* map = _bot->GetMap();
    if (map)
    {
        float groundZ = map->GetHeight(_bot->GetPhaseShift(),
                                      formationPos.GetPositionX(),
                                      formationPos.GetPositionY(),
                                      formationPos.GetPositionZ());
        formationPos.m_positionZ = groundZ + 0.5f;
    }

    auto endTime = ::std::chrono::steady_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "FindFormationPosition");

    TC_LOG_DEBUG("playerbot.position", "Bot {} found formation position at ({:.2f}, {:.2f}, {:.2f}) for {} formation",
                 _bot->GetName(), formationPos.GetPositionX(), formationPos.GetPositionY(),
                 formationPos.GetPositionZ(), static_cast<uint32>(formationType));

    return formationPos;
}

// ========================================================================================
// Method 4: FindRangePosition
// Find position at specific range with optional angle preference
// ========================================================================================
Position PositionManager::FindRangePosition(Unit* target, float minRange, float maxRange, float preferredAngle)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    auto startTime = ::std::chrono::steady_clock::now();

    Position targetPos = target->GetPosition();
    float optimalRange = (minRange + maxRange) / 2.0f;

    // Generate candidate positions at optimal range
    ::std::vector<PositionInfo> candidates;
    MovementContext context;
    context.bot = _bot;
    context.target = target;
    context.desiredType = PositionType::RANGED_DPS;
    context.preferredRange = optimalRange;
    context.validationFlags = PositionValidation::COMBAT;

    // If preferred angle specified, prioritize that direction
    if (preferredAngle != 0.0f)
    {
        // Try preferred angle first
        Position preferredPos = PositionUtils::CalculatePositionAtAngle(targetPos, optimalRange, preferredAngle);

        if (ValidatePosition(preferredPos, context.validationFlags))
        {
            PositionInfo info = EvaluatePosition(preferredPos, context);
            info.score += 50.0f;  // Bonus for preferred angle
            candidates.push_back(info);
        }

        // Try positions near preferred angle
        for (float offset : {-M_PI/12, M_PI/12, -M_PI/6, M_PI/6})
        {
            float testAngle = PositionUtils::NormalizeAngle(preferredAngle + offset);
            Position testPos = PositionUtils::CalculatePositionAtAngle(targetPos, optimalRange, testAngle);

            if (ValidatePosition(testPos, context.validationFlags))
            {
                PositionInfo info = EvaluatePosition(testPos, context);
                info.score += 25.0f * (1.0f - ::std::abs(offset) / M_PI);  // Score based on angle proximity
                candidates.push_back(info);
            }
        }
    }

    // Generate positions in range band
    uint32 numPositions = 16;
    for (uint32 i = 0; i < numPositions; ++i)
    {
        float angle = (2.0f * M_PI * i) / numPositions;

        // Try different ranges within min/max
        for (float range : {minRange, optimalRange, maxRange})
        {
            Position pos = PositionUtils::CalculatePositionAtAngle(targetPos, range, angle);

            if (ValidatePosition(pos, context.validationFlags))
            {
                PositionInfo info = EvaluatePosition(pos, context);

                // Prefer optimal range
                float rangeDiff = ::std::abs(range - optimalRange);
                info.score += (1.0f - rangeDiff / (maxRange - minRange)) * 20.0f;

                // Check line of sight
                if (HasLineOfSight(pos, targetPos))
                    info.score += 30.0f;

                candidates.push_back(info);
            }
        }
    }

    // Sort and select best position
    if (!candidates.empty())
    {
        ::std::sort(candidates.begin(), candidates.end(), ::std::greater<PositionInfo>());

        Position bestPos = candidates[0].position;

        // Adjust Z for terrain
        Map* map = _bot->GetMap();
        if (map)
        {
            float groundZ = map->GetHeight(_bot->GetPhaseShift(),
                                          bestPos.GetPositionX(),
                                          bestPos.GetPositionY(),
                                          bestPos.GetPositionZ());
            bestPos.m_positionZ = groundZ + 0.5f;
        }

        auto endTime = ::std::chrono::steady_clock::now();
        auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
        TrackPerformance(duration, "FindRangePosition");

        TC_LOG_DEBUG("playerbot.position", "Bot {} found range position at ({:.2f}, {:.2f}, {:.2f}) range [{:.1f}-{:.1f}]",
                     _bot->GetName(), bestPos.GetPositionX(), bestPos.GetPositionY(),
                     bestPos.GetPositionZ(), minRange, maxRange);

        return bestPos;
    }

    // Fallback to current position if no valid positions found
    TC_LOG_DEBUG("playerbot.position", "Bot {} unable to find valid range position, maintaining current",
                 _bot->GetName());
    return _bot->GetPosition();
}

// ========================================================================================
// Method 5: FindSupportPosition
// Find position for support role (buffers, crowd control, etc.)
// ========================================================================================
Position PositionManager::FindSupportPosition(const ::std::vector<Player*>& groupMembers)
{
    if (!_bot)
        return Position();

    auto startTime = ::std::chrono::steady_clock::now();

    // Support position priorities:
    // 1. Central to group for maximum buff/support coverage
    // 2. Safe distance from enemies
    // 3. Line of sight to most allies
    // 4. Quick escape routes available

    Position groupCenter = PositionUtils::CalculateGroupCenter(groupMembers);
    float supportRange = 20.0f;  // Typical support ability range

    ::std::vector<PositionInfo> candidates;
    MovementContext context;
    context.bot = _bot;
    context.groupMembers = groupMembers;
    context.desiredType = PositionType::SUPPORT;
    context.botRole = ThreatRole::SUPPORT;
    context.preferredRange = supportRange;
    context.validationFlags = PositionValidation::SAFE;

    // Generate candidate positions around group center
    ::std::vector<Position> positions = GenerateCircularPositions(groupCenter, supportRange * 0.7f, 12);

    for (const Position& pos : positions)
    {
        if (!ValidatePosition(pos, context.validationFlags))
            continue;

        PositionInfo info = EvaluatePosition(pos, context);

        // Count allies in support range
        uint32 alliesInRange = 0;
        uint32 alliesWithLOS = 0;

        for (Player* ally : groupMembers)
        {
            if (!ally || ally == _bot)
                continue;

            float distance = pos.GetExactDist(ally);
            if (distance <= supportRange)
            {
                alliesInRange++;

                // Simple LOS check
                if (HasLineOfSight(pos, ally->GetPosition()))
                    alliesWithLOS++;
            }
        }

        // Score based on coverage
        float coverageScore = (static_cast<float>(alliesInRange) / groupMembers.size()) * 50.0f;
        float losScore = (static_cast<float>(alliesWithLOS) / groupMembers.size()) * 30.0f;
        info.score += coverageScore + losScore;

        // Check for escape routes (multiple valid positions nearby)
        uint32 escapeRoutes = 0;
        for (float angle = 0; angle < 2 * M_PI; angle += M_PI / 4)
        {
            Position escapePos = PositionUtils::CalculatePositionAtAngle(pos, 10.0f, angle);
            if (ValidatePosition(escapePos, PositionValidation::BASIC))
                escapeRoutes++;
        }

        info.score += escapeRoutes * 5.0f;

        candidates.push_back(info);
    }

    // Also consider positions slightly behind the group
    if (_bot->GetVictim())
    {
        float retreatAngle = PositionUtils::CalculateAngleBetween(_bot->GetVictim()->GetPosition(), groupCenter);
        retreatAngle = PositionUtils::NormalizeAngle(retreatAngle + M_PI);

        Position behindPos = PositionUtils::CalculatePositionAtAngle(groupCenter, supportRange * 0.5f, retreatAngle);
        if (ValidatePosition(behindPos, context.validationFlags))
        {
            PositionInfo info = EvaluatePosition(behindPos, context);
            info.score += 20.0f;  // Bonus for being behind group
            candidates.push_back(info);
        }
    }

    // Select best support position
    Position supportPos = _bot->GetPosition();

    if (!candidates.empty())
    {
        ::std::sort(candidates.begin(), candidates.end(), ::std::greater<PositionInfo>());
        supportPos = candidates[0].position;

        // Adjust Z for terrain
        Map* map = _bot->GetMap();
        if (map)
        {
            float groundZ = map->GetHeight(_bot->GetPhaseShift(),
                                          supportPos.GetPositionX(),
                                          supportPos.GetPositionY(),
                                          supportPos.GetPositionZ());
            supportPos.m_positionZ = groundZ + 0.5f;
        }
    }

    auto endTime = ::std::chrono::steady_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "FindSupportPosition");

    TC_LOG_DEBUG("playerbot.position", "Bot {} found support position at ({:.2f}, {:.2f}, {:.2f})",
                 _bot->GetName(), supportPos.GetPositionX(), supportPos.GetPositionY(), supportPos.GetPositionZ());

    return supportPos;
}

// ========================================================================================
// Method 6: GetPositionSuccessRate
// Get historical success rate for positions near the specified location
// ========================================================================================
float PositionManager::GetPositionSuccessRate(const Position& pos, float radius)
{
    ::std::lock_guard<Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_AI_STATE>> lock(_mutex);

    if (_positionAttempts.empty())
        return 0.5f;  // Default 50% if no history

    // Create position key based on grid cell
    float gridSize = radius;
    int32 gridX = static_cast<int32>(pos.GetPositionX() / gridSize);
    int32 gridY = static_cast<int32>(pos.GetPositionY() / gridSize);
    int32 gridZ = static_cast<int32>(pos.GetPositionZ() / gridSize);

    uint32 successCount = 0;
    uint32 totalAttempts = 0;

    // Check this grid cell and adjacent cells within radius
    for (int32 dx = -1; dx <= 1; ++dx)
    {
        for (int32 dy = -1; dy <= 1; ++dy)
        {
            for (int32 dz = -1; dz <= 1; ++dz)
            {
                ::std::string key = ::std::to_string(gridX + dx) + "_" +
                                  ::std::to_string(gridY + dy) + "_" +
                                  ::std::to_string(gridZ + dz);

                auto attemptIt = _positionAttempts.find(key);
                auto successIt = _positionSuccessRates.find(key);

                if (attemptIt != _positionAttempts.end())
                {
                    totalAttempts += attemptIt->second;

                    if (successIt != _positionSuccessRates.end())
                    {
                        successCount += static_cast<uint32>(successIt->second * attemptIt->second);
                    }
                }
            }
        }
    }

    if (totalAttempts == 0)
        return 0.5f;  // Default if no data

    float successRate = static_cast<float>(successCount) / static_cast<float>(totalAttempts);

    TC_LOG_DEBUG("playerbot.position", "Bot {} position ({:.1f}, {:.1f}) success rate: {:.2f}% ({}/{} attempts)",
                 _bot->GetName(), pos.GetPositionX(), pos.GetPositionY(),
                 successRate * 100.0f, successCount, totalAttempts);

    return successRate;
}

// ========================================================================================
// Method 7: HasLineOfSight
// Check if there is line of sight between two positions
// ========================================================================================
bool PositionManager::HasLineOfSight(const Position& from, const Position& to)
{
    if (!_bot || !_bot->GetMap())
        return false;

    Map* map = _bot->GetMap();

    // Use TrinityCore's line of sight check with proper phase handling
    return map->isInLineOfSight(
        _bot->GetPhaseShift(),
        from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + 2.0f,  // Add height for eye level
        to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + 2.0f,
        LINEOFSIGHT_ALL_CHECKS,
        VMAP::ModelIgnoreFlags::Nothing
    );
}

// ========================================================================================
// Method 8: IsInEmergencyPosition
// Check if bot is in a critical/emergency position requiring immediate movement
// ========================================================================================
bool PositionManager::IsInEmergencyPosition()
{
    if (!_bot)
        return false;

    Position currentPos = _bot->GetPosition();

    // Check 1: In active AoE zone
    if (IsInDangerZone(currentPos))
    {
        TC_LOG_DEBUG("playerbot.position", "Bot {} in emergency: Active AoE zone", _bot->GetName());
        return true;
    }

    // Check 2: Health critically low and surrounded
    if (_bot->GetHealthPct() < 30.0f)
    {
        uint32 nearbyEnemies = 0;
        Map* map = _bot->GetMap();

        if (map)
        {
            DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
            if (spatialGrid)
            {
                auto hostileSnapshots = SpatialGridQueryHelpers::FindHostileCreaturesInRange(_bot, 10.0f, true);
                nearbyEnemies = hostileSnapshots.size();
            }
        }

        if (nearbyEnemies >= 3)
        {
            TC_LOG_DEBUG("playerbot.position", "Bot {} in emergency: Low health with {} enemies nearby",
                        _bot->GetName(), nearbyEnemies);
            return true;
        }
    }

    // Check 3: Falling or in water/lava
    if (_bot->IsFalling())
    {
        TC_LOG_DEBUG("playerbot.position", "Bot {} in emergency: Falling", _bot->GetName());
        return true;
    }

    Map* map = _bot->GetMap();
    if (map && map->IsInWater(_bot->GetPhaseShift(), currentPos.GetPositionX(),
                              currentPos.GetPositionY(), currentPos.GetPositionZ()))
    {
        // Check if it's harmful liquid (lava, slime, etc.)
        LiquidData liquidData;
        map->GetLiquidData(_bot->GetPhaseShift(), currentPos.GetPositionX(),
                          currentPos.GetPositionY(), currentPos.GetPositionZ(),
                          &liquidData, {});

        if (liquidData.Status & (MAP_LIQUID_STATUS_MAGMA | MAP_LIQUID_STATUS_SLIME))
        {
            TC_LOG_DEBUG("playerbot.position", "Bot {} in emergency: In harmful liquid", _bot->GetName());
            return true;
        }
    }

    // Check 4: Stunned/rooted in danger
    if (_bot->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOTED))
    {
        // Check if there are incoming projectiles or enemies approaching
        if (_bot->GetVictim() && _bot->GetVictim()->GetExactDist(_bot) < 5.0f)
        {
            TC_LOG_DEBUG("playerbot.position", "Bot {} in emergency: Stunned/rooted with enemy nearby",
                        _bot->GetName());
            return true;
        }
    }

    // Check 5: Tank lost aggro and healer/dps is being attacked
    if (_threatManager)
    {
        ThreatRole role = _threatManager->GetRole();
        if (role == ThreatRole::HEALER || role == ThreatRole::DPS)
        {
            if (_bot->GetVictim() && _bot->GetVictim()->GetVictim() == _bot)
            {
                TC_LOG_DEBUG("playerbot.position", "Bot {} in emergency: Non-tank being focused", _bot->GetName());
                return true;
            }
        }
    }

    return false;
}

// ========================================================================================
// Method 9: RecordPositionFailure
// Record a position attempt that failed for learning
// ========================================================================================
void PositionManager::RecordPositionFailure(const Position& pos, const ::std::string& reason)
{
    ::std::lock_guard<Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_AI_STATE>> lock(_mutex);

    // Create position key based on 5-yard grid cells
    float gridSize = 5.0f;
    int32 gridX = static_cast<int32>(pos.GetPositionX() / gridSize);
    int32 gridY = static_cast<int32>(pos.GetPositionY() / gridSize);
    int32 gridZ = static_cast<int32>(pos.GetPositionZ() / gridSize);

    ::std::string key = ::std::to_string(gridX) + "_" + ::std::to_string(gridY) + "_" + ::std::to_string(gridZ);

    // Update attempts count
    _positionAttempts[key]++;

    // Update success rate (decrease it)
    float currentRate = _positionSuccessRates[key];
    float newRate = currentRate * 0.9f;  // Decay success rate by 10%
    _positionSuccessRates[key] = newRate;

    TC_LOG_DEBUG("playerbot.position", "Bot {} recorded position failure at ({:.1f}, {:.1f}): {} (success rate now {:.2f}%)",
                 _bot->GetName(), pos.GetPositionX(), pos.GetPositionY(), reason, newRate * 100.0f);

    // Clean up old entries if map gets too large (memory management)
    if (_positionAttempts.size() > 1000)
    {
        // Remove entries with low attempt counts (likely old/unused positions)
        auto it = _positionAttempts.begin();
        while (it != _positionAttempts.end() && _positionAttempts.size() > 500)
        {
            if (it->second < 5)
            {
                _positionSuccessRates.erase(it->first);
                it = _positionAttempts.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

// ========================================================================================
// Method 10: RecordPositionSuccess
// Record a successful position for learning
// ========================================================================================
void PositionManager::RecordPositionSuccess(const Position& pos, PositionType type)
{
    ::std::lock_guard<Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_AI_STATE>> lock(_mutex);

    // Create position key based on 5-yard grid cells
    float gridSize = 5.0f;
    int32 gridX = static_cast<int32>(pos.GetPositionX() / gridSize);
    int32 gridY = static_cast<int32>(pos.GetPositionY() / gridSize);
    int32 gridZ = static_cast<int32>(pos.GetPositionZ() / gridSize);

    ::std::string key = ::std::to_string(gridX) + "_" + ::std::to_string(gridY) + "_" + ::std::to_string(gridZ);

    // Update attempts count
    _positionAttempts[key]++;

    // Update success rate (increase it)
    float currentRate = _positionSuccessRates[key];
    float newRate = currentRate * 0.9f + 0.1f;  // Move towards 100% success
    _positionSuccessRates[key] = newRate;

    TC_LOG_DEBUG("playerbot.position", "Bot {} recorded position success at ({:.1f}, {:.1f}) for {} (success rate now {:.2f}%)",
                 _bot->GetName(), pos.GetPositionX(), pos.GetPositionY(),
                 static_cast<uint32>(type), newRate * 100.0f);

    // Award bonus to nearby positions (they're likely good too)
    for (int32 dx = -1; dx <= 1; ++dx)
    {
        for (int32 dy = -1; dy <= 1; ++dy)
        {
            if (dx == 0 && dy == 0)
                continue;

            ::std::string nearbyKey = ::std::to_string(gridX + dx) + "_" +
                                    ::std::to_string(gridY + dy) + "_" +
                                    ::std::to_string(gridZ);

            // Slightly improve nearby positions
            float nearbyRate = _positionSuccessRates[nearbyKey];
            _positionSuccessRates[nearbyKey] = nearbyRate * 0.95f + 0.05f;  // Small improvement
        }
    }
}

// ========================================================================================
// Method 11: ShouldCircleStrafe
// Determine if circle strafing is beneficial against target
// ========================================================================================
bool PositionManager::ShouldCircleStrafe(Unit* target)
{
    if (!_bot || !target)
        return false;

    // Don't circle strafe if casting
    if (_bot->IsNonMeleeSpellCast(false))
        return false;

    // Check if we're in melee range
    float distance = _bot->GetExactDist(target);
    if (distance > 8.0f)
        return false;  // Too far for circle strafing

    // Check target type and behavior
    bool shouldStrafe = false;

    // Factor 1: Target is casting (strafe to avoid frontal cone)
    if (target->IsNonMeleeSpellCast(false))
    {
        // Check if it's a frontal cone spell (would need spell ID checks)
        shouldStrafe = true;
        TC_LOG_DEBUG("playerbot.position", "Bot {} should circle strafe - target casting", _bot->GetName());
    }

    // Factor 2: Target is a large creature (dragons, giants)
    if (target->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creature = target->ToCreature();
        if (creature)
        {
            // Large creatures often have cleave/breath attacks
            float targetScale = 1.0f;  // Would need to get actual scale
            if (targetScale > 1.5f)
            {
                shouldStrafe = true;
                TC_LOG_DEBUG("playerbot.position", "Bot {} should circle strafe - large target", _bot->GetName());
            }
        }
    }

    // Factor 3: We're a melee DPS (benefit from positional advantages)
    if (_threatManager && _threatManager->GetRole() == ThreatRole::DPS)
    {
        // Melee DPS should try to stay behind target
        float angle = target->GetRelativeAngle(_bot);
        if (::std::abs(angle) < M_PI / 2)  // We're in front
        {
            shouldStrafe = true;
            TC_LOG_DEBUG("playerbot.position", "Bot {} should circle strafe - need better position", _bot->GetName());
        }
    }

    // Factor 4: Target turning speed (don't strafe if target turns too fast)
    if (shouldStrafe && target->IsMoving())
    {
        // If target is turning rapidly, strafing might not help
        float targetSpeed = target->GetSpeed(MOVE_TURN_RATE);
        if (targetSpeed > 3.0f)  // High turn rate
        {
            shouldStrafe = false;
            TC_LOG_DEBUG("playerbot.position", "Bot {} shouldn't circle strafe - target turning too fast", _bot->GetName());
        }
    }

    // Factor 5: Check cooldowns and resources
    if (shouldStrafe)
    {
        // Make sure we have the resources to maintain strafe
        if (_bot->GetPowerPct(POWER_ENERGY) < 30.0f || _bot->GetHealthPct() < 50.0f)
        {
            shouldStrafe = false;
            TC_LOG_DEBUG("playerbot.position", "Bot {} shouldn't circle strafe - low resources", _bot->GetName());
        }
    }

    return shouldStrafe;
}

// ========================================================================================
// Method 12: ShouldMaintainGroupProximity
// Determine if bot should stay close to group
// ========================================================================================
bool PositionManager::ShouldMaintainGroupProximity()
{
    if (!_bot)
        return false;

    // Always maintain proximity if in a group
    Group* group = _bot->GetGroup();
    if (!group)
        return false;

    // Check combat status
    bool inCombat = _bot->IsInCombat();

    // Factor 1: Healer should always maintain proximity
    if (_threatManager && _threatManager->GetRole() == ThreatRole::HEALER)
    {
        TC_LOG_DEBUG("playerbot.position", "Bot {} should maintain proximity - healer role", _bot->GetName());
        return true;
    }

    // Factor 2: Check group spread
    ::std::vector<Player*> groupMembers;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
        {
            if (member->IsInWorld() && member->IsAlive())
                groupMembers.push_back(member);
        }
    }

    if (groupMembers.size() <= 1)
        return false;  // No other members to maintain proximity with

    // Calculate current distance from group center
    Position groupCenter = PositionUtils::CalculateGroupCenter(groupMembers);
    float distanceFromGroup = _bot->GetExactDist(&groupCenter);

    // Factor 3: Too far from group
    float maxAllowedDistance = inCombat ? 30.0f : 40.0f;
    if (distanceFromGroup > maxAllowedDistance)
    {
        TC_LOG_DEBUG("playerbot.position", "Bot {} should maintain proximity - too far ({:.1f} yards)",
                    _bot->GetName(), distanceFromGroup);
        return true;
    }

    // Factor 4: Group is moving together
    bool groupMoving = false;
    for (Player* member : groupMembers)
    {
        if (member && member != _bot && member->IsMoving())
        {
            groupMoving = true;
            break;
        }
    }

    if (groupMoving && distanceFromGroup > 20.0f)
    {
        TC_LOG_DEBUG("playerbot.position", "Bot {} should maintain proximity - group moving", _bot->GetName());
        return true;
    }

    // Factor 5: Dangerous area (dungeons/raids)
    Map* map = _bot->GetMap();
    if (map && (map->IsDungeon() || map->IsRaid()))
    {
        if (distanceFromGroup > 25.0f)
        {
            TC_LOG_DEBUG("playerbot.position", "Bot {} should maintain proximity - in dungeon/raid", _bot->GetName());
            return true;
        }
    }

    // Factor 6: Support roles should stay closer
    if (_threatManager && _threatManager->GetRole() == ThreatRole::SUPPORT)
    {
        if (distanceFromGroup > 20.0f)
        {
            TC_LOG_DEBUG("playerbot.position", "Bot {} should maintain proximity - support role", _bot->GetName());
            return true;
        }
    }

    // Factor 7: Check if group members need help
    for (Player* member : groupMembers)
    {
        if (member && member != _bot)
        {
            // Member in danger
            if (member->GetHealthPct() < 50.0f && _bot->GetExactDist(member) > 30.0f)
            {
                TC_LOG_DEBUG("playerbot.position", "Bot {} should maintain proximity - ally needs help", _bot->GetName());
                return true;
            }
        }
    }

    return false;
}

// ========================================================================================
// Method 13: ShouldStrafe
// Determine if strafing movement is beneficial
// ========================================================================================
bool PositionManager::ShouldStrafe(Unit* target)
{
    if (!_bot || !target)
        return false;

    // Don't strafe while casting
    if (_bot->IsNonMeleeSpellCast(false))
        return false;

    float distance = _bot->GetExactDist(target);

    // Different strafe conditions based on range
    bool shouldStrafe = false;

    // Factor 1: Ranged combat strafing (kiting)
    if (distance > 8.0f && distance < 30.0f)
    {
        // Ranged DPS/healers might strafe to avoid projectiles
        if (_threatManager)
        {
            ThreatRole role = _threatManager->GetRole();
            if (role == ThreatRole::HEALER ||
                (role == ThreatRole::DPS && distance > 15.0f))  // Ranged DPS
            {
                // Check if target has us targeted
                if (target->GetVictim() == _bot)
                {
                    shouldStrafe = true;
                    TC_LOG_DEBUG("playerbot.position", "Bot {} should strafe - being targeted at range", _bot->GetName());
                }
            }
        }
    }

    // Factor 2: Melee combat strafing
    if (distance <= 8.0f)
    {
        // Check if we need to reposition
        float angle = target->GetRelativeAngle(_bot);

        // Tanks shouldn't strafe (need to maintain position)
        if (_threatManager && _threatManager->GetRole() == ThreatRole::TANK)
        {
            shouldStrafe = false;
        }
        // DPS should strafe to get behind
        else if (::std::abs(angle) < M_PI / 2)  // We're in front
        {
            shouldStrafe = true;
            TC_LOG_DEBUG("playerbot.position", "Bot {} should strafe - need to get behind target", _bot->GetName());
        }
    }

    // Factor 3: Avoid AoE indicators
    Position currentPos = _bot->GetPosition();
    if (IsInDangerZone(currentPos))
    {
        shouldStrafe = true;
        TC_LOG_DEBUG("playerbot.position", "Bot {} should strafe - in danger zone", _bot->GetName());
    }

    // Factor 4: Target is immobile (no need to strafe)
    if (target->HasUnitState(UNIT_STATE_ROOTED | UNIT_STATE_STUNNED))
    {
        shouldStrafe = false;
        TC_LOG_DEBUG("playerbot.position", "Bot {} shouldn't strafe - target immobilized", _bot->GetName());
    }

    // Factor 5: Movement impaired (can't strafe effectively)
    if (_bot->HasUnitState(UNIT_STATE_SLOWED) || _bot->GetSpeed(MOVE_RUN) < 4.0f)
    {
        shouldStrafe = false;
        TC_LOG_DEBUG("playerbot.position", "Bot {} shouldn't strafe - movement impaired", _bot->GetName());
    }

    // Factor 6: Check recent position success
    float successRate = GetPositionSuccessRate(currentPos, 5.0f);
    if (successRate < 0.3f)  // This position has low success rate
    {
        shouldStrafe = true;
        TC_LOG_DEBUG("playerbot.position", "Bot {} should strafe - low position success rate", _bot->GetName());
    }

    return shouldStrafe;
}

// ========================================================================================
// Helper Implementation for PositionUtils::GetNearestWalkablePosition
// Find nearest walkable position within search radius
// ========================================================================================
Position PositionUtils::GetNearestWalkablePosition(const Position& pos, Map* map, float searchRadius)
{
    if (!map)
        return pos;

    // Check if current position is already walkable
    float groundZ = map->GetHeight(PhasingHandler::GetEmptyPhaseShift(),
                                  pos.GetPositionX(),
                                  pos.GetPositionY(),
                                  pos.GetPositionZ());

    if (::std::abs(pos.GetPositionZ() - groundZ) <= 2.0f)
        return pos;  // Already walkable

    // Search in expanding circles
    for (float radius = 2.0f; radius <= searchRadius; radius += 2.0f)
    {
        uint32 points = static_cast<uint32>(radius * 4);  // More points for larger radius

        for (uint32 i = 0; i < points; ++i)
        {
            float angle = (2.0f * M_PI * i) / points;
            Position testPos = CalculatePositionAtAngle(pos, radius, angle);

            float testGroundZ = map->GetHeight(PhasingHandler::GetEmptyPhaseShift(),
                                              testPos.GetPositionX(),
                                              testPos.GetPositionY(),
                                              testPos.GetPositionZ());

            testPos.m_positionZ = testGroundZ + 0.5f;

            // Check if position is on ground and not in water
            if (!map->IsInWater(PhasingHandler::GetEmptyPhaseShift(),
                               testPos.GetPositionX(),
                               testPos.GetPositionY(),
                               testPos.GetPositionZ()))
            {
                return testPos;
            }
        }
    }

    // If no walkable position found, return original
    return pos;
}

} // namespace Playerbot