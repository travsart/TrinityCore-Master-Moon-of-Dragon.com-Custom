/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "AshranScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(AshranScript, 1191);  // Ashran::MAP_ID

void AshranScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    BGScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();

    RegisterScoreWorldState(Ashran::WorldStates::ROAD_PROGRESS_ALLY, true);
    RegisterScoreWorldState(Ashran::WorldStates::ROAD_PROGRESS_HORDE, false);

    m_allianceProgress = 0.0f;
    m_hordeProgress = 0.0f;
    m_activeEvent = UINT32_MAX;
    m_eventTimer = 0;

    // Initialize control states
    for (uint32 i = 0; i < Ashran::ControlPoints::CONTROL_POINT_COUNT; ++i)
    {
        if (i == Ashran::ControlPoints::STORMSHIELD_STRONGHOLD)
            m_controlStates[i] = ObjectiveState::ALLIANCE_CONTROLLED;
        else if (i == Ashran::ControlPoints::WARSPEAR_STRONGHOLD)
            m_controlStates[i] = ObjectiveState::HORDE_CONTROLLED;
        else
            m_controlStates[i] = ObjectiveState::CONTESTED;
    }

    TC_LOG_DEBUG("playerbots.bg.script",
        "AshranScript: Loaded (Road of Glory + side events)");
}

void AshranScript::OnUpdate(uint32 diff)
{
    BGScriptBase::OnUpdate(diff);

    // Track event timers
    if (m_activeEvent < Ashran::Events::EVENT_COUNT)
    {
        if (m_eventTimer > diff)
            m_eventTimer -= diff;
        else
        {
            m_activeEvent = UINT32_MAX;
            m_eventTimer = 0;
        }
    }
}

void AshranScript::OnEvent(const BGScriptEventData& event)
{
    BGScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::OBJECTIVE_CAPTURED:
            if (event.objectiveId < Ashran::ControlPoints::CONTROL_POINT_COUNT)
            {
                m_controlStates[event.objectiveId] = event.newState;
            }
            break;

        case BGScriptEvent::BOSS_KILLED:
            // Major victory event
            break;

        default:
            break;
    }
}

std::vector<BGObjectiveData> AshranScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Road of Glory control points
    for (uint32 i = 0; i < Ashran::ControlPoints::CONTROL_POINT_COUNT; ++i)
    {
        BGObjectiveData point;
        Position pos = Ashran::GetControlPosition(i);
        point.id = i;
        point.type = ObjectiveType::NODE;
        point.name = Ashran::GetControlPointName(i);
        point.x = pos.GetPositionX();
        point.y = pos.GetPositionY();
        point.z = pos.GetPositionZ();
        point.strategicValue = (i == Ashran::ControlPoints::CROSSROADS) ? 9 : 7;
        objectives.push_back(point);
    }

    // Faction leaders as objectives
    BGObjectiveData tremblade;
    tremblade.id = 100;
    tremblade.type = ObjectiveType::BOSS;
    tremblade.name = "Grand Marshal Tremblade";
    tremblade.x = Ashran::TREMBLADE_X;
    tremblade.y = Ashran::TREMBLADE_Y;
    tremblade.z = Ashran::TREMBLADE_Z;
    tremblade.strategicValue = 10;
    objectives.push_back(tremblade);

    BGObjectiveData volrath;
    volrath.id = 101;
    volrath.type = ObjectiveType::BOSS;
    volrath.name = "High Warlord Volrath";
    volrath.x = Ashran::VOLRATH_X;
    volrath.y = Ashran::VOLRATH_Y;
    volrath.z = Ashran::VOLRATH_Z;
    volrath.strategicValue = 10;
    objectives.push_back(volrath);

    return objectives;
}

std::vector<BGPositionData> AshranScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    Position pos = Ashran::GetSpawnPosition(faction);
    spawns.push_back(BGPositionData(faction == ALLIANCE ? "Stormshield" : "Warspear",
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
        BGPositionData::PositionType::SPAWN_POINT, faction, 5));
    return spawns;
}

std::vector<BGPositionData> AshranScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Control points
    for (uint32 i = 0; i < Ashran::ControlPoints::CONTROL_POINT_COUNT; ++i)
    {
        Position pos = Ashran::GetControlPosition(i);
        positions.push_back(BGPositionData(Ashran::GetControlPointName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::STRATEGIC_POINT, 0, 8));
    }

    // Road of Glory waypoints
    auto waypoints = Ashran::GetRoadOfGloryWaypoints();
    for (size_t i = 0; i < waypoints.size(); ++i)
    {
        positions.push_back(BGPositionData("Road Waypoint",
            waypoints[i].GetPositionX(), waypoints[i].GetPositionY(), waypoints[i].GetPositionZ(), 0,
            BGPositionData::PositionType::CHOKEPOINT, 0, 6));
    }

    // Faction leaders
    positions.push_back(BGPositionData("Grand Marshal Tremblade",
        Ashran::TREMBLADE_X, Ashran::TREMBLADE_Y, Ashran::TREMBLADE_Z, 0,
        BGPositionData::PositionType::STRATEGIC_POINT, ALLIANCE, 10));

    positions.push_back(BGPositionData("High Warlord Volrath",
        Ashran::VOLRATH_X, Ashran::VOLRATH_Y, Ashran::VOLRATH_Z, 0,
        BGPositionData::PositionType::STRATEGIC_POINT, HORDE, 10));

    return positions;
}

std::vector<BGPositionData> AshranScript::GetGraveyardPositions(uint32 faction) const
{
    return GetSpawnPositions(faction);
}

