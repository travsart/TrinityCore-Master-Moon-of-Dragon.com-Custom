/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_SIEGE_ISLEOFCONQUESTDATA_H
#define PLAYERBOT_AI_COORDINATION_BG_SIEGE_ISLEOFCONQUESTDATA_H

#include "Define.h"
#include "Position.h"
#include <array>
#include <vector>

namespace Playerbot::Coordination::Battleground::IsleOfConquest
{

constexpr uint32 MAP_ID = 628;
constexpr char BG_NAME[] = "Isle of Conquest";
constexpr uint32 MAX_DURATION = 30 * 60 * 1000;  // 30 minutes
constexpr uint8 TEAM_SIZE = 40;
constexpr uint32 STARTING_REINFORCEMENTS = 300;
constexpr uint32 REINF_LOSS_PER_KILL = 1;
constexpr uint32 REINF_LOSS_PER_TOWER = 50;

// Objectives
namespace Nodes
{
    constexpr uint32 REFINERY = 0;      // Oil (vehicles)
    constexpr uint32 QUARRY = 1;        // Stone (siege)
    constexpr uint32 DOCKS = 2;         // Glaive Throwers/Catapults
    constexpr uint32 HANGAR = 3;        // Gunship
    constexpr uint32 WORKSHOP = 4;      // Siege Engines
    constexpr uint32 NODE_COUNT = 5;
}

// Gates
namespace Gates
{
    constexpr uint32 ALLIANCE_FRONT = 0;
    constexpr uint32 ALLIANCE_WEST = 1;
    constexpr uint32 ALLIANCE_EAST = 2;
    constexpr uint32 HORDE_FRONT = 3;
    constexpr uint32 HORDE_WEST = 4;
    constexpr uint32 HORDE_EAST = 5;
    constexpr uint32 GATE_COUNT = 6;
}

// Bosses
constexpr uint32 HIGH_COMMANDER_HALFORD = 34924;  // Alliance
constexpr uint32 OVERLORD_AGMAR = 34922;          // Horde

// Node positions
constexpr Position NODE_POSITIONS[] = {
    Position(1246.0f, -400.0f, 23.0f, 0),   // Refinery
    Position(340.0f, -395.0f, 26.0f, 0),    // Quarry
    Position(736.0f, -1132.0f, 14.0f, 0),   // Docks
    Position(808.0f, -123.0f, 132.0f, 0),   // Hangar
    Position(773.0f, -685.0f, 9.0f, 0)      // Workshop
};

// Gate positions
constexpr Position GATE_POSITIONS[] = {
    Position(352.0f, -762.0f, 48.0f, 0),    // Alliance Front
    Position(228.0f, -820.0f, 48.0f, 0),    // Alliance West
    Position(478.0f, -820.0f, 48.0f, 0),    // Alliance East
    Position(1141.0f, -762.0f, 48.0f, 0),   // Horde Front
    Position(1016.0f, -820.0f, 48.0f, 0),   // Horde West
    Position(1266.0f, -820.0f, 48.0f, 0)    // Horde East
};

// Boss positions
constexpr float HALFORD_X = 224.0f;
constexpr float HALFORD_Y = -836.0f;
constexpr float HALFORD_Z = 60.0f;

constexpr float AGMAR_X = 1296.0f;
constexpr float AGMAR_Y = -765.0f;
constexpr float AGMAR_Z = 70.0f;

// Spawn positions
constexpr Position ALLIANCE_SPAWNS[] = {
    Position(303.0f, -857.0f, 48.0f, 0),
    Position(345.0f, -857.0f, 48.0f, 0),
    Position(387.0f, -857.0f, 48.0f, 0)
};

constexpr Position HORDE_SPAWNS[] = {
    Position(1147.0f, -745.0f, 48.0f, 0),
    Position(1189.0f, -745.0f, 48.0f, 0),
    Position(1231.0f, -745.0f, 48.0f, 0)
};

inline Position GetNodePosition(uint32 nodeId)
{
    if (nodeId < Nodes::NODE_COUNT)
        return NODE_POSITIONS[nodeId];
    return Position(0, 0, 0, 0);
}

inline Position GetGatePosition(uint32 gateId)
{
    if (gateId < Gates::GATE_COUNT)
        return GATE_POSITIONS[gateId];
    return Position(0, 0, 0, 0);
}

inline const char* GetNodeName(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::REFINERY: return "Refinery";
        case Nodes::QUARRY: return "Quarry";
        case Nodes::DOCKS: return "Docks";
        case Nodes::HANGAR: return "Hangar";
        case Nodes::WORKSHOP: return "Workshop";
        default: return "Unknown";
    }
}

inline const char* GetGateName(uint32 gateId)
{
    switch (gateId)
    {
        case Gates::ALLIANCE_FRONT: return "Alliance Front Gate";
        case Gates::ALLIANCE_WEST: return "Alliance West Gate";
        case Gates::ALLIANCE_EAST: return "Alliance East Gate";
        case Gates::HORDE_FRONT: return "Horde Front Gate";
        case Gates::HORDE_WEST: return "Horde West Gate";
        case Gates::HORDE_EAST: return "Horde East Gate";
        default: return "Unknown Gate";
    }
}

inline bool IsAllianceGate(uint32 gateId)
{
    return gateId <= Gates::ALLIANCE_EAST;
}

inline Position GetHalfordPosition()
{
    return Position(HALFORD_X, HALFORD_Y, HALFORD_Z, 0);
}

inline Position GetAgmarPosition()
{
    return Position(AGMAR_X, AGMAR_Y, AGMAR_Z, 0);
}

// Vehicle types available from nodes
namespace Vehicles
{
    constexpr uint32 GLAIVE_THROWER = 34802;
    constexpr uint32 CATAPULT = 34793;
    constexpr uint32 DEMOLISHER = 34775;
    constexpr uint32 SIEGE_ENGINE = 34776;
    constexpr uint32 GUNSHIP = 0;  // Not a vehicle you enter, but transport
}

inline std::vector<uint32> GetVehiclesFromNode(uint32 nodeId)
{
    switch (nodeId)
    {
        case Nodes::DOCKS: return { Vehicles::GLAIVE_THROWER, Vehicles::CATAPULT };
        case Nodes::WORKSHOP: return { Vehicles::SIEGE_ENGINE, Vehicles::DEMOLISHER };
        default: return {};
    }
}

namespace WorldStates
{
    constexpr int32 REINF_ALLY = 4221;
    constexpr int32 REINF_HORDE = 4222;
    constexpr int32 REFINERY_ALLY = 4287;
    constexpr int32 REFINERY_HORDE = 4288;
    constexpr int32 QUARRY_ALLY = 4289;
    constexpr int32 QUARRY_HORDE = 4290;
    constexpr int32 DOCKS_ALLY = 4291;
    constexpr int32 DOCKS_HORDE = 4292;
    constexpr int32 HANGAR_ALLY = 4293;
    constexpr int32 HANGAR_HORDE = 4294;
    constexpr int32 WORKSHOP_ALLY = 4295;
    constexpr int32 WORKSHOP_HORDE = 4296;
}

} // namespace Playerbot::Coordination::Battleground::IsleOfConquest

#endif
