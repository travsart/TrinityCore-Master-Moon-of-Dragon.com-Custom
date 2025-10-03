#ifndef PLAYERBOT_AUCTION_MANAGER_H
#define PLAYERBOT_AUCTION_MANAGER_H

#include "Common.h"
#include "ObjectGuid.h"
#include "DatabaseEnv.h"
#include "Duration.h"
#include "Util.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

class Player;
class Item;
class AuctionHouseObject;

namespace Playerbot
{
    // Auction strategy types for bot behavior
    enum class AuctionStrategy : uint8
    {
        CONSERVATIVE = 0,      // Undercut by 1% - safe, slow profits
        AGGRESSIVE = 1,        // Undercut by 5-10% - faster sales
        PREMIUM = 2,           // List at market average - wait for buyers
        QUICK_SALE = 3,        // Undercut by 20% - immediate sales
        MARKET_MAKER = 4,      // Buy low, sell high - active trading
        SMART_PRICING = 5      // AI-driven pricing based on trends
    };

    // Market condition assessment
    enum class MarketCondition : uint8
    {
        UNKNOWN = 0,
        OVERSUPPLIED = 1,      // Many listings, low prices
        UNDERSUPPLIED = 2,     // Few listings, high prices
        STABLE = 3,            // Normal market conditions
        VOLATILE = 4,          // Rapid price changes
        PROFITABLE = 5         // Good flip opportunities
    };

    // Item price data for market analysis
    struct ItemPriceData
    {
        uint32 ItemId;
        uint64 CurrentPrice;       // Current lowest buyout
        uint64 AveragePrice7d;     // 7-day average
        uint64 MedianPrice7d;      // 7-day median (more resistant to outliers)
        uint64 MinPrice7d;         // Lowest seen in 7 days
        uint64 MaxPrice7d;         // Highest seen in 7 days
        uint32 DailyVolume;        // Average sales per day
        uint32 ActiveListings;     // Current active auctions
        float PriceTrend;          // Percentage change (positive = rising)
        MarketCondition Condition;
        TimePoint LastUpdate;

        ItemPriceData() : ItemId(0), CurrentPrice(0), AveragePrice7d(0),
            MedianPrice7d(0), MinPrice7d(0), MaxPrice7d(0), DailyVolume(0),
            ActiveListings(0), PriceTrend(0.0f), Condition(MarketCondition::UNKNOWN),
            LastUpdate(TimePoint::min()) {}
    };

    // Active auction tracking
    struct BotAuctionData
    {
        uint32 AuctionId;
        uint32 ItemId;
        uint32 ItemCount;
        uint64 StartPrice;
        uint64 BuyoutPrice;
        uint64 CostBasis;          // What we paid for the item
        TimePoint ListedTime;
        TimePoint ExpiryTime;
        bool IsCommodity;
        AuctionStrategy Strategy;

        uint64 GetExpectedProfit() const
        {
            // Account for 5% auction house cut
            uint64 salePrice = BuyoutPrice > 0 ? BuyoutPrice : StartPrice;
            uint64 ahCut = CalculatePct(salePrice, 5);
            return salePrice > (CostBasis + ahCut) ? (salePrice - CostBasis - ahCut) : 0;
        }
    };

    // Flip opportunity detection
    struct FlipOpportunity
    {
        uint32 AuctionId;
        uint32 ItemId;
        uint64 CurrentPrice;
        uint64 EstimatedResalePrice;
        uint64 EstimatedProfit;
        float ProfitMargin;        // Percentage profit
        MarketCondition Condition;
        uint32 RiskScore;          // 0-100, lower is better

        bool IsViable(uint64 minProfit, uint32 maxRisk) const
        {
            return EstimatedProfit >= minProfit && RiskScore <= maxRisk;
        }
    };

    // Auction house statistics
    struct AuctionHouseStats
    {
        uint32 TotalAuctionsCreated;
        uint32 TotalAuctionsSold;
        uint32 TotalAuctionsCancelled;
        uint32 TotalAuctionsExpired;
        uint32 TotalBidsPlaced;
        uint32 TotalCommoditiesBought;
        uint64 TotalGoldSpent;
        uint64 TotalGoldEarned;
        uint64 NetProfit;
        float SuccessRate;

        AuctionHouseStats() : TotalAuctionsCreated(0), TotalAuctionsSold(0),
            TotalAuctionsCancelled(0), TotalAuctionsExpired(0), TotalBidsPlaced(0),
            TotalCommoditiesBought(0), TotalGoldSpent(0), TotalGoldEarned(0),
            NetProfit(0), SuccessRate(0.0f) {}

        void UpdateSuccessRate()
        {
            uint32 total = TotalAuctionsCreated;
            if (total > 0)
                SuccessRate = (float(TotalAuctionsSold) / float(total)) * 100.0f;
        }
    };

    // Main auction manager class
    class BotAuctionManager
    {
    public:
        static BotAuctionManager* instance()
        {
            static BotAuctionManager instance;
            return &instance;
        }

        // Initialization and configuration
        void Initialize();
        void LoadConfiguration();
        void Update(uint32 diff);

        // Market scanning and analysis
        void ScanAuctionHouse(Player* bot, uint32 auctionHouseId);
        void AnalyzeMarketTrends(Player* bot);
        ItemPriceData GetItemPriceData(uint32 itemId) const;
        MarketCondition AssessMarketCondition(uint32 itemId) const;
        std::vector<FlipOpportunity> FindFlipOpportunities(Player* bot, uint32 auctionHouseId);

