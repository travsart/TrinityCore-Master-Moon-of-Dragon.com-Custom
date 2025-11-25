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
#include "Player.h"
#include "Position.h"
#include <vector>

class Group;
class Unit;

namespace Playerbot
{

// Forward declarations
enum class QuestCompletionStrategy : uint8;
enum class ObjectiveStatus : uint8;
struct QuestObjectiveData;
struct QuestProgressData;

class TC_GAME_API IQuestCompletion
{
public:
    virtual ~IQuestCompletion() = default;

    // Core quest completion management
    virtual bool StartQuestCompletion(uint32 questId, Player* bot) = 0;
    virtual void UpdateQuestProgress(Player* bot) = 0;
    virtual void CompleteQuest(uint32 questId, Player* bot) = 0;
    virtual bool TurnInQuest(uint32 questId, Player* bot) = 0;

    // Objective tracking and execution
    virtual void TrackQuestObjectives(Player* bot) = 0;
    virtual void ExecuteObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual bool IsObjectiveComplete(const QuestObjectiveData& objective) = 0;

    // Objective-specific handlers
    virtual void HandleKillObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleCollectObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleLocationObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleEmoteObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleEscortObjective(Player* bot, QuestObjectiveData& objective) = 0;

    // Navigation and pathfinding
    virtual void NavigateToObjective(Player* bot, const QuestObjectiveData& objective) = 0;
    virtual bool FindObjectiveTarget(Player* bot, QuestObjectiveData& objective) = 0;
    virtual ::std::vector<Position> GetObjectiveLocations(const QuestObjectiveData& objective) = 0;
    virtual Position GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective) = 0;

    // Group coordination for quest completion
    virtual void CoordinateGroupQuestCompletion(Group* group, uint32 questId) = 0;
    virtual void ShareObjectiveProgress(Group* group, uint32 questId) = 0;
    virtual void SynchronizeGroupObjectives(Group* group, uint32 questId) = 0;
    virtual void HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex) = 0;

    // Quest completion optimization
    virtual void OptimizeQuestCompletionOrder(Player* bot) = 0;
    virtual void OptimizeObjectiveSequence(Player* bot, uint32 questId) = 0;
    virtual void FindEfficientCompletionPath(Player* bot, const ::std::vector<uint32>& questIds) = 0;
    virtual void MinimizeTravelTime(Player* bot, const ::std::vector<QuestObjectiveData>& objectives) = 0;

    // Stuck detection and recovery
    virtual void DetectStuckState(Player* bot, uint32 questId) = 0;
    virtual void HandleStuckObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void RecoverFromStuckState(Player* bot, uint32 questId) = 0;
    virtual void SkipProblematicObjective(Player* bot, QuestObjectiveData& objective) = 0;

    // Quest turn-in management
    virtual void ProcessQuestTurnIn(Player* bot, uint32 questId) = 0;
    virtual bool FindQuestTurnInNpc(Player* bot, uint32 questId) = 0;
    virtual void HandleQuestRewardSelection(Player* bot, uint32 questId) = 0;
    virtual void CompleteQuestDialog(Player* bot, uint32 questId) = 0;

    // Performance monitoring
    struct QuestCompletionMetricsSnapshot
    {
        uint32 questsStarted;
        uint32 questsCompleted;
        uint32 questsFailed;
        uint32 objectivesCompleted;
        uint32 stuckInstances;
        float averageCompletionTime;
        float completionSuccessRate;
        float objectiveEfficiency;
        uint32 totalDistanceTraveled;

        float GetCompletionRate() const
        {
            return questsStarted > 0 ? (float)questsCompleted / questsStarted : 0.0f;
        }
    };

    virtual QuestCompletionMetricsSnapshot GetBotCompletionMetrics(uint32 botGuid) = 0;
    virtual QuestCompletionMetricsSnapshot GetGlobalCompletionMetrics() = 0;

    // Quest data analysis
    virtual ::std::vector<uint32> GetActiveQuests(Player* bot) = 0;
    virtual ::std::vector<uint32> GetCompletableQuests(Player* bot) = 0;
    virtual uint32 GetHighestPriorityQuest(Player* bot) = 0;
    virtual float CalculateQuestProgress(uint32 questId, Player* bot) = 0;

    // Configuration and settings
    virtual void SetQuestCompletionStrategy(uint32 botGuid, QuestCompletionStrategy strategy) = 0;
    virtual QuestCompletionStrategy GetQuestCompletionStrategy(uint32 botGuid) = 0;
    virtual void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests) = 0;
    virtual void EnableGroupCoordination(uint32 botGuid, bool enable) = 0;

    // Advanced quest completion features
    virtual void HandleDungeonQuests(Player* bot, uint32 dungeonId) = 0;
    virtual void HandlePvPQuests(Player* bot, uint32 battlegroundId) = 0;
    virtual void HandleSeasonalQuests(Player* bot) = 0;
    virtual void HandleDailyQuests(Player* bot) = 0;

    // Error handling and recovery
    virtual void HandleQuestCompletionError(Player* bot, uint32 questId, const ::std::string& error) = 0;
    virtual void RecoverFromCompletionFailure(Player* bot, uint32 questId) = 0;
    virtual void AbandonUncompletableQuest(Player* bot, uint32 questId) = 0;
    virtual void DiagnoseCompletionIssues(Player* bot, uint32 questId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateBotQuestCompletion(Player* bot, uint32 diff) = 0;
    virtual void CleanupCompletedQuests() = 0;
    virtual void ValidateQuestStates() = 0;
};

} // namespace Playerbot
