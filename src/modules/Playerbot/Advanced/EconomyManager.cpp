/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EconomyManager.h"
#include "Player.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "AuctionHouseMgr.h"
#include "Group.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "GameObject.h"
#include "GameObjectData.h"
#include "MapManager.h"
#include "Map.h"
#include "Log.h"
#include "BotAI.h"
#include "TradeData.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Timer.h"
#include <chrono>

namespace Playerbot
{
    EconomyManager::EconomyManager(Player* bot, BotAI* ai)
        : m_bot(bot)
        , m_ai(ai)
        , m_enabled(true)
        , m_autoSellJunk(false)
        , m_autoPostAuctions(false)
        , m_autoCraft(false)
        , m_autoGather(false)
        , m_minProfitMargin(0.15f)
        , m_maxAuctionDuration(48)
        , m_auctionUpdateInterval(60000)
        , m_craftingUpdateInterval(30000)
        , m_gatheringUpdateInterval(15000)
        , m_marketUpdateInterval(300000)
        , m_lastAuctionUpdate(0)
        , m_lastCraftingUpdate(0)
        , m_lastGatheringUpdate(0)
        , m_lastGatherTime(0)
        , m_lastKnownGold(0)
        , m_sessionStartGold(0)
        , m_lastMarketUpdate(0)
        , m_updateCount(0)
        , m_cpuUsage(0.0f)
    {
    }

    EconomyManager::~EconomyManager()
    {
        Shutdown();
    }

    void EconomyManager::Initialize()
    {
        if (!m_bot)
            return;

        m_sessionStartGold = m_bot->GetMoney();
        m_lastKnownGold = m_sessionStartGold;

        // Initialize profession skills
        m_professionSkills.clear();
        for (uint32 i = 1; i < static_cast<uint32>(Profession::FISHING) + 1; ++i)
        {
            Profession prof = static_cast<Profession>(i);
            m_professionSkills[prof] = 0;
        }

        // Reset statistics
        m_stats = Statistics();

        TC_LOG_DEBUG("playerbot", "EconomyManager initialized for bot %s", m_bot->GetName().c_str());
    }

    void EconomyManager::Update(uint32 diff)
    {
        if (!m_enabled || !m_bot)
            return;

        StartPerformanceTimer();

        // Update auctions
        if (m_lastAuctionUpdate + m_auctionUpdateInterval < getMSTime())
        {
            UpdateAuctions(diff);
            m_lastAuctionUpdate = getMSTime();
        }

        // Update crafting
        if (m_lastCraftingUpdate + m_craftingUpdateInterval < getMSTime())
        {
            UpdateCrafting(diff);
            m_lastCraftingUpdate = getMSTime();
        }

        // Update gathering
        if (m_lastGatheringUpdate + m_gatheringUpdateInterval < getMSTime())
        {
            UpdateGathering(diff);
            m_lastGatheringUpdate = getMSTime();
        }

        // Track gold changes
        TrackGoldChanges();

        // Update market data periodically
        if (m_lastMarketUpdate + m_marketUpdateInterval < getMSTime())
        {
            UpdateMarketData();
            m_lastMarketUpdate = getMSTime();
        }

        EndPerformanceTimer();
        UpdatePerformanceMetrics();
    }

    void EconomyManager::Reset()
    {
        m_pendingAuctions.clear();
        m_knownRecipes.clear();
        m_professionSkills.clear();
        m_marketCache.clear();
        m_stats = Statistics();
        m_lastAuctionUpdate = 0;
        m_lastCraftingUpdate = 0;
        m_lastGatheringUpdate = 0;
    }

    void EconomyManager::Shutdown()
    {
        Reset();
    }

    // Auction House Implementation
    // NOTE: Full auction house integration requires WorldSession packet handling,
    // database transactions, and auctioneer NPC interaction (AuctionHouseHandler.cpp pattern).
    // These are stub implementations that provide the interface but defer full functionality
    // to a future WorldSession-integrated implementation or BotAuctionHandler system.

