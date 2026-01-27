/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_TEMPLEOFKOTMOGUDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_TEMPLEOFKOTMOGUDATA_H

#include "Define.h"
#include "Position.h"
#include <array>

namespace Playerbot::Coordination::Battleground::TempleOfKotmogu
{

constexpr uint32 MAP_ID = 998;
constexpr char BG_NAME[] = "Temple of Kotmogu";
constexpr uint32 MAX_SCORE = 1500;
constexpr uint32 MAX_DURATION = 25 * 60 * 1000;
constexpr uint8 TEAM_SIZE = 10;
constexpr uint32 ORB_COUNT = 4;
constexpr uint32 TICK_INTERVAL = 2000;

// Orb positions (corners of temple)
namespace Orbs
{
    constexpr uint32 ORANGE = 0;  // NE
    constexpr uint32 BLUE = 1;    // NW
    constexpr uint32 GREEN = 2;   // SE
    constexpr uint32 PURPLE = 3;  // SW
}

constexpr float ORB_POSITIONS[][4] = {
    { 1784.58f, 1200.85f, 29.31f, 0.0f },  // Orange
    { 1784.58f, 1374.95f, 29.31f, 0.0f },  // Blue
    { 1680.28f, 1200.85f, 29.31f, 0.0f },  // Green
    { 1680.28f, 1374.95f, 29.31f, 0.0f }   // Purple
};

// Center position (more points when holding orb in center)
constexpr float CENTER_X = 1732.0f;
constexpr float CENTER_Y = 1287.0f;
constexpr float CENTER_Z = 13.0f;
constexpr float CENTER_RADIUS = 25.0f;  // Distance for "center" bonus

// Points per tick based on orb count and location
constexpr uint32 POINTS_BASE = 3;           // Per orb, outside center
constexpr uint32 POINTS_CENTER = 5;         // Per orb, in center
constexpr uint32 POINTS_CENTER_BONUS = 10;  // Bonus per orb in center

namespace WorldStates
{
    constexpr int32 SCORE_ALLY = 6303;
    constexpr int32 SCORE_HORDE = 6304;
}

inline Position GetOrbPosition(uint32 orbId)
{
    if (orbId < ORB_COUNT)
        return Position(ORB_POSITIONS[orbId][0], ORB_POSITIONS[orbId][1], ORB_POSITIONS[orbId][2], 0);
    return Position(0, 0, 0, 0);
}

inline Position GetCenterPosition()
{
    return Position(CENTER_X, CENTER_Y, CENTER_Z, 0);
}

inline const char* GetOrbName(uint32 orbId)
{
    switch (orbId)
    {
        case Orbs::ORANGE: return "Orange Orb";
        case Orbs::BLUE: return "Blue Orb";
        case Orbs::GREEN: return "Green Orb";
        case Orbs::PURPLE: return "Purple Orb";
        default: return "Unknown Orb";
    }
}

} // namespace Playerbot::Coordination::Battleground::TempleOfKotmogu

#endif
