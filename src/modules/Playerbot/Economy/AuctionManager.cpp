#include "AuctionManager.h"
#include "AuctionHouseMgr.h"
#include "AuctionHousePackets.h"
#include "Player.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "Config.h"
#include "ItemTemplate.h"
#include "WorldSession.h"
#include <algorithm>
#include <numeric>

namespace Playerbot
{
    BotAuctionManager::BotAuctionManager()
        : _enabled(false)
        , _updateInterval(60000)
        , _maxActiveAuctions(10)
        , _minProfit(10000)
        , _defaultStrategy(AuctionStrategy::SMART_PRICING)
        , _commodityEnabled(true)
        , _marketMakerEnabled(false)
        , _marketScanInterval(300000)
        , _maxRiskScore(50)
        , _undercutPercentage(2.0f)
        , _priceHistoryDays(7)
        , _updateTimer(0)
        , _marketScanTimer(0)
        , _lastPriceUpdate(std::chrono::steady_clock::now())
    {
    }

    void BotAuctionManager::Initialize()
    {
        TC_LOG_INFO("playerbot", "BotAuctionManager: Initializing auction system...");

        LoadConfiguration();
        LoadPriceHistory();

        TC_LOG_INFO("playerbot", "BotAuctionManager: Initialization complete. Enabled: {}", _enabled);
    }

    void BotAuctionManager::LoadConfiguration()
    {
        _enabled = sConfigMgr->GetBoolDefault("Playerbot.Auction.Enable", false);
        _updateInterval = sConfigMgr->GetIntDefault("Playerbot.Auction.UpdateInterval", 60000);
        _maxActiveAuctions = sConfigMgr->GetIntDefault("Playerbot.Auction.MaxActiveAuctions", 10);
        _minProfit = sConfigMgr->GetIntDefault("Playerbot.Auction.MinProfit", 10000);
        _defaultStrategy = static_cast<AuctionStrategy>(
            sConfigMgr->GetIntDefault("Playerbot.Auction.DefaultStrategy", 5));
        _commodityEnabled = sConfigMgr->GetBoolDefault("Playerbot.Auction.CommodityEnabled", true);
        _marketMakerEnabled = sConfigMgr->GetBoolDefault("Playerbot.Auction.MarketMakerEnabled", false);
        _marketScanInterval = sConfigMgr->GetIntDefault("Playerbot.Auction.MarketScanInterval", 300000);
        _maxRiskScore = sConfigMgr->GetIntDefault("Playerbot.Auction.MaxRiskScore", 50);
        _undercutPercentage = sConfigMgr->GetFloatDefault("Playerbot.Auction.UndercutPercentage", 2.0f);
        _priceHistoryDays = sConfigMgr->GetIntDefault("Playerbot.Auction.PriceHistoryDays", 7);
    }

    void BotAuctionManager::Update(uint32 diff)
    {
        if (!_enabled)
            return;

        std::lock_guard<std::mutex> lock(_mutex);

        _updateTimer += diff;
        _marketScanTimer += diff;

        // Periodic market scan
        if (_marketScanTimer >= _marketScanInterval)
        {
            _marketScanTimer = 0;
            // Market scanning is done per-bot in their update cycle
        }

        // Update timer for general maintenance
        if (_updateTimer >= _updateInterval)
        {
            _updateTimer = 0;

            // Clean up stale price data
            auto now = std::chrono::steady_clock::now();
            for (auto it = _priceCache.begin(); it != _priceCache.end();)
            {
                if (now - it->second.LastUpdate > Minutes(_priceHistoryDays * 1440))
                    it = _priceCache.erase(it);
                else
                    ++it;
            }
        }
    }

