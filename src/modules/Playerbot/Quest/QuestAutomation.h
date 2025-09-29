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
#include "QuestPickup.h"
#include "QuestValidation.h"
#include "Player.h"
#include "QuestDef.h"
#include "Group.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

/**
 * @brief Comprehensive quest automation system orchestrating quest pickup workflow
 *
 * This system coordinates quest discovery, validation, prioritization, and pickup
 * to provide seamless automated quest acceptance for playerbots.
 */
class TC_GAME_API QuestAutomation
{
public:
    static QuestAutomation* instance();

    // Main automation workflows
    void AutomateQuestPickup(Player* bot);
    void AutomateZoneQuestCompletion(Player* bot, uint32 zoneId);
    void AutomateQuestChainProgression(Player* bot, uint32 questChainId);
    void AutomateGroupQuestCoordination(Group* group);

    // Intelligent quest discovery
    void PerformIntelligentQuestScan(Player* bot);
    void UpdateQuestOpportunities(Player* bot);
    void MonitorQuestGiverAvailability(Player* bot);
    std::vector<uint32> DiscoverOptimalQuests(Player* bot);

    // Automated decision making
    bool ShouldAcceptQuestAutomatically(uint32 questId, Player* bot);
    void MakeQuestAcceptanceDecision(uint32 questId, Player* bot);
    void ProcessQuestDecisionQueue(Player* bot);
    void HandleQuestConflicts(Player* bot, const std::vector<uint32>& conflictingQuests);

    // Workflow orchestration
    void ExecuteQuestPickupWorkflow(Player* bot, uint32 questId);
    void HandleQuestPickupInterruption(Player* bot, uint32 questId, const std::string& reason);
    void RetryFailedQuestPickups(Player* bot);
    void OptimizeQuestPickupSequence(Player* bot);

    // Group coordination automation
    void AutomateGroupQuestSharing(Group* group);
    void CoordinateGroupQuestDecisions(Group* group, uint32 questId);
    void HandleGroupQuestDisagreements(Group* group, uint32 questId);
    void SynchronizeGroupQuestStates(Group* group);

    // Performance and efficiency
    void OptimizeQuestPickupPerformance(Player* bot);
    void MinimizeQuestGiverTravel(Player* bot);
    void BatchQuestPickupOperations(Player* bot);
    void PrioritizeHighValueQuests(Player* bot);

    // Adaptive behavior
    void AdaptToPlayerBehavior(Player* bot);
    void LearnFromQuestPickupHistory(Player* bot);
    void AdjustPickupStrategyBasedOnSuccess(Player* bot);
    void HandlePickupFailureRecovery(Player* bot);

    // Configuration management
    struct AutomationSettings
    {
        bool enableAutoPickup;
        bool enableGroupCoordination;
        bool enableChainProgression;
        bool enableZoneCompletion;
        bool enableIntelligentScanning;
        float pickupAggressiveness;     // 0.0 = conservative, 1.0 = aggressive
        uint32 maxConcurrentPickups;
        uint32 scanIntervalMs;
        uint32 maxTravelDistance;
        bool respectPlayerPreferences;
        bool enableAdaptiveBehavior;

        AutomationSettings()
            : enableAutoPickup(true), enableGroupCoordination(true)
            , enableChainProgression(true), enableZoneCompletion(true)
            , enableIntelligentScanning(true), pickupAggressiveness(0.7f)
            , maxConcurrentPickups(3), scanIntervalMs(30000), maxTravelDistance(200)
            , respectPlayerPreferences(true), enableAdaptiveBehavior(true) {}
    };

    void SetAutomationSettings(uint32 botGuid, const AutomationSettings& settings);
    AutomationSettings GetAutomationSettings(uint32 botGuid);

    // State tracking and monitoring
    struct AutomationState
    {
        bool isActive;
        uint32 currentQuestId;
        uint32 currentQuestGiverGuid;
        QuestAcceptanceStrategy activeStrategy;
        std::vector<uint32> pendingQuests;
        std::vector<uint32> completedQuests;
        uint32 lastScanTime;
        uint32 lastPickupTime;
        uint32 automationStartTime;
        uint32 consecutiveFailures;
        bool needsReconfiguration;

        AutomationState() : isActive(false), currentQuestId(0), currentQuestGiverGuid(0)
            , activeStrategy(QuestAcceptanceStrategy::LEVEL_APPROPRIATE), lastScanTime(0)
            , lastPickupTime(0), automationStartTime(getMSTime()), consecutiveFailures(0), needsReconfiguration(false) {}
    };

