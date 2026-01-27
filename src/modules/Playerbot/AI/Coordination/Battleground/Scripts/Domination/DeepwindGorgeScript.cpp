/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "DeepwindGorgeScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(DeepwindGorgeScript, 1105);  // DeepwindGorge::MAP_ID

void DeepwindGorgeScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();
    RegisterScoreWorldState(DeepwindGorge::WorldStates::RESOURCES_ALLY, true);
    RegisterScoreWorldState(DeepwindGorge::WorldStates::RESOURCES_HORDE, false);
    m_activeCart = 0;
    m_cartProgress.clear();
    m_cartContested.clear();
}

void DeepwindGorgeScript::OnEvent(const BGScriptEventData& event)
{
    DominationScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::CART_CAPTURED:
            // Cart was successfully captured
            if (event.objectiveId < DeepwindGorge::CART_COUNT)
            {
                m_cartProgress[event.objectiveId] = 0.0f;
                m_cartContested[event.objectiveId] = false;
            }
            break;

        default:
            break;
    }
}

std::vector<BGObjectiveData> DeepwindGorgeScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Add mines (nodes)
    for (uint32 i = 0; i < DeepwindGorge::NODE_COUNT; ++i)
        objectives.push_back(GetNodeData(i));

    // Add carts as objectives
    for (uint32 i = 0; i < DeepwindGorge::CART_COUNT; ++i)
    {
        BGObjectiveData cart;
        cart.id = 50 + i;  // Offset to avoid node collision
        cart.type = ObjectiveType::CART;
        cart.name = std::string("Mine Cart ") + std::to_string(i + 1);
        cart.strategicValue = 9;  // High value
        objectives.push_back(cart);
    }

    return objectives;
}

BGObjectiveData DeepwindGorgeScript::GetNodeData(uint32 nodeIndex) const
{
    BGObjectiveData node;
    Position pos = DeepwindGorge::GetNodePosition(nodeIndex);
    node.id = nodeIndex;
    node.type = ObjectiveType::NODE;
    node.name = DeepwindGorge::GetNodeName(nodeIndex);
    node.x = pos.GetPositionX();
    node.y = pos.GetPositionY();
    node.z = pos.GetPositionZ();
    node.strategicValue = (nodeIndex == DeepwindGorge::Nodes::CENTER_MINE) ? 8 : 7;
    node.captureTime = DeepwindGorge::CAPTURE_TIME;
    return node;
}

std::vector<BGPositionData> DeepwindGorgeScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    if (faction == ALLIANCE)
        spawns.push_back(BGPositionData("Alliance Spawn", 1350.0f, 1100.0f, 0.0f, 0, BGPositionData::PositionType::SPAWN_POINT, ALLIANCE, 5));
    else
        spawns.push_back(BGPositionData("Horde Spawn", 1850.0f, 800.0f, 0.0f, 0, BGPositionData::PositionType::SPAWN_POINT, HORDE, 5));
    return spawns;
}

std::vector<BGPositionData> DeepwindGorgeScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Node positions
    for (uint32 i = 0; i < DeepwindGorge::NODE_COUNT; ++i)
    {
        Position pos = DeepwindGorge::GetNodePosition(i);
        positions.push_back(BGPositionData(DeepwindGorge::GetNodeName(i), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0, BGPositionData::PositionType::DEFENSIVE_POSITION, 0, 7));
    }

    // Cart depot positions
    Position allyDepot = DeepwindGorge::GetCartDepotPosition(ALLIANCE);
    positions.push_back(BGPositionData("Alliance Cart Depot", allyDepot.GetPositionX(), allyDepot.GetPositionY(), allyDepot.GetPositionZ(), 0, BGPositionData::PositionType::STRATEGIC_POINT, ALLIANCE, 9));

    Position hordeDepot = DeepwindGorge::GetCartDepotPosition(HORDE);
    positions.push_back(BGPositionData("Horde Cart Depot", hordeDepot.GetPositionX(), hordeDepot.GetPositionY(), hordeDepot.GetPositionZ(), 0, BGPositionData::PositionType::STRATEGIC_POINT, HORDE, 9));

    return positions;
}

