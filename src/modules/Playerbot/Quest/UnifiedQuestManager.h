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
#include "Core/DI/Interfaces/IUnifiedQuestManager.h"
#include "QuestPickup.h"
#include "QuestCompletion.h"
#include "QuestValidation.h"
#include "QuestTurnIn.h"
#include "DynamicQuestSystem.h"
#include "Player.h"
#include "Group.h"
#include "Position.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Unified quest management system
 *
 * Consolidates five separate managers into one cohesive system:
 * - QuestPickup: Quest discovery and acceptance
 * - QuestCompletion: Objective tracking and execution
 * - QuestValidation: Requirement validation
 * - QuestTurnIn: Quest completion and reward selection
 * - DynamicQuestSystem: Dynamic quest assignment and optimization
 *
 * **Architecture:**
 * ```
 * UnifiedQuestManager
 *   > PickupModule      (quest discovery, acceptance)
 *   > CompletionModule  (objective tracking, execution)
 *   > ValidationModule  (requirement validation)
 *   > TurnInModule      (quest turn-in, rewards)
 *   > DynamicModule     (quest assignment, optimization)
 * ```
 *
 * **Thread Safety:**
 * - Uses OrderedMutex<QUEST_MANAGER> for all operations
 * - Modules share data through thread-safe interfaces
 * - Lock ordering prevents deadlocks
 *
 * **Migration Path:**
 * - Old managers (QuestPickup, QuestCompletion, etc.) still work
 * - New code should use UnifiedQuestManager
 * - Gradually migrate callsites over time
 * - Eventually deprecate old managers
 */
class TC_GAME_API UnifiedQuestManager final : public IUnifiedQuestManager
{
public:
    static UnifiedQuestManager* instance();

    // Singleton management
    UnifiedQuestManager();
    ~UnifiedQuestManager() override;

    // Non-copyable, non-movable
    UnifiedQuestManager(UnifiedQuestManager const&) = delete;
    UnifiedQuestManager& operator=(UnifiedQuestManager const&) = delete;
    UnifiedQuestManager(UnifiedQuestManager&&) = delete;
    UnifiedQuestManager& operator=(UnifiedQuestManager&&) = delete;

    // ========================================================================
    // PICKUP MODULE INTERFACE
    // ========================================================================

    bool PickupQuest(uint32 questId, Player* bot, uint32 questGiverGuid = 0) override;
    bool PickupQuestFromGiver(Player* bot, uint32 questGiverGuid, uint32 questId = 0) override;
    void PickupAvailableQuests(Player* bot) override;
    void PickupQuestsInArea(Player* bot, float radius = 50.0f) override;
    std::vector<uint32> DiscoverNearbyQuests(Player* bot, float scanRadius = 100.0f) override;
    std::vector<QuestGiverInfo> ScanForQuestGivers(Player* bot, float scanRadius = 100.0f) override;
    std::vector<uint32> GetAvailableQuestsFromGiver(uint32 questGiverGuid, Player* bot) override;
    QuestEligibility CheckQuestEligibility(uint32 questId, Player* bot) override;
    bool CanAcceptQuest(uint32 questId, Player* bot) override;
    bool MeetsQuestRequirements(uint32 questId, Player* bot) override;
    std::vector<uint32> FilterQuests(const std::vector<uint32>& questIds, Player* bot, const QuestPickupFilter& filter) override;
    std::vector<uint32> PrioritizeQuests(const std::vector<uint32>& questIds, Player* bot, QuestAcceptanceStrategy strategy) override;
    bool ShouldAcceptQuest(uint32 questId, Player* bot) override;

    // ========================================================================
    // COMPLETION MODULE INTERFACE
    // ========================================================================

