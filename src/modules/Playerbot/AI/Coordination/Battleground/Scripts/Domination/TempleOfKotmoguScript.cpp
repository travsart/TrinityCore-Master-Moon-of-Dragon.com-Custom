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

#include "TempleOfKotmoguScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

// Register the script
REGISTER_BG_SCRIPT(TempleOfKotmoguScript, 998);  // TempleOfKotmogu::MAP_ID

// ============================================================================
// LIFECYCLE
// ============================================================================

void TempleOfKotmoguScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    DominationScriptBase::OnLoad(coordinator);

    // Cache objective data
    m_cachedObjectives = GetObjectiveData();

    // Register world state mappings for scores
    RegisterScoreWorldState(TempleOfKotmogu::WorldStates::SCORE_ALLY, true);
    RegisterScoreWorldState(TempleOfKotmogu::WorldStates::SCORE_HORDE, false);

    // Clear orb tracking
    m_orbHolders.clear();
    m_playerOrbs.clear();
    m_allianceOrbsHeld = 0;
    m_hordeOrbsHeld = 0;

    TC_LOG_DEBUG("playerbots.bg.script",
        "TempleOfKotmoguScript: Loaded with {} orbs, center bonus active",
        TempleOfKotmogu::ORB_COUNT);
}

void TempleOfKotmoguScript::OnMatchStart()
{
    DominationScriptBase::OnMatchStart();

    TC_LOG_INFO("playerbots.bg.script",
        "TOK: Match started! Strategy: Grab orbs then push center with escort");
}

void TempleOfKotmoguScript::OnMatchEnd(bool victory)
{
    DominationScriptBase::OnMatchEnd(victory);

    TC_LOG_INFO("playerbots.bg.script",
        "TOK: Match ended - {}! Final orb control tracked.",
        victory ? "Victory" : "Defeat");
}

// ============================================================================
// EVENT HANDLING
// ============================================================================

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

                // Update faction counts (simplified - would need actual faction lookup)
                if (event.faction == ALLIANCE)
                    m_allianceOrbsHeld++;
                else
                    m_hordeOrbsHeld++;

                TC_LOG_DEBUG("playerbots.bg.script",
                    "TOK: {} picked up by {} ({}). Alliance orbs: {}, Horde orbs: {}",
                    TempleOfKotmogu::GetOrbName(event.objectiveId),
                    event.primaryGuid.GetCounter(),
                    event.faction == ALLIANCE ? "Alliance" : "Horde",
                    m_allianceOrbsHeld, m_hordeOrbsHeld);
            }
            break;

        case BGScriptEvent::ORB_DROPPED:
            if (event.objectiveId < TempleOfKotmogu::ORB_COUNT)
            {
                auto it = m_orbHolders.find(event.objectiveId);
                if (it != m_orbHolders.end())
                {
                    // Update faction counts
                    if (event.faction == ALLIANCE && m_allianceOrbsHeld > 0)
                        m_allianceOrbsHeld--;
                    else if (event.faction == HORDE && m_hordeOrbsHeld > 0)
                        m_hordeOrbsHeld--;

                    m_playerOrbs.erase(it->second);
                    m_orbHolders.erase(it);
                }

                TC_LOG_DEBUG("playerbots.bg.script",
                    "TOK: {} dropped at ({:.1f}, {:.1f}). Alliance orbs: {}, Horde orbs: {}",
                    TempleOfKotmogu::GetOrbName(event.objectiveId),
                    event.x, event.y,
                    m_allianceOrbsHeld, m_hordeOrbsHeld);
            }
            break;

        case BGScriptEvent::PLAYER_KILLED:
            // When player dies, they drop their orb
            {
                auto it = m_playerOrbs.find(event.secondaryGuid);
                if (it != m_playerOrbs.end())
                {
                    uint32 orbId = it->second;

                    // Update faction counts (victim's faction)
                    if (event.faction == ALLIANCE && m_hordeOrbsHeld > 0)  // Victim was Horde
                        m_hordeOrbsHeld--;
                    else if (event.faction == HORDE && m_allianceOrbsHeld > 0)  // Victim was Alliance
                        m_allianceOrbsHeld--;

                    m_orbHolders.erase(orbId);
                    m_playerOrbs.erase(it);

                    TC_LOG_DEBUG("playerbots.bg.script",
                        "TOK: Orb carrier killed! {} dropped.",
                        TempleOfKotmogu::GetOrbName(orbId));
                }
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// DATA PROVIDERS
// ============================================================================

std::vector<BGObjectiveData> TempleOfKotmoguScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Add orbs
    for (uint32 i = 0; i < TempleOfKotmogu::ORB_COUNT; ++i)
        objectives.push_back(GetOrbData(i));

    // Add center as a strategic objective
    BGObjectiveData center;
    center.id = 100;  // Special ID for center
    center.type = ObjectiveType::STRATEGIC;
    center.name = "Center Zone";
    center.x = TempleOfKotmogu::CENTER_X;
    center.y = TempleOfKotmogu::CENTER_Y;
    center.z = TempleOfKotmogu::CENTER_Z;
    center.strategicValue = 10;  // Highest value
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
    orb.orientation = pos.GetOrientation();
    orb.strategicValue = TempleOfKotmogu::GetOrbStrategicValue(orbId);
    orb.captureTime = 0;  // Instant pickup

    // Set game object entries
    switch (orbId)
    {
        case TempleOfKotmogu::Orbs::ORANGE:
            orb.gameObjectEntry = TempleOfKotmogu::GameObjects::ORANGE_ORB;
            break;
        case TempleOfKotmogu::Orbs::BLUE:
            orb.gameObjectEntry = TempleOfKotmogu::GameObjects::BLUE_ORB;
            break;
        case TempleOfKotmogu::Orbs::GREEN:
            orb.gameObjectEntry = TempleOfKotmogu::GameObjects::GREEN_ORB;
            break;
        case TempleOfKotmogu::Orbs::PURPLE:
            orb.gameObjectEntry = TempleOfKotmogu::GameObjects::PURPLE_ORB;
            break;
    }

    return orb;
}

