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
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <chrono>

class Player;
class Group;

namespace Playerbot
{

// Forward declarations
struct QuestObjectiveData;
enum class ObjectiveStatus : uint8;

// ObjectivePriority definition (needs full definition for return by value)
struct ObjectivePriority
{
    uint32 questId;
    uint32 objectiveIndex;
    float priorityScore;
    float urgencyFactor;
    float difficultyFactor;
    float efficiencyFactor;
    float proximityFactor;
    std::string reasoning;

    ObjectivePriority(uint32 qId, uint32 index) : questId(qId), objectiveIndex(index)
        , priorityScore(5.0f), urgencyFactor(1.0f), difficultyFactor(1.0f)
        , efficiencyFactor(1.0f), proximityFactor(1.0f) {}
};

// ObjectiveState definition (needs full definition for return by value)
struct ObjectiveState
{
    uint32 questId;
    uint32 objectiveIndex;
    ObjectiveStatus status;
    uint32 currentProgress;
    uint32 requiredProgress;
    uint32 lastUpdateTime;
    uint32 timeStarted;
    uint32 estimatedTimeRemaining;
    float completionVelocity;
    std::vector<uint32> targetIds;
    Position lastKnownPosition;
    bool isOptimized;
    uint32 failureCount;
    bool isStuck;
    uint32 stuckTime;

    // NOTE: lastUpdateTime defaults to 0 here. Callers MUST set lastUpdateTime = GameTime::GetGameTimeMS()
    // after construction, otherwise CleanupInactiveTracking() will immediately remove the objective
    // (since currentTime - 0 > OBJECTIVE_TIMEOUT for any reasonable game time).
    ObjectiveState() : questId(0), objectiveIndex(0), status(ObjectiveStatus::NOT_STARTED)
        , currentProgress(0), requiredProgress(1), lastUpdateTime(0), timeStarted(0)
        , estimatedTimeRemaining(0), completionVelocity(0.0f), isOptimized(false)
        , failureCount(0), isStuck(false), stuckTime(0) {}

    ObjectiveState(uint32 qId, uint32 index) : questId(qId), objectiveIndex(index)
        , status(ObjectiveStatus::NOT_STARTED), currentProgress(0), requiredProgress(1)
        , lastUpdateTime(0), timeStarted(0), estimatedTimeRemaining(0)
        , completionVelocity(0.0f), isOptimized(false), failureCount(0)
        , isStuck(false), stuckTime(0) {}
};

// ObjectiveAnalytics definition (needs full definition for return by reference)
struct ObjectiveAnalytics
{
    std::atomic<uint32> objectivesStarted{0};
    std::atomic<uint32> objectivesCompleted{0};
    std::atomic<uint32> objectivesFailed{0};
    std::atomic<float> averageCompletionTime{300000.0f}; // 5 minutes
    std::atomic<float> averageSuccessRate{0.9f};
    std::atomic<float> targetDetectionAccuracy{0.85f};
    std::atomic<uint32> targetsFound{0};
    std::atomic<uint32> targetsMissed{0};
    std::chrono::steady_clock::time_point lastAnalyticsUpdate;

