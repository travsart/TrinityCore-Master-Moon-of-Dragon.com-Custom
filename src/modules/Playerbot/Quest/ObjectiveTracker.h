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
#include "Threading/LockHierarchy.h"
#include "QuestCompletion.h"
#include "../Core/DI/Interfaces/IObjectiveTracker.h"
#include "Player.h"
#include "QuestDef.h"
#include "Unit.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

namespace Playerbot
{

// ObjectiveState is defined in IObjectiveTracker.h interface

/**
 * @brief Advanced objective tracking system for quest completion monitoring
 *
 * This system provides real-time tracking of quest objectives, progress monitoring,
 * and intelligent adaptation to changing quest states for optimal completion efficiency.
 */
class TC_GAME_API ObjectiveTracker final : public IObjectiveTracker
{
public:
    // Forward declarations (ObjectiveState and ObjectivePriority defined in IObjectiveTracker.h interface)

    explicit ObjectiveTracker(Player* bot);
    ~ObjectiveTracker();
    ObjectiveTracker(ObjectiveTracker const&) = delete;
    ObjectiveTracker& operator=(ObjectiveTracker const&) = delete;

    // Core objective tracking
    void StartTrackingObjective(Player* bot, const QuestObjectiveData& objective) override;
    void StopTrackingObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void UpdateObjectiveTracking(Player* bot, uint32 diff) override;
    void RefreshObjectiveStates(Player* bot) override;
    void RefreshObjectiveState(Player* bot, ObjectiveState& state);

