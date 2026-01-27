/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Position.h"
#include "GameTime.h"
#include <string>
#include <vector>
#include <atomic>

// Forward declarations
class Player;
class Group;
class Map;

namespace Playerbot
{

// InstanceProgress definition (needs full definition for return by value)
struct InstanceProgress
{
    uint32 groupId;
    uint32 instanceId;
    uint32 mapId;
    uint32 startTime;
    uint32 currentCheckpoint;
    ::std::vector<uint32> completedEncounters;
    ::std::vector<uint32> clearedTrashGroups;
    ::std::vector<uint32> collectedLoot;
    float progressPercentage;
    uint32 estimatedCompletionTime;
    bool isOnTrack;
    ::std::vector<::std::string> progressNotes;

    InstanceProgress() : groupId(0), instanceId(0), mapId(0)
        , startTime(0), currentCheckpoint(0)
        , progressPercentage(0.0f), estimatedCompletionTime(2700000)
        , isOnTrack(true) {}

    InstanceProgress(uint32 gId, uint32 iId, uint32 mId) : groupId(gId), instanceId(iId)
        , mapId(mId), startTime(GameTime::GetGameTimeMS()), currentCheckpoint(0)
        , progressPercentage(0.0f), estimatedCompletionTime(2700000) // 45 minutes
        , isOnTrack(true) {}
};

// CoordinationMetrics snapshot for return by value (no atomic members)
struct CoordinationMetrics
{
    uint32 coordinationEvents = 0;
    uint32 successfulCoordinations = 0;
    uint32 coordinationFailures = 0;
    float averageResponseTime = 2000.0f; // 2 seconds
    float groupSynchronization = 0.9f; // 90% sync rate
    float movementEfficiency = 0.85f;
    uint32 formationBreaks = 0;
    uint32 communicationEvents = 0;

    void Reset()
    {
        coordinationEvents = 0; successfulCoordinations = 0; coordinationFailures = 0;
        averageResponseTime = 2000.0f; groupSynchronization = 0.9f;
        movementEfficiency = 0.85f; formationBreaks = 0; communicationEvents = 0;
    }

    float GetCoordinationSuccessRate() const
    {
        return coordinationEvents > 0 ? static_cast<float>(successfulCoordinations) / coordinationEvents : 0.0f;
    }
};

// Internal thread-safe atomic storage for CoordinationMetrics
struct AtomicCoordinationMetrics
{
    ::std::atomic<uint32> coordinationEvents{0};
    ::std::atomic<uint32> successfulCoordinations{0};
    ::std::atomic<uint32> coordinationFailures{0};
    ::std::atomic<float> averageResponseTime{2000.0f};
    ::std::atomic<float> groupSynchronization{0.9f};
    ::std::atomic<float> movementEfficiency{0.85f};
    ::std::atomic<uint32> formationBreaks{0};
    ::std::atomic<uint32> communicationEvents{0};

    void Reset()
    {
        coordinationEvents = 0; successfulCoordinations = 0; coordinationFailures = 0;
        averageResponseTime = 2000.0f; groupSynchronization = 0.9f;
        movementEfficiency = 0.85f; formationBreaks = 0; communicationEvents = 0;
    }

    CoordinationMetrics GetSnapshot() const
    {
        CoordinationMetrics snapshot;
        snapshot.coordinationEvents = coordinationEvents.load();
        snapshot.successfulCoordinations = successfulCoordinations.load();
        snapshot.coordinationFailures = coordinationFailures.load();
        snapshot.averageResponseTime = averageResponseTime.load();
        snapshot.groupSynchronization = groupSynchronization.load();
        snapshot.movementEfficiency = movementEfficiency.load();
        snapshot.formationBreaks = formationBreaks.load();
        snapshot.communicationEvents = communicationEvents.load();
        return snapshot;
    }

    float GetCoordinationSuccessRate() const
    {
        uint32 total = coordinationEvents.load();
        uint32 successful = successfulCoordinations.load();
        return total > 0 ? static_cast<float>(successful) / total : 0.0f;
    }
};

/**
 * @brief Interface for advanced instance coordination and group management
 *
 * Defines the contract for instance-specific coordination including formation
 * movement, encounter preparation, loot distribution, and group communication.
 */
class TC_GAME_API IInstanceCoordination
{
public:
    virtual ~IInstanceCoordination() = default;

    // Core instance coordination
    virtual void InitializeInstanceCoordination(Group* group, Map* instanceMap) = 0;
    virtual void UpdateInstanceCoordination(Group* group, uint32 diff) = 0;
    virtual void HandleInstanceCompletion(Group* group) = 0;
    virtual void HandleInstanceFailure(Group* group) = 0;

