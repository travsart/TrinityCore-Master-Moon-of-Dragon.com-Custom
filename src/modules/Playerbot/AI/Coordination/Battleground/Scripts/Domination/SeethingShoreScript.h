/*
 * Copyright (C) 2025+ TrinityCore Playerbot Integration
 *
 * Enterprise-Grade Seething Shore Script Header
 * ~200 lines - Complete interface for dynamic node BG coordination
 */

#ifndef PLAYERBOT_AI_COORDINATION_BG_DOMINATION_SEETHINGSHORESCRIPT_H
#define PLAYERBOT_AI_COORDINATION_BG_DOMINATION_SEETHINGSHORESCRIPT_H

#include "DominationScriptBase.h"
#include "SeethingShoreData.h"
#include <unordered_set>

namespace Playerbot::Coordination::Battleground
{

// Game phase enumeration for phase-aware strategy
enum class SeethingShorePhase
{
    OPENING,     // First 60 seconds - establish early control
    MID_GAME,    // Middle period - dynamic response
    LATE_GAME,   // Final push - score-focused
    DESPERATE    // Behind significantly - all-in on nodes
};

class SeethingShoreScript : public DominationScriptBase
{
public:
    // ========================================================================
    // BASIC ACCESSORS (Override DominationScriptBase)
    // ========================================================================

    uint32 GetMapId() const { return SeethingShore::MAP_ID; }
    std::string GetName() const { return SeethingShore::BG_NAME; }
    BGType GetBGType() const { return BGType::SEETHING_SHORE; }
    uint32 GetMaxScore() const { return SeethingShore::MAX_SCORE; }
    uint32 GetMaxDuration() const { return SeethingShore::MAX_DURATION; }
    uint8 GetTeamSize() const { return SeethingShore::TEAM_SIZE; }
    uint32 GetOptimalNodeCount() const { return 2; }  // Control 2 of 3 active nodes

    // ========================================================================
    // LIFECYCLE METHODS
    // ========================================================================

    void OnLoad(BattlegroundCoordinator* coordinator);
    void OnMatchStart();
    void OnMatchEnd(bool victory);
    void OnUpdate(uint32 diff);
    void OnEvent(const BGScriptEventData& event);

    // ========================================================================
    // OBJECTIVE DATA PROVIDERS
    // ========================================================================

    std::vector<BGObjectiveData> GetObjectiveData() const;
    std::vector<BGPositionData> GetSpawnPositions(uint32 faction) const;
    std::vector<BGPositionData> GetStrategicPositions() const;
    std::vector<BGPositionData> GetGraveyardPositions(uint32 faction) const;
    std::vector<BGWorldState> GetInitialWorldStates() const;

    // ========================================================================
    // WORLD STATE INTERPRETATION
    // ========================================================================

    bool InterpretWorldState(int32 stateId, int32 value, uint32& outObjectiveId,
                            BGObjectiveState& outState) const;
    void GetScoreFromWorldStates(const std::map<int32, int32>& states,
                                uint32& allianceScore, uint32& hordeScore) const;

    // ========================================================================
    // STRATEGY AND ROLE DISTRIBUTION
    // ========================================================================

    RoleDistribution GetRecommendedRoles(const StrategicDecision& decision,
                                        float scoreAdvantage, uint32 timeRemaining) const;
    void AdjustStrategy(StrategicDecision& decision, float scoreAdvantage,
                       uint32 controlledCount, uint32 totalObjectives,
                       uint32 timeRemaining) const;

    // ========================================================================
    // DYNAMIC NODE METHODS (Seething Shore-specific)
    // ========================================================================

    // Active node tracking
    std::vector<SeethingShore::AzeriteNode> GetActiveNodes() const { return m_activeNodes; }
    bool IsNodeActive(uint32 nodeId) const;
    bool IsZoneActive(uint32 zoneId) const;
    uint32 GetActiveNodeCount() const { return static_cast<uint32>(m_activeNodes.size()); }

    // Node position queries
    Position GetNearestActiveNode(float x, float y) const;
    uint32 GetNearestActiveZone(float x, float y) const;
    std::vector<uint32> GetActiveZoneIds() const;

