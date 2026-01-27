/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_EPIC_ASHRANSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_EPIC_ASHRANSCRIPT_H

#include "../BGScriptBase.h"
#include "AshranData.h"

namespace Playerbot::Coordination::Battleground
{

class AshranScript : public BGScriptBase
{
public:
    uint32 GetMapId() const override { return Ashran::MAP_ID; }
    std::string GetName() const override { return Ashran::BG_NAME; }
    BGType GetBGType() const override { return BGType::ASHRAN; }
    uint32 GetMaxScore() const override { return 0; }  // No score limit
    uint32 GetMaxDuration() const override { return Ashran::MAX_DURATION; }
    uint8 GetTeamSize() const override { return Ashran::MAX_TEAM_SIZE; }
    bool IsDomination() const override { return false; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnUpdate(uint32 diff) override;
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

    // Ashran-specific methods
    float GetRoadProgress(uint32 faction) const;  // 0.0 = at own base, 1.0 = at enemy base
    bool IsEventActive() const { return m_activeEvent < Ashran::Events::EVENT_COUNT; }
    uint32 GetActiveEvent() const { return m_activeEvent; }
    Position GetEventPosition(uint32 eventId) const;
    bool ShouldParticipateInEvent(uint32 eventId) const;
    bool IsEnemyBaseVulnerable(uint32 attackingFaction) const;

private:
    float m_allianceProgress = 0.0f;  // Road position
    float m_hordeProgress = 0.0f;
    uint32 m_activeEvent = UINT32_MAX;  // Current active side event
    uint32 m_eventTimer = 0;
    std::map<uint32, ObjectiveState> m_controlStates;
};

} // namespace Playerbot::Coordination::Battleground

#endif