    bool StartQuestCompletion(uint32 questId, Player* bot) override;
    void UpdateQuestProgress(Player* bot) override;
    void CompleteQuest(uint32 questId, Player* bot) override;
    bool TurnInQuest(uint32 questId, Player* bot) override;
    void TrackQuestObjectives(Player* bot) override;
    void ExecuteObjective(Player* bot, QuestObjectiveData& objective) override;
    void UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    bool IsObjectiveComplete(const QuestObjectiveData& objective) override;
    void HandleKillObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleCollectObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleLocationObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleEmoteObjective(Player* bot, QuestObjectiveData& objective) override;
    void HandleEscortObjective(Player* bot, QuestObjectiveData& objective) override;
    void NavigateToObjective(Player* bot, const QuestObjectiveData& objective) override;
    bool FindObjectiveTarget(Player* bot, QuestObjectiveData& objective) override;
    std::vector<Position> GetObjectiveLocations(const QuestObjectiveData& objective) override;
    Position GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective) override;
    void CoordinateGroupQuestCompletion(Group* group, uint32 questId) override;
    void ShareObjectiveProgress(Group* group, uint32 questId) override;
    void SynchronizeGroupObjectives(Group* group, uint32 questId) override;
    void HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex) override;
    void OptimizeQuestCompletionOrder(Player* bot) override;
    void OptimizeObjectiveSequence(Player* bot, uint32 questId) override;
    void FindEfficientCompletionPath(Player* bot, const std::vector<uint32>& questIds) override;
    void MinimizeTravelTime(Player* bot, const std::vector<QuestObjectiveData>& objectives) override;
    void DetectStuckState(Player* bot, uint32 questId) override;
    void HandleStuckObjective(Player* bot, QuestObjectiveData& objective) override;
    void RecoverFromStuckState(Player* bot, uint32 questId) override;
    void SkipProblematicObjective(Player* bot, QuestObjectiveData& objective) override;

    // ========================================================================
    // VALIDATION MODULE INTERFACE
    // ========================================================================

    bool ValidateQuest(uint32 questId, Player* bot) override;
    bool ValidateQuestRequirements(uint32 questId, Player* bot) override;
    std::vector<std::string> GetValidationErrors(uint32 questId, Player* bot) override;
    bool ValidateLevelRequirements(uint32 questId, Player* bot) override;
    bool ValidateClassRequirements(uint32 questId, Player* bot) override;
    bool ValidateRaceRequirements(uint32 questId, Player* bot) override;
    bool ValidateSkillRequirements(uint32 questId, Player* bot) override;
    bool ValidateQuestPrerequisites(uint32 questId, Player* bot) override;
    bool ValidateQuestChain(uint32 questId, Player* bot) override;
    bool HasCompletedPrerequisites(uint32 questId, Player* bot) override;
    std::vector<uint32> GetMissingPrerequisites(uint32 questId, Player* bot) override;
    bool ValidateReputationRequirements(uint32 questId, Player* bot) override;
    bool ValidateFactionRequirements(uint32 questId, Player* bot) override;
    bool HasRequiredReputation(uint32 questId, Player* bot, uint32 factionId) override;
    bool ValidateItemRequirements(uint32 questId, Player* bot) override;
    bool HasRequiredItems(uint32 questId, Player* bot) override;
    bool HasInventorySpace(uint32 questId, Player* bot) override;
    std::vector<uint32> GetMissingQuestItems(uint32 questId, Player* bot) override;
    bool ValidateQuestAvailability(uint32 questId, Player* bot) override;
    bool ValidateSeasonalAvailability(uint32 questId) override;
    bool ValidateDailyQuestLimits(uint32 questId, Player* bot) override;
    bool ValidateQuestTimer(uint32 questId, Player* bot) override;
    bool ValidateZoneRequirements(uint32 questId, Player* bot) override;
    bool ValidateAreaRequirements(uint32 questId, Player* bot) override;
    bool IsInCorrectZone(uint32 questId, Player* bot) override;
    bool CanQuestBeStartedAtLocation(uint32 questId, const Position& location) override;
    bool ValidateGroupRequirements(uint32 questId, Player* bot) override;
    bool ValidatePartyQuestRequirements(uint32 questId, Player* bot) override;
    bool ValidateRaidQuestRequirements(uint32 questId, Player* bot) override;
    bool CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver) override;
    bool ValidateWithContext(ValidationContext& context) override;
    bool ValidateQuestObjectives(uint32 questId, Player* bot) override;
    bool ValidateQuestRewards(uint32 questId, Player* bot) override;
    bool ValidateQuestDifficulty(uint32 questId, Player* bot) override;
    ValidationResult GetCachedValidation(uint32 questId, uint32 botGuid) override;
    void CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result) override;
    void InvalidateValidationCache(uint32 botGuid) override;
    void CleanupExpiredCache() override;
    std::unordered_map<uint32, ValidationResult> ValidateMultipleQuests(
        const std::vector<uint32>& questIds, Player* bot) override;
    std::vector<uint32> FilterValidQuests(const std::vector<uint32>& questIds, Player* bot) override;
    std::vector<uint32> GetEligibleQuests(Player* bot, const std::vector<uint32>& candidates) override;
    std::string GetDetailedValidationReport(uint32 questId, Player* bot) override;
    void LogValidationFailure(uint32 questId, Player* bot, const std::string& reason) override;
    std::vector<std::string> GetRecommendationsForFailedQuest(uint32 questId, Player* bot) override;

    // ========================================================================
    // TURNIN MODULE INTERFACE
    // ========================================================================

    bool TurnInQuestWithReward(uint32 questId, Player* bot) override;
    void ProcessQuestTurnIn(Player* bot, uint32 questId) override;
    void ProcessBatchTurnIn(Player* bot, const TurnInBatch& batch) override;
    void ScheduleQuestTurnIn(Player* bot, uint32 questId, uint32 delayMs = 0) override;
    std::vector<uint32> GetCompletedQuests(Player* bot) override;
    bool IsQuestReadyForTurnIn(uint32 questId, Player* bot) override;
    void MonitorQuestCompletion(Player* bot) override;
    void HandleQuestCompletion(Player* bot, uint32 questId) override;
    void PlanOptimalTurnInRoute(Player* bot) override;
    TurnInBatch CreateTurnInBatch(Player* bot, const std::vector<uint32>& questIds) override;
    void OptimizeTurnInSequence(Player* bot, std::vector<QuestTurnInData>& turnIns) override;
    void MinimizeTurnInTravel(Player* bot) override;
    bool FindQuestTurnInNpc(Player* bot, uint32 questId) override;
    Position GetQuestTurnInLocation(uint32 questId) override;
    bool NavigateToQuestGiver(Player* bot, uint32 questGiverGuid) override;
    bool IsAtQuestGiver(Player* bot, uint32 questGiverGuid) override;
    void AnalyzeQuestRewards(QuestTurnInData& turnInData, Player* bot) override;
    uint32 SelectOptimalReward(const std::vector<QuestRewardItem>& rewards, Player* bot, RewardSelectionStrategy strategy) override;
    void EvaluateItemUpgrades(const std::vector<QuestRewardItem>& rewards, Player* bot) override;
    float CalculateItemValue(const QuestRewardItem& reward, Player* bot) override;
    void CoordinateGroupTurnIns(Group* group) override;
    void SynchronizeGroupRewardSelection(Group* group, uint32 questId) override;
    void HandleGroupTurnInConflicts(Group* group, uint32 questId) override;
    void ShareTurnInProgress(Group* group) override;
    void HandleQuestGiverDialog(Player* bot, uint32 questGiverGuid, uint32 questId) override;
    void SelectQuestReward(Player* bot, uint32 questId, uint32 rewardIndex) override;
    void ConfirmQuestTurnIn(Player* bot, uint32 questId) override;
    void HandleTurnInDialog(Player* bot, uint32 questId) override;
    void ExecuteImmediateTurnInStrategy(Player* bot) override;
    void ExecuteBatchTurnInStrategy(Player* bot) override;
    void ExecuteOptimalRoutingStrategy(Player* bot) override;
    void ExecuteGroupCoordinationStrategy(Player* bot) override;
    void ExecuteRewardOptimizationStrategy(Player* bot) override;
    void ExecuteChainContinuationStrategy(Player* bot) override;
    void HandleQuestChainProgression(Player* bot, uint32 completedQuestId) override;
    uint32 GetNextQuestInChain(uint32 completedQuestId) override;
    void AutoAcceptFollowUpQuests(Player* bot, uint32 completedQuestId) override;
    void PrioritizeChainQuests(Player* bot) override;
    void SetTurnInStrategy(uint32 botGuid, TurnInStrategy strategy) override;
    TurnInStrategy GetTurnInStrategy(uint32 botGuid) override;
    void SetRewardSelectionStrategy(uint32 botGuid, RewardSelectionStrategy strategy) override;
    RewardSelectionStrategy GetRewardSelectionStrategy(uint32 botGuid) override;
    void SetBatchTurnInThreshold(uint32 botGuid, uint32 threshold) override;
    void HandleTurnInError(Player* bot, uint32 questId, const std::string& error) override;
    void RecoverFromTurnInFailure(Player* bot, uint32 questId) override;
    void RetryFailedTurnIn(Player* bot, uint32 questId) override;
    void ValidateTurnInState(Player* bot, uint32 questId) override;

    // ========================================================================
    // DYNAMIC MODULE INTERFACE
    // ========================================================================

    std::vector<uint32> DiscoverAvailableQuests(Player* bot) override;
    std::vector<uint32> GetRecommendedQuests(Player* bot, QuestStrategy strategy = QuestStrategy::LEVEL_PROGRESSION) override;
    bool AssignQuestToBot(uint32 questId, Player* bot) override;
    void AutoAssignQuests(Player* bot, uint32 maxQuests = 10) override;
    QuestPriority CalculateQuestPriority(uint32 questId, Player* bot) override;
    std::vector<uint32> SortQuestsByPriority(const std::vector<uint32>& questIds, Player* bot) override;
    bool ShouldAbandonQuest(uint32 questId, Player* bot) override;
    void UpdateQuestProgressDynamic(Player* bot) override;
    void ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    bool CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void HandleQuestCompletionDynamic(Player* bot, uint32 questId) override;
    bool FormQuestGroup(uint32 questId, Player* initiator) override;
    void CoordinateGroupQuest(Group* group, uint32 questId) override;
    void ShareQuestProgress(Group* group, uint32 questId) override;
    bool CanShareQuest(uint32 questId, Player* from, Player* to) override;
    Position GetNextQuestLocation(Player* bot, uint32 questId) override;
    std::vector<Position> GenerateQuestPath(Player* bot, uint32 questId) override;
    void HandleQuestNavigation(Player* bot, uint32 questId) override;
    bool IsQuestLocationReachable(Player* bot, const Position& location) override;
    void AdaptQuestDifficulty(uint32 questId, Player* bot) override;
    void HandleQuestStuckState(Player* bot, uint32 questId) override;
    void RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void OptimizeQuestOrder(Player* bot) override;
    void TrackQuestChains(Player* bot) override;
    std::vector<uint32> GetQuestChain(uint32 questId) override;
    uint32 GetNextQuestInChainDynamic(uint32 completedQuestId) override;
    void AdvanceQuestChain(Player* bot, uint32 completedQuestId) override;
    void OptimizeZoneQuests(Player* bot) override;
    std::vector<uint32> GetZoneQuests(uint32 zoneId, Player* bot) override;
    void PlanZoneCompletion(Player* bot, uint32 zoneId) override;
    bool ShouldMoveToNewZone(Player* bot) override;
    QuestReward AnalyzeQuestReward(uint32 questId, Player* bot) override;
    float CalculateQuestValue(uint32 questId, Player* bot) override;
    bool IsQuestWorthwhile(uint32 questId, Player* bot) override;
    void SetQuestStrategy(uint32 botGuid, QuestStrategy strategy) override;
    QuestStrategy GetQuestStrategy(uint32 botGuid) override;
    void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests) override;
    void EnableQuestGrouping(uint32 botGuid, bool enable) override;

    // ========================================================================
    // UNIFIED OPERATIONS
    // ========================================================================

    void ProcessCompleteQuestFlow(Player* bot) override;
    std::string GetQuestRecommendation(Player* bot, uint32 questId) override;
    void OptimizeBotQuestLoad(Player* bot) override;
    std::string GetQuestStatistics() const override;
    QuestMetrics GetBotQuestMetrics(uint32 botGuid) override;
    QuestMetrics GetGlobalQuestMetrics() override;
    TurnInMetrics GetBotTurnInMetrics(uint32 botGuid) override;
    TurnInMetrics GetGlobalTurnInMetrics() override;
    ValidationMetrics GetValidationMetrics() override;
    void Update(uint32 diff) override;
    void UpdateBotTurnIns(Player* bot, uint32 diff) override;
    void ProcessScheduledTurnIns() override;
    void CleanupCompletedTurnIns() override;
    void CleanupCompletedQuests() override;
    void ValidateQuestStates() override;

