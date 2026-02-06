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

#include "DominationScriptBase.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>
#include <cmath>

namespace Playerbot::Coordination::Battleground
{

// ============================================================================
// LIFECYCLE
// ============================================================================

void DominationScriptBase::OnLoad(BattlegroundCoordinator* coordinator)
{
    BGScriptBase::OnLoad(coordinator);

    // Clear node tracking maps - will be populated by InitializeNodeTracking()
    m_nodeStates.clear();
    m_nodeCaptureProgress.clear();
    m_nodeLastContestTime.clear();

    // Reset tracking
    m_allianceScore = 0;
    m_hordeScore = 0;
    m_lastTickTime = 0;

    m_allianceNodes = 0;
    m_hordeNodes = 0;
    m_contestedNodes = 0;
    m_neutralNodes = 0;

    m_allianceResourceRate = 0.0f;
    m_hordeResourceRate = 0.0f;
    m_projectedAllianceWinTime = 0;
    m_projectedHordeWinTime = 0;

    m_nodeUpdateTimer = 0;
    m_strategyUpdateTimer = 0;

    // NOTE: Derived classes MUST call InitializeNodeTracking() from their OnLoad()
    // after calling DominationScriptBase::OnLoad(). We deliberately do NOT call
    // GetNodeCount()/GetNodeData() (virtual) here to avoid a vtable slot dispatch
    // issue observed with MSVC RelWithDebInfo where GetNodeCount() (slot N) can
    // dispatch to GetObjectivePath() (slot N-1) due to stale object files.
}

void DominationScriptBase::InitializeNodeTracking()
{
    uint32 nodeCount = GetNodeCount();
    for (uint32 i = 0; i < nodeCount; ++i)
    {
        BGObjectiveData nodeData = GetNodeData(i);
        m_nodeStates[nodeData.id] = BGObjectiveState::NEUTRAL;
        m_nodeCaptureProgress[nodeData.id] = 0.0f;
        m_nodeLastContestTime[nodeData.id] = 0;
    }

    m_neutralNodes = nodeCount;

    TC_LOG_DEBUG("playerbots.bg.script",
        "DominationScriptBase: Initialized with {} nodes for {}",
        nodeCount, GetName());
}

void DominationScriptBase::OnUpdate(uint32 diff)
{
    BGScriptBase::OnUpdate(diff);

    if (!IsMatchActive())
        return;

    // Periodic node state check
    m_nodeUpdateTimer += diff;
    if (m_nodeUpdateTimer >= NODE_UPDATE_INTERVAL)
    {
        m_nodeUpdateTimer = 0;
        UpdateNodeCounts();
        RecalculateResourceRates();
    }

    // Periodic strategy update
    m_strategyUpdateTimer += diff;
    if (m_strategyUpdateTimer >= STRATEGY_UPDATE_INTERVAL)
    {
        m_strategyUpdateTimer = 0;
        UpdateProjectedWinTimes();
    }
}

// ============================================================================
// STRATEGY - DOMINATION OVERRIDES
// ============================================================================

RoleDistribution DominationScriptBase::GetRecommendedRoles(
    const StrategicDecision& decision,
    float scoreAdvantage,
    uint32 timeRemaining) const
{
    uint32 nodeCount = GetNodeCount();
    uint8 teamSize = GetTeamSize();

    RoleDistribution dist = CreateDominationRoleDistribution(
        decision, nodeCount, teamSize);

    // Adjust based on node control
    uint32 ourNodes = (m_coordinator && m_coordinator->GetFaction() == ALLIANCE) ?
        m_allianceNodes : m_hordeNodes;
    uint32 theirNodes = (m_coordinator && m_coordinator->GetFaction() == ALLIANCE) ?
        m_hordeNodes : m_allianceNodes;

    // If we control majority, shift to defense
    if (ourNodes > theirNodes && ourNodes >= (nodeCount + 1) / 2)
    {
        dist.SetRole(BGRole::NODE_DEFENDER,
            std::min(static_cast<uint8>(teamSize - 2), static_cast<uint8>(ourNodes * 2)),
            teamSize - 2);
        dist.SetRole(BGRole::NODE_ATTACKER, 2, teamSize / 3);
        dist.reasoning = "Control majority - defend and consolidate";
    }
    // If we control minority, need to push
    else if (ourNodes < theirNodes)
    {
        dist.SetRole(BGRole::NODE_ATTACKER, teamSize / 2, teamSize - 2);
        dist.SetRole(BGRole::NODE_DEFENDER,
            std::max(static_cast<uint8>(1), static_cast<uint8>(ourNodes)),
            ourNodes * 2);
        dist.reasoning = "Control minority - aggressive capture";
    }

    // Time pressure adjustments
    if (timeRemaining < 180000) // Less than 3 minutes
    {
        if (scoreAdvantage > 0.1f)
        {
            // Winning - turtle
            uint8 defenders = static_cast<uint8>(teamSize * 0.7f);
            dist.SetRole(BGRole::NODE_DEFENDER, defenders, teamSize - 1);
            dist.reasoning += " (late-game turtle)";
        }
        else if (scoreAdvantage < -0.1f)
        {
            // Losing - all attack
            uint8 attackers = static_cast<uint8>(teamSize * 0.8f);
            dist.SetRole(BGRole::NODE_ATTACKER, attackers, teamSize - 1);
            dist.reasoning += " (late-game push)";
        }
    }

    return dist;
}

void DominationScriptBase::AdjustStrategy(StrategicDecision& decision,
    float scoreAdvantage, uint32 controlledCount,
    uint32 totalObjectives, uint32 timeRemaining) const
{
    // Calculate control ratio
    float controlRatio = totalObjectives > 0 ?
        static_cast<float>(controlledCount) / totalObjectives : 0.5f;

    // Get optimal node count for this BG
    uint32 optimalNodes = GetOptimalNodeCount();
    bool haveOptimal = controlledCount >= optimalNodes;

    // Time factor (0.0 = start, 1.0 = end)
    float timeFactor = 1.0f - (static_cast<float>(timeRemaining) / GetMaxDuration());
    bool timeCritical = timeFactor > 0.85f;  // Last 15%
    bool lateGame = timeFactor > 0.67f;      // Last third

    // Strategy selection based on position
    if (scoreAdvantage > DominationConstants::COMFORTABLE_LEAD)
    {
        // Comfortable lead
        if (haveOptimal)
        {
            decision.strategy = lateGame ? BGStrategy::TURTLE : BGStrategy::DEFENSIVE;
            decision.reasoning = "Comfortable lead with control - defend nodes";
            decision.defenseAllocation = timeCritical ? 80 : 65;
        }
        else
        {
            decision.strategy = BGStrategy::BALANCED;
            decision.reasoning = "Comfortable lead but need more control";
            decision.defenseAllocation = 50;
        }
    }
    else if (scoreAdvantage < DominationConstants::CRITICAL_DEFICIT)
    {
        // Critical deficit
        decision.strategy = timeCritical ? BGStrategy::ALL_IN : BGStrategy::AGGRESSIVE;
        decision.reasoning = "Critical deficit - must capture nodes immediately";
        decision.offenseAllocation = 85;
        decision.defenseAllocation = 15;

        // Identify attack priorities
        decision.attackObjectives = GetAttackPriorityOrder();
        // Only keep first 2-3 priorities
        if (decision.attackObjectives.size() > 3)
            decision.attackObjectives.resize(3);
    }
    else if (scoreAdvantage < DominationConstants::DANGEROUS_DEFICIT)
    {
        // Dangerous deficit
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.reasoning = "Behind on score - aggressive node capture";
        decision.offenseAllocation = 70;
        decision.defenseAllocation = 30;
    }
    else
    {
        // Close game
        if (controlRatio > 0.5f)
        {
            decision.strategy = BGStrategy::DEFENSIVE;
            decision.reasoning = "Close game with control advantage - hold";
            decision.defenseAllocation = 55;
            decision.defendObjectives = GetDefensePriorityOrder();
        }
        else if (controlRatio < 0.5f)
        {
            decision.strategy = BGStrategy::AGGRESSIVE;
            decision.reasoning = "Close game without control - need nodes";
            decision.offenseAllocation = 60;
            decision.attackObjectives = GetAttackPriorityOrder();
        }
        else
        {
            decision.strategy = BGStrategy::BALANCED;
            decision.reasoning = "Even game - flexible response";
            decision.offenseAllocation = 50;
            decision.defenseAllocation = 50;
        }
    }

    // Calculate confidence based on position clarity
    decision.confidence = 0.5f + std::abs(scoreAdvantage) * 0.3f + (controlRatio - 0.5f) * 0.2f;
    decision.confidence = std::clamp(decision.confidence, 0.3f, 0.95f);
}

uint8 DominationScriptBase::GetObjectiveAttackPriority(uint32 objectiveId,
    BGObjectiveState state, uint32 faction) const
{
    // Base priority calculation
    uint8 basePriority = BGScriptBase::GetObjectiveAttackPriority(objectiveId, state, faction);

    // Adjust based on strategic value
    uint8 strategicValue = CalculateNodeStrategicValue(objectiveId);

    // Neutral nodes are high priority
    if (state == BGObjectiveState::NEUTRAL)
        return std::min(static_cast<uint8>(10), static_cast<uint8>(basePriority + strategicValue / 2));

    // Contested enemy nodes are very high priority (finish the capture)
    if ((faction == ALLIANCE && state == BGObjectiveState::ALLIANCE_CONTESTED) ||
        (faction == HORDE && state == BGObjectiveState::HORDE_CONTESTED))
        return DominationConstants::CRITICAL_PRIORITY;

    // Enemy controlled with high strategic value
    if (state == BGObjectiveState::ALLIANCE_CONTROLLED && faction == HORDE)
        return std::min(static_cast<uint8>(8), static_cast<uint8>(basePriority + strategicValue / 3));
    if (state == BGObjectiveState::HORDE_CONTROLLED && faction == ALLIANCE)
        return std::min(static_cast<uint8>(8), static_cast<uint8>(basePriority + strategicValue / 3));

    return basePriority;
}

uint8 DominationScriptBase::GetObjectiveDefensePriority(uint32 objectiveId,
    BGObjectiveState state, uint32 faction) const
{
    uint8 basePriority = BGScriptBase::GetObjectiveDefensePriority(objectiveId, state, faction);
    uint8 strategicValue = CalculateNodeStrategicValue(objectiveId);

    // Our contested nodes are critical
    if ((faction == ALLIANCE && state == BGObjectiveState::ALLIANCE_CONTESTED) ||
        (faction == HORDE && state == BGObjectiveState::HORDE_CONTESTED))
        return DominationConstants::CRITICAL_PRIORITY;

    // Our controlled high-value nodes
    if ((faction == ALLIANCE && state == BGObjectiveState::ALLIANCE_CONTROLLED) ||
        (faction == HORDE && state == BGObjectiveState::HORDE_CONTROLLED))
    {
        return std::min(static_cast<uint8>(8), static_cast<uint8>(basePriority + strategicValue / 3));
    }

    return basePriority;
}

float DominationScriptBase::CalculateWinProbability(uint32 allianceScore, uint32 hordeScore,
    uint32 timeRemaining, uint32 objectivesControlled, uint32 faction) const
{
    uint32 maxScore = GetMaxScore();
    if (maxScore == 0)
        return 0.5f;

    uint32 ourScore = (faction == ALLIANCE) ? allianceScore : hordeScore;
    uint32 theirScore = (faction == ALLIANCE) ? hordeScore : allianceScore;
    uint32 ourNodes = (faction == ALLIANCE) ? m_allianceNodes : m_hordeNodes;
    uint32 theirNodes = (faction == ALLIANCE) ? m_hordeNodes : m_allianceNodes;

    // Can we win with current control?
    if (CanWinWithCurrentControl(ourScore, theirScore, ourNodes, theirNodes, timeRemaining))
    {
        // Calculate by how much we're winning
        uint32 ourWinTime = EstimateTimeToScore(ourScore, maxScore, ourNodes);
        uint32 theirWinTime = EstimateTimeToScore(theirScore, maxScore, theirNodes);

        if (ourWinTime < theirWinTime)
        {
            float advantage = static_cast<float>(theirWinTime - ourWinTime) /
                std::max(ourWinTime, 1u);
            return std::clamp(0.5f + advantage * 0.3f, 0.55f, 0.95f);
        }
        else
        {
            float disadvantage = static_cast<float>(ourWinTime - theirWinTime) /
                std::max(theirWinTime, 1u);
            return std::clamp(0.5f - disadvantage * 0.3f, 0.05f, 0.45f);
        }
    }

    // Need to gain control - lower probability
    uint32 nodesNeeded = CalculateMinNodesNeeded(ourScore, theirScore, timeRemaining);
    if (nodesNeeded > GetNodeCount())
        return 0.1f;  // Very unlikely

    float difficulty = static_cast<float>(nodesNeeded) / GetNodeCount();
    return std::clamp(0.5f - difficulty * 0.3f, 0.1f, 0.5f);
}

// ============================================================================
// DOMINATION-SPECIFIC IMPLEMENTATIONS
// ============================================================================

uint32 DominationScriptBase::GetTickPoints(uint32 nodeCount) const
{
    auto tickTable = GetTickPointsTable();
    if (nodeCount >= tickTable.size())
        return tickTable.empty() ? 0 : tickTable.back();
    return tickTable[nodeCount];
}

uint32 DominationScriptBase::GetOptimalNodeCount() const
{
    // Default: majority + 1 for comfortable lead
    return (GetNodeCount() / 2) + 1;
}

// ============================================================================
// EVENT HANDLING
// ============================================================================

void DominationScriptBase::OnEvent(const BGScriptEventData& event)
{
    BGScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::OBJECTIVE_CAPTURED:
            if (m_nodeStates.find(event.objectiveId) != m_nodeStates.end())
            {
                m_nodeStates[event.objectiveId] = event.newState;
                m_nodeCaptureProgress[event.objectiveId] =
                    (event.newState == BGObjectiveState::ALLIANCE_CONTROLLED ||
                     event.newState == BGObjectiveState::HORDE_CONTROLLED) ? 1.0f : 0.0f;
                UpdateNodeCounts();

                TC_LOG_DEBUG("playerbots.bg.script",
                    "Domination: Node {} captured by {} (A:{} H:{} C:{} N:{})",
                    event.objectiveId, event.faction == ALLIANCE ? "Alliance" : "Horde",
                    m_allianceNodes, m_hordeNodes, m_contestedNodes, m_neutralNodes);
            }
            break;

        case BGScriptEvent::OBJECTIVE_LOST:
            if (m_nodeStates.find(event.objectiveId) != m_nodeStates.end())
            {
                m_nodeStates[event.objectiveId] = event.newState;
                UpdateNodeCounts();
            }
            break;

        case BGScriptEvent::OBJECTIVE_CONTESTED:
            if (m_nodeStates.find(event.objectiveId) != m_nodeStates.end())
            {
                m_nodeStates[event.objectiveId] = event.newState;
                m_nodeLastContestTime[event.objectiveId] = getMSTime();
                UpdateNodeCounts();

                TC_LOG_DEBUG("playerbots.bg.script",
                    "Domination: Node {} contested!",
                    event.objectiveId);
            }
            break;

        case BGScriptEvent::WORLD_STATE_CHANGED:
            // Could be score update
            break;

        default:
            break;
    }
}

