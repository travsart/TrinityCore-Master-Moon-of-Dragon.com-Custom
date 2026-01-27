/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_DEEPWINDGORGESCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_DEEPWINDGORGESCRIPT_H

#include "DominationScriptBase.h"
#include "DeepwindGorgeData.h"

namespace Playerbot::Coordination::Battleground
{

class DeepwindGorgeScript : public DominationScriptBase
{
public:
    uint32 GetMapId() const override { return DeepwindGorge::MAP_ID; }
    std::string GetName() const override { return DeepwindGorge::BG_NAME; }
    BGType GetBGType() const override { return BGType::DEEPWIND_GORGE; }
    uint32 GetMaxScore() const override { return DeepwindGorge::MAX_SCORE; }
    uint32 GetMaxDuration() const override { return DeepwindGorge::MAX_DURATION; }
    uint8 GetTeamSize() const override { return DeepwindGorge::TEAM_SIZE; }
    uint32 GetOptimalNodeCount() const override { return 2; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnEvent(const BGScriptEventData& event) override;
    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;
    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const override;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const override;

    RoleDistribution GetRecommendedRoles(const StrategicDecision& decision, float scoreAdvantage, uint32 timeRemaining) const override;
    void AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 totalObjectives, uint32 timeRemaining) const override;

    // Cart-specific methods
    bool IsCartActive(uint32 cartId) const { return m_activeCart == cartId; }
    uint32 GetActiveCart() const { return m_activeCart; }
    float GetCartProgress(uint32 cartId) const;
    bool IsCartContested(uint32 cartId) const;

protected:
    uint32 GetNodeCount() const override { return DeepwindGorge::NODE_COUNT; }
    BGObjectiveData GetNodeData(uint32 nodeIndex) const override;
    std::vector<uint32> GetTickPointsTable() const override;
    uint32 GetTickInterval() const override { return DeepwindGorge::TICK_INTERVAL; }
    uint32 GetDefaultCaptureTime() const override { return DeepwindGorge::CAPTURE_TIME; }

private:
    uint32 m_activeCart = 0;
    std::map<uint32, float> m_cartProgress;  // cartId -> progress (0.0 - 1.0)
    std::map<uint32, bool> m_cartContested;
};

} // namespace Playerbot::Coordination::Battleground

#endif
