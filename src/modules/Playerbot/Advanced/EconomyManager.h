/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_BOT_ECONOMY_MANAGER_H
#define TRINITYCORE_BOT_ECONOMY_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <chrono>

class Player;
class Item;
class ItemTemplate;

namespace Playerbot
{
    class BotAI;

    /**
     * EconomyManager - Economic system participation for PlayerBots
     *
     * Handles all economic activities including:
     * - Auction house bidding and selling
     * - Crafting profession integration
     * - Resource gathering automation
     * - Gold management and banking
     */
    class EconomyManager
    {
    public:
        explicit EconomyManager(Player* bot, BotAI* ai);
        ~EconomyManager();

        // Core lifecycle
        void Initialize();
        void Update(uint32 diff);
        void Reset();
        void Shutdown();

        // Auction House
        struct AuctionListing
        {
            uint32 auctionId;
            uint32 itemId;
            uint32 stackSize;
            uint32 buyoutPrice;
            uint32 bidPrice;
            uint32 timeLeft;
            ObjectGuid seller;
        };

        bool PostAuction(uint32 itemId, uint32 stackSize, uint32 buyoutPrice, uint32 bidPrice, uint32 duration);
        bool BidOnAuction(uint32 auctionId, uint32 bidAmount);
        bool BuyoutAuction(uint32 auctionId);
        bool CancelAuction(uint32 auctionId);
        std::vector<AuctionListing> SearchAuctions(uint32 itemId) const;
        void ProcessExpiredAuctions();

        // Market Analysis
        struct MarketData
        {
            uint32 itemId;
            uint32 averagePrice;
            uint32 lowestPrice;
            uint32 highestPrice;
            uint32 totalListings;
            float priceVolatility;
        };

        MarketData AnalyzeMarket(uint32 itemId) const;
        uint32 GetRecommendedSellPrice(uint32 itemId) const;
        bool IsProfitableToSell(uint32 itemId, uint32 price) const;

        // Crafting System
        enum class Profession : uint8
        {
            NONE,
            ALCHEMY,
            BLACKSMITHING,
            ENCHANTING,
            ENGINEERING,
            INSCRIPTION,
            JEWELCRAFTING,
            LEATHERWORKING,
            TAILORING,
            SKINNING,
            MINING,
            HERBALISM,
            COOKING,
            FISHING
        };

        struct Recipe
        {
            uint32 recipeId;
            uint32 spellId;
            Profession profession;
            uint32 requiredSkill;
            std::vector<std::pair<uint32, uint32>> reagents; // itemId, quantity
            uint32 productId;
            uint32 productCount;
        };

        bool LearnRecipe(uint32 recipeId);
        bool CraftItem(uint32 recipeId, uint32 quantity = 1);
        bool CanCraft(uint32 recipeId) const;
        std::vector<Recipe> GetCraftableRecipes() const;
        uint32 GetCraftingCost(uint32 recipeId) const;
        uint32 GetCraftingProfit(uint32 recipeId) const;

        // Profession Management
        bool LearnProfession(Profession profession);
        uint32 GetProfessionSkill(Profession profession) const;
        bool CanLearnRecipe(uint32 recipeId) const;
        void LevelProfession(Profession profession);

        // Resource Gathering
        struct GatheringNode
        {
            ObjectGuid guid;
            uint32 entry;
            float posX;
            float posY;
            float posZ;
            uint32 resourceType;
            float distance;
        };

        std::vector<GatheringNode> FindNearbyNodes(Profession profession) const;
        bool GatherResource(GatheringNode const& node);
        void OptimizeGatheringRoute(std::vector<GatheringNode> const& nodes);

        // Gold Management
        uint64 GetTotalGold() const;
        uint64 GetBankGold() const;
        bool DepositGold(uint64 amount);
        bool WithdrawGold(uint64 amount);
        void OptimizeGoldDistribution();

        // Banking
        bool AccessBank();
        bool DepositItem(Item* item);
        bool WithdrawItem(uint32 itemId, uint32 quantity);
        bool IsBankFull() const;
        uint32 GetBankSlotCount() const;

        // Trading
        bool InitiateTrade(Player* target);
        bool AddItemToTrade(Item* item);
        bool SetTradeGold(uint64 amount);
        bool AcceptTrade();
        bool CancelTrade();

