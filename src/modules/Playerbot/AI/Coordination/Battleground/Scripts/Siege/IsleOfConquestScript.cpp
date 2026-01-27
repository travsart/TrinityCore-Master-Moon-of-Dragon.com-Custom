/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "IsleOfConquestScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(IsleOfConquestScript, 628);  // IsleOfConquest::MAP_ID

void IsleOfConquestScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    SiegeScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();

    RegisterScoreWorldState(IsleOfConquest::WorldStates::REINF_ALLY, true);
    RegisterScoreWorldState(IsleOfConquest::WorldStates::REINF_HORDE, false);

    // Initialize node states as neutral
    for (uint32 i = 0; i < IsleOfConquest::Nodes::NODE_COUNT; ++i)
        m_nodeStates[i] = ObjectiveState::NEUTRAL;

    m_destroyedGates.clear();

    TC_LOG_DEBUG("playerbots.bg.script",
        "IsleOfConquestScript: Loaded (5 nodes, 6 gates, 2 bosses, vehicles)");
}

void IsleOfConquestScript::OnEvent(const BGScriptEventData& event)
{
    SiegeScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::GATE_DESTROYED:
            if (event.objectiveId < IsleOfConquest::Gates::GATE_COUNT)
                m_destroyedGates.insert(event.objectiveId);
            break;

        case BGScriptEvent::OBJECTIVE_CAPTURED:
            if (event.objectiveId < IsleOfConquest::Nodes::NODE_COUNT)
            {
                m_nodeStates[event.objectiveId] = event.newState;
            }
            break;

        default:
            break;
    }
}

std::vector<BGObjectiveData> IsleOfConquestScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Nodes
    auto nodes = GetNodeData();
    objectives.insert(objectives.end(), nodes.begin(), nodes.end());

    // Gates
    auto gates = GetGateData();
    objectives.insert(objectives.end(), gates.begin(), gates.end());

    // Bosses
    BGObjectiveData halford;
    halford.id = 100;
    halford.type = ObjectiveType::BOSS;
    halford.name = "High Commander Halford";
    halford.x = IsleOfConquest::HALFORD_X;
    halford.y = IsleOfConquest::HALFORD_Y;
    halford.z = IsleOfConquest::HALFORD_Z;
    halford.strategicValue = 10;
    objectives.push_back(halford);

    BGObjectiveData agmar;
    agmar.id = 101;
    agmar.type = ObjectiveType::BOSS;
    agmar.name = "Overlord Agmar";
    agmar.x = IsleOfConquest::AGMAR_X;
    agmar.y = IsleOfConquest::AGMAR_Y;
    agmar.z = IsleOfConquest::AGMAR_Z;
    agmar.strategicValue = 10;
    objectives.push_back(agmar);

    return objectives;
}

std::vector<BGObjectiveData> IsleOfConquestScript::GetNodeData() const
{
    std::vector<BGObjectiveData> nodes;

    for (uint32 i = 0; i < IsleOfConquest::Nodes::NODE_COUNT; ++i)
    {
        BGObjectiveData node;
        Position pos = IsleOfConquest::GetNodePosition(i);
        node.id = i;
        node.type = ObjectiveType::NODE;
        node.name = IsleOfConquest::GetNodeName(i);
        node.x = pos.GetPositionX();
        node.y = pos.GetPositionY();
        node.z = pos.GetPositionZ();

        // Strategic value based on node importance
        switch (i)
        {
            case IsleOfConquest::Nodes::WORKSHOP:
                node.strategicValue = 9;  // Siege engines are key
                break;
            case IsleOfConquest::Nodes::HANGAR:
                node.strategicValue = 8;  // Gunship is powerful
                break;
            case IsleOfConquest::Nodes::DOCKS:
                node.strategicValue = 7;  // Vehicles
                break;
            default:
                node.strategicValue = 6;  // Resources
                break;
        }

        node.captureTime = 60000;  // 1 minute to cap
        nodes.push_back(node);
    }

    return nodes;
}

std::vector<BGObjectiveData> IsleOfConquestScript::GetTowerData() const
{
    // IOC doesn't have towers like AV
    return {};
}

std::vector<BGObjectiveData> IsleOfConquestScript::GetGraveyardData() const
{
    std::vector<BGObjectiveData> graveyards;
    // Graveyards are at spawn points in IOC
    return graveyards;
}

std::vector<BGObjectiveData> IsleOfConquestScript::GetGateData() const
{
    std::vector<BGObjectiveData> gates;

    for (uint32 i = 0; i < IsleOfConquest::Gates::GATE_COUNT; ++i)
    {
        BGObjectiveData gate;
        Position pos = IsleOfConquest::GetGatePosition(i);
        gate.id = 50 + i;  // Offset from nodes
        gate.type = ObjectiveType::GATE;
        gate.name = IsleOfConquest::GetGateName(i);
        gate.x = pos.GetPositionX();
        gate.y = pos.GetPositionY();
        gate.z = pos.GetPositionZ();
        gate.strategicValue = 8;
        gates.push_back(gate);
    }

    return gates;
}

