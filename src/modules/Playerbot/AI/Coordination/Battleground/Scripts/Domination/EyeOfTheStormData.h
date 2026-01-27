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

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_EYEOFTHESTORMDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_EYEOFTHESTORMDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::EyeOfTheStorm
{

// ============================================================================
// MAP INFORMATION
// ============================================================================

constexpr uint32 MAP_ID = 566;
constexpr char BG_NAME[] = "Eye of the Storm";
constexpr uint32 MAX_SCORE = 1500;
constexpr uint32 MAX_DURATION = 25 * 60 * 1000;  // 25 minutes
constexpr uint8 TEAM_SIZE = 15;
constexpr uint32 NODE_COUNT = 4;
constexpr uint32 TICK_INTERVAL = 2000;
constexpr uint32 CAPTURE_TIME = 8000;

// Flag capture points (per node count)
constexpr uint32 FLAG_POINTS_1_NODE = 75;
constexpr uint32 FLAG_POINTS_2_NODES = 85;
constexpr uint32 FLAG_POINTS_3_NODES = 100;
constexpr uint32 FLAG_POINTS_4_NODES = 500;  // Huge bonus for 4-cap

// ============================================================================
// NODE IDENTIFIERS
// ============================================================================

namespace Nodes
{
    constexpr uint32 FEL_REAVER = 0;       // Horde-side
    constexpr uint32 BLOOD_ELF = 1;        // Alliance-side
    constexpr uint32 DRAENEI_RUINS = 2;    // Alliance-side
    constexpr uint32 MAGE_TOWER = 3;       // Horde-side
}

// ============================================================================
// NODE POSITIONS
// ============================================================================

// Fel Reaver Ruins (Horde-side, north)
constexpr float FEL_REAVER_X = 2044.28f;
constexpr float FEL_REAVER_Y = 1729.68f;
constexpr float FEL_REAVER_Z = 1189.96f;
constexpr float FEL_REAVER_O = 0.0f;

// Blood Elf Tower (Alliance-side, south)
constexpr float BLOOD_ELF_X = 2048.71f;
constexpr float BLOOD_ELF_Y = 1393.65f;
constexpr float BLOOD_ELF_Z = 1194.05f;
constexpr float BLOOD_ELF_O = 0.0f;

// Draenei Ruins (Alliance-side, west)
constexpr float DRAENEI_RUINS_X = 2284.31f;
constexpr float DRAENEI_RUINS_Y = 1576.87f;
constexpr float DRAENEI_RUINS_Z = 1177.13f;
constexpr float DRAENEI_RUINS_O = 0.0f;

// Mage Tower (Horde-side, east)
constexpr float MAGE_TOWER_X = 1807.26f;
constexpr float MAGE_TOWER_Y = 1539.78f;
constexpr float MAGE_TOWER_Z = 1267.63f;
constexpr float MAGE_TOWER_O = 0.0f;

// Center flag position
constexpr float CENTER_FLAG_X = 2174.78f;
constexpr float CENTER_FLAG_Y = 1569.05f;
constexpr float CENTER_FLAG_Z = 1159.96f;
constexpr float CENTER_FLAG_O = 0.0f;

inline Position GetNodePosition(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::FEL_REAVER:     return Position(FEL_REAVER_X, FEL_REAVER_Y, FEL_REAVER_Z, FEL_REAVER_O);
        case Nodes::BLOOD_ELF:      return Position(BLOOD_ELF_X, BLOOD_ELF_Y, BLOOD_ELF_Z, BLOOD_ELF_O);
        case Nodes::DRAENEI_RUINS:  return Position(DRAENEI_RUINS_X, DRAENEI_RUINS_Y, DRAENEI_RUINS_Z, DRAENEI_RUINS_O);
        case Nodes::MAGE_TOWER:     return Position(MAGE_TOWER_X, MAGE_TOWER_Y, MAGE_TOWER_Z, MAGE_TOWER_O);
        default: return Position(0, 0, 0, 0);
    }
}

inline Position GetCenterFlagPosition()
{
    return Position(CENTER_FLAG_X, CENTER_FLAG_Y, CENTER_FLAG_Z, CENTER_FLAG_O);
}

inline const char* GetNodeName(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::FEL_REAVER:     return "Fel Reaver Ruins";
        case Nodes::BLOOD_ELF:      return "Blood Elf Tower";
        case Nodes::DRAENEI_RUINS:  return "Draenei Ruins";
        case Nodes::MAGE_TOWER:     return "Mage Tower";
        default: return "Unknown";
    }
}

inline uint8 GetNodeStrategicValue(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::FEL_REAVER:     return 7;
        case Nodes::BLOOD_ELF:      return 7;
        case Nodes::DRAENEI_RUINS:  return 8;  // Closer to center
        case Nodes::MAGE_TOWER:     return 8;  // Closer to center
        default: return 5;
    }
}

// ============================================================================
// TICK POINTS TABLE
// ============================================================================

