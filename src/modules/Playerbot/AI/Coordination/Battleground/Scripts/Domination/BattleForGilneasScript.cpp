/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "BattleForGilneasScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(BattleForGilneasScript, 761);  // BattleForGilneas::MAP_ID

void BattleForGilneasScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();
    RegisterScoreWorldState(BattleForGilneas::WorldStates::RESOURCES_ALLY, true);
    RegisterScoreWorldState(BattleForGilneas::WorldStates::RESOURCES_HORDE, false);
}

std::vector<BGObjectiveData> BattleForGilneasScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;
    for (uint32 i = 0; i < BattleForGilneas::NODE_COUNT; ++i)
        objectives.push_back(GetNodeData(i));
    return objectives;
}

BGObjectiveData BattleForGilneasScript::GetNodeData(uint32 nodeIndex) const
{
    BGObjectiveData node;
    Position pos = BattleForGilneas::GetNodePosition(nodeIndex);
    node.id = nodeIndex;
    node.type = ObjectiveType::NODE;
    node.name = BattleForGilneas::GetNodeName(nodeIndex);
    node.x = pos.GetPositionX();
    node.y = pos.GetPositionY();
    node.z = pos.GetPositionZ();
    node.strategicValue = (nodeIndex == BattleForGilneas::Nodes::WATERWORKS) ? 9 : 7;
    node.captureTime = BattleForGilneas::CAPTURE_TIME;
    return node;
}

std::vector<BGPositionData> BattleForGilneasScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    // Simplified spawn positions
    if (faction == ALLIANCE)
        spawns.push_back(BGPositionData("Alliance Spawn", 1052.0f, 1396.0f, 6.0f, 0, BGPositionData::PositionType::SPAWN_POINT, ALLIANCE, 5));
    else
        spawns.push_back(BGPositionData("Horde Spawn", 1330.0f, 736.0f, -8.0f, 0, BGPositionData::PositionType::SPAWN_POINT, HORDE, 5));
    return spawns;
}

std::vector<BGPositionData> BattleForGilneasScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;
    for (uint32 i = 0; i < BattleForGilneas::NODE_COUNT; ++i)
    {
        Position pos = BattleForGilneas::GetNodePosition(i);
        positions.push_back(BGPositionData(BattleForGilneas::GetNodeName(i), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0, BGPositionData::PositionType::DEFENSIVE_POSITION, 0, 7));
    }
    return positions;
}

std::vector<BGPositionData> BattleForGilneasScript::GetGraveyardPositions(uint32 /*faction*/) const
{
    return GetStrategicPositions();
}

std::vector<BGWorldState> BattleForGilneasScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(BattleForGilneas::WorldStates::RESOURCES_ALLY, "Alliance Resources", BGWorldState::StateType::SCORE_ALLIANCE, 0),
        BGWorldState(BattleForGilneas::WorldStates::RESOURCES_HORDE, "Horde Resources", BGWorldState::StateType::SCORE_HORDE, 0)
    };
}

bool BattleForGilneasScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void BattleForGilneasScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = hordeScore = 0;
    auto it = states.find(BattleForGilneas::WorldStates::RESOURCES_ALLY);
    if (it != states.end()) allianceScore = static_cast<uint32>(std::max(0, it->second));
    it = states.find(BattleForGilneas::WorldStates::RESOURCES_HORDE);
    if (it != states.end()) hordeScore = static_cast<uint32>(std::max(0, it->second));
}

std::vector<uint32> BattleForGilneasScript::GetTickPointsTable() const
{
    return std::vector<uint32>(BattleForGilneas::TICK_POINTS.begin(), BattleForGilneas::TICK_POINTS.end());
}

} // namespace Playerbot::Coordination::Battleground
