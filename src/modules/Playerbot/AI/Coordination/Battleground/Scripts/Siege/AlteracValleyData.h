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

#ifndef PLAYERBOT_AI_COORDINATION_BG_SIEGE_ALTERACVALLEYDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_SIEGE_ALTERACVALLEYDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::AlteracValley
{

// ============================================================================
// MAP INFORMATION
// ============================================================================

constexpr uint32 MAP_ID = 30;
constexpr char BG_NAME[] = "Alterac Valley";
constexpr uint32 MAX_DURATION = 0;  // No time limit (until boss kill or reinforcements)
constexpr uint8 TEAM_SIZE = 40;
constexpr uint32 STARTING_REINFORCEMENTS = 600;
constexpr uint32 REINF_LOSS_PER_DEATH = 1;
constexpr uint32 REINF_LOSS_PER_TOWER = 75;

// ============================================================================
// BOSS DATA
// ============================================================================

// Alliance Boss - Vanndar Stormpike
constexpr uint32 VANNDAR_ENTRY = 11946;
constexpr float VANNDAR_X = -1370.0f;
constexpr float VANNDAR_Y = -219.0f;
constexpr float VANNDAR_Z = 98.0f;
constexpr float VANNDAR_O = 0.0f;

// Horde Boss - Drek'Thar
constexpr uint32 DREKTHAR_ENTRY = 11946;  // Different entry in actual game
constexpr float DREKTHAR_X = -1361.0f;
constexpr float DREKTHAR_Y = -306.0f;
constexpr float DREKTHAR_Z = 89.0f;
constexpr float DREKTHAR_O = 0.0f;

inline Position GetVanndarPosition()
{
    return Position(VANNDAR_X, VANNDAR_Y, VANNDAR_Z, VANNDAR_O);
}

inline Position GetDrekTharPosition()
{
    return Position(DREKTHAR_X, DREKTHAR_Y, DREKTHAR_Z, DREKTHAR_O);
}

// ============================================================================
// TOWER/BUNKER IDS
// ============================================================================

namespace Towers
{
    // Alliance Bunkers (Horde must destroy)
    constexpr uint32 DUN_BALDAR_NORTH = 0;
    constexpr uint32 DUN_BALDAR_SOUTH = 1;
    constexpr uint32 ICEWING_BUNKER = 2;
    constexpr uint32 STONEHEARTH_BUNKER = 3;

    // Horde Towers (Alliance must destroy)
    constexpr uint32 TOWER_POINT = 4;
    constexpr uint32 ICEBLOOD_TOWER = 5;
    constexpr uint32 EAST_FROSTWOLF = 6;
    constexpr uint32 WEST_FROSTWOLF = 7;
}

// Tower positions
constexpr Position TOWER_POSITIONS[] = {
    { -1368.30f, -313.10f, 107.14f, 0.0f },  // Dun Baldar North
    { -1367.40f, -221.20f, 98.43f, 0.0f },   // Dun Baldar South
    { -173.00f, -440.00f, 33.00f, 0.0f },    // Icewing Bunker
    { -155.87f, -87.37f, 79.08f, 0.0f },     // Stonehearth Bunker
    { -570.00f, -262.00f, 75.00f, 0.0f },    // Tower Point
    { -572.00f, -359.00f, 90.00f, 0.0f },    // Iceblood Tower
    { -1302.00f, -315.00f, 113.87f, 0.0f },  // East Frostwolf Tower
    { -1297.00f, -269.00f, 114.14f, 0.0f }   // West Frostwolf Tower
};

inline const char* GetTowerName(uint32 towerId)
{
    switch (towerId)
    {
        case Towers::DUN_BALDAR_NORTH:  return "Dun Baldar North Bunker";
        case Towers::DUN_BALDAR_SOUTH:  return "Dun Baldar South Bunker";
        case Towers::ICEWING_BUNKER:    return "Icewing Bunker";
        case Towers::STONEHEARTH_BUNKER: return "Stonehearth Bunker";
        case Towers::TOWER_POINT:       return "Tower Point";
        case Towers::ICEBLOOD_TOWER:    return "Iceblood Tower";
        case Towers::EAST_FROSTWOLF:    return "East Frostwolf Tower";
        case Towers::WEST_FROSTWOLF:    return "West Frostwolf Tower";
        default: return "Unknown Tower";
    }
}

inline bool IsAllianceTower(uint32 towerId)
{
    return towerId <= Towers::STONEHEARTH_BUNKER;
}

// ============================================================================
// GRAVEYARD IDS
// ============================================================================

namespace Graveyards
{
    constexpr uint32 STORMPIKE_GY = 0;
    constexpr uint32 STORMPIKE_AID_STATION = 1;
    constexpr uint32 STONEHEARTH_GY = 2;
    constexpr uint32 SNOWFALL_GY = 3;          // Neutral, capturable
    constexpr uint32 ICEBLOOD_GY = 4;
    constexpr uint32 FROSTWOLF_GY = 5;
    constexpr uint32 FROSTWOLF_RELIEF_HUT = 6;
}

constexpr Position GRAVEYARD_POSITIONS[] = {
    { -1404.80f, -309.10f, 89.94f, 0.0f },   // Stormpike GY
    { -1361.62f, -220.67f, 98.94f, 0.0f },   // Stormpike Aid Station
    { -172.50f, -136.00f, 79.00f, 0.0f },    // Stonehearth GY
    { -203.00f, -112.00f, 78.00f, 0.0f },    // Snowfall GY
    { -545.00f, -399.00f, 52.00f, 0.0f },    // Iceblood GY
    { -1082.00f, -346.00f, 55.00f, 0.0f },   // Frostwolf GY
    { -1402.40f, -307.70f, 89.44f, 0.0f }    // Frostwolf Relief Hut
};

inline const char* GetGraveyardName(uint32 gyId)
{
    switch (gyId)
    {
        case Graveyards::STORMPIKE_GY:         return "Stormpike Graveyard";
        case Graveyards::STORMPIKE_AID_STATION: return "Stormpike Aid Station";
        case Graveyards::STONEHEARTH_GY:       return "Stonehearth Graveyard";
        case Graveyards::SNOWFALL_GY:          return "Snowfall Graveyard";
        case Graveyards::ICEBLOOD_GY:          return "Iceblood Graveyard";
        case Graveyards::FROSTWOLF_GY:         return "Frostwolf Graveyard";
        case Graveyards::FROSTWOLF_RELIEF_HUT: return "Frostwolf Relief Hut";
        default: return "Unknown Graveyard";
    }
}

// ============================================================================
// SPAWN POSITIONS
// ============================================================================

constexpr Position ALLIANCE_SPAWNS[] = {
    { 873.98f, -491.79f, 96.54f, 3.14f },
    { 869.98f, -496.79f, 96.54f, 3.14f },
    { 878.98f, -486.79f, 96.54f, 3.14f },
    { 864.98f, -501.79f, 96.54f, 3.14f },
    { 883.98f, -481.79f, 96.54f, 3.14f }
};

constexpr Position HORDE_SPAWNS[] = {
    { -1437.00f, -610.00f, 51.16f, 0.0f },
    { -1442.00f, -605.00f, 51.16f, 0.0f },
    { -1432.00f, -615.00f, 51.16f, 0.0f },
    { -1447.00f, -600.00f, 51.16f, 0.0f },
    { -1427.00f, -620.00f, 51.16f, 0.0f }
};

// ============================================================================
// STRATEGIC POSITIONS
// ============================================================================

// Chokepoints
inline std::vector<Position> GetChokepoints()
{
    return {
        // Field of Strife
        { -257.00f, -282.00f, 6.00f, 0.0f },
        // Iceblood Garrison area
        { -520.00f, -350.00f, 52.00f, 0.0f },
        // Stonehearth Outpost area
        { -168.00f, -130.00f, 79.00f, 0.0f },
        // Dun Baldar Bridge
        { 619.00f, -60.00f, 41.00f, 0.0f },
        // Frostwolf Keep entrance
        { -1230.00f, -340.00f, 60.00f, 0.0f }
    };
}

// Mine locations
namespace Mines
{
    constexpr Position IRONDEEP_MINE = { 900.00f, -365.00f, 61.00f, 0.0f };
    constexpr Position COLDTOOTH_MINE = { -1093.00f, -271.00f, 54.00f, 0.0f };
}

// ============================================================================
// WORLD STATES
// ============================================================================

namespace WorldStates
{
    // Reinforcements
    constexpr int32 REINF_ALLY = 3127;
    constexpr int32 REINF_HORDE = 3128;

    // Tower states (example IDs)
    constexpr int32 DB_NORTH_ALLY = 1326;
    constexpr int32 DB_SOUTH_ALLY = 1325;
    constexpr int32 IW_BUNKER_ALLY = 1327;
    constexpr int32 SH_BUNKER_ALLY = 1328;
    constexpr int32 TOWER_POINT_HORDE = 1377;
    constexpr int32 IB_TOWER_HORDE = 1379;
    constexpr int32 EF_TOWER_HORDE = 1381;
    constexpr int32 WF_TOWER_HORDE = 1383;

    // Graveyard states
    constexpr int32 SNOWFALL_NEUTRAL = 1966;
    constexpr int32 SNOWFALL_ALLY = 1341;
    constexpr int32 SNOWFALL_HORDE = 1342;
}

// ============================================================================
// NPCS
// ============================================================================

namespace NPCs
{
    // Captains (killing gives reinf bonus)
    constexpr uint32 BALINDA_STONEHEARTH = 11949;
    constexpr uint32 GALVANGAR = 11947;

    // Wing commanders (summon aerial units)
    constexpr uint32 WING_COMMANDER_SLIDORE = 13437;
    constexpr uint32 WING_COMMANDER_GUSE = 13438;
    constexpr uint32 WING_COMMANDER_VIPORE = 13439;

    // Warmaster NPCs
    constexpr uint32 WARMASTERS_START = 14762;  // Multiple entries
}

// ============================================================================
// STRATEGIC ROUTES
// ============================================================================

// Alliance to Horde boss rush route
inline std::vector<Position> GetAllianceRushRoute()
{
    return {
        { 873.98f, -491.79f, 96.54f, 0.0f },     // Alliance spawn
        { -168.00f, -130.00f, 79.00f, 0.0f },    // Stonehearth
        { -520.00f, -350.00f, 52.00f, 0.0f },    // Iceblood area
        { -1082.00f, -346.00f, 55.00f, 0.0f },   // Frostwolf GY
        { -1230.00f, -340.00f, 60.00f, 0.0f },   // Keep entrance
        { DREKTHAR_X, DREKTHAR_Y, DREKTHAR_Z, 0.0f }
    };
}

// Horde to Alliance boss rush route
inline std::vector<Position> GetHordeRushRoute()
{
    return {
        { -1437.00f, -610.00f, 51.16f, 0.0f },   // Horde spawn
        { -1082.00f, -346.00f, 55.00f, 0.0f },   // Frostwolf GY
        { -520.00f, -350.00f, 52.00f, 0.0f },    // Iceblood area
        { -168.00f, -130.00f, 79.00f, 0.0f },    // Stonehearth
        { 619.00f, -60.00f, 41.00f, 0.0f },      // Dun Baldar Bridge
        { VANNDAR_X, VANNDAR_Y, VANNDAR_Z, 0.0f }
    };
}

} // namespace Playerbot::Coordination::Battleground::AlteracValley

#endif // PLAYERBOT_AI_COORDINATION_BG_SIEGE_ALTERACVALLEYDATA_H
