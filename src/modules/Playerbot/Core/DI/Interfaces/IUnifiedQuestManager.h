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
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

class Player;
class Group;
class Quest;
class Creature;
struct Position;

namespace Playerbot
{

// Forward declarations from quest system
enum class QuestAcceptanceStrategy : uint8;
enum class QuestGiverType : uint8;
enum class QuestEligibility : uint8;
enum class QuestObjectiveType : uint8;
enum class QuestCompletionStrategy : uint8;
enum class ObjectiveStatus : uint8;
enum class TurnInStrategy : uint8;
enum class RewardSelectionStrategy : uint8;
enum class QuestPriority : uint8;
enum class QuestType : uint8;

// Full definition needed for default parameter usage
enum class QuestSelectionStrategy : uint8
{
    SOLO_FOCUSED        = 0,  // Complete quests independently
    GROUP_PREFERRED     = 1,  // Try to group for efficiency
    ZONE_OPTIMIZATION   = 2,  // Complete all quests in current zone
    LEVEL_PROGRESSION   = 3,  // Focus on experience gain
    GEAR_PROGRESSION    = 4,  // Focus on equipment upgrades
    STORY_PROGRESSION   = 5,  // Follow main storylines
    REPUTATION_FOCUSED  = 6,  // Build faction reputation
    PROFESSION_FOCUSED  = 7   // Complete profession quests
};

struct QuestGiverInfo;
struct QuestPickupRequest;
struct QuestPickupFilter;
struct QuestObjectiveData;
struct QuestProgressData;
struct QuestRewardItem;
struct QuestTurnInData;
struct TurnInBatch;
struct QuestMetadata;
struct QuestProgress;
struct ValidationResult;
struct ValidationMetrics;
struct QuestReward;
struct QuestMetrics;
struct EquipmentMetrics;

// Forward declare IQuestTurnIn for conversion
class IQuestTurnIn;

// TurnInMetrics - defined here to avoid circular dependency with IQuestTurnIn
struct TurnInMetrics
{
    uint32 questsTurnedIn{0};
    uint32 turnInAttempts{0};
    uint32 successfulTurnIns{0};
    uint32 failedTurnIns{0};
    float averageTurnInTime{0.0f};
    float turnInSuccessRate{0.0f};
    uint32 totalTravelDistance{0};
    uint32 rewardsSelected{0};
    float rewardSelectionAccuracy{0.0f};

    float GetSuccessRate() const {
        return turnInAttempts > 0 ? (float)successfulTurnIns / turnInAttempts : 0.0f;
    }

    // Conversion from IQuestTurnIn::TurnInMetricsSnapshot
    TurnInMetrics() = default;
    template<typename T>
    TurnInMetrics(T const& snapshot) :
        questsTurnedIn(snapshot.questsTurnedIn),
        turnInAttempts(snapshot.turnInAttempts),
        successfulTurnIns(snapshot.successfulTurnIns),
        failedTurnIns(snapshot.failedTurnIns),
        averageTurnInTime(snapshot.averageTurnInTime),
        turnInSuccessRate(snapshot.turnInSuccessRate),
        totalTravelDistance(snapshot.totalTravelDistance),
        rewardsSelected(snapshot.rewardsSelected),
        rewardSelectionAccuracy(snapshot.rewardSelectionAccuracy)
    {}
};

/**
 * @brief Unified interface for all quest management operations
 *
 * This interface consolidates functionality from:
 * - QuestPickup: Quest discovery and acceptance
 * - QuestCompletion: Objective tracking and execution
 * - QuestValidation: Requirement validation
 * - QuestTurnIn: Quest completion and reward selection
 * - DynamicQuestSystem: Dynamic quest assignment and optimization
 *
 * **Design Pattern:** Facade Pattern
 * - Single entry point for all quest operations
 * - Simplifies quest system interactions
 * - Reduces coupling between components
 *
 * **Benefits:**
 * - Easier to test (single mock instead of 5)
 * - Clear API surface
 * - Reduced complexity
 * - Better encapsulation
 */
class TC_GAME_API IUnifiedQuestManager
{
public:
    virtual ~IUnifiedQuestManager() = default;

