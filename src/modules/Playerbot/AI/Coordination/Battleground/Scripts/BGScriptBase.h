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

#ifndef PLAYERBOT_AI_COORDINATION_BG_BGSCRIPTBASE_H
#define PLAYERBOT_AI_COORDINATION_BG_BGSCRIPTBASE_H

#include "IBGScript.h"
#include <unordered_map>

namespace Playerbot::Coordination::Battleground
{

/**
 * @brief Base implementation of IBGScript with common functionality
 *
 * This class provides default implementations for common BG script operations.
 * Derived classes should override specific methods as needed for their BG type.
 */
class BGScriptBase : public IBGScript
{
public:
    BGScriptBase() = default;
    virtual ~BGScriptBase() = default;

    // ========================================================================
    // LIFECYCLE - Default implementations
    // ========================================================================

    void OnLoad(BattlegroundCoordinator* coordinator) override;
    void OnUnload() override;
    void OnUpdate(uint32 diff) override;

    // ========================================================================
    // STRATEGY - Default implementations
    // ========================================================================

    RoleDistribution GetRecommendedRoles(
        const StrategicDecision& decision,
        float scoreAdvantage,
        uint32 timeRemaining) const override;

    void AdjustStrategy(StrategicDecision& decision,
        float scoreAdvantage, uint32 controlledCount,
        uint32 totalObjectives, uint32 timeRemaining) const override;

    uint8 GetObjectiveAttackPriority(uint32 objectiveId,
        BGObjectiveState state, uint32 faction) const override;

    uint8 GetObjectiveDefensePriority(uint32 objectiveId,
        BGObjectiveState state, uint32 faction) const override;

    float CalculateWinProbability(uint32 allianceScore, uint32 hordeScore,
        uint32 timeRemaining, uint32 objectivesControlled, uint32 faction) const override;

    BGPhaseInfo::Phase GetMatchPhase(uint32 timeRemaining,
        uint32 allianceScore, uint32 hordeScore) const override;

    // ========================================================================
    // EVENTS - Default implementations
    // ========================================================================

    void OnEvent(const BGScriptEventData& event) override;
    void OnMatchStart() override;
    void OnMatchEnd(bool victory) override;

    // ========================================================================
    // UTILITY - Default implementations
    // ========================================================================

    Position GetTacticalPosition(
        BGPositionData::PositionType positionType, uint32 faction) const override;

protected:
    // ========================================================================
    // HELPER METHODS FOR DERIVED CLASSES
    // ========================================================================

    /**
     * @brief Get the coordinator (may be null before OnLoad)
     */
    BattlegroundCoordinator* GetCoordinator() const { return m_coordinator; }

    /**
     * @brief Check if we're in an active match
     */
    bool IsMatchActive() const;

    /**
     * @brief Get time elapsed since match start in milliseconds
     */
    uint32 GetElapsedTime() const;

    /**
     * @brief Calculate distance between two positions
     */
    static float CalculateDistance(float x1, float y1, float z1,
                                   float x2, float y2, float z2);

    /**
     * @brief Calculate distance between two positions (2D)
     */
    static float CalculateDistance2D(float x1, float y1, float x2, float y2);

    /**
     * @brief Find the nearest objective from a list
     */
    const BGObjectiveData* FindNearestObjective(
        float x, float y, float z,
        const std::vector<BGObjectiveData>& objectives) const;

    /**
     * @brief Find objectives of a specific type
     */
    std::vector<const BGObjectiveData*> FindObjectivesByType(
        ObjectiveType type,
        const std::vector<BGObjectiveData>& objectives) const;

    /**
     * @brief Get offensive role count recommendation based on strategy
     */
    uint8 GetOffenseRoleCount(const StrategicDecision& decision, uint8 totalBots) const;

    /**
     * @brief Get defensive role count recommendation based on strategy
     */
    uint8 GetDefenseRoleCount(const StrategicDecision& decision, uint8 totalBots) const;

    /**
     * @brief Create a default role distribution for domination BGs
     */
    RoleDistribution CreateDominationRoleDistribution(
        const StrategicDecision& decision,
        uint8 nodeCount,
        uint8 teamSize) const;

    /**
     * @brief Create a default role distribution for CTF BGs
     */
    RoleDistribution CreateCTFRoleDistribution(
        const StrategicDecision& decision,
        bool weHaveFlag,
        bool theyHaveFlag,
        uint8 teamSize) const;

    /**
     * @brief Log event for debugging
     */
    void LogEvent(const BGScriptEventData& event) const;

    // ========================================================================
    // WORLD STATE HELPERS
    // ========================================================================

    /**
     * @brief Cache a world state mapping
     */
    void RegisterWorldStateMapping(int32 stateId, uint32 objectiveId,
                                   BGObjectiveState targetState);

    /**
     * @brief Cache a score world state
     */
    void RegisterScoreWorldState(int32 stateId, bool isAlliance);

    /**
     * @brief Try to interpret a state from cached mappings
     */
    bool TryInterpretFromCache(int32 stateId, int32 value,
                               uint32& outObjectiveId, BGObjectiveState& outState) const;

    // ========================================================================
    // PROTECTED STATE
    // ========================================================================

    // Match tracking
    uint32 m_matchStartTime = 0;
    bool m_matchActive = false;

    // Event counters
    uint32 m_objectivesCaptured = 0;
    uint32 m_objectivesLost = 0;
    uint32 m_flagCaptures = 0;
    uint32 m_playerKills = 0;
    uint32 m_playerDeaths = 0;

    // Cached objective data (populated by derived classes)
    std::vector<BGObjectiveData> m_cachedObjectives;
    std::vector<BGPositionData> m_cachedPositions;
    std::vector<BGWorldState> m_cachedWorldStates;

private:
    // World state interpretation cache
    struct WorldStateMapping
    {
        uint32 objectiveId;
        BGObjectiveState state;
    };

    std::unordered_map<int32, WorldStateMapping> m_worldStateMappings;
    int32 m_allianceScoreState = 0;
    int32 m_hordeScoreState = 0;
};

} // namespace Playerbot::Coordination::Battleground

#endif // PLAYERBOT_AI_COORDINATION_BG_BGSCRIPTBASE_H