    void BotAuctionManager::ScanAuctionHouse(Player* bot, uint32 auctionHouseId)
    {
        if (!bot || !_enabled)
            return;

        AuctionHouseObject* ah = GetAuctionHouse(bot, auctionHouseId);
        if (!ah)
        {
            TC_LOG_ERROR("playerbot", "BotAuctionManager::ScanAuctionHouse - Failed to get auction house {} for bot {}",
                auctionHouseId, bot->GetName());
            return;
        }

        std::lock_guard<std::mutex> lock(_mutex);

        // Get all auctions for market analysis using iterator API
        std::unordered_map<uint32, std::vector<uint64>> itemPrices;

        for (auto it = ah->GetAuctionsBegin(); it != ah->GetAuctionsEnd(); ++it)
        {
            AuctionPosting const& auction = it->second;
            if (auction.Items.empty())
                continue;

            uint32 itemId = auction.Items[0]->GetEntry();
            uint64 buyoutPrice = auction.BuyoutOrUnitPrice;

            if (buyoutPrice > 0)
                itemPrices[itemId].push_back(buyoutPrice);
        }

        // Update price cache for each item
        auto now = std::chrono::steady_clock::now();
        for (auto const& [itemId, prices] : itemPrices)
        {
            if (prices.empty())
                continue;

            auto& priceData = _priceCache[itemId];
            priceData.ItemId = itemId;
            priceData.ActiveListings = static_cast<uint32>(prices.size());
            priceData.LastUpdate = now;

            // Calculate current lowest price
            priceData.CurrentPrice = *std::min_element(prices.begin(), prices.end());

            // Save to history
            SavePriceHistory(itemId, priceData.CurrentPrice);

            // Calculate statistics from price history
            CalculatePriceTrends(itemId);
        }

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::ScanAuctionHouse - Scanned {} unique items for bot {}",
            itemPrices.size(), bot->GetName());
    }

    void BotAuctionManager::AnalyzeMarketTrends(Player* bot)
    {
        if (!bot || !_enabled)
            return;

        std::lock_guard<std::mutex> lock(_mutex);

        for (auto& [itemId, priceData] : _priceCache)
        {
            // Assess market condition based on price trends and listings
            if (priceData.PriceTrend > 10.0f)
                priceData.Condition = MarketCondition::UNDERSUPPLIED;
            else if (priceData.PriceTrend < -10.0f)
                priceData.Condition = MarketCondition::OVERSUPPLIED;
            else if (std::abs(priceData.PriceTrend) > 5.0f)
                priceData.Condition = MarketCondition::VOLATILE;
            else if (priceData.ActiveListings > 0 && priceData.CurrentPrice < priceData.MedianPrice7d * 0.8f)
                priceData.Condition = MarketCondition::PROFITABLE;
            else
                priceData.Condition = MarketCondition::STABLE;
        }
    }

    ItemPriceData BotAuctionManager::GetItemPriceData(uint32 itemId) const
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _priceCache.find(itemId);
        if (it != _priceCache.end())
            return it->second;

        return ItemPriceData();
    }

    MarketCondition BotAuctionManager::AssessMarketCondition(uint32 itemId) const
    {
        auto priceData = GetItemPriceData(itemId);
        return priceData.Condition;
    }

    std::vector<FlipOpportunity> BotAuctionManager::FindFlipOpportunities(Player* bot, uint32 auctionHouseId)
    {
        std::vector<FlipOpportunity> opportunities;

        if (!bot || !_enabled || !_marketMakerEnabled)
            return opportunities;

        AuctionHouseObject* ah = GetAuctionHouse(bot, auctionHouseId);
        if (!ah)
            return opportunities;

        std::lock_guard<std::mutex> lock(_mutex);

        for (auto auctionIt = ah->GetAuctionsBegin(); auctionIt != ah->GetAuctionsEnd(); ++auctionIt)
        {
            AuctionPosting const& auction = auctionIt->second;
            if (auction.Items.empty() || auction.BuyoutOrUnitPrice == 0)
                continue;

            uint32 itemId = auction.Items[0]->GetEntry();
            uint64 currentPrice = auction.BuyoutOrUnitPrice;

            // Get price data
            auto priceIt = _priceCache.find(itemId);
            if (priceIt == _priceCache.end())
                continue;

            const auto& priceData = priceIt->second;

            // Calculate potential profit (accounting for 5% AH cut)
            uint64 estimatedResale = priceData.MedianPrice7d;
            if (estimatedResale <= currentPrice)
                continue;

            uint64 ahCut = CalculatePct(estimatedResale, 5);
            uint64 estimatedProfit = estimatedResale - currentPrice - ahCut;

            if (estimatedProfit < _minProfit)
                continue;

            FlipOpportunity opp;
            opp.AuctionId = auctionIt->first; // Use auction ID from map key
            opp.ItemId = itemId;
            opp.CurrentPrice = currentPrice;
            opp.EstimatedResalePrice = estimatedResale;
            opp.EstimatedProfit = estimatedProfit;
            opp.ProfitMargin = (float(estimatedProfit) / float(currentPrice)) * 100.0f;
            opp.Condition = priceData.Condition;
            opp.RiskScore = CalculateRiskScore(opp);

            if (opp.IsViable(_minProfit, _maxRiskScore))
                opportunities.push_back(opp);
        }

        // Sort by profit margin
        std::sort(opportunities.begin(), opportunities.end(),
            [](const FlipOpportunity& a, const FlipOpportunity& b) {
                return a.ProfitMargin > b.ProfitMargin;
            });

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::FindFlipOpportunities - Found {} opportunities for bot {}",
            opportunities.size(), bot->GetName());

        return opportunities;
    }

    bool BotAuctionManager::CreateAuction(Player* bot, Item* item, uint64 bidPrice,
        uint64 buyoutPrice, uint32 duration, AuctionStrategy strategy)
    {
        if (!ValidateAuctionCreation(bot, item, bidPrice, buyoutPrice))
            return false;

        // Check throttle
        if (!CheckThrottle(bot, false))
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::CreateAuction - Bot {} is throttled",
                bot->GetName());
            return false;
        }

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return false;

        // Calculate optimal pricing if using smart pricing
        if (strategy == AuctionStrategy::SMART_PRICING)
        {
            uint64 optimalPrice = CalculateOptimalPrice(item->GetEntry(), strategy);
            if (optimalPrice > 0)
            {
                buyoutPrice = optimalPrice;
                bidPrice = CalculatePct(buyoutPrice, 80); // 80% of buyout as starting bid
            }
        }

        // Calculate deposit
        uint64 deposit = CalculateDepositCost(bot, item, duration);
        if (bot->GetMoney() < deposit)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::CreateAuction - Bot {} has insufficient gold for deposit",
                bot->GetName());
            return false;
        }

        // Create auction posting
        AuctionPosting posting;
        posting.Owner = bot->GetGUID();
        posting.OwnerAccount = bot->GetSession()->GetAccountGUID();
        posting.Items.push_back(item);
        posting.MinBid = bidPrice;
        posting.BuyoutOrUnitPrice = buyoutPrice;
        posting.Deposit = deposit;
        posting.StartTime = SystemTimePoint(std::chrono::system_clock::now());
        posting.EndTime = SystemTimePoint(std::chrono::system_clock::now() + std::chrono::hours(duration));

        // Deduct deposit
        bot->ModifyMoney(-int64(deposit));

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Add auction
        ah->AddAuction(trans, posting);

        CharacterDatabase.CommitTransaction(trans);

        // Track bot auction
        BotAuctionData auctionData;
        auctionData.AuctionId = posting.Id;
        auctionData.ItemId = item->GetEntry();
        auctionData.ItemCount = item->GetCount();
        auctionData.StartPrice = bidPrice;
        auctionData.BuyoutPrice = buyoutPrice;
        auctionData.CostBasis = CalculateVendorValue(item); // Use vendor value as baseline
        auctionData.ListedTime = std::chrono::steady_clock::now();
        auctionData.ExpiryTime = auctionData.ListedTime + Hours(duration);
        auctionData.IsCommodity = false;
        auctionData.Strategy = strategy;

        RegisterBotAuction(bot, posting.Id, auctionData);
        RecordAuctionCreated(bot->GetGUID());

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::CreateAuction - Bot {} created auction {} for item {} (bid: {}, buyout: {})",
            bot->GetName(), posting.Id, item->GetEntry(), bidPrice, buyoutPrice);

        return true;
    }

    bool BotAuctionManager::CreateCommodityAuction(Player* bot, Item* item,
        uint32 quantity, uint64 unitPrice, uint32 duration)
    {
        if (!_commodityEnabled || !bot || !item)
            return false;

        // Commodities use different throttle
        if (!CheckThrottle(bot, true))
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::CreateCommodityAuction - Bot {} is throttled",
                bot->GetName());
            return false;
        }

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return false;

        // Calculate deposit for commodity
        uint64 deposit = CalculateDepositCost(bot, item, duration);
        if (bot->GetMoney() < deposit)
            return false;

        // Create commodity posting
        AuctionPosting posting;
        posting.Owner = bot->GetGUID();
        posting.OwnerAccount = bot->GetSession()->GetAccountGUID();
        posting.Items.push_back(item);
        posting.MinBid = 0; // Commodities don't have bids
        posting.BuyoutOrUnitPrice = unitPrice;
        posting.Deposit = deposit;
        posting.StartTime = SystemTimePoint(std::chrono::system_clock::now());
        posting.EndTime = SystemTimePoint(std::chrono::system_clock::now() + std::chrono::hours(duration));

        bot->ModifyMoney(-int64(deposit));

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        ah->AddAuction(trans, posting);
        CharacterDatabase.CommitTransaction(trans);

        // Track auction
        BotAuctionData auctionData;
        auctionData.AuctionId = posting.Id;
        auctionData.ItemId = item->GetEntry();
        auctionData.ItemCount = quantity;
        auctionData.StartPrice = unitPrice * quantity;
        auctionData.BuyoutPrice = unitPrice * quantity;
        auctionData.CostBasis = CalculateVendorValue(item) * quantity;
        auctionData.ListedTime = std::chrono::steady_clock::now();
        auctionData.ExpiryTime = auctionData.ListedTime + Hours(duration);
        auctionData.IsCommodity = true;
        auctionData.Strategy = AuctionStrategy::SMART_PRICING;

        RegisterBotAuction(bot, posting.Id, auctionData);
        RecordAuctionCreated(bot->GetGUID());

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::CreateCommodityAuction - Bot {} created commodity auction for item {} x{} at {} per unit",
            bot->GetName(), item->GetEntry(), quantity, unitPrice);

        return true;
    }

    bool BotAuctionManager::CancelAuction(Player* bot, uint32 auctionId)
    {
        if (!bot || !_enabled)
            return false;

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return false;

        // Get auction pointer first
        AuctionPosting* auction = ah->GetAuction(auctionId);
        if (!auction)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::CancelAuction - Auction {} not found for bot {}",
                auctionId, bot->GetName());
            return false;
        }

        // Verify bot owns this auction
        if (auction->Owner != bot->GetGUID())
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::CancelAuction - Bot {} does not own auction {}",
                bot->GetName(), auctionId);
            return false;
        }

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Cancel auction (no deposit refund on manual cancel)
        ah->RemoveAuction(trans, auction);

        CharacterDatabase.CommitTransaction(trans);

        UnregisterBotAuction(bot, auctionId);
        RecordAuctionCancelled(bot->GetGUID());

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::CancelAuction - Bot {} cancelled auction {}",
            bot->GetName(), auctionId);

        return true;
    }

    void BotAuctionManager::CancelUnprofitableAuctions(Player* bot)
    {
        if (!bot || !_enabled)
            return;

        auto auctions = GetBotAuctions(bot);
        auto now = std::chrono::steady_clock::now();

        for (const auto& auction : auctions)
        {
            // Cancel if price has dropped significantly below our listing
            auto priceData = GetItemPriceData(auction.ItemId);
            if (priceData.CurrentPrice > 0 && priceData.CurrentPrice < auction.BuyoutPrice * 0.7f)
            {
                // Market crashed, cancel and relist lower
                CancelAuction(bot, auction.AuctionId);
                TC_LOG_DEBUG("playerbot", "BotAuctionManager::CancelUnprofitableAuctions - Cancelled auction {} due to market price drop",
                    auction.AuctionId);
            }
        }
    }

    bool BotAuctionManager::PlaceBid(Player* bot, uint32 auctionId, uint64 bidAmount)
    {
        if (!ValidateBidPlacement(bot, auctionId, bidAmount))
            return false;

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return false;

        AuctionPosting* auction = ah->GetAuction(auctionId);
        if (!auction)
            return false;

        // Ensure bid is higher than current
        if (bidAmount <= auction->BidAmount)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::PlaceBid - Bid amount {} too low for auction {}",
                bidAmount, auctionId);
            return false;
        }

        // Check bot has enough gold
        if (bot->GetMoney() < bidAmount)
            return false;

        // NOTE: PlaceBid is not a public API in AuctionHouseObject
        // This would require packet-based implementation through WorldSession
        // For now, return false as this needs proper packet handling
        TC_LOG_ERROR("playerbot", "BotAuctionManager::PlaceBid - Bid placement requires packet-based implementation (not yet supported for bots)");
        return false;

        RecordBidPlaced(bot->GetGUID(), bidAmount);

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::PlaceBid - Bot {} placed bid of {} on auction {}",
            bot->GetName(), bidAmount, auctionId);

        return true;
    }

    bool BotAuctionManager::BuyCommodity(Player* bot, uint32 itemId, uint32 quantity)
    {
        if (!_commodityEnabled || !bot)
            return false;

        if (!CheckThrottle(bot, true))
            return false;

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return false;

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Create commodity quote
        CommodityQuote const* quote = ah->CreateCommodityQuote(bot, itemId, quantity);

        if (!quote)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::BuyCommodity - Failed to create quote for item {} x{} for bot {}",
                itemId, quantity, bot->GetName());
            return false;
        }

        uint64 totalCost = quote->TotalPrice;
        if (bot->GetMoney() < totalCost)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::BuyCommodity - Bot {} has insufficient gold for commodity purchase (need: {}, has: {})",
                bot->GetName(), totalCost, bot->GetMoney());
            return false;
        }

        // Execute purchase (0ms delay for bots)
        ah->BuyCommodity(trans, bot, itemId, quantity, Milliseconds(0));

        CharacterDatabase.CommitTransaction(trans);

        RecordCommodityPurchase(bot->GetGUID(), totalCost);

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::BuyCommodity - Bot {} purchased commodity {} x{} for {} gold",
            bot->GetName(), itemId, quantity, totalCost);

        return true;
    }

    bool BotAuctionManager::BuyAuction(Player* bot, uint32 auctionId)
    {
        if (!bot || !_enabled)
            return false;

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return false;

        auto auction = ah->GetAuction(auctionId);
        if (!auction)
            return false;

        uint64 buyoutPrice = auction->BuyoutOrUnitPrice;
        if (buyoutPrice == 0)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::BuyAuction - Auction {} has no buyout price",
                auctionId);
            return false;
        }

        if (bot->GetMoney() < buyoutPrice)
            return false;

        // NOTE: BuyoutAuction is not a public API in AuctionHouseObject
        // This would require packet-based implementation through WorldSession
        // For now, return false as this needs proper packet handling
        TC_LOG_ERROR("playerbot", "BotAuctionManager::BuyAuction - Auction buyout requires packet-based implementation (not yet supported for bots)");
        return false;

        RecordCommodityPurchase(bot->GetGUID(), buyoutPrice);

        TC_LOG_DEBUG("playerbot", "BotAuctionManager::BuyAuction - Bot {} bought auction {} for {}",
            bot->GetName(), auctionId, buyoutPrice);

        return true;
    }

    bool BotAuctionManager::ExecuteFlipOpportunity(Player* bot, const FlipOpportunity& opportunity)
    {
        if (!_marketMakerEnabled)
            return false;

        // Buy the underpriced auction
        if (!BuyAuction(bot, opportunity.AuctionId))
            return false;

        TC_LOG_INFO("playerbot", "BotAuctionManager::ExecuteFlipOpportunity - Bot {} executed flip on item {} with estimated profit {}",
            bot->GetName(), opportunity.ItemId, opportunity.EstimatedProfit);

        // Item will be relisted in the bot's normal auction cycle
        return true;
    }

    uint64 BotAuctionManager::CalculateOptimalPrice(uint32 itemId, AuctionStrategy strategy)
    {
        auto priceData = GetItemPriceData(itemId);

        if (priceData.CurrentPrice == 0)
        {
            // No market data, use vendor value * 2
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
            if (proto)
                return proto->GetSellPrice() * 2;
            return 0;
        }

        uint64 basePrice = priceData.MedianPrice7d > 0 ? priceData.MedianPrice7d : priceData.CurrentPrice;

        switch (strategy)
        {
            case AuctionStrategy::CONSERVATIVE:
                // Undercut by 1%
                return CalculateUndercutPrice(priceData.CurrentPrice, strategy);

            case AuctionStrategy::AGGRESSIVE:
                // Undercut by 5-10%
                return CalculateUndercutPrice(priceData.CurrentPrice, strategy);

            case AuctionStrategy::PREMIUM:
                // List at median price (wait for market)
                return basePrice;

            case AuctionStrategy::QUICK_SALE:
                // Undercut by 20% for fast sale
                return CalculateUndercutPrice(priceData.CurrentPrice, strategy);

            case AuctionStrategy::MARKET_MAKER:
                // List at median + 5% (buy low, sell high)
                return basePrice + CalculatePct(basePrice, 5);

            case AuctionStrategy::SMART_PRICING:
            default:
            {
                // Adaptive pricing based on market condition
                switch (priceData.Condition)
                {
                    case MarketCondition::OVERSUPPLIED:
                        // Market saturated, be aggressive
                        return CalculateUndercutPrice(priceData.CurrentPrice, AuctionStrategy::AGGRESSIVE);

                    case MarketCondition::UNDERSUPPLIED:
                        // Low supply, can charge premium
                        return basePrice + CalculatePct(basePrice, 10);

                    case MarketCondition::VOLATILE:
                        // Unstable market, price conservatively
                        return CalculateUndercutPrice(priceData.CurrentPrice, AuctionStrategy::CONSERVATIVE);

                    case MarketCondition::PROFITABLE:
                        // Good flip opportunity, buy and relist at median
                        return basePrice;

                    case MarketCondition::STABLE:
                    default:
                        // Normal conditions, slight undercut
                        return CalculateUndercutPrice(priceData.CurrentPrice, AuctionStrategy::CONSERVATIVE);
                }
            }
        }

        return basePrice;
    }

    uint64 BotAuctionManager::CalculateOptimalBid(uint32 itemId, uint64 currentBid, uint64 buyoutPrice)
    {
        auto priceData = GetItemPriceData(itemId);

        // Don't bid if buyout is close to market price (just buy it)
        if (buyoutPrice > 0 && priceData.MedianPrice7d > 0)
        {
            if (buyoutPrice <= priceData.MedianPrice7d * 0.9f)
                return 0; // Signal to use buyout instead
        }

        // Bid up to 80% of market median
        uint64 maxBid = priceData.MedianPrice7d > 0 ? CalculatePct(priceData.MedianPrice7d, 80) : currentBid * 2;

        // Increment by 5%
        uint64 newBid = currentBid + CalculatePct(currentBid, 5);

        return std::min(newBid, maxBid);
    }

    uint64 BotAuctionManager::CalculateUndercutPrice(uint64 lowestPrice, AuctionStrategy strategy)
    {
        if (lowestPrice == 0)
            return 0;

        float undercutPct = 0.0f;

        switch (strategy)
        {
            case AuctionStrategy::CONSERVATIVE:
                undercutPct = 1.0f;
                break;
            case AuctionStrategy::AGGRESSIVE:
                undercutPct = urand(5, 10);
                break;
            case AuctionStrategy::QUICK_SALE:
                undercutPct = 20.0f;
                break;
            default:
                undercutPct = _undercutPercentage;
                break;
        }

        uint64 undercut = CalculatePct(lowestPrice, undercutPct);
        return lowestPrice > undercut ? lowestPrice - undercut : lowestPrice;
    }

    uint64 BotAuctionManager::CalculateVendorValue(Item* item)
    {
        if (!item)
            return 0;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return 0;

        return proto->GetSellPrice() * item->GetCount();
    }

    void BotAuctionManager::RegisterBotAuction(Player* bot, uint32 auctionId, const BotAuctionData& data)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _botAuctions[bot->GetGUID()].push_back(data);
    }

    void BotAuctionManager::UnregisterBotAuction(Player* bot, uint32 auctionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _botAuctions.find(bot->GetGUID());
        if (it != _botAuctions.end())
        {
            it->second.erase(
                std::remove_if(it->second.begin(), it->second.end(),
                    [auctionId](const BotAuctionData& data) { return data.AuctionId == auctionId; }),
                it->second.end());
        }
    }

    std::vector<BotAuctionData> BotAuctionManager::GetBotAuctions(Player* bot) const
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _botAuctions.find(bot->GetGUID());
        if (it != _botAuctions.end())
            return it->second;

        return std::vector<BotAuctionData>();
    }

    void BotAuctionManager::UpdateBotAuctionStatus(Player* bot)
    {
        if (!bot || !_enabled)
            return;

        uint32 ahId = GetAuctionHouseIdForBot(bot);
        AuctionHouseObject* ah = GetAuctionHouse(bot, ahId);
        if (!ah)
            return;

        auto auctions = GetBotAuctions(bot);
        auto now = std::chrono::steady_clock::now();

        for (const auto& botAuction : auctions)
        {
            auto auction = ah->GetAuction(botAuction.AuctionId);
            if (!auction)
            {
                // Auction ended or sold
                UnregisterBotAuction(bot, botAuction.AuctionId);
                continue;
            }

            // Check if auction expired
            if (now >= botAuction.ExpiryTime)
            {
                UnregisterBotAuction(bot, botAuction.AuctionId);
                RecordAuctionCancelled(bot->GetGUID()); // Treat expiry as cancellation
            }
        }
    }

    AuctionHouseStats BotAuctionManager::GetBotStats(ObjectGuid botGuid) const
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _botStats.find(botGuid);
        if (it != _botStats.end())
            return it->second;

        return AuctionHouseStats();
    }

    void BotAuctionManager::RecordAuctionSold(ObjectGuid botGuid, uint64 salePrice, uint64 costBasis)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto& stats = _botStats[botGuid];
        stats.TotalAuctionsSold++;
        stats.TotalGoldEarned += salePrice;

        // Account for cost basis
        if (salePrice > costBasis)
            stats.NetProfit += (salePrice - costBasis);

        stats.UpdateSuccessRate();
    }

    void BotAuctionManager::RecordAuctionCreated(ObjectGuid botGuid)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _botStats[botGuid].TotalAuctionsCreated++;
        _botStats[botGuid].UpdateSuccessRate();
    }

    void BotAuctionManager::RecordAuctionCancelled(ObjectGuid botGuid)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _botStats[botGuid].TotalAuctionsCancelled++;
        _botStats[botGuid].UpdateSuccessRate();
    }

    void BotAuctionManager::RecordCommodityPurchase(ObjectGuid botGuid, uint64 cost)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto& stats = _botStats[botGuid];
        stats.TotalCommoditiesBought++;
        stats.TotalGoldSpent += cost;
    }

    void BotAuctionManager::RecordBidPlaced(ObjectGuid botGuid, uint64 bidAmount)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto& stats = _botStats[botGuid];
        stats.TotalBidsPlaced++;
        stats.TotalGoldSpent += bidAmount; // Reserve the gold
    }

    AuctionHouseObject* BotAuctionManager::GetAuctionHouse(Player* bot, uint32 auctionHouseId)
    {
        if (!bot)
            return nullptr;

        return sAuctionMgr->GetAuctionsById(auctionHouseId);
    }

    bool BotAuctionManager::CheckThrottle(Player* bot, bool isCommodity)
    {
        if (!bot)
            return false;

        AuctionCommand cmd = isCommodity ? AuctionCommand::PlaceBid : AuctionCommand::SellItem;
        AuctionThrottleResult throttle = sAuctionMgr->CheckThrottle(bot, false, cmd);

        return !throttle.Throttled;
    }

    uint32 BotAuctionManager::GetAuctionHouseIdForBot(Player* bot)
    {
        if (!bot)
            return 0;

        // Determine faction-appropriate auction house
        // Alliance = 2, Horde = 6, Neutral = 7
        switch (bot->GetTeam())
        {
            case ALLIANCE:
                return 2;
            case HORDE:
                return 6;
            default:
                return 7;
        }
    }

    void BotAuctionManager::SavePriceHistory(uint32 itemId, uint64 price)
    {
        auto now = std::chrono::steady_clock::now();
        _priceHistory[itemId].emplace_back(now, price);

        // Keep only last 7 days (convert days to minutes: days * 1440 minutes/day)
        auto cutoff = now - Minutes(_priceHistoryDays * 1440);
        auto& history = _priceHistory[itemId];
        history.erase(
            std::remove_if(history.begin(), history.end(),
                [cutoff](const auto& entry) { return entry.first < cutoff; }),
            history.end());
    }

    void BotAuctionManager::LoadPriceHistory()
    {
        // TODO: Load from database if we implement persistent price history
        TC_LOG_DEBUG("playerbot", "BotAuctionManager::LoadPriceHistory - Price history loading skipped (not implemented)");
    }

    void BotAuctionManager::UpdatePriceData(uint32 itemId, AuctionHouseObject* ah)
    {
        if (!ah)
            return;

        std::vector<uint64> prices;

        for (auto it = ah->GetAuctionsBegin(); it != ah->GetAuctionsEnd(); ++it)
        {
            AuctionPosting const& auction = it->second;
            if (!auction.Items.empty() && auction.Items[0]->GetEntry() == itemId && auction.BuyoutOrUnitPrice > 0)
                prices.push_back(auction.BuyoutOrUnitPrice);
        }

        if (prices.empty())
            return;

        auto& priceData = _priceCache[itemId];
        priceData.ItemId = itemId;
        priceData.CurrentPrice = *std::min_element(prices.begin(), prices.end());
        priceData.ActiveListings = static_cast<uint32>(prices.size());
        priceData.LastUpdate = std::chrono::steady_clock::now();

        SavePriceHistory(itemId, priceData.CurrentPrice);
    }

    void BotAuctionManager::CalculatePriceTrends(uint32 itemId)
    {
        auto it = _priceHistory.find(itemId);
        if (it == _priceHistory.end() || it->second.size() < 2)
            return;

        auto& history = it->second;
        auto& priceData = _priceCache[itemId];

        // Calculate statistics
        std::vector<uint64> prices;
        uint64 sum = 0;
        for (const auto& [time, price] : history)
        {
            prices.push_back(price);
            sum += price;
        }

        // Average
        priceData.AveragePrice7d = sum / prices.size();

        // Median
        std::sort(prices.begin(), prices.end());
        size_t mid = prices.size() / 2;
        priceData.MedianPrice7d = prices.size() % 2 == 0 ?
            (prices[mid - 1] + prices[mid]) / 2 : prices[mid];

        // Min/Max
        priceData.MinPrice7d = prices.front();
        priceData.MaxPrice7d = prices.back();

        // Price trend (percentage change from oldest to newest)
        if (history.size() >= 2)
        {
            uint64 oldestPrice = history.front().second;
            uint64 newestPrice = history.back().second;

            if (oldestPrice > 0)
                priceData.PriceTrend = ((float(newestPrice) - float(oldestPrice)) / float(oldestPrice)) * 100.0f;
        }

        // Estimate daily volume (rough approximation)
        priceData.DailyVolume = static_cast<uint32>(priceData.ActiveListings / _priceHistoryDays);
    }

    uint32 BotAuctionManager::CalculateRiskScore(const FlipOpportunity& opportunity) const
    {
        uint32 risk = 0;

        // Higher risk if profit margin is too good to be true
        if (opportunity.ProfitMargin > 100.0f)
            risk += 30;
        else if (opportunity.ProfitMargin > 50.0f)
            risk += 15;

        // Market condition risk
        switch (opportunity.Condition)
        {
            case MarketCondition::VOLATILE:
                risk += 25;
                break;
            case MarketCondition::OVERSUPPLIED:
                risk += 15;
                break;
            case MarketCondition::UNKNOWN:
                risk += 40;
                break;
            default:
                break;
        }

        // Price data availability
        auto priceData = GetItemPriceData(opportunity.ItemId);
        if (priceData.ActiveListings < 3)
            risk += 20; // Low liquidity

        if (IsPriceHistoryStale(opportunity.ItemId))
            risk += 15; // Old data

        return std::min(risk, 100u);
    }

    bool BotAuctionManager::IsPriceHistoryStale(uint32 itemId) const
    {
        auto it = _priceCache.find(itemId);
        if (it == _priceCache.end())
            return true;

        auto age = std::chrono::steady_clock::now() - it->second.LastUpdate;
        return age > Hours(24);
    }

    bool BotAuctionManager::ValidateAuctionCreation(Player* bot, Item* item, uint64 bidPrice, uint64 buyoutPrice)
    {
        if (!bot || !item || !_enabled)
            return false;

        // Check max active auctions
        auto auctions = GetBotAuctions(bot);
        if (auctions.size() >= _maxActiveAuctions)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::ValidateAuctionCreation - Bot {} has reached max active auctions",
                bot->GetName());
            return false;
        }

        // Validate prices
        if (buyoutPrice > 0 && bidPrice > buyoutPrice)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::ValidateAuctionCreation - Bid price {} exceeds buyout {}",
                bidPrice, buyoutPrice);
            return false;
        }

        // Check item is valid for auction
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || proto->GetBonding() == BIND_ON_ACQUIRE || proto->GetBonding() == BIND_QUEST)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::ValidateAuctionCreation - Item {} is soulbound or invalid",
                item->GetEntry());
            return false;
        }

        return true;
    }

    bool BotAuctionManager::ValidateBidPlacement(Player* bot, uint32 auctionId, uint64 bidAmount)
    {
        if (!bot || !_enabled || auctionId == 0 || bidAmount == 0)
            return false;

        if (bot->GetMoney() < bidAmount)
        {
            TC_LOG_DEBUG("playerbot", "BotAuctionManager::ValidateBidPlacement - Bot {} has insufficient gold for bid",
                bot->GetName());
            return false;
        }

        return true;
    }

    uint64 BotAuctionManager::CalculateDepositCost(Player* bot, Item* item, uint32 duration)
    {
        if (!bot || !item)
            return 0;

        return sAuctionMgr->GetItemAuctionDeposit(bot, item, Minutes(duration * 60));
    }
}
