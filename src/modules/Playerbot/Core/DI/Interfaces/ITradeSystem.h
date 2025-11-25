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
#include <atomic>
#include <chrono>

namespace Playerbot
{

// Forward declarations
enum class TradeDecision : uint8;
struct VendorInfo;
struct TradeSession;
struct TradeConfiguration;

// TradeMetrics definition (needs full definition for return by value)
struct TradeMetrics
{
    std::atomic<uint32> tradesInitiated{0};
    std::atomic<uint32> tradesCompleted{0};
    std::atomic<uint32> tradesCancelled{0};
    std::atomic<uint32> vendorTransactions{0};
    std::atomic<uint32> repairTransactions{0};
    std::atomic<float> averageTradeValue{1000.0f};
    std::atomic<float> tradeSuccessRate{0.8f};
    std::atomic<uint32> totalGoldTraded{0};
    std::atomic<uint32> totalItemsTraded{0};
    std::chrono::steady_clock::time_point lastUpdate;

    // Default constructor
    TradeMetrics() = default;

    // Copy constructor - loads atomic values
    TradeMetrics(const TradeMetrics& other)
        : tradesInitiated(other.tradesInitiated.load())
        , tradesCompleted(other.tradesCompleted.load())
        , tradesCancelled(other.tradesCancelled.load())
        , vendorTransactions(other.vendorTransactions.load())
        , repairTransactions(other.repairTransactions.load())
        , averageTradeValue(other.averageTradeValue.load())
        , tradeSuccessRate(other.tradeSuccessRate.load())
        , totalGoldTraded(other.totalGoldTraded.load())
        , totalItemsTraded(other.totalItemsTraded.load())
        , lastUpdate(other.lastUpdate)
    {
    }

    // Copy assignment operator - loads atomic values
    TradeMetrics& operator=(const TradeMetrics& other)
    {
        if (this != &other)
        {
            tradesInitiated.store(other.tradesInitiated.load());
            tradesCompleted.store(other.tradesCompleted.load());
            tradesCancelled.store(other.tradesCancelled.load());
            vendorTransactions.store(other.vendorTransactions.load());
            repairTransactions.store(other.repairTransactions.load());
            averageTradeValue.store(other.averageTradeValue.load());
            tradeSuccessRate.store(other.tradeSuccessRate.load());
            totalGoldTraded.store(other.totalGoldTraded.load());
            totalItemsTraded.store(other.totalItemsTraded.load());
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }

    void Reset() {
        tradesInitiated = 0; tradesCompleted = 0; tradesCancelled = 0;
        vendorTransactions = 0; repairTransactions = 0; averageTradeValue = 1000.0f;
        tradeSuccessRate = 0.8f; totalGoldTraded = 0; totalItemsTraded = 0;
        lastUpdate = std::chrono::steady_clock::now();
    }

    float GetCompletionRate() const {
        uint32 initiated = tradesInitiated.load();
        uint32 completed = tradesCompleted.load();
        return initiated > 0 ? (float)completed / initiated : 0.0f;
    }
};

class TC_GAME_API ITradeSystem
{
public:
    virtual ~ITradeSystem() = default;

    // Core trade functionality
    virtual bool InitiateTrade(Player* initiator, Player* target) = 0;
    virtual void ProcessTradeRequest(uint32 sessionId, TradeDecision decision) = 0;
    virtual void UpdateTradeSession(uint32 sessionId) = 0;
    virtual void CompleteTradeSession(uint32 sessionId) = 0;
    virtual void CancelTradeSession(uint32 sessionId) = 0;

    // Player-to-player trading
    virtual bool CanInitiateTrade(Player* initiator, Player* target) = 0;
    virtual TradeDecision EvaluateTradeRequest(uint32 sessionId) = 0;

    // Vendor interactions using TrinityCore data
    virtual void LoadVendorDatabase() = 0;
    virtual std::vector<VendorInfo> FindNearbyVendors(float radius = 100.0f) = 0;
    virtual bool InteractWithVendor(uint32 vendorGuid) = 0;

    // Vendor purchasing and selling
    virtual void ProcessVendorBuy(uint32 vendorGuid, uint32 itemId, uint32 count) = 0;
    virtual void ProcessVendorSell(uint32 vendorGuid, uint32 itemGuid, uint32 count) = 0;
    virtual bool CanBuyFromVendor(uint32 vendorGuid, uint32 itemId) = 0;

    // Equipment repair using TrinityCore repair vendors
    virtual void AutoRepairEquipment() = 0;
    virtual std::vector<uint32> FindRepairVendors(float radius = 200.0f) = 0;
    virtual void ProcessEquipmentRepair(uint32 vendorGuid) = 0;

    // Innkeeper services using TrinityCore innkeeper data
    virtual void InteractWithInnkeeper(uint32 innkeeperGuid) = 0;
    virtual std::vector<uint32> FindNearbyInnkeepers(float radius = 150.0f) = 0;

    // Intelligent trade decision making
    virtual float AnalyzeTradeValue(const TradeSession& session) = 0;
    virtual bool IsTradeWorthwhile(const TradeSession& session) = 0;

    // Trade safety and validation
    virtual bool ValidateTradeSession(const TradeSession& session) = 0;
    virtual bool DetectSuspiciousTradeActivity(const TradeSession& session) = 0;

    // Performance monitoring
    virtual TradeMetrics GetPlayerTradeMetrics() = 0;
    virtual TradeMetrics GetGlobalTradeMetrics() = 0;

    // Automated vendor management
    virtual void AutoSellJunkItems() = 0;
    virtual void AutoBuyConsumables() = 0;

    // Configuration and settings
    virtual void SetTradeConfiguration(const TradeConfiguration& config) = 0;
    virtual TradeConfiguration GetTradeConfiguration() = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessActiveTrades() = 0;
    virtual void CleanupExpiredTradeSessions() = 0;
};

} // namespace Playerbot