    // Progress monitoring
    void MonitorObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void UpdateProgressMetrics(Player* bot, const QuestObjectiveData& objective) override;
    bool HasProgressStalled(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    float CalculateObjectiveVelocity(Player* bot, uint32 questId, uint32 objectiveIndex) override;

    // Target detection and analysis
    std::vector<uint32> DetectObjectiveTargets(Player* bot, const QuestObjectiveData& objective) override;
    std::vector<uint32> ScanForKillTargets(Player* bot, uint32 creatureId, float radius = 100.0f) override;
    std::vector<uint32> ScanForCollectibles(Player* bot, uint32 itemId, float radius = 50.0f) override;
    std::vector<uint32> ScanForGameObjects(Player* bot, uint32 objectId, float radius = 50.0f) override;

    // Objective state management (ObjectiveState moved to namespace scope above)
    ObjectiveState GetObjectiveState(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void UpdateObjectiveState(Player* bot, const ObjectiveState& state) override;
    std::vector<ObjectiveState> GetActiveObjectives(Player* bot) override;

    // Intelligent objective prioritization (ObjectivePriority defined in IObjectiveTracker.h interface)
    std::vector<ObjectivePriority> CalculateObjectivePriorities(Player* bot) override;
    ObjectivePriority GetHighestPriorityObjective(Player* bot) override;
    void OptimizeObjectiveSequence(Player* bot, std::vector<ObjectivePriority>& priorities) override;

    // Target availability and spawn tracking
    void TrackTargetAvailability(Player* bot, uint32 questId, uint32 targetId) override;
    bool IsTargetAvailable(uint32 targetId, const Position& location, float radius = 100.0f) override;
    uint32 GetTargetRespawnTime(uint32 targetId) override;
    Position GetOptimalTargetLocation(uint32 targetId, const Position& playerPosition) override;

    // Competition and interference management
    void MonitorTargetCompetition(Player* bot, uint32 targetId) override;
    bool IsTargetContested(uint32 targetId, float radius = 50.0f) override;
    void HandleTargetCompetition(Player* bot, uint32 targetId) override;
    std::vector<Position> FindAlternativeTargetLocations(uint32 targetId, const Position& currentLocation) override;

    // Group objective coordination
    void CoordinateGroupObjectives(Group* group, uint32 questId) override;
    void DistributeObjectiveTargets(Group* group, uint32 questId, uint32 objectiveIndex) override;
    void SynchronizeObjectiveProgress(Group* group, uint32 questId) override;
    void HandleObjectiveConflicts(Group* group, uint32 questId, uint32 objectiveIndex) override;

    // Performance analytics (ObjectiveAnalytics defined in IObjectiveTracker.h interface)
    const ObjectiveAnalytics& GetBotObjectiveAnalytics(uint32 botGuid) override;
    const ObjectiveAnalytics& GetGlobalObjectiveAnalytics() override;

    // Advanced tracking features
    void EnablePredictiveTracking(Player* bot, bool enable) override;
    void PredictObjectiveCompletion(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void AdaptTrackingStrategy(Player* bot, const ObjectiveState& state) override;
    void OptimizeTrackingPerformance(Player* bot) override;

    // Error detection and recovery
    void DetectTrackingErrors(Player* bot) override;
    void HandleTrackingFailure(Player* bot, uint32 questId, uint32 objectiveIndex, const std::string& error) override;
    void HandleStuckObjective(Player* bot, ObjectiveState& state) override;
    void RecoverTrackingState(Player* bot, uint32 questId) override;
    void ValidateObjectiveConsistency(Player* bot) override;

    // Data conversion utilities
    QuestObjectiveData ConvertToQuestObjectiveData(const ObjectiveState& state) override;

    // Configuration and settings
    void SetTrackingPrecision(uint32 botGuid, float precision) override; // 0.0 = low, 1.0 = high
    void SetUpdateFrequency(uint32 botGuid, uint32 frequencyMs) override;
    void EnableAdvancedFeatures(uint32 botGuid, bool enable) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateBotTracking(Player* bot, uint32 diff) override;
    void CleanupInactiveTracking() override;

    // Position finding (PUBLIC - used by QuestStrategy for dynamic position refresh)
    Position FindObjectiveTargetLocation(Player* bot, const QuestObjectiveData& objective) override;

private:
    Player* _bot;

    // Thread safety - MUST be locked when accessing any member maps
    // Using recursive_mutex to allow nested calls from the same thread
    mutable std::recursive_mutex _mutex;

    // Tracking data storage
    std::unordered_map<uint32, std::vector<ObjectiveState>> _botObjectiveStates; // botGuid -> objectives
    std::unordered_map<uint32, std::vector<ObjectivePriority>> _botObjectivePriorities; // botGuid -> priorities
    std::unordered_map<uint32, ObjectiveAnalytics> _botAnalytics;
    

    // Target tracking and caching
    struct TargetTrackingData
    {
        uint32 targetId;
        std::vector<Position> knownLocations;
        uint32 lastSeenTime;
        uint32 respawnTime;
        bool isAvailable;
        uint32 competitionLevel; // Number of players competing for this target
        float spawnProbability;

        // Default constructor for std::unordered_map compatibility
        TargetTrackingData() : targetId(0), lastSeenTime(0), respawnTime(0)
            , isAvailable(true), competitionLevel(0), spawnProbability(1.0f) {}

        TargetTrackingData(uint32 id) : targetId(id), lastSeenTime(0), respawnTime(0)
            , isAvailable(true), competitionLevel(0), spawnProbability(1.0f) {}
    };

    std::unordered_map<uint32, TargetTrackingData> _targetTracking; // targetId -> data
    

    // Group coordination data
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _groupObjectiveAssignments; // groupId -> memberGuid -> objectiveIndex
    std::unordered_map<uint32, uint32> _groupObjectiveSyncTime; // groupId -> lastSyncTime

    // Performance tracking
    ObjectiveAnalytics _globalAnalytics;

    // Helper functions
    void UpdateObjectiveProgress(Player* bot, ObjectiveState& state);
    void CalculateObjectiveVelocity(ObjectiveState& state);
    void EstimateCompletionTime(ObjectiveState& state);
    bool ValidateObjectiveState(Player* bot, const ObjectiveState& state);
    void OptimizeObjectiveExecution(Player* bot, ObjectiveState& state);
    uint32 GetCurrentObjectiveProgress(Player* bot, const Quest* quest, uint32 objectiveIndex);
    uint32 AssignObjectiveToGroupMember(Group* group, Player* member, uint32 questId);
    void AssignSpecificTargetToBot(Player* bot, uint32 questId, uint32 objectiveIndex, uint32 targetIndex);

    // Priority calculation helpers
    float CalculateUrgencyFactor(Player* bot, const ObjectiveState& state);
    float CalculateDifficultyFactor(Player* bot, const ObjectiveState& state);
    float CalculateEfficiencyFactor(Player* bot, const ObjectiveState& state);
    float CalculateProximityFactor(Player* bot, const ObjectiveState& state);

    // Target detection algorithms
    std::vector<uint32> ScanAreaForTargets(const Position& center, float radius, uint32 targetId);
    void UpdateTargetAvailability(uint32 targetId, const Position& location);
    void PredictTargetSpawns(uint32 targetId);
    void AnalyzeTargetCompetition(uint32 targetId, const Position& location);

    // Group coordination helpers
    void AssignObjectivesToGroupMembers(Group* group, uint32 questId, uint32 objectiveIndex);
    void BalanceObjectiveLoad(Group* group, uint32 questId);
    void ResolveObjectiveConflicts(Group* group, uint32 questId, uint32 objectiveIndex);

    // Performance optimization
    void OptimizeTrackingAlgorithms();
    void CacheFrequentlyAccessedData();
    void BatchTrackingUpdates();
    void PruneStaleTrackingData();

    // Analytics and reporting
    void UpdateTrackingAnalytics(uint32 botGuid, const ObjectiveState& state, bool wasSuccessful);
    void GenerateTrackingReport(Player* bot);
    void LogTrackingEvent(uint32 botGuid, const std::string& event, const std::string& details = "");

    // Constants
    static constexpr uint32 TRACKING_UPDATE_INTERVAL = 2000; // 2 seconds
    static constexpr uint32 TARGET_SCAN_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 PRIORITY_RECALC_INTERVAL = 10000; // 10 seconds
    static constexpr float DEFAULT_TRACKING_PRECISION = 0.8f;
    static constexpr uint32 MAX_TRACKED_OBJECTIVES = 25;
    static constexpr float STALLED_PROGRESS_THRESHOLD = 0.1f;
    static constexpr uint32 STUCK_DETECTION_TIME = 120000; // 2 minutes
    static constexpr uint32 TARGET_CACHE_DURATION = 300000; // 5 minutes
    static constexpr float COMPETITION_THRESHOLD = 3.0f; // 3+ players = high competition
    static constexpr uint32 OBJECTIVE_TIMEOUT = 1800000; // 30 minutes
    static constexpr float MIN_VELOCITY_THRESHOLD = 0.01f; // objectives per second
};

} // namespace Playerbot