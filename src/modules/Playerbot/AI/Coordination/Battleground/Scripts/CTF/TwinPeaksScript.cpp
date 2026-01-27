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

#include "TwinPeaksScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

// Register the script
REGISTER_BG_SCRIPT(TwinPeaksScript, 726);  // TwinPeaks::MAP_ID

// ============================================================================
// LIFECYCLE
// ============================================================================

void TwinPeaksScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    CTFScriptBase::OnLoad(coordinator);

    // Cache objective data
    m_cachedObjectives = GetObjectiveData();

    // Register world state mappings
    RegisterScoreWorldState(TwinPeaks::WorldStates::ALLIANCE_FLAG_CAPTURES, true);
    RegisterScoreWorldState(TwinPeaks::WorldStates::HORDE_FLAG_CAPTURES, false);

    RegisterWorldStateMapping(TwinPeaks::WorldStates::ALLIANCE_FLAG_STATE,
        TwinPeaks::ObjectiveIds::ALLIANCE_FLAG, ObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(TwinPeaks::WorldStates::HORDE_FLAG_STATE,
        TwinPeaks::ObjectiveIds::HORDE_FLAG, ObjectiveState::HORDE_CONTROLLED);

    TC_LOG_DEBUG("playerbots.bg.script",
        "TwinPeaksScript: Loaded with {} objectives",
        m_cachedObjectives.size());
}

// ============================================================================
// DATA PROVIDERS
// ============================================================================

std::vector<BGObjectiveData> TwinPeaksScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Alliance Flag
    BGObjectiveData allianceFlag;
    allianceFlag.id = TwinPeaks::ObjectiveIds::ALLIANCE_FLAG;
    allianceFlag.type = ObjectiveType::FLAG;
    allianceFlag.name = "Alliance Flag";
    allianceFlag.x = TwinPeaks::ALLIANCE_FLAG_X;
    allianceFlag.y = TwinPeaks::ALLIANCE_FLAG_Y;
    allianceFlag.z = TwinPeaks::ALLIANCE_FLAG_Z;
    allianceFlag.orientation = TwinPeaks::ALLIANCE_FLAG_O;
    allianceFlag.strategicValue = 10;
    allianceFlag.captureTime = 0;
    allianceFlag.gameObjectEntry = TwinPeaks::GameObjects::ALLIANCE_FLAG;
    allianceFlag.allianceWorldState = TwinPeaks::WorldStates::ALLIANCE_FLAG_STATE;
    allianceFlag.distanceFromAllianceSpawn = 0.0f;
    allianceFlag.distanceFromHordeSpawn = 550.0f;
    objectives.push_back(allianceFlag);

    // Horde Flag
    BGObjectiveData hordeFlag;
    hordeFlag.id = TwinPeaks::ObjectiveIds::HORDE_FLAG;
    hordeFlag.type = ObjectiveType::FLAG;
    hordeFlag.name = "Horde Flag";
    hordeFlag.x = TwinPeaks::HORDE_FLAG_X;
    hordeFlag.y = TwinPeaks::HORDE_FLAG_Y;
    hordeFlag.z = TwinPeaks::HORDE_FLAG_Z;
    hordeFlag.orientation = TwinPeaks::HORDE_FLAG_O;
    hordeFlag.strategicValue = 10;
    hordeFlag.captureTime = 0;
    hordeFlag.gameObjectEntry = TwinPeaks::GameObjects::HORDE_FLAG;
    hordeFlag.hordeWorldState = TwinPeaks::WorldStates::HORDE_FLAG_STATE;
    hordeFlag.distanceFromAllianceSpawn = 550.0f;
    hordeFlag.distanceFromHordeSpawn = 0.0f;
    objectives.push_back(hordeFlag);

    // Connect flags
    objectives[0].connectedObjectives.push_back(TwinPeaks::ObjectiveIds::HORDE_FLAG);
    objectives[1].connectedObjectives.push_back(TwinPeaks::ObjectiveIds::ALLIANCE_FLAG);

    return objectives;
}

std::vector<BGPositionData> TwinPeaksScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    if (faction == ALLIANCE)
    {
        for (const auto& pos : TwinPeaks::ALLIANCE_SPAWNS)
        {
            BGPositionData spawn;
            spawn.name = "Alliance Spawn";
            spawn.x = pos.GetPositionX();
            spawn.y = pos.GetPositionY();
            spawn.z = pos.GetPositionZ();
            spawn.orientation = pos.GetOrientation();
            spawn.faction = ALLIANCE;
            spawn.posType = BGPositionData::PositionType::SPAWN_POINT;
            spawn.importance = 5;
            spawns.push_back(spawn);
        }
    }
    else
    {
        for (const auto& pos : TwinPeaks::HORDE_SPAWNS)
        {
            BGPositionData spawn;
            spawn.name = "Horde Spawn";
            spawn.x = pos.GetPositionX();
            spawn.y = pos.GetPositionY();
            spawn.z = pos.GetPositionZ();
            spawn.orientation = pos.GetOrientation();
            spawn.faction = HORDE;
            spawn.posType = BGPositionData::PositionType::SPAWN_POINT;
            spawn.importance = 5;
            spawns.push_back(spawn);
        }
    }

    return spawns;
}