constexpr std::array<uint32, 5> TICK_POINTS = {
    0,   // 0 nodes
    1,   // 1 node
    2,   // 2 nodes
    5,   // 3 nodes
    10   // 4 nodes
};

// ============================================================================
// SPAWN POSITIONS
// ============================================================================

constexpr Position ALLIANCE_SPAWNS[] = {
    { 2523.68f, 1596.59f, 1269.35f, 3.14f },
    { 2518.68f, 1591.59f, 1269.35f, 3.14f },
    { 2528.68f, 1601.59f, 1269.35f, 3.14f },
    { 2513.68f, 1586.59f, 1269.35f, 3.14f },
    { 2533.68f, 1606.59f, 1269.35f, 3.14f }
};

constexpr Position HORDE_SPAWNS[] = {
    { 1803.73f, 1539.41f, 1267.63f, 0.0f },
    { 1808.73f, 1544.41f, 1267.63f, 0.0f },
    { 1798.73f, 1534.41f, 1267.63f, 0.0f },
    { 1813.73f, 1549.41f, 1267.63f, 0.0f },
    { 1793.73f, 1529.41f, 1267.63f, 0.0f }
};

// ============================================================================
// NODE DEFENSE POSITIONS
// ============================================================================

inline std::vector<Position> GetNodeDefensePositions(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::FEL_REAVER:
            return {
                { 2044.28f, 1729.68f, 1189.96f, 0.0f },
                { 2034.28f, 1729.68f, 1189.96f, 3.14f },
                { 2054.28f, 1729.68f, 1189.96f, 0.0f },
                { 2044.28f, 1719.68f, 1189.96f, 1.57f },
                { 2044.28f, 1739.68f, 1189.96f, 4.71f }
            };
        case Nodes::BLOOD_ELF:
            return {
                { 2048.71f, 1393.65f, 1194.05f, 0.0f },
                { 2038.71f, 1393.65f, 1194.05f, 3.14f },
                { 2058.71f, 1393.65f, 1194.05f, 0.0f },
                { 2048.71f, 1383.65f, 1194.05f, 1.57f },
                { 2048.71f, 1403.65f, 1194.05f, 4.71f }
            };
        case Nodes::DRAENEI_RUINS:
            return {
                { 2284.31f, 1576.87f, 1177.13f, 0.0f },
                { 2274.31f, 1576.87f, 1177.13f, 3.14f },
                { 2294.31f, 1576.87f, 1177.13f, 0.0f },
                { 2284.31f, 1566.87f, 1177.13f, 1.57f },
                { 2284.31f, 1586.87f, 1177.13f, 4.71f }
            };
        case Nodes::MAGE_TOWER:
            return {
                { 1807.26f, 1539.78f, 1267.63f, 0.0f },
                { 1797.26f, 1539.78f, 1267.63f, 3.14f },
                { 1817.26f, 1539.78f, 1267.63f, 0.0f },
                { 1807.26f, 1529.78f, 1267.63f, 1.57f },
                { 1807.26f, 1549.78f, 1267.63f, 4.71f }
            };
        default:
            return {};
    }
}

// ============================================================================
// WORLD STATES
// ============================================================================

namespace WorldStates
{
    // Node states
    constexpr int32 FEL_REAVER_ALLIANCE = 2722;
    constexpr int32 FEL_REAVER_HORDE = 2723;
    constexpr int32 BLOOD_ELF_ALLIANCE = 2724;
    constexpr int32 BLOOD_ELF_HORDE = 2725;
    constexpr int32 DRAENEI_RUINS_ALLIANCE = 2726;
    constexpr int32 DRAENEI_RUINS_HORDE = 2727;
    constexpr int32 MAGE_TOWER_ALLIANCE = 2728;
    constexpr int32 MAGE_TOWER_HORDE = 2729;

    // Resources
    constexpr int32 RESOURCES_ALLY = 2749;
    constexpr int32 RESOURCES_HORDE = 2750;

    // Flag state
    constexpr int32 FLAG_STATE = 2757;
}

// ============================================================================
// STRATEGIC ROUTES
// ============================================================================

inline std::vector<uint32> GetAllianceOpeningRoute()
{
    return { Nodes::BLOOD_ELF, Nodes::DRAENEI_RUINS };
}

inline std::vector<uint32> GetHordeOpeningRoute()
{
    return { Nodes::FEL_REAVER, Nodes::MAGE_TOWER };
}

// Route from node to center flag
inline std::vector<Position> GetRouteToCenter(uint32 nodeId)
{
    Position nodePos = GetNodePosition(nodeId);
    Position centerPos = GetCenterFlagPosition();

    // Simple direct route (could be enhanced with pathfinding)
    return { nodePos, centerPos };
}

} // namespace Playerbot::Coordination::Battleground::EyeOfTheStorm

#endif // PLAYERBOT_AI_COORDINATION_BG_DOMINATION_EYEOFTHESTORMDATA_H
