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
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "Item.h"
#include "../Core/DI/Interfaces/IAuctionHouse.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

namespace Playerbot
{

// AuctionActionType enum defined in IAuctionHouse.h interface
// AuctionItem struct defined in IAuctionHouse.h interface (moved for proper dependency management)

struct AuctionSearchQuery
{
    std::string itemName;
    uint32 itemId;
    uint32 minLevel;
    uint32 maxLevel;
    uint32 minQuality;
    uint32 maxQuality;
    uint32 maxPrice;
    uint32 minItemLevel;
    std::vector<uint32> itemClasses;
    std::vector<uint32> itemSubClasses;
    bool exactMatch;
    bool usableOnly;

    AuctionSearchQuery() : itemId(0), minLevel(0), maxLevel(0), minQuality(0)
        , maxQuality(6), maxPrice(0), minItemLevel(0), exactMatch(false)
        , usableOnly(false) {}
};

/**
 * @brief Advanced auction house system for automated buying and selling
 *
 * This system provides intelligent auction house interactions, market analysis,
 * and automated trading strategies for playerbots using TrinityCore's auction system.
 */
class TC_GAME_API AuctionHouse final : public IAuctionHouse
{
public:
    explicit AuctionHouse(Player* bot);
    ~AuctionHouse();
    AuctionHouse(AuctionHouse const&) = delete;
    AuctionHouse& operator=(AuctionHouse const&) = delete;

    // Core auction house operations using TrinityCore's AuctionHouseMgr
    void SearchAuctionHouse(const AuctionSearchQuery& query) override;
    bool PlaceAuctionBid(uint32 auctionId, uint32 bidAmount) override;
    bool BuyoutAuction(uint32 auctionId) override;
    bool CreateAuction(uint32 itemGuid, uint32 stackCount, uint32 bid, uint32 buyout, uint32 duration) override;
    bool CancelAuction(uint32 auctionId) override;

    // Intelligent auction strategies
    void ExecuteAuctionStrategy(AuctionStrategy strategy) override;
    void ScanForBargains() override;
    void AutoSellItems(const std::vector<uint32>& itemGuids) override;
    void AutoBuyNeededItems() override;
    void ManageActiveAuctions() override;

    // Market analysis and price discovery
    float GetMarketPrice(uint32 itemId, uint32 stackSize = 1) override;
    float GetPriceHistory(uint32 itemId, uint32 days = 7) override;
    std::vector<AuctionItem> GetSimilarAuctions(uint32 itemId, uint32 maxResults = 10) override;
    bool IsPriceBelowMarket(uint32 itemId, uint32 price, float threshold = 0.8f) override;
    void UpdateMarketData() override;

    // Advanced auction features (AuctionProfile defined in IAuctionHouse.h interface)
    void SetAuctionProfile(const AuctionProfile& profile);
    AuctionProfile GetAuctionProfile() override;

    // Auction monitoring and automation (AuctionSession defined in IAuctionHouse.h interface)
    uint32 StartAuctionSession(AuctionActionType primaryAction) override;
    void UpdateAuctionSession(uint32 sessionId) override;
    void CompleteAuctionSession(uint32 sessionId) override;
    AuctionSession GetAuctionSession(uint32 sessionId) override;

    // Price optimization and profit calculation
    uint32 CalculateOptimalListingPrice(uint32 itemId, uint32 stackSize) override;
    uint32 CalculateMaxBidAmount(uint32 itemId, uint32 stackSize) override;
    float CalculatePotentialProfit(const AuctionItem& auction, uint32 resellPrice) override;
    bool IsWorthBuying(const AuctionItem& auction) override;
    bool ShouldUndercut(uint32 itemId, uint32 currentLowest) override;

    // Market intelligence and learning
    void TrackPriceMovement(uint32 itemId, uint32 price, uint32 timestamp) override;
    void AnalyzeMarketTrends(uint32 itemId) override;
    void LearnFromAuctionOutcome(uint32 auctionId, bool wasSuccessful) override;
    void AdaptAuctionBehavior() override;

