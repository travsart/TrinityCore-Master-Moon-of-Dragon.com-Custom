/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ObjectGuid.h"
#include <cstdint>
#include <vector>
#include <string>

namespace Playerbot {

// ============================================================================
// BATTLEGROUND ENUMS
// ============================================================================

enum class BGState : uint8
{
    IDLE = 0,
    QUEUED = 1,
    PREPARATION = 2,
    ACTIVE = 3,
    OVERTIME = 4,
    VICTORY = 5,
    DEFEAT = 6
};

enum class BGType : uint32
{
    WARSONG_GULCH = 489,
    ARATHI_BASIN = 529,
    ALTERAC_VALLEY = 30,
    EYE_OF_THE_STORM = 566,
    STRAND_OF_THE_ANCIENTS = 607,
    ISLE_OF_CONQUEST = 628,
    TWIN_PEAKS = 726,
    BATTLE_FOR_GILNEAS = 761,
    SILVERSHARD_MINES = 727,
    TEMPLE_OF_KOTMOGU = 998,
    DEEPWIND_GORGE = 1105,
    SEETHING_SHORE = 1803,
    ASHRAN = 1191
};

enum class BGRole : uint8
{
    UNASSIGNED = 0,
    FLAG_CARRIER = 1,
    FLAG_ESCORT = 2,
    FLAG_HUNTER = 3,      // Hunt enemy FC
    NODE_ATTACKER = 4,
    NODE_DEFENDER = 5,
    ROAMER = 6,
    HEALER_OFFENSE = 7,
    HEALER_DEFENSE = 8,
    CART_PUSHER = 9,      // Silvershard Mines
    ORB_CARRIER = 10,     // Temple of Kotmogu
    GRAVEYARD_ASSAULT = 11,
    RESOURCE_GATHERER = 12  // Deepwind Gorge
};

enum class ObjectiveType : uint8
{
    FLAG = 1,
    NODE = 2,
    TOWER = 3,
    GRAVEYARD = 4,
    GATE = 5,
    CART = 6,
    ORB = 7,
    BOSS = 8,
    MINE = 9,
    WORKSHOP = 10,
    RELIC = 11
};

enum class ObjectiveState : uint8
{
    NEUTRAL = 0,
    ALLIANCE_CONTROLLED = 1,
    HORDE_CONTROLLED = 2,
    ALLIANCE_CONTESTED = 3,
    HORDE_CONTESTED = 4,
    ALLIANCE_CAPTURING = 5,
    HORDE_CAPTURING = 6,
    DESTROYED = 7
};

enum class BGPriority : uint8
{
    IGNORE = 0,
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    CRITICAL = 4
};

// ============================================================================
// BATTLEGROUND STRUCTURES
// ============================================================================

struct BGObjective
{
    uint32 id;
    ObjectiveType type;
    ObjectiveState state;
    ::std::string name;
    float x, y, z;

    // Capture progress
    float captureProgress;  // 0.0 - 1.0
    uint32 captureTime;     // Time when capture will complete
    uint32 contestedSince;  // When contesting started

    // Assignment
    ::std::vector<ObjectGuid> assignedDefenders;
    ::std::vector<ObjectGuid> assignedAttackers;

    // Strategic value
    uint8 strategicValue;   // 1-10
    bool isContested;
    BGPriority currentPriority;

    // Location info
    float nearbyEnemyCount;
    float nearbyAllyCount;

    BGObjective()
        : id(0), type(ObjectiveType::NODE), state(ObjectiveState::NEUTRAL),
          x(0), y(0), z(0),
          captureProgress(0), captureTime(0), contestedSince(0),
          strategicValue(5), isContested(false), currentPriority(BGPriority::NORMAL),
          nearbyEnemyCount(0), nearbyAllyCount(0) {}
};

struct BGScoreInfo
{
    uint32 allianceScore;
    uint32 hordeScore;
    uint32 maxScore;
    uint32 allianceResources;
    uint32 hordeResources;
    uint32 timeRemaining;