BGObjectiveData TempleOfKotmoguScript::GetNodeData(uint32 nodeIndex) const
{
    return GetOrbData(nodeIndex);
}

std::vector<BGPositionData> TempleOfKotmoguScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    if (faction == ALLIANCE)
    {
        for (const auto& pos : TempleOfKotmogu::ALLIANCE_SPAWNS)
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
        for (const auto& pos : TempleOfKotmogu::HORDE_SPAWNS)
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

std::vector<BGPositionData> TempleOfKotmoguScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Add orb defense positions
    for (uint32 i = 0; i < TempleOfKotmogu::ORB_COUNT; ++i)
    {
        auto orbPositions = TempleOfKotmogu::GetOrbDefensePositions(i);
        for (const auto& pos : orbPositions)
        {
            std::string name = std::string(TempleOfKotmogu::GetOrbName(i)) + " Defense";
            positions.push_back(BGPositionData(name,
                pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                pos.GetOrientation(),
                BGPositionData::PositionType::DEFENSIVE_POSITION,
                0, 8));
        }
    }

    // Add center defense positions
    auto centerPositions = TempleOfKotmogu::GetCenterDefensePositions();
    for (const auto& pos : centerPositions)
    {
        positions.push_back(BGPositionData("Center Zone",
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
            pos.GetOrientation(),
            BGPositionData::PositionType::STRATEGIC_POINT,
            0, 10));
    }

    // Add chokepoints
    auto chokepoints = TempleOfKotmogu::GetChokepoints();
    for (size_t i = 0; i < chokepoints.size(); ++i)
    {
        const auto& pos = chokepoints[i];
        std::string name = "Chokepoint " + std::to_string(i + 1);
        positions.push_back(BGPositionData(name,
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
            pos.GetOrientation(),
            BGPositionData::PositionType::CHOKEPOINT,
            0, 7));
    }

    // Add sniper positions
    auto sniperPositions = TempleOfKotmogu::GetSniperPositions();
    for (size_t i = 0; i < sniperPositions.size(); ++i)
    {
        const auto& pos = sniperPositions[i];
        std::string name = "Sniper Position " + std::to_string(i + 1);
        positions.push_back(BGPositionData(name,
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
            pos.GetOrientation(),
            BGPositionData::PositionType::SNIPER_POSITION,
            0, 8));
    }

    // Add buff positions
    auto buffPositions = TempleOfKotmogu::GetBuffPositions();
    for (size_t i = 0; i < buffPositions.size(); ++i)
    {
        const auto& pos = buffPositions[i];
        std::string name = "Power-up " + std::to_string(i + 1);
        positions.push_back(BGPositionData(name,
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
            pos.GetOrientation(),
            BGPositionData::PositionType::BUFF_LOCATION,
            0, 5));
    }

    return positions;
}

std::vector<BGPositionData> TempleOfKotmoguScript::GetGraveyardPositions(uint32 faction) const
{
    // TOK has faction-specific graveyards (same as spawns)
    return GetSpawnPositions(faction);
}

