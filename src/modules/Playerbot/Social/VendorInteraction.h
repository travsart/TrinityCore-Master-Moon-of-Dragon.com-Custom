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
#include "Player.h"
#include "Creature.h"
#include "CreatureData.h"
#include "ObjectMgr.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>

namespace Playerbot
{

/**
 * @brief Advanced vendor interaction system leveraging TrinityCore's creature database
 *
 * This system directly integrates with TrinityCore's creature_template, npc_vendor,
 * and gossip systems to provide intelligent vendor interactions for playerbots.
 */
class TC_GAME_API VendorInteraction
{
public:
    static VendorInteraction* instance();

    // Core vendor discovery using TrinityCore data
    void LoadVendorDataFromDatabase();
    std::vector<VendorInfo> QueryVendorsByZone(uint32 zoneId);
    std::vector<VendorInfo> QueryVendorsByType(VendorType type);
    VendorInfo GetVendorFromCreature(const Creature* creature);

    // Intelligent vendor selection
    uint32 FindOptimalVendor(Player* player, VendorType preferredType, float maxDistance = 200.0f);
    std::vector<uint32> FindVendorsWithItem(uint32 itemId, uint32 playerZone);
    uint32 FindCheapestVendor(uint32 itemId, const std::vector<uint32>& vendorGuids);
    uint32 FindNearestRepairVendor(Player* player);

    // Vendor interaction optimization
    void OptimizeVendorRoute(Player* player, const std::vector<std::pair<VendorType, uint32>>& needs);
    void PlanVendorTrip(Player* player, const std::vector<uint32>& itemsToBuy, const std::vector<uint32>& itemsToSell);
    bool ShouldTravelToVendor(Player* player, uint32 vendorGuid, float expectedValue);

    // Advanced vendor analysis using TrinityCore vendor data
    struct VendorAnalysis
    {
        uint32 vendorGuid;
        VendorType type;
        std::vector<uint32> availableItems;
        std::vector<std::pair<uint32, uint32>> itemPrices; // itemId, price
        std::unordered_map<uint32, uint32> stockLevels; // itemId, stock count
        float averagePrice;
        float priceModifier;
        uint32 factionRequirement;
        uint32 reputationRequirement;
        bool isAlwaysAvailable;
        uint32 respawnTimer;

        VendorAnalysis(uint32 guid) : vendorGuid(guid), type(VendorType::GENERAL_GOODS)
            , averagePrice(0.0f), priceModifier(1.0f), factionRequirement(0)
            , reputationRequirement(0), isAlwaysAvailable(true), respawnTimer(0) {}
    };

    VendorAnalysis AnalyzeVendor(uint32 vendorGuid);
    void UpdateVendorAnalysis(uint32 vendorGuid);
    bool CanPlayerUseVendor(Player* player, uint32 vendorGuid);

    // Dynamic vendor inventory management
    void TrackVendorInventory(uint32 vendorGuid);
    void UpdateVendorStock(uint32 vendorGuid, uint32 itemId, int32 stockChange);
    uint32 GetVendorStock(uint32 vendorGuid, uint32 itemId);
    void PredictVendorRestocking(uint32 vendorGuid);

    // Automated buying strategies
    struct BuyingStrategy
    {
        std::vector<uint32> priorityItems;
        std::vector<uint32> consumableItems;
        std::vector<uint32> reagentItems;
        std::unordered_map<uint32, uint32> maxQuantities; // itemId -> max to buy
        std::unordered_map<uint32, uint32> stockThresholds; // itemId -> buy when below threshold
        uint32 maxSpendingBudget;
        bool buyBestAvailable;
        bool considerItemLevel;
        float priceThreshold; // Don't buy if price > threshold

        BuyingStrategy() : maxSpendingBudget(10000), buyBestAvailable(true)
            , considerItemLevel(true), priceThreshold(1.5f) {}
    };

    void ExecuteBuyingStrategy(Player* player, uint32 vendorGuid, const BuyingStrategy& strategy);
    void AutoBuyConsumables(Player* player, uint32 vendorGuid);
    void AutoBuyReagents(Player* player, uint32 vendorGuid);
    void BuyBestAvailableGear(Player* player, uint32 vendorGuid);

    // Automated selling strategies
    struct SellingStrategy
    {
        std::vector<uint32> junkItemTypes;
        std::vector<uint32> whiteItems;
        std::vector<uint32> greyItems;
        std::unordered_set<uint32> keepItems; // Never sell these
        uint32 minItemLevel; // Don't sell items above this level
        uint32 minItemValue; // Don't sell items below this value
        bool sellDuplicates;
        bool sellOutdatedGear;
        bool keepSetItems;

        SellingStrategy() : minItemLevel(0), minItemValue(1), sellDuplicates(true)
            , sellOutdatedGear(true), keepSetItems(true) {}
    };

    void ExecuteSellingStrategy(Player* player, uint32 vendorGuid, const SellingStrategy& strategy);
    void AutoSellJunkItems(Player* player, uint32 vendorGuid);
    void SellOutdatedEquipment(Player* player, uint32 vendorGuid);
    uint32 CalculateSellingValue(Player* player, const std::vector<uint32>& itemGuids);

    // Reputation and faction vendor handling
    void HandleFactionVendors(Player* player);
    std::vector<uint32> GetAccessibleFactionVendors(Player* player);
    bool MeetsReputationRequirement(Player* player, uint32 vendorGuid);
    void OptimizeReputationGains(Player* player);

