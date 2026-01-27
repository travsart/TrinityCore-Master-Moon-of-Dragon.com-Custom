/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_SIEGE_STRANDOFTHEANCIENTSSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_SIEGE_STRANDOFTHEANCIENTSSCRIPT_H

#include "SiegeScriptBase.h"
#include "StrandOfTheAncientsData.h"

namespace Playerbot::Coordination::Battleground
{

class StrandOfTheAncientsScript : public SiegeScriptBase
{
public:
    uint32 GetMapId() const override { return StrandOfTheAncients::MAP_ID; }
    std::string GetName() const override { return StrandOfTheAncients::BG_NAME; }
    BGType GetBGType() const override { return BGType::STRAND_OF_THE_ANCIENTS; }
    uint32 GetMaxScore() const override { return 0; }  // Time-based win
    uint32 GetMaxDuration() const override { return StrandOfTheAncients::MAX_DURATION; }
    uint8 GetTeamSize() const override { return StrandOfTheAncients::TEAM_SIZE; }
    bool HasVehicles() const override { return true; }
    bool HasRounds() const override { return true; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnEvent(const BGScriptEventData& event) override;
    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGVehicleData> GetVehicleData() const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;
    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, ObjectiveState& outState) const override;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const override;

    RoleDistribution GetRecommendedRoles(const StrategicDecision& decision, float scoreAdvantage, uint32 timeRemaining) const override;
    void AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 totalObjectives, uint32 timeRemaining) const override;

    // Siege implementations (not used in SOTA - time-based)
    uint32 GetBossEntry(uint32 /*faction*/) const override { return StrandOfTheAncients::TITAN_RELIC_ENTRY; }
    Position GetBossPosition(uint32 /*faction*/) const override { return StrandOfTheAncients::GetRelicPosition(); }
    bool CanAttackBoss(uint32 /*faction*/) const override { return IsAncientGateDestroyed(); }

    // SOTA-specific methods
    bool IsAttacker() const { return m_isAttacker; }
    bool IsDefender() const { return !m_isAttacker; }
    uint32 GetCurrentRound() const { return m_currentRound; }
    bool IsGateDestroyed(uint32 gateId) const { return m_destroyedGates.find(gateId) != m_destroyedGates.end(); }
    bool IsAncientGateDestroyed() const { return IsGateDestroyed(StrandOfTheAncients::Gates::ANCIENT_GATE); }
    uint32 GetDestroyedGateCount() const { return static_cast<uint32>(m_destroyedGates.size()); }
    StrandOfTheAncients::AttackPath GetRecommendedPath() const;
    std::vector<uint32> GetNextTargetGates() const;
    bool CanAttackGate(uint32 gateId) const;

protected:
    std::vector<BGObjectiveData> GetTowerData() const override;
    std::vector<BGObjectiveData> GetGraveyardData() const override;
    std::vector<BGObjectiveData> GetGateData() const override;

private:
    bool m_isAttacker = false;
    uint32 m_currentRound = 1;
    uint32 m_round1Time = 0;  // Time attacker took in round 1
    std::set<uint32> m_destroyedGates;
    std::set<uint32> m_capturedGraveyards;
};

} // namespace Playerbot::Coordination::Battleground

#endif
