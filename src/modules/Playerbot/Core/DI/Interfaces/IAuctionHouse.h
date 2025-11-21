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
#include <vector>
#include <string>
#include <queue>
#include <utility>
#include <atomic>
#include <unordered_set>

class Player;
class AuctionHouseObject;

namespace Playerbot
{

// Forward declarations
struct AuctionSearchQuery;

// AuctionItem definition (needs full definition for use in std::vector)
// Moved from AuctionHouse.h to interface for proper dependency management
struct AuctionItem
{
    uint32 auctionId;
    uint32 itemId;
    uint32 itemGuid;
    uint32 stackCount;
    uint32 currentBid;
    uint32 buyoutPrice;
    uint32 timeLeft;
    uint32 sellerGuid;
    std::string sellerName;
    uint32 quality;
    uint32 itemLevel;
    bool hasEnchants;
    bool hasSockets;
    float marketValue;
    float pricePerItem;
    bool isBargain;
    uint32 lastSeen;

    AuctionItem() : auctionId(0), itemId(0), itemGuid(0), stackCount(1)
        , currentBid(0), buyoutPrice(0), timeLeft(0), sellerGuid(0)
        , quality(0), itemLevel(0), hasEnchants(false), hasSockets(false)
        , marketValue(0.0f), pricePerItem(0.0f), isBargain(false)
        , lastSeen(0) {}  // Changed from GameTime::GetGameTimeMS() to 0 to avoid dependency
};

// Auction strategy types for bot behavior (full definition needed for return by value)
enum class AuctionStrategy : uint8
{
    CONSERVATIVE = 0,      // Undercut by 1% - safe, slow profits
    AGGRESSIVE = 1,        // Undercut by 5-10% - faster sales
    PREMIUM = 2,           // List at market average - wait for buyers
    QUICK_SALE = 3,        // Undercut by 20% - immediate sales
    MARKET_MAKER = 4,      // Buy low, sell high - active trading
    SMART_PRICING = 5,     // AI-driven pricing based on trends
    OPPORTUNISTIC = 6      // Look for bargains and flip opportunities
};

enum class AuctionActionType : uint8
{
    BUY_ITEM        = 0,
    SELL_ITEM       = 1,
    CANCEL_AUCTION  = 2,
    UPDATE_BID      = 3,
    SEARCH_MARKET   = 4,
    ANALYZE_PRICES  = 5
};

// AuctionMetrics definition (needs full definition for return by value)
struct AuctionMetrics
{
    std::atomic<uint32> auctionsCreated{0};
    std::atomic<uint32> auctionsSold{0};
    std::atomic<uint32> auctionsCancelled{0};
    std::atomic<uint32> itemsPurchased{0};
    std::atomic<uint32> bargainsFound{0};
    std::atomic<uint32> totalGoldSpent{0};
    std::atomic<uint32> totalGoldEarned{0};
    std::atomic<uint32> marketScans{0};
    std::atomic<float> averageProfitMargin{0.0f};

    void Reset()
    {
        auctionsCreated = 0;
        auctionsSold = 0;
        auctionsCancelled = 0;
        itemsPurchased = 0;
        bargainsFound = 0;
        totalGoldSpent = 0;
        totalGoldEarned = 0;
        marketScans = 0;
        averageProfitMargin = 0.0f;
    }

    int32 GetNetProfit() const
    {
        return static_cast<int32>(totalGoldEarned.load()) - static_cast<int32>(totalGoldSpent.load());
    }

    float GetROI() const
    {
        uint32 spent = totalGoldSpent.load();
        return spent > 0 ? static_cast<float>(totalGoldEarned.load()) / spent : 0.0f;
    }
};

// AuctionSession definition (needs full definition for return by value)
struct AuctionSession
{
    uint32 sessionId;
    uint32 playerGuid;
    AuctionActionType actionType;
    std::vector<AuctionItem> searchResults;
    std::vector<uint32> targetAuctions;
    std::queue<std::pair<AuctionActionType, uint32>> actionQueue;
    uint32 sessionStartTime;
    uint32 budgetUsed;
    uint32 itemsSold;
    uint32 itemsBought;
    bool isActive;

    AuctionSession() : sessionId(0), playerGuid(0), actionType(static_cast<AuctionActionType>(0))
        , sessionStartTime(0), budgetUsed(0), itemsSold(0), itemsBought(0), isActive(false) {}

    AuctionSession(uint32 id, uint32 pGuid, AuctionActionType action = static_cast<AuctionActionType>(0))
        : sessionId(id), playerGuid(pGuid), actionType(action), sessionStartTime(0)
        , budgetUsed(0), itemsSold(0), itemsBought(0), isActive(true) {}
};

// AuctionProfile definition (needs full definition for return by value)
struct AuctionProfile
{
    AuctionStrategy primaryStrategy;
    AuctionStrategy secondaryStrategy;
    uint32 maxBiddingBudget;
    uint32 maxListingBudget;
    float bargainThreshold; // Buy if price is below this % of market value
    float profitMargin; // Minimum profit margin for flipping
    std::vector<uint32> preferredItemTypes;
    std::vector<uint32> avoidedItemTypes;
    std::unordered_set<uint32> watchList; // Items to monitor
    std::unordered_set<uint32> blackList; // Never buy these items
    bool autoRelist; // Automatically relist unsold items
    bool autoBuyConsumables;
    bool autoSellJunk;
    uint32 maxAuctionsActive;

