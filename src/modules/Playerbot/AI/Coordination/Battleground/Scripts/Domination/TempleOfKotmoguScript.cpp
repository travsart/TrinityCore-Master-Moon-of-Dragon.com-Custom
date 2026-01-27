/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "TempleOfKotmoguScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(TempleOfKotmoguScript, 998);  // TempleOfKotmogu::MAP_ID

void TempleOfKotmoguScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();
    RegisterScoreWorldState(TempleOfKotmogu::WorldStates::SCORE_ALLY, true);
    RegisterScoreWorldState(TempleOfKotmogu::WorldStates::SCORE_HORDE, false);
    m_orbHolders.clear();
    m_playerOrbs.clear();
}

void TempleOfKotmoguScript::OnEvent(const BGScriptEventData& event)
{
    DominationScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::ORB_PICKED_UP:
            if (event.objectiveId < TempleOfKotmogu::ORB_COUNT)
            {
                m_orbHolders[event.objectiveId] = event.primaryGuid;
                m_playerOrbs[event.primaryGuid] = event.objectiveId;
            }
            break;

        case BGScriptEvent::ORB_DROPPED:
            if (event.objectiveId < TempleOfKotmogu::ORB_COUNT)
            {
                auto it = m_orbHolders.find(event.objectiveId);
                if (it != m_orbHolders.end())
                {
                    m_playerOrbs.erase(it->second);
                    m_orbHolders.erase(it);
                }
            }
            break;

        case BGScriptEvent::PLAYER_KILLED:
            // When player dies, they drop their orb
            {
                auto it = m_playerOrbs.find(event.secondaryGuid);
                if (it != m_playerOrbs.end())
                {
                    m_orbHolders.erase(it->second);
                    m_playerOrbs.erase(it);
                }
            }
            break;

        default:
            break;
    }
}

std::vector<BGObjectiveData> TempleOfKotmoguScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;
    for (uint32 i = 0; i < TempleOfKotmogu::ORB_COUNT; ++i)
        objectives.push_back(GetOrbData(i));

    // Add center as a strategic objective
    BGObjectiveData center;
    center.id = 100;  // Special ID for center
    center.type = ObjectiveType::STRATEGIC;
    center.name = "Center";
    center.x = TempleOfKotmogu::CENTER_X;
    center.y = TempleOfKotmogu::CENTER_Y;
    center.z = TempleOfKotmogu::CENTER_Z;
    center.strategicValue = 10;
    objectives.push_back(center);

    return objectives;
}

BGObjectiveData TempleOfKotmoguScript::GetOrbData(uint32 orbId) const
{
    BGObjectiveData orb;
    Position pos = TempleOfKotmogu::GetOrbPosition(orbId);
    orb.id = orbId;
    orb.type = ObjectiveType::ORB;
    orb.name = TempleOfKotmogu::GetOrbName(orbId);
    orb.x = pos.GetPositionX();
    orb.y = pos.GetPositionY();
    orb.z = pos.GetPositionZ();
    orb.strategicValue = 8;
    orb.captureTime = 0;  // Instant pickup
    return orb;
}

BGObjectiveData TempleOfKotmoguScript::GetNodeData(uint32 nodeIndex) const
{
    return GetOrbData(nodeIndex);
}

std::vector<BGPositionData> TempleOfKotmoguScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    // Spawn positions on opposite sides
    if (faction == ALLIANCE)
        spawns.push_back(BGPositionData("Alliance Spawn", 1790.0f, 1312.0f, 35.0f, 0, BGPositionData::PositionType::SPAWN_POINT, ALLIANCE, 5));
    else
        spawns.push_back(BGPositionData("Horde Spawn", 1674.0f, 1263.0f, 35.0f, 0, BGPositionData::PositionType::SPAWN_POINT, HORDE, 5));
    return spawns;
}

std::vector<BGPositionData> TempleOfKotmoguScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Orb positions
    for (uint32 i = 0; i < TempleOfKotmogu::ORB_COUNT; ++i)
    {
        Position pos = TempleOfKotmogu::GetOrbPosition(i);
        positions.push_back(BGPositionData(TempleOfKotmogu::GetOrbName(i), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0, BGPositionData::PositionType::STRATEGIC_POINT, 0, 8));
    }

    // Center position (highest priority)
    positions.push_back(BGPositionData("Center", TempleOfKotmogu::CENTER_X, TempleOfKotmogu::CENTER_Y, TempleOfKotmogu::CENTER_Z, 0, BGPositionData::PositionType::STRATEGIC_POINT, 0, 10));

    return positions;
}

std::vector<BGPositionData> TempleOfKotmoguScript::GetGraveyardPositions(uint32 /*faction*/) const
{
    // Kotmogu has faction-specific graveyards
    return GetSpawnPositions(0);
}

