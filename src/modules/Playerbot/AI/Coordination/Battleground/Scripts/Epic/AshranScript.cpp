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

#include "AshranScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"
#include "Timer.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(AshranScript, 1191);  // Ashran::MAP_ID

// ============================================================================
// LIFECYCLE
// ============================================================================

void AshranScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    BGScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();

    // Register world states for tracking
    RegisterScoreWorldState(Ashran::WorldStates::ROAD_PROGRESS_ALLY, true);
    RegisterScoreWorldState(Ashran::WorldStates::ROAD_PROGRESS_HORDE, false);

    // Initialize state
    m_allianceProgress = 0.0f;
    m_hordeProgress = 0.0f;
    m_activeEvent = UINT32_MAX;
    m_eventTimer = 0;
    m_matchStartTime = 0;
    m_lastRoadUpdate = 0;
    m_lastStrategyUpdate = 0;
    m_trembladeAlive = true;
    m_volrathAlive = true;

    // Initialize control point states
    m_controlStates[Ashran::ControlPoints::STORMSHIELD_STRONGHOLD] = BGObjectiveState::ALLIANCE_CONTROLLED;
    m_controlStates[Ashran::ControlPoints::CROSSROADS] = BGObjectiveState::CONTESTED;
    m_controlStates[Ashran::ControlPoints::WARSPEAR_STRONGHOLD] = BGObjectiveState::HORDE_CONTROLLED;

    TC_LOG_DEBUG("playerbots.bg.script",
        "AshranScript: Loaded enterprise-grade epic BG script (Road of Glory + {} events + boss assault)",
        Ashran::Events::EVENT_COUNT);
}

void AshranScript::OnMatchStart()
{
    BGScriptBase::OnMatchStart();

    m_matchStartTime = getMSTime();
    m_lastRoadUpdate = m_matchStartTime;
    m_lastStrategyUpdate = m_matchStartTime;

    // Reset to initial state
    m_allianceProgress = 0.0f;
    m_hordeProgress = 0.0f;
    m_activeEvent = UINT32_MAX;
    m_eventTimer = 0;
    m_trembladeAlive = true;
    m_volrathAlive = true;

    // Reset control points
    m_controlStates[Ashran::ControlPoints::STORMSHIELD_STRONGHOLD] = BGObjectiveState::ALLIANCE_CONTROLLED;
    m_controlStates[Ashran::ControlPoints::CROSSROADS] = BGObjectiveState::CONTESTED;
    m_controlStates[Ashran::ControlPoints::WARSPEAR_STRONGHOLD] = BGObjectiveState::HORDE_CONTROLLED;

    TC_LOG_INFO("playerbots.bg.script",
        "AshranScript: Match started! Road of Glory battle begins - "
        "Control Crossroads, push to enemy base, kill their leader!");
}

void AshranScript::OnMatchEnd(bool victory)
{
    BGScriptBase::OnMatchEnd(victory);

    uint32 duration = getMSTime() - m_matchStartTime;
    uint32 minutes = duration / 60000;
    uint32 seconds = (duration % 60000) / 1000;

    const char* result = victory ? "VICTORY" : "DEFEAT";
    const char* reason = "";

    if (!m_volrathAlive)
        reason = " - High Warlord Volrath slain!";
    else if (!m_trembladeAlive)
        reason = " - Grand Marshal Tremblade slain!";

    TC_LOG_INFO("playerbots.bg.script",
        "AshranScript: Match ended - {} after {}m {}s{}",
        result, minutes, seconds, reason);
}

void AshranScript::OnUpdate(uint32 diff)
{
    BGScriptBase::OnUpdate(diff);

    uint32 now = getMSTime();

    // Update road progress periodically
    if (now - m_lastRoadUpdate >= Ashran::Strategy::ROAD_UPDATE_INTERVAL)
    {
        UpdateRoadProgress();
        m_lastRoadUpdate = now;
    }

    // Update event status
    UpdateEventStatus();

    // Track event timer
    if (m_activeEvent < Ashran::Events::EVENT_COUNT)
    {
        if (m_eventTimer > diff)
            m_eventTimer -= diff;
        else
        {
            TC_LOG_DEBUG("playerbots.bg.script",
                "AshranScript: Event '{}' ended",
                Ashran::GetEventName(m_activeEvent));
            m_activeEvent = UINT32_MAX;
            m_eventTimer = 0;
        }
    }
}