    // ========================================================================
    // PICKUP MODULE (from QuestPickup)
    // ========================================================================

    /**
     * @brief Core quest pickup functionality
     */
    virtual bool PickupQuest(uint32 questId, Player* bot, uint32 questGiverGuid = 0) = 0;
    virtual bool PickupQuestFromGiver(Player* bot, uint32 questGiverGuid, uint32 questId = 0) = 0;
    virtual void PickupAvailableQuests(Player* bot) = 0;
    virtual void PickupQuestsInArea(Player* bot, float radius = 50.0f) = 0;

    /**
     * @brief Quest discovery and scanning
     */
    virtual ::std::vector<uint32> DiscoverNearbyQuests(Player* bot, float scanRadius = 100.0f) = 0;
    virtual ::std::vector<QuestGiverInfo> ScanForQuestGivers(Player* bot, float scanRadius = 100.0f) = 0;
    virtual ::std::vector<uint32> GetAvailableQuestsFromGiver(uint32 questGiverGuid, Player* bot) = 0;

    /**
     * @brief Quest eligibility and validation
     */
    virtual QuestEligibility CheckQuestEligibility(uint32 questId, Player* bot) = 0;
    virtual bool CanAcceptQuest(uint32 questId, Player* bot) = 0;
    virtual bool MeetsQuestRequirements(uint32 questId, Player* bot) = 0;

    /**
     * @brief Quest filtering and prioritization
     */
    virtual ::std::vector<uint32> FilterQuests(const ::std::vector<uint32>& questIds, Player* bot, const QuestPickupFilter& filter) = 0;
    virtual ::std::vector<uint32> PrioritizeQuests(const ::std::vector<uint32>& questIds, Player* bot, QuestAcceptanceStrategy strategy) = 0;
    virtual bool ShouldAcceptQuest(uint32 questId, Player* bot) = 0;

    // ========================================================================
    // COMPLETION MODULE (from QuestCompletion)
    // ========================================================================

    /**
     * @brief Core quest completion management
     */
    virtual bool StartQuestCompletion(uint32 questId, Player* bot) = 0;
    virtual void UpdateQuestProgress(Player* bot) = 0;
    virtual void CompleteQuest(uint32 questId, Player* bot) = 0;
    virtual bool TurnInQuest(uint32 questId, Player* bot) = 0;

    /**
     * @brief Objective tracking and execution
     */
    virtual void TrackQuestObjectives(Player* bot) = 0;
    virtual void ExecuteObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual bool IsObjectiveComplete(const QuestObjectiveData& objective) = 0;

    /**
     * @brief Objective-specific handlers
     */
    virtual void HandleKillObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleCollectObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleLocationObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleEmoteObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void HandleEscortObjective(Player* bot, QuestObjectiveData& objective) = 0;

    /**
     * @brief Navigation and pathfinding
     */
    virtual void NavigateToObjective(Player* bot, const QuestObjectiveData& objective) = 0;
    virtual bool FindObjectiveTarget(Player* bot, QuestObjectiveData& objective) = 0;
    virtual ::std::vector<Position> GetObjectiveLocations(const QuestObjectiveData& objective) = 0;
    virtual Position GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective) = 0;

