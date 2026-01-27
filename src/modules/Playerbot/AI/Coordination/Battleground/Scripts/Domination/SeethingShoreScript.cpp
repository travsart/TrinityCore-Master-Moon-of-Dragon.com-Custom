/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "SeethingShoreScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(SeethingShoreScript, 1803);  // SeethingShore::MAP_ID

void SeethingShoreScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();
    RegisterScoreWorldState(SeethingShore::WorldStates::AZERITE_ALLY, true);
    RegisterScoreWorldState(SeethingShore::WorldStates::AZERITE_HORDE, false);
    m_activeNodes.clear();
    m_nextSpawnTimer = 0;
    m_nextNodeId = 0;
}

void SeethingShoreScript::OnUpdate(uint32 diff)
{
    DominationScriptBase::OnUpdate(diff);
    UpdateActiveNodes();
}

void SeethingShoreScript::OnEvent(const BGScriptEventData& event)
{
    DominationScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::AZERITE_SPAWNED:
            // New node spawned
            SpawnNewNode();
            break;

        case BGScriptEvent::OBJECTIVE_CAPTURED:
            // Remove captured node from active list
            m_activeNodes.erase(
                std::remove_if(m_activeNodes.begin(), m_activeNodes.end(),
                    [&event](const SeethingShore::AzeriteNode& node) {
                        return node.id == event.objectiveId;
                    }),
                m_activeNodes.end());
            break;

        default:
            break;
    }
}

void SeethingShoreScript::UpdateActiveNodes()
{
    // Ensure we have enough active nodes
    while (m_activeNodes.size() < SeethingShore::MAX_ACTIVE_NODES)
    {
        SpawnNewNode();
    }
}

void SeethingShoreScript::SpawnNewNode()
{
    // Find a zone that doesn't have an active node
    std::vector<uint32> availableZones;
    for (uint32 i = 0; i < SeethingShore::SpawnZones::ZONE_COUNT; ++i)
    {
        bool zoneUsed = false;
        for (const auto& node : m_activeNodes)
        {
            Position zonePos = SeethingShore::GetZoneCenter(i);
            float dx = node.position.GetPositionX() - zonePos.GetPositionX();
            float dy = node.position.GetPositionY() - zonePos.GetPositionY();
            if (dx * dx + dy * dy < 400.0f)  // Within 20 yards
            {
                zoneUsed = true;
                break;
            }
        }
        if (!zoneUsed)
            availableZones.push_back(i);
    }

    if (!availableZones.empty())
    {
        uint32 zoneId = availableZones[rand() % availableZones.size()];
        SeethingShore::AzeriteNode node;
        node.id = m_nextNodeId++;
        node.position = SeethingShore::GetZoneCenter(zoneId);
        node.active = true;
        node.spawnTime = 0;
        node.capturedByFaction = 0;
        m_activeNodes.push_back(node);
    }
}

std::vector<BGObjectiveData> SeethingShoreScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Add all potential zones as objectives
    for (uint32 i = 0; i < SeethingShore::SpawnZones::ZONE_COUNT; ++i)
        objectives.push_back(GetNodeData(i));

    return objectives;
}

BGObjectiveData SeethingShoreScript::GetNodeData(uint32 nodeIndex) const
{
    BGObjectiveData node;
    Position pos = SeethingShore::GetZoneCenter(nodeIndex);
    node.id = nodeIndex;
    node.type = ObjectiveType::NODE;
    node.name = SeethingShore::GetZoneName(nodeIndex);
    node.x = pos.GetPositionX();
    node.y = pos.GetPositionY();
    node.z = pos.GetPositionZ();
    node.strategicValue = 8;
    node.captureTime = SeethingShore::CAPTURE_TIME;
    return node;
}

std::vector<BGPositionData> SeethingShoreScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    Position pos = SeethingShore::GetSpawnPosition(faction);
    spawns.push_back(BGPositionData(faction == ALLIANCE ? "Alliance Spawn" : "Horde Spawn",
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
        BGPositionData::PositionType::SPAWN_POINT, faction, 5));
    return spawns;
}

std::vector<BGPositionData> SeethingShoreScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Add all zone centers as strategic positions
    for (uint32 i = 0; i < SeethingShore::SpawnZones::ZONE_COUNT; ++i)
    {
        Position pos = SeethingShore::GetZoneCenter(i);
        positions.push_back(BGPositionData(SeethingShore::GetZoneName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::STRATEGIC_POINT, 0, 7));
    }

    return positions;
}