    // Specialized auction services
    void HandleConsumableAutomation() override;
    void HandleEquipmentUpgrades() override;
    void HandleCraftingMaterials() override;
    void HandleCollectibleTrading() override;
    void HandleBulkItemTrading() override;

    // Competition analysis
    void AnalyzeCompetition(uint32 itemId) override;
    std::vector<uint32> GetFrequentSellers(uint32 itemId) override;
    float GetCompetitorUndercutRate(uint32 sellerGuid) override;
    void TrackCompetitorBehavior(uint32 sellerGuid, const AuctionItem& auction) override;

    // Performance monitoring (AuctionMetrics defined in IAuctionHouse.h interface)
    AuctionMetrics GetAuctionMetrics() override;
    AuctionMetrics GetGlobalAuctionMetrics() override;

    // Auction house integration with TrinityCore
    void LoadAuctionData() override;
    void SynchronizeWithAuctionHouseMgr() override;
    AuctionHouseObject* GetAuctionHouseForPlayer() override;
    bool ValidateAuctionAccess(uint32 auctionHouseId) override;

    // Configuration and customization
    void SetAuctionHouseEnabled(bool enabled) override;
    void SetMaxConcurrentAuctions(uint32 maxAuctions) override;
    void SetAuctionBudget(uint32 budget) override;
    void AddToWatchList(uint32 itemId) override;
    void RemoveFromWatchList(uint32 itemId) override;

    // Error handling and recovery
    void HandleAuctionError(uint32 sessionId, const std::string& error) override;
    void RecoverFromAuctionFailure(uint32 sessionId) override;
    void HandleInsufficientFunds(uint32 requiredAmount) override;
    void HandleAuctionTimeout(uint32 auctionId) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateAuctionSessions() override;
    void UpdateMarketAnalysis() override;
    void CleanupExpiredData() override;

private:
    Player* _bot;

    // Per-bot instance data
    AuctionProfile _profile;
    std::unordered_map<uint32, AuctionSession> _activeSessions; // sessionId -> session (this bot's sessions)
    AuctionMetrics _metrics;
    bool _auctionHouseEnabled{true};

    // Shared static data (session IDs must be unique across all bots)
    static std::atomic<uint32> _nextSessionId;

    // Market data and analysis
    struct MarketData
    {
        uint32 itemId;
        std::vector<std::pair<uint32, uint32>> priceHistory; // price, timestamp
        float averagePrice;
        float medianPrice;
        uint32 totalVolume;
        uint32 activeListings;
        float volatility;
        uint32 lastAnalysisTime;

        MarketData() : itemId(0), averagePrice(0.0f), medianPrice(0.0f)
            , totalVolume(0), activeListings(0), volatility(0.0f)
            , lastAnalysisTime(0) {}

        MarketData(uint32 id) : itemId(id), averagePrice(0.0f), medianPrice(0.0f)
            , totalVolume(0), activeListings(0), volatility(0.0f)
            , lastAnalysisTime(GameTime::GetGameTimeMS()) {}
    };

    // Shared static market data (all bots share market intelligence)
    static std::unordered_map<uint32, MarketData> _marketData;
    static std::unordered_map<uint32, std::vector<AuctionItem>> _auctionCache;
    static Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TRADE_MANAGER> _marketMutex;

    // Competition tracking (shared)
    struct CompetitorProfile
    {
        uint32 sellerGuid;
        std::string sellerName;
        std::vector<uint32> frequentItems;
        std::vector<std::pair<uint32, uint32>> pricingHistory; // itemId, price
        float averageUndercutRate;
        float aggressiveness;
        uint32 totalAuctions;
        uint32 successfulSales;
        uint32 lastActivity;

        CompetitorProfile() : sellerGuid(0), averageUndercutRate(0.05f)
            , aggressiveness(0.5f), totalAuctions(0), successfulSales(0)
            , lastActivity(0) {}

