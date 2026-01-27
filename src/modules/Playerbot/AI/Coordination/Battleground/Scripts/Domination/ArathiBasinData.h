/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2021+ WarheadCore <https://github.com/AzerothCore/WarheadCore>
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_ARATHIBASINDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_ARATHIBASINDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::ArathiBasin
{

// ============================================================================
// MAP INFORMATION
// ============================================================================

constexpr uint32 MAP_ID = 529;
constexpr char BG_NAME[] = "Arathi Basin";
constexpr uint32 MAX_SCORE = 1500;             // Changed from 1600/2000 in various patches
constexpr uint32 MAX_DURATION = 25 * 60 * 1000; // 25 minutes
constexpr uint8 TEAM_SIZE = 15;
constexpr uint32 NODE_COUNT = 5;
constexpr uint32 TICK_INTERVAL = 2000;          // 2 seconds
constexpr uint32 CAPTURE_TIME = 8000;           // 8 seconds to capture/assault

// ============================================================================
// NODE IDENTIFIERS
// ============================================================================

namespace Nodes
{
    constexpr uint32 STABLES = 0;
    constexpr uint32 BLACKSMITH = 1;
    constexpr uint32 FARM = 2;
    constexpr uint32 GOLD_MINE = 3;
    constexpr uint32 LUMBER_MILL = 4;
}

// ============================================================================
// NODE POSITIONS
// ============================================================================

// Stables (Alliance-side, north)
constexpr float STABLES_X = 1166.785f;
constexpr float STABLES_Y = 1200.132f;
constexpr float STABLES_Z = -56.70f;
constexpr float STABLES_O = 0.0f;

// Blacksmith (Center, critical)
constexpr float BLACKSMITH_X = 977.017f;
constexpr float BLACKSMITH_Y = 1046.534f;
constexpr float BLACKSMITH_Z = -44.80f;
constexpr float BLACKSMITH_O = 0.0f;

// Farm (Horde-side, south)
constexpr float FARM_X = 806.218f;
constexpr float FARM_Y = 874.217f;
constexpr float FARM_Z = -55.99f;
constexpr float FARM_O = 0.0f;

// Gold Mine (Horde-side, east)
constexpr float GOLD_MINE_X = 1146.923f;
constexpr float GOLD_MINE_Y = 848.277f;
constexpr float GOLD_MINE_Z = -110.52f;
constexpr float GOLD_MINE_O = 0.0f;

// Lumber Mill (Alliance-side, west - high ground)
constexpr float LUMBER_MILL_X = 856.141f;
constexpr float LUMBER_MILL_Y = 1148.902f;
constexpr float LUMBER_MILL_Z = 11.18f;
constexpr float LUMBER_MILL_O = 0.0f;

inline Position GetNodePosition(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::STABLES:     return Position(STABLES_X, STABLES_Y, STABLES_Z, STABLES_O);
        case Nodes::BLACKSMITH:  return Position(BLACKSMITH_X, BLACKSMITH_Y, BLACKSMITH_Z, BLACKSMITH_O);
        case Nodes::FARM:        return Position(FARM_X, FARM_Y, FARM_Z, FARM_O);
        case Nodes::GOLD_MINE:   return Position(GOLD_MINE_X, GOLD_MINE_Y, GOLD_MINE_Z, GOLD_MINE_O);
        case Nodes::LUMBER_MILL: return Position(LUMBER_MILL_X, LUMBER_MILL_Y, LUMBER_MILL_Z, LUMBER_MILL_O);
        default: return Position(0, 0, 0, 0);
    }
}

inline const char* GetNodeName(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::STABLES:     return "Stables";
        case Nodes::BLACKSMITH:  return "Blacksmith";
        case Nodes::FARM:        return "Farm";
        case Nodes::GOLD_MINE:   return "Gold Mine";
        case Nodes::LUMBER_MILL: return "Lumber Mill";
        default: return "Unknown";
    }
}

// Node strategic values (1-10)
inline uint8 GetNodeStrategicValue(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::STABLES:     return 7;  // Alliance home base
        case Nodes::BLACKSMITH:  return 10; // Center - critical control point
        case Nodes::FARM:        return 7;  // Horde home base
        case Nodes::GOLD_MINE:   return 6;  // Distant, lower value
        case Nodes::LUMBER_MILL: return 8;  // High ground advantage
        default: return 5;
    }
}

// ============================================================================
// TICK POINTS TABLE
// ============================================================================

// Points per tick based on nodes controlled
// [0] = 0 nodes, [1] = 1 node, etc.
constexpr std::array<uint32, 6> TICK_POINTS = {
    0,   // 0 nodes
    10,  // 1 node
    10,  // 2 nodes
    10,  // 3 nodes
    10,  // 4 nodes
    30   // 5 nodes (full control bonus)
};

// Resources per second (approximate for planning)
constexpr float RESOURCE_RATE_PER_NODE = 5.0f;  // 10 points / 2 seconds

// ============================================================================
// SPAWN POSITIONS
// ============================================================================

