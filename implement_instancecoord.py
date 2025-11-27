#!/usr/bin/env python3
"""Implement full InstanceCoordination.cpp stub methods"""

def implement_instance_coordination():
    filepath = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/Dungeon/InstanceCoordination.cpp'

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Add required includes for PathGenerator and Map
    old_includes = '''#include "InstanceCoordination.h"
#include "GameTime.h"
#include "DungeonBehavior.h"
#include "EncounterStrategy.h"
#include "../Group/GroupFormation.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Timer.h"
#include "World.h"
#include "WorldPacket.h"
#include "Item.h"
#include "Loot.h"
#include "LootMgr.h"
#include <algorithm>
#include <cmath>'''

    new_includes = '''#include "InstanceCoordination.h"
#include "GameTime.h"
#include "DungeonBehavior.h"
#include "EncounterStrategy.h"
#include "../Group/GroupFormation.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Timer.h"
#include "World.h"
#include "WorldPacket.h"
#include "Item.h"
#include "Loot.h"
#include "LootMgr.h"
#include "PathGenerator.h"
#include "Map.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include <algorithm>
#include <cmath>'''

    content = content.replace(old_includes, new_includes)

    # Replace AdaptFormationToTerrain with full terrain analysis implementation
    old_adapt_formation = '''void InstanceCoordination::AdaptFormationToTerrain(Group* group, const Position& location)
{
    if (!group)
        return;

    ::std::lock_guard lock(_formationMutex);

    uint32 groupId = group->GetGUID().GetCounter();

    auto formationItr = _groupFormations.find(groupId);
    if (formationItr == _groupFormations.end())
        return;

    FormationData& formation = formationItr->second;

    // DESIGN NOTE: Terrain analysis for formation adaptation
    // Uses 0.5f as default terrain complexity value
    // Full implementation should:
    // - Query TrinityCore navmesh/pathfinding data for terrain geometry
    // - Analyze passage width and height restrictions
    // - Detect narrow corridors, wide rooms, staircases, bridges
    // - Calculate terrain complexity score (0.0 = wide open, 1.0 = very narrow)
    // - Use Map::IsInLineOfSight() and Map::GetHeight() for terrain checks
    // - Consider dynamic obstacles (mobs, other players)
    // Reference: TrinityCore PathGenerator, MMAP navigation mesh
    float terrainComplexity = 0.5f;
    formation.isCompact = terrainComplexity > 0.6f;
    formation.formationRadius = formation.isCompact ? 8.0f : 12.0f;

    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::AdaptFormationToTerrain - Group {} adapted formation (compact: {}, radius: {:.2f})",
        groupId, formation.isCompact, formation.formationRadius);
}'''

    new_adapt_formation = '''void InstanceCoordination::AdaptFormationToTerrain(Group* group, const Position& location)
{
    if (!group)
        return;

    ::std::lock_guard lock(_formationMutex);

    uint32 groupId = group->GetGUID().GetCounter();

    auto formationItr = _groupFormations.find(groupId);
    if (formationItr == _groupFormations.end())
        return;

    FormationData& formation = formationItr->second;

    // Get group leader's map for terrain analysis
    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (!leader || !leader->GetMap())
    {
        // Fallback to default values if no leader available
        formation.isCompact = true;
        formation.formationRadius = 8.0f;
        return;
    }

    Map* map = leader->GetMap();
    float terrainComplexity = 0.0f;
    uint32 obstructedDirections = 0;
    uint32 totalDirections = 8; // Check 8 cardinal directions

    // Analyze terrain complexity using pathfinding and height checks
    // Check multiple directions around the location to assess passage width
    static constexpr float PROBE_DISTANCES[] = { 5.0f, 10.0f, 15.0f };
    static constexpr float ANGLE_STEP = M_PI / 4.0f; // 45 degrees

    for (float probeDistance : PROBE_DISTANCES)
    {
        for (uint32 dir = 0; dir < totalDirections; ++dir)
        {
            float angle = dir * ANGLE_STEP;
            float probeX = location.GetPositionX() + probeDistance * std::cos(angle);
            float probeY = location.GetPositionY() + probeDistance * std::sin(angle);
            float probeZ = location.GetPositionZ();

            // Check if path is clear using PathGenerator
            PathGenerator pathGen(leader);
            pathGen.CalculatePath(location.GetPositionX(), location.GetPositionY(),
                                  location.GetPositionZ(), probeX, probeY, probeZ);

            PathType pathType = pathGen.GetPathType();

            // Check for obstructions
            if (pathType & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_FARFROMPOLY))
            {
                obstructedDirections++;
            }

            // Also check height variations (stairs, ramps, drops)
            float groundHeight = map->GetHeight(leader->GetPhaseShift(), probeX, probeY, probeZ + 5.0f);
            float heightDiff = std::abs(groundHeight - location.GetPositionZ());

            if (heightDiff > 3.0f) // Significant height change
            {
                terrainComplexity += 0.1f;
            }

            // Check for water
            if (map->IsInWater(leader->GetPhaseShift(), probeX, probeY, probeZ))
            {
                terrainComplexity += 0.05f;
            }
        }
    }

    // Calculate terrain complexity score (0.0 = wide open, 1.0 = very narrow)
    float obstructionRatio = static_cast<float>(obstructedDirections) /
                             static_cast<float>(totalDirections * 3); // 3 probe distances
    terrainComplexity += obstructionRatio * 0.6f;

    // Check for nearby hostile creatures (dynamic obstacles)
    uint32 nearbyHostileCount = 0;
    float searchRadius = 30.0f;

    // Use GridNotifier to count nearby hostile units
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(leader, leader, searchRadius);
    Trinity::UnitSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(leader, checker);

    Cell::VisitAllObjects(leader, searcher, searchRadius);
    if (searcher.GetTarget())
    {
        // Found at least one hostile - increase complexity
        terrainComplexity += 0.2f;
    }

    // Clamp terrain complexity to 0.0-1.0 range
    terrainComplexity = ::std::max(0.0f, ::std::min(1.0f, terrainComplexity));

    // Determine formation based on terrain complexity
    // High complexity (>0.6) = compact formation for tight spaces
    // Medium complexity (0.3-0.6) = standard formation
    // Low complexity (<0.3) = spread formation for open areas
    if (terrainComplexity > 0.6f)
    {
        formation.isCompact = true;
        formation.formationRadius = 6.0f; // Very tight formation
        formation.formationType = "column"; // Single file for narrow passages
    }
    else if (terrainComplexity > 0.3f)
    {
        formation.isCompact = true;
        formation.formationRadius = 8.0f; // Standard dungeon formation
        formation.formationType = "wedge"; // Wedge for moderate spaces
    }
    else
    {
        formation.isCompact = false;
        formation.formationRadius = 12.0f; // Spread formation for open areas
        formation.formationType = "spread"; // Spread out in open areas
    }

    // Adjust movement speed based on terrain
    formation.movementSpeed = terrainComplexity > 0.5f ? 0.8f : 1.0f; // Slower in complex terrain

    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::AdaptFormationToTerrain - Group {} terrain analysis: complexity={:.2f}, obstructed={}/{}, compact={}, radius={:.2f}, type={}",
        groupId, terrainComplexity, obstructedDirections, totalDirections * 3,
        formation.isCompact, formation.formationRadius, formation.formationType);
}'''

    content = content.replace(old_adapt_formation, new_adapt_formation)

    # Replace CheckGroupResources with full mana calculation
    old_check_resources = '''void InstanceCoordination::CheckGroupResources(Group* group)
{
    if (!group)
        return;

    uint32 groupId = group->GetGUID().GetCounter();

    auto resourceItr = _resourceCoordination.find(groupId);
    if (resourceItr == _resourceCoordination.end())
        return;

    ResourceCoordination& resources = resourceItr->second;

    // Check mana
    ManageGroupMana(group);

    // Check health
    float totalHealth = 0.0f;
    uint32 memberCount = 0;
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        float healthPercent = player->GetMaxHealth() > 0 ?
            static_cast<float>(player->GetHealth()) / player->GetMaxHealth() : 0.0f;

        resources.memberHealth[player->GetGUID().GetCounter()] = healthPercent;
        totalHealth += healthPercent;
        memberCount++;
    }

    // Calculate group readiness
    float averageHealth = memberCount > 0 ? totalHealth / memberCount : 0.0f;

    // DESIGN NOTE: Simplified mana calculation
    // Current behavior: Uses fixed 0.8f (80%) as average mana value
    // Full implementation should:
    // - Call ManageGroupMana() to calculate actual average mana across mana-using members
    // - Track mana percentage per member in ResourceCoordination::memberMana
    // - Weight mana importance based on number of mana-dependent members (healers, casters)
    // - Consider mana-less classes (warriors, rogues, DKs) should not affect mana average
    // Reference: ManageGroupMana() method at line 480
    float averageMana = 0.8f;

    resources.groupReadiness = static_cast<uint32>((averageHealth * 0.6f + averageMana * 0.4f) * 100.0f);

    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::CheckGroupResources - Group {} readiness: {}%",
        groupId, resources.groupReadiness);
}'''

    new_check_resources = '''void InstanceCoordination::CheckGroupResources(Group* group)
{
    if (!group)
        return;

    uint32 groupId = group->GetGUID().GetCounter();

    auto resourceItr = _resourceCoordination.find(groupId);
    if (resourceItr == _resourceCoordination.end())
        return;

    ResourceCoordination& resources = resourceItr->second;

    // Track health and mana for all members
    float totalHealth = 0.0f;
    float totalMana = 0.0f;
    uint32 memberCount = 0;
    uint32 manaDependentMembers = 0;
    uint32 criticalRolesLowMana = 0;
    uint32 criticalRolesLowHealth = 0;

    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        uint32 playerGuid = player->GetGUID().GetCounter();

        // Track health
        float healthPercent = player->GetMaxHealth() > 0 ?
            static_cast<float>(player->GetHealth()) / static_cast<float>(player->GetMaxHealth()) : 0.0f;

        resources.memberHealth[playerGuid] = healthPercent;
        totalHealth += healthPercent;
        memberCount++;

        // Track mana for mana-using classes
        if (player->GetMaxPower(POWER_MANA) > 0)
        {
            float manaPercent = static_cast<float>(player->GetPower(POWER_MANA)) /
                               static_cast<float>(player->GetMaxPower(POWER_MANA));

            resources.memberMana[playerGuid] = manaPercent;
            totalMana += manaPercent;
            manaDependentMembers++;

            // Determine player role for critical role tracking
            DungeonRole role = DungeonBehavior::instance()->DeterminePlayerRole(player);

            // Track critical roles (healer, tank) with low resources
            if (role == DungeonRole::HEALER)
            {
                if (manaPercent < 0.3f) // Healer below 30% mana is critical
                    criticalRolesLowMana++;
                if (healthPercent < 0.5f) // Healer below 50% health
                    criticalRolesLowHealth++;
            }
            else if (role == DungeonRole::TANK)
            {
                if (healthPercent < 0.5f) // Tank below 50% health is critical
                    criticalRolesLowHealth++;
            }
        }
        else
        {
            // Non-mana users (warriors, rogues, feral druids, DKs)
            // Track their secondary resource if applicable
            resources.memberMana[playerGuid] = 1.0f; // Treat as "full" for readiness calculation
        }

        // Track major cooldown availability
        // This would check important defensives like Shield Wall, Divine Shield, etc.
        uint32 cooldownsAvailable = 0;

        // Check for common defensive cooldowns by class
        uint8 playerClass = player->GetClass();
        switch (playerClass)
        {
            case CLASS_WARRIOR:
                // Shield Wall, Last Stand, etc.
                if (!player->GetSpellHistory()->HasCooldown(871)) // Shield Wall
                    cooldownsAvailable++;
                if (!player->GetSpellHistory()->HasCooldown(12975)) // Last Stand
                    cooldownsAvailable++;
                break;
            case CLASS_PALADIN:
                // Divine Shield, Lay on Hands
                if (!player->GetSpellHistory()->HasCooldown(642)) // Divine Shield
                    cooldownsAvailable++;
                if (!player->GetSpellHistory()->HasCooldown(633)) // Lay on Hands
                    cooldownsAvailable++;
                break;
            case CLASS_PRIEST:
                // Pain Suppression, Guardian Spirit, etc.
                if (!player->GetSpellHistory()->HasCooldown(33206)) // Pain Suppression
                    cooldownsAvailable++;
                if (!player->GetSpellHistory()->HasCooldown(47788)) // Guardian Spirit
                    cooldownsAvailable++;
                break;
            case CLASS_DEATH_KNIGHT:
                // Icebound Fortitude, Anti-Magic Shell
                if (!player->GetSpellHistory()->HasCooldown(48792)) // Icebound Fortitude
                    cooldownsAvailable++;
                if (!player->GetSpellHistory()->HasCooldown(48707)) // Anti-Magic Shell
                    cooldownsAvailable++;
                break;
            case CLASS_DRUID:
                // Barkskin, Survival Instincts
                if (!player->GetSpellHistory()->HasCooldown(22812)) // Barkskin
                    cooldownsAvailable++;
                break;
            case CLASS_MONK:
                // Fortifying Brew, Zen Meditation
                if (!player->GetSpellHistory()->HasCooldown(115203)) // Fortifying Brew
                    cooldownsAvailable++;
                break;
            case CLASS_DEMON_HUNTER:
                // Metamorphosis, etc.
                if (!player->GetSpellHistory()->HasCooldown(187827)) // Metamorphosis
                    cooldownsAvailable++;
                break;
            default:
                break;
        }

        resources.memberCooldowns[playerGuid] = cooldownsAvailable;
    }

    // Calculate average health
    float averageHealth = memberCount > 0 ? totalHealth / static_cast<float>(memberCount) : 0.0f;

    // Calculate average mana (only for mana-dependent members)
    float averageMana = manaDependentMembers > 0 ?
        totalMana / static_cast<float>(manaDependentMembers) : 1.0f;

    // Weight mana importance based on number of mana users in group
    // More mana users = mana is more important for group readiness
    float manaWeight = 0.4f;
    if (manaDependentMembers >= 3)
        manaWeight = 0.5f; // Heavy caster group
    else if (manaDependentMembers <= 1)
        manaWeight = 0.2f; // Mostly melee group

    float healthWeight = 1.0f - manaWeight;

    // Calculate base readiness score
    float baseReadiness = (averageHealth * healthWeight + averageMana * manaWeight);

    // Apply penalties for critical roles with low resources
    float criticalPenalty = 0.0f;
    if (criticalRolesLowMana > 0)
        criticalPenalty += 0.15f * criticalRolesLowMana; // -15% per healer low on mana
    if (criticalRolesLowHealth > 0)
        criticalPenalty += 0.1f * criticalRolesLowHealth; // -10% per critical role low on health

    float finalReadiness = ::std::max(0.0f, baseReadiness - criticalPenalty);
    resources.groupReadiness = static_cast<uint32>(finalReadiness * 100.0f);

    // Determine if rest break is needed
    resources.needsRestBreak = (resources.groupReadiness < 60) ||
                               (averageMana < 0.4f && manaDependentMembers >= 2) ||
                               (criticalRolesLowMana > 0);

    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::CheckGroupResources - Group {} readiness: {}% (health: {:.1f}%, mana: {:.1f}%, manaUsers: {}, criticalLowMana: {}, criticalLowHealth: {})",
        groupId, resources.groupReadiness, averageHealth * 100.0f, averageMana * 100.0f,
        manaDependentMembers, criticalRolesLowMana, criticalRolesLowHealth);
}'''

    content = content.replace(old_check_resources, new_check_resources)

    # Replace CalculateOptimalRoute with A* pathfinding implementation
    old_calculate_route = '''::std::vector<Position> InstanceCoordination::CalculateOptimalRoute(Group* group, const ::std::vector<Position>& objectives)
{
    if (!group || objectives.empty())
        return {};

    // In a full implementation, this would use A* or similar pathfinding
    // For now, return objectives as-is (simple sequential route)
    return objectives;
}'''

    new_calculate_route = '''::std::vector<Position> InstanceCoordination::CalculateOptimalRoute(Group* group, const ::std::vector<Position>& objectives)
{
    if (!group || objectives.empty())
        return {};

    // Get group leader for pathfinding
    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (!leader || !leader->GetMap())
        return objectives; // Fallback to simple route

    ::std::vector<Position> optimalRoute;
    Position currentPosition = leader->GetPosition();

    // Build optimized route using nearest-neighbor heuristic with path validation
    ::std::vector<bool> visited(objectives.size(), false);

    for (size_t i = 0; i < objectives.size(); ++i)
    {
        float bestDistance = std::numeric_limits<float>::max();
        size_t bestIndex = 0;
        bool foundValid = false;

        // Find nearest unvisited objective with valid path
        for (size_t j = 0; j < objectives.size(); ++j)
        {
            if (visited[j])
                continue;

            Position const& objective = objectives[j];

            // Calculate path to this objective
            PathGenerator pathGen(leader);
            pathGen.CalculatePath(currentPosition.GetPositionX(), currentPosition.GetPositionY(),
                                  currentPosition.GetPositionZ(),
                                  objective.GetPositionX(), objective.GetPositionY(), objective.GetPositionZ());

            PathType pathType = pathGen.GetPathType();

            // Skip objectives without valid paths
            if (pathType & PATHFIND_NOPATH)
                continue;

            float pathLength = pathGen.GetPathLength();

            // Prefer shorter paths, but consider path quality
            float effectiveDistance = pathLength;
            if (pathType & PATHFIND_INCOMPLETE)
                effectiveDistance *= 1.5f; // Penalize incomplete paths
            if (pathType & PATHFIND_FARFROMPOLY)
                effectiveDistance *= 1.2f; // Penalize paths far from navmesh

            if (effectiveDistance < bestDistance)
            {
                bestDistance = effectiveDistance;
                bestIndex = j;
                foundValid = true;
            }
        }

        if (foundValid)
        {
            visited[bestIndex] = true;
            optimalRoute.push_back(objectives[bestIndex]);
            currentPosition = objectives[bestIndex];
        }
        else
        {
            // No valid path found to any remaining objective
            // Add remaining objectives in order
            for (size_t j = 0; j < objectives.size(); ++j)
            {
                if (!visited[j])
                {
                    visited[j] = true;
                    optimalRoute.push_back(objectives[j]);
                }
            }
            break;
        }
    }

    // Insert intermediate waypoints for long paths
    ::std::vector<Position> finalRoute;
    for (size_t i = 0; i < optimalRoute.size(); ++i)
    {
        Position const& target = optimalRoute[i];

        if (i == 0)
        {
            // First waypoint - add path from current position
            PathGenerator pathGen(leader);
            pathGen.CalculatePath(leader->GetPositionX(), leader->GetPositionY(),
                                  leader->GetPositionZ(),
                                  target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());

            // Add intermediate points from pathfinding
            Movement::PointsArray const& pathPoints = pathGen.GetPath();
            for (size_t p = 1; p < pathPoints.size(); ++p)
            {
                Position waypoint;
                waypoint.Relocate(pathPoints[p].x, pathPoints[p].y, pathPoints[p].z);
                finalRoute.push_back(waypoint);
            }
        }
        else
        {
            // Subsequent waypoints - add path from previous waypoint
            Position const& prev = optimalRoute[i - 1];

            PathGenerator pathGen(leader);
            pathGen.CalculatePath(prev.GetPositionX(), prev.GetPositionY(), prev.GetPositionZ(),
                                  target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());

            // Add intermediate points
            Movement::PointsArray const& pathPoints = pathGen.GetPath();
            for (size_t p = 1; p < pathPoints.size(); ++p)
            {
                Position waypoint;
                waypoint.Relocate(pathPoints[p].x, pathPoints[p].y, pathPoints[p].z);
                finalRoute.push_back(waypoint);
            }
        }
    }

    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::CalculateOptimalRoute - Generated route with {} waypoints from {} objectives",
        finalRoute.size(), objectives.size());

    return finalRoute.empty() ? objectives : finalRoute;
}'''

    content = content.replace(old_calculate_route, new_calculate_route)

    # Replace SynchronizeGroupMovement with real movement coordination
    old_sync_movement = '''void InstanceCoordination::SynchronizeGroupMovement(Group* group, const Position& destination)
{
    if (!group)
        return;

    // Move all group members to destination in formation
    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // In a full implementation, this would calculate formation-relative position
        // and move player to that position
    }
}'''

    new_sync_movement = '''void InstanceCoordination::SynchronizeGroupMovement(Group* group, const Position& destination)
{
    if (!group)
        return;

    uint32 groupId = group->GetGUID().GetCounter();

    auto formationItr = _groupFormations.find(groupId);
    if (formationItr == _groupFormations.end())
        return;

    FormationData const& formation = formationItr->second;

    // Get group leader as reference point
    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (!leader)
        return;

    // Calculate direction vector to destination
    float dx = destination.GetPositionX() - leader->GetPositionX();
    float dy = destination.GetPositionY() - leader->GetPositionY();
    float distToDest = std::sqrt(dx * dx + dy * dy);

    if (distToDest < 1.0f)
        return; // Already at destination

    // Normalize direction
    float dirX = dx / distToDest;
    float dirY = dy / distToDest;

    // Calculate formation angle (perpendicular to movement direction)
    float formationAngle = std::atan2(dirY, dirX);

    // Assign formation positions based on formation type
    uint32 memberIndex = 0;
    uint32 totalMembers = group->GetMembersCount();

    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        // Skip the leader - they move to destination directly
        if (player->GetGUID() == group->GetLeaderGUID())
        {
            memberIndex++;
            continue;
        }

        // Determine player role for positioning
        DungeonRole role = DungeonBehavior::instance()->DeterminePlayerRole(player);

        // Calculate formation offset based on role and formation type
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetDistance = formation.formationRadius / 2.0f;

        if (formation.formationType == "wedge")
        {
            // Wedge formation: Tank at front, healers at back, DPS on sides
            if (role == DungeonRole::TANK)
            {
                // Tank at front (no offset from destination)
                offsetX = 0.0f;
                offsetY = 0.0f;
            }
            else if (role == DungeonRole::HEALER)
            {
                // Healers at back center
                offsetX = -dirX * offsetDistance * 2.0f;
                offsetY = -dirY * offsetDistance * 2.0f;
            }
            else // DPS
            {
                // DPS on sides in V shape
                float sideAngle = (memberIndex % 2 == 0) ? M_PI / 4.0f : -M_PI / 4.0f;
                float rotatedAngle = formationAngle + M_PI + sideAngle;
                offsetX = std::cos(rotatedAngle) * offsetDistance;
                offsetY = std::sin(rotatedAngle) * offsetDistance;
            }
        }
        else if (formation.formationType == "column")
        {
            // Column formation: Single file behind leader
            offsetX = -dirX * offsetDistance * memberIndex;
            offsetY = -dirY * offsetDistance * memberIndex;
        }
        else if (formation.formationType == "spread")
        {
            // Spread formation: Distributed around destination
            float spreadAngle = (2.0f * M_PI * memberIndex) / totalMembers;
            offsetX = std::cos(spreadAngle) * offsetDistance;
            offsetY = std::sin(spreadAngle) * offsetDistance;
        }
        else // Default formation
        {
            // Simple follow formation
            offsetX = -dirX * offsetDistance * (memberIndex + 1);
            offsetY = -dirY * offsetDistance * (memberIndex + 1);
        }

        // Calculate member's destination position
        Position memberDest;
        memberDest.Relocate(
            destination.GetPositionX() + offsetX,
            destination.GetPositionY() + offsetY,
            destination.GetPositionZ()
        );

        // Store in formation data
        _groupFormations[groupId].memberPositions[player->GetGUID().GetCounter()] = memberDest;

        memberIndex++;
    }

    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::SynchronizeGroupMovement - Group {} synchronized {} members to formation type {}",
        groupId, memberIndex, formation.formationType);
}'''

    content = content.replace(old_sync_movement, new_sync_movement)

    # Replace OptimizeResourceDistribution with real healing coordination
    old_optimize_resource = '''void InstanceCoordination::OptimizeResourceDistribution(Group* group)
{
    if (!group)
        return;

    uint32 groupId = group->GetGUID().GetCounter();

    // In a full implementation, this would coordinate healers to prioritize low-health members
    TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::OptimizeResourceDistribution - Group {} optimizing resources",
        groupId);
}'''

    new_optimize_resource = '''void InstanceCoordination::OptimizeResourceDistribution(Group* group)
{
    if (!group)
        return;

    uint32 groupId = group->GetGUID().GetCounter();

    auto resourceItr = _resourceCoordination.find(groupId);
    if (resourceItr == _resourceCoordination.end())
        return;

    ResourceCoordination const& resources = resourceItr->second;

    // Build priority list of members needing healing
    struct HealingPriority
    {
        ObjectGuid guid;
        float healthPercent;
        DungeonRole role;
        uint32 priority; // Lower = higher priority
    };

    ::std::vector<HealingPriority> healingPriorities;

    for (auto const& member : group->GetMemberSlots())
    {
        Player* player = ObjectAccessor::FindPlayer(member.guid);
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        uint32 playerGuidCounter = player->GetGUID().GetCounter();
        auto healthItr = resources.memberHealth.find(playerGuidCounter);
        if (healthItr == resources.memberHealth.end())
            continue;

        float healthPercent = healthItr->second;
        DungeonRole role = DungeonBehavior::instance()->DeterminePlayerRole(player);

        // Only add members that need healing (below 90%)
        if (healthPercent >= 0.9f)
            continue;

        HealingPriority priority;
        priority.guid = player->GetGUID();
        priority.healthPercent = healthPercent;
        priority.role = role;

        // Calculate priority score (lower = higher priority)
        // Tank always highest priority
        if (role == DungeonRole::TANK)
            priority.priority = static_cast<uint32>((1.0f - healthPercent) * 100.0f);
        // Healer next priority
        else if (role == DungeonRole::HEALER)
            priority.priority = static_cast<uint32>((1.0f - healthPercent) * 100.0f) + 100;
        // DPS lowest priority
        else
            priority.priority = static_cast<uint32>((1.0f - healthPercent) * 100.0f) + 200;

        // Emergency priority for very low health
        if (healthPercent < 0.3f)
            priority.priority -= 300; // Emergency boost

        healingPriorities.push_back(priority);
    }

    // Sort by priority (ascending)
    ::std::sort(healingPriorities.begin(), healingPriorities.end(),
        [](HealingPriority const& a, HealingPriority const& b) {
            return a.priority < b.priority;
        });

    // Log healing priority order
    if (!healingPriorities.empty())
    {
        TC_LOG_DEBUG("module.playerbot", "InstanceCoordination::OptimizeResourceDistribution - Group {} healing priority ({} targets):",
            groupId, healingPriorities.size());

        for (size_t i = 0; i < ::std::min(healingPriorities.size(), size_t(5)); ++i)
        {
            HealingPriority const& hp = healingPriorities[i];
            Player* player = ObjectAccessor::FindPlayer(hp.guid);
            if (player)
            {
                TC_LOG_DEBUG("module.playerbot", "  #{}: {} ({:.1f}% HP, role: {}, priority: {})",
                    i + 1, player->GetName(), hp.healthPercent * 100.0f,
                    static_cast<uint32>(hp.role), hp.priority);
            }
        }
    }
}'''

    content = content.replace(old_optimize_resource, new_optimize_resource)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Successfully updated {filepath}")
        return True
    else:
        print(f"No changes needed for {filepath}")
        return False

if __name__ == '__main__':
    implement_instance_coordination()