        CompetitorProfile(uint32 guid) : sellerGuid(guid), averageUndercutRate(0.05f)
            , aggressiveness(0.5f), totalAuctions(0), successfulSales(0)
            , lastActivity(GameTime::GetGameTimeMS()) {}
    };

    static std::unordered_map<uint32, CompetitorProfile> _competitors; // sellerGuid -> profile

    // Performance tracking
    static AuctionMetrics _globalMetrics;

    // Strategy implementations
    void ExecuteConservativeStrategy(AuctionSession& session);
    void ExecuteAggressiveStrategy(AuctionSession& session);
    void ExecuteOpportunisticStrategy(AuctionSession& session);
    void ExecuteMarketMakerStrategy(AuctionSession& session);
    void ExecuteCollectorStrategy(AuctionSession& session);
    void ExecuteProfitFocusedStrategy(AuctionSession& session);

    // Market analysis helpers
    void UpdateItemMarketData(uint32 itemId);
    float CalculateMarketVolatility(const std::vector<std::pair<uint32, uint32>>& priceHistory);
    float PredictPriceMovement(uint32 itemId);
    bool IsMarketTrendy(uint32 itemId, bool& isRising);

    // Search and filtering
    std::vector<AuctionItem> FilterAuctionResults(const std::vector<AuctionItem>& auctions,
                                                 const AuctionSearchQuery& query);
    std::vector<AuctionItem> FindBargainAuctions( float maxPriceRatio = 0.8f);
    std::vector<AuctionItem> FindFlipOpportunities();
    bool MatchesSearchCriteria(const AuctionItem& auction, const AuctionSearchQuery& query);

    // Price calculation algorithms
    uint32 CalculateReasonableBid(const AuctionItem& auction, float aggressiveness = 0.5f);
    uint32 CalculateCompetitivePrice(uint32 itemId, uint32 stackSize);
    uint32 CalculateUndercutPrice(uint32 itemId, uint32 currentLowest, float undercutRate = 0.05f);
    float CalculateExpectedReturn(const AuctionItem& auction, uint32 resellPrice);

    // Auction execution helpers
    bool ExecuteBuyAction( const AuctionItem& auction);
    bool ExecuteSellAction( uint32 itemGuid, uint32 stackCount, uint32 price);
    bool ExecuteCancelAction( uint32 auctionId);
    void ProcessActionQueue( AuctionSession& session);

    // Learning and adaptation
    void UpdateSuccessRates( bool wasSuccessful, AuctionActionType actionType);
    void AdaptPricingStrategy();
    void LearnMarketPatterns(uint32 itemId);
    void UpdatePlayerPreferences( uint32 itemId, bool wasPurchased);

    // Performance optimization
    void CacheFrequentAuctions();
    void PreloadMarketData(const std::vector<uint32>& itemIds);
    void OptimizeAuctionQueries();
    void UpdateAuctionMetrics(uint32 playerGuid, AuctionActionType actionType,
                             uint32 goldAmount, bool wasSuccessful);

    // Constants
    static constexpr uint32 AUCTION_UPDATE_INTERVAL = 30000; // 30 seconds
    static constexpr uint32 MARKET_ANALYSIS_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 SESSION_TIMEOUT = 1800000; // 30 minutes
    static constexpr uint32 MAX_SEARCH_RESULTS = 100;
    static constexpr float DEFAULT_UNDERCUT_RATE = 0.05f; // 5%
    static constexpr float MIN_PROFIT_MARGIN = 0.1f; // 10%
    static constexpr uint32 PRICE_HISTORY_DAYS = 30;
    static constexpr uint32 MAX_CONCURRENT_SESSIONS = 50;
    static constexpr float BARGAIN_THRESHOLD = 0.8f; // 80% of market price
    static constexpr uint32 MARKET_DATA_CACHE_DURATION = 600000; // 10 minutes
};

} // namespace Playerbot