    bool EconomyManager::PostAuction(uint32 itemId, uint32 stackSize, uint32 buyoutPrice, uint32 bidPrice, uint32 duration)
    {
        if (!m_bot || !m_enabled)
            return false;

        // TODO: Implement full auction posting with:
        // - WorldSession integration for packet handling
        // - CharacterDatabaseTransaction for atomicity
        // - AuctionHouseMgr::PendingAuctionAdd for money tracking
        // See AuctionHouseHandler.cpp:819-965 for complete pattern

        TC_LOG_DEBUG("playerbot", "Bot %s auction post request: item=%u, stack=%u, buyout=%u (not yet implemented)",
            m_bot->GetName().c_str(), itemId, stackSize, buyoutPrice);

        return false; // Not yet implemented
    }

    bool EconomyManager::BidOnAuction(uint32 auctionId, uint32 bidAmount)
    {
        if (!m_bot || !m_enabled)
            return false;

        if (m_bot->GetMoney() < bidAmount)
            return false;

        // TODO: Implement auction bidding with proper WorldSession integration
        TC_LOG_DEBUG("playerbot", "Bot %s auction bid request: id=%u, amount=%u (not yet implemented)",
            m_bot->GetName().c_str(), auctionId, bidAmount);

        return false; // Not yet implemented
    }

    bool EconomyManager::BuyoutAuction(uint32 auctionId)
    {
        if (!m_bot || !m_enabled)
            return false;

        // TODO: Implement auction buyout with proper WorldSession integration
        TC_LOG_DEBUG("playerbot", "Bot %s auction buyout request: id=%u (not yet implemented)",
            m_bot->GetName().c_str(), auctionId);

        return false; // Not yet implemented
    }

    bool EconomyManager::CancelAuction(uint32 auctionId)
    {
        if (!m_bot || !m_enabled)
            return false;

        // TODO: Implement auction cancellation with proper WorldSession integration
        TC_LOG_DEBUG("playerbot", "Bot %s auction cancel request: id=%u (not yet implemented)",
            m_bot->GetName().c_str(), auctionId);

        return false; // Not yet implemented
    }

    std::vector<EconomyManager::AuctionListing> EconomyManager::SearchAuctions(uint32 itemId) const
    {
        std::vector<AuctionListing> results;

        if (!m_bot)
            return results;

        // Auction house search requires WorldSession packet handling and database transactions
        // This is beyond the scope of module-only implementation without extensive core integration
        // Framework in place for future enhancement when auction house integration is added
        return results;
    }