std::vector<BGWorldState> TempleOfKotmoguScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(TempleOfKotmogu::WorldStates::SCORE_ALLY, "Alliance Score", BGWorldState::StateType::SCORE_ALLIANCE, 0),
        BGWorldState(TempleOfKotmogu::WorldStates::SCORE_HORDE, "Horde Score", BGWorldState::StateType::SCORE_HORDE, 0)
    };
}

bool TempleOfKotmoguScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void TempleOfKotmoguScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = hordeScore = 0;
    auto it = states.find(TempleOfKotmogu::WorldStates::SCORE_ALLY);
    if (it != states.end()) allianceScore = static_cast<uint32>(std::max(0, it->second));
    it = states.find(TempleOfKotmogu::WorldStates::SCORE_HORDE);
    if (it != states.end()) hordeScore = static_cast<uint32>(std::max(0, it->second));
}

RoleDistribution TempleOfKotmoguScript::GetRecommendedRoles(const StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution dist;

    // Kotmogu needs orb carriers and escorts
    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            dist.roleCounts[BGRole::ORB_CARRIER] = 40;      // Multiple orb carriers
            dist.roleCounts[BGRole::FLAG_ESCORT] = 30;      // Protect carriers
            dist.roleCounts[BGRole::NODE_ATTACKER] = 20;    // Kill enemy orb holders
            dist.roleCounts[BGRole::NODE_DEFENDER] = 10;
            dist.reasoning = "Aggressive orb capture";
            break;

        case BGStrategy::DEFENSIVE:
            dist.roleCounts[BGRole::ORB_CARRIER] = 30;
            dist.roleCounts[BGRole::FLAG_ESCORT] = 40;      // Heavy protection
            dist.roleCounts[BGRole::NODE_DEFENDER] = 20;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 10;
            dist.reasoning = "Defensive orb hold";
            break;

        default:  // BALANCED
            dist.roleCounts[BGRole::ORB_CARRIER] = 30;
            dist.roleCounts[BGRole::FLAG_ESCORT] = 30;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 20;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 20;
            dist.reasoning = "Balanced orb control";
            break;
    }

    return dist;
}

void TempleOfKotmoguScript::AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 /*totalObjectives*/, uint32 timeRemaining) const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourOrbs = GetOrbsHeldByFaction(faction);
    uint32 theirOrbs = GetOrbsHeldByFaction(faction == ALLIANCE ? HORDE : ALLIANCE);

    // Strategy based on orb control
    if (ourOrbs >= 2 && scoreAdvantage > 0.1f)
    {
        decision.strategy = BGStrategy::DEFENSIVE;
        decision.reasoning = "Hold orbs and protect carriers";
        decision.defenseAllocation = 60;
        decision.offenseAllocation = 40;
    }
    else if (theirOrbs >= 3)
    {
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.reasoning = "Enemy has too many orbs - attack!";
        decision.offenseAllocation = 70;
        decision.defenseAllocation = 30;
    }
    else if (timeRemaining < 120000 && scoreAdvantage < 0)
    {
        decision.strategy = BGStrategy::ALL_IN;
        decision.reasoning = "Behind with little time - all in!";
        decision.offenseAllocation = 80;
        decision.defenseAllocation = 20;
    }
    else
    {
        decision.strategy = BGStrategy::BALANCED;
        decision.reasoning = "Balanced orb control";
        decision.offenseAllocation = 50;
        decision.defenseAllocation = 50;
    }

    // Center control bonus
    decision.reasoning += " (center = bonus points)";
}

ObjectGuid TempleOfKotmoguScript::GetOrbHolder(uint32 orbId) const
{
    auto it = m_orbHolders.find(orbId);
    return it != m_orbHolders.end() ? it->second : ObjectGuid::Empty;
}

bool TempleOfKotmoguScript::IsPlayerHoldingOrb(ObjectGuid guid) const
{
    return m_playerOrbs.find(guid) != m_playerOrbs.end();
}

uint32 TempleOfKotmoguScript::GetOrbsHeldByFaction(uint32 /*faction*/) const
{
    // This would need actual player faction lookup
    // For now, return count based on holder map
    return static_cast<uint32>(m_orbHolders.size() / 2);  // Approximate
}

bool TempleOfKotmoguScript::IsInCenter(float x, float y) const
{
    float dx = x - TempleOfKotmogu::CENTER_X;
    float dy = y - TempleOfKotmogu::CENTER_Y;
    return (dx * dx + dy * dy) <= (TempleOfKotmogu::CENTER_RADIUS * TempleOfKotmogu::CENTER_RADIUS);
}

std::vector<uint32> TempleOfKotmoguScript::GetTickPointsTable() const
{
    // Points per orb held (modified by center position)
    return { 0, TempleOfKotmogu::POINTS_BASE, TempleOfKotmogu::POINTS_BASE * 2, TempleOfKotmogu::POINTS_BASE * 3, TempleOfKotmogu::POINTS_BASE * 4 };
}

} // namespace Playerbot::Coordination::Battleground