std::vector<BGPositionData> SeethingShoreScript::GetGraveyardPositions(uint32 faction) const
{
    return GetSpawnPositions(faction);
}

std::vector<BGWorldState> SeethingShoreScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(SeethingShore::WorldStates::AZERITE_ALLY, "Alliance Azerite", BGWorldState::StateType::SCORE_ALLIANCE, 0),
        BGWorldState(SeethingShore::WorldStates::AZERITE_HORDE, "Horde Azerite", BGWorldState::StateType::SCORE_HORDE, 0)
    };
}

bool SeethingShoreScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void SeethingShoreScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = hordeScore = 0;
    auto it = states.find(SeethingShore::WorldStates::AZERITE_ALLY);
    if (it != states.end()) allianceScore = static_cast<uint32>(std::max(0, it->second));
    it = states.find(SeethingShore::WorldStates::AZERITE_HORDE);
    if (it != states.end()) hordeScore = static_cast<uint32>(std::max(0, it->second));
}

RoleDistribution SeethingShoreScript::GetRecommendedRoles(const StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution dist;

    // Seething Shore needs mobile, aggressive groups
    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            dist.roleCounts[BGRole::NODE_ATTACKER] = 50;
            dist.roleCounts[BGRole::ROAMER] = 30;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 20;
            dist.reasoning = "Aggressive node capture";
            break;

        case BGStrategy::DEFENSIVE:
            dist.roleCounts[BGRole::NODE_DEFENDER] = 40;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 35;
            dist.roleCounts[BGRole::ROAMER] = 25;
            dist.reasoning = "Defensive hold";
            break;

        default:  // BALANCED
            dist.roleCounts[BGRole::NODE_ATTACKER] = 40;
            dist.roleCounts[BGRole::ROAMER] = 35;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 25;
            dist.reasoning = "Balanced control";
            break;
    }

    return dist;
}

void SeethingShoreScript::AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 /*controlledCount*/, uint32 /*totalObjectives*/, uint32 timeRemaining) const
{
    uint32 activeNodes = GetActiveNodeCount();

    // Dynamic node spawning requires mobile strategy
    if (activeNodes >= 3)
    {
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.reasoning = "Multiple nodes active - spread and capture";
        decision.offenseAllocation = 70;
        decision.defenseAllocation = 30;
    }
    else if (scoreAdvantage > 0.2f)
    {
        decision.strategy = BGStrategy::BALANCED;
        decision.reasoning = "Leading - maintain pressure on new nodes";
        decision.offenseAllocation = 55;
        decision.defenseAllocation = 45;
    }
    else if (scoreAdvantage < -0.2f && timeRemaining < 180000)
    {
        decision.strategy = BGStrategy::ALL_IN;
        decision.reasoning = "Behind with little time - all in on nodes!";
        decision.offenseAllocation = 85;
        decision.defenseAllocation = 15;
    }
    else
    {
        decision.strategy = BGStrategy::BALANCED;
        decision.reasoning = "Balanced dynamic capture";
        decision.offenseAllocation = 60;
        decision.defenseAllocation = 40;
    }

    decision.reasoning += " (dynamic spawning)";
}

bool SeethingShoreScript::IsNodeActive(uint32 nodeId) const
{
    for (const auto& node : m_activeNodes)
        if (node.id == nodeId && node.active)
            return true;
    return false;
}

Position SeethingShoreScript::GetNearestActiveNode(float x, float y) const
{
    Position nearest(0, 0, 0, 0);
    float minDist = std::numeric_limits<float>::max();

    for (const auto& node : m_activeNodes)
    {
        if (!node.active)
            continue;

        float dx = node.position.GetPositionX() - x;
        float dy = node.position.GetPositionY() - y;
        float dist = dx * dx + dy * dy;

        if (dist < minDist)
        {
            minDist = dist;
            nearest = node.position;
        }
    }

    return nearest;
}

std::vector<uint32> SeethingShoreScript::GetTickPointsTable() const
{
    // Seething Shore uses flat points per capture
    return { SeethingShore::AZERITE_PER_NODE };
}

} // namespace Playerbot::Coordination::Battleground
