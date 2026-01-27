/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_SIEGE_STRANDOFTHEANCIENTSDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_SIEGE_STRANDOFTHEANCIENTSDATA_H

#include "Define.h"
#include "Position.h"
#include <array>

namespace Playerbot::Coordination::Battleground::StrandOfTheAncients
{

constexpr uint32 MAP_ID = 607;
constexpr char BG_NAME[] = "Strand of the Ancients";
constexpr uint32 MAX_DURATION = 10 * 60 * 1000;  // 10 minutes per round
constexpr uint8 TEAM_SIZE = 15;
constexpr uint32 ROUND_COUNT = 2;

// Gates (from beach to relic)
namespace Gates
{
    constexpr uint32 GREEN_JADE = 0;      // Left outer
    constexpr uint32 BLUE_SAPPHIRE = 1;   // Right outer
    constexpr uint32 RED_SUN = 2;         // Left middle
    constexpr uint32 PURPLE_AMETHYST = 3; // Right middle
    constexpr uint32 YELLOW_MOON = 4;     // Inner
    constexpr uint32 ANCIENT_GATE = 5;    // Final gate
    constexpr uint32 GATE_COUNT = 6;
}

// Graveyards
namespace Graveyards
{
    constexpr uint32 BEACH_GY = 0;           // Attackers spawn
    constexpr uint32 WEST_GY = 1;            // After outer gates
    constexpr uint32 EAST_GY = 2;            // After outer gates
    constexpr uint32 SOUTH_GY = 3;           // After middle gates
    constexpr uint32 DEFENDER_START_GY = 4;  // Defender spawn
    constexpr uint32 GRAVEYARD_COUNT = 5;
}

// Vehicle spawns
namespace VehicleSpawns
{
    constexpr uint32 BEACH_WEST = 0;
    constexpr uint32 BEACH_EAST = 1;
    constexpr uint32 INNER_WEST = 2;
    constexpr uint32 INNER_EAST = 3;
    constexpr uint32 SPAWN_COUNT = 4;
}

// Gate positions
constexpr Position GATE_POSITIONS[] = {
    Position(1411.0f, -108.0f, 61.0f, 0),   // Green Jade
    Position(1411.0f, 53.0f, 61.0f, 0),     // Blue Sapphire
    Position(1232.0f, -182.0f, 66.0f, 0),   // Red Sun
    Position(1232.0f, 133.0f, 66.0f, 0),    // Purple Amethyst
    Position(1095.0f, -24.0f, 67.0f, 0),    // Yellow Moon
    Position(878.0f, -24.0f, 79.0f, 0)      // Ancient Gate
};

// Graveyard positions
constexpr Position GRAVEYARD_POSITIONS[] = {
    Position(1597.0f, -106.0f, 8.0f, 0),    // Beach
    Position(1338.0f, -298.0f, 32.0f, 0),   // West
    Position(1338.0f, 245.0f, 32.0f, 0),    // East
    Position(1119.0f, -24.0f, 67.0f, 0),    // South
    Position(830.0f, -24.0f, 93.0f, 0)      // Defender start
};

// Demolisher spawn positions
constexpr Position DEMOLISHER_SPAWNS[] = {
    Position(1611.0f, -117.0f, 8.0f, 0),    // Beach West
    Position(1611.0f, 61.0f, 8.0f, 0),      // Beach East
    Position(1353.0f, -317.0f, 35.0f, 0),   // Inner West
    Position(1353.0f, 260.0f, 35.0f, 0)     // Inner East
};

// Relic position
constexpr float RELIC_X = 836.0f;
constexpr float RELIC_Y = -24.0f;
constexpr float RELIC_Z = 94.0f;

// Titan Relic entry
constexpr uint32 TITAN_RELIC_ENTRY = 192834;

// Demolisher entry
constexpr uint32 DEMOLISHER_ENTRY = 28781;

// Turrets (cannon) entry
constexpr uint32 TURRET_ENTRY = 27894;

inline Position GetGatePosition(uint32 gateId)
{
    if (gateId < Gates::GATE_COUNT)
        return GATE_POSITIONS[gateId];
    return Position(0, 0, 0, 0);
}

inline Position GetGraveyardPosition(uint32 gyId)
{
    if (gyId < Graveyards::GRAVEYARD_COUNT)
        return GRAVEYARD_POSITIONS[gyId];
    return Position(0, 0, 0, 0);
}

inline Position GetDemolisherSpawn(uint32 spawnId)
{
    if (spawnId < VehicleSpawns::SPAWN_COUNT)
        return DEMOLISHER_SPAWNS[spawnId];
    return Position(0, 0, 0, 0);
}

inline const char* GetGateName(uint32 gateId)
{
    switch (gateId)
    {
        case Gates::GREEN_JADE: return "Gate of the Green Jade";
        case Gates::BLUE_SAPPHIRE: return "Gate of the Blue Sapphire";
        case Gates::RED_SUN: return "Gate of the Red Sun";
        case Gates::PURPLE_AMETHYST: return "Gate of the Purple Amethyst";
        case Gates::YELLOW_MOON: return "Gate of the Yellow Moon";
        case Gates::ANCIENT_GATE: return "Chamber of Ancient Relics";
        default: return "Unknown Gate";
    }
}

inline const char* GetGraveyardName(uint32 gyId)
{
    switch (gyId)
    {
        case Graveyards::BEACH_GY: return "Beach Graveyard";
        case Graveyards::WEST_GY: return "West Graveyard";
        case Graveyards::EAST_GY: return "East Graveyard";
        case Graveyards::SOUTH_GY: return "South Graveyard";
        case Graveyards::DEFENDER_START_GY: return "Defender Graveyard";
        default: return "Unknown Graveyard";
    }
}

// Gate dependencies - which gate must be destroyed first
inline std::vector<uint32> GetGateDependencies(uint32 gateId)
{
    switch (gateId)
    {
        case Gates::RED_SUN:
            return { Gates::GREEN_JADE };
        case Gates::PURPLE_AMETHYST:
            return { Gates::BLUE_SAPPHIRE };
        case Gates::YELLOW_MOON:
            return { Gates::RED_SUN, Gates::PURPLE_AMETHYST };  // Either one
        case Gates::ANCIENT_GATE:
            return { Gates::YELLOW_MOON };
        default:
            return {};  // Outer gates have no dependencies
    }
}

inline Position GetRelicPosition()
{
    return Position(RELIC_X, RELIC_Y, RELIC_Z, 0);
}

// Determine optimal path based on gate health
enum class AttackPath
{
    LEFT,   // Green Jade -> Red Sun -> Yellow Moon
    RIGHT,  // Blue Sapphire -> Purple Amethyst -> Yellow Moon
    SPLIT   // Both paths
};

namespace WorldStates
{
    constexpr int32 ROUND_TIME = 3564;
    constexpr int32 ATTACKER_TEAM = 3565;
    constexpr int32 GATE_DESTROYED_COUNT = 3566;
}

} // namespace Playerbot::Coordination::Battleground::StrandOfTheAncients

#endif
