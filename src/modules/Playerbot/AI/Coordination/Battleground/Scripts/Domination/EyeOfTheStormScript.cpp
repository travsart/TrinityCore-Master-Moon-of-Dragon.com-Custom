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

#include "EyeOfTheStormScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"
#include "Timer.h"
#include "Timer.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(EyeOfTheStormScript, 566);  // EyeOfTheStorm::MAP_ID

// ============================================================================
// LIFECYCLE
// ============================================================================

void EyeOfTheStormScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);
    InitializeNodeTracking();

    m_cachedObjectives = GetObjectiveData();

    // Register score world states
    RegisterScoreWorldState(EyeOfTheStorm::WorldStates::RESOURCES_ALLY, true);
    RegisterScoreWorldState(EyeOfTheStorm::WorldStates::RESOURCES_HORDE, false);

    // Register node world states
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::FEL_REAVER_ALLIANCE,
        EyeOfTheStorm::Nodes::FEL_REAVER, BGObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::FEL_REAVER_HORDE,
        EyeOfTheStorm::Nodes::FEL_REAVER, BGObjectiveState::HORDE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::BLOOD_ELF_ALLIANCE,
        EyeOfTheStorm::Nodes::BLOOD_ELF, BGObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::BLOOD_ELF_HORDE,
        EyeOfTheStorm::Nodes::BLOOD_ELF, BGObjectiveState::HORDE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::DRAENEI_RUINS_ALLIANCE,
        EyeOfTheStorm::Nodes::DRAENEI_RUINS, BGObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::DRAENEI_RUINS_HORDE,
        EyeOfTheStorm::Nodes::DRAENEI_RUINS, BGObjectiveState::HORDE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::MAGE_TOWER_ALLIANCE,
        EyeOfTheStorm::Nodes::MAGE_TOWER, BGObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(EyeOfTheStorm::WorldStates::MAGE_TOWER_HORDE,
        EyeOfTheStorm::Nodes::MAGE_TOWER, BGObjectiveState::HORDE_CONTROLLED);

    // Reset flag state
    m_flagAtCenter = true;
    m_flagCarrier.Clear();
    m_flagPickupTime = 0;
    m_allianceFlagCaptures = 0;
    m_hordeFlagCaptures = 0;

    TC_LOG_DEBUG("playerbots.bg.script",
        "EyeOfTheStormScript: Loaded (4 nodes + center flag)");
}

void EyeOfTheStormScript::OnUpdate(uint32 diff)
{
    DominationScriptBase::OnUpdate(diff);

    // Additional EOTS-specific updates could go here
    // e.g., flag position tracking
}

// ============================================================================
// DATA PROVIDERS
// ============================================================================

std::vector<BGObjectiveData> EyeOfTheStormScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Add the 4 nodes
    for (uint32 i = 0; i < EyeOfTheStorm::NODE_COUNT; ++i)
    {
        objectives.push_back(GetNodeData(i));
    }

    // Add center flag as a special objective
    BGObjectiveData centerFlag;
    centerFlag.id = EyeOfTheStorm::NODE_COUNT;  // ID 4
    centerFlag.type = ObjectiveType::FLAG;
    centerFlag.name = "Center Flag";
    Position flagPos = EyeOfTheStorm::GetCenterFlagPosition();
    centerFlag.x = flagPos.GetPositionX();
    centerFlag.y = flagPos.GetPositionY();
    centerFlag.z = flagPos.GetPositionZ();
    centerFlag.strategicValue = 9;  // Very important when we have nodes
    centerFlag.captureTime = 0;     // Instant pickup
    objectives.push_back(centerFlag);

    return objectives;
}

BGObjectiveData EyeOfTheStormScript::GetNodeData(uint32 nodeIndex) const
{
    BGObjectiveData node;
    Position pos = EyeOfTheStorm::GetNodePosition(nodeIndex);

    node.id = nodeIndex;
    node.type = ObjectiveType::NODE;
    node.name = EyeOfTheStorm::GetNodeName(nodeIndex);
    node.x = pos.GetPositionX();
    node.y = pos.GetPositionY();
    node.z = pos.GetPositionZ();
    node.orientation = pos.GetOrientation();
    node.strategicValue = EyeOfTheStorm::GetNodeStrategicValue(nodeIndex);
    node.captureTime = EyeOfTheStorm::CAPTURE_TIME;

    return node;
}