std::vector<BGWorldState> TempleOfKotmoguScript::GetInitialWorldStates() const
{
    std::vector<BGWorldState> states;

    // Scores
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::SCORE_ALLY,
        "Alliance Score", BGWorldState::StateType::SCORE_ALLIANCE, 0));
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::SCORE_HORDE,
        "Horde Score", BGWorldState::StateType::SCORE_HORDE, 0));
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::MAX_SCORE,
        "Max Score", BGWorldState::StateType::CUSTOM, TempleOfKotmogu::MAX_SCORE));

    // Orb states
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::ORANGE_ORB_STATE,
        "Orange Orb", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::BLUE_ORB_STATE,
        "Blue Orb", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::GREEN_ORB_STATE,
        "Green Orb", BGWorldState::StateType::OBJECTIVE_STATE, 0));
    states.push_back(BGWorldState(TempleOfKotmogu::WorldStates::PURPLE_ORB_STATE,
        "Purple Orb", BGWorldState::StateType::OBJECTIVE_STATE, 0));

    return states;
}

std::vector<uint32> TempleOfKotmoguScript::GetTickPointsTable() const
{
    // Points per orb held (modified by center position)
    return std::vector<uint32>(
        TempleOfKotmogu::TICK_POINTS_OUTSIDE.begin(),
        TempleOfKotmogu::TICK_POINTS_OUTSIDE.end()
    );
}

// ============================================================================
// WORLD STATE INTERPRETATION
// ============================================================================

bool TempleOfKotmoguScript::InterpretWorldState(int32 stateId, int32 value,
    uint32& outObjectiveId, BGObjectiveState& outState) const
{
    // Try cached mappings
    if (TryInterpretFromCache(stateId, value, outObjectiveId, outState))
        return true;

    // Orb-specific interpretation would go here
    // (Orbs don't have the same controlled/contested states as nodes)

    return false;
}