void DominationScriptBase::OnMatchStart()
{
    BGScriptBase::OnMatchStart();

    // Reset all nodes to neutral
    for (auto& [nodeId, state] : m_nodeStates)
    {
        state = BGObjectiveState::NEUTRAL;
        m_nodeCaptureProgress[nodeId] = 0.0f;
        m_nodeLastContestTime[nodeId] = 0;
    }

    m_allianceScore = 0;
    m_hordeScore = 0;
    m_lastTickTime = getMSTime();

    UpdateNodeCounts();

    TC_LOG_DEBUG("playerbots.bg.script",
        "Domination: Match started with {} nodes",
        GetNodeCount());
}

// ============================================================================
// DOMINATION HELPERS
// ============================================================================

float DominationScriptBase::CalculateResourceRate(uint32 controlledNodes) const
{
    uint32 tickPoints = GetTickPoints(controlledNodes);
    uint32 tickInterval = GetTickInterval();

    if (tickInterval == 0)
        return 0.0f;

    // Points per second
    return static_cast<float>(tickPoints) / (tickInterval / 1000.0f);
}

uint32 DominationScriptBase::EstimateTimeToScore(uint32 currentScore, uint32 targetScore,
    uint32 controlledNodes) const
{
    if (currentScore >= targetScore)
        return 0;

    float rate = CalculateResourceRate(controlledNodes);
    if (rate <= 0.0f)
        return std::numeric_limits<uint32>::max();

    uint32 pointsNeeded = targetScore - currentScore;
    return static_cast<uint32>((pointsNeeded / rate) * 1000.0f);
}