std::vector<BGPositionData> EyeOfTheStormScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    if (faction == ALLIANCE)
    {
        for (const auto& pos : EyeOfTheStorm::ALLIANCE_SPAWNS)
        {
            BGPositionData spawn;
            spawn.name = "Alliance Spawn";
            spawn.x = pos.GetPositionX();
            spawn.y = pos.GetPositionY();
            spawn.z = pos.GetPositionZ();
            spawn.orientation = pos.GetOrientation();
            spawn.faction = ALLIANCE;
            spawn.posType = BGPositionData::PositionType::SPAWN_POINT;
            spawns.push_back(spawn);
        }
    }
    else
    {
        for (const auto& pos : EyeOfTheStorm::HORDE_SPAWNS)
        {
            BGPositionData spawn;
            spawn.name = "Horde Spawn";
            spawn.x = pos.GetPositionX();
            spawn.y = pos.GetPositionY();
            spawn.z = pos.GetPositionZ();
            spawn.orientation = pos.GetOrientation();
            spawn.faction = HORDE;
            spawn.posType = BGPositionData::PositionType::SPAWN_POINT;
            spawns.push_back(spawn);
        }
    }

    return spawns;
}

std::vector<BGPositionData> EyeOfTheStormScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Node defense positions
    for (uint32 i = 0; i < EyeOfTheStorm::NODE_COUNT; ++i)
    {
        auto defPos = EyeOfTheStorm::GetNodeDefensePositions(i);
        for (const auto& pos : defPos)
        {
            BGPositionData p(EyeOfTheStorm::GetNodeName(i),
                pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                pos.GetOrientation(), BGPositionData::PositionType::DEFENSIVE_POSITION,
                0, EyeOfTheStorm::GetNodeStrategicValue(i));
            positions.push_back(p);
        }
    }

    // Center flag area defense positions
    auto centerPositions = EyeOfTheStorm::GetCenterFlagDefensePositions();
    for (size_t i = 0; i < centerPositions.size(); ++i)
    {
        const auto& pos = centerPositions[i];
        std::string name = "Center Flag " + std::to_string(i + 1);
        positions.push_back(BGPositionData(name, pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::STRATEGIC_POINT, 0, 9));
    }

    // Bridge positions (critical EOTS chokepoints!)
    auto bridgePositions = EyeOfTheStorm::GetBridgePositions();
    for (size_t i = 0; i < bridgePositions.size(); ++i)
    {
        const auto& pos = bridgePositions[i];
        std::string name = "Bridge " + std::to_string(i + 1);
        positions.push_back(BGPositionData(name, pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::CHOKEPOINT, 0, 8));
    }

    // Sniper positions (elevated, good for ranged)
    auto sniperPositions = EyeOfTheStorm::GetSniperPositions();
    for (size_t i = 0; i < sniperPositions.size(); ++i)
    {
        const auto& pos = sniperPositions[i];
        std::string name = "Sniper Position " + std::to_string(i + 1);
        positions.push_back(BGPositionData(name, pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::SNIPER_POSITION, 0, 7));
    }

    return positions;
}

std::vector<BGPositionData> EyeOfTheStormScript::GetGraveyardPositions(uint32 faction) const
{
    // In EOTS, graveyards are at controlled nodes
    std::vector<BGPositionData> graveyards;

    for (uint32 i = 0; i < EyeOfTheStorm::NODE_COUNT; ++i)
    {
        Position pos = EyeOfTheStorm::GetNodePosition(i);
        std::string name = std::string(EyeOfTheStorm::GetNodeName(i)) + " GY";
        BGPositionData p(name, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
            0.0f, BGPositionData::PositionType::GRAVEYARD, 0, 5);
        graveyards.push_back(p);
    }

    return graveyards;
}