        // Auction creation and management
        bool CreateAuction(Player* bot, Item* item, uint64 bidPrice, uint64 buyoutPrice,
            uint32 duration = 12, AuctionStrategy strategy = AuctionStrategy::SMART_PRICING);
        bool CreateCommodityAuction(Player* bot, Item* item, uint32 quantity,
            uint64 unitPrice, uint32 duration = 12);
        bool CancelAuction(Player* bot, uint32 auctionId);
        void CancelUnprofitableAuctions(Player* bot);

        // Buying and bidding
        bool PlaceBid(Player* bot, uint32 auctionId, uint64 bidAmount);
        bool BuyCommodity(Player* bot, uint32 itemId, uint32 quantity);
        bool BuyAuction(Player* bot, uint32 auctionId);
        bool ExecuteFlipOpportunity(Player* bot, const FlipOpportunity& opportunity);

        // Pricing algorithms
        uint64 CalculateOptimalPrice(uint32 itemId, AuctionStrategy strategy = AuctionStrategy::SMART_PRICING);
        uint64 CalculateOptimalBid(uint32 itemId, uint64 currentBid, uint64 buyoutPrice);
        uint64 CalculateUndercutPrice(uint64 lowestPrice, AuctionStrategy strategy);
        uint64 CalculateVendorValue(Item* item);

        // Bot auction tracking
        void RegisterBotAuction(Player* bot, uint32 auctionId, const BotAuctionData& data);
        void UnregisterBotAuction(Player* bot, uint32 auctionId);
        std::vector<BotAuctionData> GetBotAuctions(Player* bot) const;
        void UpdateBotAuctionStatus(Player* bot);

        // Statistics and reporting
        AuctionHouseStats GetBotStats(ObjectGuid botGuid) const;
        void RecordAuctionSold(ObjectGuid botGuid, uint64 salePrice, uint64 costBasis);
        void RecordAuctionCreated(ObjectGuid botGuid);
        void RecordAuctionCancelled(ObjectGuid botGuid);
        void RecordCommodityPurchase(ObjectGuid botGuid, uint64 cost);
        void RecordBidPlaced(ObjectGuid botGuid, uint64 bidAmount);

        // Configuration getters
        bool IsEnabled() const { return _enabled; }
        uint32 GetUpdateInterval() const { return _updateInterval; }
        uint32 GetMaxActiveAuctions() const { return _maxActiveAuctions; }
        uint64 GetMinProfit() const { return _minProfit; }
        AuctionStrategy GetDefaultStrategy() const { return _defaultStrategy; }
        bool IsCommodityTradingEnabled() const { return _commodityEnabled; }
        bool IsMarketMakerEnabled() const { return _marketMakerEnabled; }
        uint32 GetMarketScanInterval() const { return _marketScanInterval; }
        uint32 GetMaxRiskScore() const { return _maxRiskScore; }

        // Utility functions
        AuctionHouseObject* GetAuctionHouse(Player* bot, uint32 auctionHouseId);
        bool CheckThrottle(Player* bot, bool isCommodity);
        uint32 GetAuctionHouseIdForBot(Player* bot);
        void SavePriceHistory(uint32 itemId, uint64 price);
        void LoadPriceHistory();

    private:
        BotAuctionManager();
        ~BotAuctionManager() = default;
        BotAuctionManager(const BotAuctionManager&) = delete;
        BotAuctionManager& operator=(const BotAuctionManager&) = delete;

        // Internal market analysis
        void UpdatePriceData(uint32 itemId, AuctionHouseObject* ah);
        void CalculatePriceTrends(uint32 itemId);
        uint32 CalculateRiskScore(const FlipOpportunity& opportunity) const;
        bool IsPriceHistoryStale(uint32 itemId) const;

        // Internal auction operations
        bool ValidateAuctionCreation(Player* bot, Item* item, uint64 bidPrice, uint64 buyoutPrice);
        bool ValidateBidPlacement(Player* bot, uint32 auctionId, uint64 bidAmount);
        uint64 CalculateDepositCost(Player* bot, Item* item, uint32 duration);

        // Thread safety
        mutable std::mutex _mutex;

        // Configuration
        bool _enabled;
        uint32 _updateInterval;
        uint32 _maxActiveAuctions;
        uint64 _minProfit;
        AuctionStrategy _defaultStrategy;
        bool _commodityEnabled;
        bool _marketMakerEnabled;
        uint32 _marketScanInterval;
        uint32 _maxRiskScore;
        float _undercutPercentage;
        uint32 _priceHistoryDays;

        // Market data cache
        std::unordered_map<uint32, ItemPriceData> _priceCache;
        std::unordered_map<uint32, std::vector<std::pair<TimePoint, uint64>>> _priceHistory;

        // Bot auction tracking
        std::unordered_map<ObjectGuid, std::vector<BotAuctionData>> _botAuctions;
        std::unordered_map<ObjectGuid, AuctionHouseStats> _botStats;

        // Update tracking
        uint32 _updateTimer;
        uint32 _marketScanTimer;
        TimePoint _lastPriceUpdate;
    };
}

#define sBotAuctionMgr Playerbot::BotAuctionManager::instance()

#endif // PLAYERBOT_AUCTION_MANAGER_H
