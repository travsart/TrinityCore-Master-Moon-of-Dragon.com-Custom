/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_SIEGE_ISLEOFCONQUESTSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_SIEGE_ISLEOFCONQUESTSCRIPT_H

#include "SiegeScriptBase.h"
#include "IsleOfConquestData.h"

namespace Playerbot::Coordination::Battleground
{

class IsleOfConquestScript : public SiegeScriptBase
{
public:
    uint32 GetMapId() const override { return IsleOfConquest::MAP_ID; }
    std::string GetName() const override { return IsleOfConquest::BG_NAME; }
    BGType GetBGType() const override { return BGType::ISLE_OF_CONQUEST; }
    uint32 GetMaxScore() const override { return IsleOfConquest::STARTING_REINFORCEMENTS; }
    uint32 GetMaxDuration() const override { return IsleOfConquest::MAX_DURATION; }
    uint8 GetTeamSize() const override { return IsleOfConquest::TEAM_SIZE; }
    bool HasVehicles() const override { return true; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnEvent(const BGScriptEventData& event) override;
    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGVehicleData> GetVehicleData() const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;
    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, BGObjectiveState& outState) const override;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const override;

    RoleDistribution GetRecommendedRoles(const StrategicDecision& decision, float scoreAdvantage, uint32 timeRemaining) const override;
    void AdjustStrategy(StrategicDecision& decision, float scoreAdvantage, uint32 controlledCount, uint32 totalObjectives, uint32 timeRemaining) const override;

    // Siege implementations
    uint32 GetBossEntry(uint32 faction) const override;
    Position GetBossPosition(uint32 faction) const override;
    bool CanAttackBoss(uint32 faction) const override;

    // IOC-specific methods
    bool IsGateDestroyed(uint32 gateId) const { return m_destroyedGates.find(gateId) != m_destroyedGates.end(); }
    uint32 GetIntactGateCount(uint32 faction) const;
    bool CanAccessKeep(uint32 targetFaction) const;
    bool IsNodeControlled(uint32 nodeId, uint32 faction) const;
    std::vector<uint32> GetAvailableVehicles(uint32 faction) const;

protected:
    std::vector<BGObjectiveData> GetTowerData() const override;
    std::vector<BGObjectiveData> GetGraveyardData() const override;
    std::vector<BGObjectiveData> GetGateData() const override;

private:
    std::vector<BGObjectiveData> GetNodeData() const;

    std::map<uint32, BGObjectiveState> m_nodeStates;
    std::set<uint32> m_destroyedGates;
};

} // namespace Playerbot::Coordination::Battleground

#endif