std::vector<BGWorldState> EyeOfTheStormScript::GetInitialWorldStates() const
{
    std::vector<BGWorldState> states;

    states.push_back(BGWorldState(EyeOfTheStorm::WorldStates::RESOURCES_ALLY,
        "Alliance Resources", BGWorldState::StateType::SCORE_ALLIANCE, 0));
    states.push_back(BGWorldState(EyeOfTheStorm::WorldStates::RESOURCES_HORDE,
        "Horde Resources", BGWorldState::StateType::SCORE_HORDE, 0));
    states.push_back(BGWorldState(EyeOfTheStorm::WorldStates::FLAG_STATE,
        "Flag State", BGWorldState::StateType::FLAG_STATE, 0));

    return states;
}

std::vector<uint32> EyeOfTheStormScript::GetTickPointsTable() const
{
    return std::vector<uint32>(
        EyeOfTheStorm::TICK_POINTS.begin(),
        EyeOfTheStorm::TICK_POINTS.end()
    );
}

// ============================================================================
// WORLD STATE
// ============================================================================

bool EyeOfTheStormScript::InterpretWorldState(int32 stateId, int32 value,
    uint32& outObjectiveId, BGObjectiveState& outState) const
{
    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void EyeOfTheStormScript::GetScoreFromWorldStates(const std::map<int32, int32>& states,
    uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = 0;
    hordeScore = 0;

    auto allyIt = states.find(EyeOfTheStorm::WorldStates::RESOURCES_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(EyeOfTheStorm::WorldStates::RESOURCES_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

// ============================================================================
// STRATEGY - EOTS SPECIFIC
// ============================================================================

RoleDistribution EyeOfTheStormScript::GetRecommendedRoles(
    const StrategicDecision& decision,
    float scoreAdvantage,
    uint32 timeRemaining) const
{
    RoleDistribution dist = DominationScriptBase::GetRecommendedRoles(
        decision, scoreAdvantage, timeRemaining);

    // EOTS-specific: Add flag runners when we have nodes
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourNodes = (faction == ALLIANCE) ? m_allianceNodes : m_hordeNodes;

    if (ourNodes >= 2 && ShouldPrioritizeFlag())
    {
        // Dedicate some players to flag running
        dist.SetRole(BGRole::FLAG_CARRIER, 1, 2);
        dist.SetRole(BGRole::FLAG_ESCORT, 2, 3);
        dist.reasoning += " (flag running with node control)";
    }
    else if (ourNodes < 2)
    {
        // Focus on nodes first
        dist.SetRole(BGRole::NODE_ATTACKER,
            std::max(dist.GetCount(BGRole::NODE_ATTACKER), static_cast<uint8>(6)),
            GetTeamSize() - 2);
        dist.reasoning = "Need more nodes before flag running";
    }

    return dist;
}

void EyeOfTheStormScript::AdjustStrategy(StrategicDecision& decision,
    float scoreAdvantage, uint32 controlledCount,
    uint32 totalObjectives, uint32 timeRemaining) const
{
    // Get base domination strategy
    DominationScriptBase::AdjustStrategy(decision, scoreAdvantage,
        controlledCount, totalObjectives, timeRemaining);

    // EOTS-specific: Consider flag strategy
    if (controlledCount >= 3)
    {
        // With 3+ nodes, flag caps give huge points
        decision.reasoning += " + focus on flag captures";

        // If we have 4 nodes, flag gives 500 points!
        if (controlledCount == 4)
        {
            decision.strategy = BGStrategy::AGGRESSIVE;
            decision.reasoning = "4-cap! Flag worth 500 points - push center";
        }
    }
    else if (controlledCount < 2)
    {
        // Without nodes, flag is worth little
        decision.attackObjectives.clear();
        for (uint32 i = 0; i < EyeOfTheStorm::NODE_COUNT; ++i)
        {
            decision.attackObjectives.push_back(i);
        }
        decision.reasoning = "Need nodes first - flag worth too little";
    }
}

// ============================================================================
// EVENTS
// ============================================================================

void EyeOfTheStormScript::OnEvent(const BGScriptEventData& event)
{
    DominationScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::FLAG_PICKED_UP:
            m_flagAtCenter = false;
            m_flagCarrier = event.primaryGuid;
            m_flagPickupTime = getMSTime();
            TC_LOG_DEBUG("playerbots.bg.script", "EOTS: Flag picked up");
            break;

        case BGScriptEvent::FLAG_DROPPED:
            m_flagCarrier.Clear();
            m_flagDropX = static_cast<uint32>(event.x);
            m_flagDropY = static_cast<uint32>(event.y);
            m_flagDropZ = static_cast<uint32>(event.z);
            TC_LOG_DEBUG("playerbots.bg.script", "EOTS: Flag dropped");
            break;

        case BGScriptEvent::FLAG_CAPTURED:
            m_flagAtCenter = true;  // Respawns at center
            m_flagCarrier.Clear();
            if (event.faction == ALLIANCE)
                ++m_allianceFlagCaptures;
            else
                ++m_hordeFlagCaptures;
            TC_LOG_DEBUG("playerbots.bg.script",
                "EOTS: Flag captured for {} points!",
                GetFlagCapturePoints());
            break;

        case BGScriptEvent::FLAG_RESET:
            m_flagAtCenter = true;
            m_flagCarrier.Clear();
            break;

        default:
            break;
    }
}

// ============================================================================
// EOTS-SPECIFIC HELPERS
// ============================================================================

uint32 EyeOfTheStormScript::GetFlagCapturePoints() const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourNodes = (faction == ALLIANCE) ? m_allianceNodes : m_hordeNodes;

    switch (ourNodes)
    {
        case 1: return EyeOfTheStorm::FLAG_POINTS_1_NODE;
        case 2: return EyeOfTheStorm::FLAG_POINTS_2_NODES;
        case 3: return EyeOfTheStorm::FLAG_POINTS_3_NODES;
        case 4: return EyeOfTheStorm::FLAG_POINTS_4_NODES;
        default: return 0;
    }
}

bool EyeOfTheStormScript::ShouldPrioritizeFlag() const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourNodes = (faction == ALLIANCE) ? m_allianceNodes : m_hordeNodes;

    // Flag is worth it with 2+ nodes
    if (ourNodes < 2)
        return false;

    // Check if flag is available
    if (!m_flagAtCenter && m_flagCarrier.IsEmpty())
        return false;  // Flag dropped somewhere - might not be worth pursuing

    return true;
}

ObjectGuid EyeOfTheStormScript::GetBestFlagRunnerCandidate() const
{
    // Would query coordinator for best candidate
    // Typically: mobile class with survivability
    return ObjectGuid();  // Placeholder
}

// ============================================================================
// ENTERPRISE-GRADE FLAG RUNNING AND POSITIONING
// ============================================================================

std::vector<Position> EyeOfTheStormScript::GetFlagRouteToNode(uint32 nodeId) const
{
    return EyeOfTheStorm::GetFlagRouteToNode(nodeId);
}

std::vector<Position> EyeOfTheStormScript::GetEscortFormation(Position const& fcPosition, uint8 escortCount) const
{
    return EyeOfTheStorm::GetEscortFormation(fcPosition, escortCount);
}

uint32 EyeOfTheStormScript::GetBestFlagCaptureNode(uint32 faction) const
{
    auto priorities = EyeOfTheStorm::GetFlagCapturePriority(faction);

    // Find first controlled node in priority order
    for (uint32 nodeId : priorities)
    {
        auto it = m_nodeStates.find(nodeId);
        if (it != m_nodeStates.end())
        {
            bool weControl = (faction == ALLIANCE)
                ? (it->second == BGObjectiveState::ALLIANCE_CONTROLLED)
                : (it->second == BGObjectiveState::HORDE_CONTROLLED);

            if (weControl)
                return nodeId;
        }
    }

    // No controlled nodes - flag is less valuable anyway
    return priorities[0];  // Return closest as fallback
}

float EyeOfTheStormScript::GetDistanceToCenter(uint32 nodeId) const
{
    return EyeOfTheStorm::GetDistanceToCenter(nodeId);
}

std::vector<Position> EyeOfTheStormScript::GetBridgePositions() const
{
    return EyeOfTheStorm::GetBridgePositions();
}

std::vector<Position> EyeOfTheStormScript::GetSniperPositions() const
{
    return EyeOfTheStorm::GetSniperPositions();
}

std::vector<Position> EyeOfTheStormScript::GetCenterFlagDefensePositions() const
{
    return EyeOfTheStorm::GetCenterFlagDefensePositions();
}

bool EyeOfTheStormScript::IsFlagWorthPursuing() const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourNodes = (faction == ALLIANCE) ? m_allianceNodes : m_hordeNodes;

    // Flag is worth it with 2+ nodes (using Strategy constant)
    if (ourNodes < EyeOfTheStorm::Strategy::MIN_NODES_FOR_FLAG)
        return false;

    // Especially worth it with 3+ nodes
    if (ourNodes >= EyeOfTheStorm::Strategy::IDEAL_NODES_FOR_FLAG)
        return true;

    // 2 nodes - only pursue if flag is available and nearby
    return m_flagAtCenter;
}