    void Reset()
    {
        objectivesStarted = 0; objectivesCompleted = 0; objectivesFailed = 0;
        averageCompletionTime = 300000.0f; averageSuccessRate = 0.9f;
        targetDetectionAccuracy = 0.85f; targetsFound = 0; targetsMissed = 0;
        lastAnalyticsUpdate = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Interface for quest objective tracking and monitoring
 *
 * Provides comprehensive quest objective tracking with progress monitoring,
 * intelligent prioritization, target detection, and group coordination.
 */
class TC_GAME_API IObjectiveTracker
{
public:
    virtual ~IObjectiveTracker() = default;

    // Core objective tracking
    virtual void StartTrackingObjective(Player* bot, const QuestObjectiveData& objective) = 0;
    virtual void StopTrackingObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void UpdateObjectiveTracking(Player* bot, uint32 diff) = 0;
    virtual void RefreshObjectiveStates(Player* bot) = 0;
    virtual void RefreshObjectiveState(Player* bot, ObjectiveState& state) = 0;

    // Progress monitoring
    virtual void MonitorObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void UpdateProgressMetrics(Player* bot, const QuestObjectiveData& objective) = 0;
    virtual bool HasProgressStalled(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual float CalculateObjectiveVelocity(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;

    // Target detection and analysis
    virtual ::std::vector<uint32> DetectObjectiveTargets(Player* bot, const QuestObjectiveData& objective) = 0;
    virtual ::std::vector<uint32> ScanForKillTargets(Player* bot, uint32 creatureId, float radius = 100.0f) = 0;
    virtual ::std::vector<uint32> ScanForCollectibles(Player* bot, uint32 itemId, float radius = 50.0f) = 0;
    virtual ::std::vector<uint32> ScanForGameObjects(Player* bot, uint32 objectId, float radius = 50.0f) = 0;

    // Objective state management
    virtual ObjectiveState GetObjectiveState(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void UpdateObjectiveState(Player* bot, const ObjectiveState& state) = 0;
    virtual ::std::vector<ObjectiveState> GetActiveObjectives(Player* bot) = 0;

    // Intelligent objective prioritization
    virtual ::std::vector<ObjectivePriority> CalculateObjectivePriorities(Player* bot) = 0;
    virtual ObjectivePriority GetHighestPriorityObjective(Player* bot) = 0;
    virtual void OptimizeObjectiveSequence(Player* bot, ::std::vector<ObjectivePriority>& priorities) = 0;

    // Target availability and spawn tracking
    virtual void TrackTargetAvailability(Player* bot, uint32 questId, uint32 targetId) = 0;
    virtual bool IsTargetAvailable(uint32 targetId, const Position& location, float radius = 100.0f) = 0;
    virtual uint32 GetTargetRespawnTime(uint32 targetId) = 0;
    virtual Position GetOptimalTargetLocation(uint32 targetId, const Position& playerPosition) = 0;

    // Competition and interference management
    virtual void MonitorTargetCompetition(Player* bot, uint32 targetId) = 0;
    virtual bool IsTargetContested(uint32 targetId, float radius = 50.0f) = 0;
    virtual void HandleTargetCompetition(Player* bot, uint32 targetId) = 0;
    virtual ::std::vector<Position> FindAlternativeTargetLocations(uint32 targetId, const Position& currentLocation) = 0;

    // Group objective coordination
    virtual void CoordinateGroupObjectives(Group* group, uint32 questId) = 0;
    virtual void DistributeObjectiveTargets(Group* group, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void SynchronizeObjectiveProgress(Group* group, uint32 questId) = 0;
    virtual void HandleObjectiveConflicts(Group* group, uint32 questId, uint32 objectiveIndex) = 0;

    // Performance analytics
    virtual const ObjectiveAnalytics& GetBotObjectiveAnalytics(uint32 botGuid) = 0;
    virtual const ObjectiveAnalytics& GetGlobalObjectiveAnalytics() = 0;

    // Advanced tracking features
    virtual void EnablePredictiveTracking(Player* bot, bool enable) = 0;
    virtual void PredictObjectiveCompletion(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void AdaptTrackingStrategy(Player* bot, const ObjectiveState& state) = 0;
    virtual void OptimizeTrackingPerformance(Player* bot) = 0;

    // Error detection and recovery
    virtual void DetectTrackingErrors(Player* bot) = 0;
    virtual void HandleTrackingFailure(Player* bot, uint32 questId, uint32 objectiveIndex, const ::std::string& error) = 0;
    virtual void HandleStuckObjective(Player* bot, ObjectiveState& state) = 0;
    virtual void RecoverTrackingState(Player* bot, uint32 questId) = 0;
    virtual void ValidateObjectiveConsistency(Player* bot) = 0;

    // Data conversion utilities
    virtual QuestObjectiveData ConvertToQuestObjectiveData(const ObjectiveState& state) = 0;

    // Configuration and settings
    virtual void SetTrackingPrecision(uint32 botGuid, float precision) = 0;
    virtual void SetUpdateFrequency(uint32 botGuid, uint32 frequencyMs) = 0;
    virtual void EnableAdvancedFeatures(uint32 botGuid, bool enable) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateBotTracking(Player* bot, uint32 diff) = 0;
    virtual void CleanupInactiveTracking() = 0;

    // Position finding
    virtual Position FindObjectiveTargetLocation(Player* bot, const QuestObjectiveData& objective) = 0;
};

} // namespace Playerbot
