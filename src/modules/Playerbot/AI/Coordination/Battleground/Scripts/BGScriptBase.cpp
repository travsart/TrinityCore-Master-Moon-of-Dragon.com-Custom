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

#include "BGScriptBase.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"
#include "Timer.h"
#include <cmath>

namespace Playerbot::Coordination::Battleground
{

// ============================================================================
// LIFECYCLE
// ============================================================================

void BGScriptBase::OnLoad(BattlegroundCoordinator* coordinator)
{
    m_coordinator = coordinator;
    m_matchActive = false;
    m_matchStartTime = 0;

    // Reset counters
    m_objectivesCaptured = 0;
    m_objectivesLost = 0;
    m_flagCaptures = 0;
    m_playerKills = 0;
    m_playerDeaths = 0;

    // Clear caches
    m_cachedObjectives.clear();
    m_cachedPositions.clear();
    m_cachedWorldStates.clear();
    m_worldStateMappings.clear();

    TC_LOG_DEBUG("playerbots.bg.script", "BGScriptBase: Script loaded for {} (Map {})",
        GetName(), GetMapId());
}

void BGScriptBase::OnUnload()
{
    TC_LOG_DEBUG("playerbots.bg.script", "BGScriptBase: Script unloaded for {}",
        GetName());

    m_coordinator = nullptr;
    m_matchActive = false;
}

void BGScriptBase::OnUpdate(uint32 /*diff*/)
{
    // Default implementation does nothing
    // Derived classes override for periodic updates
}

// ============================================================================
// STRATEGY - DEFAULT IMPLEMENTATIONS
// ============================================================================

RoleDistribution BGScriptBase::GetRecommendedRoles(
    const StrategicDecision& decision,
    float scoreAdvantage,
    uint32 timeRemaining) const
{
    RoleDistribution dist;
    uint8 teamSize = GetTeamSize();

    // Default balanced distribution
    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
        case BGStrategy::ALL_IN:
            // Heavy offense
            dist.SetRole(BGRole::NODE_ATTACKER, teamSize / 2, teamSize - 2);
            dist.SetRole(BGRole::NODE_DEFENDER, 2, teamSize / 4);
            dist.SetRole(BGRole::HEALER_OFFENSE, 2, 4);
            dist.SetRole(BGRole::HEALER_DEFENSE, 1, 2);
            dist.SetRole(BGRole::ROAMER, 1, 3);
            dist.reasoning = "Aggressive push - maximize attackers";
            break;

        case BGStrategy::DEFENSIVE:
        case BGStrategy::TURTLE:
            // Heavy defense
            dist.SetRole(BGRole::NODE_ATTACKER, 2, teamSize / 4);
            dist.SetRole(BGRole::NODE_DEFENDER, teamSize / 2, teamSize - 2);
            dist.SetRole(BGRole::HEALER_OFFENSE, 1, 2);
            dist.SetRole(BGRole::HEALER_DEFENSE, 2, 4);
            dist.SetRole(BGRole::ROAMER, 1, 2);
            dist.reasoning = "Defensive hold - protect objectives";
            break;

        case BGStrategy::STALL:
            // Minimal engagement, defense focus
            dist.SetRole(BGRole::NODE_ATTACKER, 1, 2);
            dist.SetRole(BGRole::NODE_DEFENDER, teamSize / 2 + 1, teamSize - 1);
            dist.SetRole(BGRole::HEALER_OFFENSE, 1, 2);
            dist.SetRole(BGRole::HEALER_DEFENSE, 2, 4);
            dist.SetRole(BGRole::ROAMER, 2, 4);
            dist.reasoning = "Stalling - delay with strong defense";
            break;

        case BGStrategy::COMEBACK:
            // Aggressive with risk
            dist.SetRole(BGRole::NODE_ATTACKER, teamSize / 2 + 1, teamSize - 1);
            dist.SetRole(BGRole::NODE_DEFENDER, 1, 3);
            dist.SetRole(BGRole::HEALER_OFFENSE, 2, 4);
            dist.SetRole(BGRole::HEALER_DEFENSE, 1, 2);
            dist.SetRole(BGRole::ROAMER, 2, 4);
            dist.reasoning = "Comeback attempt - high risk offense";
            break;

        case BGStrategy::BALANCED:
        default:
            // Even split
            dist.SetRole(BGRole::NODE_ATTACKER, teamSize / 3, teamSize / 2);
            dist.SetRole(BGRole::NODE_DEFENDER, teamSize / 3, teamSize / 2);
            dist.SetRole(BGRole::HEALER_OFFENSE, 1, 3);
            dist.SetRole(BGRole::HEALER_DEFENSE, 1, 3);
            dist.SetRole(BGRole::ROAMER, 2, 4);
            dist.reasoning = "Balanced - flexible response";
            break;
    }

