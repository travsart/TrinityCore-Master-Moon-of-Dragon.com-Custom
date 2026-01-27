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

#include "AlteracValleyScript.h"
#include "BGScriptRegistry.h"
#include "BattlegroundCoordinator.h"
#include "Log.h"

namespace Playerbot::Coordination::Battleground
{

REGISTER_BG_SCRIPT(AlteracValleyScript, 30);  // AlteracValley::MAP_ID

// ============================================================================
// LIFECYCLE
// ============================================================================

void AlteracValleyScript::OnLoad(BattlegroundCoordinator* coordinator)
{
    SiegeScriptBase::OnLoad(coordinator);

    m_cachedObjectives = GetObjectiveData();

    // Register reinforcement world states
    RegisterScoreWorldState(AlteracValley::WorldStates::REINF_ALLY, true);
    RegisterScoreWorldState(AlteracValley::WorldStates::REINF_HORDE, false);

    // Initialize tower states
    for (uint32 i = 0; i <= AlteracValley::Towers::WEST_FROSTWOLF; ++i)
    {
        ObjectiveState state = AlteracValley::IsAllianceTower(i) ?
            ObjectiveState::ALLIANCE_CONTROLLED : ObjectiveState::HORDE_CONTROLLED;
        m_towerStates[i] = state;
    }

    // Captains alive
    m_balindaAlive = true;
    m_galvangarAlive = true;

    TC_LOG_DEBUG("playerbots.bg.script",
        "AlteracValleyScript: Loaded (8 towers, 7 graveyards, 2 bosses)");
}

// ============================================================================
// DATA PROVIDERS
// ============================================================================

std::vector<BGObjectiveData> AlteracValleyScript::GetObjectiveData() const
{
    std::vector<BGObjectiveData> objectives;

    // Add towers
    auto towers = GetTowerData();
    objectives.insert(objectives.end(), towers.begin(), towers.end());

    // Add graveyards
    auto graveyards = GetGraveyardData();
    objectives.insert(objectives.end(), graveyards.begin(), graveyards.end());

    // Add bosses as objectives
    BGObjectiveData vanndar;
    vanndar.id = 100;  // Special ID for Vanndar
    vanndar.type = ObjectiveType::BOSS;
    vanndar.name = "Vanndar Stormpike";
    vanndar.x = AlteracValley::VANNDAR_X;
    vanndar.y = AlteracValley::VANNDAR_Y;
    vanndar.z = AlteracValley::VANNDAR_Z;
    vanndar.strategicValue = 10;
    objectives.push_back(vanndar);

    BGObjectiveData drekthar;
    drekthar.id = 101;  // Special ID for Drek'Thar
    drekthar.type = ObjectiveType::BOSS;
    drekthar.name = "Drek'Thar";
    drekthar.x = AlteracValley::DREKTHAR_X;
    drekthar.y = AlteracValley::DREKTHAR_Y;
    drekthar.z = AlteracValley::DREKTHAR_Z;
    drekthar.strategicValue = 10;
    objectives.push_back(drekthar);

    return objectives;
}

std::vector<BGObjectiveData> AlteracValleyScript::GetTowerData() const
{
    std::vector<BGObjectiveData> towers;

    for (uint32 i = 0; i <= AlteracValley::Towers::WEST_FROSTWOLF; ++i)
    {
        BGObjectiveData tower;
        tower.id = i;
        tower.type = ObjectiveType::TOWER;
        tower.name = AlteracValley::GetTowerName(i);
        tower.x = AlteracValley::TOWER_POSITIONS[i].GetPositionX();
        tower.y = AlteracValley::TOWER_POSITIONS[i].GetPositionY();
        tower.z = AlteracValley::TOWER_POSITIONS[i].GetPositionZ();
        tower.strategicValue = 8;
        tower.captureTime = 240000;  // 4 minutes to burn

        towers.push_back(tower);
    }

    return towers;
}

std::vector<BGObjectiveData> AlteracValleyScript::GetGraveyardData() const
{
    std::vector<BGObjectiveData> graveyards;

    for (uint32 i = 0; i <= AlteracValley::Graveyards::FROSTWOLF_RELIEF_HUT; ++i)
    {
        BGObjectiveData gy;
        gy.id = 50 + i;  // Offset to avoid tower ID collision
        gy.type = ObjectiveType::GRAVEYARD;
        gy.name = AlteracValley::GetGraveyardName(i);
        gy.x = AlteracValley::GRAVEYARD_POSITIONS[i].GetPositionX();
        gy.y = AlteracValley::GRAVEYARD_POSITIONS[i].GetPositionY();
        gy.z = AlteracValley::GRAVEYARD_POSITIONS[i].GetPositionZ();
        gy.strategicValue = (i == AlteracValley::Graveyards::SNOWFALL_GY) ? 7 : 5;
        gy.captureTime = 60000;  // 1 minute to capture

        graveyards.push_back(gy);
    }

    return graveyards;
}

std::vector<BGObjectiveData> AlteracValleyScript::GetGateData() const
{
    // AV doesn't have destructible gates like SOTA/IOC
    return {};
}

std::vector<BGPositionData> AlteracValleyScript::GetSpawnPositions(uint32 faction) const
{
    std::vector<BGPositionData> spawns;

    if (faction == ALLIANCE)
    {
        for (const auto& pos : AlteracValley::ALLIANCE_SPAWNS)
        {
            BGPositionData spawn("Alliance Spawn", pos.GetPositionX(),
                pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
                BGPositionData::PositionType::SPAWN_POINT, ALLIANCE, 5);
            spawns.push_back(spawn);
        }
    }
    else
    {
        for (const auto& pos : AlteracValley::HORDE_SPAWNS)
        {
            BGPositionData spawn("Horde Spawn", pos.GetPositionX(),
                pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
                BGPositionData::PositionType::SPAWN_POINT, HORDE, 5);
            spawns.push_back(spawn);
        }
    }

    return spawns;
}

std::vector<BGPositionData> AlteracValleyScript::GetStrategicPositions() const
{
    std::vector<BGPositionData> positions;

    // Chokepoints
    auto chokepoints = AlteracValley::GetChokepoints();
    for (const auto& pos : chokepoints)
    {
        BGPositionData p("Chokepoint", pos.GetPositionX(), pos.GetPositionY(),
            pos.GetPositionZ(), 0.0f, BGPositionData::PositionType::CHOKEPOINT, 0, 7);
        positions.push_back(p);
    }

    // Tower defense positions
    for (uint32 i = 0; i <= AlteracValley::Towers::WEST_FROSTWOLF; ++i)
    {
        BGPositionData p(AlteracValley::GetTowerName(i),
            AlteracValley::TOWER_POSITIONS[i].GetPositionX(),
            AlteracValley::TOWER_POSITIONS[i].GetPositionY(),
            AlteracValley::TOWER_POSITIONS[i].GetPositionZ(),
            0.0f, BGPositionData::PositionType::DEFENSIVE_POSITION,
            AlteracValley::IsAllianceTower(i) ? ALLIANCE : HORDE, 8);
        positions.push_back(p);
    }

    // Boss positions
    positions.push_back(BGPositionData("Vanndar Stormpike",
        AlteracValley::VANNDAR_X, AlteracValley::VANNDAR_Y, AlteracValley::VANNDAR_Z,
        0.0f, BGPositionData::PositionType::STRATEGIC_POINT, ALLIANCE, 10));

    positions.push_back(BGPositionData("Drek'Thar",
        AlteracValley::DREKTHAR_X, AlteracValley::DREKTHAR_Y, AlteracValley::DREKTHAR_Z,
        0.0f, BGPositionData::PositionType::STRATEGIC_POINT, HORDE, 10));

    return positions;
}

std::vector<BGPositionData> AlteracValleyScript::GetGraveyardPositions(uint32 faction) const
{
    std::vector<BGPositionData> graveyards;

    for (uint32 i = 0; i <= AlteracValley::Graveyards::FROSTWOLF_RELIEF_HUT; ++i)
    {
        // Determine graveyard faction
        uint32 gyFaction = 0;  // Neutral
        if (i == AlteracValley::Graveyards::STORMPIKE_GY ||
            i == AlteracValley::Graveyards::STORMPIKE_AID_STATION ||
            i == AlteracValley::Graveyards::STONEHEARTH_GY)
            gyFaction = ALLIANCE;
        else if (i == AlteracValley::Graveyards::ICEBLOOD_GY ||
                 i == AlteracValley::Graveyards::FROSTWOLF_GY ||
                 i == AlteracValley::Graveyards::FROSTWOLF_RELIEF_HUT)
            gyFaction = HORDE;
        // Snowfall starts neutral

        if (faction == 0 || faction == gyFaction || gyFaction == 0)
        {
            BGPositionData p(AlteracValley::GetGraveyardName(i),
                AlteracValley::GRAVEYARD_POSITIONS[i].GetPositionX(),
                AlteracValley::GRAVEYARD_POSITIONS[i].GetPositionY(),
                AlteracValley::GRAVEYARD_POSITIONS[i].GetPositionZ(),
                0.0f, BGPositionData::PositionType::GRAVEYARD, gyFaction, 6);
            graveyards.push_back(p);
        }
    }

    return graveyards;
}

std::vector<BGWorldState> AlteracValleyScript::GetInitialWorldStates() const
{
    std::vector<BGWorldState> states;

    states.push_back(BGWorldState(AlteracValley::WorldStates::REINF_ALLY,
        "Alliance Reinforcements", BGWorldState::StateType::REINFORCEMENTS,
        AlteracValley::STARTING_REINFORCEMENTS));
    states.push_back(BGWorldState(AlteracValley::WorldStates::REINF_HORDE,
        "Horde Reinforcements", BGWorldState::StateType::REINFORCEMENTS,
        AlteracValley::STARTING_REINFORCEMENTS));

    return states;
}

// ============================================================================
// WORLD STATE
// ============================================================================

bool AlteracValleyScript::InterpretWorldState(int32 stateId, int32 value,
    uint32& outObjectiveId, ObjectiveState& outState) const
{
    // Reinforcement states
    if (stateId == AlteracValley::WorldStates::REINF_ALLY ||
        stateId == AlteracValley::WorldStates::REINF_HORDE)
    {
        return false;  // Handled as score, not objective
    }

    // Tower states
    if (stateId == AlteracValley::WorldStates::DB_NORTH_ALLY && value)
    {
        outObjectiveId = AlteracValley::Towers::DUN_BALDAR_NORTH;
        outState = ObjectiveState::ALLIANCE_CONTROLLED;
        return true;
    }

    // (Additional tower state mappings would follow)

    return TryInterpretFromCache(stateId, value, outObjectiveId, outState);
}

void AlteracValleyScript::GetScoreFromWorldStates(const std::map<int32, int32>& states,
    uint32& allianceScore, uint32& hordeScore) const
{
    // In AV, "score" is reinforcements
    allianceScore = AlteracValley::STARTING_REINFORCEMENTS;
    hordeScore = AlteracValley::STARTING_REINFORCEMENTS;

    auto allyIt = states.find(AlteracValley::WorldStates::REINF_ALLY);
    if (allyIt != states.end())
        allianceScore = static_cast<uint32>(std::max(0, allyIt->second));

    auto hordeIt = states.find(AlteracValley::WorldStates::REINF_HORDE);
    if (hordeIt != states.end())
        hordeScore = static_cast<uint32>(std::max(0, hordeIt->second));
}

// ============================================================================
// STRATEGY - AV SPECIFIC
// ============================================================================

void AlteracValleyScript::AdjustStrategy(StrategicDecision& decision,
    float scoreAdvantage, uint32 controlledCount,
    uint32 totalObjectives, uint32 timeRemaining) const
{
    // Get base siege strategy
    SiegeScriptBase::AdjustStrategy(decision, scoreAdvantage,
        controlledCount, totalObjectives, timeRemaining);

    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 ourTowers = GetStandingTowerCount(faction);
    uint32 theirTowers = GetStandingTowerCount(faction == ALLIANCE ? HORDE : ALLIANCE);

    // Tower burn strategy
    if (ShouldBurnTowers())
    {
        decision.reasoning += " + tower burn priority";

        if (theirTowers > 0)
        {
            // Dedicate some players to burning towers
            decision.offenseAllocation = std::max(decision.offenseAllocation,
                static_cast<uint8>(60));
        }
    }

    // Captain status affects strategy
    bool enemyCaptainAlive = (faction == ALLIANCE) ? m_galvangarAlive : m_balindaAlive;
    if (enemyCaptainAlive)
    {
        decision.reasoning += " (kill captain for bonus)";
    }

    // Rush boss condition check
    bool canRush = CanAttackBoss(faction == ALLIANCE ? HORDE : ALLIANCE);
    if (canRush && theirTowers <= 1)
    {
        decision.strategy = BGStrategy::ALL_IN;
        decision.reasoning = "Boss accessible and weak - RUSH!";
        decision.offenseAllocation = 85;
        decision.defenseAllocation = 15;
    }
}

// ============================================================================
// SIEGE IMPLEMENTATIONS
// ============================================================================

uint32 AlteracValleyScript::GetBossEntry(uint32 faction) const
{
    return faction == ALLIANCE ? AlteracValley::VANNDAR_ENTRY : AlteracValley::DREKTHAR_ENTRY;
}

Position AlteracValleyScript::GetBossPosition(uint32 faction) const
{
    return faction == ALLIANCE ?
        AlteracValley::GetVanndarPosition() :
        AlteracValley::GetDrekTharPosition();
}

bool AlteracValleyScript::CanAttackBoss(uint32 faction) const
{
    // In AV, boss can always be attacked, but is much stronger with towers up
    // Check if path is clear (no gates in AV)
    return true;
}

// ============================================================================
// AV-SPECIFIC HELPERS
// ============================================================================

std::vector<Position> AlteracValleyScript::GetRushRoute(uint32 faction) const
{
    if (faction == ALLIANCE)
        return AlteracValley::GetAllianceRushRoute();
    else
        return AlteracValley::GetHordeRushRoute();
}

bool AlteracValleyScript::ShouldBurnTowers() const
{
    // Burn towers if enemy has more than 2 standing
    uint32 faction = m_coordinator ? m_coordinator->GetFaction() : ALLIANCE;
    uint32 theirTowers = GetStandingTowerCount(faction == ALLIANCE ? HORDE : ALLIANCE);

    return theirTowers > 2;
}

std::vector<uint32> AlteracValleyScript::GetTowerBurnOrder(uint32 attackingFaction) const
{
    std::vector<uint32> order;

    if (attackingFaction == ALLIANCE)
    {
        // Alliance burns Horde towers from south to north
        order = {
            AlteracValley::Towers::TOWER_POINT,
            AlteracValley::Towers::ICEBLOOD_TOWER,
            AlteracValley::Towers::EAST_FROSTWOLF,
            AlteracValley::Towers::WEST_FROSTWOLF
        };
    }
    else
    {
        // Horde burns Alliance bunkers from south to north
        order = {
            AlteracValley::Towers::STONEHEARTH_BUNKER,
            AlteracValley::Towers::ICEWING_BUNKER,
            AlteracValley::Towers::DUN_BALDAR_SOUTH,
            AlteracValley::Towers::DUN_BALDAR_NORTH
        };
    }

    // Filter out already destroyed towers
    std::vector<uint32> result;
    for (uint32 towerId : order)
    {
        if (m_destroyedTowers.find(towerId) == m_destroyedTowers.end())
            result.push_back(towerId);
    }

    return result;
}

bool AlteracValleyScript::IsCaptainAlive(uint32 faction) const
{
    return faction == ALLIANCE ? m_balindaAlive : m_galvangarAlive;
}

} // namespace Playerbot::Coordination::Battleground
