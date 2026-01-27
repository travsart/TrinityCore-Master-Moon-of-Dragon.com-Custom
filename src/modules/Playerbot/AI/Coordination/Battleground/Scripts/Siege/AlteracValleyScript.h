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

#ifndef PLAYERBOT_AI_COORDINATION_BG_SIEGE_ALTERACVALLEYSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_SIEGE_ALTERACVALLEYSCRIPT_H

#include "SiegeScriptBase.h"
#include "AlteracValleyData.h"

namespace Playerbot::Coordination::Battleground
{

/**
 * @brief Alterac Valley battleground script
 *
 * Classic 40v40 epic battleground:
 * - Win by killing enemy boss (Vanndar/Drek'Thar) or depleting reinforcements
 * - 8 towers (4 per faction) that weaken boss when destroyed
 * - Multiple graveyards to capture
 * - Captains that give bonuses when killed
 * - Mines that provide resources
 *
 * Map ID: 30
 */
class AlteracValleyScript : public SiegeScriptBase
{
public:
    AlteracValleyScript() = default;
    ~AlteracValleyScript() override = default;

    // ========================================================================
    // IDENTIFICATION
    // ========================================================================

    uint32 GetMapId() const override { return AlteracValley::MAP_ID; }
    std::string GetName() const override { return AlteracValley::BG_NAME; }
    BGType GetBGType() const override { return BGType::ALTERAC_VALLEY; }
    uint32 GetMaxScore() const override { return AlteracValley::STARTING_REINFORCEMENTS; }
    uint32 GetMaxDuration() const override { return AlteracValley::MAX_DURATION; }
    uint8 GetTeamSize() const override { return AlteracValley::TEAM_SIZE; }

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    void OnLoad(BattlegroundCoordinator* coordinator) override;

    // ========================================================================
    // DATA PROVIDERS
    // ========================================================================

    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;

    // ========================================================================
    // WORLD STATE
    // ========================================================================

    bool InterpretWorldState(int32 stateId, int32 value,
        uint32& outObjectiveId, BGObjectiveState& outState) const override;

    void GetScoreFromWorldStates(const std::map<int32, int32>& states,
        uint32& allianceScore, uint32& hordeScore) const override;

    // ========================================================================
    // STRATEGY - AV SPECIFIC
    // ========================================================================

    void AdjustStrategy(StrategicDecision& decision,
        float scoreAdvantage, uint32 controlledCount,
        uint32 totalObjectives, uint32 timeRemaining) const override;

protected:
    // ========================================================================
    // SIEGE ABSTRACT IMPLEMENTATIONS
    // ========================================================================

    uint32 GetBossEntry(uint32 faction) const override;
    Position GetBossPosition(uint32 faction) const override;
    std::vector<BGObjectiveData> GetGateData() const override;
    std::vector<BGObjectiveData> GetTowerData() const override;
    std::vector<BGObjectiveData> GetGraveyardData() const override;

    uint32 GetStartingReinforcements() const override
    {
        return AlteracValley::STARTING_REINFORCEMENTS;
    }

    uint32 GetReinforcementLossPerDeath() const override
    {
        return AlteracValley::REINF_LOSS_PER_DEATH;
    }

    uint32 GetReinforcementLossPerTower() const override
    {
        return AlteracValley::REINF_LOSS_PER_TOWER;
    }

    bool CanAttackBoss(uint32 faction) const override;

private:
    // ========================================================================
    // AV-SPECIFIC HELPERS
    // ========================================================================

    /**
     * @brief Get the "zerg" rush strategy targets
     */
    std::vector<Position> GetRushRoute(uint32 faction) const;

    /**
     * @brief Check if we should do a tower burn strategy
     */
    bool ShouldBurnTowers() const;

    /**
     * @brief Get priority towers to burn
     */
    std::vector<uint32> GetTowerBurnOrder(uint32 attackingFaction) const;

    /**
     * @brief Check if captain is alive
     */
    bool IsCaptainAlive(uint32 faction) const;

    // Captain state
    bool m_balindaAlive = true;
    bool m_galvangarAlive = true;
};

} // namespace Playerbot::Coordination::Battleground

#endif // PLAYERBOT_AI_COORDINATION_BG_SIEGE_ALTERACVALLEYSCRIPT_H