    // Vendor service coordination
    void CoordinateRepairServices(Player* player);
    void HandleInnkeeperServices(Player* player, uint32 innkeeperGuid);
    void ManageFlightPathServices(Player* player, uint32 flightMasterGuid);
    void ProcessTrainerServices(Player* player, uint32 trainerGuid);

    // Performance monitoring
    struct VendorMetrics
    {
        std::atomic<uint32> vendorInteractions{0};
        std::atomic<uint32> itemsPurchased{0};
        std::atomic<uint32> itemsSold{0};
        std::atomic<uint32> repairTransactions{0};
        std::atomic<uint32> totalGoldSpent{0};
        std::atomic<uint32> totalGoldEarned{0};
        std::atomic<float> averageTransactionValue{100.0f};
        std::atomic<float> vendorEfficiency{0.9f};

        void Reset() {
            vendorInteractions = 0; itemsPurchased = 0; itemsSold = 0;
            repairTransactions = 0; totalGoldSpent = 0; totalGoldEarned = 0;
            averageTransactionValue = 100.0f; vendorEfficiency = 0.9f;
        }
    };

    VendorMetrics GetPlayerVendorMetrics(uint32 playerGuid);
    VendorMetrics GetGlobalVendorMetrics();

    // Configuration and optimization
    void SetBuyingStrategy(uint32 playerGuid, const BuyingStrategy& strategy);
    void SetSellingStrategy(uint32 playerGuid, const SellingStrategy& strategy);
    BuyingStrategy GetBuyingStrategy(uint32 playerGuid);
    SellingStrategy GetSellingStrategy(uint32 playerGuid);

    // Update and maintenance
    void Update(uint32 diff);
    void RefreshVendorDatabase();
    void ValidateVendorData();

private:
    VendorInteraction();
    ~VendorInteraction() = default;

    // Vendor database populated from TrinityCore
    std::unordered_map<uint32, VendorAnalysis> _vendorAnalysisCache; // creatureGuid -> analysis
    std::unordered_map<uint32, std::vector<uint32>> _zoneVendorCache; // zoneId -> vendorGuids
    std::unordered_map<VendorType, std::vector<uint32>> _typeVendorCache; // type -> vendorGuids
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _vendorInventoryCache; // vendorGuid -> itemId -> stock
    mutable std::mutex _vendorCacheMutex;

    // Player strategies
    std::unordered_map<uint32, BuyingStrategy> _playerBuyingStrategies; // playerGuid -> strategy
    std::unordered_map<uint32, SellingStrategy> _playerSellingStrategies; // playerGuid -> strategy
    std::unordered_map<uint32, VendorMetrics> _playerMetrics; // playerGuid -> metrics

    // Performance tracking
    VendorMetrics _globalMetrics;

    // TrinityCore integration functions
    void LoadVendorsFromCreatureTemplate();
    void LoadVendorInventoryFromNpcVendor();
    void LoadGossipOptionsFromDatabase();
    void LoadFactionVendorRequirements();

    // Vendor analysis helpers
    VendorType DetermineVendorTypeFromFlags(uint32 npcFlags);
    void AnalyzeVendorInventory(uint32 vendorGuid, VendorAnalysis& analysis);
    void CalculateVendorPricing(uint32 vendorGuid, VendorAnalysis& analysis);
    bool ValidateVendorAccessibility(Player* player, uint32 vendorGuid);

    // Strategy execution helpers
    void ExecutePurchaseTransaction(Player* player, uint32 vendorGuid, uint32 itemId, uint32 count);
    void ExecuteSellTransaction(Player* player, uint32 vendorGuid, uint32 itemGuid, uint32 count);
    bool ShouldBuyItem(Player* player, uint32 itemId, const BuyingStrategy& strategy);
    bool ShouldSellItem(Player* player, uint32 itemGuid, const SellingStrategy& strategy);

    // Navigation and pathfinding
    bool NavigateToVendor(Player* player, uint32 vendorGuid);
    float CalculateVendorDistance(Player* player, uint32 vendorGuid);
    void OptimizeVendorVisitOrder(Player* player, std::vector<uint32>& vendorGuids);

    // Price analysis and optimization
    void AnalyzeMarketPrices(uint32 itemId);
    float GetBestAvailablePrice(uint32 itemId, const std::vector<uint32>& vendorGuids);
    void UpdatePriceHistory(uint32 vendorGuid, uint32 itemId, uint32 price);

    // Performance optimization
    void CacheFrequentVendorData();
    void PreloadVendorInventories(Player* player);
    void OptimizeVendorQueries();
    void UpdateVendorMetrics(uint32 playerGuid, uint32 vendorGuid, uint32 transactionValue, bool wasPurchase);

    // Constants using TrinityCore systems
    static constexpr uint32 VENDOR_CACHE_REFRESH_INTERVAL = 300000; // 5 minutes
    static constexpr float MAX_VENDOR_DISTANCE = 300.0f; // 300 yards
    static constexpr uint32 INVENTORY_UPDATE_INTERVAL = 60000; // 1 minute
    static constexpr uint32 MAX_BUYING_BUDGET = 100000; // 100 gold
    static constexpr float PRICE_TOLERANCE = 1.2f; // 20% price variation acceptable
    static constexpr uint32 MIN_STOCK_FOR_PURCHASE = 1;
    static constexpr uint32 VENDOR_INTERACTION_TIMEOUT = 30000; // 30 seconds
    static constexpr float VENDOR_EFFICIENCY_THRESHOLD = 0.8f;
};

} // namespace Playerbot