void AshranScript::OnEvent(const BGScriptEventData& event)
{
    BGScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::OBJECTIVE_CAPTURED:
        {
            if (event.objectiveId < Ashran::ControlPoints::CONTROL_POINT_COUNT)
            {
                m_controlStates[event.objectiveId] = event.newState;

                const char* pointName = Ashran::GetControlPointName(event.objectiveId);
                const char* controller = (event.newState == BGObjectiveState::ALLIANCE_CONTROLLED) ?
                    "ALLIANCE" : (event.newState == BGObjectiveState::HORDE_CONTROLLED) ? "HORDE" : "CONTESTED";

                TC_LOG_INFO("playerbots.bg.script",
                    "AshranScript: {} is now {}!",
                    pointName, controller);

                // Critical: Crossroads control affects road progress
                if (event.objectiveId == Ashran::ControlPoints::CROSSROADS)
                {
                    TC_LOG_INFO("playerbots.bg.script",
                        "AshranScript: CROSSROADS CONTROL CHANGE - This is a turning point!");
                }
            }
            break;
        }

        case BGScriptEvent::OBJECTIVE_NEUTRALIZED:
        {
            if (event.objectiveId < Ashran::ControlPoints::CONTROL_POINT_COUNT)
            {
                m_controlStates[event.objectiveId] = BGObjectiveState::CONTESTED;

                TC_LOG_INFO("playerbots.bg.script",
                    "AshranScript: {} is being contested!",
                    Ashran::GetControlPointName(event.objectiveId));
            }
            break;
        }

        case BGScriptEvent::BOSS_KILLED:
        {
            if (event.objectiveId == Ashran::ObjectiveIds::TREMBLADE)
            {
                m_trembladeAlive = false;
                TC_LOG_INFO("playerbots.bg.script",
                    "AshranScript: GRAND MARSHAL TREMBLADE HAS FALLEN! Horde Victory!");
            }
            else if (event.objectiveId == Ashran::ObjectiveIds::VOLRATH)
            {
                m_volrathAlive = false;
                TC_LOG_INFO("playerbots.bg.script",
                    "AshranScript: HIGH WARLORD VOLRATH HAS FALLEN! Alliance Victory!");
            }
            break;
        }

        case BGScriptEvent::CUSTOM_EVENT:
        {
            // Event spawn/completion - check if objectiveId is an event
            if (event.objectiveId >= Ashran::ObjectiveIds::EVENT_RACE_SUPREMACY &&
                event.objectiveId <= Ashran::ObjectiveIds::EVENT_BRUTE_ASSAULT)
            {
                uint32 eventId = event.objectiveId - Ashran::ObjectiveIds::EVENT_RACE_SUPREMACY;
                // Use newState to determine if event started or ended
                if (event.newState != BGObjectiveState::NEUTRAL)  // Event started
                {
                    m_activeEvent = eventId;
                    m_eventTimer = 300000;  // 5 minute events

                    TC_LOG_INFO("playerbots.bg.script",
                        "AshranScript: EVENT STARTED - '{}' (Priority: {})",
                        Ashran::GetEventName(eventId),
                        Ashran::GetEventPriority(eventId));
                }
                else  // Event ended
                {
                    if (m_activeEvent == eventId)
                    {
                        m_activeEvent = UINT32_MAX;
                        m_eventTimer = 0;
                    }
                }
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// DATA PROVIDERS
// ============================================================================

std::vector<BGObjectiveData> AshranScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Road of Glory control points
    for (uint32 i = 0; i < Ashran::ControlPoints::CONTROL_POINT_COUNT; ++i)
    {
        objectives.push_back(GetControlPointData(i));
    }

    // Faction leaders as boss objectives
    BGObjectiveData tremblade;
    tremblade.id = Ashran::ObjectiveIds::TREMBLADE;
    tremblade.type = ObjectiveType::BOSS;
    tremblade.name = "Grand Marshal Tremblade";
    Position tremPos = Ashran::GetTrembladePosition();
    tremblade.x = tremPos.GetPositionX();
    tremblade.y = tremPos.GetPositionY();
    tremblade.z = tremPos.GetPositionZ();
    tremblade.strategicValue = 10;  // Maximum value
    objectives.push_back(tremblade);

    BGObjectiveData volrath;
    volrath.id = Ashran::ObjectiveIds::VOLRATH;
    volrath.type = ObjectiveType::BOSS;
    volrath.name = "High Warlord Volrath";
    Position volPos = Ashran::GetVolrathPosition();
    volrath.x = volPos.GetPositionX();
    volrath.y = volPos.GetPositionY();
    volrath.z = volPos.GetPositionZ();
    volrath.strategicValue = 10;
    objectives.push_back(volrath);

    // Side events as dynamic objectives
    for (uint32 i = 0; i < Ashran::Events::EVENT_COUNT; ++i)
    {
        objectives.push_back(GetEventObjectiveData(i));
    }

    return objectives;
}

BGObjectiveData AshranScript::GetControlPointData(uint32 pointId) const
{
    BGObjectiveData point;
    Position pos = Ashran::GetControlPosition(pointId);

    point.id = pointId;
    point.type = ObjectiveType::NODE;
    point.name = Ashran::GetControlPointName(pointId);
    point.x = pos.GetPositionX();
    point.y = pos.GetPositionY();
    point.z = pos.GetPositionZ();

    // Crossroads is the most valuable control point
    if (pointId == Ashran::ControlPoints::CROSSROADS)
        point.strategicValue = 9;
    else
        point.strategicValue = 7;

    return point;
}

BGObjectiveData AshranScript::GetEventObjectiveData(uint32 eventId) const
{
    BGObjectiveData eventObj;
    Position pos = Ashran::GetEventCenter(eventId);

    eventObj.id = Ashran::ObjectiveIds::EVENT_RACE_SUPREMACY + eventId;
    eventObj.type = ObjectiveType::STRATEGIC;
    eventObj.name = Ashran::GetEventName(eventId);
    eventObj.x = pos.GetPositionX();
    eventObj.y = pos.GetPositionY();
    eventObj.z = pos.GetPositionZ();
    eventObj.strategicValue = Ashran::GetEventPriority(eventId);

    return eventObj;
}

std::vector<BGPositionData> AshranScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    Position pos = Ashran::GetSpawnPosition(faction);

    spawns.push_back(BGPositionData(
        faction == ALLIANCE ? "Stormshield" : "Warspear",
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
        BGPositionData::PositionType::SPAWN_POINT, faction, 5));

    return spawns;
}

std::vector<BGPositionData> AshranScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Control points
    for (uint32 i = 0; i < Ashran::ControlPoints::CONTROL_POINT_COUNT; ++i)
    {
        Position pos = Ashran::GetControlPosition(i);
        uint8 priority = (i == Ashran::ControlPoints::CROSSROADS) ? 9 : 7;
        positions.push_back(BGPositionData(Ashran::GetControlPointName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::STRATEGIC_POINT, 0, priority));
    }

    // Control point defense positions
    for (uint32 i = 0; i < Ashran::ControlPoints::CONTROL_POINT_COUNT; ++i)
    {
        auto defPositions = Ashran::GetControlPointDefensePositions(i);
        for (const auto& pos : defPositions)
        {
            positions.push_back(BGPositionData(
                std::string(Ashran::GetControlPointName(i)) + " Defense",
                pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
                BGPositionData::PositionType::DEFENSIVE_POSITION, 0, 6));
        }
    }

    // Road chokepoints
    auto chokepoints = Ashran::GetRoadChokepoints();
    for (size_t i = 0; i < chokepoints.size(); ++i)
    {
        positions.push_back(BGPositionData("Road Chokepoint",
            chokepoints[i].GetPositionX(), chokepoints[i].GetPositionY(),
            chokepoints[i].GetPositionZ(), chokepoints[i].GetOrientation(),
            BGPositionData::PositionType::CHOKEPOINT, 0, 7));
    }

    // Sniper positions
    auto sniperPos = Ashran::GetSniperPositions();
    for (const auto& pos : sniperPos)
    {
        positions.push_back(BGPositionData("Overlook",
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
            BGPositionData::PositionType::SNIPER_POSITION, 0, 6));
    }

    // Faction leaders (boss positions)
    Position tremPos = Ashran::GetTrembladePosition();
    positions.push_back(BGPositionData("Grand Marshal Tremblade",
        tremPos.GetPositionX(), tremPos.GetPositionY(), tremPos.GetPositionZ(), tremPos.GetOrientation(),
        BGPositionData::PositionType::STRATEGIC_POINT, ALLIANCE, 10));

    Position volPos = Ashran::GetVolrathPosition();
    positions.push_back(BGPositionData("High Warlord Volrath",
        volPos.GetPositionX(), volPos.GetPositionY(), volPos.GetPositionZ(), volPos.GetOrientation(),
        BGPositionData::PositionType::STRATEGIC_POINT, HORDE, 10));

    // Event positions
    for (uint32 i = 0; i < Ashran::Events::EVENT_COUNT; ++i)
    {
        Position eventPos = Ashran::GetEventCenter(i);
        positions.push_back(BGPositionData(Ashran::GetEventName(i),
            eventPos.GetPositionX(), eventPos.GetPositionY(), eventPos.GetPositionZ(), 0,
            BGPositionData::PositionType::STRATEGIC_POINT, 0, Ashran::GetEventPriority(i)));
    }

    return positions;
}