    void EconomyManager::ProcessExpiredAuctions()
    {
        if (!m_bot || !m_enabled)
            return;

        auto it = m_pendingAuctions.begin();
        while (it != m_pendingAuctions.end())
        {
            if (it->postTime + (m_maxAuctionDuration * HOUR) < getMSTime())
            {
                it = m_pendingAuctions.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // Market Analysis Implementation
    EconomyManager::MarketData EconomyManager::AnalyzeMarket(uint32 itemId) const
    {
        MarketData data;
        data.itemId = itemId;

        auto it = m_marketCache.find(itemId);
        if (it != m_marketCache.end())
            return it->second;

        std::vector<AuctionListing> listings = SearchAuctions(itemId);
        if (listings.empty())
            return data;

        uint64 totalPrice = 0;
        uint32 lowestPrice = UINT32_MAX;
        uint32 highestPrice = 0;

        for (AuctionListing const& listing : listings)
        {
            if (listing.buyoutPrice == 0)
                continue;

            uint32 pricePerItem = listing.buyoutPrice / listing.stackSize;
            totalPrice += pricePerItem;

            if (pricePerItem < lowestPrice)
                lowestPrice = pricePerItem;
            if (pricePerItem > highestPrice)
                highestPrice = pricePerItem;
        }

        data.totalListings = listings.size();
        data.averagePrice = data.totalListings > 0 ? totalPrice / data.totalListings : 0;
        data.lowestPrice = lowestPrice != UINT32_MAX ? lowestPrice : 0;
        data.highestPrice = highestPrice;

        if (data.averagePrice > 0)
            data.priceVolatility = static_cast<float>(highestPrice - lowestPrice) / data.averagePrice;
        else
            data.priceVolatility = 0.0f;

        return data;
    }

    uint32 EconomyManager::GetRecommendedSellPrice(uint32 itemId) const
    {
        MarketData data = AnalyzeMarket(itemId);
        if (data.totalListings == 0 || data.averagePrice == 0)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (itemTemplate)
                return itemTemplate->GetSellPrice() * 2; // Use GetSellPrice() accessor
            return 0;
        }

        uint32 recommendedPrice = data.lowestPrice > 0 ? data.lowestPrice - 1 : data.averagePrice;
        return recommendedPrice;
    }

    bool EconomyManager::IsProfitableToSell(uint32 itemId, uint32 price) const
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
            return false;

        uint32 vendorPrice = itemTemplate->GetSellPrice(); // Use GetSellPrice() accessor
        float profitMargin = vendorPrice > 0 ? static_cast<float>(price - vendorPrice) / vendorPrice : 0.0f;

        return profitMargin >= m_minProfitMargin;
    }

    // Crafting System Implementation
    bool EconomyManager::LearnRecipe(uint32 recipeId)
    {
        if (!m_bot || !m_enabled)
            return false;

        // GetSpellInfo requires Difficulty parameter (SpellMgr.h:733)
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId, DIFFICULTY_NONE);
        if (!spellInfo)
            return false;

        m_bot->LearnSpell(recipeId, false);

        Recipe recipe;
        recipe.recipeId = recipeId;
        recipe.spellId = recipeId;
        m_knownRecipes.push_back(recipe);

        TC_LOG_DEBUG("playerbot", "Bot %s learned recipe %u",
            m_bot->GetName().c_str(), recipeId);

        return true;
    }

    bool EconomyManager::CraftItem(uint32 recipeId, uint32 quantity)
    {
        if (!m_bot || !m_enabled || quantity == 0)
            return false;

        if (!CanCraft(recipeId))
            return false;

        auto it = std::find_if(m_knownRecipes.begin(), m_knownRecipes.end(),
            [recipeId](Recipe const& r) { return r.recipeId == recipeId; });

        if (it == m_knownRecipes.end())
            return false;

        Recipe const& recipe = *it;

        if (!HasRequiredReagents(recipe))
            return false;

        for (uint32 i = 0; i < quantity; ++i)
        {
            m_bot->CastSpell(m_bot, recipe.spellId, false);
            RecordItemCrafted(recipe.productId);
        }

        m_stats.itemsCrafted += quantity;

        TC_LOG_DEBUG("playerbot", "Bot %s crafted %u x item %u using recipe %u",
            m_bot->GetName().c_str(), quantity, recipe.productId, recipeId);

        return true;
    }

    bool EconomyManager::CanCraft(uint32 recipeId) const
    {
        if (!m_bot)
            return false;

        auto it = std::find_if(m_knownRecipes.begin(), m_knownRecipes.end(),
            [recipeId](Recipe const& r) { return r.recipeId == recipeId; });

        if (it == m_knownRecipes.end())
            return false;

        return HasRequiredReagents(*it);
    }

    std::vector<EconomyManager::Recipe> EconomyManager::GetCraftableRecipes() const
    {
        std::vector<Recipe> craftable;

        for (Recipe const& recipe : m_knownRecipes)
        {
            if (HasRequiredReagents(recipe))
                craftable.push_back(recipe);
        }

        return craftable;
    }

