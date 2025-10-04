/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PathfindingManager.h"
#include "Player.h"
#include "Unit.h"
#include "Map.h"
#include "Log.h"
#include "PathGenerator.h"
#include "MoveSpline.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

PathfindingManager::PathfindingManager(Player* bot)
    : _bot(bot), _defaultNodeSpacing(DEFAULT_NODE_SPACING), _maxNodes(DEFAULT_MAX_NODES),
      _pathfindingTimeout(DEFAULT_TIMEOUT), _cacheDuration(DEFAULT_CACHE_DURATION),
      _enableCaching(true), _enableDangerAvoidance(true), _lastDangerUpdate(0), _lastCacheCleanup(0)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "PathfindingManager: Bot player is null!");
        return;
    }

    TC_LOG_DEBUG("playerbot.pathfinding", "PathfindingManager initialized for bot {}", _bot->GetName());
}

PathResult PathfindingManager::FindPath(const PathRequest& request)
{
    auto startTime = std::chrono::steady_clock::now();
    PathResult result;

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try
    {
        _metrics.pathRequests++;

        if (request.startPos.GetExactDist(&request.goalPos) < 0.5f)
        {
            result.success = true;
            result.waypoints = { request.startPos, request.goalPos };
            result.quality = PathQuality::OPTIMAL;
            result.totalDistance = request.startPos.GetExactDist(&request.goalPos);
            result.estimatedTime = result.totalDistance / _bot->GetSpeed(MOVE_RUN);
            return result;
        }

        if (_enableCaching)
        {
            PathCacheEntry* cacheEntry = FindCacheEntry(request.startPos, request.goalPos);
            if (cacheEntry && !cacheEntry->IsExpired(getMSTime()))
            {
                _metrics.cacheHits++;
                result.success = true;
                result.waypoints = cacheEntry->waypoints;
                result.quality = cacheEntry->quality;
                result.totalDistance = PathfindingUtils::CalculatePathLength(result.waypoints);
                result.estimatedTime = result.totalDistance / _bot->GetSpeed(MOVE_RUN);
                cacheEntry->accessCount++;

                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                TrackPerformance(duration, true, true);
                return result;
            }
        }

        _metrics.cacheMisses++;

        UpdateDangerZones(getMSTime());

        if (IsDirectPathPossible(request.startPos, request.goalPos, request))
        {
            result.success = true;
            result.waypoints = { request.startPos, request.goalPos };
            result.quality = PathQuality::OPTIMAL;
            result.usedPathType = PathType::STRAIGHT_LINE;
        }
        else
        {
            result = CalculateAStarPath(request.startPos, request.goalPos, request);
        }

        if (result.success)
        {
            result.waypoints = OptimizePath(result.waypoints);
            result.totalDistance = PathfindingUtils::CalculatePathLength(result.waypoints);
            result.estimatedTime = result.totalDistance / _bot->GetSpeed(MOVE_RUN);
            result.quality = AssessPathQuality(result.waypoints, request);

            if (_enableCaching && result.quality >= PathQuality::GOOD)
            {
                PathCacheEntry cacheEntry;
                cacheEntry.startPos = request.startPos;
                cacheEntry.goalPos = request.goalPos;
                cacheEntry.waypoints = result.waypoints;
                cacheEntry.quality = result.quality;
                cacheEntry.timestamp = getMSTime();
                cacheEntry.expirationTime = cacheEntry.timestamp + _cacheDuration;
                cacheEntry.accessCount = 1;
                AddCacheEntry(cacheEntry);
            }

            _metrics.successfulPaths++;

            TC_LOG_DEBUG("playerbot.pathfinding", "Path found for bot {} with {} waypoints, distance: {:.2f}",
                       _bot->GetName(), result.waypoints.size(), result.totalDistance);
        }
        else
        {
            _metrics.failedPaths++;
            TC_LOG_DEBUG("playerbot.pathfinding", "Path finding failed for bot {}: {}",
                       _bot->GetName(), result.failureReason);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.calculationTime = static_cast<uint32>(duration.count());
        TrackPerformance(duration, result.success, false);
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.failureReason = "Exception during pathfinding: " + std::string(e.what());
        TC_LOG_ERROR("playerbot.pathfinding", "Exception in FindPath for bot {}: {}", _bot->GetName(), e.what());
    }

    return result;
}

PathResult PathfindingManager::FindPath(const Position& goal, PathFlags flags)
{
    PathRequest request;
    request.botGuid = _bot->GetGUID();
    request.startPos = _bot->GetPosition();
    request.goalPos = goal;
    request.pathType = PathType::GROUND_PATH;
    request.behavior = MovementBehavior::DIRECT;
    request.flags = flags;
    request.nodeSpacing = _defaultNodeSpacing;
    request.maxNodes = _maxNodes;
    request.timeoutMs = _pathfindingTimeout;

    return FindPath(request);
}

PathResult PathfindingManager::FindPathToUnit(Unit* target, float range, PathFlags flags)
{
    if (!target)
    {
        PathResult result;
        result.failureReason = "No target specified";
        return result;
    }

    Position goalPos = target->GetPosition();
    if (range > 0.0f)
    {
        Position botPos = _bot->GetPosition();
        float currentDistance = botPos.GetExactDist(&goalPos);

        if (currentDistance > range)
        {
            float angle = std::atan2(goalPos.GetPositionY() - botPos.GetPositionY(),
                                   goalPos.GetPositionX() - botPos.GetPositionX());

            goalPos.m_positionX = goalPos.GetPositionX() - range * std::cos(angle);
            goalPos.m_positionY = goalPos.GetPositionY() - range * std::sin(angle);
        }
    }

    PathRequest request;
    request.botGuid = _bot->GetGUID();
    request.startPos = _bot->GetPosition();
    request.goalPos = goalPos;
    request.pathType = PathType::GROUND_PATH;
    request.behavior = MovementBehavior::DIRECT;
    request.flags = flags;

    return FindPath(request);
}

PathResult PathfindingManager::FindEscapePath(const std::vector<Unit*>& threats, float minDistance)
{
    PathRequest request;
    request.botGuid = _bot->GetGUID();
    request.startPos = _bot->GetPosition();
    request.behavior = MovementBehavior::RETREAT;
    request.flags = PathFlags::FAST | PathFlags::AVOID_ENEMIES;

    Position botPos = _bot->GetPosition();
    Position escapeDirection(0.0f, 0.0f, 0.0f);

    for (Unit* threat : threats)
    {
        if (!threat)
            continue;

        Position threatPos = threat->GetPosition();
        float angle = std::atan2(botPos.GetPositionY() - threatPos.GetPositionY(),
                               botPos.GetPositionX() - threatPos.GetPositionX());

        escapeDirection.m_positionX += std::cos(angle);
        escapeDirection.m_positionY += std::sin(angle);
    }

    float length = std::sqrt(escapeDirection.m_positionX * escapeDirection.m_positionX +
                           escapeDirection.m_positionY * escapeDirection.m_positionY);

    if (length > 0.0f)
    {
        escapeDirection.m_positionX /= length;
        escapeDirection.m_positionY /= length;
    }
    else
    {
        escapeDirection.m_positionX = 1.0f;
        escapeDirection.m_positionY = 0.0f;
    }

    request.goalPos.m_positionX = botPos.GetPositionX() + escapeDirection.m_positionX * minDistance;
    request.goalPos.m_positionY = botPos.GetPositionY() + escapeDirection.m_positionY * minDistance;
    request.goalPos.m_positionZ = botPos.GetPositionZ();

    return FindPath(request);
}

PathResult PathfindingManager::CalculateAStarPath(const Position& start, const Position& goal, const PathRequest& request)
{
    PathResult result;
    result.usedPathType = PathType::GROUND_PATH;

    auto startTime = std::chrono::steady_clock::now();

    std::priority_queue<PathNode> openSet;
    std::vector<PathNode*> closedSet;
    std::unordered_map<uint32, std::unique_ptr<PathNode>> allNodes;

    PathNode* startNode = CreateNode(start);
    startNode->gCost = 0.0f;
    startNode->hCost = CalculateHeuristic(start, goal);
    startNode->CalculateFCost();

    openSet.push(*startNode);
    allNodes[startNode->nodeId] = std::unique_ptr<PathNode>(startNode);

    uint32 nodesExpanded = 0;
    bool pathFound = false;
    PathNode* goalNode = nullptr;

    while (!openSet.empty() && nodesExpanded < request.maxNodes)
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
        if (elapsed.count() > request.timeoutMs)
        {
            result.failureReason = "Pathfinding timeout exceeded";
            break;
        }

        PathNode currentNode = openSet.top();
        openSet.pop();

        if (currentNode.position.GetExactDist(&goal) <= request.nodeSpacing)
        {
            goalNode = allNodes[currentNode.nodeId].get();
            pathFound = true;
            break;
        }

        closedSet.push_back(allNodes[currentNode.nodeId].get());

        std::vector<Position> neighbors = GetNeighborNodes(currentNode.position, request.nodeSpacing);

        for (const Position& neighborPos : neighbors)
        {
            if (!IsNodeWalkable(neighborPos, request))
                continue;

            if (IsInClosedSet(neighborPos, closedSet))
                continue;

            PathNode* neighborNode = CreateNode(neighborPos);
            float tentativeGCost = currentNode.gCost + GetNodeCost(currentNode.position, neighborPos, request);

            bool inOpenSet = false;
            for (auto& [id, node] : allNodes)
            {
                if (node->position.GetExactDist(&neighborPos) <= 0.1f)
                {
                    if (tentativeGCost < node->gCost)
                    {
                        node->gCost = tentativeGCost;
                        node->parent = allNodes[currentNode.nodeId].get();
                        node->CalculateFCost();
                    }
                    inOpenSet = true;
                    break;
                }
            }

            if (!inOpenSet)
            {
                neighborNode->gCost = tentativeGCost;
                neighborNode->hCost = CalculateHeuristic(neighborPos, goal);
                neighborNode->parent = allNodes[currentNode.nodeId].get();
                neighborNode->CalculateFCost();

                openSet.push(*neighborNode);
                allNodes[neighborNode->nodeId] = std::unique_ptr<PathNode>(neighborNode);
            }
        }

        nodesExpanded++;
    }

    if (pathFound && goalNode)
    {
        std::vector<PathNode*> pathNodes = ReconstructPath(goalNode);
        result.waypoints.clear();
        result.waypoints.reserve(pathNodes.size());

        for (PathNode* node : pathNodes)
        {
            result.waypoints.push_back(node->position);
        }

        result.success = true;
        result.nodeCount = static_cast<uint32>(pathNodes.size());

        TC_LOG_DEBUG("playerbot.pathfinding", "A* path calculated for bot {} with {} nodes",
                   _bot->GetName(), result.nodeCount);
    }
    else
    {
        if (nodesExpanded >= request.maxNodes)
        {
            result.failureReason = "Maximum node limit reached";
        }
        else if (openSet.empty())
        {
            result.failureReason = "No viable path found";
        }

        PathNode* closestNode = nullptr;
        float closestDistance = std::numeric_limits<float>::max();

        for (PathNode* node : closedSet)
        {
            float distance = node->position.GetExactDist(&goal);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestNode = node;
            }
        }

        if (closestNode && closestDistance < start.GetExactDist(&goal) * 0.8f)
        {
            std::vector<PathNode*> partialPathNodes = ReconstructPath(closestNode);
            result.waypoints.clear();
            result.waypoints.reserve(partialPathNodes.size());

            for (PathNode* node : partialPathNodes)
            {
                result.waypoints.push_back(node->position);
            }

            result.partialPath = true;
            result.furthestReachable = closestNode->position;
            _metrics.partialPaths++;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    result.calculationTime = static_cast<uint32>(duration.count());

    return result;
}

std::vector<PathNode*> PathfindingManager::ReconstructPath(PathNode* goalNode)
{
    std::vector<PathNode*> path;
    PathNode* current = goalNode;

    while (current != nullptr)
    {
        path.push_back(current);
        current = current->parent;
    }

    std::reverse(path.begin(), path.end());
    return path;
}

float PathfindingManager::CalculateHeuristic(const Position& current, const Position& goal)
{
    return PathfindingUtils::CalculateOctileDistance(current, goal);
}

bool PathfindingManager::IsPathValid(const std::vector<Position>& waypoints)
{
    if (waypoints.size() < 2)
        return false;

    for (size_t i = 1; i < waypoints.size(); ++i)
    {
        float distance = waypoints[i-1].GetExactDist(&waypoints[i]);
        if (distance > _defaultNodeSpacing * 3.0f)
            return false;

        if (!IsPositionInWorld(waypoints[i]))
            return false;
    }

    return true;
}

std::vector<Position> PathfindingManager::OptimizePath(const std::vector<Position>& waypoints)
{
    if (waypoints.size() <= 2)
        return waypoints;

    std::vector<Position> optimized = PathfindingUtils::RemoveRedundantWaypoints(waypoints);
    optimized = SmoothPath(optimized);

    return optimized;
}

std::vector<Position> PathfindingManager::SmoothPath(const std::vector<Position>& waypoints)
{
    if (waypoints.size() <= 2)
        return waypoints;

    std::vector<Position> smoothed;
    smoothed.push_back(waypoints[0]);

    for (size_t i = 1; i < waypoints.size() - 1; ++i)
    {
        const Position& prev = waypoints[i-1];
        const Position& current = waypoints[i];
        const Position& next = waypoints[i+1];

        Position smoothedPos;
        smoothedPos.m_positionX = (prev.GetPositionX() + current.GetPositionX() + next.GetPositionX()) / 3.0f;
        smoothedPos.m_positionY = (prev.GetPositionY() + current.GetPositionY() + next.GetPositionY()) / 3.0f;
        smoothedPos.m_positionZ = current.GetPositionZ();

        smoothed.push_back(smoothedPos);
    }

    smoothed.push_back(waypoints.back());

    return smoothed;
}

PathQuality PathfindingManager::AssessPathQuality(const std::vector<Position>& waypoints, const PathRequest& request)
{
    if (waypoints.empty())
        return PathQuality::BLOCKED;

    float totalDistance = PathfindingUtils::CalculatePathLength(waypoints);
    float directDistance = request.startPos.GetExactDist(&request.goalPos);

    float efficiency = directDistance / totalDistance;

    if (efficiency >= 0.95f)
        return PathQuality::OPTIMAL;
    else if (efficiency >= 0.80f)
        return PathQuality::GOOD;
    else if (efficiency >= 0.60f)
        return PathQuality::SUBOPTIMAL;
    else
        return PathQuality::POOR;
}

bool PathfindingManager::IsNodeWalkable(const Position& pos, const PathRequest& request)
{
    if (!IsPositionInWorld(pos))
        return false;

    Map* map = _bot->GetMap();
    if (!map)
        return false;

    if (!PathfindingUtils::IsPositionOnGround(pos, map))
    {
        if (!(request.flags & PathFlags::ALLOW_FLYING))
            return false;
    }

    if (IsWaterNode(pos) && !(request.flags & PathFlags::ALLOW_SWIMMING))
        return false;

    if (_enableDangerAvoidance && (request.flags & PathFlags::AVOID_AOE))
    {
        if (GetDangerAtPosition(pos) > 0.5f)
            return false;
    }

    return true;
}

float PathfindingManager::GetNodeCost(const Position& from, const Position& to, const PathRequest& request)
{
    float baseCost = from.GetExactDist(&to);
    float totalCost = baseCost;

    if (IsWaterNode(to))
    {
        totalCost *= 1.5f;
    }

    if (RequiresJump(from, to))
    {
        if (request.flags & PathFlags::ALLOW_JUMPING)
            totalCost *= 1.3f;
        else
            totalCost *= 10.0f;
    }

    float terrainCost = CalculateTerrainCost(to);
    totalCost += terrainCost;

    if (_enableDangerAvoidance && (request.flags & PathFlags::AVOID_AOE))
    {
        float dangerLevel = GetDangerAtPosition(to);
        totalCost += dangerLevel * 10.0f;
    }

    return totalCost;
}

std::vector<Position> PathfindingManager::GetNeighborNodes(const Position& center, float spacing)
{
    std::vector<Position> neighbors;
    neighbors.reserve(8);

    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            if (dx == 0 && dy == 0)
                continue;

            Position neighbor;
            neighbor.m_positionX = center.GetPositionX() + dx * spacing;
            neighbor.m_positionY = center.GetPositionY() + dy * spacing;
            neighbor.m_positionZ = center.GetPositionZ();

            neighbors.push_back(neighbor);
        }
    }

    return neighbors;
}