    // Flag-specific
    uint32 allianceFlagCaptures;
    uint32 hordeFlagCaptures;

    // Resource income rate
    float allianceResourceRate;
    float hordeResourceRate;

    BGScoreInfo()
        : allianceScore(0), hordeScore(0), maxScore(0),
          allianceResources(0), hordeResources(0), timeRemaining(0),
          allianceFlagCaptures(0), hordeFlagCaptures(0),
          allianceResourceRate(0), hordeResourceRate(0) {}
};

struct FlagInfo
{
    ObjectGuid carrierGuid;
    bool isPickedUp;
    bool isAtBase;
    bool isDropped;
    float x, y, z;
    uint8 stackCount;       // For debuffs (focused assault, etc.)
    uint32 pickupTime;
    uint32 dropTime;

    FlagInfo()
        : carrierGuid(), isPickedUp(false), isAtBase(true), isDropped(false),
          x(0), y(0), z(0), stackCount(0), pickupTime(0), dropTime(0) {}
};

struct BGPlayer
{
    ObjectGuid guid;
    uint32 classId;
    BGRole role;
    BGPriority threat;

    // Status
    float healthPercent;
    float manaPercent;
    bool isAlive;
    bool isInCombat;
    bool hasFlag;

    // Position
    float x, y, z;
    uint32 nearestObjectiveId;
    float distanceToObjective;

    // Performance
    uint32 kills;
    uint32 deaths;
    uint32 honorableKills;
    uint32 objectivesAssisted;

    BGPlayer()
        : guid(), classId(0), role(BGRole::UNASSIGNED), threat(BGPriority::NORMAL),
          healthPercent(100.0f), manaPercent(100.0f), isAlive(true),
          isInCombat(false), hasFlag(false),
          x(0), y(0), z(0), nearestObjectiveId(0), distanceToObjective(0),
          kills(0), deaths(0), honorableKills(0), objectivesAssisted(0) {}
};

struct RoleAssignment
{
    ObjectGuid player;
    BGRole role;
    uint32 objectiveId;     // If assigned to specific objective
    uint32 assignTime;
    float efficiency;       // 0-1, how well performing role

    RoleAssignment()
        : player(), role(BGRole::UNASSIGNED), objectiveId(0),
          assignTime(0), efficiency(0.5f) {}
};

struct BGMatchStats
{
    uint32 matchStartTime;
    uint32 matchDuration;
    BGType bgType;

    // Score tracking
    uint32 peakScoreAdvantage;
    uint32 peakScoreDisadvantage;

    // Objective tracking
    uint32 objectivesCaptured;
    uint32 objectivesLost;
    uint32 objectivesDefended;

    // Combat tracking
    uint32 totalKills;
    uint32 totalDeaths;
    uint32 flagCaptures;
    uint32 flagReturns;

