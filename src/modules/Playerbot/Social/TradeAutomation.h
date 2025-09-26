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
#include "TradeSystem.h"
#include "VendorInteraction.h"
#include "Player.h"
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
 * @brief Comprehensive trade automation system orchestrating all trading activities
 *
 * This system coordinates player-to-player trades, vendor interactions, and
 * economic activities to provide seamless automated trading for playerbots.
 */
class TC_GAME_API TradeAutomation
{
public:
    static TradeAutomation* instance();

    // Core automation workflows
    void AutomatePlayerTrading(Player* player);
    void AutomateVendorInteractions(Player* player);
    void AutomateInventoryManagement(Player* player);
    void AutomateEconomicActivities(Player* player);

    // Intelligent trading workflows
    void ExecuteOptimalTradingSequence(Player* player);
    void ProcessTradeOpportunities(Player* player);
    void HandleTradeRequests(Player* player);
    void ManageTradeRelationships(Player* player);

    // Vendor automation workflows
    void ExecuteVendorMaintenanceRoutine(Player* player);
    void AutoRepairAndRestock(Player* player);
    void OptimizeVendorVisits(Player* player);
    void HandleSpecializedVendors(Player* player);

    // Economic optimization
    void AnalyzeMarketOpportunities(Player* player);
    void OptimizeSpendingBehavior(Player* player);
    void ManagePlayerEconomy(Player* player);
    void PlanEconomicActivities(Player* player);

    // Group trade coordination
    void CoordinateGroupTrading(Group* group);
    void ShareTradingOpportunities(Group* group);
    void OptimizeGroupEconomy(Group* group);
    void HandleGroupVendorTrips(Group* group);

    // Advanced automation features
    struct AutomationProfile
    {
        bool enablePlayerTrading;
        bool enableVendorAutomation;
        bool enableRepairAutomation;
        bool enableConsumableManagement;
        bool enableInventoryOptimization;
        float tradingAggressiveness; // 0.0 = conservative, 1.0 = aggressive
        uint32 maxTradingBudget;
        uint32 repairThreshold; // Repair when durability below %
        uint32 consumableThreshold; // Buy consumables when below count
        std::vector<uint32> priorityItems;
        std::vector<uint32> autoSellItems;

        AutomationProfile() : enablePlayerTrading(true), enableVendorAutomation(true)
            , enableRepairAutomation(true), enableConsumableManagement(true)
            , enableInventoryOptimization(true), tradingAggressiveness(0.7f)
            , maxTradingBudget(10000), repairThreshold(25), consumableThreshold(5) {}
    };

    void SetAutomationProfile(uint32 playerGuid, const AutomationProfile& profile);
    AutomationProfile GetAutomationProfile(uint32 playerGuid);

    // Automation state management
    struct AutomationState
    {
        bool isActive;
        uint32 currentTask;
        uint32 lastVendorVisit;
        uint32 lastRepairCheck;
        uint32 lastInventoryOptimization;
        std::queue<std::string> pendingTasks;
        std::vector<std::string> completedTasks;
        uint32 automationStartTime;
        uint32 totalAutomationTime;
        bool needsAttention;

        AutomationState() : isActive(false), currentTask(0), lastVendorVisit(0)
            , lastRepairCheck(0), lastInventoryOptimization(0), automationStartTime(getMSTime())
            , totalAutomationTime(0), needsAttention(false) {}
    };

    AutomationState GetAutomationState(uint32 playerGuid);
    void SetAutomationActive(uint32 playerGuid, bool active);
    bool IsAutomationActive(uint32 playerGuid);

    // Performance analytics
    struct AutomationMetrics
    {
        std::atomic<uint32> totalTradesProcessed{0};
        std::atomic<uint32> successfulTrades{0};
        std::atomic<uint32> vendorVisits{0};
        std::atomic<uint32> repairActions{0};
        std::atomic<uint32> goldSpent{0};
        std::atomic<uint32> goldEarned{0};
        std::atomic<float> averageTaskTime{30000.0f}; // 30 seconds
        std::atomic<float> automationEfficiency{0.85f};
        std::atomic<float> economicPerformance{1.0f}; // 1.0 = break even
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            totalTradesProcessed = 0; successfulTrades = 0; vendorVisits = 0;
            repairActions = 0; goldSpent = 0; goldEarned = 0;
            averageTaskTime = 30000.0f; automationEfficiency = 0.85f;
            economicPerformance = 1.0f;
            lastUpdate = std::chrono::steady_clock::now();
        }

        float GetTradeSuccessRate() const {
            uint32 total = totalTradesProcessed.load();
            uint32 successful = successfulTrades.load();
            return total > 0 ? (float)successful / total : 0.0f;
        }

        float GetNetProfit() const {
            uint32 earned = goldEarned.load();
            uint32 spent = goldSpent.load();
            return spent > 0 ? (float)earned / spent : 1.0f;
        }
    };

    AutomationMetrics GetPlayerAutomationMetrics(uint32 playerGuid);
    AutomationMetrics GetGlobalAutomationMetrics();

    // Decision making and strategy
    void MakeTradeDecision(Player* player, uint32 sessionId);
    void PlanVendorStrategy(Player* player);
    void AdaptTradingBehavior(Player* player);
    void OptimizeAutomationSettings(Player* player);

    // Economic intelligence
    void AnalyzePlayerEconomy(Player* player);
    void ForecastPlayerNeeds(Player* player);
    void IdentifyTradingOpportunities(Player* player);
    void OptimizeResourceAllocation(Player* player);

    // Safety and validation
    void ValidateAutomationState(Player* player);
    void DetectAutomationIssues(Player* player);
    void HandleAutomationFailures(Player* player);
    void RecoverFromErrors(Player* player);

    // Configuration and customization
    void LoadAutomationPresets();
    void SaveAutomationSettings(uint32 playerGuid);
    void ResetAutomationToDefaults(uint32 playerGuid);
    void ApplyAutomationTemplate(uint32 playerGuid, const std::string& templateName);

    // Update and maintenance
    void Update(uint32 diff);
    void UpdatePlayerAutomation(Player* player, uint32 diff);
    void ProcessAutomationQueue();
    void CleanupAutomationData();

