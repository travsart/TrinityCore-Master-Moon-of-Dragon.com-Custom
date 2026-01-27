/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_RESOURCERACE_SILVERSHARDMINESSCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_RESOURCERACE_SILVERSHARDMINESSCRIPT_H

#include "ResourceRaceScriptBase.h"
#include "SilvershardMinesData.h"

namespace Playerbot::Coordination::Battleground
{

class SilvershardMinesScript : public ResourceRaceScriptBase
{
public:
    uint32 GetMapId() const override { return SilvershardMines::MAP_ID; }
    std::string GetName() const override { return SilvershardMines::BG_NAME; }
    BGType GetBGType() const override { return BGType::SILVERSHARD_MINES; }
    uint32 GetMaxScore() const override { return SilvershardMines::MAX_SCORE; }
    uint32 GetMaxDuration() const override { return SilvershardMines::MAX_DURATION; }
    uint8 GetTeamSize() const override { return SilvershardMines::TEAM_SIZE; }

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnEvent(const BGScriptEventData& event) override;
    std::vector<BGObjectiveData> GetObjectiveData() const override;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const override;
    std::vector<BGPositionData> GetStrategicPositions() const override;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const override;
    std::vector<BGWorldState> GetInitialWorldStates() const override;
    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId, BGObjectiveState& outState) const override;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states, uint32& allianceScore, uint32& hordeScore) const override;

    // ResourceRaceScriptBase implementations
    uint32 GetCartCount() const override { return SilvershardMines::CART_COUNT; }
    Position GetCartPosition(uint32 cartId) const override;
    float GetCartProgress(uint32 cartId) const override;
    uint32 GetCartController(uint32 cartId) const override;
    bool IsCartContested(uint32 cartId) const override;
    uint32 GetPointsPerCapture() const override { return SilvershardMines::POINTS_PER_CAPTURE; }

    // Track info
    uint32 GetTrackCount() const override { return SilvershardMines::Tracks::TRACK_COUNT; }
    std::vector<Position> GetTrackWaypoints(uint32 trackId) const override;
    uint32 GetCartOnTrack(uint32 trackId) const override;

    // Intersection handling
    bool HasIntersections() const override { return true; }
    std::vector<uint32> GetIntersectionIds() const override;
    uint32 GetIntersectionDecisionTime(uint32 intersectionId) const override;

    // SSM-specific methods
    uint32 GetTrackForCart(uint32 cartId) const;
    bool IsCartNearIntersection(uint32 cartId) const;
    uint32 GetEstimatedCaptureTime(uint32 cartId) const;

private:
    struct SSMCartState
    {
        uint32 trackId;
        float trackProgress;
        bool atIntersection;
        uint32 intersectionId;
    };

    std::map<uint32, SSMCartState> m_ssmCartStates;
    std::map<uint32, Position> m_cartPositions;
    std::map<uint32, uint32> m_cartControllers;
    std::map<uint32, bool> m_cartContested;
};

} // namespace Playerbot::Coordination::Battleground

#endif
