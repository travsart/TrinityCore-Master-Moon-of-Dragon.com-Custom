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

#ifndef PLAYERBOT_AI_COORDINATION_BG_CTF_TWINPEAKSDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_CTF_TWINPEAKSDATA_H

#include "Define.h"
#include "Position.h"

namespace Playerbot::Coordination::Battleground::TwinPeaks
{

// ============================================================================
// MAP INFORMATION
// ============================================================================

constexpr uint32 MAP_ID = 726;
constexpr char BG_NAME[] = "Twin Peaks";
constexpr uint32 MAX_SCORE = 3;
constexpr uint32 MAX_DURATION = 25 * 60 * 1000;  // 25 minutes
constexpr uint8 TEAM_SIZE = 10;

// ============================================================================
// FLAG POSITIONS
// ============================================================================

// Alliance flag position (Wildhammer Stronghold)
constexpr float ALLIANCE_FLAG_X = 2118.210f;
constexpr float ALLIANCE_FLAG_Y = 191.621f;
constexpr float ALLIANCE_FLAG_Z = 44.052f;
constexpr float ALLIANCE_FLAG_O = 5.741f;

// Horde flag position (Dragonmaw Fortress)
constexpr float HORDE_FLAG_X = 1578.339f;
constexpr float HORDE_FLAG_Y = 344.063f;
constexpr float HORDE_FLAG_Z = 2.419f;
constexpr float HORDE_FLAG_O = 5.235f;

inline Position GetAllianceFlagPos()
{
    return Position(ALLIANCE_FLAG_X, ALLIANCE_FLAG_Y, ALLIANCE_FLAG_Z, ALLIANCE_FLAG_O);
}

inline Position GetHordeFlagPos()
{
    return Position(HORDE_FLAG_X, HORDE_FLAG_Y, HORDE_FLAG_Z, HORDE_FLAG_O);
}

// ============================================================================
// SPAWN POSITIONS
// ============================================================================

// Alliance spawn points
constexpr Position ALLIANCE_SPAWNS[] = {
    { 2152.63f, 158.58f, 43.32f, 5.235f },
    { 2140.63f, 158.58f, 43.32f, 5.235f },
    { 2146.63f, 152.58f, 43.32f, 5.235f },
    { 2146.63f, 164.58f, 43.32f, 5.235f },
    { 2152.63f, 164.58f, 43.32f, 5.235f }
};

// Horde spawn points
constexpr Position HORDE_SPAWNS[] = {
    { 1542.74f, 364.78f, -6.19f, 2.094f },
    { 1554.74f, 364.78f, -6.19f, 2.094f },
    { 1548.74f, 358.78f, -6.19f, 2.094f },
    { 1548.74f, 370.78f, -6.19f, 2.094f },
    { 1554.74f, 370.78f, -6.19f, 2.094f }
};

// ============================================================================
// GRAVEYARD POSITIONS
// ============================================================================

// Alliance graveyard
constexpr float ALLIANCE_GY_X = 2029.56f;
constexpr float ALLIANCE_GY_Y = 228.41f;
constexpr float ALLIANCE_GY_Z = 56.02f;
constexpr float ALLIANCE_GY_O = 5.235f;

// Horde graveyard
constexpr float HORDE_GY_X = 1632.29f;
constexpr float HORDE_GY_Y = 309.01f;
constexpr float HORDE_GY_Z = -6.72f;
constexpr float HORDE_GY_O = 2.094f;

inline Position GetAllianceGraveyard()
{
    return Position(ALLIANCE_GY_X, ALLIANCE_GY_Y, ALLIANCE_GY_Z, ALLIANCE_GY_O);
}

inline Position GetHordeGraveyard()
{
    return Position(HORDE_GY_X, HORDE_GY_Y, HORDE_GY_Z, HORDE_GY_O);
}

// ============================================================================
// FLAG ROOM DEFENSIVE POSITIONS
// ============================================================================

// Alliance flag room (Wildhammer Stronghold) - defensive positions
inline std::vector<Position> GetAllianceFlagRoomDefense()
{
    return {
        // Main flag room
        { 2118.21f, 191.62f, 44.05f, 5.741f },  // Flag position
        { 2108.21f, 191.62f, 44.05f, 3.141f },  // In front of flag
        { 2113.21f, 181.62f, 44.05f, 2.356f },  // Right side
        { 2113.21f, 201.62f, 44.05f, 3.927f },  // Left side
        { 2098.21f, 191.62f, 44.05f, 3.141f },  // Further back

        // Upper level positions
        { 2118.21f, 185.62f, 51.05f, 5.741f },  // Upper balcony
        { 2108.21f, 185.62f, 51.05f, 3.141f },  // Upper front

        // Entrance defense
        { 2088.21f, 191.62f, 44.05f, 3.141f },  // Main entrance
        { 2088.21f, 181.62f, 44.05f, 2.356f },  // Right entrance
        { 2088.21f, 201.62f, 44.05f, 3.927f }   // Left entrance
    };
}

// Horde flag room (Dragonmaw Fortress) - defensive positions
inline std::vector<Position> GetHordeFlagRoomDefense()
{
    return {
        // Main flag room
        { 1578.34f, 344.06f, 2.42f, 5.235f },   // Flag position
        { 1588.34f, 344.06f, 2.42f, 0.000f },   // In front of flag
        { 1583.34f, 334.06f, 2.42f, 5.498f },   // Right side
        { 1583.34f, 354.06f, 2.42f, 0.785f },   // Left side
        { 1598.34f, 344.06f, 2.42f, 0.000f },   // Further back

        // Upper level positions
        { 1578.34f, 338.06f, 9.42f, 5.235f },   // Upper balcony
        { 1588.34f, 338.06f, 9.42f, 0.000f },   // Upper front

        // Entrance defense
        { 1608.34f, 344.06f, 2.42f, 0.000f },   // Main entrance
        { 1608.34f, 334.06f, 2.42f, 5.498f },   // Right entrance
        { 1608.34f, 354.06f, 2.42f, 0.785f }    // Left entrance
    };
}

// ============================================================================
// MIDDLE MAP POSITIONS
// ============================================================================

// Center chokepoints
inline std::vector<Position> GetMiddleChokepoints()
{
    return {
        // Main center (river crossing)
        { 1848.38f, 268.38f, 8.91f, 0.0f },     // Dead center (bridge)
        { 1848.38f, 238.38f, 8.91f, 0.0f },     // South of bridge
        { 1848.38f, 298.38f, 8.91f, 0.0f },     // North of bridge

        // Alliance side approach
        { 1948.38f, 238.38f, 25.91f, 5.5f },    // Alliance hill
        { 1978.38f, 218.38f, 35.91f, 5.5f },    // Alliance high ground

        // Horde side approach
        { 1748.38f, 298.38f, 0.0f, 2.3f },      // Horde approach
        { 1698.38f, 328.38f, -3.0f, 2.3f },     // Horde low ground

        // Flanking routes
        { 1848.38f, 178.38f, 0.0f, 0.0f },      // South flank
        { 1848.38f, 358.38f, 0.0f, 0.0f }       // North flank
    };
}

// ============================================================================
// BUFF POSITIONS
// ============================================================================

// Speed buff locations
inline std::vector<Position> GetSpeedBuffPositions()
{
    return {
        { 2039.54f, 208.87f, 46.14f, 0.0f },    // Alliance side speed buff
        { 1660.54f, 333.87f, -6.78f, 0.0f }     // Horde side speed buff
    };
}

// Restoration buff locations
inline std::vector<Position> GetRestoreBuffPositions()
{
    return {
        { 1996.24f, 136.05f, 32.92f, 0.0f },    // Alliance side restore
        { 1702.65f, 384.02f, -6.58f, 0.0f }     // Horde side restore
    };
}

// Berserk buff locations
inline std::vector<Position> GetBerserkBuffPositions()
{
    return {
        { 1972.42f, 309.82f, 14.90f, 0.0f },    // Alliance side berserk
        { 1731.24f, 198.65f, 2.45f, 0.0f }      // Horde side berserk
    };
}

// ============================================================================
// WORLD STATES
// ============================================================================

namespace WorldStates
{
    // Flag states
    constexpr int32 ALLIANCE_FLAG_STATE = 5546;
    constexpr int32 HORDE_FLAG_STATE = 5547;