    // Adjust based on time
    if (timeRemaining < 120000) // Less than 2 minutes
    {
        if (scoreAdvantage > 0.1f)
        {
            // Winning - turtle up
            dist.SetRole(BGRole::NODE_DEFENDER,
                dist.GetCount(BGRole::NODE_DEFENDER) + 2,
                dist.GetMax(BGRole::NODE_DEFENDER) + 2);
            dist.reasoning += " (late-game defense)";
        }
        else if (scoreAdvantage < -0.1f)
        {
            // Losing - all-in attack
            dist.SetRole(BGRole::NODE_ATTACKER,
                dist.GetCount(BGRole::NODE_ATTACKER) + 2,
                dist.GetMax(BGRole::NODE_ATTACKER) + 2);
            dist.reasoning += " (late-game push)";
        }
    }

    return dist;
}

void BGScriptBase::AdjustStrategy(StrategicDecision& decision,
    float scoreAdvantage, uint32 controlledCount,
    uint32 totalObjectives, uint32 timeRemaining) const
{
    // Default strategy adjustment based on control ratio
    float controlRatio = totalObjectives > 0 ?
        static_cast<float>(controlledCount) / totalObjectives : 0.5f;

    // Time pressure adjustments
    bool timeCritical = timeRemaining < 180000; // Less than 3 minutes

    // Winning significantly
    if (scoreAdvantage > 0.3f)
    {
        if (controlRatio >= 0.5f)
        {
            decision.strategy = timeCritical ? BGStrategy::DEFENSIVE : BGStrategy::BALANCED;
            decision.reasoning = "Winning significantly - maintain control";
        }
        else
        {
            decision.strategy = BGStrategy::BALANCED;
            decision.reasoning = "Winning but weak control - solidify position";
        }
        decision.defenseAllocation = std::max(decision.defenseAllocation, static_cast<uint8>(60));
        decision.offenseAllocation = 100 - decision.defenseAllocation;
    }
    // Losing significantly
    else if (scoreAdvantage < -0.3f)
    {
        if (timeCritical)
        {
            decision.strategy = BGStrategy::ALL_IN;
            decision.reasoning = "Losing badly with time pressure - desperate push";
            decision.offenseAllocation = 85;
            decision.defenseAllocation = 15;
        }
        else
        {
            decision.strategy = BGStrategy::AGGRESSIVE;
            decision.reasoning = "Losing badly - aggressive offense needed";
            decision.offenseAllocation = 70;
            decision.defenseAllocation = 30;
        }
    }
    // Close game
    else
    {
        if (controlRatio > 0.6f)
        {
            decision.strategy = BGStrategy::DEFENSIVE;
            decision.reasoning = "Close game with control advantage - hold objectives";
            decision.defenseAllocation = 55;
            decision.offenseAllocation = 45;
        }
        else if (controlRatio < 0.4f)
        {
            decision.strategy = BGStrategy::AGGRESSIVE;
            decision.reasoning = "Close game but losing control - need to capture";
            decision.offenseAllocation = 60;
            decision.defenseAllocation = 40;
        }
        else
        {
            decision.strategy = BGStrategy::BALANCED;
            decision.reasoning = "Close game with even control - flexible approach";
            decision.offenseAllocation = 50;
            decision.defenseAllocation = 50;
        }
    }

    decision.confidence = std::min(1.0f, 0.5f + std::abs(scoreAdvantage));
}

