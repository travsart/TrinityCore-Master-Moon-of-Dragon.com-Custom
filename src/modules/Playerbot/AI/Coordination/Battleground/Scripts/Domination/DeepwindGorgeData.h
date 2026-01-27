/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_DEEPWINDGORGEDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_DEEPWINDGORGEDATA_H

#include "Define.h"
#include "Position.h"
#include <array>

namespace Playerbot::Coordination::Battleground::DeepwindGorge
{

constexpr uint32 MAP_ID = 1105;
constexpr char BG_NAME[] = "Deepwind Gorge";
constexpr uint32 MAX_SCORE = 1500;
constexpr uint32 MAX_DURATION = 25 * 60 * 1000;
constexpr uint8 TEAM_SIZE = 10;
constexpr uint32 NODE_COUNT = 3;
constexpr uint32 CART_COUNT = 3;
constexpr uint32 TICK_INTERVAL = 2000;
constexpr uint32 CAPTURE_TIME = 8000;

namespace Nodes
{
    constexpr uint32 PANDAREN_MINE = 0;   // Center
    constexpr uint32 GOBLIN_MINE = 1;     // Alliance side
    constexpr uint32 CENTER_MINE = 2;     // Horde side
}

// Node positions
constexpr float PANDAREN_MINE_X = 1600.53f;
constexpr float PANDAREN_MINE_Y = 945.24f;
constexpr float PANDAREN_MINE_Z = 0.0f;

constexpr float GOBLIN_MINE_X = 1447.27f;
constexpr float GOBLIN_MINE_Y = 1110.36f;
constexpr float GOBLIN_MINE_Z = 0.0f;

constexpr float CENTER_MINE_X = 1753.79f;
constexpr float CENTER_MINE_Y = 780.12f;
constexpr float CENTER_MINE_Z = 0.0f;

inline Position GetNodePosition(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::PANDAREN_MINE: return Position(PANDAREN_MINE_X, PANDAREN_MINE_Y, PANDAREN_MINE_Z, 0);
        case Nodes::GOBLIN_MINE: return Position(GOBLIN_MINE_X, GOBLIN_MINE_Y, GOBLIN_MINE_Z, 0);
        case Nodes::CENTER_MINE: return Position(CENTER_MINE_X, CENTER_MINE_Y, CENTER_MINE_Z, 0);
        default: return Position(0, 0, 0, 0);
    }
}

inline const char* GetNodeName(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::PANDAREN_MINE: return "Pandaren Mine";
        case Nodes::GOBLIN_MINE: return "Goblin Mine";
        case Nodes::CENTER_MINE: return "Center Mine";
        default: return "Unknown";
    }
}

// Cart tracks
namespace Carts
{
    constexpr uint32 CART_NORTH = 0;
    constexpr uint32 CART_CENTER = 1;
    constexpr uint32 CART_SOUTH = 2;
}

// Cart capture points
constexpr float ALLIANCE_CART_DEPOT_X = 1350.0f;
constexpr float ALLIANCE_CART_DEPOT_Y = 1050.0f;
constexpr float ALLIANCE_CART_DEPOT_Z = 0.0f;

constexpr float HORDE_CART_DEPOT_X = 1850.0f;
constexpr float HORDE_CART_DEPOT_Y = 850.0f;
constexpr float HORDE_CART_DEPOT_Z = 0.0f;

inline Position GetCartDepotPosition(uint32 faction)
{
    if (faction == 1)  // ALLIANCE
        return Position(ALLIANCE_CART_DEPOT_X, ALLIANCE_CART_DEPOT_Y, ALLIANCE_CART_DEPOT_Z, 0);
    else  // HORDE
        return Position(HORDE_CART_DEPOT_X, HORDE_CART_DEPOT_Y, HORDE_CART_DEPOT_Z, 0);
}

constexpr std::array<uint32, 4> TICK_POINTS = { 0, 1, 3, 10 };
constexpr uint32 CART_CAPTURE_POINTS = 200;

namespace WorldStates
{
    constexpr int32 RESOURCES_ALLY = 6446;
    constexpr int32 RESOURCES_HORDE = 6447;
}

} // namespace Playerbot::Coordination::Battleground::DeepwindGorge

#endif