uint32 DominationScriptBase::CalculateMinNodesNeeded(uint32 ourScore, uint32 theirScore,
    uint32 timeRemaining) const
{
    if (ourScore >= GetMaxScore())
        return 0;

    // Try each node count to find minimum needed
    for (uint32 nodes = 1; nodes <= GetNodeCount(); ++nodes)
    {
        uint32 ourTime = EstimateTimeToScore(ourScore, GetMaxScore(), nodes);
        if (ourTime <= timeRemaining)
            return nodes;
    }

    return GetNodeCount() + 1;  // Can't win
}

bool DominationScriptBase::CanWinWithCurrentControl(uint32 ourScore, uint32 theirScore,
    uint32 ourNodes, uint32 theirNodes, uint32 timeRemaining) const
{
    uint32 maxScore = GetMaxScore();

    if (ourScore >= maxScore)
        return true;

    uint32 ourWinTime = EstimateTimeToScore(ourScore, maxScore, ourNodes);
    uint32 theirWinTime = EstimateTimeToScore(theirScore, maxScore, theirNodes);

    // We win if we finish before them or before time runs out
    return ourWinTime <= timeRemaining && ourWinTime < theirWinTime;
}

uint8 DominationScriptBase::GetRecommendedDefenders(uint32 /*nodeId*/, uint8 threat) const
{
    if (threat >= 4)
        return DominationConstants::MAX_DEFENDERS;
    if (threat >= 2)
        return DominationConstants::NORMAL_DEFENDERS;
    return DominationConstants::MIN_DEFENDERS;
}

