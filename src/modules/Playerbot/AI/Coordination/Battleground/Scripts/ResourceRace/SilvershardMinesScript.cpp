/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#include "SilvershardMinesScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(SilvershardMinesScript, 727);  // SilvershardMines::MAP_ID

void SilvershardMinesScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    ResourceRaceScriptBase::OnLoad(coordinator);
    m_cachedObjectives = GetObjectiveData();

    RegisterScoreWorldState(SilvershardMines::WorldStates::SCORE_ALLY, true);
    RegisterScoreWorldState(SilvershardMines::WorldStates::SCORE_HORDE, false);

    // Initialize cart states
    m_ssmCartStates.clear();
    m_cartPositions.clear();
    m_cartControllers.clear();
    m_cartContested.clear();

    for (uint32 i = 0; i < SilvershardMines::CART_COUNT; ++i)
    {
        SSMCartState state;
        state.trackId = i;  // Each cart starts on its own track
        state.trackProgress = 0.0f;
        state.atIntersection = false;
        state.intersectionId = 0;
        m_ssmCartStates[i] = state;

        m_cartPositions[i] = SilvershardMines::GetCartSpawnPosition(i);
        m_cartControllers[i] = 0;
        m_cartContested[i] = false;
    }
}

void SilvershardMinesScript::OnEvent(const BGScriptEventData& event)
{
    ResourceRaceScriptBase::OnEvent(event);

    switch (event.eventType)
    {
        case BGScriptEvent::CART_CAPTURED:
            if (event.objectiveId < SilvershardMines::CART_COUNT)
            {
                // Reset cart state
                m_ssmCartStates[event.objectiveId].trackProgress = 0.0f;
                m_ssmCartStates[event.objectiveId].atIntersection = false;
                m_cartControllers[event.objectiveId] = 0;
            }
            break;

        case BGScriptEvent::OBJECTIVE_CONTESTED:
            if (event.objectiveId < SilvershardMines::CART_COUNT)
            {
                m_cartContested[event.objectiveId] = true;
            }
            break;

        case BGScriptEvent::OBJECTIVE_CAPTURED:
            if (event.objectiveId < SilvershardMines::CART_COUNT)
            {
                m_cartControllers[event.objectiveId] =
                    (event.newState == ObjectiveState::ALLIANCE_CONTROLLED) ? ALLIANCE :
                    (event.newState == ObjectiveState::HORDE_CONTROLLED) ? HORDE : 0;
                m_cartContested[event.objectiveId] = false;
            }
            break;

        default:
            break;
    }
}

std::vector<BGObjectiveData> SilvershardMinesScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Carts as objectives
    for (uint32 i = 0; i < SilvershardMines::CART_COUNT; ++i)
    {
        BGObjectiveData cart;
        Position pos = SilvershardMines::GetCartSpawnPosition(i);
        cart.id = i;
        cart.type = ObjectiveType::CART;
        cart.name = SilvershardMines::GetCartName(i);
        cart.x = pos.GetPositionX();
        cart.y = pos.GetPositionY();
        cart.z = pos.GetPositionZ();
        cart.strategicValue = 8;
        objectives.push_back(cart);
    }

    // Intersections as strategic points
    for (uint32 i = 0; i < SilvershardMines::Intersections::INTERSECTION_COUNT; ++i)
    {
        BGObjectiveData intersection;
        Position pos = SilvershardMines::GetIntersectionPosition(i);
        intersection.id = 50 + i;
        intersection.type = ObjectiveType::STRATEGIC;
        intersection.name = std::string("Intersection ") + std::to_string(i + 1);
        intersection.x = pos.GetPositionX();
        intersection.y = pos.GetPositionY();
        intersection.z = pos.GetPositionZ();
        intersection.strategicValue = 6;
        objectives.push_back(intersection);
    }

    return objectives;
}

std::vector<BGPositionData> SilvershardMinesScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;
    Position pos = SilvershardMines::GetSpawnPosition(faction);
    spawns.push_back(BGPositionData(faction == ALLIANCE ? "Alliance Spawn" : "Horde Spawn",
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
        BGPositionData::PositionType::SPAWN_POINT, faction, 5));
    return spawns;
}

std::vector<BGPositionData> SilvershardMinesScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Cart spawn positions
    for (uint32 i = 0; i < SilvershardMines::CART_COUNT; ++i)
    {
        Position pos = SilvershardMines::GetCartSpawnPosition(i);
        positions.push_back(BGPositionData(SilvershardMines::GetCartName(i),
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::STRATEGIC_POINT, 0, 8));
    }

    // Intersections
    for (uint32 i = 0; i < SilvershardMines::Intersections::INTERSECTION_COUNT; ++i)
    {
        Position pos = SilvershardMines::GetIntersectionPosition(i);
        positions.push_back(BGPositionData("Intersection",
            pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0,
            BGPositionData::PositionType::CHOKEPOINT, 0, 7));
    }

    // Depots
    positions.push_back(BGPositionData("Alliance Depot",
        SilvershardMines::ALLIANCE_DEPOT.GetPositionX(),
        SilvershardMines::ALLIANCE_DEPOT.GetPositionY(),
        SilvershardMines::ALLIANCE_DEPOT.GetPositionZ(), 0,
        BGPositionData::PositionType::STRATEGIC_POINT, ALLIANCE, 9));

    positions.push_back(BGPositionData("Horde Depot",
        SilvershardMines::HORDE_DEPOT.GetPositionX(),
        SilvershardMines::HORDE_DEPOT.GetPositionY(),
        SilvershardMines::HORDE_DEPOT.GetPositionZ(), 0,
        BGPositionData::PositionType::STRATEGIC_POINT, HORDE, 9));

    return positions;
}