uint8 EyeOfTheStormScript::GetRecommendedEscortCount() const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourNodes = (faction == ALLIANCE) ? m_allianceNodes : m_hordeNodes;

    // With 4 nodes, flag is worth 500 points - protect heavily!
    if (ourNodes == 4)
        return EyeOfTheStorm::Strategy::OPTIMAL_FLAG_ESCORT;

    // With 3 nodes, still valuable
    if (ourNodes >= 3)
        return EyeOfTheStorm::Strategy::MIN_FLAG_ESCORT + 1;

    return EyeOfTheStorm::Strategy::MIN_FLAG_ESCORT;
}

// ============================================================================
// ENHANCED EVENT HANDLING
// ============================================================================

void EyeOfTheStormScript::OnMatchStart()
{
    DominationScriptBase::OnMatchStart();

    TC_LOG_INFO("playerbots.bg.script",
        "EOTS: Match started! Strategy: secure 2 nodes then contest center flag");
}

void EyeOfTheStormScript::OnMatchEnd(bool victory)
{
    DominationScriptBase::OnMatchEnd(victory);

    TC_LOG_INFO("playerbots.bg.script",
        "EOTS: Match ended - {}! Flag captures: Alliance={}, Horde={}",
        victory ? "Victory" : "Defeat",
        m_allianceFlagCaptures, m_hordeFlagCaptures);
}