std::vector<BGPositionData> IsleOfConquestScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    if (faction == ALLIANCE)
    {
        for (const auto& pos : IsleOfConquest::ALLIANCE_SPAWNS)
        {
            spawns.push_back(BGPositionData("Alliance Spawn",
                pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
                BGPositionData::PositionType::SPAWN_POINT, ALLIANCE, 5));
        }
    }
    else
    {
        for (const auto& pos : IsleOfConquest::HORDE_SPAWNS)
        {
            spawns.push_back(BGPositionData("Horde Spawn",
                pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
                BGPositionData::PositionType::SPAWN_POINT, HORDE, 5));
        }
    }

    return spawns;
}

std::vector<BGPositionData> IsleOfConquestScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Nodes
    for (uint32 i = 0; i < IsleOfConquest::Nodes::NODE_COUNT; ++i)
    {
        Position pos = IsleOfConquest::GetNodePosition(i);
        positions.push_back(BGPositionData(IsleOfConquest::GetNodeName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::STRATEGIC_POINT, 0, 7));
    }

    // Gates
    for (uint32 i = 0; i < IsleOfConquest::Gates::GATE_COUNT; ++i)
    {
        Position pos = IsleOfConquest::GetGatePosition(i);
        uint32 faction = IsleOfConquest::IsAllianceGate(i) ? ALLIANCE : HORDE;
        positions.push_back(BGPositionData(IsleOfConquest::GetGateName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::DEFENSIVE_POSITION, faction, 8));
    }

    // Boss positions
    positions.push_back(BGPositionData("High Commander Halford",
        IsleOfConquest::HALFORD_X, IsleOfConquest::HALFORD_Y, IsleOfConquest::HALFORD_Z, 0,
        BGPositionData::PositionType::STRATEGIC_POINT, ALLIANCE, 10));

    positions.push_back(BGPositionData("Overlord Agmar",
        IsleOfConquest::AGMAR_X, IsleOfConquest::AGMAR_Y, IsleOfConquest::AGMAR_Z, 0,
        BGPositionData::PositionType::STRATEGIC_POINT, HORDE, 10));

    return positions;
}

std::vector<BGPositionData> IsleOfConquestScript::GetGraveyardPositions(uint32 faction) const
{
    return GetSpawnPositions(faction);
}

std::vector<BGVehicleData> IsleOfConquestScript::GetVehicleData() const
{
    std::vector<BGVehicleData> vehicles;

    vehicles.push_back(BGVehicleData(IsleOfConquest::Vehicles::GLAIVE_THROWER, "Glaive Thrower", 100000, 1, true));
    vehicles.push_back(BGVehicleData(IsleOfConquest::Vehicles::CATAPULT, "Catapult", 75000, 1, true));
    vehicles.push_back(BGVehicleData(IsleOfConquest::Vehicles::DEMOLISHER, "Demolisher", 150000, 2, true));
    vehicles.push_back(BGVehicleData(IsleOfConquest::Vehicles::SIEGE_ENGINE, "Siege Engine", 500000, 4, true));

    return vehicles;
}

std::vector<BGWorldState> IsleOfConquestScript::GetInitialWorldStates() const
{
    std::vector<BGWorldState> states;

    states.push_back(BGWorldState(IsleOfConquest::WorldStates::REINF_ALLY,
        "Alliance Reinforcements", BGWorldState::StateType::REINFORCEMENTS,
        IsleOfConquest::STARTING_REINFORCEMENTS));
    states.push_back(BGWorldState(IsleOfConquest::WorldStates::REINF_HORDE,
        "Horde Reinforcements", BGWorldState::StateType::REINFORCEMENTS,
        IsleOfConquest::STARTING_REINFORCEMENTS));

    return states;
}