    BGMatchStats()
        : matchStartTime(0), matchDuration(0), bgType(BGType::WARSONG_GULCH),
          peakScoreAdvantage(0), peakScoreDisadvantage(0),
          objectivesCaptured(0), objectivesLost(0), objectivesDefended(0),
          totalKills(0), totalDeaths(0), flagCaptures(0), flagReturns(0) {}
};

// ============================================================================
// STRING CONVERSION UTILITIES
// ============================================================================

inline const char* BGStateToString(BGState state)
{
    switch (state)
    {
        case BGState::IDLE: return "IDLE";
        case BGState::QUEUED: return "QUEUED";
        case BGState::PREPARATION: return "PREPARATION";
        case BGState::ACTIVE: return "ACTIVE";
        case BGState::OVERTIME: return "OVERTIME";
        case BGState::VICTORY: return "VICTORY";
        case BGState::DEFEAT: return "DEFEAT";
        default: return "UNKNOWN";
    }
}

inline const char* BGRoleToString(BGRole role)
{
    switch (role)
    {
        case BGRole::UNASSIGNED: return "UNASSIGNED";
        case BGRole::FLAG_CARRIER: return "FLAG_CARRIER";
        case BGRole::FLAG_ESCORT: return "FLAG_ESCORT";
        case BGRole::FLAG_HUNTER: return "FLAG_HUNTER";
        case BGRole::NODE_ATTACKER: return "NODE_ATTACKER";
        case BGRole::NODE_DEFENDER: return "NODE_DEFENDER";
        case BGRole::ROAMER: return "ROAMER";
        case BGRole::HEALER_OFFENSE: return "HEALER_OFFENSE";
        case BGRole::HEALER_DEFENSE: return "HEALER_DEFENSE";
        case BGRole::CART_PUSHER: return "CART_PUSHER";
        case BGRole::ORB_CARRIER: return "ORB_CARRIER";
        case BGRole::GRAVEYARD_ASSAULT: return "GRAVEYARD_ASSAULT";
        case BGRole::RESOURCE_GATHERER: return "RESOURCE_GATHERER";
        default: return "UNKNOWN";
    }
}

inline const char* BGTypeToString(BGType type)
{
    switch (type)
    {
        case BGType::WARSONG_GULCH: return "WARSONG_GULCH";
        case BGType::ARATHI_BASIN: return "ARATHI_BASIN";
        case BGType::ALTERAC_VALLEY: return "ALTERAC_VALLEY";
        case BGType::EYE_OF_THE_STORM: return "EYE_OF_THE_STORM";
        case BGType::STRAND_OF_THE_ANCIENTS: return "STRAND_OF_ANCIENTS";
        case BGType::ISLE_OF_CONQUEST: return "ISLE_OF_CONQUEST";
        case BGType::TWIN_PEAKS: return "TWIN_PEAKS";
        case BGType::BATTLE_FOR_GILNEAS: return "BATTLE_FOR_GILNEAS";
        case BGType::SILVERSHARD_MINES: return "SILVERSHARD_MINES";
        case BGType::TEMPLE_OF_KOTMOGU: return "TEMPLE_OF_KOTMOGU";
        case BGType::DEEPWIND_GORGE: return "DEEPWIND_GORGE";
        case BGType::SEETHING_SHORE: return "SEETHING_SHORE";
        case BGType::ASHRAN: return "ASHRAN";
        default: return "UNKNOWN";
    }
}

inline const char* ObjectiveTypeToString(ObjectiveType type)
{
    switch (type)
    {
        case ObjectiveType::FLAG: return "FLAG";
        case ObjectiveType::NODE: return "NODE";
        case ObjectiveType::TOWER: return "TOWER";
        case ObjectiveType::GRAVEYARD: return "GRAVEYARD";
        case ObjectiveType::GATE: return "GATE";
        case ObjectiveType::CART: return "CART";
        case ObjectiveType::ORB: return "ORB";
        case ObjectiveType::BOSS: return "BOSS";
        case ObjectiveType::MINE: return "MINE";
        case ObjectiveType::WORKSHOP: return "WORKSHOP";
        case ObjectiveType::RELIC: return "RELIC";
        default: return "UNKNOWN";
    }
}

inline const char* ObjectiveStateToString(ObjectiveState state)
{
    switch (state)
    {
        case ObjectiveState::NEUTRAL: return "NEUTRAL";
        case ObjectiveState::ALLIANCE_CONTROLLED: return "ALLIANCE_CONTROLLED";
        case ObjectiveState::HORDE_CONTROLLED: return "HORDE_CONTROLLED";
        case ObjectiveState::ALLIANCE_CONTESTED: return "ALLIANCE_CONTESTED";
        case ObjectiveState::HORDE_CONTESTED: return "HORDE_CONTESTED";
        case ObjectiveState::ALLIANCE_CAPTURING: return "ALLIANCE_CAPTURING";
        case ObjectiveState::HORDE_CAPTURING: return "HORDE_CAPTURING";
        case ObjectiveState::DESTROYED: return "DESTROYED";
        default: return "UNKNOWN";
    }
}

} // namespace Playerbot