uint8 BGScriptBase::GetObjectiveAttackPriority(uint32 objectiveId,
    BGObjectiveState state, uint32 faction) const
{
    // Default: prioritize neutral and enemy objectives
    switch (state)
    {
        case BGObjectiveState::NEUTRAL:
            return 8;  // High priority - unclaimed

        case BGObjectiveState::ALLIANCE_CONTROLLED:
            return (faction == HORDE) ? 6 : 0;
        case BGObjectiveState::HORDE_CONTROLLED:
            return (faction == ALLIANCE) ? 6 : 0;

        case BGObjectiveState::ALLIANCE_CONTESTED:
            return (faction == HORDE) ? 9 : 0;
        case BGObjectiveState::HORDE_CONTESTED:
            return (faction == ALLIANCE) ? 9 : 0;

        case BGObjectiveState::ALLIANCE_CAPTURING:
            return (faction == HORDE) ? 7 : 0;
        case BGObjectiveState::HORDE_CAPTURING:
            return (faction == ALLIANCE) ? 7 : 0;

        case BGObjectiveState::DESTROYED:
            return 0;

        default:
            return 3;
    }
}

uint8 BGScriptBase::GetObjectiveDefensePriority(uint32 objectiveId,
    BGObjectiveState state, uint32 faction) const
{
    // Default: prioritize our contested objectives
    switch (state)
    {
        case BGObjectiveState::ALLIANCE_CONTROLLED:
            return (faction == ALLIANCE) ? 5 : 0;
        case BGObjectiveState::HORDE_CONTROLLED:
            return (faction == HORDE) ? 5 : 0;

        case BGObjectiveState::ALLIANCE_CONTESTED:
            return (faction == ALLIANCE) ? 9 : 0;  // Critical - under attack!
        case BGObjectiveState::HORDE_CONTESTED:
            return (faction == HORDE) ? 9 : 0;

        case BGObjectiveState::ALLIANCE_CAPTURING:
            return (faction == ALLIANCE) ? 7 : 0;
        case BGObjectiveState::HORDE_CAPTURING:
            return (faction == HORDE) ? 7 : 0;

        default:
            return 0;
    }
}

float BGScriptBase::CalculateWinProbability(uint32 allianceScore, uint32 hordeScore,
    uint32 timeRemaining, uint32 objectivesControlled, uint32 faction) const
{
    uint32 maxScore = GetMaxScore();
    if (maxScore == 0)
        return 0.5f;

    uint32 ourScore = (faction == ALLIANCE) ? allianceScore : hordeScore;
    uint32 theirScore = (faction == ALLIANCE) ? hordeScore : allianceScore;

    // Base probability from score difference
    float scoreDiff = static_cast<float>(ourScore) - static_cast<float>(theirScore);
    float scoreProbability = 0.5f + (scoreDiff / maxScore) * 0.4f;

    // Adjust for objective control
    float controlBonus = (objectivesControlled > 2) ? 0.1f : -0.1f;

    // Adjust for time (less time = more weight on current score)
    float timeWeight = 1.0f - (static_cast<float>(timeRemaining) / GetMaxDuration());
    float finalProbability = scoreProbability + (controlBonus * (1.0f - timeWeight * 0.5f));

    return std::clamp(finalProbability, 0.05f, 0.95f);
}

BGPhaseInfo::Phase BGScriptBase::GetMatchPhase(uint32 timeRemaining,
    uint32 allianceScore, uint32 hordeScore) const
{
    uint32 maxDuration = GetMaxDuration();
    uint32 maxScore = GetMaxScore();

    // Time-based phase calculation
    float timeProgress = 1.0f - (static_cast<float>(timeRemaining) / maxDuration);

    // Score-based phase calculation
    float maxCurrentScore = static_cast<float>(std::max(allianceScore, hordeScore));
    float scoreProgress = maxScore > 0 ? maxCurrentScore / maxScore : 0.0f;

    // Use the higher progress indicator
    float progress = std::max(timeProgress, scoreProgress);

    // Opening: first 10% or first 2 minutes
    if (progress < 0.1f || timeRemaining > maxDuration - 120000)
        return BGPhaseInfo::Phase::OPENING;

    // Closing: last 60 seconds or within 10% of winning
    if (timeRemaining < 60000 || scoreProgress > 0.9f)
        return BGPhaseInfo::Phase::CLOSING;

    // Overtime: tied and time is up
    if (timeRemaining == 0 && std::abs(static_cast<int>(allianceScore) -
        static_cast<int>(hordeScore)) < maxScore * 0.05f)
        return BGPhaseInfo::Phase::OVERTIME;

    // Late game: final third
    if (progress > 0.67f)
        return BGPhaseInfo::Phase::LATE_GAME;

    // Mid game: middle third
    if (progress > 0.33f)
        return BGPhaseInfo::Phase::MID_GAME;

    // Early game: first third (but after opening)
    return BGPhaseInfo::Phase::EARLY_GAME;
}