private:
    // ========================================================================
    // INTERNAL MODULES
    // ========================================================================

    /**
     * @brief Pickup module - quest discovery and acceptance
     */
    class PickupModule
    {
    public:
        // Delegates to QuestPickup singleton
        bool PickupQuest(uint32 questId, Player* bot, uint32 questGiverGuid = 0);
        bool PickupQuestFromGiver(Player* bot, uint32 questGiverGuid, uint32 questId = 0);
        void PickupAvailableQuests(Player* bot);
        void PickupQuestsInArea(Player* bot, float radius);
        std::vector<uint32> DiscoverNearbyQuests(Player* bot, float scanRadius);
        std::vector<QuestGiverInfo> ScanForQuestGivers(Player* bot, float scanRadius);
        std::vector<uint32> GetAvailableQuestsFromGiver(uint32 questGiverGuid, Player* bot);
        QuestEligibility CheckQuestEligibility(uint32 questId, Player* bot);
        bool CanAcceptQuest(uint32 questId, Player* bot);
        bool MeetsQuestRequirements(uint32 questId, Player* bot);
        std::vector<uint32> FilterQuests(const std::vector<uint32>& questIds, Player* bot, const QuestPickupFilter& filter);
        std::vector<uint32> PrioritizeQuests(const std::vector<uint32>& questIds, Player* bot, QuestAcceptanceStrategy strategy);
        bool ShouldAcceptQuest(uint32 questId, Player* bot);

    private:
        std::atomic<uint64> _questsPickedUp{0};
        std::atomic<uint64> _questsDiscovered{0};
    };

    /**
     * @brief Completion module - objective tracking and execution
     */
    class CompletionModule
    {
    public:
        // Delegates to QuestCompletion singleton
        bool StartQuestCompletion(uint32 questId, Player* bot);
        void UpdateQuestProgress(Player* bot);
        void CompleteQuest(uint32 questId, Player* bot);
        bool TurnInQuest(uint32 questId, Player* bot);
        void TrackQuestObjectives(Player* bot);
        void ExecuteObjective(Player* bot, QuestObjectiveData& objective);
        void UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex);
        bool IsObjectiveComplete(const QuestObjectiveData& objective);
        void HandleKillObjective(Player* bot, QuestObjectiveData& objective);
        void HandleCollectObjective(Player* bot, QuestObjectiveData& objective);
        void HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective);
        void HandleLocationObjective(Player* bot, QuestObjectiveData& objective);
        void HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective);
        void HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective);
        void HandleEmoteObjective(Player* bot, QuestObjectiveData& objective);
        void HandleEscortObjective(Player* bot, QuestObjectiveData& objective);
        void NavigateToObjective(Player* bot, const QuestObjectiveData& objective);
        bool FindObjectiveTarget(Player* bot, QuestObjectiveData& objective);
        std::vector<Position> GetObjectiveLocations(const QuestObjectiveData& objective);
        Position GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective);
        void CoordinateGroupQuestCompletion(Group* group, uint32 questId);
        void ShareObjectiveProgress(Group* group, uint32 questId);
        void SynchronizeGroupObjectives(Group* group, uint32 questId);
        void HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex);
        void OptimizeQuestCompletionOrder(Player* bot);
        void OptimizeObjectiveSequence(Player* bot, uint32 questId);
        void FindEfficientCompletionPath(Player* bot, const std::vector<uint32>& questIds);
        void MinimizeTravelTime(Player* bot, const std::vector<QuestObjectiveData>& objectives);
        void DetectStuckState(Player* bot, uint32 questId);
        void HandleStuckObjective(Player* bot, QuestObjectiveData& objective);
        void RecoverFromStuckState(Player* bot, uint32 questId);
        void SkipProblematicObjective(Player* bot, QuestObjectiveData& objective);

    private:
        std::atomic<uint64> _objectivesCompleted{0};
        std::atomic<uint64> _questsCompleted{0};
    };

    /**
     * @brief Validation module - requirement validation
     */
    class ValidationModule
    {
    public:
        // Delegates to QuestValidation singleton
        bool ValidateQuest(uint32 questId, Player* bot);
        bool ValidateQuestRequirements(uint32 questId, Player* bot);
        std::vector<std::string> GetValidationErrors(uint32 questId, Player* bot);
        bool ValidateLevelRequirements(uint32 questId, Player* bot);
        bool ValidateClassRequirements(uint32 questId, Player* bot);
        bool ValidateRaceRequirements(uint32 questId, Player* bot);
        bool ValidateSkillRequirements(uint32 questId, Player* bot);
        bool ValidateQuestPrerequisites(uint32 questId, Player* bot);
        bool ValidateQuestChain(uint32 questId, Player* bot);
        bool HasCompletedPrerequisites(uint32 questId, Player* bot);
        std::vector<uint32> GetMissingPrerequisites(uint32 questId, Player* bot);
        bool ValidateReputationRequirements(uint32 questId, Player* bot);
        bool ValidateFactionRequirements(uint32 questId, Player* bot);
        bool HasRequiredReputation(uint32 questId, Player* bot, uint32 factionId);
        bool ValidateItemRequirements(uint32 questId, Player* bot);
        bool HasRequiredItems(uint32 questId, Player* bot);
        bool HasInventorySpace(uint32 questId, Player* bot);
        std::vector<uint32> GetMissingQuestItems(uint32 questId, Player* bot);
        bool ValidateQuestAvailability(uint32 questId, Player* bot);
        bool ValidateSeasonalAvailability(uint32 questId);
        bool ValidateDailyQuestLimits(uint32 questId, Player* bot);
        bool ValidateQuestTimer(uint32 questId, Player* bot);
        bool ValidateZoneRequirements(uint32 questId, Player* bot);
        bool ValidateAreaRequirements(uint32 questId, Player* bot);
        bool IsInCorrectZone(uint32 questId, Player* bot);
        bool CanQuestBeStartedAtLocation(uint32 questId, const Position& location);
        bool ValidateGroupRequirements(uint32 questId, Player* bot);
        bool ValidatePartyQuestRequirements(uint32 questId, Player* bot);
        bool ValidateRaidQuestRequirements(uint32 questId, Player* bot);
        bool CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver);
        bool ValidateWithContext(ValidationContext& context);
        bool ValidateQuestObjectives(uint32 questId, Player* bot);
        bool ValidateQuestRewards(uint32 questId, Player* bot);
        bool ValidateQuestDifficulty(uint32 questId, Player* bot);
        ValidationResult GetCachedValidation(uint32 questId, uint32 botGuid);
        void CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result);
        void InvalidateValidationCache(uint32 botGuid);
        void CleanupExpiredCache();
        std::unordered_map<uint32, ValidationResult> ValidateMultipleQuests(
            const std::vector<uint32>& questIds, Player* bot);
        std::vector<uint32> FilterValidQuests(const std::vector<uint32>& questIds, Player* bot);
        std::vector<uint32> GetEligibleQuests(Player* bot, const std::vector<uint32>& candidates);
        std::string GetDetailedValidationReport(uint32 questId, Player* bot);
        void LogValidationFailure(uint32 questId, Player* bot, const std::string& reason);
        std::vector<std::string> GetRecommendationsForFailedQuest(uint32 questId, Player* bot);
        ValidationMetrics GetValidationMetrics();

    private:
        std::atomic<uint64> _validationsPerformed{0};
        std::atomic<uint64> _validationsPassed{0};
    };

    /**
     * @brief TurnIn module - quest completion and reward selection
     */
    class TurnInModule
    {
    public:
        // Delegates to QuestTurnIn singleton
        bool TurnInQuestWithReward(uint32 questId, Player* bot);
        void ProcessQuestTurnIn(Player* bot, uint32 questId);
        void ProcessBatchTurnIn(Player* bot, const TurnInBatch& batch);
        void ScheduleQuestTurnIn(Player* bot, uint32 questId, uint32 delayMs);
        std::vector<uint32> GetCompletedQuests(Player* bot);
        bool IsQuestReadyForTurnIn(uint32 questId, Player* bot);
        void MonitorQuestCompletion(Player* bot);
        void HandleQuestCompletion(Player* bot, uint32 questId);
        void PlanOptimalTurnInRoute(Player* bot);
        TurnInBatch CreateTurnInBatch(Player* bot, const std::vector<uint32>& questIds);
        void OptimizeTurnInSequence(Player* bot, std::vector<QuestTurnInData>& turnIns);
        void MinimizeTurnInTravel(Player* bot);
        bool FindQuestTurnInNpc(Player* bot, uint32 questId);
        Position GetQuestTurnInLocation(uint32 questId);
        bool NavigateToQuestGiver(Player* bot, uint32 questGiverGuid);
        bool IsAtQuestGiver(Player* bot, uint32 questGiverGuid);
        void AnalyzeQuestRewards(QuestTurnInData& turnInData, Player* bot);
        uint32 SelectOptimalReward(const std::vector<QuestRewardItem>& rewards, Player* bot, RewardSelectionStrategy strategy);
        void EvaluateItemUpgrades(const std::vector<QuestRewardItem>& rewards, Player* bot);
        float CalculateItemValue(const QuestRewardItem& reward, Player* bot);
        void CoordinateGroupTurnIns(Group* group);
        void SynchronizeGroupRewardSelection(Group* group, uint32 questId);
        void HandleGroupTurnInConflicts(Group* group, uint32 questId);
        void ShareTurnInProgress(Group* group);
        void HandleQuestGiverDialog(Player* bot, uint32 questGiverGuid, uint32 questId);
        void SelectQuestReward(Player* bot, uint32 questId, uint32 rewardIndex);
        void ConfirmQuestTurnIn(Player* bot, uint32 questId);
        void HandleTurnInDialog(Player* bot, uint32 questId);
        void ExecuteImmediateTurnInStrategy(Player* bot);
        void ExecuteBatchTurnInStrategy(Player* bot);
        void ExecuteOptimalRoutingStrategy(Player* bot);
        void ExecuteGroupCoordinationStrategy(Player* bot);
        void ExecuteRewardOptimizationStrategy(Player* bot);
        void ExecuteChainContinuationStrategy(Player* bot);
        void HandleQuestChainProgression(Player* bot, uint32 completedQuestId);
        uint32 GetNextQuestInChain(uint32 completedQuestId);
        void AutoAcceptFollowUpQuests(Player* bot, uint32 completedQuestId);
        void PrioritizeChainQuests(Player* bot);
        void SetTurnInStrategy(uint32 botGuid, TurnInStrategy strategy);
        TurnInStrategy GetTurnInStrategy(uint32 botGuid);
        void SetRewardSelectionStrategy(uint32 botGuid, RewardSelectionStrategy strategy);
        RewardSelectionStrategy GetRewardSelectionStrategy(uint32 botGuid);
        void SetBatchTurnInThreshold(uint32 botGuid, uint32 threshold);
        void HandleTurnInError(Player* bot, uint32 questId, const std::string& error);
        void RecoverFromTurnInFailure(Player* bot, uint32 questId);
        void RetryFailedTurnIn(Player* bot, uint32 questId);
        void ValidateTurnInState(Player* bot, uint32 questId);
        TurnInMetrics GetBotTurnInMetrics(uint32 botGuid);
        TurnInMetrics GetGlobalTurnInMetrics();

    private:
        std::atomic<uint64> _questsTurnedIn{0};
        std::atomic<uint64> _rewardsSelected{0};
    };

    /**
     * @brief Dynamic module - quest assignment and optimization
     */
    class DynamicModule
    {
    public:
        // Delegates to DynamicQuestSystem singleton
        std::vector<uint32> DiscoverAvailableQuests(Player* bot);
        std::vector<uint32> GetRecommendedQuests(Player* bot, QuestStrategy strategy);
        bool AssignQuestToBot(uint32 questId, Player* bot);
        void AutoAssignQuests(Player* bot, uint32 maxQuests);
        QuestPriority CalculateQuestPriority(uint32 questId, Player* bot);
        std::vector<uint32> SortQuestsByPriority(const std::vector<uint32>& questIds, Player* bot);
        bool ShouldAbandonQuest(uint32 questId, Player* bot);
        void UpdateQuestProgressDynamic(Player* bot);
        void ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex);
        bool CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex);
        void HandleQuestCompletionDynamic(Player* bot, uint32 questId);
        bool FormQuestGroup(uint32 questId, Player* initiator);
        void CoordinateGroupQuest(Group* group, uint32 questId);
        void ShareQuestProgress(Group* group, uint32 questId);
        bool CanShareQuest(uint32 questId, Player* from, Player* to);
        Position GetNextQuestLocation(Player* bot, uint32 questId);
        std::vector<Position> GenerateQuestPath(Player* bot, uint32 questId);
        void HandleQuestNavigation(Player* bot, uint32 questId);
        bool IsQuestLocationReachable(Player* bot, const Position& location);
        void AdaptQuestDifficulty(uint32 questId, Player* bot);
        void HandleQuestStuckState(Player* bot, uint32 questId);
        void RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex);
        void OptimizeQuestOrder(Player* bot);
        void TrackQuestChains(Player* bot);
        std::vector<uint32> GetQuestChain(uint32 questId);
        uint32 GetNextQuestInChainDynamic(uint32 completedQuestId);
        void AdvanceQuestChain(Player* bot, uint32 completedQuestId);
        void OptimizeZoneQuests(Player* bot);
        std::vector<uint32> GetZoneQuests(uint32 zoneId, Player* bot);
        void PlanZoneCompletion(Player* bot, uint32 zoneId);
        bool ShouldMoveToNewZone(Player* bot);
        QuestReward AnalyzeQuestReward(uint32 questId, Player* bot);
        float CalculateQuestValue(uint32 questId, Player* bot);
        bool IsQuestWorthwhile(uint32 questId, Player* bot);
        void SetQuestStrategy(uint32 botGuid, QuestStrategy strategy);
        QuestStrategy GetQuestStrategy(uint32 botGuid);
        void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests);
        void EnableQuestGrouping(uint32 botGuid, bool enable);
        QuestMetrics GetBotQuestMetrics(uint32 botGuid);
        QuestMetrics GetGlobalQuestMetrics();

    private:
        std::atomic<uint64> _questsAssigned{0};
        std::atomic<uint64> _questsOptimized{0};
    };

    // Module instances
    std::unique_ptr<PickupModule> _pickup;
    std::unique_ptr<CompletionModule> _completion;
    std::unique_ptr<ValidationModule> _validation;
    std::unique_ptr<TurnInModule> _turnIn;
    std::unique_ptr<DynamicModule> _dynamic;

    // Global mutex for unified operations
    mutable Playerbot::OrderedMutex<Playerbot::LockOrder::QUEST_MANAGER> _mutex;

    // Statistics
    std::atomic<uint64> _totalOperations{0};
    std::atomic<uint64> _totalProcessingTimeMs{0};
};

} // namespace Playerbot
