/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "StrandOfTheAncientsScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(StrandOfTheAncientsScript, 607);  // StrandOfTheAncients::MAP_ID

void StrandOfTheAncientsScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    SiegeScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();

    m_isAttacker = false;  // Will be set by world state
    m_currentRound = 1;
    m_round1Time = 0;
    m_destroyedGates.clear();
    m_capturedGraveyards.clear();

    TC_LOG_DEBUG("playerbots.bg.script",
        "StrandOfTheAncientsScript: Loaded (6 gates, 5 graveyards, round-based)");
}

void StrandOfTheAncientsScript::OnEvent(const BGScriptEventData& event)
{
    SiegeScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::GATE_DESTROYED:
            if (event.objectiveId < StrandOfTheAncients::Gates::GATE_COUNT)
                m_destroyedGates.insert(event.objectiveId);
            break;

        case BGScriptEvent::OBJECTIVE_CAPTURED:
            // Graveyard captured
            if (event.objectiveId >= 50 && event.objectiveId < 50 + StrandOfTheAncients::Graveyards::GRAVEYARD_COUNT)
                m_capturedGraveyards.insert(event.objectiveId - 50);
            break;

        case BGScriptEvent::ROUND_STARTED:
            m_currentRound = static_cast<uint32>(event.stateValue);
            m_isAttacker = !m_isAttacker;  // Swap roles
            m_destroyedGates.clear();
            m_capturedGraveyards.clear();
            break;

        case BGScriptEvent::ROUND_ENDED:
            if (m_currentRound == 1)
                m_round1Time = static_cast<uint32>(event.stateValue);  // Store round 1 time
            break;

        default:
            break;
    }
}

std::vector<BGObjectiveData> StrandOfTheAncientsScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Gates
    auto gates = GetGateData();
    objectives.insert(objectives.end(), gates.begin(), gates.end());

    // Graveyards
    auto graveyards = GetGraveyardData();
    objectives.insert(objectives.end(), graveyards.begin(), graveyards.end());

    // Titan Relic as final objective
    BGObjectiveData relic;
    relic.id = 100;
    relic.type = ObjectiveType::RELIC;
    relic.name = "Titan Relic";
    relic.x = StrandOfTheAncients::RELIC_X;
    relic.y = StrandOfTheAncients::RELIC_Y;
    relic.z = StrandOfTheAncients::RELIC_Z;
    relic.strategicValue = 10;
    objectives.push_back(relic);

    return objectives;
}

std::vector<BGObjectiveData> StrandOfTheAncientsScript::GetTowerData() const
{
    // SOTA has turrets, not towers
    return {};
}

std::vector<BGObjectiveData> StrandOfTheAncientsScript::GetGraveyardData() const
{
    std::vector<BGObjectiveData> graveyards;

    for (uint32 i = 0; i < StrandOfTheAncients::Graveyards::GRAVEYARD_COUNT; ++i)
    {
        BGObjectiveData gy;
        Position pos = StrandOfTheAncients::GetGraveyardPosition(i);
        gy.id = 50 + i;
        gy.type = ObjectiveType::GRAVEYARD;
        gy.name = StrandOfTheAncients::GetGraveyardName(i);
        gy.x = pos.GetPositionX();
        gy.y = pos.GetPositionY();
        gy.z = pos.GetPositionZ();
        gy.strategicValue = 6;
        gy.captureTime = 30000;  // 30 seconds
        graveyards.push_back(gy);
    }

    return graveyards;
}

std::vector<BGObjectiveData> StrandOfTheAncientsScript::GetGateData() const
{
    std::vector<BGObjectiveData> gates;

    for (uint32 i = 0; i < StrandOfTheAncients::Gates::GATE_COUNT; ++i)
    {
        BGObjectiveData gate;
        Position pos = StrandOfTheAncients::GetGatePosition(i);
        gate.id = i;
        gate.type = ObjectiveType::GATE;
        gate.name = StrandOfTheAncients::GetGateName(i);
        gate.x = pos.GetPositionX();
        gate.y = pos.GetPositionY();
        gate.z = pos.GetPositionZ();

        // Final gate is most important
        gate.strategicValue = (i == StrandOfTheAncients::Gates::ANCIENT_GATE) ? 10 : 8;
        gates.push_back(gate);
    }

    return gates;
}