        // Vendor Arbitrage
        struct ArbitrageOpportunity
        {
            uint32 itemId;
            uint32 vendorBuyPrice;
            uint32 auctionSellPrice;
            uint32 profitPerItem;
            float profitMargin;
        };

        std::vector<ArbitrageOpportunity> FindArbitrageOpportunities() const;
        bool ExecuteArbitrage(ArbitrageOpportunity const& opportunity);

        // Configuration
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool enabled) { m_enabled = enabled; }

        void SetAutoSellJunk(bool enable) { m_autoSellJunk = enable; }
        void SetAutoPostAuctions(bool enable) { m_autoPostAuctions = enable; }
        void SetAutoCraft(bool enable) { m_autoCraft = enable; }
        void SetAutoGather(bool enable) { m_autoGather = enable; }

        void SetMinProfitMargin(float margin) { m_minProfitMargin = margin; }
        void SetMaxAuctionDuration(uint32 hours) { m_maxAuctionDuration = hours; }

        // Statistics
        struct Statistics
        {
            uint32 auctionsPosted = 0;
            uint32 auctionsSold = 0;
            uint32 itemsCrafted = 0;
            uint32 resourcesGathered = 0;
            uint64 totalGoldEarned = 0;
            uint64 totalGoldSpent = 0;
            uint64 netProfit = 0;
            float successRate = 0.0f;
        };

        Statistics const& GetStatistics() const { return m_stats; }

        // Performance metrics
        float GetCPUUsage() const { return m_cpuUsage; }
        size_t GetMemoryUsage() const;

    private:
        // Auction house logic
        struct PendingAuction
        {
            uint32 auctionId;
            uint32 itemId;
            uint32 postTime;
            uint32 buyoutPrice;
        };

        std::vector<PendingAuction> m_pendingAuctions;
        void UpdateAuctions(uint32 diff);
        bool HasActiveAuctions() const;
        void ProcessAuctionSales();

        // Crafting logic
        std::unordered_map<Profession, uint32> m_professionSkills;
        std::vector<Recipe> m_knownRecipes;
        void UpdateCrafting(uint32 diff);
        bool HasRequiredReagents(Recipe const& recipe) const;
        void QueueCraftingJob(uint32 recipeId, uint32 quantity);

        // Gathering logic
        uint32 m_lastGatherTime;
        void UpdateGathering(uint32 diff);
        bool IsGatheringProfession(Profession profession) const;

        // Gold tracking
        uint64 m_lastKnownGold;
        uint64 m_sessionStartGold;
        void TrackGoldChanges();

        // Market analysis
        std::unordered_map<uint32, MarketData> m_marketCache;
        uint32 m_lastMarketUpdate;
        void UpdateMarketData();
        void ClearMarketCache();

        // Statistics tracking
        void RecordAuctionPosted(uint32 itemId, uint32 price);
        void RecordAuctionSold(uint32 itemId, uint32 price);
        void RecordItemCrafted(uint32 itemId);
        void RecordResourceGathered(uint32 itemId);
        void UpdateProfitStatistics();

        // Performance tracking
        void StartPerformanceTimer();
        void EndPerformanceTimer();
        void UpdatePerformanceMetrics();

    private:
        Player* m_bot;
        BotAI* m_ai;
        bool m_enabled;

        // Configuration
        bool m_autoSellJunk;
        bool m_autoPostAuctions;
        bool m_autoCraft;
        bool m_autoGather;
        float m_minProfitMargin;
        uint32 m_maxAuctionDuration;

        // Update intervals
        uint32 m_auctionUpdateInterval;
        uint32 m_craftingUpdateInterval;
        uint32 m_gatheringUpdateInterval;
        uint32 m_marketUpdateInterval;

        // Last update times
        uint32 m_lastAuctionUpdate;
        uint32 m_lastCraftingUpdate;
        uint32 m_lastGatheringUpdate;

        // Statistics
        Statistics m_stats;

        // Performance metrics
        std::chrono::high_resolution_clock::time_point m_performanceStart;
        std::chrono::microseconds m_lastUpdateDuration;
        std::chrono::microseconds m_totalUpdateTime;
        uint32 m_updateCount;
        float m_cpuUsage;
    };

} // namespace Playerbot

#endif // TRINITYCORE_BOT_ECONOMY_MANAGER_H