    /**
     * @brief Group coordination for quest completion
     */
    virtual void CoordinateGroupQuestCompletion(Group* group, uint32 questId) = 0;
    virtual void ShareObjectiveProgress(Group* group, uint32 questId) = 0;
    virtual void SynchronizeGroupObjectives(Group* group, uint32 questId) = 0;
    virtual void HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex) = 0;

    /**
     * @brief Quest completion optimization
     */
    virtual void OptimizeQuestCompletionOrder(Player* bot) = 0;
    virtual void OptimizeObjectiveSequence(Player* bot, uint32 questId) = 0;
    virtual void FindEfficientCompletionPath(Player* bot, const ::std::vector<uint32>& questIds) = 0;
    virtual void MinimizeTravelTime(Player* bot, const ::std::vector<QuestObjectiveData>& objectives) = 0;

    /**
     * @brief Stuck detection and recovery
     */
    virtual void DetectStuckState(Player* bot, uint32 questId) = 0;
    virtual void HandleStuckObjective(Player* bot, QuestObjectiveData& objective) = 0;
    virtual void RecoverFromStuckState(Player* bot, uint32 questId) = 0;
    virtual void SkipProblematicObjective(Player* bot, QuestObjectiveData& objective) = 0;

    // ========================================================================
    // VALIDATION MODULE (from QuestValidation)
    // ========================================================================

    /**
     * @brief Comprehensive quest validation
     */
    virtual bool ValidateQuest(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestRequirements(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<::std::string> GetValidationErrors(uint32 questId, Player* bot) = 0;

    /**
     * @brief Level and class requirements
     */
    virtual bool ValidateLevelRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateClassRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateRaceRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateSkillRequirements(uint32 questId, Player* bot) = 0;

    /**
     * @brief Quest chain and prerequisite validation
     */
    virtual bool ValidateQuestPrerequisites(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestChain(uint32 questId, Player* bot) = 0;
    virtual bool HasCompletedPrerequisites(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<uint32> GetMissingPrerequisites(uint32 questId, Player* bot) = 0;

    /**
     * @brief Reputation and faction validation
     */
    virtual bool ValidateReputationRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateFactionRequirements(uint32 questId, Player* bot) = 0;
    virtual bool HasRequiredReputation(uint32 questId, Player* bot, uint32 factionId) = 0;

    /**
     * @brief Item and inventory validation
     */
    virtual bool ValidateItemRequirements(uint32 questId, Player* bot) = 0;
    virtual bool HasRequiredItems(uint32 questId, Player* bot) = 0;
    virtual bool HasInventorySpace(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<uint32> GetMissingQuestItems(uint32 questId, Player* bot) = 0;

    /**
     * @brief Time and availability validation
     */
    virtual bool ValidateQuestAvailability(uint32 questId, Player* bot) = 0;
    virtual bool ValidateSeasonalAvailability(uint32 questId) = 0;
    virtual bool ValidateDailyQuestLimits(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestTimer(uint32 questId, Player* bot) = 0;

    /**
     * @brief Zone and location validation
     */
    virtual bool ValidateZoneRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateAreaRequirements(uint32 questId, Player* bot) = 0;
    virtual bool IsInCorrectZone(uint32 questId, Player* bot) = 0;
    virtual bool CanQuestBeStartedAtLocation(uint32 questId, const Position& location) = 0;

    /**
     * @brief Group and party validation
     */
    virtual bool ValidateGroupRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidatePartyQuestRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateRaidQuestRequirements(uint32 questId, Player* bot) = 0;
    virtual bool CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver) = 0;

    /**
     * @brief Advanced validation
     */
    struct ValidationContext;
    virtual bool ValidateWithContext(ValidationContext& context) = 0;
    virtual bool ValidateQuestObjectives(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestRewards(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestDifficulty(uint32 questId, Player* bot) = 0;

    /**
     * @brief Validation caching and optimization
     */
    virtual ValidationResult GetCachedValidation(uint32 questId, uint32 botGuid) = 0;
    virtual void CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result) = 0;
    virtual void InvalidateValidationCache(uint32 botGuid) = 0;
    virtual void CleanupExpiredCache() = 0;

    /**
     * @brief Batch validation for efficiency
     */
    virtual ::std::unordered_map<uint32, ValidationResult> ValidateMultipleQuests(
        const ::std::vector<uint32>& questIds, Player* bot) = 0;
    virtual ::std::vector<uint32> FilterValidQuests(const ::std::vector<uint32>& questIds, Player* bot) = 0;
    virtual ::std::vector<uint32> GetEligibleQuests(Player* bot, const ::std::vector<uint32>& candidates) = 0;

    /**
     * @brief Error reporting and diagnostics
     */
    virtual ::std::string GetDetailedValidationReport(uint32 questId, Player* bot) = 0;
    virtual void LogValidationFailure(uint32 questId, Player* bot, const ::std::string& reason) = 0;
    virtual ::std::vector<::std::string> GetRecommendationsForFailedQuest(uint32 questId, Player* bot) = 0;

    // ========================================================================
    // TURNIN MODULE (from QuestTurnIn)
    // ========================================================================

    /**
     * @brief Core turn-in functionality
     */
    virtual bool TurnInQuestWithReward(uint32 questId, Player* bot) = 0;
    virtual void ProcessQuestTurnIn(Player* bot, uint32 questId) = 0;
    virtual void ProcessBatchTurnIn(Player* bot, const TurnInBatch& batch) = 0;
    virtual void ScheduleQuestTurnIn(Player* bot, uint32 questId, uint32 delayMs = 0) = 0;

    /**
     * @brief Quest completion detection
     */
    virtual ::std::vector<uint32> GetCompletedQuests(Player* bot) = 0;
    virtual bool IsQuestReadyForTurnIn(uint32 questId, Player* bot) = 0;
    virtual void MonitorQuestCompletion(Player* bot) = 0;
    virtual void HandleQuestCompletion(Player* bot, uint32 questId) = 0;

    /**
     * @brief Turn-in planning and optimization
     */
    virtual void PlanOptimalTurnInRoute(Player* bot) = 0;
    virtual TurnInBatch CreateTurnInBatch(Player* bot, const ::std::vector<uint32>& questIds) = 0;
    virtual void OptimizeTurnInSequence(Player* bot, ::std::vector<QuestTurnInData>& turnIns) = 0;
    virtual void MinimizeTurnInTravel(Player* bot) = 0;

    /**
     * @brief Quest giver location and navigation
     */
    virtual bool FindQuestTurnInNpc(Player* bot, uint32 questId) = 0;
    virtual Position GetQuestTurnInLocation(uint32 questId) = 0;
    virtual bool NavigateToQuestGiver(Player* bot, uint32 questGiverGuid) = 0;
    virtual bool IsAtQuestGiver(Player* bot, uint32 questGiverGuid) = 0;

    /**
     * @brief Reward selection and optimization
     */
    virtual void AnalyzeQuestRewards(QuestTurnInData& turnInData, Player* bot) = 0;
    virtual uint32 SelectOptimalReward(const ::std::vector<QuestRewardItem>& rewards, Player* bot, RewardSelectionStrategy strategy) = 0;
    virtual void EvaluateItemUpgrades(const ::std::vector<QuestRewardItem>& rewards, Player* bot) = 0;
    virtual float CalculateItemValue(const QuestRewardItem& reward, Player* bot) = 0;

    /**
     * @brief Group turn-in coordination
     */
    virtual void CoordinateGroupTurnIns(Group* group) = 0;
    virtual void SynchronizeGroupRewardSelection(Group* group, uint32 questId) = 0;
    virtual void HandleGroupTurnInConflicts(Group* group, uint32 questId) = 0;
    virtual void ShareTurnInProgress(Group* group) = 0;

    /**
     * @brief Turn-in dialog and interaction
     */
    virtual void HandleQuestGiverDialog(Player* bot, uint32 questGiverGuid, uint32 questId) = 0;
    virtual void SelectQuestReward(Player* bot, uint32 questId, uint32 rewardIndex) = 0;
    virtual void ConfirmQuestTurnIn(Player* bot, uint32 questId) = 0;
    virtual void HandleTurnInDialog(Player* bot, uint32 questId) = 0;

    /**
     * @brief Advanced turn-in strategies
     */
    virtual void ExecuteImmediateTurnInStrategy(Player* bot) = 0;
    virtual void ExecuteBatchTurnInStrategy(Player* bot) = 0;
    virtual void ExecuteOptimalRoutingStrategy(Player* bot) = 0;
    virtual void ExecuteGroupCoordinationStrategy(Player* bot) = 0;
    virtual void ExecuteRewardOptimizationStrategy(Player* bot) = 0;
    virtual void ExecuteChainContinuationStrategy(Player* bot) = 0;

    /**
     * @brief Quest chain management
     */
    virtual void HandleQuestChainProgression(Player* bot, uint32 completedQuestId) = 0;
    virtual uint32 GetNextQuestInChain(uint32 completedQuestId) = 0;
    virtual void AutoAcceptFollowUpQuests(Player* bot, uint32 completedQuestId) = 0;
    virtual void PrioritizeChainQuests(Player* bot) = 0;

    /**
     * @brief Configuration and settings
     */
    virtual void SetTurnInStrategy(uint32 botGuid, TurnInStrategy strategy) = 0;
    virtual TurnInStrategy GetTurnInStrategy(uint32 botGuid) = 0;
    virtual void SetRewardSelectionStrategy(uint32 botGuid, RewardSelectionStrategy strategy) = 0;
    virtual RewardSelectionStrategy GetRewardSelectionStrategy(uint32 botGuid) = 0;
    virtual void SetBatchTurnInThreshold(uint32 botGuid, uint32 threshold) = 0;

    /**
     * @brief Error handling and recovery
     */
    virtual void HandleTurnInError(Player* bot, uint32 questId, const ::std::string& error) = 0;
    virtual void RecoverFromTurnInFailure(Player* bot, uint32 questId) = 0;
    virtual void RetryFailedTurnIn(Player* bot, uint32 questId) = 0;
    virtual void ValidateTurnInState(Player* bot, uint32 questId) = 0;

    // ========================================================================
    // DYNAMIC MODULE (from DynamicQuestSystem)
    // ========================================================================

    /**
     * @brief Quest discovery and assignment
     */
    virtual ::std::vector<uint32> DiscoverAvailableQuests(Player* bot) = 0;
    virtual ::std::vector<uint32> GetRecommendedQuests(Player* bot, QuestSelectionStrategy strategy = QuestSelectionStrategy::LEVEL_PROGRESSION) = 0;
    virtual bool AssignQuestToBot(uint32 questId, Player* bot) = 0;
    virtual void AutoAssignQuests(Player* bot, uint32 maxQuests = 10) = 0;

    /**
     * @brief Quest prioritization
     */
    virtual QuestPriority CalculateQuestPriority(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<uint32> SortQuestsByPriority(const ::std::vector<uint32>& questIds, Player* bot) = 0;
    virtual bool ShouldAbandonQuest(uint32 questId, Player* bot) = 0;

    /**
     * @brief Quest execution and coordination
     */
    virtual void UpdateQuestProgressDynamic(Player* bot) = 0;
    virtual void ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual bool CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void HandleQuestCompletionDynamic(Player* bot, uint32 questId) = 0;

    /**
     * @brief Group quest coordination
     */
    virtual bool FormQuestGroup(uint32 questId, Player* initiator) = 0;
    virtual void CoordinateGroupQuest(Group* group, uint32 questId) = 0;
    virtual void ShareQuestProgress(Group* group, uint32 questId) = 0;
    virtual bool CanShareQuest(uint32 questId, Player* from, Player* to) = 0;

    /**
     * @brief Quest pathfinding and navigation
     */
    virtual Position GetNextQuestLocation(Player* bot, uint32 questId) = 0;
    virtual ::std::vector<Position> GenerateQuestPath(Player* bot, uint32 questId) = 0;
    virtual void HandleQuestNavigation(Player* bot, uint32 questId) = 0;
    virtual bool IsQuestLocationReachable(Player* bot, const Position& location) = 0;

    /**
     * @brief Dynamic quest adaptation
     */
    virtual void AdaptQuestDifficulty(uint32 questId, Player* bot) = 0;
    virtual void HandleQuestStuckState(Player* bot, uint32 questId) = 0;
    virtual void RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void OptimizeQuestOrder(Player* bot) = 0;

    /**
     * @brief Quest chain management (dynamic)
     */
    virtual void TrackQuestChains(Player* bot) = 0;
    virtual ::std::vector<uint32> GetQuestChain(uint32 questId) = 0;
    virtual uint32 GetNextQuestInChainDynamic(uint32 completedQuestId) = 0;
    virtual void AdvanceQuestChain(Player* bot, uint32 completedQuestId) = 0;

    /**
     * @brief Zone-based quest optimization
     */
    virtual void OptimizeZoneQuests(Player* bot) = 0;
    virtual ::std::vector<uint32> GetZoneQuests(uint32 zoneId, Player* bot) = 0;
    virtual void PlanZoneCompletion(Player* bot, uint32 zoneId) = 0;
    virtual bool ShouldMoveToNewZone(Player* bot) = 0;

    /**
     * @brief Quest reward analysis
     */
    virtual QuestReward AnalyzeQuestReward(uint32 questId, Player* bot) = 0;
    virtual float CalculateQuestValue(uint32 questId, Player* bot) = 0;
    virtual bool IsQuestWorthwhile(uint32 questId, Player* bot) = 0;

    /**
     * @brief Configuration and settings (dynamic)
     */
    virtual void SetQuestStrategy(uint32 botGuid, QuestSelectionStrategy strategy) = 0;
    virtual QuestSelectionStrategy GetQuestStrategy(uint32 botGuid) = 0;
    virtual void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests) = 0;
    virtual void EnableQuestGrouping(uint32 botGuid, bool enable) = 0;

    // ========================================================================
    // UNIFIED OPERATIONS (combining all modules)
    // ========================================================================

    /**
     * @brief Complete end-to-end quest processing
     *
     * This method orchestrates the entire quest flow:
     * 1. Discovery and validation (Pickup + Validation modules)
     * 2. Assignment and prioritization (Dynamic module)
     * 3. Execution and tracking (Completion module)
     * 4. Turn-in and reward selection (TurnIn module)
     *
     * @param bot Bot to process quests for
     */
    virtual void ProcessCompleteQuestFlow(Player* bot) = 0;

    /**
     * @brief Get comprehensive quest recommendation
     *
     * Combines validation, priority calculation, and reward analysis to provide:
     * - Quest eligibility status
     * - Priority score
     * - Recommended approach
     * - Expected reward value
     * - Reasoning
     *
     * @param bot Bot context
     * @param questId Quest to evaluate
     * @return Detailed recommendation string
     */
    virtual ::std::string GetQuestRecommendation(Player* bot, uint32 questId) = 0;

    /**
     * @brief Optimize quest load for a bot
     *
     * Analyzes current quest load and suggests:
     * - Which quests to abandon
     * - Which quests to prioritize
     * - Optimal completion order
     * - Zone optimization opportunities
     *
     * @param bot Bot to optimize for
     */
    virtual void OptimizeBotQuestLoad(Player* bot) = 0;

    /**
     * @brief Get statistics for quest operations
     * @return Statistics string (for debugging/monitoring)
     */
    virtual ::std::string GetQuestStatistics() const = 0;

    /**
     * @brief Performance monitoring
     */
    virtual QuestMetrics GetBotQuestMetrics(uint32 botGuid) = 0;
    virtual QuestMetrics GetGlobalQuestMetrics() = 0;

    /**
     * @brief Turn-in performance monitoring
     */
    virtual TurnInMetrics GetBotTurnInMetrics(uint32 botGuid) = 0;
    virtual TurnInMetrics GetGlobalTurnInMetrics() = 0;

    /**
     * @brief Validation performance monitoring
     */
    virtual ValidationMetrics GetValidationMetrics() = 0;

    /**
     * @brief Update and maintenance
     */
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateBotTurnIns(Player* bot, uint32 diff) = 0;
    virtual void ProcessScheduledTurnIns() = 0;
    virtual void CleanupCompletedTurnIns() = 0;
    virtual void CleanupCompletedQuests() = 0;
    virtual void ValidateQuestStates() = 0;
};

} // namespace Playerbot