    AutomationState GetAutomationState(uint32 botGuid);
    void SetAutomationActive(uint32 botGuid, bool active);
    bool IsAutomationActive(uint32 botGuid);

    // Performance metrics and analytics
    struct AutomationMetrics
    {
        std::atomic<uint32> totalQuestsAutomated{0};
        std::atomic<uint32> successfulAutomations{0};
        std::atomic<uint32> failedAutomations{0};
        std::atomic<uint32> questsSkipped{0};
        std::atomic<float> averageAutomationTime{10000.0f};
        std::atomic<float> automationEfficiency{0.8f};
        std::atomic<uint32> totalTravelDistance{0};
        std::atomic<uint32> questGiversVisited{0};
        std::chrono::steady_clock::time_point lastMetricsUpdate;

        // Default constructor
        AutomationMetrics() : lastMetricsUpdate(std::chrono::steady_clock::now()) {}

        void Reset() {
            totalQuestsAutomated = 0; successfulAutomations = 0; failedAutomations = 0;
            questsSkipped = 0; averageAutomationTime = 10000.0f; automationEfficiency = 0.8f;
            totalTravelDistance = 0; questGiversVisited = 0;
            lastMetricsUpdate = std::chrono::steady_clock::now();
        }

        float GetSuccessRate() const {
            uint32 total = successfulAutomations.load() + failedAutomations.load();
            return total > 0 ? (float)successfulAutomations.load() / total : 0.0f;
        }

        // Copy constructor for atomic members
        AutomationMetrics(const AutomationMetrics& other)
            : totalQuestsAutomated(other.totalQuestsAutomated.load()),
              successfulAutomations(other.successfulAutomations.load()),
              failedAutomations(other.failedAutomations.load()),
              questsSkipped(other.questsSkipped.load()),
              averageAutomationTime(other.averageAutomationTime.load()),
              automationEfficiency(other.automationEfficiency.load()),
              totalTravelDistance(other.totalTravelDistance.load()),
              questGiversVisited(other.questGiversVisited.load()),
              lastMetricsUpdate(other.lastMetricsUpdate) {}

        // Assignment operator for atomic members
        AutomationMetrics& operator=(const AutomationMetrics& other) {
            if (this != &other) {
                totalQuestsAutomated = other.totalQuestsAutomated.load();
                successfulAutomations = other.successfulAutomations.load();
                failedAutomations = other.failedAutomations.load();
                questsSkipped = other.questsSkipped.load();
                averageAutomationTime = other.averageAutomationTime.load();
                automationEfficiency = other.automationEfficiency.load();
                totalTravelDistance = other.totalTravelDistance.load();
                questGiversVisited = other.questGiversVisited.load();
                lastMetricsUpdate = other.lastMetricsUpdate;
            }
            return *this;
        }
    };

    AutomationMetrics GetBotAutomationMetrics(uint32 botGuid);
    AutomationMetrics GetGlobalAutomationMetrics();

    // Error handling and recovery
    void HandleAutomationError(uint32 botGuid, const std::string& error);
    void RecoverFromAutomationFailure(Player* bot);
    void ResetAutomationState(uint32 botGuid);
    void DiagnoseAutomationIssues(Player* bot);

    // Update and maintenance
    void Update(uint32 diff);
    void UpdateBotAutomation(Player* bot, uint32 diff);
    void ProcessAutomationQueues();
    void CleanupAutomationData();

private:
    QuestAutomation();
    ~QuestAutomation() = default;

    // Core automation data
    std::unordered_map<uint32, AutomationSettings> _botSettings;
    std::unordered_map<uint32, AutomationState> _botStates;
    std::unordered_map<uint32, AutomationMetrics> _botMetrics;
    mutable std::mutex _automationMutex;

    // Automation workflows
    struct WorkflowStep
    {
        enum Type
        {
            SCAN_FOR_QUESTS,
            VALIDATE_QUEST,
            MOVE_TO_GIVER,
            INTERACT_WITH_GIVER,
            ACCEPT_QUEST,
            HANDLE_DIALOG,
            CONFIRM_ACCEPTANCE,
            UPDATE_STATE
        };

        Type type;
        uint32 questId;
        uint32 questGiverGuid;
        std::string description;
        uint32 timeoutMs;
        uint32 retryCount;
        bool isCompleted;

        WorkflowStep(Type t, uint32 qId = 0, uint32 gId = 0)
            : type(t), questId(qId), questGiverGuid(gId), timeoutMs(30000)
            , retryCount(0), isCompleted(false) {}
    };

