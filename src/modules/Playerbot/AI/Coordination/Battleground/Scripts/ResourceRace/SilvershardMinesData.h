/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_RESOURCERACE_SILVERSHARDMINESDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_RESOURCERACE_SILVERSHARDMINESDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::SilvershardMines
{

constexpr uint32 MAP_ID = 727;
constexpr char BG_NAME[] = "Silvershard Mines";
constexpr uint32 MAX_SCORE = 1600;
constexpr uint32 MAX_DURATION = 25 * 60 * 1000;
constexpr uint8 TEAM_SIZE = 10;
constexpr uint32 CART_COUNT = 3;
constexpr uint32 POINTS_PER_CAPTURE = 200;
constexpr uint32 TICK_INTERVAL = 1000;

// Cart tracks
namespace Tracks
{
    constexpr uint32 LAVA = 0;      // Bottom track (through lava)
    constexpr uint32 UPPER = 1;    // Upper track
    constexpr uint32 DIAMOND = 2;  // Diamond track (starts later)
    constexpr uint32 TRACK_COUNT = 3;
}

// Cart spawn positions
constexpr Position CART_SPAWN_POSITIONS[] = {
    Position(830.0f, 190.0f, 387.0f, 0),   // Lava cart spawn
    Position(746.0f, 305.0f, 402.0f, 0),   // Upper cart spawn
    Position(933.0f, 305.0f, 404.0f, 0)    // Diamond cart spawn
};

// Cart destination positions (depot)
constexpr Position ALLIANCE_DEPOT = Position(580.0f, 200.0f, 380.0f, 0);
constexpr Position HORDE_DEPOT = Position(1050.0f, 200.0f, 380.0f, 0);

// Intersection positions (where carts can change direction)
namespace Intersections
{
    constexpr uint32 LAVA_UPPER = 0;     // Lava-Upper intersection
    constexpr uint32 UPPER_DIAMOND = 1;  // Upper-Diamond intersection
    constexpr uint32 INTERSECTION_COUNT = 2;
}

constexpr Position INTERSECTION_POSITIONS[] = {
    Position(746.0f, 248.0f, 395.0f, 0),   // Lava-Upper
    Position(846.0f, 305.0f, 403.0f, 0)    // Upper-Diamond
};

// Spawn positions
constexpr float ALLIANCE_SPAWN_X = 570.0f;
constexpr float ALLIANCE_SPAWN_Y = 215.0f;
constexpr float ALLIANCE_SPAWN_Z = 380.0f;

constexpr float HORDE_SPAWN_X = 1058.0f;
constexpr float HORDE_SPAWN_Y = 215.0f;
constexpr float HORDE_SPAWN_Z = 380.0f;

inline Position GetSpawnPosition(uint32 faction)
{
    if (faction == 1)  // ALLIANCE
        return Position(ALLIANCE_SPAWN_X, ALLIANCE_SPAWN_Y, ALLIANCE_SPAWN_Z, 0);
    else
        return Position(HORDE_SPAWN_X, HORDE_SPAWN_Y, HORDE_SPAWN_Z, 0);
}

inline Position GetCartSpawnPosition(uint32 cartId)
{
    if (cartId < CART_COUNT)
        return CART_SPAWN_POSITIONS[cartId];
    return Position(0, 0, 0, 0);
}

inline Position GetIntersectionPosition(uint32 intersectionId)
{
    if (intersectionId < Intersections::INTERSECTION_COUNT)
        return INTERSECTION_POSITIONS[intersectionId];
    return Position(0, 0, 0, 0);
}

inline const char* GetTrackName(uint32 trackId)
{
    switch (trackId)
    {
        case Tracks::LAVA: return "Lava Track";
        case Tracks::UPPER: return "Upper Track";
        case Tracks::DIAMOND: return "Diamond Track";
        default: return "Unknown Track";
    }
}

inline const char* GetCartName(uint32 cartId)
{
    switch (cartId)
    {
        case 0: return "Lava Cart";
        case 1: return "Upper Cart";
        case 2: return "Diamond Cart";
        default: return "Unknown Cart";
    }
}

// Track waypoints for navigation
inline std::vector<Position> GetLavaTrackWaypoints()
{
    return {
        Position(830.0f, 190.0f, 387.0f, 0),
        Position(780.0f, 190.0f, 387.0f, 0),
        Position(720.0f, 195.0f, 387.0f, 0),
        Position(660.0f, 200.0f, 382.0f, 0),
        Position(580.0f, 200.0f, 380.0f, 0)  // Alliance depot
    };
}

inline std::vector<Position> GetUpperTrackWaypoints()
{
    return {
        Position(746.0f, 305.0f, 402.0f, 0),
        Position(700.0f, 295.0f, 398.0f, 0),
        Position(650.0f, 260.0f, 392.0f, 0),
        Position(600.0f, 230.0f, 385.0f, 0),
        Position(580.0f, 200.0f, 380.0f, 0)  // Alliance depot
    };
}

inline std::vector<Position> GetDiamondTrackWaypoints()
{
    return {
        Position(933.0f, 305.0f, 404.0f, 0),
        Position(980.0f, 280.0f, 400.0f, 0),
        Position(1010.0f, 250.0f, 395.0f, 0),
        Position(1040.0f, 220.0f, 385.0f, 0),
        Position(1050.0f, 200.0f, 380.0f, 0)  // Horde depot
    };
}

// Estimated travel time per track in ms
constexpr uint32 LAVA_TRACK_TIME = 90000;     // 90 seconds
constexpr uint32 UPPER_TRACK_TIME = 75000;    // 75 seconds
constexpr uint32 DIAMOND_TRACK_TIME = 60000;  // 60 seconds

namespace WorldStates
{
    constexpr int32 SCORE_ALLY = 6308;
    constexpr int32 SCORE_HORDE = 6309;
    constexpr int32 CART1_STATE = 6310;
    constexpr int32 CART2_STATE = 6311;
    constexpr int32 CART3_STATE = 6312;
}

} // namespace Playerbot::Coordination::Battleground::SilvershardMines

#endif
