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
#include "AuctionHouse.h"
#include "MarketAnalysis.h"
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
 * @brief Comprehensive auction automation system orchestrating all auction activities
 *
 * This system coordinates auction house interactions, market monitoring, and
 * automated trading to provide seamless auction house experiences for playerbots.
 */
class TC_GAME_API AuctionAutomation
{
public:
    static AuctionAutomation* instance();

    // Core automation workflows
    void AutomateAuctionHouseActivities(Player* player);
    void AutomateBuyingActivities(Player* player);
    void AutomateSellingActivities(Player* player);
    void AutomateMarketMonitoring(Player* player);

    // Intelligent auction workflows
    void ExecuteOptimalAuctionSequence(Player* player);
    void ProcessMarketOpportunities(Player* player);
    void HandleAuctionManagement(Player* player);
    void ManageAuctionPortfolio(Player* player);

    // Automated buying workflows
    void ExecuteAutomatedBuying(Player* player);
    void AutoBuyConsumables(Player* player);
    void AutoBuyEquipmentUpgrades(Player* player);
    void AutoBuyCraftingMaterials(Player* player);

    // Automated selling workflows
    void ExecuteAutomatedSelling(Player* player);
    void AutoSellJunkItems(Player* player);
    void AutoSellOutdatedEquipment(Player* player);
    void AutoSellCraftedItems(Player* player);

    // Market monitoring and response
    void MonitorMarketConditions(Player* player);
    void RespondToMarketChanges(Player* player);
    void AdjustAuctionPrices(Player* player);
    void HandleCompetitiveUndercuts(Player* player);

    // Advanced automation features
    struct AutomationProfile
    {
        bool enableBuyingAutomation;
        bool enableSellingAutomation;
        bool enableMarketMonitoring;
        bool enableCompetitiveResponse;
        bool enableOpportunityScanning;
        float automationAggressiveness; // 0.0 = conservative, 1.0 = aggressive
        uint32 maxBiddingBudget;
        uint32 maxListingBudget;
        std::vector<uint32> priorityItems;
        std::vector<uint32> autoSellItems;
        std::unordered_set<uint32> protectedItems; // Never sell these
        AuctionStrategy primaryStrategy;
        AuctionStrategy fallbackStrategy;

        AutomationProfile() : enableBuyingAutomation(true), enableSellingAutomation(true)
            , enableMarketMonitoring(true), enableCompetitiveResponse(false)
            , enableOpportunityScanning(true), automationAggressiveness(0.6f)
            , maxBiddingBudget(10000), maxListingBudget(5000)
            , primaryStrategy(AuctionStrategy::CONSERVATIVE)
            , fallbackStrategy(AuctionStrategy::OPPORTUNISTIC) {}
    };

    void SetAutomationProfile(uint32 playerGuid, const AutomationProfile& profile);
    AutomationProfile GetAutomationProfile(uint32 playerGuid);

    // Automation state management
    struct AutomationState
    {
        bool isActive;
        uint32 currentTask;
        uint32 lastMarketScan;
        uint32 lastBuyingAttempt;
        uint32 lastSellingAttempt;
        std::queue<std::string> pendingActions;
        std::vector<std::string> completedActions;
        uint32 automationStartTime;
        uint32 totalAutomationTime;
        bool needsAttention;
        uint32 consecutiveFailures;

        AutomationState() : isActive(false), currentTask(0), lastMarketScan(0)
            , lastBuyingAttempt(0), lastSellingAttempt(0), automationStartTime(getMSTime())
            , totalAutomationTime(0), needsAttention(false), consecutiveFailures(0) {}
    };

    AutomationState GetAutomationState(uint32 playerGuid);
    void SetAutomationActive(uint32 playerGuid, bool active);
    bool IsAutomationActive(uint32 playerGuid);