std::vector<BGWorldState> AshranScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(Ashran::WorldStates::ROAD_PROGRESS_ALLY, "Alliance Progress", BGWorldState::StateType::CUSTOM, 0),
        BGWorldState(Ashran::WorldStates::ROAD_PROGRESS_HORDE, "Horde Progress", BGWorldState::StateType::CUSTOM, 0)
    };
}

bool AshranScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    if (stateId == Ashran::WorldStates::ACTIVE_EVENT)
    {
        // Value indicates which event is active
        return false;  // Not an objective
    }

    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void AshranScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    // In Ashran, "score" is effectively road progress
    allianceScore = hordeScore = 50;  // Start at neutral

    auto allyIt = states.find(Ashran::WorldStates::ROAD_PROGRESS_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(Ashran::WorldStates::ROAD_PROGRESS_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

RoleDistribution AshranScript::GetRecommendedRoles(const StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution dist;

    // Ashran needs a mix of road pushers and event participants
    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            dist.roleCounts[BGRole::NODE_ATTACKER] = 45;     // Road pushers
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 25;      // Boss kill team
            dist.roleCounts[BGRole::ROAMER] = 20;            // Event participants
            dist.roleCounts[BGRole::NODE_DEFENDER] = 10;
            dist.reasoning = "Aggressive road push";
            break;

        case BGStrategy::DEFENSIVE:
            dist.roleCounts[BGRole::NODE_DEFENDER] = 40;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 25;
            dist.roleCounts[BGRole::ROAMER] = 20;
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 15;
            dist.reasoning = "Defensive hold";
            break;

        case BGStrategy::ALL_IN:
            dist.roleCounts[BGRole::NODE_ATTACKER] = 35;
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 40;
            dist.roleCounts[BGRole::ROAMER] = 15;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 10;
            dist.reasoning = "All-in boss assault";
            break;

        default:  // BALANCED
            dist.roleCounts[BGRole::NODE_ATTACKER] = 35;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 25;
            dist.roleCounts[BGRole::ROAMER] = 25;
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 15;
            dist.reasoning = "Balanced control";
            break;
    }

    return dist;
}

void AshranScript::AdjustStrategy(StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*controlledCount*/, uint32 /*totalObjectives*/, uint32 /*timeRemaining*/) const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    float ourProgress = GetRoadProgress(faction);
    float theirProgress = GetRoadProgress(faction == ALLIANCE ? HORDE : ALLIANCE);

    // Check if we can attack enemy boss
    if (IsEnemyBaseVulnerable(faction))
    {
        decision.strategy = BGStrategy::ALL_IN;
        decision.reasoning = "Enemy base vulnerable - PUSH TO BOSS!";
        decision.offenseAllocation = 80;
        decision.defenseAllocation = 20;
    }
    else if (ourProgress > 0.7f)
    {
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.reasoning = "Deep in enemy territory - maintain pressure";
        decision.offenseAllocation = 70;
        decision.defenseAllocation = 30;
    }
    else if (theirProgress > 0.6f)
    {
        decision.strategy = BGStrategy::DEFENSIVE;
        decision.reasoning = "Enemy pushing - defend the road!";
        decision.defenseAllocation = 65;
        decision.offenseAllocation = 35;
    }
    else
    {
        decision.strategy = BGStrategy::BALANCED;
        decision.reasoning = "Control the Road of Glory";
        decision.offenseAllocation = 50;
        decision.defenseAllocation = 50;
    }

    // Side events provide valuable buffs
    if (IsEventActive() && ShouldParticipateInEvent(m_activeEvent))
    {
        decision.reasoning += " + participate in " + std::string(Ashran::GetEventName(m_activeEvent));
    }
}

float AshranScript::GetRoadProgress(uint32 faction) const
{
    return (faction == ALLIANCE) ? m_allianceProgress : m_hordeProgress;
}

Position AshranScript::GetEventPosition(uint32 eventId) const
{
    // Event positions are scattered around the zone
    // This is simplified - actual positions would be more complex
    switch (eventId)
    {
        case Ashran::Events::RING_OF_CONQUEST:
            return Position(4750.0f, -4300.0f, 25.0f, 0);
        case Ashran::Events::SEAT_OF_OMEN:
            return Position(4400.0f, -4250.0f, 35.0f, 0);
        case Ashran::Events::EMPOWERED_ORE:
            return Position(4650.0f, -3950.0f, 30.0f, 0);
        default:
            return Ashran::GetControlPosition(Ashran::ControlPoints::CROSSROADS);
    }
}

bool AshranScript::ShouldParticipateInEvent(uint32 eventId) const
{
    // Prioritize certain events based on rewards
    switch (eventId)
    {
        case Ashran::Events::SEAT_OF_OMEN:
        case Ashran::Events::RING_OF_CONQUEST:
            return true;  // High value events
        case Ashran::Events::EMPOWERED_ORE:
        case Ashran::Events::ANCIENT_ARTIFACT:
            return true;  // Good buffs
        default:
            return false;  // Focus on road unless idle
    }
}

bool AshranScript::IsEnemyBaseVulnerable(uint32 attackingFaction) const
{
    // Enemy base is vulnerable when we control the crossroads and are pushing deep
    auto crossroadsState = m_controlStates.find(Ashran::ControlPoints::CROSSROADS);
    if (crossroadsState == m_controlStates.end())
        return false;

    bool controlCrossroads =
        (attackingFaction == ALLIANCE && crossroadsState->second == ObjectiveState::ALLIANCE_CONTROLLED) ||
        (attackingFaction == HORDE && crossroadsState->second == ObjectiveState::HORDE_CONTROLLED);

    float progress = GetRoadProgress(attackingFaction);

    return controlCrossroads && progress > 0.8f;
}

} // namespace Playerbot::Coordination::Battleground
