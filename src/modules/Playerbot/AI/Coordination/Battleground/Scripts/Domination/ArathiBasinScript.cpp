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

#include "ArathiBasinScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

// Register the script
REGISTER_BG_SCRIPT(ArathiBasinScript, 529);  // ArathiBasin::MAP_ID

// ============================================================================
// LIFECYCLE
// ============================================================================

void ArathiBasinScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);

    // Cache objective data
    m_cachedObjectives = GetObjectiveData();

    // Register world state mappings for scores
    RegisterScoreWorldState(ArathiBasin::WorldStates::RESOURCES_ALLY, true);
    RegisterScoreWorldState(ArathiBasin::WorldStates::RESOURCES_HORDE, false);

    // Register node world states
    // Stables
    RegisterWorldStateMapping(ArathiBasin::WorldStates::STABLES_ALLIANCE_CONTROLLED,
        ArathiBasin::Nodes::STABLES, ObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::STABLES_HORDE_CONTROLLED,
        ArathiBasin::Nodes::STABLES, ObjectiveState::HORDE_CONTROLLED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::STABLES_ALLIANCE,
        ArathiBasin::Nodes::STABLES, ObjectiveState::ALLIANCE_CONTESTED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::STABLES_HORDE,
        ArathiBasin::Nodes::STABLES, ObjectiveState::HORDE_CONTESTED);

    // Blacksmith
    RegisterWorldStateMapping(ArathiBasin::WorldStates::BLACKSMITH_ALLIANCE_CONTROLLED,
        ArathiBasin::Nodes::BLACKSMITH, ObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::BLACKSMITH_HORDE_CONTROLLED,
        ArathiBasin::Nodes::BLACKSMITH, ObjectiveState::HORDE_CONTROLLED);

    // Farm
    RegisterWorldStateMapping(ArathiBasin::WorldStates::FARM_ALLIANCE_CONTROLLED,
        ArathiBasin::Nodes::FARM, ObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::FARM_HORDE_CONTROLLED,
        ArathiBasin::Nodes::FARM, ObjectiveState::HORDE_CONTROLLED);

    // Gold Mine
    RegisterWorldStateMapping(ArathiBasin::WorldStates::GOLD_MINE_ALLIANCE_CONTROLLED,
        ArathiBasin::Nodes::GOLD_MINE, ObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::GOLD_MINE_HORDE_CONTROLLED,
        ArathiBasin::Nodes::GOLD_MINE, ObjectiveState::HORDE_CONTROLLED);

    // Lumber Mill
    RegisterWorldStateMapping(ArathiBasin::WorldStates::LUMBER_MILL_ALLIANCE_CONTROLLED,
        ArathiBasin::Nodes::LUMBER_MILL, ObjectiveState::ALLIANCE_CONTROLLED);
    RegisterWorldStateMapping(ArathiBasin::WorldStates::LUMBER_MILL_HORDE_CONTROLLED,
        ArathiBasin::Nodes::LUMBER_MILL, ObjectiveState::HORDE_CONTROLLED);

    TC_LOG_DEBUG("playerbots.bg.script",
        "ArathiBasinScript: Loaded with {} nodes",
        ArathiBasin::NODE_COUNT);
}

// ============================================================================
// DATA PROVIDERS
// ============================================================================

std::vector<BGObjectiveData> ArathiBasinScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    for (uint32 i = 0; i < ArathiBasin::NODE_COUNT; ++i)
    {
        BGObjectiveData node = GetNodeData(i);
        objectives.push_back(node);
    }

    return objectives;
}

BGObjectiveData ArathiBasinScript::GetNodeData(uint32 nodeIndex) const
{
    BGObjectiveData node;
    Position pos = ArathiBasin::GetNodePosition(nodeIndex);

    node.id = nodeIndex;
    node.type = ObjectiveType::NODE;
    node.name = ArathiBasin::GetNodeName(nodeIndex);
    node.x = pos.GetPositionX();
    node.y = pos.GetPositionY();
    node.z = pos.GetPositionZ();
    node.orientation = pos.GetOrientation();
    node.strategicValue = ArathiBasin::GetNodeStrategicValue(nodeIndex);
    node.captureTime = ArathiBasin::CAPTURE_TIME;

    // Set game object entries
    switch (nodeIndex)
    {
        case ArathiBasin::Nodes::STABLES:
            node.gameObjectEntry = ArathiBasin::GameObjects::STABLES_BANNER;
            break;
        case ArathiBasin::Nodes::BLACKSMITH:
            node.gameObjectEntry = ArathiBasin::GameObjects::BLACKSMITH_BANNER;
            break;
        case ArathiBasin::Nodes::FARM:
            node.gameObjectEntry = ArathiBasin::GameObjects::FARM_BANNER;
            break;
        case ArathiBasin::Nodes::GOLD_MINE:
            node.gameObjectEntry = ArathiBasin::GameObjects::GOLD_MINE_BANNER;
            break;
        case ArathiBasin::Nodes::LUMBER_MILL:
            node.gameObjectEntry = ArathiBasin::GameObjects::LUMBER_MILL_BANNER;
            break;
    }

    // Set connected nodes
    node.connectedObjectives = ArathiBasin::GetAdjacentNodes(nodeIndex);

    // Calculate distances from spawns
    node.distanceFromAllianceSpawn = GetDistanceFromSpawn(nodeIndex, ALLIANCE);
    node.distanceFromHordeSpawn = GetDistanceFromSpawn(nodeIndex, HORDE);

    return node;
}