// ============================================================================
// STRATEGIC NODE ANALYSIS
// ============================================================================

bool EyeOfTheStormScript::IsStrategicNode(uint32 nodeId) const
{
    // Draenei Ruins and Mage Tower are closer to center - strategic importance
    return nodeId == EyeOfTheStorm::Nodes::DRAENEI_RUINS ||
           nodeId == EyeOfTheStorm::Nodes::MAGE_TOWER;
}

uint32 EyeOfTheStormScript::GetNearestNodeToFlag() const
{
    float minDist = std::numeric_limits<float>::max();
    uint32 nearest = 0;

    for (uint32 i = 0; i < EyeOfTheStorm::NODE_COUNT; ++i)
    {
        float dist = EyeOfTheStorm::GetDistanceToCenter(i);
        if (dist < minDist)
        {
            minDist = dist;
            nearest = i;
        }
    }

    return nearest;
}

uint32 EyeOfTheStormScript::GetDefendersNeeded(uint32 nodeId) const
{
    // Strategic nodes need more defenders
    if (IsStrategicNode(nodeId))
        return EyeOfTheStorm::Strategy::STRATEGIC_NODE_DEFENDERS;

    return EyeOfTheStorm::Strategy::MIN_NODE_DEFENDERS;
}

} // namespace Playerbot::Coordination::Battleground