    // Performance analytics
    struct AutomationMetrics
    {
        std::atomic<uint32> totalAuctionsProcessed{0};
        std::atomic<uint32> successfulPurchases{0};
        std::atomic<uint32> successfulSales{0};
        std::atomic<uint32> marketScans{0};
        std::atomic<uint32> goldSpent{0};
        std::atomic<uint32> goldEarned{0};
        std::atomic<float> averageTaskTime{45000.0f}; // 45 seconds
        std::atomic<float> automationEfficiency{0.8f};
        std::atomic<float> profitMargin{0.2f}; // 20% average profit
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            totalAuctionsProcessed = 0; successfulPurchases = 0; successfulSales = 0;
            marketScans = 0; goldSpent = 0; goldEarned = 0;
            averageTaskTime = 45000.0f; automationEfficiency = 0.8f;
            profitMargin = 0.2f; lastUpdate = std::chrono::steady_clock::now();
        }

        float GetSuccessRate() const {
            uint32 total = totalAuctionsProcessed.load();
            uint32 successful = successfulPurchases.load() + successfulSales.load();
            return total > 0 ? (float)successful / total : 0.0f;
        }

        uint32 GetNetProfit() const {
            uint32 earned = goldEarned.load();
            uint32 spent = goldSpent.load();
            return earned > spent ? earned - spent : 0;
        }
    };

    AutomationMetrics GetPlayerAutomationMetrics(uint32 playerGuid);
    AutomationMetrics GetGlobalAutomationMetrics();

    // Decision making and strategy
    void MakeAuctionDecision(Player* player, uint32 sessionId);
    void PlanBuyingStrategy(Player* player);
    void PlanSellingStrategy(Player* player);
    void AdaptAutomationBehavior(Player* player);

    // Market intelligence
    void AnalyzePlayerEconomy(Player* player);
    void ForecastPlayerNeeds(Player* player);
    void IdentifyAuctionOpportunities(Player* player);
    void OptimizeAuctionPortfolio(Player* player);

    // Safety and validation
    void ValidateAutomationState(Player* player);
    void DetectAutomationIssues(Player* player);
    void HandleAutomationFailures(Player* player);
    void RecoverFromErrors(Player* player);

    // Group coordination
    void CoordinateGroupAuctions(Group* group);
    void ShareMarketIntelligence(Group* group);
    void OptimizeGroupEconomy(Group* group);
    void HandleGroupBulkBuying(Group* group);

    // Configuration and customization
    void LoadAutomationPresets();
    void SaveAutomationSettings(uint32 playerGuid);
    void ResetAutomationToDefaults(uint32 playerGuid);
    void ApplyAutomationTemplate(uint32 playerGuid, const std::string& templateName);

    // Advanced features
    void HandleSeasonalBuying(Player* player);
    void HandleSpeculativeInvesting(Player* player);
    void HandleArbitrageOpportunities(Player* player);
    void HandleBulkTrading(Player* player);

    // Update and maintenance
    void Update(uint32 diff);
    void UpdatePlayerAutomation(Player* player, uint32 diff);
    void ProcessAutomationQueue();
    void CleanupAutomationData();