    AuctionProfile() : primaryStrategy(AuctionStrategy::CONSERVATIVE)
        , secondaryStrategy(AuctionStrategy::OPPORTUNISTIC), maxBiddingBudget(10000)
        , maxListingBudget(5000), bargainThreshold(0.7f), profitMargin(0.2f)
        , autoRelist(true), autoBuyConsumables(false), autoSellJunk(true)
        , maxAuctionsActive(10) {}
};

class TC_GAME_API IAuctionHouse
{
public:
    virtual ~IAuctionHouse() = default;

    // Core auction house operations
    virtual void SearchAuctionHouse(const AuctionSearchQuery& query) = 0;
    virtual bool PlaceAuctionBid(uint32 auctionId, uint32 bidAmount) = 0;
    virtual bool BuyoutAuction(uint32 auctionId) = 0;
    virtual bool CreateAuction(uint32 itemGuid, uint32 stackCount, uint32 bid, uint32 buyout, uint32 duration) = 0;
    virtual bool CancelAuction(uint32 auctionId) = 0;

    // Intelligent auction strategies
    virtual void ExecuteAuctionStrategy(AuctionStrategy strategy) = 0;
    virtual void ScanForBargains() = 0;
    virtual void AutoSellItems(const std::vector<uint32>& itemGuids) = 0;
    virtual void AutoBuyNeededItems() = 0;
    virtual void ManageActiveAuctions() = 0;

    // Market analysis and price discovery
    virtual float GetMarketPrice(uint32 itemId, uint32 stackSize) = 0;
    virtual float GetPriceHistory(uint32 itemId, uint32 days) = 0;
    virtual std::vector<AuctionItem> GetSimilarAuctions(uint32 itemId, uint32 maxResults) = 0;
    virtual bool IsPriceBelowMarket(uint32 itemId, uint32 price, float threshold) = 0;
    virtual void UpdateMarketData() = 0;

    // Advanced auction features
    virtual void SetAuctionProfile(const AuctionProfile& profile) = 0;
    virtual AuctionProfile GetAuctionProfile() = 0;

    // Auction monitoring and automation
    virtual uint32 StartAuctionSession(AuctionActionType primaryAction) = 0;
    virtual void UpdateAuctionSession(uint32 sessionId) = 0;
    virtual void CompleteAuctionSession(uint32 sessionId) = 0;
    virtual AuctionSession GetAuctionSession(uint32 sessionId) = 0;

    // Price optimization and profit calculation
    virtual uint32 CalculateOptimalListingPrice(uint32 itemId, uint32 stackSize) = 0;
    virtual uint32 CalculateMaxBidAmount(uint32 itemId, uint32 stackSize) = 0;
    virtual float CalculatePotentialProfit(const AuctionItem& auction, uint32 resellPrice) = 0;
    virtual bool IsWorthBuying(const AuctionItem& auction) = 0;
    virtual bool ShouldUndercut(uint32 itemId, uint32 currentLowest) = 0;

    // Market intelligence and learning
    virtual void TrackPriceMovement(uint32 itemId, uint32 price, uint32 timestamp) = 0;
    virtual void AnalyzeMarketTrends(uint32 itemId) = 0;
    virtual void LearnFromAuctionOutcome(uint32 auctionId, bool wasSuccessful) = 0;
    virtual void AdaptAuctionBehavior() = 0;

    // Specialized auction services
    virtual void HandleConsumableAutomation() = 0;
    virtual void HandleEquipmentUpgrades() = 0;
    virtual void HandleCraftingMaterials() = 0;
    virtual void HandleCollectibleTrading() = 0;
    virtual void HandleBulkItemTrading() = 0;

    // Competition analysis
    virtual void AnalyzeCompetition(uint32 itemId) = 0;
    virtual std::vector<uint32> GetFrequentSellers(uint32 itemId) = 0;
    virtual float GetCompetitorUndercutRate(uint32 sellerGuid) = 0;
    virtual void TrackCompetitorBehavior(uint32 sellerGuid, const AuctionItem& auction) = 0;

    // Performance monitoring
    virtual AuctionMetrics GetAuctionMetrics() = 0;
    virtual AuctionMetrics GetGlobalAuctionMetrics() = 0;

    // Auction house integration
    virtual void LoadAuctionData() = 0;
    virtual void SynchronizeWithAuctionHouseMgr() = 0;
    virtual AuctionHouseObject* GetAuctionHouseForPlayer() = 0;
    virtual bool ValidateAuctionAccess(uint32 auctionHouseId) = 0;

    // Configuration and customization
    virtual void SetAuctionHouseEnabled(bool enabled) = 0;
    virtual void SetMaxConcurrentAuctions(uint32 maxAuctions) = 0;
    virtual void SetAuctionBudget(uint32 budget) = 0;
    virtual void AddToWatchList(uint32 itemId) = 0;
    virtual void RemoveFromWatchList(uint32 itemId) = 0;

    // Error handling and recovery
    virtual void HandleAuctionError(uint32 sessionId, const std::string& error) = 0;
    virtual void RecoverFromAuctionFailure(uint32 sessionId) = 0;
    virtual void HandleInsufficientFunds(uint32 requiredAmount) = 0;
    virtual void HandleAuctionTimeout(uint32 auctionId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateAuctionSessions() = 0;
    virtual void UpdateMarketAnalysis() = 0;
    virtual void CleanupExpiredData() = 0;
};

} // namespace Playerbot