uint8 DominationScriptBase::GetRecommendedAttackers(uint32 /*nodeId*/, uint8 defenderCount) const
{
    // Need numerical advantage to capture
    return std::max(DominationConstants::MIN_ATTACKERS,
        static_cast<uint8>(defenderCount + 2));
}

uint32 DominationScriptBase::FindWeakestEnemyNode() const
{
    uint32 weakestId = 0;
    uint8 lowestPriority = DominationConstants::CRITICAL_PRIORITY;

    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

    for (const auto& [nodeId, state] : m_nodeStates)
    {
        if ((faction == ALLIANCE && state == BGObjectiveState::HORDE_CONTROLLED) ||
            (faction == HORDE && state == BGObjectiveState::ALLIANCE_CONTROLLED))
        {
            uint8 priority = GetObjectiveDefensePriority(nodeId, state, faction);
            if (priority < lowestPriority)
            {
                lowestPriority = priority;
                weakestId = nodeId;
            }
        }
    }

    return weakestId;
}

uint32 DominationScriptBase::FindMostThreatenedFriendlyNode() const
{
    uint32 threatenedId = 0;
    uint32 mostRecentContest = 0;

    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

    for (const auto& [nodeId, state] : m_nodeStates)
    {
        bool isFriendly =
            (faction == ALLIANCE && (state == BGObjectiveState::ALLIANCE_CONTROLLED ||
                                     state == BGObjectiveState::ALLIANCE_CONTESTED)) ||
            (faction == HORDE && (state == BGObjectiveState::HORDE_CONTROLLED ||
                                  state == BGObjectiveState::HORDE_CONTESTED));

        if (isFriendly)
        {
            auto contestIt = m_nodeLastContestTime.find(nodeId);
            if (contestIt != m_nodeLastContestTime.end() &&
                contestIt->second > mostRecentContest)
            {
                mostRecentContest = contestIt->second;
                threatenedId = nodeId;
            }
        }
    }

    return threatenedId;
}