private:
    AuctionAutomation();
    ~AuctionAutomation() = default;

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
            SCAN_MARKET,
            BUY_OPPORTUNITY,
            SELL_ITEMS,
            ADJUST_PRICES,
            CANCEL_AUCTIONS,
            ANALYZE_PORTFOLIO
        };

        Type type;
        uint32 playerGuid;
        uint32 priority;
        uint32 scheduledTime;
        uint32 timeoutTime;
        std::string parameters;
        bool isCompleted;

        AutomationTask(Type t, uint32 guid, uint32 prio = 100)
            : type(t), playerGuid(guid), priority(prio), scheduledTime(getMSTime())
            , timeoutTime(getMSTime() + 600000), isCompleted(false) {}
    };

    std::priority_queue<AutomationTask> _taskQueue;
    std::unordered_map<uint32, std::vector<AutomationTask>> _playerTaskQueues; // playerGuid -> tasks
    mutable std::mutex _taskMutex;

    // Economic intelligence
    struct EconomicProfile
    {
        uint32 playerGuid;
        uint32 currentGold;
        uint32 liquidAssets; // Items easily convertible to gold
        uint32 totalPortfolioValue;
        std::unordered_map<uint32, uint32> inventoryValue; // itemId -> total value
        std::vector<std::pair<uint32, uint32>> recentTransactions; // value, time
        float economicGrowthRate;
        float riskTolerance;
        uint32 lastEconomicAnalysis;

        EconomicProfile(uint32 guid) : playerGuid(guid), currentGold(0), liquidAssets(0)
            , totalPortfolioValue(0), economicGrowthRate(0.05f), riskTolerance(0.5f)
            , lastEconomicAnalysis(getMSTime()) {}
    };

    std::unordered_map<uint32, EconomicProfile> _economicProfiles; // playerGuid -> profile

    // Performance tracking
    AutomationMetrics _globalMetrics;

    // Automation workflow implementations
    void ExecuteBuyingWorkflow(Player* player);
    void ExecuteSellingWorkflow(Player* player);
    void ExecuteMonitoringWorkflow(Player* player);
    void ExecuteOptimizationWorkflow(Player* player);

    // Task execution helpers
    void ProcessAutomationTask(const AutomationTask& task);
    void ScheduleTask(const AutomationTask& task);
    void CompleteTask(Player* player, const AutomationTask& task);
    void HandleTaskFailure(Player* player, const AutomationTask& task, const std::string& reason);

    // Decision making algorithms
    bool ShouldBuyItem(Player* player, uint32 itemId, uint32 price);
    bool ShouldSellItem(Player* player, uint32 itemGuid);
    bool ShouldAdjustPrice(Player* player, uint32 auctionId, uint32 currentPrice);
    bool ShouldCancelAuction(Player* player, uint32 auctionId);

    // Economic analysis helpers
    void UpdateEconomicProfile(Player* player);
    void AnalyzeSpendingPatterns(Player* player);
    void ForecastEconomicNeeds(Player* player);
    void OptimizeEconomicBehavior(Player* player);

    // Market response algorithms
    void HandlePriceWar(Player* player, uint32 itemId);
    void HandleMarketManipulation(Player* player, uint32 itemId);
    void HandleSupplyShortage(Player* player, uint32 itemId);
    void HandleDemandSurge(Player* player, uint32 itemId);

    // Risk management
    void AssessInvestmentRisk(Player* player, uint32 itemId, uint32 investment);
    void DiversifyPortfolio(Player* player);
    void ManageExposure(Player* player);
    void HandleLosses(Player* player);

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

    // Learning and adaptation
    void LearnFromMarketOutcomes(Player* player);
    void AdaptToPlayerBehavior(Player* player);
    void UpdateStrategyEffectiveness(Player* player, AuctionStrategy strategy, float outcome);
    void OptimizeAutomationParameters(Player* player);

    // Constants
    static constexpr uint32 AUTOMATION_UPDATE_INTERVAL = 30000; // 30 seconds
    static constexpr uint32 TASK_PROCESSING_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 MARKET_MONITORING_INTERVAL = 60000; // 1 minute
    static constexpr uint32 ECONOMIC_ANALYSIS_INTERVAL = 600000; // 10 minutes
    static constexpr uint32 MAX_PENDING_TASKS = 25;
    static constexpr uint32 TASK_TIMEOUT = 600000; // 10 minutes
    static constexpr float MIN_AUTOMATION_EFFICIENCY = 0.4f;
    static constexpr uint32 MAX_AUTOMATION_FAILURES = 5;
    static constexpr float PROFIT_TARGET = 0.15f; // 15% profit target
    static constexpr uint32 PRICE_ADJUSTMENT_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 MARKET_SCAN_COOLDOWN = 180000; // 3 minutes
};

} // namespace Playerbot