std::vector<BGPositionData> DeepwindGorgeScript::GetGraveyardPositions(uint32 /*faction*/) const
{
    return GetStrategicPositions();
}

std::vector<BGWorldState> DeepwindGorgeScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(DeepwindGorge::WorldStates::RESOURCES_ALLY, "Alliance Resources", BGWorldState::StateType::SCORE_ALLIANCE, 0),
        BGWorldState(DeepwindGorge::WorldStates::RESOURCES_HORDE, "Horde Resources", BGWorldState::StateType::SCORE_HORDE, 0)
    };
}

bool DeepwindGorgeScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, BGObjectiveState& outState) const
{
    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void DeepwindGorgeScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = hordeScore = 0;
    auto it = states.find(DeepwindGorge::WorldStates::RESOURCES_ALLY);
    if (it != states.end()) allianceScore = static_cast<uint32>(std::max(0, it->second));
    it = states.find(DeepwindGorge::WorldStates::RESOURCES_HORDE);
    if (it != states.end()) hordeScore = static_cast<uint32>(std::max(0, it->second));
}

RoleDistribution DeepwindGorgeScript::GetRecommendedRoles(const StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution dist;

    // Deepwind needs both node control and cart escorts
    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            dist.roleCounts[BGRole::NODE_ATTACKER] = 30;
            dist.roleCounts[BGRole::CART_PUSHER] = 35;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 20;
            dist.roleCounts[BGRole::ROAMER] = 15;
            dist.reasoning = "Aggressive node capture";
            break;

        case BGStrategy::DEFENSIVE:
            dist.roleCounts[BGRole::NODE_DEFENDER] = 40;
            dist.roleCounts[BGRole::CART_PUSHER] = 25;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 20;
            dist.roleCounts[BGRole::ROAMER] = 15;
            dist.reasoning = "Defensive node hold";
            break;

        default:  // BALANCED
            dist.roleCounts[BGRole::NODE_ATTACKER] = 25;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 25;
            dist.roleCounts[BGRole::CART_PUSHER] = 30;
            dist.roleCounts[BGRole::ROAMER] = 20;
            dist.reasoning = "Balanced control";
            break;
    }

    return dist;
}

void DeepwindGorgeScript::AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 totalObjectives, uint32 timeRemaining) const
{
    DominationScriptBase::AdjustStrategy(decision, scoreAdvantage, controlledCount, totalObjectives, timeRemaining);

    // Add cart-specific strategy
    if (m_activeCart < DeepwindGorge::CART_COUNT)
    {
        float progress = GetCartProgress(m_activeCart);
        bool contested = IsCartContested(m_activeCart);

        if (progress > 0.7f && !contested)
        {
            decision.reasoning += " + cart nearly captured";
            decision.offenseAllocation += 10;
        }
        else if (contested)
        {
            decision.reasoning += " + cart contested - send reinforcements";
            decision.offenseAllocation += 15;
        }
    }

    decision.reasoning += " (nodes + carts hybrid)";
}

float DeepwindGorgeScript::GetCartProgress(uint32 cartId) const
{
    auto it = m_cartProgress.find(cartId);
    return it != m_cartProgress.end() ? it->second : 0.0f;
}

bool DeepwindGorgeScript::IsCartContested(uint32 cartId) const
{
    auto it = m_cartContested.find(cartId);
    return it != m_cartContested.end() && it->second;
}

std::vector<uint32> DeepwindGorgeScript::GetTickPointsTable() const
{
    return std::vector<uint32>(DeepwindGorge::TICK_POINTS.begin(), DeepwindGorge::TICK_POINTS.end());
}

} // namespace Playerbot::Coordination::Battleground