    // Group formation and movement
    virtual void CoordinateGroupMovement(Group* group, const Position& destination) = 0;
    virtual void MaintainDungeonFormation(Group* group) = 0;
    virtual void HandleFormationBreaks(Group* group) = 0;
    virtual void AdaptFormationToTerrain(Group* group, const Position& location) = 0;

    // Encounter preparation and coordination
    virtual void PrepareForEncounter(Group* group, uint32 encounterId) = 0;
    virtual void CoordinateEncounterStart(Group* group, uint32 encounterId) = 0;
    virtual void MonitorEncounterProgress(Group* group, uint32 encounterId) = 0;
    virtual void HandleEncounterRecovery(Group* group, uint32 encounterId) = 0;

    // Resource management and optimization
    virtual void CoordinateResourceUsage(Group* group) = 0;
    virtual void ManageGroupMana(Group* group) = 0;
    virtual void CoordinateRestBreaks(Group* group) = 0;
    virtual void OptimizeGroupEfficiency(Group* group) = 0;

    // Communication and coordination
    virtual void BroadcastInstanceInformation(Group* group, const ::std::string& message) = 0;
    virtual void CoordinateGroupActions(Group* group, const ::std::string& action) = 0;
    virtual void HandleGroupDecisionMaking(Group* group, const ::std::string& decision) = 0;
    virtual void SynchronizeGroupStates(Group* group) = 0;

    // Loot coordination and distribution
    virtual void CoordinateLootDistribution(Group* group, const ::std::vector<uint32>& lootItems) = 0;
    virtual void HandleLootRolling(Group* group, uint32 itemId) = 0;
    virtual void ManageLootPriorities(Group* group) = 0;
    virtual void ResolveeLootConflicts(Group* group, uint32 itemId) = 0;

    // Progress tracking and optimization
    virtual InstanceProgress GetInstanceProgress(uint32 groupId) = 0;
    virtual void UpdateInstanceProgress(Group* group) = 0;
    virtual void AnalyzeProgressEfficiency(Group* group) = 0;

    // Route planning and navigation
    virtual void PlanInstanceRoute(Group* group, const ::std::vector<uint32>& objectiveIds) = 0;
    virtual void UpdateNavigationRoute(Group* group, const Position& currentLocation) = 0;
    virtual void HandleNavigationObstacles(Group* group, const ::std::vector<Position>& obstacles) = 0;
    virtual Position GetNextWaypoint(Group* group) = 0;

    // Safety and emergency coordination
    virtual void MonitorGroupSafety(Group* group) = 0;
    virtual void HandleEmergencySituations(Group* group, const ::std::string& emergency) = 0;
    virtual void CoordinateEmergencyEvacuation(Group* group) = 0;
    virtual void HandlePlayerIncapacitation(Group* group, Player* incapacitatedPlayer) = 0;

    // Performance optimization
    virtual CoordinationMetrics GetGroupCoordinationMetrics(uint32 groupId) = 0;
    virtual CoordinationMetrics GetGlobalCoordinationMetrics() = 0;

    // Advanced coordination features
    virtual void EnablePredictiveCoordination(Group* group, bool enable) = 0;
    virtual void AdaptCoordinationToGroupSkill(Group* group) = 0;
    virtual void OptimizeCoordinationAlgorithms(Group* group) = 0;
    virtual void HandleDynamicGroupChanges(Group* group, Player* newMember = nullptr) = 0;

    // Instance-specific coordination strategies
    virtual void ApplyInstanceSpecificStrategy(Group* group, uint32 instanceId) = 0;
    virtual void HandleInstanceMechanics(Group* group, const ::std::string& mechanic) = 0;
    virtual void AdaptToInstanceDifficulty(Group* group, float difficultyRating) = 0;

    // Configuration and settings
    virtual void SetCoordinationPrecision(uint32 groupId, float precision) = 0;
    virtual void SetFormationStyle(uint32 groupId, const ::std::string& formationStyle) = 0;
    virtual void EnableAdvancedCoordination(uint32 groupId, bool enable) = 0;
    virtual void SetCommunicationLevel(uint32 groupId, uint32 level) = 0;

    // Error handling and recovery
    virtual void HandleCoordinationError(Group* group, const ::std::string& error) = 0;
    virtual void RecoverFromCoordinationFailure(Group* group) = 0;
    virtual void DiagnoseCoordinationIssues(Group* group) = 0;
    virtual void ResetCoordinationState(Group* group) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateGroupCoordination(Group* group, uint32 diff) = 0;
    virtual void CleanupInactiveCoordinations() = 0;
};

} // namespace Playerbot