std::vector<BGPositionData> StrandOfTheAncientsScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    // Attackers spawn at beach, defenders at the relic
    bool isAttacking = (m_isAttacker && faction == m_coordinator->GetFaction()) ||
                       (!m_isAttacker && faction != m_coordinator->GetFaction());

    if (isAttacking)
    {
        Position beachPos = StrandOfTheAncients::GetGraveyardPosition(StrandOfTheAncients::Graveyards::BEACH_GY);
        spawns.push_back(BGPositionData("Beach Spawn",
            beachPos.GetPositionX(), beachPos.GetPositionY(), beachPos.GetPositionZ(), 0,
            BGPositionData::PositionType::SPAWN_POINT, faction, 5));
    }
    else
    {
        Position defenderPos = StrandOfTheAncients::GetGraveyardPosition(StrandOfTheAncients::Graveyards::DEFENDER_START_GY);
        spawns.push_back(BGPositionData("Defender Spawn",
            defenderPos.GetPositionX(), defenderPos.GetPositionY(), defenderPos.GetPositionZ(), 0,
            BGPositionData::PositionType::SPAWN_POINT, faction, 5));
    }

    return spawns;
}

std::vector<BGPositionData> StrandOfTheAncientsScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Gate positions
    for (uint32 i = 0; i < StrandOfTheAncients::Gates::GATE_COUNT; ++i)
    {
        Position pos = StrandOfTheAncients::GetGatePosition(i);
        positions.push_back(BGPositionData(StrandOfTheAncients::GetGateName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::DEFENSIVE_POSITION, 0, 8));
    }

    // Demolisher spawns
    for (uint32 i = 0; i < StrandOfTheAncients::VehicleSpawns::SPAWN_COUNT; ++i)
    {
        Position pos = StrandOfTheAncients::GetDemolisherSpawn(i);
        positions.push_back(BGPositionData("Demolisher Spawn",
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::VEHICLE_SPAWN, 0, 7));
    }

    // Relic
    positions.push_back(BGPositionData("Titan Relic",
        StrandOfTheAncients::RELIC_X, StrandOfTheAncients::RELIC_Y, StrandOfTheAncients::RELIC_Z, 0,
        BGPositionData::PositionType::STRATEGIC_POINT, 0, 10));

    return positions;
}

std::vector<BGPositionData> StrandOfTheAncientsScript::GetGraveyardPositions(uint32 /*faction*/) const
{
    std::vector<BGPositionData> graveyards;

    for (uint32 i = 0; i < StrandOfTheAncients::Graveyards::GRAVEYARD_COUNT; ++i)
    {
        Position pos = StrandOfTheAncients::GetGraveyardPosition(i);
        graveyards.push_back(BGPositionData(StrandOfTheAncients::GetGraveyardName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::GRAVEYARD, 0, 6));
    }

    return graveyards;
}

std::vector<BGVehicleData> StrandOfTheAncientsScript::GetVehicleData() const
{
    std::vector<BGVehicleData> vehicles;
    vehicles.push_back(BGVehicleData(StrandOfTheAncients::DEMOLISHER_ENTRY, "Demolisher", 50000, 2, true));
    return vehicles;
}

std::vector<BGWorldState> StrandOfTheAncientsScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(StrandOfTheAncients::WorldStates::ROUND_TIME, "Round Time", BGWorldState::StateType::TIMER, StrandOfTheAncients::MAX_DURATION)
    };
}

bool StrandOfTheAncientsScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    if (stateId == StrandOfTheAncients::WorldStates::ATTACKER_TEAM)
    {
        // Value indicates which team is attacking
        return false;  // Not an objective state
    }

    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void StrandOfTheAncientsScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    // In SOTA, "score" is effectively time remaining / gates destroyed
    allianceScore = hordeScore = 0;

    // The team that captures the relic faster wins
    auto timeIt = states.find(StrandOfTheAncients::WorldStates::ROUND_TIME);
    if (timeIt != states.end())
    {
        // Score based on time and progress
        uint32 gatesDestroyed = GetDestroyedGateCount();
        if (m_isAttacker)
            allianceScore = gatesDestroyed * 100;
        else
            hordeScore = gatesDestroyed * 100;
    }
}

RoleDistribution StrandOfTheAncientsScript::GetRecommendedRoles(const StrategicDecision& /*decision*/, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution dist;

    if (m_isAttacker)
    {
        // Attackers need vehicles and infantry support
        dist.roleCounts[BGRole::VEHICLE_DRIVER] = 25;
        dist.roleCounts[BGRole::VEHICLE_GUNNER] = 15;
        dist.roleCounts[BGRole::NODE_ATTACKER] = 35;  // Infantry support
        dist.roleCounts[BGRole::HEALER_OFFENSE] = 15;
        dist.roleCounts[BGRole::ROAMER] = 10;
        dist.reasoning = "Attack formation";
    }
    else
    {
        // Defenders focus on killing vehicles and players
        dist.roleCounts[BGRole::NODE_DEFENDER] = 45;
        dist.roleCounts[BGRole::TURRET_OPERATOR] = 20;
        dist.roleCounts[BGRole::HEALER_DEFENSE] = 20;
        dist.roleCounts[BGRole::ROAMER] = 15;
        dist.reasoning = "Defense formation";
    }

    return dist;
}