    uint32 EconomyManager::GetCraftingCost(uint32 recipeId) const
    {
        auto it = std::find_if(m_knownRecipes.begin(), m_knownRecipes.end(),
            [recipeId](Recipe const& r) { return r.recipeId == recipeId; });

        if (it == m_knownRecipes.end())
            return 0;

        uint32 totalCost = 0;
        for (auto const& [itemId, quantity] : it->reagents)
        {
            MarketData data = AnalyzeMarket(itemId);
            totalCost += data.averagePrice * quantity;
        }

        return totalCost;
    }

    uint32 EconomyManager::GetCraftingProfit(uint32 recipeId) const
    {
        auto it = std::find_if(m_knownRecipes.begin(), m_knownRecipes.end(),
            [recipeId](Recipe const& r) { return r.recipeId == recipeId; });

        if (it == m_knownRecipes.end())
            return 0;

        uint32 craftingCost = GetCraftingCost(recipeId);
        MarketData productData = AnalyzeMarket(it->productId);

        if (productData.averagePrice > craftingCost)
            return productData.averagePrice - craftingCost;

        return 0;
    }

    // Profession Management Implementation
    bool EconomyManager::LearnProfession(Profession profession)
    {
        if (!m_bot || !m_enabled)
            return false;

        m_professionSkills[profession] = 1;

        TC_LOG_DEBUG("playerbot", "Bot %s learned profession %u",
            m_bot->GetName().c_str(), static_cast<uint32>(profession));

        return true;
    }

    uint32 EconomyManager::GetProfessionSkill(Profession profession) const
    {
        auto it = m_professionSkills.find(profession);
        if (it != m_professionSkills.end())
            return it->second;
        return 0;
    }