float PathfindingManager::CalculateTerrainCost(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return 0.0f;

    float slope = GetTerrainSlope(pos);
    return slope * 2.0f;
}

void PathfindingManager::RegisterDangerZone(const DangerZone& zone)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _dangerZones.push_back(zone);

    TC_LOG_DEBUG("playerbot.pathfinding", "Registered danger zone for bot {} at ({:.2f}, {:.2f}) radius {:.2f}",
               _bot->GetName(), zone.center.GetPositionX(), zone.center.GetPositionY(), zone.radius);
}

void PathfindingManager::UpdateDangerZones(uint32 currentTime)
{
    if (currentTime - _lastDangerUpdate < DANGER_UPDATE_INTERVAL)
        return;

    ClearExpiredDangerZones(currentTime);
    _lastDangerUpdate = currentTime;
}

void PathfindingManager::ClearExpiredDangerZones(uint32 currentTime)
{
    _dangerZones.erase(
        std::remove_if(_dangerZones.begin(), _dangerZones.end(),
            [currentTime](const DangerZone& zone) {
                return !zone.isActive || currentTime > zone.startTime + zone.duration;
            }),
        _dangerZones.end()
    );
}

float PathfindingManager::GetDangerAtPosition(const Position& pos)
{
    float maxDanger = 0.0f;
    uint32 currentTime = getMSTime();

    for (const DangerZone& zone : _dangerZones)
    {
        float danger = zone.GetDangerAtPosition(pos, currentTime);
        maxDanger = std::max(maxDanger, danger);
    }

    return maxDanger;
}