// ============================================================================
// EVENTS - DEFAULT IMPLEMENTATIONS
// ============================================================================

void BGScriptBase::OnEvent(const BGScriptEventData& event)
{
    LogEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::MATCH_START:
            OnMatchStart();
            break;

        case BGScriptEvent::MATCH_END:
            OnMatchEnd(event.stateValue > 0);  // stateValue = victory
            break;

        case BGScriptEvent::OBJECTIVE_CAPTURED:
            ++m_objectivesCaptured;
            break;

        case BGScriptEvent::OBJECTIVE_LOST:
            ++m_objectivesLost;
            break;

        case BGScriptEvent::FLAG_CAPTURED:
            ++m_flagCaptures;
            break;

        case BGScriptEvent::PLAYER_KILLED:
            ++m_playerKills;
            break;

        case BGScriptEvent::PLAYER_DIED:
            ++m_playerDeaths;
            break;

        default:
            // Unhandled event types - derived classes handle
            break;
    }
}

void BGScriptBase::OnMatchStart()
{
    m_matchActive = true;
    m_matchStartTime = getMSTime();

    // Reset counters
    m_objectivesCaptured = 0;
    m_objectivesLost = 0;
    m_flagCaptures = 0;
    m_playerKills = 0;
    m_playerDeaths = 0;

    TC_LOG_DEBUG("playerbots.bg.script", "BGScriptBase: Match started for {}",
        GetName());
}

void BGScriptBase::OnMatchEnd(bool victory)
{
    m_matchActive = false;

    TC_LOG_DEBUG("playerbots.bg.script",
        "BGScriptBase: Match ended for {} - {} (Captures: {}, Losses: {}, Kills: {}, Deaths: {})",
        GetName(),
        victory ? "Victory" : "Defeat",
        m_objectivesCaptured,
        m_objectivesLost,
        m_playerKills,
        m_playerDeaths);
}

// ============================================================================
// UTILITY - DEFAULT IMPLEMENTATIONS
// ============================================================================