std::vector<BGPositionData> ArathiBasinScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    if (faction == ALLIANCE)
    {
        for (const auto& pos : ArathiBasin::ALLIANCE_SPAWNS)
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
        for (const auto& pos : ArathiBasin::HORDE_SPAWNS)
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

std::vector<BGPositionData> ArathiBasinScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Add defense positions for each node
    for (uint32 i = 0; i < ArathiBasin::NODE_COUNT; ++i)
    {
        auto defensePositions = ArathiBasin::GetNodeDefensePositions(i);
        for (const auto& pos : defensePositions)
        {
            BGPositionData p(ArathiBasin::GetNodeName(i),
                pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                pos.GetOrientation(),
                BGPositionData::PositionType::DEFENSIVE_POSITION,
                0, ArathiBasin::GetNodeStrategicValue(i));
            positions.push_back(p);
        }
    }

    // Add chokepoints between nodes
    // Stables <-> Blacksmith
    positions.push_back(BGPositionData("Stables-BS Road", 1070.0f, 1125.0f, -55.0f, 0.0f,
        BGPositionData::PositionType::CHOKEPOINT, 0, 6));

    // Blacksmith <-> Farm
    positions.push_back(BGPositionData("BS-Farm Road", 890.0f, 960.0f, -50.0f, 0.0f,
        BGPositionData::PositionType::CHOKEPOINT, 0, 6));

    // Blacksmith <-> Gold Mine
    positions.push_back(BGPositionData("BS-GM Road", 1060.0f, 945.0f, -80.0f, 0.0f,
        BGPositionData::PositionType::CHOKEPOINT, 0, 5));

    // Blacksmith <-> Lumber Mill
    positions.push_back(BGPositionData("BS-LM Road", 915.0f, 1095.0f, -20.0f, 0.0f,
        BGPositionData::PositionType::CHOKEPOINT, 0, 6));

    // Lumber Mill high ground (sniper position)
    positions.push_back(BGPositionData("LM Overlook", 850.0f, 1140.0f, 15.0f, 0.0f,
        BGPositionData::PositionType::SNIPER_POSITION, 0, 8));

    return positions;
}

std::vector<BGPositionData> ArathiBasinScript::GetGraveyardPositions(uint32 faction) const
{
    std::vector<BGPositionData> graveyards;

    // In AB, graveyards are at controlled nodes
    for (uint32 i = 0; i < ArathiBasin::NODE_COUNT; ++i)
    {
        Position gy = ArathiBasin::GetNodeGraveyard(i);
        if (gy.GetPositionX() != 0)  // Valid position
        {
            std::string name = std::string(ArathiBasin::GetNodeName(i)) + " Graveyard";
            BGPositionData p(name, gy.GetPositionX(), gy.GetPositionY(), gy.GetPositionZ(),
                gy.GetOrientation(), BGPositionData::PositionType::GRAVEYARD, 0, 6);
            graveyards.push_back(p);
        }
    }

    return graveyards;
}