std::vector<BGPositionData> TwinPeaksScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Flag rooms
    auto allianceDefense = TwinPeaks::GetAllianceFlagRoomDefense();
    for (const auto& pos : allianceDefense)
    {
        BGPositionData p("Alliance Flag Defense", pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::FLAG_ROOM, ALLIANCE, 8);
        positions.push_back(p);
    }

    auto hordeDefense = TwinPeaks::GetHordeFlagRoomDefense();
    for (const auto& pos : hordeDefense)
    {
        BGPositionData p("Horde Flag Defense", pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::FLAG_ROOM, HORDE, 8);
        positions.push_back(p);
    }

    // Chokepoints (including bridge)
    auto chokepoints = TwinPeaks::GetMiddleChokepoints();
    for (const auto& pos : chokepoints)
    {
        BGPositionData p("Middle Chokepoint", pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::CHOKEPOINT, 0, 7);
        positions.push_back(p);
    }

    // River crossings (important for TP)
    auto riverCrossings = TwinPeaks::GetRiverCrossingPoints();
    for (const auto& pos : riverCrossings)
    {
        BGPositionData p("River Crossing", pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::STRATEGIC_POINT, 0, 6);
        p.description = "Water slows movement - choose crossing carefully";
        positions.push_back(p);
    }

    // Buffs
    auto speedBuffs = TwinPeaks::GetSpeedBuffPositions();
    for (size_t i = 0; i < speedBuffs.size(); ++i)
    {
        uint32 faction = (i == 0) ? ALLIANCE : HORDE;
        BGPositionData p("Speed Buff", speedBuffs[i].GetPositionX(),
            speedBuffs[i].GetPositionY(), speedBuffs[i].GetPositionZ(),
            speedBuffs[i].GetOrientation(),
            BGPositionData::PositionType::BUFF_LOCATION, faction, 7);
        positions.push_back(p);
    }

    auto restoreBuffs = TwinPeaks::GetRestoreBuffPositions();
    for (size_t i = 0; i < restoreBuffs.size(); ++i)
    {
        uint32 faction = (i == 0) ? ALLIANCE : HORDE;
        BGPositionData p("Restore Buff", restoreBuffs[i].GetPositionX(),
            restoreBuffs[i].GetPositionY(), restoreBuffs[i].GetPositionZ(),
            restoreBuffs[i].GetOrientation(),
            BGPositionData::PositionType::BUFF_LOCATION, faction, 5);
        positions.push_back(p);
    }

    auto berserkBuffs = TwinPeaks::GetBerserkBuffPositions();
    for (size_t i = 0; i < berserkBuffs.size(); ++i)
    {
        uint32 faction = (i == 0) ? ALLIANCE : HORDE;
        BGPositionData p("Berserk Buff", berserkBuffs[i].GetPositionX(),
            berserkBuffs[i].GetPositionY(), berserkBuffs[i].GetPositionZ(),
            berserkBuffs[i].GetOrientation(),
            BGPositionData::PositionType::BUFF_LOCATION, faction, 5);
        positions.push_back(p);
    }

    return positions;
}

std::vector<BGPositionData> TwinPeaksScript::GetGraveyardPositions(uint32 faction) const
{
    std::vector<BGPositionData> graveyards;

    if (faction == 0 || faction == ALLIANCE)
    {
        Position allyGY = TwinPeaks::GetAllianceGraveyard();
        BGPositionData p("Alliance Graveyard", allyGY.GetPositionX(),
            allyGY.GetPositionY(), allyGY.GetPositionZ(), allyGY.GetOrientation(),
            BGPositionData::PositionType::GRAVEYARD, ALLIANCE, 5);
        graveyards.push_back(p);
    }

    if (faction == 0 || faction == HORDE)
    {
        Position hordeGY = TwinPeaks::GetHordeGraveyard();
        BGPositionData p("Horde Graveyard", hordeGY.GetPositionX(),
            hordeGY.GetPositionY(), hordeGY.GetPositionZ(), hordeGY.GetOrientation(),
            BGPositionData::PositionType::GRAVEYARD, HORDE, 5);
        graveyards.push_back(p);
    }

    return graveyards;
}

