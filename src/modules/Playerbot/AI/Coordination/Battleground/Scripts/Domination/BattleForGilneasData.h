/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_BATTLEFORGILNEASDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_BATTLEFORGILNEASDATA_H

#include "Define.h"
#include "Position.h"
#include <array>

namespace Playerbot::Coordination::Battleground::BattleForGilneas
{

constexpr uint32 MAP_ID = 761;
constexpr char BG_NAME[] = "Battle for Gilneas";
constexpr uint32 MAX_SCORE = 2000;
constexpr uint32 MAX_DURATION = 25 * 60 * 1000;
constexpr uint8 TEAM_SIZE = 10;
constexpr uint32 NODE_COUNT = 3;
constexpr uint32 TICK_INTERVAL = 2000;
constexpr uint32 CAPTURE_TIME = 8000;

namespace Nodes
{
    constexpr uint32 LIGHTHOUSE = 0;  // Alliance side
    constexpr uint32 WATERWORKS = 1;  // Center
    constexpr uint32 MINES = 2;       // Horde side
}

constexpr float LIGHTHOUSE_X = 1057.73f;
constexpr float LIGHTHOUSE_Y = 1278.29f;
constexpr float LIGHTHOUSE_Z = 3.19f;

constexpr float WATERWORKS_X = 980.07f;
constexpr float WATERWORKS_Y = 948.17f;
constexpr float WATERWORKS_Z = 12.72f;

constexpr float MINES_X = 1251.01f;
constexpr float MINES_Y = 836.59f;
constexpr float MINES_Z = -7.43f;

inline Position GetNodePosition(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::LIGHTHOUSE: return Position(LIGHTHOUSE_X, LIGHTHOUSE_Y, LIGHTHOUSE_Z, 0);
        case Nodes::WATERWORKS: return Position(WATERWORKS_X, WATERWORKS_Y, WATERWORKS_Z, 0);
        case Nodes::MINES: return Position(MINES_X, MINES_Y, MINES_Z, 0);
        default: return Position(0, 0, 0, 0);
    }
}

inline const char* GetNodeName(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::LIGHTHOUSE: return "Lighthouse";
        case Nodes::WATERWORKS: return "Waterworks";
        case Nodes::MINES: return "Mines";
        default: return "Unknown";
    }
}

constexpr std::array<uint32, 4> TICK_POINTS = { 0, 1, 3, 10 };

namespace WorldStates
{
    constexpr int32 RESOURCES_ALLY = 5496;
    constexpr int32 RESOURCES_HORDE = 5497;
}

} // namespace Playerbot::Coordination::Battleground::BattleForGilneas

#endif // PLAYERBOT_AI_COORDINATION_BG_DOMINATION_BATTLEFORGILNEASDATA_H