    // Zone defense positions for currently active nodes only
    std::vector<BGPositionData> GetActiveZoneDefensePositions() const;

    // Zone priority and value assessment
    uint32 GetZonePriority(uint32 zoneId, uint32 faction) const;
    float GetZoneStrategicValue(uint32 zoneId, bool isContested) const;
    std::vector<uint32> GetPrioritizedActiveZones(uint32 faction) const;

    // Split/group decision logic
    bool ShouldSplitTeam() const;
    uint32 GetRecommendedSplitCount() const;

    // ========================================================================
    // POSITIONAL DATA PROVIDERS
    // ========================================================================

    // Chokepoints between zones
    std::vector<BGPositionData> GetChokepoints() const;

    // Sniper/overlook positions
    std::vector<BGPositionData> GetSniperPositions() const;

    // Buff locations
    std::vector<BGPositionData> GetBuffPositions() const;

    // Ambush positions (faction-specific interception points)
    std::vector<BGPositionData> GetAmbushPositions(uint32 faction) const;

    // Route between two zones
    std::vector<Position> GetRotationPath(uint32 fromZone, uint32 toZone) const;

    // Adjacent zones for rotation decisions
    std::vector<uint32> GetAdjacentZones(uint32 zoneId) const;

    // ========================================================================
    // PHASE AND STATE QUERIES
    // ========================================================================

    SeethingShorePhase GetCurrentPhase() const { return m_currentPhase; }
    uint32 GetMatchElapsedTime() const { return m_matchElapsedTime; }
    uint32 GetMatchRemainingTime() const;

    // Contested node tracking
    bool IsNodeContested(uint32 nodeId) const;
    std::vector<uint32> GetContestedNodeIds() const;

protected:
    // ========================================================================
    // DOMINATION BASE OVERRIDES
    // ========================================================================

    uint32 GetNodeCount() const { return SeethingShore::SpawnZones::ZONE_COUNT; }
    BGObjectiveData GetNodeData(uint32 nodeIndex) const;
    std::vector<uint32> GetTickPointsTable() const;
    uint32 GetTickInterval() const { return SeethingShore::TICK_INTERVAL; }
    uint32 GetDefaultCaptureTime() const { return SeethingShore::CAPTURE_TIME; }

private:
    // ========================================================================
    // INTERNAL UPDATE METHODS
    // ========================================================================

    void UpdateActiveNodes();
    void SpawnNewNode();
    void RemoveCapturedNode(uint32 nodeId);
    void UpdatePhase(uint32 timeRemaining);
    void UpdateContestedStatus();

    // ========================================================================
    // INTERNAL STRATEGY HELPERS
    // ========================================================================

    void ApplyOpeningPhaseStrategy(StrategicDecision& decision, uint32 faction) const;
    void ApplyMidGameStrategy(StrategicDecision& decision, float scoreAdvantage) const;
    void ApplyLateGameStrategy(StrategicDecision& decision, float scoreAdvantage,
                               uint32 timeRemaining) const;
    void ApplyDesperateStrategy(StrategicDecision& decision) const;

    // Calculate best nodes to target based on faction position and active nodes
    std::vector<uint32> CalculateBestTargetZones(uint32 faction, uint32 count) const;

    // ========================================================================
    // STATE TRACKING
    // ========================================================================

    // Active node list (dynamically spawned)
    std::vector<SeethingShore::AzeriteNode> m_activeNodes;

    // Contested node tracking
    std::unordered_set<uint32> m_contestedNodeIds;

    // Node spawn management
    uint32 m_nextSpawnTimer = 0;
    uint32 m_nextNodeId = 0;
    uint32 m_nodeSpawnCooldown = 0;

    // Match timing
    uint32 m_matchElapsedTime = 0;
    uint32 m_matchStartTime = 0;
    bool m_matchActive = false;

    // Phase tracking
    SeethingShorePhase m_currentPhase = SeethingShorePhase::OPENING;

    // Recently captured zones (cooldown before respawn)
    std::map<uint32, uint32> m_zoneCaptureTimestamps;

    // Strategy state
    mutable uint32 m_cachedFaction = 0;
    mutable std::vector<uint32> m_cachedPriorityZones;
};

} // namespace Playerbot::Coordination::Battleground

#endif