// Alliance spawn (Trollbane Hall)
constexpr Position ALLIANCE_SPAWNS[] = {
    { 1285.96f, 1281.62f, -15.67f, 0.7f },
    { 1280.96f, 1276.62f, -15.67f, 0.7f },
    { 1290.96f, 1286.62f, -15.67f, 0.7f },
    { 1275.96f, 1271.62f, -15.67f, 0.7f },
    { 1295.96f, 1291.62f, -15.67f, 0.7f }
};

// Horde spawn (Defiler's Den)
constexpr Position HORDE_SPAWNS[] = {
    { 686.57f, 683.04f, -12.59f, 0.7f },
    { 691.57f, 688.04f, -12.59f, 0.7f },
    { 681.57f, 678.04f, -12.59f, 0.7f },
    { 696.57f, 693.04f, -12.59f, 0.7f },
    { 676.57f, 673.04f, -12.59f, 0.7f }
};

// ============================================================================
// GRAVEYARD POSITIONS (one per node when controlled)
// ============================================================================

constexpr float STABLES_GY_X = 1237.64f;
constexpr float STABLES_GY_Y = 1212.19f;
constexpr float STABLES_GY_Z = -57.74f;

constexpr float BLACKSMITH_GY_X = 1016.49f;
constexpr float BLACKSMITH_GY_Y = 1062.50f;
constexpr float BLACKSMITH_GY_Z = -44.64f;

constexpr float FARM_GY_X = 809.67f;
constexpr float FARM_GY_Y = 842.91f;
constexpr float FARM_GY_Z = -56.11f;

constexpr float GOLD_MINE_GY_X = 1104.35f;
constexpr float GOLD_MINE_GY_Y = 819.79f;
constexpr float GOLD_MINE_GY_Z = -111.06f;

constexpr float LUMBER_MILL_GY_X = 847.68f;
constexpr float LUMBER_MILL_GY_Y = 1176.47f;
constexpr float LUMBER_MILL_GY_Z = 12.22f;

inline Position GetNodeGraveyard(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::STABLES:     return Position(STABLES_GY_X, STABLES_GY_Y, STABLES_GY_Z, 0);
        case Nodes::BLACKSMITH:  return Position(BLACKSMITH_GY_X, BLACKSMITH_GY_Y, BLACKSMITH_GY_Z, 0);
        case Nodes::FARM:        return Position(FARM_GY_X, FARM_GY_Y, FARM_GY_Z, 0);
        case Nodes::GOLD_MINE:   return Position(GOLD_MINE_GY_X, GOLD_MINE_GY_Y, GOLD_MINE_GY_Z, 0);
        case Nodes::LUMBER_MILL: return Position(LUMBER_MILL_GY_X, LUMBER_MILL_GY_Y, LUMBER_MILL_GY_Z, 0);
        default: return Position(0, 0, 0, 0);
    }
}

// ============================================================================
// NODE DEFENSE POSITIONS
// ============================================================================

inline std::vector<Position> GetNodeDefensePositions(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::STABLES:
            return {
                { 1166.78f, 1200.13f, -56.70f, 0.0f },  // Flag
                { 1156.78f, 1200.13f, -56.70f, 3.14f }, // North
                { 1176.78f, 1200.13f, -56.70f, 0.0f },  // South
                { 1166.78f, 1190.13f, -56.70f, 1.57f }, // East
                { 1166.78f, 1210.13f, -56.70f, 4.71f }  // West
            };
        case Nodes::BLACKSMITH:
            return {
                { 977.02f, 1046.53f, -44.80f, 0.0f },   // Flag (center)
                { 967.02f, 1046.53f, -44.80f, 3.14f },
                { 987.02f, 1046.53f, -44.80f, 0.0f },
                { 977.02f, 1036.53f, -44.80f, 1.57f },
                { 977.02f, 1056.53f, -44.80f, 4.71f },
                { 972.02f, 1041.53f, -44.80f, 2.36f },  // Extra - critical node
                { 982.02f, 1051.53f, -44.80f, 5.50f }
            };
        case Nodes::FARM:
            return {
                { 806.22f, 874.22f, -55.99f, 0.0f },
                { 796.22f, 874.22f, -55.99f, 3.14f },
                { 816.22f, 874.22f, -55.99f, 0.0f },
                { 806.22f, 864.22f, -55.99f, 1.57f },
                { 806.22f, 884.22f, -55.99f, 4.71f }
            };
        case Nodes::GOLD_MINE:
            return {
                { 1146.92f, 848.28f, -110.52f, 0.0f },
                { 1136.92f, 848.28f, -110.52f, 3.14f },
                { 1156.92f, 848.28f, -110.52f, 0.0f },
                { 1146.92f, 838.28f, -110.52f, 1.57f },
                { 1146.92f, 858.28f, -110.52f, 4.71f }
            };
        case Nodes::LUMBER_MILL:
            return {
                { 856.14f, 1148.90f, 11.18f, 0.0f },    // Flag
                { 846.14f, 1148.90f, 11.18f, 3.14f },
                { 866.14f, 1148.90f, 11.18f, 0.0f },
                { 856.14f, 1138.90f, 11.18f, 1.57f },
                { 856.14f, 1158.90f, 11.18f, 4.71f },
                { 861.14f, 1143.90f, 11.18f, 0.79f }    // Cliff edge - good LoS
            };
        default:
            return {};
    }
}

