/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_EPIC_ASHRANDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_EPIC_ASHRANDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::Ashran
{

constexpr uint32 MAP_ID = 1191;
constexpr char BG_NAME[] = "Ashran";
constexpr uint32 MAX_DURATION = 0;  // No time limit
constexpr uint8 MIN_TEAM_SIZE = 10;
constexpr uint8 MAX_TEAM_SIZE = 75;

// Main objectives - Road of Glory control points
namespace ControlPoints
{
    constexpr uint32 STORMSHIELD_STRONGHOLD = 0;  // Alliance base
    constexpr uint32 CROSSROADS = 1;               // Center control
    constexpr uint32 WARSPEAR_STRONGHOLD = 2;      // Horde base
    constexpr uint32 CONTROL_POINT_COUNT = 3;
}

// Side events
namespace Events
{
    constexpr uint32 RACE_FOR_SUPREMACY = 0;       // Racing event
    constexpr uint32 RING_OF_CONQUEST = 1;         // Arena battles
    constexpr uint32 SEAT_OF_OMEN = 2;             // Boss event
    constexpr uint32 EMPOWERED_ORE = 3;            // Mining event
    constexpr uint32 ANCIENT_ARTIFACT = 4;         // Artifact hunt
    constexpr uint32 STADIUM_RACING = 5;           // Motorcycle racing
    constexpr uint32 OGRE_FIRES = 6;               // Fire lighting
    constexpr uint32 BRUTE_ASSAULT = 7;            // NPC assault
    constexpr uint32 EVENT_COUNT = 8;
}

// Bosses
constexpr uint32 HIGH_WARLORD_VOLRATH = 82882;     // Alliance target
constexpr uint32 GRAND_MARSHAL_TREMBLADE = 82877;  // Horde target

// Faction leader positions (in enemy base)
constexpr float VOLRATH_X = 4001.0f;
constexpr float VOLRATH_Y = -4088.0f;
constexpr float VOLRATH_Z = 52.0f;

constexpr float TREMBLADE_X = 5178.0f;
constexpr float TREMBLADE_Y = -4117.0f;
constexpr float TREMBLADE_Z = 1.0f;

// Control point positions
constexpr Position CONTROL_POSITIONS[] = {
    Position(4982.0f, -4171.0f, 15.0f, 0),   // Stormshield Stronghold
    Position(4585.0f, -4117.0f, 32.0f, 0),   // Crossroads
    Position(4188.0f, -4063.0f, 50.0f, 0)    // Warspear Stronghold
};

// Road of Glory waypoints (from Alliance to Horde base)
inline std::vector<Position> GetRoadOfGloryWaypoints()
{
    return {
        Position(5178.0f, -4117.0f, 1.0f, 0),    // Alliance base
        Position(5050.0f, -4130.0f, 5.0f, 0),
        Position(4900.0f, -4150.0f, 12.0f, 0),
        Position(4750.0f, -4140.0f, 20.0f, 0),
        Position(4585.0f, -4117.0f, 32.0f, 0),   // Crossroads
        Position(4420.0f, -4094.0f, 38.0f, 0),
        Position(4280.0f, -4075.0f, 45.0f, 0),
        Position(4140.0f, -4060.0f, 50.0f, 0),
        Position(4001.0f, -4088.0f, 52.0f, 0)    // Horde base
    };
}

// Spawn positions
constexpr float ALLIANCE_SPAWN_X = 5200.0f;
constexpr float ALLIANCE_SPAWN_Y = -4100.0f;
constexpr float ALLIANCE_SPAWN_Z = 1.0f;

constexpr float HORDE_SPAWN_X = 3970.0f;
constexpr float HORDE_SPAWN_Y = -4100.0f;
constexpr float HORDE_SPAWN_Z = 55.0f;

inline Position GetSpawnPosition(uint32 faction)
{
    if (faction == 1)  // ALLIANCE
        return Position(ALLIANCE_SPAWN_X, ALLIANCE_SPAWN_Y, ALLIANCE_SPAWN_Z, 0);
    else
        return Position(HORDE_SPAWN_X, HORDE_SPAWN_Y, HORDE_SPAWN_Z, 0);
}

inline Position GetControlPosition(uint32 pointId)
{
    if (pointId < ControlPoints::CONTROL_POINT_COUNT)
        return CONTROL_POSITIONS[pointId];
    return Position(0, 0, 0, 0);
}

inline const char* GetControlPointName(uint32 pointId)
{
    switch (pointId)
    {
        case ControlPoints::STORMSHIELD_STRONGHOLD: return "Stormshield Stronghold";
        case ControlPoints::CROSSROADS: return "Crossroads";
        case ControlPoints::WARSPEAR_STRONGHOLD: return "Warspear Stronghold";
        default: return "Unknown";
    }
}

inline const char* GetEventName(uint32 eventId)
{
    switch (eventId)
    {
        case Events::RACE_FOR_SUPREMACY: return "Race for Supremacy";
        case Events::RING_OF_CONQUEST: return "Ring of Conquest";
        case Events::SEAT_OF_OMEN: return "Seat of Omen";
        case Events::EMPOWERED_ORE: return "Empowered Ore";
        case Events::ANCIENT_ARTIFACT: return "Ancient Artifact";
        case Events::STADIUM_RACING: return "Stadium Racing";
        case Events::OGRE_FIRES: return "Ogre Fires";
        case Events::BRUTE_ASSAULT: return "Brute Assault";
        default: return "Unknown Event";
    }
}

inline Position GetTrembladePosition()
{
    return Position(TREMBLADE_X, TREMBLADE_Y, TREMBLADE_Z, 0);
}

inline Position GetVolrathPosition()
{
    return Position(VOLRATH_X, VOLRATH_Y, VOLRATH_Z, 0);
}

// Artifact buffs (items that grant temp bonuses)
namespace Artifacts
{
    constexpr uint32 BOOK_OF_MEDIVH = 114205;
    constexpr uint32 NECROTIC_ORB = 114203;
    constexpr uint32 RACING_FLAG = 114207;
}

// Conquest/honor points per activity
constexpr uint32 BOSS_KILL_HONOR = 500;
constexpr uint32 EVENT_WIN_HONOR = 150;
constexpr uint32 KILL_HONOR = 15;

namespace WorldStates
{
    constexpr int32 ROAD_PROGRESS_ALLY = 8671;
    constexpr int32 ROAD_PROGRESS_HORDE = 8672;
    constexpr int32 ACTIVE_EVENT = 8673;
}

} // namespace Playerbot::Coordination::Battleground::Ashran

#endif