    std::unordered_map<uint32, std::queue<WorkflowStep>> _botWorkflows; // botGuid -> workflow
    mutable std::mutex _workflowMutex;

    // Decision making system
    struct DecisionFactor
    {
        std::string name;
        float weight;
        float value;
        std::string reasoning;

        DecisionFactor(const std::string& n, float w, float v, const std::string& r = "")
            : name(n), weight(w), value(v), reasoning(r) {}
    };

    std::vector<DecisionFactor> AnalyzeQuestDecisionFactors(uint32 questId, Player* bot);
    float CalculateQuestAcceptanceScore(uint32 questId, Player* bot);
    bool MakeAutomatedDecision(uint32 questId, Player* bot, float threshold = 0.6f);

    // Workflow execution
    void InitializeWorkflow(Player* bot, uint32 questId);
    void ExecuteWorkflowStep(Player* bot, WorkflowStep& step);
    void HandleWorkflowFailure(Player* bot, const WorkflowStep& step, const std::string& reason);
    void CompleteWorkflow(Player* bot, uint32 questId, bool wasSuccessful);

    // Intelligence and adaptation
    struct LearningData
    {
        std::unordered_map<uint32, uint32> questAcceptanceHistory; // questId -> acceptance count
        std::unordered_map<uint32, float> questSuccessRates; // questId -> success rate
        std::unordered_map<uint32, uint32> questGiverInteractionTimes; // giverGuid -> avg time
        std::unordered_map<uint8, float> strategyEffectiveness; // strategy -> effectiveness
        uint32 totalExperience;
        uint32 lastLearningUpdate;

        LearningData() : totalExperience(0), lastLearningUpdate(getMSTime()) {}
    };

    std::unordered_map<uint32, LearningData> _botLearningData;
    void UpdateLearningData(Player* bot, uint32 questId, bool wasSuccessful, uint32 timeSpent);
    void AdaptStrategyBasedOnLearning(Player* bot);

    // Group coordination helpers
    void InitiateGroupQuestDiscussion(Group* group, uint32 questId);
    void ProcessGroupQuestVotes(Group* group, uint32 questId);
    void ResolveGroupQuestDecision(Group* group, uint32 questId, bool accept);
    void HandleGroupMemberDisagreement(Group* group, uint32 questId, Player* dissenter);

    // Performance optimization
    void OptimizeAutomationPipeline(Player* bot);
    void BatchAutomationOperations();
    void PreemptiveQuestScanning(Player* bot);
    void CacheQuestDecisions(Player* bot);

    // Helper functions
    std::vector<Creature*> FindNearbyQuestGivers(Player* bot);
    std::vector<uint32> GetAvailableQuestsFromGiver(Player* bot, Creature* questGiver);
    std::vector<uint32> GetZoneQuests(uint32 zoneId, Player* bot);
    uint32 GetNextQuestInChain(uint32 completedQuestId);
    void AcceptQuest(Player* bot, uint32 questId, Creature* questGiver = nullptr);
    bool IsQuestWorthAutomating(uint32 questId, Player* bot);
    uint32 EstimateQuestCompletionTime(uint32 questId, Player* bot);
    float CalculateQuestEfficiencyScore(uint32 questId, Player* bot);
    float CalculateDistanceFactor(Player* bot, uint32 questId);
    void UpdateAutomationMetrics(uint32 botGuid, bool wasSuccessful, uint32 timeSpent);
    void LogAutomationEvent(uint32 botGuid, const std::string& event, const std::string& details = "");

    // Global automation metrics
    AutomationMetrics _globalMetrics;

    // Constants
    static constexpr uint32 AUTOMATION_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 SCAN_INTERVAL_MIN = 10000; // 10 seconds
    static constexpr uint32 SCAN_INTERVAL_MAX = 60000; // 1 minute
    static constexpr uint32 MAX_WORKFLOW_STEPS = 20;
    static constexpr uint32 WORKFLOW_TIMEOUT = 300000; // 5 minutes
    static constexpr float MIN_AUTOMATION_SCORE = 0.3f;
    static constexpr uint32 MAX_CONSECUTIVE_FAILURES = 5;
    static constexpr uint32 LEARNING_UPDATE_INTERVAL = 300000; // 5 minutes
    static constexpr float ADAPTATION_THRESHOLD = 0.1f;
    static constexpr uint32 GROUP_DECISION_TIMEOUT = 30000; // 30 seconds
};

} // namespace Playerbot