bool PathfindingManager::IsPositionSafe(const Position& pos, float safetyThreshold)
{
    return GetDangerAtPosition(pos) <= safetyThreshold;
}

PathNode* PathfindingManager::CreateNode(const Position& pos)
{
    static uint32 nodeIdCounter = 1;
    PathNode* node = new PathNode(pos);
    node->nodeId = nodeIdCounter++;
    return node;
}

bool PathfindingManager::IsInClosedSet(const Position& pos, const std::vector<PathNode*>& closedSet)
{
    for (PathNode* node : closedSet)
    {
        if (node->position.GetExactDist(&pos) <= 0.1f)
            return true;
    }
    return false;
}

bool PathfindingManager::IsDirectPathPossible(const Position& start, const Position& goal, const PathRequest& request)
{
    float distance = start.GetExactDist(&goal);
    if (distance > 50.0f)
        return false;

    if (!IsNodeWalkable(goal, request))
        return false;

    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return PathfindingUtils::CanWalkBetween(start, goal, map);
}

bool PathfindingManager::IsWaterNode(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return map->IsInWater(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
}

bool PathfindingManager::RequiresJump(const Position& from, const Position& to)
{
    float heightDiff = to.GetPositionZ() - from.GetPositionZ();
    return heightDiff > 1.0f && heightDiff <= 5.0f;
}

PathCacheEntry* PathfindingManager::FindCacheEntry(const Position& start, const Position& goal)
{
    std::string key = GenerateCacheKey(start, goal);
    auto it = _pathCache.find(key);

    if (it != _pathCache.end() && it->second.IsValid(start, goal))
        return &it->second;

    return nullptr;
}

void PathfindingManager::AddCacheEntry(const PathCacheEntry& entry)
{
    if (_pathCache.size() >= MAX_CACHE_SIZE)
    {
        ClearExpiredCacheEntries();
    }

    std::string key = GenerateCacheKey(entry.startPos, entry.goalPos);
    _pathCache[key] = entry;
}

std::string PathfindingManager::GenerateCacheKey(const Position& start, const Position& goal)
{
    return std::to_string(static_cast<int>(start.GetPositionX())) + "_" +
           std::to_string(static_cast<int>(start.GetPositionY())) + "_" +
           std::to_string(static_cast<int>(goal.GetPositionX())) + "_" +
           std::to_string(static_cast<int>(goal.GetPositionY()));
}

void PathfindingManager::ClearExpiredCacheEntries()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    uint32 currentTime = getMSTime();
    if (currentTime - _lastCacheCleanup < CACHE_CLEANUP_INTERVAL)
        return;

    auto it = _pathCache.begin();
    while (it != _pathCache.end())
    {
        if (it->second.IsExpired(currentTime))
            it = _pathCache.erase(it);
        else
            ++it;
    }

    _lastCacheCleanup = currentTime;
}

