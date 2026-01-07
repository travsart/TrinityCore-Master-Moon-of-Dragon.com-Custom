/*
 * Copyright (C) 2024+ TrinityCore <http://www.trinitycore.org/>
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

#include "FlightMasterManager.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerTaxi.h"
#include "TaxiPathGraph.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Playerbot
{
    // ============================================================================
    // FlightMasterManager Implementation
    // ============================================================================

    FlightResult FlightMasterManager::FlyToPosition(
        Player* player,
        Position const& destination,
        FlightPathStrategy strategy)
    {
        // Validate player
    if (!player)
        {
            TC_LOG_ERROR("playerbot.flight", "FlightMasterManager: Invalid player (nullptr)");
            return FlightResult::PLAYER_INVALID;
        }

        // Check if already in flight
    if (player->IsInFlight())
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: Player {} already in flight",
                player->GetName());
            return FlightResult::ALREADY_IN_FLIGHT;
        }

        // Find nearest flight master to player
        auto flightMasterOpt = FindNearestFlightMaster(player);
        if (!flightMasterOpt.has_value())
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: No flight master found near player {}",
                player->GetName());
            return FlightResult::FLIGHT_MASTER_NOT_FOUND;
        }

        FlightMasterLocation const& flightMaster = *flightMasterOpt;
        uint32 sourceNode = flightMaster.taxiNode;

        // Find nearest taxi node to destination (faction-aware)
        uint32 destinationNode = FindNearestTaxiNode(destination, player->GetMapId(), player);
        if (destinationNode == 0)
        {
            TC_LOG_ERROR("playerbot.flight",
                "FlightMasterManager: No taxi node found near destination ({:.1f}, {:.1f}, {:.1f})",
                destination.GetPositionX(),
                destination.GetPositionY(),
                destination.GetPositionZ());
            return FlightResult::PATH_NOT_FOUND;
        }

        // Check if already at destination
    if (sourceNode == destinationNode)
        {
            TC_LOG_DEBUG("playerbot.flight",
                "FlightMasterManager: Player {} already at destination taxi node {}",
                player->GetName(), sourceNode);
            return FlightResult::SAME_LOCATION;
        }

        TC_LOG_DEBUG("playerbot.flight",
            "FlightMasterManager: Player {} flying from node {} to node {} (strategy: {})",
            player->GetName(), sourceNode, destinationNode, static_cast<int>(strategy));

        // Activate flight to destination node
        return FlyToTaxiNode(player, destinationNode, strategy);
    }

    FlightResult FlightMasterManager::FlyToTaxiNode(
        Player* player,
        uint32 destinationNodeId,
        FlightPathStrategy strategy)
    {
        // Validate player
    if (!player)
        {
            TC_LOG_ERROR("playerbot.flight", "FlightMasterManager: Invalid player (nullptr)");
            return FlightResult::PLAYER_INVALID;
        }

        // Check if already in flight
    if (player->IsInFlight())
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: Player {} already in flight",
                player->GetName());
            return FlightResult::ALREADY_IN_FLIGHT;
        }

        // Find nearest flight master
        auto flightMasterOpt = FindNearestFlightMaster(player);
        if (!flightMasterOpt.has_value())
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: No flight master found near player {}",
                player->GetName());
            return FlightResult::FLIGHT_MASTER_NOT_FOUND;
        }

        FlightMasterLocation const& flightMaster = *flightMasterOpt;
        uint32 sourceNode = flightMaster.taxiNode;

        // Validate flight
        FlightResult validationResult = ValidateFlight(player, sourceNode, destinationNodeId);
        if (validationResult != FlightResult::SUCCESS)
            return validationResult;

        // Calculate flight path
        auto pathInfoOpt = CalculateFlightPath(player, sourceNode, destinationNodeId, strategy);
        if (!pathInfoOpt.has_value())
        {
            TC_LOG_ERROR("playerbot.flight",
                "FlightMasterManager: No flight path found from node {} to node {} for player {}",
                sourceNode, destinationNodeId, player->GetName());
            return FlightResult::PATH_NOT_FOUND;
        }

        FlightPathInfo const& pathInfo = *pathInfoOpt;

        // Check if player can afford flight
    if (pathInfo.goldCost > player->GetMoney())
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: Player {} cannot afford flight ({} copper cost, {} copper available)",
                player->GetName(), pathInfo.goldCost, player->GetMoney());
            return FlightResult::INSUFFICIENT_GOLD;
        }

        // Activate taxi path using TrinityCore API
        bool success = player->ActivateTaxiPathTo(pathInfo.nodes);

        if (success)
        {
            TC_LOG_DEBUG("playerbot.flight",
                "FlightMasterManager: Player {} activated taxi path - {} nodes, {:.1f} yards, {} seconds, {} copper",
                player->GetName(), pathInfo.nodes.size(), pathInfo.totalDistance,
                pathInfo.flightTime, pathInfo.goldCost);
            return FlightResult::SUCCESS;
        }
        else
        {
            TC_LOG_ERROR("playerbot.flight",
                "FlightMasterManager: ActivateTaxiPathTo failed for player {} (unknown reason)",
                player->GetName());
            return FlightResult::PATH_NOT_FOUND;
        }
    }

    ::std::optional<FlightMasterLocation> FlightMasterManager::FindNearestFlightMaster(
        Player const* player,
        float maxDistance)
    {
        if (!player)
            return ::std::nullopt;

        Map* map = player->GetMap();
        if (!map)
            return ::std::nullopt;

        FlightMasterLocation nearest;
        float minDistance = maxDistance > 0.0f ? maxDistance : ::std::numeric_limits<float>::max();
        bool found = false;

        // Iterate all creatures on map
        auto const& creatures = map->GetCreatureBySpawnIdStore();
        for (auto const& [spawnId, creature] : creatures)
        {
            if (!creature || !creature->IsInWorld())
                continue;

            // Check if creature is a flight master
            if (!creature->IsTaxi())
                continue;

            // FACTION CHECK: Ensure flight master is friendly to player
            // This prevents Horde bots from selecting Alliance flight masters and vice versa
            if (creature->IsHostileTo(player) || (!creature->IsFriendlyTo(player) && !creature->IsNeutralToAll()))
                continue;

            // Calculate distance to player
            float distance = player->GetDistance2d(creature);
            if (distance < minDistance)
            {
                // Get associated taxi node
                // Flight masters are associated with taxi nodes by proximity
                TaxiNodesEntry const* nearestNode = nullptr;
                float nearestNodeDist = ::std::numeric_limits<float>::max();

                for (TaxiNodesEntry const* node : sTaxiNodesStore)
                {
                    if (!node)
                        continue;

                    // Check if taxi node is on same continent
    if (node->ContinentID != creature->GetMapId())
                        continue;

                    // Calculate distance from creature to taxi node
                    float dx = creature->GetPositionX() - node->Pos.X;
                    float dy = creature->GetPositionY() - node->Pos.Y;
                    float dist = ::std::sqrt(dx * dx + dy * dy);

                    if (dist < nearestNodeDist)
                    {
                        nearestNodeDist = dist;
                        nearestNode = node;
                    }
                }

                if (nearestNode && nearestNodeDist < 50.0f) // Flight master within 50 yards of taxi node
                {
                    minDistance = distance;
                    nearest.guid = creature->GetGUID();
                    nearest.entry = creature->GetEntry();
                    nearest.position = *creature;
                    nearest.taxiNode = nearestNode->ID;
                    nearest.distanceFromPlayer = distance;
                    nearest.name = creature->GetName();
                    found = true;
                }
            }
        }

        return found ? ::std::make_optional(nearest) : ::std::nullopt;
    }

    uint32 FlightMasterManager::FindNearestTaxiNode(
        Position const& position,
        uint32 mapId,
        Player const* player)
    {
        TaxiNodesEntry const* nearestNode = nullptr;
        float minDistance = ::std::numeric_limits<float>::max();

        // Iterate all taxi nodes
        for (TaxiNodesEntry const* node : sTaxiNodesStore)
        {
            if (!node)
                continue;

            // Check if taxi node is on same map
            if (node->ContinentID != mapId)
                continue;

            // Only consider nodes part of taxi network
            if (!node->IsPartOfTaxiNetwork())
                continue;

            // FACTION CHECK: If player is provided, verify taxi node is available for their faction
            // This uses the same TaxiNodeFlags that TrinityCore's TaxiPathGraph uses
            if (player)
            {
                bool isVisibleForFaction = false;
                switch (player->GetTeam())
                {
                    case HORDE:
                        isVisibleForFaction = node->GetFlags().HasFlag(TaxiNodeFlags::ShowOnHordeMap);
                        break;
                    case ALLIANCE:
                        isVisibleForFaction = node->GetFlags().HasFlag(TaxiNodeFlags::ShowOnAllianceMap);
                        break;
                    default:
                        break;
                }
                if (!isVisibleForFaction)
                    continue;
            }

            // Calculate distance to position
            float dx = position.GetPositionX() - node->Pos.X;
            float dy = position.GetPositionY() - node->Pos.Y;
            float distance = ::std::sqrt(dx * dx + dy * dy);

            if (distance < minDistance)
            {
                minDistance = distance;
                nearestNode = node;
            }
        }

        return nearestNode ? nearestNode->ID : 0;
    }

    ::std::optional<FlightPathInfo> FlightMasterManager::CalculateFlightPath(
        Player const* player,
        uint32 sourceNode,
        uint32 destinationNode,
        FlightPathStrategy strategy)
    {
        if (!player)
            return ::std::nullopt;

        // Get taxi node entries
        TaxiNodesEntry const* from = sTaxiNodesStore.LookupEntry(sourceNode);
        TaxiNodesEntry const* to = sTaxiNodesStore.LookupEntry(destinationNode);

        if (!from || !to)
        {
            TC_LOG_ERROR("playerbot.flight",
                "FlightMasterManager: Invalid taxi nodes (source: {}, dest: {})",
                sourceNode, destinationNode);
            return ::std::nullopt;
        }

        // Use TaxiPathGraph to find shortest path
        ::std::vector<uint32> shortestPath;
        ::std::size_t pathCost = TaxiPathGraph::GetCompleteNodeRoute(from, to, player, shortestPath);

        if (shortestPath.empty() || pathCost == 0)
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: No path found from node {} to node {}",
                sourceNode, destinationNode);
            return ::std::nullopt;
        }

        // Build flight path info
        FlightPathInfo pathInfo;
        pathInfo.sourceNode = sourceNode;
        pathInfo.destinationNode = destinationNode;
        pathInfo.nodes = shortestPath;
        pathInfo.stopCount = static_cast<uint32>(shortestPath.size()) - 2; // Exclude source and dest
        pathInfo.totalDistance = CalculateFlightDistance(shortestPath);
        pathInfo.flightTime = EstimateFlightTime(shortestPath);
        pathInfo.goldCost = CalculateFlightCost(player, shortestPath);

        TC_LOG_DEBUG("playerbot.flight",
            "FlightMasterManager: Calculated path - {} nodes, {} stops, {:.1f} yards, {} sec, {} copper",
            shortestPath.size(), pathInfo.stopCount, pathInfo.totalDistance,
            pathInfo.flightTime, pathInfo.goldCost);

        return pathInfo;
    }

    bool FlightMasterManager::HasTaxiNode(
        Player const* player,
        uint32 nodeId)
    {
        if (!player)
            return false;

        return player->m_taxi.IsTaximaskNodeKnown(nodeId);
    }

    char const* FlightMasterManager::GetResultString(FlightResult result)
    {
        switch (result)
        {
            case FlightResult::SUCCESS:
                return "SUCCESS";
            case FlightResult::FLIGHT_MASTER_NOT_FOUND:
                return "FLIGHT_MASTER_NOT_FOUND";
            case FlightResult::NOT_A_FLIGHT_MASTER:
                return "NOT_A_FLIGHT_MASTER";
            case FlightResult::OUT_OF_RANGE:
                return "OUT_OF_RANGE";
            case FlightResult::NODE_UNKNOWN:
                return "NODE_UNKNOWN";
            case FlightResult::PATH_NOT_FOUND:
                return "PATH_NOT_FOUND";
            case FlightResult::INSUFFICIENT_GOLD:
                return "INSUFFICIENT_GOLD";
            case FlightResult::ALREADY_IN_FLIGHT:
                return "ALREADY_IN_FLIGHT";
            case FlightResult::PLAYER_INVALID:
                return "PLAYER_INVALID";
            case FlightResult::DESTINATION_INVALID:
                return "DESTINATION_INVALID";
            case FlightResult::SAME_LOCATION:
                return "SAME_LOCATION";
            default:
                return "UNKNOWN";
        }
    }

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    FlightResult FlightMasterManager::ValidateFlight(
        Player const* player,
        uint32 sourceNode,
        uint32 destinationNode)
    {
        // Validate player
    if (!player)
            return FlightResult::PLAYER_INVALID;

        // Check if already in flight
    if (player->IsInFlight())
            return FlightResult::ALREADY_IN_FLIGHT;

        // Check if source node is valid
        TaxiNodesEntry const* sourceEntry = sTaxiNodesStore.LookupEntry(sourceNode);
        if (!sourceEntry)
            return FlightResult::PATH_NOT_FOUND;

        // Check if destination node is valid
        TaxiNodesEntry const* destEntry = sTaxiNodesStore.LookupEntry(destinationNode);
        if (!destEntry)
            return FlightResult::DESTINATION_INVALID;

        // Check if player has discovered destination node
    if (!HasTaxiNode(player, destinationNode))
        {
            TC_LOG_WARN("playerbot.flight",
                "FlightMasterManager: Player {} has not discovered taxi node {}",
                player->GetName(), destinationNode);
            return FlightResult::NODE_UNKNOWN;
        }

        return FlightResult::SUCCESS;
    }

    uint32 FlightMasterManager::CalculateFlightCost(
        Player const* player,
        ::std::vector<uint32> const& nodes)
    {
        if (!player || nodes.size() < 2)
            return 0;

        // Simplified cost calculation
        // Real implementation would use TaxiPath costs from DB2
        // For now, estimate based on distance

        float totalDistance = CalculateFlightDistance(nodes);

        // Base cost: 1 copper per 10 yards
        uint32 baseCost = static_cast<uint32>(totalDistance / 10.0f);

        // Minimum cost: 10 copper
    if (baseCost < 10)
            baseCost = 10;

        // Apply level scaling (higher level = higher cost)
        uint32 levelMultiplier = player->GetLevel() / 10 + 1;
        uint32 totalCost = baseCost * levelMultiplier;

        return totalCost;
    }

    uint32 FlightMasterManager::EstimateFlightTime(
        ::std::vector<uint32> const& nodes)
    {
        if (nodes.size() < 2)
            return 0;

        float totalDistance = CalculateFlightDistance(nodes);

        // Assume flight speed of ~60 yards/second (typical flying mount speed)
        constexpr float FLIGHT_SPEED = 60.0f;

        uint32 flightTime = static_cast<uint32>(totalDistance / FLIGHT_SPEED);

        // Add 5 seconds per intermediate stop
        uint32 stopTime = (static_cast<uint32>(nodes.size()) - 2) * 5;

        return flightTime + stopTime;
    }

    float FlightMasterManager::CalculateFlightDistance(
        ::std::vector<uint32> const& nodes)
    {
        if (nodes.size() < 2)
            return 0.0f;

        float totalDistance = 0.0f;

        for (size_t i = 0; i < nodes.size() - 1; ++i)
        {
            TaxiNodesEntry const* from = sTaxiNodesStore.LookupEntry(nodes[i]);
            TaxiNodesEntry const* to = sTaxiNodesStore.LookupEntry(nodes[i + 1]);

            if (from && to)
            {
                // Calculate 3D distance between taxi nodes
                float dx = to->Pos.X - from->Pos.X;
                float dy = to->Pos.Y - from->Pos.Y;
                float dz = to->Pos.Z - from->Pos.Z;
                float distance = ::std::sqrt(dx * dx + dy * dy + dz * dz);

                totalDistance += distance;
            }
        }

        return totalDistance;
    }

} // namespace Playerbot
