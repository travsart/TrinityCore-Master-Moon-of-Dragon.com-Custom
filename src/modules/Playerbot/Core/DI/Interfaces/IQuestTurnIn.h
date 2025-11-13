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

class Player;
class Group;

namespace Playerbot
{

// Forward declarations
enum class TurnInStrategy : uint8;
enum class RewardSelectionStrategy : uint8;
struct QuestRewardItem;
struct QuestTurnInData;
struct TurnInBatch;
struct TurnInMetrics;

class TC_GAME_API IQuestTurnIn
{
public:
    virtual ~IQuestTurnIn() = default;

    // Core turn-in functionality
    virtual bool TurnInQuest(uint32 questId, Player* bot) = 0;
    virtual void ProcessQuestTurnIn(Player* bot, uint32 questId) = 0;
    virtual void ProcessBatchTurnIn(Player* bot, const TurnInBatch& batch) = 0;
    virtual void ScheduleQuestTurnIn(Player* bot, uint32 questId, uint32 delayMs = 0) = 0;

    // Quest completion detection
    virtual ::std::vector<uint32> GetCompletedQuests(Player* bot) = 0;
    virtual bool IsQuestReadyForTurnIn(uint32 questId, Player* bot) = 0;
    virtual void MonitorQuestCompletion(Player* bot) = 0;
    virtual void HandleQuestCompletion(Player* bot, uint32 questId) = 0;

    // Turn-in planning and optimization
    virtual void PlanOptimalTurnInRoute(Player* bot) = 0;
    virtual TurnInBatch CreateTurnInBatch(Player* bot, const ::std::vector<uint32>& questIds) = 0;
    virtual void OptimizeTurnInSequence(Player* bot, ::std::vector<QuestTurnInData>& turnIns) = 0;
    virtual void MinimizeTurnInTravel(Player* bot) = 0;

    // Quest giver location and navigation
    virtual bool FindQuestTurnInNpc(Player* bot, uint32 questId) = 0;
    virtual Position GetQuestTurnInLocation(uint32 questId) = 0;
    virtual bool NavigateToQuestGiver(Player* bot, uint32 questGiverGuid) = 0;
    virtual bool IsAtQuestGiver(Player* bot, uint32 questGiverGuid) = 0;

    // Reward selection and optimization
    virtual void AnalyzeQuestRewards(QuestTurnInData& turnInData, Player* bot) = 0;
    virtual uint32 SelectOptimalReward(const ::std::vector<QuestRewardItem>& rewards, Player* bot, RewardSelectionStrategy strategy) = 0;
    virtual void EvaluateItemUpgrades(const ::std::vector<QuestRewardItem>& rewards, Player* bot) = 0;
    virtual float CalculateItemValue(const QuestRewardItem& reward, Player* bot) = 0;

    // Group turn-in coordination
    virtual void CoordinateGroupTurnIns(Group* group) = 0;
    virtual void SynchronizeGroupRewardSelection(Group* group, uint32 questId) = 0;
    virtual void HandleGroupTurnInConflicts(Group* group, uint32 questId) = 0;
    virtual void ShareTurnInProgress(Group* group) = 0;

    // Turn-in dialog and interaction
    virtual void HandleQuestGiverDialog(Player* bot, uint32 questGiverGuid, uint32 questId) = 0;
    virtual void SelectQuestReward(Player* bot, uint32 questId, uint32 rewardIndex) = 0;
    virtual void ConfirmQuestTurnIn(Player* bot, uint32 questId) = 0;
    virtual void HandleTurnInDialog(Player* bot, uint32 questId) = 0;

    // Advanced turn-in strategies
    virtual void ExecuteImmediateTurnInStrategy(Player* bot) = 0;
    virtual void ExecuteBatchTurnInStrategy(Player* bot) = 0;
    virtual void ExecuteOptimalRoutingStrategy(Player* bot) = 0;
    virtual void ExecuteGroupCoordinationStrategy(Player* bot) = 0;
    virtual void ExecuteRewardOptimizationStrategy(Player* bot) = 0;
    virtual void ExecuteChainContinuationStrategy(Player* bot) = 0;

    // Turn-in performance monitoring
    struct TurnInMetricsSnapshot
    {
        uint32 questsTurnedIn;
        uint32 turnInAttempts;
        uint32 successfulTurnIns;
        uint32 failedTurnIns;
        float averageTurnInTime;
        float turnInSuccessRate;
        uint32 totalTravelDistance;
        uint32 rewardsSelected;
        float rewardSelectionAccuracy;

        float GetSuccessRate() const {
            return turnInAttempts > 0 ? (float)successfulTurnIns / turnInAttempts : 0.0f;
        }
    };

    virtual TurnInMetricsSnapshot GetBotTurnInMetrics(uint32 botGuid) = 0;
    virtual TurnInMetricsSnapshot GetGlobalTurnInMetrics() = 0;

    // Quest chain management
    virtual void HandleQuestChainProgression(Player* bot, uint32 completedQuestId) = 0;
    virtual uint32 GetNextQuestInChain(uint32 completedQuestId) = 0;
    virtual void AutoAcceptFollowUpQuests(Player* bot, uint32 completedQuestId) = 0;
    virtual void PrioritizeChainQuests(Player* bot) = 0;

    // Configuration and settings
    virtual void SetTurnInStrategy(uint32 botGuid, TurnInStrategy strategy) = 0;
    virtual TurnInStrategy GetTurnInStrategy(uint32 botGuid) = 0;
    virtual void SetRewardSelectionStrategy(uint32 botGuid, RewardSelectionStrategy strategy) = 0;
    virtual RewardSelectionStrategy GetRewardSelectionStrategy(uint32 botGuid) = 0;
    virtual void SetBatchTurnInThreshold(uint32 botGuid, uint32 threshold) = 0;

    // Error handling and recovery
    virtual void HandleTurnInError(Player* bot, uint32 questId, const ::std::string& error) = 0;
    virtual void RecoverFromTurnInFailure(Player* bot, uint32 questId) = 0;
    virtual void RetryFailedTurnIn(Player* bot, uint32 questId) = 0;
    virtual void ValidateTurnInState(Player* bot, uint32 questId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateBotTurnIns(Player* bot, uint32 diff) = 0;
    virtual void ProcessScheduledTurnIns() = 0;
    virtual void CleanupCompletedTurnIns() = 0;
};

} // namespace Playerbot