uint8 DominationScriptBase::CalculateNodeStrategicValue(uint32 nodeId) const
{
    // Default implementation - look up from cached objectives
    for (const auto& obj : m_cachedObjectives)
    {
        if (obj.id == nodeId)
            return obj.strategicValue;
    }
    return 5;  // Default mid-value
}

std::vector<uint32> DominationScriptBase::GetAttackPriorityOrder() const
{
    std::vector<std::pair<uint32, uint8>> nodePriorities;
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

    for (const auto& [nodeId, state] : m_nodeStates)
    {
        uint8 priority = GetObjectiveAttackPriority(nodeId, state, faction);
        if (priority > 0)
            nodePriorities.emplace_back(nodeId, priority);
    }

    // Sort by priority descending
    std::sort(nodePriorities.begin(), nodePriorities.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<uint32> result;
    result.reserve(nodePriorities.size());
    for (const auto& [nodeId, priority] : nodePriorities)
        result.push_back(nodeId);

    return result;
}

std::vector<uint32> DominationScriptBase::GetDefensePriorityOrder() const
{
    std::vector<std::pair<uint32, uint8>> nodePriorities;
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

    for (const auto& [nodeId, state] : m_nodeStates)
    {
        uint8 priority = GetObjectiveDefensePriority(nodeId, state, faction);
        if (priority > 0)
            nodePriorities.emplace_back(nodeId, priority);
    }

    std::sort(nodePriorities.begin(), nodePriorities.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<uint32> result;
    result.reserve(nodePriorities.size());
    for (const auto& [nodeId, priority] : nodePriorities)
        result.push_back(nodeId);

    return result;
}

void DominationScriptBase::UpdateNodeCounts()
{
    m_allianceNodes = 0;
    m_hordeNodes = 0;
    m_contestedNodes = 0;
    m_neutralNodes = 0;

    for (const auto& [nodeId, state] : m_nodeStates)
    {
        switch (state)
        {
            case BGObjectiveState::ALLIANCE_CONTROLLED:
                ++m_allianceNodes;
                break;
            case BGObjectiveState::HORDE_CONTROLLED:
                ++m_hordeNodes;
                break;
            case BGObjectiveState::ALLIANCE_CONTESTED:
            case BGObjectiveState::HORDE_CONTESTED:
                ++m_contestedNodes;
                break;
            case BGObjectiveState::NEUTRAL:
            default:
                ++m_neutralNodes;
                break;
        }
    }
}

void DominationScriptBase::UpdateProjectedWinTimes()
{
    m_projectedAllianceWinTime = EstimateTimeToScore(m_allianceScore, GetMaxScore(), m_allianceNodes);
    m_projectedHordeWinTime = EstimateTimeToScore(m_hordeScore, GetMaxScore(), m_hordeNodes);
}

void DominationScriptBase::RecalculateResourceRates()
{
    m_allianceResourceRate = CalculateResourceRate(m_allianceNodes);
    m_hordeResourceRate = CalculateResourceRate(m_hordeNodes);
}

} // namespace Playerbot::Coordination::Battleground
