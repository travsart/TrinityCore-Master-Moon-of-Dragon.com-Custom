/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_BATTLEFORGILNEASSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_BATTLEFORGILNEASSCRIPT_H

#include "DominationScriptBase.h"
#include "BattleForGilneasData.h"

namespace Playerbot::Coordination::Battleground
{

class BattleForGilneasScript : public DominationScriptBase
{
public:
    uint32 GetMapId() const override { return BattleForGilneas::MAP_ID; }
    std::string GetName() const override { return BattleForGilneas::BG_NAME; }
    BGType GetBGType() const override { return BGType::BATTLE_FOR_GILNEAS; }
    uint32 GetMaxScore() const override { return BattleForGilneas::MAX_SCORE; }
    uint32 GetMaxDuration() const override { return BattleForGilneas::MAX_DURATION; }
    uint8 GetTeamSize() const override { return BattleForGilneas::TEAM_SIZE; }
    uint32 GetOptimalNodeCount() const override { return 2; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;
    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, BGObjectiveState& outState) const override;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const override;

protected:
    uint32 GetNodeCount() const override { return BattleForGilneas::NODE_COUNT; }
    BGObjectiveData GetNodeData(uint32 nodeIndex) const override;
    std::vector<uint32> GetTickPointsTable() const override;
    uint32 GetTickInterval() const override { return BattleForGilneas::TICK_INTERVAL; }
    uint32 GetDefaultCaptureTime() const override { return BattleForGilneas::CAPTURE_TIME; }
};

} // namespace Playerbot::Coordination::Battleground

#endif