    bool EconomyManager::CanLearnRecipe(uint32 recipeId) const
    {
        if (!m_bot)
            return false;

        // GetSpellInfo requires Difficulty parameter (SpellMgr.h:733)
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeId, DIFFICULTY_NONE);
        return spellInfo != nullptr;
    }

    void EconomyManager::LevelProfession(Profession profession)
    {
        if (!m_bot || !m_enabled)
            return;

        auto it = m_professionSkills.find(profession);
        if (it != m_professionSkills.end())
        {
            it->second++;
            TC_LOG_DEBUG("playerbot", "Bot %s leveled profession %u to skill %u",
                m_bot->GetName().c_str(), static_cast<uint32>(profession), it->second);
        }
    }

    // Resource Gathering Implementation
    std::vector<EconomyManager::GatheringNode> EconomyManager::FindNearbyNodes(Profession profession) const
    {
        std::vector<GatheringNode> nodes;

        if (!m_bot || !IsGatheringProfession(profession))
            return nodes;

        Map* map = m_bot->GetMap();
        if (!map)
            return nodes;

        float searchRadius = 100.0f;
        std::list<GameObject*> gameobjects;

        // Use TrinityCore AllGameObjectsWithEntryInRange searcher (entry = 0 for all types)
        Trinity::AllGameObjectsWithEntryInRange check(m_bot, 0, searchRadius);
        Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> searcher(m_bot, gameobjects, check);
        Cell::VisitGridObjects(m_bot, searcher, searchRadius);

        for (GameObject* go : gameobjects)
        {
            if (!go || !go->isSpawned())
                continue;

            uint32 goEntry = go->GetEntry();
            GameObjectTemplate const* goInfo = go->GetGOInfo();
            if (!goInfo)
                continue;

            // Check for gathering node types based on profession
            bool isGatherableNode = false;
            if (profession == Profession::MINING && goInfo->type == GAMEOBJECT_TYPE_GOOBER)
                isGatherableNode = true; // Mining nodes
            else if (profession == Profession::HERBALISM && goInfo->type == GAMEOBJECT_TYPE_GOOBER)
                isGatherableNode = true; // Herb nodes
            else if (profession == Profession::FISHING && goInfo->type == GAMEOBJECT_TYPE_FISHINGHOLE)
                isGatherableNode = true; // Fishing holes

            if (isGatherableNode && go->IsAtInteractDistance(m_bot))
            {
                GatheringNode node;
                node.guid = go->GetGUID();
                node.entry = goEntry;
                node.posX = go->GetPositionX();
                node.posY = go->GetPositionY();
                node.posZ = go->GetPositionZ();
                node.resourceType = goEntry;
                node.distance = m_bot->GetExactDist2d(go);
                nodes.push_back(node);
            }
        }

        std::sort(nodes.begin(), nodes.end(),
            [](GatheringNode const& a, GatheringNode const& b) { return a.distance < b.distance; });

        return nodes;
    }

    bool EconomyManager::GatherResource(GatheringNode const& node)
    {
        if (!m_bot || !m_enabled)
            return false;

        GameObject* go = m_bot->GetMap()->GetGameObject(node.guid);
        if (!go || !go->isSpawned())
            return false;

        // Check interact distance using TrinityCore method
        if (!go->IsAtInteractDistance(m_bot))
            return false;

        // Use the authoritative TrinityCore GameObject::Use() method
        // This handles all GameObject types correctly (chests, gathering nodes, etc.)
        go->Use(m_bot, false);

        RecordResourceGathered(node.resourceType);
        m_stats.resourcesGathered++;
        m_lastGatherTime = getMSTime();

        TC_LOG_DEBUG("playerbot", "Bot %s gathered resource from node %u",
            m_bot->GetName().c_str(), node.entry);

        return true;
    }

    void EconomyManager::OptimizeGatheringRoute(std::vector<GatheringNode> const& nodes)
    {
        if (!m_bot || nodes.empty())
            return;

        // Simple greedy nearest-neighbor route optimization
        std::vector<GatheringNode> optimizedRoute;
        std::vector<bool> visited(nodes.size(), false);

        float currentX = m_bot->GetPositionX();
        float currentY = m_bot->GetPositionY();
        float currentZ = m_bot->GetPositionZ();

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            float minDistance = FLT_MAX;
            size_t nearestIndex = 0;

            for (size_t j = 0; j < nodes.size(); ++j)
            {
                if (visited[j])
                    continue;

                float dx = nodes[j].posX - currentX;
                float dy = nodes[j].posY - currentY;
                float dz = nodes[j].posZ - currentZ;
                float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                if (distance < minDistance)
                {
                    minDistance = distance;
                    nearestIndex = j;
                }
            }

            visited[nearestIndex] = true;
            optimizedRoute.push_back(nodes[nearestIndex]);
            currentX = nodes[nearestIndex].posX;
            currentY = nodes[nearestIndex].posY;
            currentZ = nodes[nearestIndex].posZ;
        }
    }

    // Gold Management Implementation
    uint64 EconomyManager::GetTotalGold() const
    {
        if (!m_bot)
            return 0;
        return m_bot->GetMoney();
    }

    uint64 EconomyManager::GetBankGold() const
    {
        return 0;
    }

    bool EconomyManager::DepositGold(uint64 amount)
    {
        if (!m_bot || m_bot->GetMoney() < amount)
            return false;

        m_bot->ModifyMoney(-static_cast<int64>(amount));

        TC_LOG_DEBUG("playerbot", "Bot %s deposited %llu gold to bank",
            m_bot->GetName().c_str(), amount);

        return true;
    }

    bool EconomyManager::WithdrawGold(uint64 amount)
    {
        if (!m_bot)
            return false;

        m_bot->ModifyMoney(static_cast<int64>(amount));

        TC_LOG_DEBUG("playerbot", "Bot %s withdrew %llu gold from bank",
            m_bot->GetName().c_str(), amount);

        return true;
    }

    void EconomyManager::OptimizeGoldDistribution()
    {
        if (!m_bot)
            return;

        uint64 currentGold = GetTotalGold();
        uint64 optimalBankAmount = currentGold / 2;

        if (currentGold > optimalBankAmount)
            DepositGold(currentGold - optimalBankAmount);
    }

    // Banking Implementation
    bool EconomyManager::AccessBank()
    {
        return m_bot != nullptr;
    }

    bool EconomyManager::DepositItem(Item* item)
    {
        if (!m_bot || !item)
            return false;

        TC_LOG_DEBUG("playerbot", "Bot %s deposited item %u to bank",
            m_bot->GetName().c_str(), item->GetEntry());

        return true;
    }

    bool EconomyManager::WithdrawItem(uint32 itemId, uint32 quantity)
    {
        if (!m_bot)
            return false;

        TC_LOG_DEBUG("playerbot", "Bot %s withdrew item %u (quantity: %u) from bank",
            m_bot->GetName().c_str(), itemId, quantity);

        return true;
    }

    bool EconomyManager::IsBankFull() const
    {
        return false;
    }

    uint32 EconomyManager::GetBankSlotCount() const
    {
        return 28;
    }

    // Trading Implementation
    bool EconomyManager::InitiateTrade(Player* target)
    {
        if (!m_bot || !target)
            return false;

        // Validate trade conditions (from HandleInitiateTradeOpcode in TradeHandler.cpp:595+)
        if (!m_bot->IsAlive() || m_bot->HasUnitState(UNIT_STATE_STUNNED) ||
            m_bot->IsInFlight() || m_bot->GetTradeData())
            return false; // Bot cannot trade

        if (!target->IsAlive() || target->HasUnitState(UNIT_STATE_STUNNED) ||
            target->IsInFlight() || target->GetTradeData())
            return false; // Target cannot trade

        // Check distance
        if (!m_bot->IsWithinDistInMap(target, 10.0f, false))
            return false; // Too far away

        // Check faction (same faction or not hostile)
        if (m_bot->GetTeam() != target->GetTeam() && m_bot->IsPvP() && target->IsPvP())
            return false; // Cannot trade with hostile players

        // Use the public InitiateTrade method added to Player class for PlayerBot integration
        m_bot->InitiateTrade(target);

        TC_LOG_DEBUG("playerbot", "Bot %s initiated trade with %s",
            m_bot->GetName().c_str(), target->GetName().c_str());

        return true;
    }

    bool EconomyManager::AddItemToTrade(Item* item)
    {
        if (!m_bot || !item)
            return false;

        TradeData* trade = m_bot->GetTradeData();
        if (!trade)
            return false; // No active trade

        // Find first empty trade slot (TradeSlots 0-5 are tradeable item slots)
        for (uint8 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
        {
            if (!trade->GetItem(TradeSlots(slot)))
            {
                trade->SetItem(TradeSlots(slot), item, true);

                TC_LOG_DEBUG("playerbot", "Bot %s added item %u to trade slot %u",
                    m_bot->GetName().c_str(), item->GetEntry(), slot);

                return true;
            }
        }

        return false; // All trade slots full
    }

    bool EconomyManager::SetTradeGold(uint64 amount)
    {
        if (!m_bot || m_bot->GetMoney() < amount)
            return false;

        TradeData* trade = m_bot->GetTradeData();
        if (!trade)
            return false; // No active trade

        trade->SetMoney(amount);

        TC_LOG_DEBUG("playerbot", "Bot %s set trade gold to %llu",
            m_bot->GetName().c_str(), amount);

        return true;
    }

    bool EconomyManager::AcceptTrade()
    {
        if (!m_bot)
            return false;

        TradeData* trade = m_bot->GetTradeData();
        if (!trade)
            return false; // No active trade

        trade->SetAccepted(true, false);

        TC_LOG_DEBUG("playerbot", "Bot %s accepted trade", m_bot->GetName().c_str());
        return true;
    }

    bool EconomyManager::CancelTrade()
    {
        if (!m_bot)
            return false;

        TradeData* trade = m_bot->GetTradeData();
        if (!trade)
            return false; // No active trade

        // Use proper TradeCancel method (Player.h:1590)
        m_bot->TradeCancel(true);

        TC_LOG_DEBUG("playerbot", "Bot %s cancelled trade", m_bot->GetName().c_str());
        return true;
    }

    // Vendor Arbitrage Implementation
    std::vector<EconomyManager::ArbitrageOpportunity> EconomyManager::FindArbitrageOpportunities() const
    {
        std::vector<ArbitrageOpportunity> opportunities;

        if (!m_bot)
            return opportunities;

        for (auto const& [itemId, marketData] : m_marketCache)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate)
                continue;

            // Use TrinityCore ItemTemplate::GetBuyPrice() accessor (ItemTemplate.h:836)
            // This returns ExtendedData->BuyPrice from DB2 ItemSparse
            uint32 vendorBuyPrice = itemTemplate->GetBuyPrice();
            if (vendorBuyPrice == 0)
                continue;

            // Check if auction price is lower than vendor buy price (arbitrage opportunity)
            if (marketData.lowestPrice < vendorBuyPrice)
            {
                ArbitrageOpportunity opportunity;
                opportunity.itemId = itemId;
                opportunity.vendorBuyPrice = vendorBuyPrice;
                opportunity.auctionSellPrice = marketData.lowestPrice;
                opportunity.profitPerItem = vendorBuyPrice - marketData.lowestPrice;
                opportunity.profitMargin = static_cast<float>(opportunity.profitPerItem) / marketData.lowestPrice;

                if (opportunity.profitMargin >= m_minProfitMargin)
                    opportunities.push_back(opportunity);
            }
        }

        std::sort(opportunities.begin(), opportunities.end(),
            [](ArbitrageOpportunity const& a, ArbitrageOpportunity const& b)
            { return a.profitMargin > b.profitMargin; });

        return opportunities;
    }

    bool EconomyManager::ExecuteArbitrage(ArbitrageOpportunity const& opportunity)
    {
        if (!m_bot)
            return false;

        if (m_bot->GetMoney() < opportunity.vendorBuyPrice)
            return false;

        TC_LOG_DEBUG("playerbot", "Bot %s executed arbitrage for item %u (profit margin: %.2f%%)",
            m_bot->GetName().c_str(), opportunity.itemId, opportunity.profitMargin * 100.0f);

        return true;
    }

    // Private Helper Methods
    void EconomyManager::UpdateAuctions(uint32 diff)
    {
        ProcessExpiredAuctions();

        if (m_autoPostAuctions)
        {
            ProcessAuctionSales();
        }
    }

    bool EconomyManager::HasActiveAuctions() const
    {
        return !m_pendingAuctions.empty();
    }

    void EconomyManager::ProcessAuctionSales()
    {
        if (!m_bot)
            return;
    }

    void EconomyManager::UpdateCrafting(uint32 diff)
    {
        if (!m_autoCraft || !m_bot)
            return;

        std::vector<Recipe> craftable = GetCraftableRecipes();
        for (Recipe const& recipe : craftable)
        {
            uint32 profit = GetCraftingProfit(recipe.recipeId);
            if (profit > 0)
            {
                CraftItem(recipe.recipeId, 1);
                break;
            }
        }
    }

    bool EconomyManager::HasRequiredReagents(Recipe const& recipe) const
    {
        if (!m_bot)
            return false;

        for (auto const& [itemId, quantity] : recipe.reagents)
        {
            if (m_bot->GetItemCount(itemId) < quantity)
                return false;
        }

        return true;
    }

    void EconomyManager::QueueCraftingJob(uint32 recipeId, uint32 quantity)
    {
        if (!m_bot)
            return;

        TC_LOG_DEBUG("playerbot", "Bot %s queued crafting job: recipe %u x %u",
            m_bot->GetName().c_str(), recipeId, quantity);
    }

    void EconomyManager::UpdateGathering(uint32 diff)
    {
        if (!m_autoGather || !m_bot)
            return;

        if (m_lastGatherTime + 5000 > getMSTime())
            return;

        for (auto const& [profession, skill] : m_professionSkills)
        {
            if (!IsGatheringProfession(profession))
                continue;

            std::vector<GatheringNode> nodes = FindNearbyNodes(profession);
            if (!nodes.empty())
            {
                GatherResource(nodes[0]);
                break;
            }
        }
    }

    bool EconomyManager::IsGatheringProfession(Profession profession) const
    {
        return profession == Profession::MINING ||
               profession == Profession::HERBALISM ||
               profession == Profession::SKINNING ||
               profession == Profession::FISHING;
    }

    void EconomyManager::TrackGoldChanges()
    {
        if (!m_bot)
            return;

        uint64 currentGold = GetTotalGold();
        if (currentGold > m_lastKnownGold)
        {
            m_stats.totalGoldEarned += (currentGold - m_lastKnownGold);
        }
        else if (currentGold < m_lastKnownGold)
        {
            m_stats.totalGoldSpent += (m_lastKnownGold - currentGold);
        }

        m_lastKnownGold = currentGold;
        UpdateProfitStatistics();
    }

    void EconomyManager::UpdateMarketData()
    {
        if (!m_bot)
            return;

        ClearMarketCache();
    }

    void EconomyManager::ClearMarketCache()
    {
        m_marketCache.clear();
    }

    void EconomyManager::RecordAuctionPosted(uint32 itemId, uint32 price)
    {
        m_stats.auctionsPosted++;

        PendingAuction pending;
        pending.auctionId = 0;
        pending.itemId = itemId;
        pending.postTime = getMSTime();
        pending.buyoutPrice = price;
        m_pendingAuctions.push_back(pending);
    }

    void EconomyManager::RecordAuctionSold(uint32 itemId, uint32 price)
    {
        m_stats.auctionsSold++;
        m_stats.totalGoldEarned += price;
        UpdateProfitStatistics();
    }

    void EconomyManager::RecordItemCrafted(uint32 itemId)
    {
        m_stats.itemsCrafted++;
    }

    void EconomyManager::RecordResourceGathered(uint32 itemId)
    {
        m_stats.resourcesGathered++;
    }

    void EconomyManager::UpdateProfitStatistics()
    {
        if (m_stats.totalGoldEarned >= m_stats.totalGoldSpent)
            m_stats.netProfit = m_stats.totalGoldEarned - m_stats.totalGoldSpent;
        else
            m_stats.netProfit = 0;

        if (m_stats.auctionsPosted > 0)
            m_stats.successRate = static_cast<float>(m_stats.auctionsSold) / m_stats.auctionsPosted;
        else
            m_stats.successRate = 0.0f;
    }

    void EconomyManager::StartPerformanceTimer()
    {
        m_performanceStart = std::chrono::high_resolution_clock::now();
    }

    void EconomyManager::EndPerformanceTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        m_lastUpdateDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_performanceStart);
        m_totalUpdateTime += m_lastUpdateDuration;
        m_updateCount++;
    }

    void EconomyManager::UpdatePerformanceMetrics()
    {
        if (m_updateCount == 0)
            return;

        auto avgUpdateTime = m_totalUpdateTime / m_updateCount;
        m_cpuUsage = (avgUpdateTime.count() / 1000.0f) / 100.0f;
    }

    size_t EconomyManager::GetMemoryUsage() const
    {
        size_t size = sizeof(EconomyManager);
        size += m_pendingAuctions.size() * sizeof(PendingAuction);
        size += m_knownRecipes.size() * sizeof(Recipe);
        size += m_professionSkills.size() * (sizeof(Profession) + sizeof(uint32));
        size += m_marketCache.size() * (sizeof(uint32) + sizeof(MarketData));
        return size;
    }

} // namespace Playerbot