std::vector<BGWorldState> ArathiBasinScript::GetInitialWorldStates() const
{
    std::vector<BGWorldState> states;

    // Resource scores
    states.push_back(BGWorldState(ArathiBasin::WorldStates::RESOURCES_ALLY,
        "Alliance Resources", BGWorldState::StateType::SCORE_ALLIANCE, 0));
    states.push_back(BGWorldState(ArathiBasin::WorldStates::RESOURCES_HORDE,
        "Horde Resources", BGWorldState::StateType::SCORE_HORDE, 0));

    // Max score
    states.push_back(BGWorldState(ArathiBasin::WorldStates::MAX_RESOURCES,
        "Max Resources", BGWorldState::StateType::CUSTOM, ArathiBasin::MAX_SCORE));

    // Node states (neutral initially)
    states.push_back(BGWorldState(ArathiBasin::WorldStates::STABLES_ICON,
        "Stables", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(ArathiBasin::WorldStates::BLACKSMITH_ICON,
        "Blacksmith", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(ArathiBasin::WorldStates::FARM_ICON,
        "Farm", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(ArathiBasin::WorldStates::GOLD_MINE_ICON,
        "Gold Mine", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(ArathiBasin::WorldStates::LUMBER_MILL_ICON,
        "Lumber Mill", BGWorldState::StateType::OBJECTIVE_STATE, 0));

    return states;
}

std::vector<uint32> ArathiBasinScript::GetTickPointsTable() const
{
    return std::vector<uint32>(
        ArathiBasin::TICK_POINTS.begin(),
        ArathiBasin::TICK_POINTS.end()
    );
}

// ============================================================================
// WORLD STATE
// ============================================================================

bool ArathiBasinScript::InterpretWorldState(int32 stateId, int32 value,
    uint32& outObjectiveId, ObjectiveState& outState) const
{
    // Try cached mappings
    if (TryInterpretFromCache(stateId, value, outObjectiveId, outState))
        return true;

    // Manual interpretation for complex states
    // Stables
    if (stateId == ArathiBasin::WorldStates::STABLES_ALLIANCE_CONTROLLED && value)
    {
        outObjectiveId = ArathiBasin::Nodes::STABLES;
        outState = ObjectiveState::ALLIANCE_CONTROLLED;
        return true;
    }
    if (stateId == ArathiBasin::WorldStates::STABLES_HORDE_CONTROLLED && value)
    {
        outObjectiveId = ArathiBasin::Nodes::STABLES;
        outState = ObjectiveState::HORDE_CONTROLLED;
        return true;
    }

    // Blacksmith
    if (stateId == ArathiBasin::WorldStates::BLACKSMITH_ALLIANCE_CONTROLLED && value)
    {
        outObjectiveId = ArathiBasin::Nodes::BLACKSMITH;
        outState = ObjectiveState::ALLIANCE_CONTROLLED;
        return true;
    }
    if (stateId == ArathiBasin::WorldStates::BLACKSMITH_HORDE_CONTROLLED && value)
    {
        outObjectiveId = ArathiBasin::Nodes::BLACKSMITH;
        outState = ObjectiveState::HORDE_CONTROLLED;
        return true;
    }

    // Farm, Gold Mine, Lumber Mill - similar pattern
    // (abbreviated for space)

    return false;
}

void ArathiBasinScript::GetScoreFromWorldStates(const std::map<int32, int32>& states,
    uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = 0;
    hordeScore = 0;

    auto allyIt = states.find(ArathiBasin::WorldStates::RESOURCES_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(ArathiBasin::WorldStates::RESOURCES_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

// ============================================================================
// STRATEGY - AB SPECIFIC
// ============================================================================

void ArathiBasinScript::AdjustStrategy(StrategicDecision& decision,
    float scoreAdvantage, uint32 controlledCount,
    uint32 totalObjectives, uint32 timeRemaining) const
{
    // Get base domination strategy
    DominationScriptBase::AdjustStrategy(decision, scoreAdvantage,
        controlledCount, totalObjectives, timeRemaining);

    // AB-specific adjustments

    // Opening phase - prioritize home bases
    if (GetElapsedTime() < 60000)  // First minute
    {
        uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

        decision.attackObjectives.clear();
        decision.attackObjectives = Get3CapStrategy(faction);

        decision.reasoning = "Opening rush - secure home bases first";
        decision.offenseAllocation = 80;
        decision.defenseAllocation = 20;
        return;
    }

    // Blacksmith priority
    if (IsBlacksmithCritical())
    {
        // If we don't have BS, prioritize it
        auto bsIt = m_nodeStates.find(ArathiBasin::Nodes::BLACKSMITH);
        if (bsIt != m_nodeStates.end())
        {
            uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
            bool weControlBS =
                (faction == ALLIANCE && bsIt->second == ObjectiveState::ALLIANCE_CONTROLLED) ||
                (faction == HORDE && bsIt->second == ObjectiveState::HORDE_CONTROLLED);

            if (!weControlBS)
            {
                // Insert BS at front of attack priorities
                auto it = std::find(decision.attackObjectives.begin(),
                    decision.attackObjectives.end(), ArathiBasin::Nodes::BLACKSMITH);
                if (it != decision.attackObjectives.end())
                    decision.attackObjectives.erase(it);
                decision.attackObjectives.insert(decision.attackObjectives.begin(),
                    ArathiBasin::Nodes::BLACKSMITH);

                decision.reasoning += " (Blacksmith is critical)";
            }
        }
    }

    // 3-cap strategy if we have 3 nodes
    if (controlledCount == 3)
    {
        decision.strategy = BGStrategy::DEFENSIVE;
        decision.reasoning = "3-cap achieved - defend and tick to victory";
        decision.defenseAllocation = 70;
        decision.offenseAllocation = 30;
    }

    // 4-cap aggressive if comfortable lead
    if (controlledCount >= 4 && scoreAdvantage > 0.2f)
    {
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.reasoning = "Strong position - push for 5-cap";
        decision.offenseAllocation = 60;
    }
}

uint8 ArathiBasinScript::GetObjectiveAttackPriority(uint32 objectiveId,
    ObjectiveState state, uint32 faction) const
{
    uint8 basePriority = DominationScriptBase::GetObjectiveAttackPriority(
        objectiveId, state, faction);

    // Blacksmith is always high priority
    if (objectiveId == ArathiBasin::Nodes::BLACKSMITH)
        return std::min(static_cast<uint8>(10), static_cast<uint8>(basePriority + 2));

    // Home bases slightly higher priority at game start
    if (GetElapsedTime() < 120000)  // First 2 minutes
    {
        if (faction == ALLIANCE &&
            (objectiveId == ArathiBasin::Nodes::STABLES ||
             objectiveId == ArathiBasin::Nodes::LUMBER_MILL))
            return std::min(static_cast<uint8>(10), static_cast<uint8>(basePriority + 1));

        if (faction == HORDE &&
            (objectiveId == ArathiBasin::Nodes::FARM ||
             objectiveId == ArathiBasin::Nodes::GOLD_MINE))
            return std::min(static_cast<uint8>(10), static_cast<uint8>(basePriority + 1));
    }

    return basePriority;
}

uint8 ArathiBasinScript::GetObjectiveDefensePriority(uint32 objectiveId,
    ObjectiveState state, uint32 faction) const
{
    uint8 basePriority = DominationScriptBase::GetObjectiveDefensePriority(
        objectiveId, state, faction);

    // Blacksmith defense is critical if controlled
    if (objectiveId == ArathiBasin::Nodes::BLACKSMITH &&
        ((faction == ALLIANCE && state == ObjectiveState::ALLIANCE_CONTROLLED) ||
         (faction == HORDE && state == ObjectiveState::HORDE_CONTROLLED)))
    {
        return std::min(static_cast<uint8>(10), static_cast<uint8>(basePriority + 2));
    }

    // Lumber Mill defense bonus (high ground advantage)
    if (objectiveId == ArathiBasin::Nodes::LUMBER_MILL)
        return std::min(static_cast<uint8>(10), static_cast<uint8>(basePriority + 1));

    return basePriority;
}

// ============================================================================
// AB-SPECIFIC HELPERS
// ============================================================================

std::vector<uint32> ArathiBasinScript::Get3CapStrategy(uint32 faction) const
{
    if (faction == ALLIANCE)
    {
        // Alliance typically takes Stables, Blacksmith, Lumber Mill
        return {
            ArathiBasin::Nodes::STABLES,
            ArathiBasin::Nodes::BLACKSMITH,
            ArathiBasin::Nodes::LUMBER_MILL
        };
    }
    else
    {
        // Horde typically takes Farm, Blacksmith, Gold Mine
        return {
            ArathiBasin::Nodes::FARM,
            ArathiBasin::Nodes::BLACKSMITH,
            ArathiBasin::Nodes::GOLD_MINE
        };
    }
}

bool ArathiBasinScript::IsBlacksmithCritical() const
{
    // Blacksmith is always critical in AB
    // It controls map center and provides strategic advantage
    return true;
}

uint32 ArathiBasinScript::GetOpeningRushTarget(uint32 faction) const
{
    // First rush: closest home base, then BS
    if (faction == ALLIANCE)
        return ArathiBasin::Nodes::STABLES;
    else
        return ArathiBasin::Nodes::FARM;
}

float ArathiBasinScript::GetDistanceFromSpawn(uint32 nodeId, uint32 faction) const
{
    Position nodePos = ArathiBasin::GetNodePosition(nodeId);
    Position spawnPos;

    if (faction == ALLIANCE)
        spawnPos = ArathiBasin::ALLIANCE_SPAWNS[0];
    else
        spawnPos = ArathiBasin::HORDE_SPAWNS[0];

    return CalculateDistance(spawnPos.GetPositionX(), spawnPos.GetPositionY(), spawnPos.GetPositionZ(),
        nodePos.GetPositionX(), nodePos.GetPositionY(), nodePos.GetPositionZ());
}

} // namespace Playerbot::Coordination::Battleground
