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

#ifndef PLAYERBOT_AI_COORDINATION_BG_CTF_TWINPEAKSSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_CTF_TWINPEAKSSCRIPT_H

#include "CTFScriptBase.h"
#include "TwinPeaksData.h"

namespace Playerbot::Coordination::Battleground
{

/**
 * @brief Twin Peaks battleground script
 *
 * Implements CTF mechanics for Twin Peaks:
 * - 10v10 capture the flag
 * - First to 3 captures wins
 * - 25 minute time limit
 * - Different terrain from WSG (river crossing, elevation changes)
 *
 * Map ID: 726
 */
class TwinPeaksScript : public CTFScriptBase
{
public:
    TwinPeaksScript() = default;
    ~TwinPeaksScript() override = default;

    // ========================================================================
    // IDENTIFICATION
    // ========================================================================

    uint32 GetMapId() const override { return TwinPeaks::MAP_ID; }
    std::string GetName() const override { return TwinPeaks::BG_NAME; }
    BGType GetBGType() const override { return BGType::TWIN_PEAKS; }
    uint32 GetMaxScore() const override { return TwinPeaks::MAX_SCORE; }
    uint32 GetMaxDuration() const override { return TwinPeaks::MAX_DURATION; }
    uint8 GetTeamSize() const override { return TwinPeaks::TEAM_SIZE; }

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
    // UTILITY
    // ========================================================================

    std::vector<Position> GetObjectivePath(
        uint32 fromObjective, uint32 toObjective) const override;

protected:
    // ========================================================================
    // CTF ABSTRACT IMPLEMENTATIONS
    // ========================================================================

    Position GetAllianceFlagPosition() const override
    {
        return TwinPeaks::GetAllianceFlagPos();
    }

    Position GetHordeFlagPosition() const override
    {
        return TwinPeaks::GetHordeFlagPos();
    }

    std::vector<Position> GetAllianceFlagRoomDefense() const override
    {
        return TwinPeaks::GetAllianceFlagRoomDefense();
    }

    std::vector<Position> GetHordeFlagRoomDefense() const override
    {
        return TwinPeaks::GetHordeFlagRoomDefense();
    }

    std::vector<Position> GetMiddleChokepoints() const override
    {
        return TwinPeaks::GetMiddleChokepoints();
    }

    std::vector<Position> GetSpeedBuffPositions() const override
    {
        return TwinPeaks::GetSpeedBuffPositions();
    }

    std::vector<Position> GetRestoreBuffPositions() const override
    {
        return TwinPeaks::GetRestoreBuffPositions();
    }

    std::vector<Position> GetBerserkBuffPositions() const override
    {
        return TwinPeaks::GetBerserkBuffPositions();
    }

private:
    /**
     * @brief Get river crossing points for FC routing
     */
    std::vector<Position> GetRiverCrossings() const;

    /**
     * @brief Check if position is in water (affects movement speed)
     */
    bool IsInWater(float x, float y, float z) const;
};

} // namespace Playerbot::Coordination::Battleground

#endif // PLAYERBOT_AI_COORDINATION_BG_CTF_TWINPEAKSSCRIPT_H