bool IsleOfConquestScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    // Node states
    if (stateId == IsleOfConquest::WorldStates::REFINERY_ALLY && value)
    {
        outObjectiveId = IsleOfConquest::Nodes::REFINERY;
        outState = ObjectiveState::ALLIANCE_CONTROLLED;
        return true;
    }
    if (stateId == IsleOfConquest::WorldStates::REFINERY_HORDE && value)
    {
        outObjectiveId = IsleOfConquest::Nodes::REFINERY;
        outState = ObjectiveState::HORDE_CONTROLLED;
        return true;
    }
    // ... (similar for other nodes)

    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void IsleOfConquestScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = IsleOfConquest::STARTING_REINFORCEMENTS;
    hordeScore = IsleOfConquest::STARTING_REINFORCEMENTS;

    auto allyIt = states.find(IsleOfConquest::WorldStates::REINF_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(IsleOfConquest::WorldStates::REINF_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

RoleDistribution IsleOfConquestScript::GetRecommendedRoles(const StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution dist;

    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            dist.roleCounts[BGRole::VEHICLE_DRIVER] = 15;
            dist.roleCounts[BGRole::VEHICLE_GUNNER] = 10;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 30;
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 25;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 10;
            dist.roleCounts[BGRole::ROAMER] = 10;
            dist.reasoning = "Aggressive vehicle assault";
            break;

        case BGStrategy::DEFENSIVE:
            dist.roleCounts[BGRole::NODE_DEFENDER] = 35;
            dist.roleCounts[BGRole::VEHICLE_DRIVER] = 10;
            dist.roleCounts[BGRole::VEHICLE_GUNNER] = 10;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 20;
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 15;
            dist.roleCounts[BGRole::ROAMER] = 10;
            dist.reasoning = "Defensive node hold";
            break;

        default:  // BALANCED
            dist.roleCounts[BGRole::VEHICLE_DRIVER] = 12;
            dist.roleCounts[BGRole::VEHICLE_GUNNER] = 8;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 25;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 25;
            dist.roleCounts[BGRole::BOSS_ASSAULT] = 20;
            dist.roleCounts[BGRole::ROAMER] = 10;
            dist.reasoning = "Balanced siege";
            break;
    }

    return dist;
}

void IsleOfConquestScript::AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 totalObjectives, uint32 timeRemaining) const
{
    SiegeScriptBase::AdjustStrategy(decision, scoreAdvantage, controlledCount, totalObjectives, timeRemaining);

    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 targetFaction = (faction == ALLIANCE) ? HORDE : ALLIANCE;

    // Check if we can access enemy keep
    if (CanAccessKeep(targetFaction))
    {
        decision.reasoning += " + gate destroyed";

        // Workshop control is key for siege
        if (IsNodeControlled(IsleOfConquest::Nodes::WORKSHOP, faction))
        {
            decision.strategy = BGStrategy::ALL_IN;
            decision.reasoning = "Gate down + siege engines - RUSH BOSS!";
            decision.offenseAllocation = 80;
            decision.defenseAllocation = 20;
        }
        else
        {
            decision.offenseAllocation = std::min(static_cast<uint8>(decision.offenseAllocation + 15), static_cast<uint8>(85));
        }
    }

    // Hangar control provides gunship
    if (IsNodeControlled(IsleOfConquest::Nodes::HANGAR, faction))
    {
        decision.reasoning += " + gunship";
    }
}

uint32 IsleOfConquestScript::GetBossEntry(uint32 faction) const
{
    return faction == ALLIANCE ? IsleOfConquest::HIGH_COMMANDER_HALFORD : IsleOfConquest::OVERLORD_AGMAR;
}

Position IsleOfConquestScript::GetBossPosition(uint32 faction) const
{
    return faction == ALLIANCE ? IsleOfConquest::GetHalfordPosition() : IsleOfConquest::GetAgmarPosition();
}

bool IsleOfConquestScript::CanAttackBoss(uint32 targetFaction) const
{
    return CanAccessKeep(targetFaction);
}

uint32 IsleOfConquestScript::GetIntactGateCount(uint32 faction) const
{
    uint32 count = 0;
    uint32 start = (faction == ALLIANCE) ? IsleOfConquest::Gates::ALLIANCE_FRONT : IsleOfConquest::Gates::HORDE_FRONT;
    uint32 end = (faction == ALLIANCE) ? IsleOfConquest::Gates::ALLIANCE_EAST : IsleOfConquest::Gates::HORDE_EAST;

    for (uint32 i = start; i <= end; ++i)
    {
        if (!IsGateDestroyed(i))
            ++count;
    }

    return count;
}

bool IsleOfConquestScript::CanAccessKeep(uint32 targetFaction) const
{
    // At least one gate must be destroyed to access the keep
    uint32 start = (targetFaction == ALLIANCE) ? IsleOfConquest::Gates::ALLIANCE_FRONT : IsleOfConquest::Gates::HORDE_FRONT;
    uint32 end = (targetFaction == ALLIANCE) ? IsleOfConquest::Gates::ALLIANCE_EAST : IsleOfConquest::Gates::HORDE_EAST;

    for (uint32 i = start; i <= end; ++i)
    {
        if (IsGateDestroyed(i))
            return true;
    }

    return false;
}

bool IsleOfConquestScript::IsNodeControlled(uint32 nodeId, uint32 faction) const
{
    auto it = m_nodeStates.find(nodeId);
    if (it == m_nodeStates.end())
        return false;

    return (faction == ALLIANCE && it->second == ObjectiveState::ALLIANCE_CONTROLLED) ||
           (faction == HORDE && it->second == ObjectiveState::HORDE_CONTROLLED);
}

std::vector<uint32> IsleOfConquestScript::GetAvailableVehicles(uint32 faction) const
{
    std::vector<uint32> vehicles;

    if (IsNodeControlled(IsleOfConquest::Nodes::DOCKS, faction))
    {
        auto dockVehicles = IsleOfConquest::GetVehiclesFromNode(IsleOfConquest::Nodes::DOCKS);
        vehicles.insert(vehicles.end(), dockVehicles.begin(), dockVehicles.end());
    }

    if (IsNodeControlled(IsleOfConquest::Nodes::WORKSHOP, faction))
    {
        auto workshopVehicles = IsleOfConquest::GetVehiclesFromNode(IsleOfConquest::Nodes::WORKSHOP);
        vehicles.insert(vehicles.end(), workshopVehicles.begin(), workshopVehicles.end());
    }

    return vehicles;
}

} // namespace Playerbot::Coordination::Battleground