// ============================================================================
// STRATEGIC ROUTE DATA
// ============================================================================

// Standard opening routes by faction
inline std::vector<uint32> GetAllianceOpeningRoute()
{
    return { Nodes::STABLES, Nodes::BLACKSMITH, Nodes::LUMBER_MILL };
}

inline std::vector<uint32> GetHordeOpeningRoute()
{
    return { Nodes::FARM, Nodes::BLACKSMITH, Nodes::GOLD_MINE };
}

// Node adjacency (which nodes are close to each other)
inline std::vector<uint32> GetAdjacentNodes(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::STABLES:     return { Nodes::BLACKSMITH, Nodes::LUMBER_MILL };
        case Nodes::BLACKSMITH:  return { Nodes::STABLES, Nodes::FARM, Nodes::GOLD_MINE, Nodes::LUMBER_MILL };
        case Nodes::FARM:        return { Nodes::BLACKSMITH, Nodes::GOLD_MINE };
        case Nodes::GOLD_MINE:   return { Nodes::FARM, Nodes::BLACKSMITH };
        case Nodes::LUMBER_MILL: return { Nodes::STABLES, Nodes::BLACKSMITH };
        default: return {};
    }
}

// ============================================================================
// WORLD STATES
// ============================================================================

namespace WorldStates
{
    // Node state world states (show icon on map)
    constexpr int32 STABLES_ICON = 1842;
    constexpr int32 STABLES_ALLIANCE = 1767;
    constexpr int32 STABLES_HORDE = 1768;
    constexpr int32 STABLES_HORDE_CONTROLLED = 1769;
    constexpr int32 STABLES_ALLIANCE_CONTROLLED = 1770;

    constexpr int32 BLACKSMITH_ICON = 1846;
    constexpr int32 BLACKSMITH_ALLIANCE = 1772;
    constexpr int32 BLACKSMITH_HORDE = 1773;
    constexpr int32 BLACKSMITH_HORDE_CONTROLLED = 1774;
    constexpr int32 BLACKSMITH_ALLIANCE_CONTROLLED = 1775;

    constexpr int32 FARM_ICON = 1845;
    constexpr int32 FARM_ALLIANCE = 1801;
    constexpr int32 FARM_HORDE = 1802;
    constexpr int32 FARM_HORDE_CONTROLLED = 1803;
    constexpr int32 FARM_ALLIANCE_CONTROLLED = 1804;

    constexpr int32 GOLD_MINE_ICON = 1843;
    constexpr int32 GOLD_MINE_ALLIANCE = 1782;
    constexpr int32 GOLD_MINE_HORDE = 1783;
    constexpr int32 GOLD_MINE_HORDE_CONTROLLED = 1784;
    constexpr int32 GOLD_MINE_ALLIANCE_CONTROLLED = 1785;

    constexpr int32 LUMBER_MILL_ICON = 1844;
    constexpr int32 LUMBER_MILL_ALLIANCE = 1792;
    constexpr int32 LUMBER_MILL_HORDE = 1793;
    constexpr int32 LUMBER_MILL_HORDE_CONTROLLED = 1794;
    constexpr int32 LUMBER_MILL_ALLIANCE_CONTROLLED = 1795;

    // Resource totals
    constexpr int32 RESOURCES_ALLY = 1776;
    constexpr int32 RESOURCES_HORDE = 1777;

    // Max resources
    constexpr int32 MAX_RESOURCES = 1780;
}

// ============================================================================
// GAME OBJECTS
// ============================================================================

namespace GameObjects
{
    // Node banner objects
    constexpr uint32 STABLES_BANNER = 180087;
    constexpr uint32 BLACKSMITH_BANNER = 180088;
    constexpr uint32 FARM_BANNER = 180089;
    constexpr uint32 GOLD_MINE_BANNER = 180090;
    constexpr uint32 LUMBER_MILL_BANNER = 180091;

    // Aura objects (show control state)
    constexpr uint32 ALLIANCE_BANNER = 180058;
    constexpr uint32 HORDE_BANNER = 180059;
    constexpr uint32 NEUTRAL_BANNER = 180060;
}

// ============================================================================
// SPELLS
// ============================================================================

namespace Spells
{
    constexpr uint32 HONORABLE_DEFENDER_25 = 21235;  // +25% honor when defending
    constexpr uint32 HONORABLE_DEFENDER_50 = 21236;  // +50% honor (2+ defenders)
}

} // namespace Playerbot::Coordination::Battleground::ArathiBasin

#endif // PLAYERBOT_AI_COORDINATION_BG_DOMINATION_ARATHIBASINDATA_H