std::vector<BGWorldState> TwinPeaksScript::GetInitialWorldStates() const
{
    std::vector<BGWorldState> states;

    // Flag states
    states.push_back(BGWorldState(TwinPeaks::WorldStates::ALLIANCE_FLAG_STATE,
        "Alliance Flag State", BGWorldState::StateType::FLAG_STATE,
        TwinPeaks::WorldStates::FLAG_STATE_AT_BASE));
    states.push_back(BGWorldState(TwinPeaks::WorldStates::HORDE_FLAG_STATE,
        "Horde Flag State", BGWorldState::StateType::FLAG_STATE,
        TwinPeaks::WorldStates::FLAG_STATE_AT_BASE));

    // Scores
    states.push_back(BGWorldState(TwinPeaks::WorldStates::ALLIANCE_FLAG_CAPTURES,
        "Alliance Captures", BGWorldState::StateType::SCORE_ALLIANCE, 0));
    states.push_back(BGWorldState(TwinPeaks::WorldStates::HORDE_FLAG_CAPTURES,
        "Horde Captures", BGWorldState::StateType::SCORE_HORDE, 0));

    // Timer
    states.push_back(BGWorldState(TwinPeaks::WorldStates::TIME_REMAINING,
        "Time Remaining", BGWorldState::StateType::TIMER,
        static_cast<int32>(TwinPeaks::MAX_DURATION / 1000)));

    return states;
}

// ============================================================================
// WORLD STATE
// ============================================================================

bool TwinPeaksScript::InterpretWorldState(int32 stateId, int32 value,
    uint32& outObjectiveId, ObjectiveState& outState) const
{
    if (TryInterpretFromCache(stateId, value, outObjectiveId, outState))
        return true;

    // Alliance flag state
    if (stateId == TwinPeaks::WorldStates::ALLIANCE_FLAG_STATE)
    {
        outObjectiveId = TwinPeaks::ObjectiveIds::ALLIANCE_FLAG;
        switch (value)
        {
            case TwinPeaks::WorldStates::FLAG_STATE_AT_BASE:
                outState = ObjectiveState::ALLIANCE_CONTROLLED;
                return true;
            case TwinPeaks::WorldStates::FLAG_STATE_PICKED_UP:
                outState = ObjectiveState::HORDE_CAPTURING;
                return true;
            case TwinPeaks::WorldStates::FLAG_STATE_DROPPED:
                outState = ObjectiveState::NEUTRAL;
                return true;
        }
    }

    // Horde flag state
    if (stateId == TwinPeaks::WorldStates::HORDE_FLAG_STATE)
    {
        outObjectiveId = TwinPeaks::ObjectiveIds::HORDE_FLAG;
        switch (value)
        {
            case TwinPeaks::WorldStates::FLAG_STATE_AT_BASE:
                outState = ObjectiveState::HORDE_CONTROLLED;
                return true;
            case TwinPeaks::WorldStates::FLAG_STATE_PICKED_UP:
                outState = ObjectiveState::ALLIANCE_CAPTURING;
                return true;
            case TwinPeaks::WorldStates::FLAG_STATE_DROPPED:
                outState = ObjectiveState::NEUTRAL;
                return true;
        }
    }

    return false;
}

void TwinPeaksScript::GetScoreFromWorldStates(const std::map<int32, int32>& states,
    uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = 0;
    hordeScore = 0;

    auto allyIt = states.find(TwinPeaks::WorldStates::ALLIANCE_FLAG_CAPTURES);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(allyIt->second);

    auto hordeIt = states.find(TwinPeaks::WorldStates::HORDE_FLAG_CAPTURES);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(hordeIt->second);
}

// ============================================================================
// UTILITY
// ============================================================================

std::vector<Position> TwinPeaksScript::GetObjectivePath(
    uint32 fromObjective, uint32 toObjective) const
{
    if (fromObjective == TwinPeaks::ObjectiveIds::ALLIANCE_FLAG &&
        toObjective == TwinPeaks::ObjectiveIds::HORDE_FLAG)
    {
        return TwinPeaks::GetAllianceFCKitePath();
    }
    else if (fromObjective == TwinPeaks::ObjectiveIds::HORDE_FLAG &&
             toObjective == TwinPeaks::ObjectiveIds::ALLIANCE_FLAG)
    {
        return TwinPeaks::GetHordeFCKitePath();
    }

    return {};
}

std::vector<Position> TwinPeaksScript::GetRiverCrossings() const
{
    return TwinPeaks::GetRiverCrossingPoints();
}

bool TwinPeaksScript::IsInWater(float x, float /*y*/, float z) const
{
    // Simplified water detection based on known river area
    // River runs roughly from x=1750 to x=1950 in the middle
    if (x > 1750.0f && x < 1950.0f && z < 5.0f)
        return true;

    return false;
}

} // namespace Playerbot::Coordination::Battleground