void PathfindingManager::TrackPerformance(std::chrono::microseconds duration, bool successful, bool cached)
{
    if (duration > _metrics.maxCalculationTime)
        _metrics.maxCalculationTime = duration;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        uint32 totalRequests = _metrics.pathRequests.load();
        if (totalRequests > 0)
        {
            _metrics.averageCalculationTime = std::chrono::microseconds(
                static_cast<uint64_t>(_metrics.averageCalculationTime.count() * 0.9 + duration.count() * 0.1)
            );
        }
        _metrics.lastUpdate = currentTime;
    }
}

bool PathfindingManager::IsPositionInWorld(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return pos.GetPositionX() >= -20000.0f && pos.GetPositionX() <= 20000.0f &&
           pos.GetPositionY() >= -20000.0f && pos.GetPositionY() <= 20000.0f &&
           pos.GetPositionZ() >= -500.0f && pos.GetPositionZ() <= 2000.0f;
}

float PathfindingManager::GetTerrainSlope(const Position& pos)
{
    Map* map = _bot->GetMap();
    if (!map)
        return 0.0f;

    float currentZ = map->GetHeight(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
    float forwardZ = map->GetHeight(pos.GetPositionX() + 1.0f, pos.GetPositionY(), pos.GetPositionZ());

    return std::abs(forwardZ - currentZ);
}

// PathfindingUtils implementation
float PathfindingUtils::CalculateEuclideanDistance(const Position& a, const Position& b)
{
    return a.GetExactDist(&b);
}

float PathfindingUtils::CalculateManhattanDistance(const Position& a, const Position& b)
{
    return std::abs(a.GetPositionX() - b.GetPositionX()) +
           std::abs(a.GetPositionY() - b.GetPositionY()) +
           std::abs(a.GetPositionZ() - b.GetPositionZ());
}

float PathfindingUtils::CalculateOctileDistance(const Position& a, const Position& b)
{
    float dx = std::abs(a.GetPositionX() - b.GetPositionX());
    float dy = std::abs(a.GetPositionY() - b.GetPositionY());
    float dz = std::abs(a.GetPositionZ() - b.GetPositionZ());

    return 0.414f * std::min(dx, dy) + std::max(dx, dy) + dz;
}

float PathfindingUtils::CalculatePathLength(const std::vector<Position>& waypoints)
{
    if (waypoints.size() < 2)
        return 0.0f;

    float totalLength = 0.0f;
    for (size_t i = 1; i < waypoints.size(); ++i)
    {
        totalLength += waypoints[i-1].GetExactDist(&waypoints[i]);
    }

    return totalLength;
}

bool PathfindingUtils::IsPositionOnGround(const Position& pos, Map* map)
{
    if (!map)
        return false;

    float groundZ = map->GetHeight(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
    return std::abs(pos.GetPositionZ() - groundZ) <= 2.0f;
}

bool PathfindingUtils::CanWalkBetween(const Position& a, const Position& b, Map* map)
{
    if (!map)
        return false;

    return map->isInLineOfSight(a.GetPositionX(), a.GetPositionY(), a.GetPositionZ(),
                              b.GetPositionX(), b.GetPositionY(), b.GetPositionZ());
}

std::vector<Position> PathfindingUtils::RemoveRedundantWaypoints(const std::vector<Position>& waypoints)
{
    if (waypoints.size() <= 2)
        return waypoints;

    std::vector<Position> optimized;
    optimized.push_back(waypoints[0]);

    for (size_t i = 1; i < waypoints.size() - 1; ++i)
    {
        const Position& prev = waypoints[i-1];
        const Position& current = waypoints[i];
        const Position& next = waypoints[i+1];

        float angle1 = std::atan2(current.GetPositionY() - prev.GetPositionY(),
                                current.GetPositionX() - prev.GetPositionX());
        float angle2 = std::atan2(next.GetPositionY() - current.GetPositionY(),
                                next.GetPositionX() - current.GetPositionX());

        float angleDiff = std::abs(angle2 - angle1);
        if (angleDiff > M_PI)
            angleDiff = 2.0f * M_PI - angleDiff;

        if (angleDiff > 0.1f)
        {
            optimized.push_back(current);
        }
    }

    optimized.push_back(waypoints.back());
    return optimized;
}

Position PathfindingUtils::CalculateFormationPosition(const Position& leaderPos, float angle, float distance)
{
    Position formationPos;
    formationPos.m_positionX = leaderPos.GetPositionX() + distance * std::cos(angle);
    formationPos.m_positionY = leaderPos.GetPositionY() + distance * std::sin(angle);
    formationPos.m_positionZ = leaderPos.GetPositionZ();
    return formationPos;
}

std::vector<Position> PathfindingUtils::GenerateFormationPositions(const Position& center, uint32 memberCount, float spacing)
{
    std::vector<Position> positions;
    positions.reserve(memberCount);

    for (uint32 i = 0; i < memberCount; ++i)
    {
        float angle = (2.0f * M_PI * i) / memberCount;
        Position pos = CalculateFormationPosition(center, angle, spacing);
        positions.push_back(pos);
    }

    return positions;
}

} // namespace Playerbot