Position BGScriptBase::GetTacticalPosition(
    BGPositionData::PositionType positionType, uint32 faction) const
{
    // Search cached positions
    for (const auto& pos : m_cachedPositions)
    {
        if (pos.posType == positionType &&
            (pos.faction == 0 || pos.faction == faction))
        {
            return Position(pos.x, pos.y, pos.z, pos.orientation);
        }
    }

    // No matching position found
    return Position(0, 0, 0, 0);
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool BGScriptBase::IsMatchActive() const
{
    return m_matchActive && m_coordinator != nullptr;
}

uint32 BGScriptBase::GetElapsedTime() const
{
    if (!m_matchActive || m_matchStartTime == 0)
        return 0;

    return getMSTime() - m_matchStartTime;
}

float BGScriptBase::CalculateDistance(float x1, float y1, float z1,
                                       float x2, float y2, float z2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

float BGScriptBase::CalculateDistance2D(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx*dx + dy*dy);
}

const BGObjectiveData* BGScriptBase::FindNearestObjective(
    float x, float y, float z,
    const std::vector<BGObjectiveData>& objectives) const
{
    const BGObjectiveData* nearest = nullptr;
    float minDist = std::numeric_limits<float>::max();

    for (const auto& obj : objectives)
    {
        float dist = CalculateDistance(x, y, z, obj.x, obj.y, obj.z);
        if (dist < minDist)
        {
            minDist = dist;
            nearest = &obj;
        }
    }

    return nearest;
}

std::vector<const BGObjectiveData*> BGScriptBase::FindObjectivesByType(
    ObjectiveType type,
    const std::vector<BGObjectiveData>& objectives) const
{
    std::vector<const BGObjectiveData*> result;

    for (const auto& obj : objectives)
    {
        if (obj.type == type)
            result.push_back(&obj);
    }

    return result;
}

uint8 BGScriptBase::GetOffenseRoleCount(const StrategicDecision& decision, uint8 totalBots) const
{
    return static_cast<uint8>((totalBots * decision.offenseAllocation) / 100);
}

uint8 BGScriptBase::GetDefenseRoleCount(const StrategicDecision& decision, uint8 totalBots) const
{
    return static_cast<uint8>((totalBots * decision.defenseAllocation) / 100);
}

RoleDistribution BGScriptBase::CreateDominationRoleDistribution(
    const StrategicDecision& decision,
    uint8 nodeCount,
    uint8 teamSize) const
{
    RoleDistribution dist;

    uint8 defendersPerNode = std::max(1, static_cast<int>(teamSize) / (nodeCount + 2));
    uint8 attackers = teamSize - (defendersPerNode * nodeCount);

    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
        case BGStrategy::ALL_IN:
            dist.SetRole(BGRole::NODE_ATTACKER, attackers + 2, teamSize - 3);
            dist.SetRole(BGRole::NODE_DEFENDER, nodeCount, defendersPerNode * nodeCount);
            break;

        case BGStrategy::DEFENSIVE:
        case BGStrategy::TURTLE:
            dist.SetRole(BGRole::NODE_ATTACKER, 2, attackers);
            dist.SetRole(BGRole::NODE_DEFENDER, defendersPerNode * nodeCount + 2,
                teamSize - 2);
            break;

        default:  // BALANCED
            dist.SetRole(BGRole::NODE_ATTACKER, attackers / 2, attackers);
            dist.SetRole(BGRole::NODE_DEFENDER, defendersPerNode * nodeCount / 2,
                defendersPerNode * nodeCount + 2);
            break;
    }

    // Always have healers and roamers
    dist.SetRole(BGRole::HEALER_OFFENSE, 1, 3);
    dist.SetRole(BGRole::HEALER_DEFENSE, 1, 3);
    dist.SetRole(BGRole::ROAMER, 1, 3);

    return dist;
}

RoleDistribution BGScriptBase::CreateCTFRoleDistribution(
    const StrategicDecision& decision,
    bool weHaveFlag,
    bool theyHaveFlag,
    uint8 teamSize) const
{
    RoleDistribution dist;

    if (weHaveFlag && theyHaveFlag)
    {
        // Standoff - need both escort and hunting
        dist.SetRole(BGRole::FLAG_CARRIER, 1, 1);
        dist.SetRole(BGRole::FLAG_ESCORT, 3, 5);
        dist.SetRole(BGRole::FLAG_HUNTER, 3, 4);
        dist.SetRole(BGRole::HEALER_OFFENSE, 1, 2);
        dist.SetRole(BGRole::HEALER_DEFENSE, 1, 2);
        dist.reasoning = "Both flags taken - balanced standoff";
    }
    else if (weHaveFlag)
    {
        // We have their flag - protect FC, send few hunters
        dist.SetRole(BGRole::FLAG_CARRIER, 1, 1);
        dist.SetRole(BGRole::FLAG_ESCORT, 4, 6);
        dist.SetRole(BGRole::FLAG_HUNTER, 1, 2);
        dist.SetRole(BGRole::NODE_DEFENDER, 1, 2);  // Flag room defense
        dist.SetRole(BGRole::HEALER_DEFENSE, 2, 3);
        dist.reasoning = "Holding their flag - heavy escort";
    }
    else if (theyHaveFlag)
    {
        // They have our flag - hunt them down
        dist.SetRole(BGRole::FLAG_HUNTER, 5, 7);
        dist.SetRole(BGRole::NODE_DEFENDER, 2, 3);  // Defend our flag spawn
        dist.SetRole(BGRole::HEALER_OFFENSE, 2, 3);
        dist.reasoning = "They have our flag - hunting party";
    }
    else
    {
        // Neither team has flag - race for pickup
        switch (decision.strategy)
        {
            case BGStrategy::AGGRESSIVE:
            case BGStrategy::ALL_IN:
                dist.SetRole(BGRole::FLAG_HUNTER, 6, 8);  // Rush their base
                dist.SetRole(BGRole::NODE_DEFENDER, 1, 2);
                dist.reasoning = "No flags - aggressive pickup";
                break;

            case BGStrategy::DEFENSIVE:
            case BGStrategy::TURTLE:
                dist.SetRole(BGRole::FLAG_HUNTER, 2, 3);
                dist.SetRole(BGRole::NODE_DEFENDER, 4, 6);  // Camp our flag
                dist.reasoning = "No flags - defensive wait";
                break;

            default:
                dist.SetRole(BGRole::FLAG_HUNTER, 4, 5);
                dist.SetRole(BGRole::NODE_DEFENDER, 2, 3);
                dist.reasoning = "No flags - balanced approach";
                break;
        }

        dist.SetRole(BGRole::HEALER_OFFENSE, 1, 2);
        dist.SetRole(BGRole::HEALER_DEFENSE, 1, 2);
    }

    dist.SetRole(BGRole::ROAMER, 1, 2);
    return dist;
}