    // Score
    constexpr int32 ALLIANCE_FLAG_CAPTURES = 5548;
    constexpr int32 HORDE_FLAG_CAPTURES = 5549;

    // Timer
    constexpr int32 TIME_REMAINING = 5550;

    // State values
    constexpr int32 FLAG_STATE_AT_BASE = 1;
    constexpr int32 FLAG_STATE_PICKED_UP = 2;
    constexpr int32 FLAG_STATE_DROPPED = 3;
}

// ============================================================================
// GAME OBJECTS
// ============================================================================

namespace GameObjects
{
    // Flags
    constexpr uint32 ALLIANCE_FLAG = 179830;  // Same model as WSG
    constexpr uint32 HORDE_FLAG = 179831;

    // Doors
    constexpr uint32 ALLIANCE_DOOR = 206655;
    constexpr uint32 HORDE_DOOR = 206656;
}

// ============================================================================
// OBJECTIVE IDS
// ============================================================================

namespace ObjectiveIds
{
    constexpr uint32 ALLIANCE_FLAG = 1;
    constexpr uint32 HORDE_FLAG = 2;
}

// ============================================================================
// WATER/TERRAIN FEATURES
// ============================================================================

// River positions (important for LoS and movement)
inline std::vector<Position> GetRiverCrossingPoints()
{
    return {
        { 1848.38f, 268.38f, 0.0f, 0.0f },     // Main bridge
        { 1798.38f, 238.38f, -2.0f, 0.0f },    // South ford
        { 1898.38f, 298.38f, -2.0f, 0.0f }     // North ford
    };
}

// ============================================================================
// FC KITING PATHS
// ============================================================================

// Alliance FC kiting path (carrying Horde flag home)
inline std::vector<Position> GetAllianceFCKitePath()
{
    return {
        { HORDE_FLAG_X, HORDE_FLAG_Y, HORDE_FLAG_Z, HORDE_FLAG_O },
        { 1660.54f, 333.87f, -6.78f, 0.0f },   // Horde speed buff
        { 1748.38f, 298.38f, 0.0f, 0.0f },     // Horde approach
        { 1848.38f, 268.38f, 8.91f, 0.0f },    // Bridge
        { 1948.38f, 238.38f, 25.91f, 5.5f },   // Alliance hill
        { 2039.54f, 208.87f, 46.14f, 0.0f },   // Alliance speed buff
        { ALLIANCE_FLAG_X, ALLIANCE_FLAG_Y, ALLIANCE_FLAG_Z, ALLIANCE_FLAG_O }
    };
}

// Horde FC kiting path (carrying Alliance flag home)
inline std::vector<Position> GetHordeFCKitePath()
{
    return {
        { ALLIANCE_FLAG_X, ALLIANCE_FLAG_Y, ALLIANCE_FLAG_Z, ALLIANCE_FLAG_O },
        { 2039.54f, 208.87f, 46.14f, 0.0f },   // Alliance speed buff
        { 1948.38f, 238.38f, 25.91f, 0.0f },   // Alliance hill
        { 1848.38f, 268.38f, 8.91f, 0.0f },    // Bridge
        { 1748.38f, 298.38f, 0.0f, 0.0f },     // Horde approach
        { 1660.54f, 333.87f, -6.78f, 0.0f },   // Horde speed buff
        { HORDE_FLAG_X, HORDE_FLAG_Y, HORDE_FLAG_Z, HORDE_FLAG_O }
    };
}

} // namespace Playerbot::Coordination::Battleground::TwinPeaks

#endif // PLAYERBOT_AI_COORDINATION_BG_CTF_TWINPEAKSDATA_H