std::vector<BGPositionData> SilvershardMinesScript::GetGraveyardPositions(uint32 faction) const
{
    return GetSpawnPositions(faction);
}

std::vector<BGWorldState> SilvershardMinesScript::GetInitialWorldStates() const
{
    return {
        BGWorldState(SilvershardMines::WorldStates::SCORE_ALLY, "Alliance Score", BGWorldState::StateType::SCORE_ALLIANCE, 0),
        BGWorldState(SilvershardMines::WorldStates::SCORE_HORDE, "Horde Score", BGWorldState::StateType::SCORE_HORDE, 0)
    };
}

bool SilvershardMinesScript::InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const
{
    // Cart states
    if (stateId == SilvershardMines::WorldStates::CART1_STATE ||
        stateId == SilvershardMines::WorldStates::CART2_STATE ||
        stateId == SilvershardMines::WorldStates::CART3_STATE)
    {
        outObjectiveId = stateId - SilvershardMines::WorldStates::CART1_STATE;
        // Value interpretation: 0 = neutral, 1 = alliance, 2 = horde
        outState = (value == 1) ? ObjectiveState::ALLIANCE_CONTROLLED :
                   (value == 2) ? ObjectiveState::HORDE_CONTROLLED :
                   ObjectiveState::NEUTRAL;
        return true;
    }

    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void SilvershardMinesScript::GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const
{
    allianceScore = hordeScore = 0;
    auto it = states.find(SilvershardMines::WorldStates::SCORE_ALLY);
    if (it != states.end()) allianceScore = static_cast<uint32>(std::max(0, it->second));
    it = states.find(SilvershardMines::WorldStates::SCORE_HORDE);
    if (it != states.end()) hordeScore = static_cast<uint32>(std::max(0, it->second));
}

Position SilvershardMinesScript::GetCartPosition(uint32 cartId) const
{
    auto it = m_cartPositions.find(cartId);
    return it != m_cartPositions.end() ? it->second : Position(0, 0, 0, 0);
}

float SilvershardMinesScript::GetCartProgress(uint32 cartId) const
{
    auto it = m_ssmCartStates.find(cartId);
    return it != m_ssmCartStates.end() ? it->second.trackProgress : 0.0f;
}

uint32 SilvershardMinesScript::GetCartController(uint32 cartId) const
{
    auto it = m_cartControllers.find(cartId);
    return it != m_cartControllers.end() ? it->second : 0;
}

bool SilvershardMinesScript::IsCartContested(uint32 cartId) const
{
    auto it = m_cartContested.find(cartId);
    return it != m_cartContested.end() && it->second;
}

std::vector<Position> SilvershardMinesScript::GetTrackWaypoints(uint32 trackId) const
{
    switch (trackId)
    {
        case SilvershardMines::Tracks::LAVA:
            return SilvershardMines::GetLavaTrackWaypoints();
        case SilvershardMines::Tracks::UPPER:
            return SilvershardMines::GetUpperTrackWaypoints();
        case SilvershardMines::Tracks::DIAMOND:
            return SilvershardMines::GetDiamondTrackWaypoints();
        default:
            return {};
    }
}

uint32 SilvershardMinesScript::GetCartOnTrack(uint32 trackId) const
{
    for (const auto& [cartId, state] : m_ssmCartStates)
    {
        if (state.trackId == trackId)
            return cartId;
    }
    return UINT32_MAX;  // No cart on this track
}

std::vector<uint32> SilvershardMinesScript::GetIntersectionIds() const
{
    return { SilvershardMines::Intersections::LAVA_UPPER, SilvershardMines::Intersections::UPPER_DIAMOND };
}

uint32 SilvershardMinesScript::GetIntersectionDecisionTime(uint32 /*intersectionId*/) const
{
    return 5000;  // 5 seconds to decide at intersections
}

uint32 SilvershardMinesScript::GetTrackForCart(uint32 cartId) const
{
    auto it = m_ssmCartStates.find(cartId);
    return it != m_ssmCartStates.end() ? it->second.trackId : 0;
}

bool SilvershardMinesScript::IsCartNearIntersection(uint32 cartId) const
{
    auto it = m_ssmCartStates.find(cartId);
    return it != m_ssmCartStates.end() && it->second.atIntersection;
}

uint32 SilvershardMinesScript::GetEstimatedCaptureTime(uint32 cartId) const
{
    auto it = m_ssmCartStates.find(cartId);
    if (it == m_ssmCartStates.end())
        return UINT32_MAX;

    float remainingProgress = 1.0f - it->second.trackProgress;
    uint32 trackTime = 0;

    switch (it->second.trackId)
    {
        case SilvershardMines::Tracks::LAVA:
            trackTime = SilvershardMines::LAVA_TRACK_TIME;
            break;
        case SilvershardMines::Tracks::UPPER:
            trackTime = SilvershardMines::UPPER_TRACK_TIME;
            break;
        case SilvershardMines::Tracks::DIAMOND:
            trackTime = SilvershardMines::DIAMOND_TRACK_TIME;
            break;
    }

    return static_cast<uint32>(trackTime * remainingProgress);
}

} // namespace Playerbot::Coordination::Battleground
