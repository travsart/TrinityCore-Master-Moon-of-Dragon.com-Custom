/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_SEETHINGSHORESCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_SEETHINGSHORESCRIPT_H

#include "DominationScriptBase.h"
#include "SeethingShoreData.h"

namespace Playerbot::Coordination::Battleground
{

class SeethingShoreScript : public DominationScriptBase
{
public:
    uint32 GetMapId() const override { return SeethingShore::MAP_ID; }
    std::string GetName() const override { return SeethingShore::BG_NAME; }
    BGType GetBGType() const override { return BGType::SEETHING_SHORE; }
    uint32 GetMaxScore() const override { return SeethingShore::MAX_SCORE; }
    uint32 GetMaxDuration() const override { return SeethingShore::MAX_DURATION; }
    uint8 GetTeamSize() const override { return SeethingShore::TEAM_SIZE; }
    uint32 GetOptimalNodeCount() const override { return 2; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnUpdate(uint32 diff) override;
    void OnEvent(const BGScriptEventData& event) override;
    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;
    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, BGObjectiveState& outState) const override;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const override;

    RoleDistribution GetRecommendedRoles(const StrategicDecision& decision, float scoreAdvantage, uint32 timeRemaining) const override;
    void AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 totalObjectives, uint32 timeRemaining) const override;

    // Dynamic node methods
    std::vector<SeethingShore::AzeriteNode> GetActiveNodes() const { return m_activeNodes; }
    bool IsNodeActive(uint32 nodeId) const;
    Position GetNearestActiveNode(float x, float y) const;
    uint32 GetActiveNodeCount() const { return static_cast<uint32>(m_activeNodes.size()); }

protected:
    uint32 GetNodeCount() const override { return SeethingShore::SpawnZones::ZONE_COUNT; }
    BGObjectiveData GetNodeData(uint32 nodeIndex) const override;
    std::vector<uint32> GetTickPointsTable() const override;
    uint32 GetTickInterval() const override { return SeethingShore::TICK_INTERVAL; }
    uint32 GetDefaultCaptureTime() const override { return SeethingShore::CAPTURE_TIME; }

private:
    void UpdateActiveNodes();
    void SpawnNewNode();

    std::vector<SeethingShore::AzeriteNode> m_activeNodes;
    uint32 m_nextSpawnTimer = 0;
    uint32 m_nextNodeId = 0;
};

} // namespace Playerbot::Coordination::Battleground

#endif