void BGScriptBase::LogEvent(const BGScriptEventData& event) const
{
    static const char* eventNames[] = {
        "MATCH_START", "MATCH_END", "ROUND_STARTED", "ROUND_ENDED",
        "", "", "", "", "", "",
        "OBJECTIVE_CAPTURED", "OBJECTIVE_LOST", "OBJECTIVE_CONTESTED", "OBJECTIVE_NEUTRALIZED",
        "", "", "", "", "", "",
        "FLAG_PICKED_UP", "FLAG_DROPPED", "FLAG_CAPTURED", "FLAG_RETURNED", "FLAG_RESET",
        "", "", "", "", "",
        "ORB_PICKED_UP", "ORB_DROPPED",
        "", "", "", "", "", "", "", "",
        "CART_CAPTURED", "CART_CONTESTED",
        "", "", "", "", "", "", "", "",
        "GATE_DESTROYED", "VEHICLE_SPAWNED", "VEHICLE_DESTROYED", "BOSS_ENGAGED", "BOSS_KILLED", "TOWER_DESTROYED",
        "", "", "", "",
        "PLAYER_KILLED", "PLAYER_DIED", "PLAYER_RESURRECTED",
        "", "", "", "",
        "AZERITE_SPAWNED", "RESOURCE_NODE_CLAIMED",
        "", "", "", "", "", "", "", "", "", "",
        "WORLD_STATE_CHANGED", "SCORE_THRESHOLD_REACHED", "TIME_WARNING"
    };

    uint8 eventIdx = static_cast<uint8>(event.eventType);
    const char* eventName = (eventIdx < sizeof(eventNames)/sizeof(eventNames[0])) ?
        eventNames[eventIdx] : "UNKNOWN";

    TC_LOG_DEBUG("playerbots.bg.script", "BGScript Event: {} on {} (obj: {}, faction: {})",
        eventName, GetName(), event.objectiveId, event.faction);
}

// ============================================================================
// WORLD STATE HELPERS
// ============================================================================

void BGScriptBase::RegisterWorldStateMapping(int32 stateId, uint32 objectiveId,
                                              BGObjectiveState targetState)
{
    WorldStateMapping mapping;
    mapping.objectiveId = objectiveId;
    mapping.state = targetState;
    m_worldStateMappings[stateId] = mapping;
}

void BGScriptBase::RegisterScoreWorldState(int32 stateId, bool isAlliance)
{
    if (isAlliance)
        m_allianceScoreState = stateId;
    else
        m_hordeScoreState = stateId;
}

bool BGScriptBase::TryInterpretFromCache(int32 stateId, int32 value,
                                          uint32& outObjectiveId, BGObjectiveState& outState) const
{
    auto it = m_worldStateMappings.find(stateId);
    if (it == m_worldStateMappings.end())
        return false;

    // Only return mapping if state value is non-zero (state is active)
    if (value != 0)
    {
        outObjectiveId = it->second.objectiveId;
        outState = it->second.state;
        return true;
    }

    return false;
}

} // namespace Playerbot::Coordination::Battleground