void StrandOfTheAncientsScript::AdjustStrategy(StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*controlledCount*/, uint32 /*totalObjectives*/, uint32 timeRemaining) const
{
    if (m_isAttacker)
    {
        // Attacker strategy
        uint32 gatesRemaining = StrandOfTheAncients::Gates::GATE_COUNT - GetDestroyedGateCount();

        if (gatesRemaining <= 2 && timeRemaining > 120000)
        {
            decision.strategy = BGStrategy::AGGRESSIVE;
            decision.reasoning = "Close to victory - push hard";
            decision.offenseAllocation = 85;
            decision.defenseAllocation = 15;
        }
        else if (timeRemaining < 60000 && gatesRemaining > 2)
        {
            decision.strategy = BGStrategy::ALL_IN;
            decision.reasoning = "Running out of time - all in!";
            decision.offenseAllocation = 95;
            decision.defenseAllocation = 5;
        }
        else
        {
            decision.strategy = BGStrategy::BALANCED;
            decision.reasoning = "Steady push through gates";
            decision.offenseAllocation = 70;
            decision.defenseAllocation = 30;
        }

        // Path recommendation
        auto path = GetRecommendedPath();
        switch (path)
        {
            case StrandOfTheAncients::AttackPath::LEFT:
                decision.reasoning += " (focus left path)";
                break;
            case StrandOfTheAncients::AttackPath::RIGHT:
                decision.reasoning += " (focus right path)";
                break;
            case StrandOfTheAncients::AttackPath::SPLIT:
                decision.reasoning += " (split attack)";
                break;
        }
    }
    else
    {
        // Defender strategy
        if (m_currentRound == 2 && m_round1Time > 0)
        {
            // In round 2, we need to hold longer than attackers did
            if (timeRemaining > m_round1Time)
            {
                decision.strategy = BGStrategy::DEFENSIVE;
                decision.reasoning = "Stall for victory";
                decision.defenseAllocation = 85;
                decision.offenseAllocation = 15;
            }
            else
            {
                decision.strategy = BGStrategy::TURTLE;
                decision.reasoning = "Must hold - turtle up!";
                decision.defenseAllocation = 95;
                decision.offenseAllocation = 5;
            }
        }
        else
        {
            decision.strategy = BGStrategy::DEFENSIVE;
            decision.reasoning = "Defend gates and kill demos";
            decision.defenseAllocation = 80;
            decision.offenseAllocation = 20;
        }
    }
}

StrandOfTheAncients::AttackPath StrandOfTheAncientsScript::GetRecommendedPath() const
{
    bool leftOuterDown = IsGateDestroyed(StrandOfTheAncients::Gates::GREEN_JADE);
    bool rightOuterDown = IsGateDestroyed(StrandOfTheAncients::Gates::BLUE_SAPPHIRE);

    if (leftOuterDown && !rightOuterDown)
        return StrandOfTheAncients::AttackPath::LEFT;
    else if (rightOuterDown && !leftOuterDown)
        return StrandOfTheAncients::AttackPath::RIGHT;
    else
        return StrandOfTheAncients::AttackPath::SPLIT;
}

std::vector<uint32> StrandOfTheAncientsScript::GetNextTargetGates() const
{
    std::vector<uint32> targets;

    for (uint32 i = 0; i < StrandOfTheAncients::Gates::GATE_COUNT; ++i)
    {
        if (!IsGateDestroyed(i) && CanAttackGate(i))
            targets.push_back(i);
    }

    return targets;
}

bool StrandOfTheAncientsScript::CanAttackGate(uint32 gateId) const
{
    auto deps = StrandOfTheAncients::GetGateDependencies(gateId);

    if (deps.empty())
        return true;  // No dependencies

    // For Yellow Moon, we need EITHER left OR right path (not both)
    if (gateId == StrandOfTheAncients::Gates::YELLOW_MOON)
    {
        return IsGateDestroyed(StrandOfTheAncients::Gates::RED_SUN) ||
               IsGateDestroyed(StrandOfTheAncients::Gates::PURPLE_AMETHYST);
    }

    // For other gates, check if dependency is destroyed
    for (uint32 dep : deps)
    {
        if (!IsGateDestroyed(dep))
            return false;
    }

    return true;
}

} // namespace Playerbot::Coordination::Battleground