std::vector<BGPositionData> AshranScript::GetGraveyardPositions(uint32 faction) const
{
    // In Ashran, you respawn at your faction base
    return GetSpawnPositions(faction);
}

std::vector<BGWorldState> AshranScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(Ashran::WorldStates::ROAD_PROGRESS_ALLY, "Alliance Progress",
            BGWorldState::StateType::CUSTOM, 50),  // Start at 50%
        BGWorldState(Ashran::WorldStates::ROAD_PROGRESS_HORDE, "Horde Progress",
            BGWorldState::StateType::CUSTOM, 50),
        BGWorldState(Ashran::WorldStates::CROSSROADS_CONTROL, "Crossroads Control",
            BGWorldState::StateType::OBJECTIVE_STATE, 0),  // 0 = contested
        BGWorldState(Ashran::WorldStates::TREMBLADE_HEALTH, "Tremblade Health",
            BGWorldState::StateType::CUSTOM, 100),
        BGWorldState(Ashran::WorldStates::VOLRATH_HEALTH, "Volrath Health",
            BGWorldState::StateType::CUSTOM, 100)
    };
}

// ============================================================================
// WORLD STATE INTERPRETATION
// ============================================================================

bool AshranScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, BGObjectiveState& outState) const
{
    if (stateId == Ashran::WorldStates::CROSSROADS_CONTROL)
    {
        outObjectiveId = Ashran::ControlPoints::CROSSROADS;
        if (value == 1)
            outState = BGObjectiveState::ALLIANCE_CONTROLLED;
        else if (value == 2)
            outState = BGObjectiveState::HORDE_CONTROLLED;
        else
            outState = BGObjectiveState::CONTESTED;
        return true;
    }

    if (stateId == Ashran::WorldStates::ACTIVE_EVENT)
    {
        // Event tracking - not an objective state
        return false;
    }

    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void AshranScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    // In Ashran, "score" is road progress (0-100)
    allianceScore = 50;
    hordeScore = 50;

    auto allyIt = states.find(Ashran::WorldStates::ROAD_PROGRESS_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(Ashran::WorldStates::ROAD_PROGRESS_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

// ============================================================================
// STRATEGY & ROLE DISTRIBUTION
// ============================================================================

RoleDistribution AshranScript::GetRecommendedRoles(const StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*timeRemaining*/) const
{
    RoleDistribution roles;

    // Ashran is large-scale (up to 75 players per side)
    // Roles need to cover: road push, control points, events, boss assault

    switch (decision.strategy)
    {
        case BGStrategy::AGGRESSIVE:
            // Aggressive road push
            roles.SetRole(BGRole::NODE_ATTACKER, 35, 45);     // Road pushers
            roles.SetRole(BGRole::BOSS_ASSAULT, 20, 30);      // Boss kill team
            roles.SetRole(BGRole::ROAMER, 15, 20);            // Event participants
            roles.SetRole(BGRole::NODE_DEFENDER, 10, 15);     // Hold positions
            roles.reasoning = "Aggressive push: focus on road control and boss pressure";
            break;

        case BGStrategy::DEFENSIVE:
            // Hold positions and defend
            roles.SetRole(BGRole::NODE_DEFENDER, 35, 45);     // Hold control points
            roles.SetRole(BGRole::NODE_ATTACKER, 20, 25);     // Counter-push
            roles.SetRole(BGRole::ROAMER, 15, 20);            // Events and reinforcement
            roles.SetRole(BGRole::BOSS_ASSAULT, 10, 15);      // Opportunistic
            roles.reasoning = "Defensive: hold road and protect base";
            break;

        case BGStrategy::ALL_IN:
            // Boss assault mode
            roles.SetRole(BGRole::BOSS_ASSAULT, 40, 50);      // Main assault force
            roles.SetRole(BGRole::NODE_ATTACKER, 25, 35);     // Road control
            roles.SetRole(BGRole::ROAMER, 10, 15);            // Support/events
            roles.SetRole(BGRole::NODE_DEFENDER, 5, 10);      // Minimal defense
            roles.reasoning = "ALL-IN BOSS ASSAULT! Push to enemy leader!";
            break;

        default:  // BALANCED
            // Even distribution for normal gameplay
            roles.SetRole(BGRole::NODE_ATTACKER, 30, 35);     // Road push
            roles.SetRole(BGRole::NODE_DEFENDER, 25, 30);     // Control points
            roles.SetRole(BGRole::ROAMER, 20, 25);            // Events and roaming
            roles.SetRole(BGRole::BOSS_ASSAULT, 15, 20);      // Boss pressure
            roles.reasoning = "Balanced: control road while maintaining flexibility";
            break;
    }

    return roles;
}

void AshranScript::AdjustStrategy(StrategicDecision& decision, float /*scoreAdvantage*/, uint32 /*controlledCount*/, uint32 /*totalObjectives*/, uint32 /*timeRemaining*/) const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

    // Determine current phase
    AshranPhase phase = GetCurrentPhase();

    TC_LOG_DEBUG("playerbots.bg.script",
        "AshranScript: Phase = {}, Ally Progress = {:.2f}, Horde Progress = {:.2f}",
        GetPhaseName(phase), m_allianceProgress, m_hordeProgress);

    // Get road progress
    float ourProgress = GetRoadProgress(faction);
    float enemyProgress = GetRoadProgress(faction == ALLIANCE ? HORDE : ALLIANCE);

    // Apply phase-specific strategy
    switch (phase)
    {
        case AshranPhase::OPENING:
            ApplyOpeningPhaseStrategy(decision, faction);
            break;

        case AshranPhase::BOSS_ASSAULT:
            ApplyBossAssaultStrategy(decision, faction);
            break;

        case AshranPhase::DEFENSE:
            ApplyDefensiveStrategy(decision, faction, enemyProgress);
            break;

        case AshranPhase::EVENT_FOCUS:
            ApplyEventFocusStrategy(decision, faction, m_activeEvent);
            break;

        case AshranPhase::ROAD_PUSH:
        default:
            ApplyRoadPushStrategy(decision, faction, ourProgress, enemyProgress);
            break;
    }

    // Add event participation note if active
    if (IsEventActive() && ShouldParticipateInEvent(m_activeEvent))
    {
        decision.reasoning += " | Event: " + std::string(Ashran::GetEventName(m_activeEvent));
    }

    decision.confidence = 0.8f;
}

// ============================================================================
// ASHRAN-SPECIFIC METHODS
// ============================================================================

float AshranScript::GetRoadProgress(uint32 faction) const
{
    return (faction == ALLIANCE) ? m_allianceProgress : m_hordeProgress;
}

Position AshranScript::GetEventPosition(uint32 eventId) const
{
    return Ashran::GetEventCenter(eventId);
}

bool AshranScript::ShouldParticipateInEvent(uint32 eventId) const
{
    if (eventId >= Ashran::Events::EVENT_COUNT)
        return false;

    uint8 priority = Ashran::GetEventPriority(eventId);

    // High priority events (6+) are always worth participating in
    if (priority >= 6)
        return true;

    // Medium priority (4-5) only if we have spare forces
    if (priority >= 4)
    {
        // Participate if we control crossroads
        auto it = m_controlStates.find(Ashran::ControlPoints::CROSSROADS);
        if (it != m_controlStates.end())
        {
            uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
            if ((faction == ALLIANCE && it->second == BGObjectiveState::ALLIANCE_CONTROLLED) ||
                (faction == HORDE && it->second == BGObjectiveState::HORDE_CONTROLLED))
            {
                return true;
            }
        }
    }

    // Low priority events - skip unless nothing else to do
    return false;
}

bool AshranScript::IsEnemyBaseVulnerable(uint32 attackingFaction) const
{
    // Check if we control crossroads
    if (!ControlsCrossroads(attackingFaction))
        return false;

    // Check progress threshold
    float progress = GetRoadProgress(attackingFaction);
    return progress >= Ashran::Strategy::BOSS_PUSH_THRESHOLD;
}

uint8 AshranScript::GetEventPriority(uint32 eventId) const
{
    return Ashran::GetEventPriority(eventId);
}

bool AshranScript::ControlsCrossroads(uint32 faction) const
{
    auto it = m_controlStates.find(Ashran::ControlPoints::CROSSROADS);
    if (it == m_controlStates.end())
        return false;

    if (faction == ALLIANCE)
        return it->second == BGObjectiveState::ALLIANCE_CONTROLLED;
    else
        return it->second == BGObjectiveState::HORDE_CONTROLLED;
}

uint32 AshranScript::GetControlledPointCount(uint32 faction) const
{
    uint32 count = 0;
    for (const auto& [pointId, state] : m_controlStates)
    {
        if (faction == ALLIANCE && state == BGObjectiveState::ALLIANCE_CONTROLLED)
            ++count;
        else if (faction == HORDE && state == BGObjectiveState::HORDE_CONTROLLED)
            ++count;
    }
    return count;
}

// ============================================================================
// ENTERPRISE-GRADE POSITIONING
// ============================================================================

std::vector<Position> AshranScript::GetChokepoints() const
{
    return Ashran::GetRoadChokepoints();
}

std::vector<Position> AshranScript::GetSniperPositions() const
{
    return Ashran::GetSniperPositions();
}

std::vector<Position> AshranScript::GetAmbushPositions(uint32 faction) const
{
    return Ashran::GetAmbushPositions(faction);
}

std::vector<Position> AshranScript::GetControlPointDefensePositions(uint32 pointId) const
{
    return Ashran::GetControlPointDefensePositions(pointId);
}

std::vector<Position> AshranScript::GetEventPositions(uint32 eventId) const
{
    std::vector<Position> positions;

    switch (eventId)
    {
        case Ashran::Events::RING_OF_CONQUEST:
            for (const auto& pos : Ashran::EventPositions::RING_POSITIONS)
                positions.push_back(pos);
            break;

        case Ashran::Events::SEAT_OF_OMEN:
            for (const auto& pos : Ashran::EventPositions::OMEN_POSITIONS)
                positions.push_back(pos);
            break;

        case Ashran::Events::EMPOWERED_ORE:
            for (const auto& pos : Ashran::EventPositions::ORE_NODES)
                positions.push_back(pos);
            break;

        case Ashran::Events::ANCIENT_ARTIFACT:
            for (const auto& pos : Ashran::EventPositions::ARTIFACT_SPAWNS)
                positions.push_back(pos);
            break;

        case Ashran::Events::RACE_FOR_SUPREMACY:
            for (const auto& pos : Ashran::EventPositions::RACE_DEFENSE)
                positions.push_back(pos);
            break;

        case Ashran::Events::STADIUM_RACING:
            for (const auto& pos : Ashran::EventPositions::STADIUM_POSITIONS)
                positions.push_back(pos);
            break;

        case Ashran::Events::OGRE_FIRES:
            for (const auto& pos : Ashran::EventPositions::FIRE_LOCATIONS)
                positions.push_back(pos);
            break;

        case Ashran::Events::BRUTE_ASSAULT:
            for (const auto& pos : Ashran::EventPositions::BRUTE_POSITIONS)
                positions.push_back(pos);
            break;

        default:
            // Return event center as fallback
            positions.push_back(Ashran::GetEventCenter(eventId));
            break;
    }

    return positions;
}

std::vector<Position> AshranScript::GetBossApproachRoute(uint32 faction) const
{
    // Alliance attacks Volrath (Horde leader)
    // Horde attacks Tremblade (Alliance leader)
    if (faction == ALLIANCE)
        return Ashran::BossRoutes::GetVolrathApproach();
    else
        return Ashran::BossRoutes::GetTrembladeApproach();
}

std::vector<Position> AshranScript::GetBossRaidPositions(uint32 faction) const
{
    std::vector<Position> positions;

    if (faction == ALLIANCE)
    {
        // Alliance raids Volrath
        for (const auto& pos : Ashran::BossRoutes::RaidPositions::VOLRATH_RAID)
            positions.push_back(pos);
    }
    else
    {
        // Horde raids Tremblade
        for (const auto& pos : Ashran::BossRoutes::RaidPositions::TREMBLADE_RAID)
            positions.push_back(pos);
    }

    return positions;
}

std::vector<Position> AshranScript::GetRoadWaypoints() const
{
    return Ashran::GetRoadOfGloryWaypoints();
}

float AshranScript::CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2) const
{
    return Ashran::CalculateDistance(x1, y1, z1, x2, y2, z2);
}

// ============================================================================
// PHASE MANAGEMENT
// ============================================================================

AshranScript::AshranPhase AshranScript::GetCurrentPhase() const
{
    uint32 now = getMSTime();
    uint32 elapsedMs = now - m_matchStartTime;

    // Opening phase: first 2 minutes
    if (elapsedMs < 120000)
        return AshranPhase::OPENING;

    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    float ourProgress = GetRoadProgress(faction);
    float enemyProgress = GetRoadProgress(faction == ALLIANCE ? HORDE : ALLIANCE);

    // Boss assault: we're deep in enemy territory and can push
    if (IsEnemyBaseVulnerable(faction))
        return AshranPhase::BOSS_ASSAULT;

    // Defense: enemy is threatening our base
    if (enemyProgress >= Ashran::Strategy::DEFENSIVE_THRESHOLD)
        return AshranPhase::DEFENSE;

    // Event focus: high-priority event is active
    if (IsEventActive() && Ashran::GetEventPriority(m_activeEvent) >= 7)
        return AshranPhase::EVENT_FOCUS;

    // Default: road push
    return AshranPhase::ROAD_PUSH;
}

const char* AshranScript::GetPhaseName(AshranPhase phase) const
{
    switch (phase)
    {
        case AshranPhase::OPENING:     return "OPENING";
        case AshranPhase::ROAD_PUSH:   return "ROAD_PUSH";
        case AshranPhase::EVENT_FOCUS: return "EVENT_FOCUS";
        case AshranPhase::BOSS_ASSAULT: return "BOSS_ASSAULT";
        case AshranPhase::DEFENSE:     return "DEFENSE";
        default:                       return "UNKNOWN";
    }
}

void AshranScript::ApplyPhaseStrategy(StrategicDecision& decision, AshranPhase phase, float scoreAdvantage) const
{
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;

    switch (phase)
    {
        case AshranPhase::OPENING:
            ApplyOpeningPhaseStrategy(decision, faction);
            break;

        case AshranPhase::BOSS_ASSAULT:
            ApplyBossAssaultStrategy(decision, faction);
            break;

        case AshranPhase::DEFENSE:
            ApplyDefensiveStrategy(decision, faction, GetRoadProgress(faction == ALLIANCE ? HORDE : ALLIANCE));
            break;

        case AshranPhase::EVENT_FOCUS:
            ApplyEventFocusStrategy(decision, faction, m_activeEvent);
            break;

        case AshranPhase::ROAD_PUSH:
        default:
            ApplyRoadPushStrategy(decision, faction,
                GetRoadProgress(faction),
                GetRoadProgress(faction == ALLIANCE ? HORDE : ALLIANCE));
            break;
    }
}

void AshranScript::ApplyOpeningPhaseStrategy(StrategicDecision& decision, uint32 faction) const
{
    decision.strategy = BGStrategy::AGGRESSIVE;
    decision.offenseAllocation = 75;
    decision.defenseAllocation = 25;

    // Rush to crossroads
    decision.attackObjectives.clear();
    decision.attackObjectives.push_back(Ashran::ControlPoints::CROSSROADS);

    // Also push toward enemy stronghold
    if (faction == ALLIANCE)
        decision.attackObjectives.push_back(Ashran::ControlPoints::WARSPEAR_STRONGHOLD);
    else
        decision.attackObjectives.push_back(Ashran::ControlPoints::STORMSHIELD_STRONGHOLD);

    decision.reasoning = "OPENING: Rush to Crossroads! Control the center!";
    decision.confidence = 0.9f;
}

void AshranScript::ApplyRoadPushStrategy(StrategicDecision& decision, uint32 faction, float ourProgress, float enemyProgress) const
{
    // Determine attack/defense balance based on position
    if (ourProgress > Ashran::Strategy::DEEP_PUSH_THRESHOLD)
    {
        // Deep in enemy territory - push hard
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.offenseAllocation = 70;
        decision.defenseAllocation = 30;
        decision.reasoning = "Deep push - maintain pressure toward enemy base";
    }
    else if (ourProgress > 0.5f)
    {
        // Winning but not dominant
        decision.strategy = BGStrategy::AGGRESSIVE;
        decision.offenseAllocation = 60;
        decision.defenseAllocation = 40;
        decision.reasoning = "Ahead on road - continue pushing while holding ground";
    }
    else if (enemyProgress > 0.5f)
    {
        // Losing ground
        decision.strategy = BGStrategy::BALANCED;
        decision.offenseAllocation = 50;
        decision.defenseAllocation = 50;
        decision.reasoning = "Behind on road - stabilize and counter-push";
    }
    else
    {
        // Stalemate around crossroads
        decision.strategy = BGStrategy::BALANCED;
        decision.offenseAllocation = 55;
        decision.defenseAllocation = 45;
        decision.reasoning = "Contesting crossroads - fight for center control";
    }

    // Set objectives
    decision.attackObjectives.clear();
    decision.defendObjectives.clear();

    if (!ControlsCrossroads(faction))
    {
        decision.attackObjectives.push_back(Ashran::ControlPoints::CROSSROADS);
    }
    else
    {
        decision.defendObjectives.push_back(Ashran::ControlPoints::CROSSROADS);

        // Push further
        if (faction == ALLIANCE)
            decision.attackObjectives.push_back(Ashran::ControlPoints::WARSPEAR_STRONGHOLD);
        else
            decision.attackObjectives.push_back(Ashran::ControlPoints::STORMSHIELD_STRONGHOLD);
    }

    decision.confidence = 0.8f;
}

void AshranScript::ApplyEventFocusStrategy(StrategicDecision& decision, uint32 faction, uint32 eventId) const
{
    // Maintain road presence but focus some forces on event
    decision.strategy = BGStrategy::BALANCED;
    decision.offenseAllocation = 50;
    decision.defenseAllocation = 50;

    // Still try to hold/take crossroads
    if (ControlsCrossroads(faction))
        decision.defendObjectives.push_back(Ashran::ControlPoints::CROSSROADS);
    else
        decision.attackObjectives.push_back(Ashran::ControlPoints::CROSSROADS);

    // Add event as attack objective
    decision.attackObjectives.push_back(Ashran::ObjectiveIds::EVENT_RACE_SUPREMACY + eventId);

    decision.reasoning = "EVENT FOCUS: Win '" + std::string(Ashran::GetEventName(eventId)) +
        "' while maintaining road control";
    decision.confidence = 0.75f;
}

void AshranScript::ApplyBossAssaultStrategy(StrategicDecision& decision, uint32 faction) const
{
    decision.strategy = BGStrategy::ALL_IN;
    decision.offenseAllocation = 80;
    decision.defenseAllocation = 20;

    // Clear objectives and focus on boss
    decision.attackObjectives.clear();
    decision.defendObjectives.clear();

    if (faction == ALLIANCE)
    {
        decision.attackObjectives.push_back(Ashran::ObjectiveIds::VOLRATH);
        decision.attackObjectives.push_back(Ashran::ControlPoints::WARSPEAR_STRONGHOLD);
    }
    else
    {
        decision.attackObjectives.push_back(Ashran::ObjectiveIds::TREMBLADE);
        decision.attackObjectives.push_back(Ashran::ControlPoints::STORMSHIELD_STRONGHOLD);
    }

    // Keep minimal crossroads defense
    decision.defendObjectives.push_back(Ashran::ControlPoints::CROSSROADS);

    decision.reasoning = "BOSS ASSAULT! Push to enemy leader - KILL THEM!";
    decision.confidence = 0.9f;
}

void AshranScript::ApplyDefensiveStrategy(StrategicDecision& decision, uint32 faction, float enemyProgress) const
{
    if (enemyProgress >= Ashran::Strategy::BOSS_PUSH_THRESHOLD)
    {
        // Emergency defense
        decision.strategy = BGStrategy::DEFENSIVE;
        decision.offenseAllocation = 20;
        decision.defenseAllocation = 80;
        decision.reasoning = "EMERGENCY DEFENSE! Enemy at our base - protect the leader!";
    }
    else
    {
        // Standard defense
        decision.strategy = BGStrategy::DEFENSIVE;
        decision.offenseAllocation = 35;
        decision.defenseAllocation = 65;
        decision.reasoning = "Defensive stance - repel enemy push and stabilize";
    }

    // Set objectives
    decision.attackObjectives.clear();
    decision.defendObjectives.clear();

    // Defend our stronghold
    if (faction == ALLIANCE)
    {
        decision.defendObjectives.push_back(Ashran::ControlPoints::STORMSHIELD_STRONGHOLD);
        decision.defendObjectives.push_back(Ashran::ObjectiveIds::TREMBLADE);
    }
    else
    {
        decision.defendObjectives.push_back(Ashran::ControlPoints::WARSPEAR_STRONGHOLD);
        decision.defendObjectives.push_back(Ashran::ObjectiveIds::VOLRATH);
    }

    // Try to retake crossroads
    decision.attackObjectives.push_back(Ashran::ControlPoints::CROSSROADS);

    decision.confidence = 0.85f;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void AshranScript::UpdateRoadProgress()
{
    // In a real implementation, this would query actual game state
    // For now, estimate based on control point ownership

    uint32 allyPoints = GetControlledPointCount(ALLIANCE);
    uint32 hordePoints = GetControlledPointCount(HORDE);

    // Adjust progress based on control
    if (allyPoints > hordePoints)
    {
        m_allianceProgress = std::min(1.0f, m_allianceProgress + 0.01f);
        m_hordeProgress = std::max(0.0f, m_hordeProgress - 0.01f);
    }
    else if (hordePoints > allyPoints)
    {
        m_hordeProgress = std::min(1.0f, m_hordeProgress + 0.01f);
        m_allianceProgress = std::max(0.0f, m_allianceProgress - 0.01f);
    }

    // Crossroads control provides bonus
    if (ControlsCrossroads(ALLIANCE))
        m_allianceProgress = std::min(1.0f, m_allianceProgress + Ashran::Strategy::CROSSROADS_CONTROL_BONUS * 0.01f);
    else if (ControlsCrossroads(HORDE))
        m_hordeProgress = std::min(1.0f, m_hordeProgress + Ashran::Strategy::CROSSROADS_CONTROL_BONUS * 0.01f);
}

void AshranScript::UpdateEventStatus()
{
    // In a real implementation, this would track actual event spawns
    // Events spawn periodically and can be tracked via world states
}

} // namespace Playerbot::Coordination::Battleground