private:
    TradeAutomation();
    ~TradeAutomation() = default;

    // Core automation data
    std::unordered_map<uint32, AutomationProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, AutomationState> _playerStates; // playerGuid -> state
    std::unordered_map<uint32, AutomationMetrics> _playerMetrics; // playerGuid -> metrics
    mutable std::mutex _automationMutex;

    // Task scheduling and execution
    struct AutomationTask
    {
        enum Type
        {
            CHECK_REPAIRS,
            VISIT_VENDOR,
            PROCESS_TRADES,
            OPTIMIZE_INVENTORY,
            ANALYZE_ECONOMY,
            UPDATE_STRATEGIES
        };

        Type type;
        uint32 playerGuid;
        uint32 priority;
        uint32 scheduledTime;
        uint32 timeoutTime;
        std::string description;
        bool isCompleted;

        AutomationTask(Type t, uint32 guid, uint32 prio = 100)
            : type(t), playerGuid(guid), priority(prio), scheduledTime(getMSTime())
            , timeoutTime(getMSTime() + 300000), isCompleted(false) {}
    };

    std::priority_queue<AutomationTask> _taskQueue;
    std::unordered_map<uint32, std::vector<AutomationTask>> _playerTaskQueues; // playerGuid -> tasks
    mutable std::mutex _taskMutex;

    // Economic analysis and intelligence
    struct EconomicProfile
    {
        uint32 playerGuid;
        uint32 currentGold;
        uint32 averageIncome;
        uint32 averageExpenses;
        std::unordered_map<uint32, uint32> itemValues; // itemId -> estimated value
        std::vector<std::pair<uint32, uint32>> recentTransactions; // value, time
        float economicStability;
        float spendingRate;
        uint32 lastAnalysisTime;

        EconomicProfile(uint32 guid) : playerGuid(guid), currentGold(0), averageIncome(0)
            , averageExpenses(0), economicStability(1.0f), spendingRate(0.5f)
            , lastAnalysisTime(getMSTime()) {}
    };

    std::unordered_map<uint32, EconomicProfile> _economicProfiles; // playerGuid -> profile

    // Performance tracking
    AutomationMetrics _globalMetrics;

    // Automation workflow implementations
    void ExecuteRepairWorkflow(Player* player);
    void ExecuteVendorWorkflow(Player* player);
    void ExecuteTradeWorkflow(Player* player);
    void ExecuteInventoryWorkflow(Player* player);

    // Task execution helpers
    void ProcessAutomationTask(const AutomationTask& task);
    void ScheduleTask(const AutomationTask& task);
    void CompleteTask(Player* player, const AutomationTask& task);
    void HandleTaskFailure(Player* player, const AutomationTask& task, const std::string& reason);

    // Decision making algorithms
    bool ShouldAcceptTrade(Player* player, uint32 sessionId);
    bool ShouldVisitVendor(Player* player, VendorType vendorType);
    bool NeedsRepair(Player* player);
    bool NeedsConsumables(Player* player);

    // Economic analysis helpers
    void UpdateEconomicProfile(Player* player);
    void AnalyzeSpendingPatterns(Player* player);
    void ForecastEconomicNeeds(Player* player);
    void OptimizeEconomicBehavior(Player* player);

    // Performance optimization
    void OptimizeAutomationPerformance(Player* player);
    void BatchAutomationOperations();
    void CacheFrequentlyUsedData();
    void UpdateAutomationMetrics(uint32 playerGuid, const AutomationTask& task, bool wasSuccessful);

    // Error handling and recovery
    void HandleAutomationError(Player* player, const std::string& error);
    void RecoverFromAutomationFailure(Player* player);
    void DiagnoseAutomationIssues(Player* player);
    void LogAutomationEvent(uint32 playerGuid, const std::string& event, const std::string& details = "");

    // Constants
    static constexpr uint32 AUTOMATION_UPDATE_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 TASK_PROCESSING_INTERVAL = 1000; // 1 second
    static constexpr uint32 ECONOMIC_ANALYSIS_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 MAX_PENDING_TASKS = 20;
    static constexpr uint32 TASK_TIMEOUT = 300000; // 5 minutes
    static constexpr float MIN_AUTOMATION_EFFICIENCY = 0.5f;
    static constexpr uint32 MAX_AUTOMATION_FAILURES = 5;
    static constexpr float ECONOMIC_STABILITY_THRESHOLD = 0.8f;
    static constexpr uint32 VENDOR_VISIT_COOLDOWN = 600000; // 10 minutes
    static constexpr uint32 REPAIR_CHECK_INTERVAL = 300000; // 5 minutes

    // Helper functions
    void OptimizeInventorySpace(Player* player);
    void OrganizeInventory(Player* player);
    Creature* FindNearestRepairVendor(Player* player);
    void RepairAllItems(Player* player);
    void RestockConsumables(Player* player);
};

} // namespace Playerbot