/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_SEETHINGSHOREDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_SEETHINGSHOREDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::SeethingShore
{

constexpr uint32 MAP_ID = 1803;
constexpr char BG_NAME[] = "Seething Shore";
constexpr uint32 MAX_SCORE = 1500;  // Azerite collected
constexpr uint32 MAX_DURATION = 12 * 60 * 1000;  // 12 minutes
constexpr uint8 TEAM_SIZE = 10;
constexpr uint32 MAX_ACTIVE_NODES = 3;
constexpr uint32 TICK_INTERVAL = 1000;
constexpr uint32 CAPTURE_TIME = 6000;  // 6 seconds to capture
constexpr uint32 AZERITE_PER_NODE = 100;

// Potential spawn locations for Azerite nodes
// These are the possible positions where Azerite can spawn dynamically
namespace SpawnZones
{
    constexpr uint32 ZONE_COUNT = 12;
}

// Zone center positions (Azerite spawns randomly in these areas)
constexpr float ZONE_POSITIONS[][3] = {
    { -1863.0f, 2112.0f, 5.0f },   // Zone 1 - North Beach
    { -1938.0f, 2027.0f, 8.0f },   // Zone 2 - Northwest Hill
    { -1783.0f, 2148.0f, 3.0f },   // Zone 3 - Northeast Rocks
    { -1998.0f, 1942.0f, 12.0f },  // Zone 4 - West Cliff
    { -1703.0f, 2083.0f, 6.0f },   // Zone 5 - East Shore
    { -1858.0f, 1987.0f, 10.0f },  // Zone 6 - Center North
    { -1923.0f, 1857.0f, 15.0f },  // Zone 7 - West South
    { -1773.0f, 1918.0f, 8.0f },   // Zone 8 - Center
    { -1643.0f, 1998.0f, 4.0f },   // Zone 9 - East Beach
    { -1888.0f, 1772.0f, 18.0f },  // Zone 10 - Southwest Hill
    { -1728.0f, 1833.0f, 12.0f },  // Zone 11 - South Center
    { -1588.0f, 1913.0f, 6.0f }    // Zone 12 - Southeast
};

inline Position GetZoneCenter(uint32 zoneId)
{
    if (zoneId < SpawnZones::ZONE_COUNT)
        return Position(ZONE_POSITIONS[zoneId][0], ZONE_POSITIONS[zoneId][1], ZONE_POSITIONS[zoneId][2], 0);
    return Position(0, 0, 0, 0);
}

inline const char* GetZoneName(uint32 zoneId)
{
    static const char* names[] = {
        "North Beach", "Northwest Hill", "Northeast Rocks", "West Cliff",
        "East Shore", "Center North", "West South", "Center",
        "East Beach", "Southwest Hill", "South Center", "Southeast"
    };
    return zoneId < SpawnZones::ZONE_COUNT ? names[zoneId] : "Unknown";
}

// Spawn positions
constexpr float ALLIANCE_SPAWN_X = -1573.0f;
constexpr float ALLIANCE_SPAWN_Y = 1758.0f;
constexpr float ALLIANCE_SPAWN_Z = 0.0f;

constexpr float HORDE_SPAWN_X = -2053.0f;
constexpr float HORDE_SPAWN_Y = 2172.0f;
constexpr float HORDE_SPAWN_Z = 0.0f;

inline Position GetSpawnPosition(uint32 faction)
{
    if (faction == 1)  // ALLIANCE
        return Position(ALLIANCE_SPAWN_X, ALLIANCE_SPAWN_Y, ALLIANCE_SPAWN_Z, 0);
    else  // HORDE
        return Position(HORDE_SPAWN_X, HORDE_SPAWN_Y, HORDE_SPAWN_Z, 0);
}

namespace WorldStates
{
    constexpr int32 AZERITE_ALLY = 13231;
    constexpr int32 AZERITE_HORDE = 13232;
}

// Dynamic node tracking structure
struct AzeriteNode
{
    uint32 id;
    Position position;
    bool active;
    uint32 spawnTime;
    uint32 capturedByFaction;
};

} // namespace Playerbot::Coordination::Battleground::SeethingShore

#endif