void TempleOfKotmoguScript::GetScoreFromWorldStates(const std::map<int32, int32>& states,
    uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = 0;
    hordeScore = 0;

    auto allyIt = states.find(TempleOfKotmogu::WorldStates::SCORE_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(TempleOfKotmogu::WorldStates::SCORE_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

// ============================================================================
// STRATEGY
// ============================================================================

TempleOfKotmoguScript::GamePhase TempleOfKotmoguScript::GetCurrentPhase() const
{
    uint32 elapsed = GetElapsedTime();

    // Check score advantage for desperate phase using member variables
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourScore = (faction == ALLIANCE) ? m_allianceScore : m_hordeScore;
    uint32 theirScore = (faction == ALLIANCE) ? m_hordeScore : m_allianceScore;

    // Desperate if we're significantly behind
    if (ourScore + TempleOfKotmogu::Strategy::DESPERATION_THRESHOLD < theirScore &&
        elapsed > TempleOfKotmogu::Strategy::MID_GAME_START)
    {
        return GamePhase::DESPERATE;
    }

    if (elapsed < TempleOfKotmogu::Strategy::OPENING_PHASE_DURATION)
        return GamePhase::OPENING;

    if (elapsed < TempleOfKotmogu::Strategy::LATE_GAME_START)
        return GamePhase::MID_GAME;

    return GamePhase::LATE_GAME;
}

void TempleOfKotmoguScript::ApplyPhaseStrategy(StrategicDecision& decision, GamePhase phase, float scoreAdvantage) const
{
    switch (phase)
    {
        case GamePhase::OPENING:
            decision.reasoning = "Opening phase - rush to grab orbs!";
            decision.strategy = BGStrategy::AGGRESSIVE;
            decision.offenseAllocation = 80;
            decision.defenseAllocation = 20;
            break;

        case GamePhase::MID_GAME:
            if (scoreAdvantage > 0.15f)
            {
                decision.reasoning = "Mid-game (leading) - hold orbs and score safely";
                decision.strategy = BGStrategy::DEFENSIVE;
                decision.offenseAllocation = 35;
                decision.defenseAllocation = 65;
            }
            else if (scoreAdvantage < -0.15f)
            {
                decision.reasoning = "Mid-game (behind) - aggressive orb hunting";
                decision.strategy = BGStrategy::AGGRESSIVE;
                decision.offenseAllocation = 70;
                decision.defenseAllocation = 30;
            }
            else
            {
                decision.reasoning = "Mid-game (even) - balanced orb control";
                decision.strategy = BGStrategy::BALANCED;
                decision.offenseAllocation = 50;
                decision.defenseAllocation = 50;
            }
            break;

        case GamePhase::LATE_GAME:
            if (scoreAdvantage > 0.1f)
            {
                decision.reasoning = "Late game (winning) - protect orb carriers";
                decision.strategy = BGStrategy::DEFENSIVE;
                decision.offenseAllocation = 25;
                decision.defenseAllocation = 75;
            }
            else
            {
                decision.reasoning = "Late game (close/behind) - push center with orbs!";
                decision.strategy = BGStrategy::AGGRESSIVE;
                decision.offenseAllocation = 65;
                decision.defenseAllocation = 35;
            }
            break;

        case GamePhase::DESPERATE:
            decision.reasoning = "DESPERATE - All in on center with orbs!";
            decision.strategy = BGStrategy::ALL_IN;
            decision.offenseAllocation = 90;
            decision.defenseAllocation = 10;
            break;
    }
}

void TempleOfKotmoguScript::AdjustStrategy(StrategicDecision& decision,
    float scoreAdvantage, uint32 controlledCount,
    uint32 totalObjectives, uint32 timeRemaining) const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourOrbs = GetOrbsHeldByFaction(faction);
    uint32 theirOrbs = GetOrbsHeldByFaction(faction == ALLIANCE ? HORDE : ALLIANCE);

    // Apply phase-specific strategy
    GamePhase phase = GetCurrentPhase();
    ApplyPhaseStrategy(decision, phase, scoreAdvantage);

    // Orb-specific adjustments
    if (ourOrbs >= 2)
    {
        // We have enough orbs - consider center push
        if (ShouldPushToCenter(faction))
        {
            decision.reasoning += " (pushing to center!)";
            decision.offenseAllocation += 10;  // More aggressive
        }
        else
        {
            decision.reasoning += " (holding orbs safely)";
        }
    }
    else if (ourOrbs == 0)
    {
        // No orbs - must be aggressive
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.reasoning = "No orbs! Must grab orbs immediately!";
        decision.offenseAllocation = 80;
        decision.defenseAllocation = 20;
    }

    // If enemy has many orbs, prioritize killing carriers
    if (theirOrbs >= 3)
    {
        decision.reasoning += " (enemy has too many orbs - HUNT THEM!)";
        decision.offenseAllocation = std::min(static_cast<uint8>(90), static_cast<uint8>(decision.offenseAllocation + 20));
    }

    // Center bonus reminder
    decision.reasoning += " (center = 5x points!)";
}

RoleDistribution TempleOfKotmoguScript::GetRecommendedRoles(const StrategicDecision& decision,
    float scoreAdvantage, uint32 timeRemaining) const
{
    RoleDistribution dist;

    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourOrbs = GetOrbsHeldByFaction(faction);

    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            dist.roleCounts[BGRole::ORB_CARRIER] = 30;      // Orb grabbers
            dist.roleCounts[BGRole::FLAG_ESCORT] = 20;      // Protect carriers
            dist.roleCounts[BGRole::NODE_ATTACKER] = 40;    // Kill enemy carriers
            dist.roleCounts[BGRole::NODE_DEFENDER] = 10;
            dist.reasoning = "Aggressive orb hunting";
            break;

        case BGStrategy::DEFENSIVE:
            dist.roleCounts[BGRole::ORB_CARRIER] = 25;
            dist.roleCounts[BGRole::FLAG_ESCORT] = 45;      // Heavy protection
            dist.roleCounts[BGRole::NODE_DEFENDER] = 20;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 10;
            dist.reasoning = "Defensive orb hold";
            break;

        case BGStrategy::ALL_IN:
            dist.roleCounts[BGRole::ORB_CARRIER] = 40;
            dist.roleCounts[BGRole::FLAG_ESCORT] = 40;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 20;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 0;
            dist.reasoning = "All-in center push";
            break;

        default:  // BALANCED
            dist.roleCounts[BGRole::ORB_CARRIER] = 30;
            dist.roleCounts[BGRole::FLAG_ESCORT] = 30;
            dist.roleCounts[BGRole::NODE_ATTACKER] = 25;
            dist.roleCounts[BGRole::NODE_DEFENDER] = 15;
            dist.reasoning = "Balanced orb control";
            break;
    }

    // Adjust based on orb count
    if (ourOrbs >= 2)
    {
        // Shift toward escort
        dist.roleCounts[BGRole::FLAG_ESCORT] += 10;
        dist.roleCounts[BGRole::NODE_ATTACKER] -= 10;
        dist.reasoning = "Escorting orb carriers";
    }

    return dist;
}

// ============================================================================
// ORB-SPECIFIC METHODS
// ============================================================================

ObjectGuid TempleOfKotmoguScript::GetOrbHolder(uint32 orbId) const
{
    auto it = m_orbHolders.find(orbId);
    return it != m_orbHolders.end() ? it->second : ObjectGuid::Empty;
}

bool TempleOfKotmoguScript::IsPlayerHoldingOrb(ObjectGuid guid) const
{
    return m_playerOrbs.find(guid) != m_playerOrbs.end();
}

int32 TempleOfKotmoguScript::GetPlayerOrbId(ObjectGuid guid) const
{
    auto it = m_playerOrbs.find(guid);
    return it != m_playerOrbs.end() ? static_cast<int32>(it->second) : -1;
}

uint32 TempleOfKotmoguScript::GetOrbsHeldByFaction(uint32 faction) const
{
    return (faction == ALLIANCE) ? m_allianceOrbsHeld : m_hordeOrbsHeld;
}

bool TempleOfKotmoguScript::IsInCenter(float x, float y) const
{
    return TempleOfKotmogu::IsInCenterZone(x, y);
}

std::vector<Position> TempleOfKotmoguScript::GetOrbCarrierRoute(uint32 orbId) const
{
    return TempleOfKotmogu::GetOrbCarrierRoute(orbId);
}

std::vector<Position> TempleOfKotmoguScript::GetEscortFormation(float carrierX, float carrierY, float carrierZ) const
{
    return TempleOfKotmogu::GetEscortFormation(carrierX, carrierY, carrierZ);
}

bool TempleOfKotmoguScript::ShouldPushToCenter(uint32 faction) const
{
    uint32 ourOrbs = GetOrbsHeldByFaction(faction);

    // Need at least 2 orbs to push center
    if (ourOrbs < TempleOfKotmogu::Strategy::CENTER_PUSH_ORB_COUNT)
        return false;

    // Don't push too early
    if (GetElapsedTime() < TempleOfKotmogu::Strategy::INITIAL_HOLD_TIME)
        return false;

    // Push if we have good orb advantage
    uint32 theirOrbs = GetOrbsHeldByFaction(faction == ALLIANCE ? HORDE : ALLIANCE);
    return ourOrbs >= theirOrbs;
}

std::vector<uint32> TempleOfKotmoguScript::GetOrbPriority(uint32 faction) const
{
    // Alliance closer to Orange/Blue (east side)
    // Horde closer to Green/Purple (west side)
    if (faction == ALLIANCE)
    {
        return {
            TempleOfKotmogu::Orbs::ORANGE,  // Closest
            TempleOfKotmogu::Orbs::BLUE,
            TempleOfKotmogu::Orbs::GREEN,
            TempleOfKotmogu::Orbs::PURPLE   // Furthest
        };
    }
    else
    {
        return {
            TempleOfKotmogu::Orbs::GREEN,   // Closest
            TempleOfKotmogu::Orbs::PURPLE,
            TempleOfKotmogu::Orbs::ORANGE,
            TempleOfKotmogu::Orbs::BLUE     // Furthest
        };
    }
}

// ============================================================================
// ENTERPRISE-GRADE POSITIONING
// ============================================================================

std::vector<Position> TempleOfKotmoguScript::GetChokepoints() const
{
    return TempleOfKotmogu::GetChokepoints();
}

std::vector<Position> TempleOfKotmoguScript::GetSniperPositions() const
{
    return TempleOfKotmogu::GetSniperPositions();
}

std::vector<Position> TempleOfKotmoguScript::GetBuffPositions() const
{
    return TempleOfKotmogu::GetBuffPositions();
}

std::vector<Position> TempleOfKotmoguScript::GetAmbushPositions(uint32 faction) const
{
    return TempleOfKotmogu::GetAmbushPositions(faction);
}

std::vector<Position> TempleOfKotmoguScript::GetCenterDefensePositions() const
{
    return TempleOfKotmogu::GetCenterDefensePositions();
}

std::vector<Position> TempleOfKotmoguScript::GetOrbDefensePositions(uint32 orbId) const
{
    return TempleOfKotmogu::GetOrbDefensePositions(orbId);
}

float TempleOfKotmoguScript::GetOrbToOrbDistance(uint32 fromOrb, uint32 toOrb) const
{
    return TempleOfKotmogu::GetOrbDistance(fromOrb, toOrb);
}

float TempleOfKotmoguScript::GetOrbToCenterDistance(uint32 orbId) const
{
    return TempleOfKotmogu::GetOrbToCenterDistance(orbId);
}

} // namespace Playerbot::Coordination::Battleground
