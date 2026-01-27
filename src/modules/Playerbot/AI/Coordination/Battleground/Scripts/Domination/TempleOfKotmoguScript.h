/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_TEMPLEOFKOTMOGUSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_TEMPLEOFKOTMOGUSCRIPT_H

#include "DominationScriptBase.h"
#include "TempleOfKotmoguData.h"

namespace Playerbot::Coordination::Battleground
{

class TempleOfKotmoguScript : public DominationScriptBase
{
public:
    uint32 GetMapId() const override { return TempleOfKotmogu::MAP_ID; }
    std::string GetName() const override { return TempleOfKotmogu::BG_NAME; }
    BGType GetBGType() const override { return BGType::TEMPLE_OF_KOTMOGU; }
    uint32 GetMaxScore() const override { return TempleOfKotmogu::MAX_SCORE; }
    uint32 GetMaxDuration() const override { return TempleOfKotmogu::MAX_DURATION; }
    uint8 GetTeamSize() const override { return TempleOfKotmogu::TEAM_SIZE; }
    uint32 GetOptimalNodeCount() const override { return 2; }  // 2 orbs is good

    void OnLoad(BattlegroundCoordinator* coordinator) override;
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

    // Orb-specific methods
    bool IsOrbHeld(uint32 orbId) const { return m_orbHolders.find(orbId) != m_orbHolders.end(); }
    ObjectGuid GetOrbHolder(uint32 orbId) const;
    bool IsPlayerHoldingOrb(ObjectGuid guid) const;
    uint32 GetOrbsHeldByFaction(uint32 faction) const;
    bool IsInCenter(float x, float y) const;

protected:
    uint32 GetNodeCount() const override { return TempleOfKotmogu::ORB_COUNT; }
    BGObjectiveData GetNodeData(uint32 nodeIndex) const override;
    std::vector<uint32> GetTickPointsTable() const override;
    uint32 GetTickInterval() const override { return TempleOfKotmogu::TICK_INTERVAL; }
    uint32 GetDefaultCaptureTime() const override { return 0; }  // Orbs are instant pickup

private:
    BGObjectiveData GetOrbData(uint32 orbId) const;

    std::map<uint32, ObjectGuid> m_orbHolders;  // orbId -> holder guid
    std::map<ObjectGuid, uint32> m_playerOrbs;  // player guid -> orbId
};

} // namespace Playerbot::Coordination::Battleground

#endif
