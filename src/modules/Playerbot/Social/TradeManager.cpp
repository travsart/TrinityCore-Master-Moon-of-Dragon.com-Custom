/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "TradeManager.h"
#include "BotAI.h"
#include "Player.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ItemDefines.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "TradeData.h"
#include "Group.h"
#include "Guild.h"
#include "Chat.h"
#include "Log.h"
#include "World.h"
#include "SpellMgr.h"
#include "DB2Stores.h"
#include "Bag.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <algorithm>
#include <numeric>

namespace Playerbot
{
    // TradeSession implementation
    void TradeSession::Reset()
    {
        traderGuid.Clear();
        state = TradeState::IDLE;
        offeredGold = 0;
        receivedGold = 0;
        offeredItems.clear();
        receivedItems.clear();
        updateCount = 0;
        isAccepted = false;
        traderAccepted = false;
    }

    uint64 TradeSession::GetTotalOfferedValue() const
    {
        uint64 totalValue = offeredGold;
        for (auto const& slot : offeredItems)
            totalValue += slot.estimatedValue * slot.itemCount;
        return totalValue;
    }

    uint64 TradeSession::GetTotalReceivedValue() const
    {
        uint64 totalValue = receivedGold;
        for (auto const& slot : receivedItems)
            totalValue += slot.estimatedValue * slot.itemCount;
        return totalValue;
    }

    bool TradeSession::IsBalanced(float tolerance) const
    {
        uint64 offeredValue = GetTotalOfferedValue();
        uint64 receivedValue = GetTotalReceivedValue();

        if (offeredValue == 0 && receivedValue == 0)
            return true;

        if (offeredValue == 0 || receivedValue == 0)
            return false;

        float ratio = static_cast<float>(std::min(offeredValue, receivedValue)) /
                     static_cast<float>(std::max(offeredValue, receivedValue));

        return ratio >= (1.0f - tolerance);
    }

    // TradeStatistics implementation
    float TradeStatistics::GetSuccessRate() const
    {
        if (totalTrades == 0)
            return 0.0f;
        return static_cast<float>(successfulTrades) / static_cast<float>(totalTrades);
    }

    std::chrono::milliseconds TradeStatistics::GetAverageTradeTime() const
    {
        if (successfulTrades == 0)
            return std::chrono::milliseconds(0);
        return totalTradeTime / successfulTrades;
    }

    // BotTradeManager implementation
    BotTradeManager::BotTradeManager(BotAI* botAI) :
        m_botAI(botAI),
        m_bot(botAI ? botAI->GetBot() : nullptr),
        m_securityLevel(TradeSecurity::STANDARD),
        m_autoAcceptGroup(true),
        m_autoAcceptGuild(false),
        m_autoAcceptWhitelist(true),
        m_maxTradeValue(10000 * GOLD),
        m_maxTradeDistance(TRADE_DISTANCE),
        m_updateTimer(0)
    {
        m_lastUpdateTime = std::chrono::steady_clock::now();
    }

    BotTradeManager::~BotTradeManager()
    {
        if (IsTrading())
            CancelTrade("Bot shutting down");
    }

    bool BotTradeManager::InitiateTrade(Player* target, std::string const& reason)
    {
        if (!target)
            return false;

        return InitiateTrade(target->GetGUID(), reason);
    }

    bool BotTradeManager::InitiateTrade(ObjectGuid targetGuid, std::string const& reason)
    {
        if (!m_bot || !targetGuid)
            return false;

        // Check if already trading
        if (IsTrading())
        {
            LogTradeAction("INITIATE_FAILED", "Already in trade");
            return false;
        }

        // Find target player
        Player* target = ObjectAccessor::FindPlayer(targetGuid);
        if (!target)
        {
            LogTradeAction("INITIATE_FAILED", "Target not found");
            return false;
        }

        // Validate target
        if (!ValidateTradeTarget(target))
        {
            LogTradeAction("INITIATE_FAILED", "Target validation failed");
            return false;
        }

        // Check distance
        if (!ValidateTradeDistance(target))
        {
            LogTradeAction("INITIATE_FAILED", "Target too far away");
            return false;
        }

        // Check if target is already trading
        if (target->GetTradeData())
        {
            LogTradeAction("INITIATE_FAILED", "Target already trading");
            return false;
        }

        // Initialize trade session
        ResetTradeSession();
        m_currentSession.traderGuid = targetGuid;
        m_currentSession.startTime = std::chrono::steady_clock::now();
        SetTradeState(TradeState::INITIATING);

        // Create trade data
        TradeData* myTrade = new TradeData(m_bot, target);
        TradeData* theirTrade = new TradeData(target, m_bot);

        m_bot->SetTradeData(myTrade);
        target->SetTradeData(theirTrade);

        // Send trade status
        WorldPacket data(SMSG_TRADE_STATUS, 12);
        data << uint32(TRADE_STATUS_BEGIN_TRADE);
        data << targetGuid;
        m_bot->GetSession()->SendPacket(&data);
        target->GetSession()->SendPacket(&data);

        LogTradeAction("INITIATE", "Trade initiated with " + target->GetName() + " - " + reason);
        return true;
    }

    bool BotTradeManager::AcceptTradeRequest(ObjectGuid requesterGuid)
    {
        if (!m_bot || !requesterGuid)
            return false;

        // Check if already trading
        if (IsTrading())
        {
            LogTradeAction("ACCEPT_REQUEST_FAILED", "Already in trade");
            return false;
        }

        // Find requester
        Player* requester = ObjectAccessor::FindPlayer(requesterGuid);
        if (!requester)
        {
            LogTradeAction("ACCEPT_REQUEST_FAILED", "Requester not found");
            return false;
        }

        // Check for pending request
        auto it = m_pendingRequests.find(requesterGuid);
        if (it == m_pendingRequests.end())
        {
            LogTradeAction("ACCEPT_REQUEST_FAILED", "No pending request from player");
            return false;
        }

        // Check request timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.requestTime).count();

        if (elapsed > TRADE_REQUEST_TIMEOUT)
        {
            m_pendingRequests.erase(it);
            LogTradeAction("ACCEPT_REQUEST_FAILED", "Request timed out");
            return false;
        }

        // Accept the trade request
        m_pendingRequests.erase(it);
        return InitiateTrade(requester, "Accepting trade request");
    }

    bool BotTradeManager::DeclineTradeRequest(ObjectGuid requesterGuid)
    {
        auto it = m_pendingRequests.find(requesterGuid);
        if (it != m_pendingRequests.end())
        {
            m_pendingRequests.erase(it);
            LogTradeAction("DECLINE_REQUEST", "Declined trade from " + requesterGuid.ToString());
            return true;
        }
        return false;
    }

    void BotTradeManager::CancelTrade(std::string const& reason)
    {
        if (!IsTrading())
            return;

        ProcessTradeCancellation(reason);
    }

    bool BotTradeManager::AcceptTrade()
    {
        if (!m_bot || !IsTrading())
            return false;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return false;

        // Validate trade before accepting
        if (m_securityLevel >= TradeSecurity::BASIC)
        {
            if (!ValidateTradeItems())
            {
                LogTradeAction("ACCEPT_FAILED", "Item validation failed");
                CancelTrade("Invalid items in trade");
                return false;
            }

            if (!IsTradeSafe())
            {
                LogTradeAction("ACCEPT_FAILED", "Trade safety check failed");
                CancelTrade("Unsafe trade detected");
                return false;
            }
        }

        // Check for scam (skip for owner trades)
        if (m_securityLevel >= TradeSecurity::STANDARD)
        {
            // Don't check scam for trades with owner
            bool isOwnerTrade = false;
            if (Player* trader = ObjectAccessor::FindPlayer(m_currentSession.traderGuid))
            {
                // Check if trader is the bot's master/owner (would need to be set elsewhere)
                // For now, we'll allow trades with group leader as owner
                Group* group = m_bot->GetGroup();
                if (group && group->GetLeaderGUID() == trader->GetGUID())
                    isOwnerTrade = true;
            }

            if (!isOwnerTrade && IsTradeScam())
            {
                LogTradeAction("ACCEPT_FAILED", "Potential scam detected");
                CancelTrade("Potential scam detected");
                return false;
            }
        }

        // Check fairness
        if (m_securityLevel >= TradeSecurity::STRICT)
        {
            if (!EvaluateTradeFairness())
            {
                LogTradeAction("ACCEPT_FAILED", "Trade not fair");
                CancelTrade("Unfair trade");
                return false;
            }
        }

        // Set accepted
        myTrade->SetAccepted(true, false);
        m_currentSession.isAccepted = true;
        SetTradeState(TradeState::ACCEPTING);

        // Check if both accepted
        TradeData* theirTrade = myTrade->GetTraderData();
        if (theirTrade && theirTrade->IsAccepted())
        {
            ProcessTradeCompletion();
        }

        LogTradeAction("ACCEPT", "Trade accepted");
        return true;
    }

    bool BotTradeManager::AddItemToTrade(Item* item, uint8 slot)
    {
        if (!m_bot || !item || !IsTrading())
            return false;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return false;

        // Validate item can be traded
        if (!CanTradeItem(item))
        {
            LogTradeAction("ADD_ITEM_FAILED", "Item cannot be traded: " + std::to_string(item->GetEntry()));
            return false;
        }

        // Find free slot if not specified
        if (slot == 255)
        {
            slot = GetNextFreeTradeSlot();
            if (slot >= MAX_TRADE_ITEMS)
            {
                LogTradeAction("ADD_ITEM_FAILED", "No free trade slots");
                return false;
            }
        }

        // Add item to trade
        myTrade->SetItem(TradeSlots(slot), item, false);

        // Update session
        TradeItemSlot itemSlot;
        itemSlot.slot = slot;
        itemSlot.itemGuid = item->GetGUID();
        itemSlot.itemEntry = item->GetEntry();
        itemSlot.itemCount = item->GetCount();
        itemSlot.estimatedValue = EstimateItemValue(item);
        itemSlot.isQuestItem = item->GetTemplate()->HasFlag(ITEM_FLAG_IS_QUEST_ITEM);
        itemSlot.isSoulbound = item->GetTemplate()->HasFlag(ITEM_FLAG_IS_BOUND_TO_ACCOUNT);
        itemSlot.quality = ItemQuality(item->GetTemplate()->GetQuality());

        m_currentSession.offeredItems.push_back(itemSlot);
        SetTradeState(TradeState::ADDING_ITEMS);

        LogTradeItem(item, true);
        return true;
    }

    bool BotTradeManager::AddItemsToTrade(std::vector<Item*> const& items)
    {
        if (!m_bot || items.empty() || !IsTrading())
            return false;

        uint32 addedCount = 0;
        for (Item* item : items)
        {
            if (AddItemToTrade(item))
                addedCount++;

            // Check if all slots are full
            if (GetNextFreeTradeSlot() >= MAX_TRADE_ITEMS)
                break;
        }

        return addedCount > 0;
    }

    bool BotTradeManager::RemoveItemFromTrade(uint8 slot)
    {
        if (!m_bot || !IsTrading() || slot >= MAX_TRADE_ITEMS)
            return false;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return false;

        // Clear the slot
        myTrade->SetItem(TradeSlots(slot), nullptr, false);

        // Remove from session
        auto it = std::remove_if(m_currentSession.offeredItems.begin(),
                                 m_currentSession.offeredItems.end(),
                                 [slot](TradeItemSlot const& item) { return item.slot == slot; });

        if (it != m_currentSession.offeredItems.end())
        {
            m_currentSession.offeredItems.erase(it, m_currentSession.offeredItems.end());
            LogTradeAction("REMOVE_ITEM", "Removed item from slot " + std::to_string(slot));
            return true;
        }

        return false;
    }

    bool BotTradeManager::SetTradeGold(uint64 gold)
    {
        if (!m_bot || !IsTrading())
            return false;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return false;

        // Validate gold amount
        if (!ValidateTradeGold(gold))
        {
            LogTradeAction("SET_GOLD_FAILED", "Invalid gold amount: " + std::to_string(gold));
            return false;
        }

        // Check bot has enough gold
        if (gold > m_bot->GetMoney())
        {
            LogTradeAction("SET_GOLD_FAILED", "Insufficient gold");
            return false;
        }

        // Set gold in trade
        myTrade->SetMoney(gold);
        m_currentSession.offeredGold = gold;
        SetTradeState(TradeState::ADDING_ITEMS);

        LogTradeAction("SET_GOLD", "Set gold to " + std::to_string(gold));
        return true;
    }

    void BotTradeManager::OnTradeStarted(Player* trader)
    {
        if (!trader)
            return;

        m_currentSession.traderGuid = trader->GetGUID();
        SetTradeState(TradeState::ADDING_ITEMS);
        m_currentSession.startTime = std::chrono::steady_clock::now();

        LogTradeAction("TRADE_STARTED", "Trade window opened with " + trader->GetName());
    }

    void BotTradeManager::OnTradeStatusUpdate(TradeData* myTrade, TradeData* theirTrade)
    {
        if (!myTrade || !theirTrade)
            return;

        // Update received items
        m_currentSession.receivedItems.clear();
        for (uint8 slot = 0; slot < MAX_TRADE_ITEMS; ++slot)
        {
            Item* item = theirTrade->GetItem(TradeSlots(slot));
            if (!item)
                continue;

            TradeItemSlot itemSlot;
            itemSlot.slot = slot;
            itemSlot.itemGuid = item->GetGUID();
            itemSlot.itemEntry = item->GetEntry();
            itemSlot.itemCount = item->GetCount();
            itemSlot.estimatedValue = EstimateItemValue(item);
            itemSlot.isQuestItem = item->GetTemplate()->HasFlag(ITEM_FLAG_IS_QUEST_ITEM);
            itemSlot.isSoulbound = item->GetTemplate()->HasFlag(ITEM_FLAG_IS_BOUND_TO_ACCOUNT);
            itemSlot.quality = ItemQuality(item->GetTemplate()->GetQuality());

            m_currentSession.receivedItems.push_back(itemSlot);
        }

        // Update received gold
        m_currentSession.receivedGold = theirTrade->GetMoney();

        // Update acceptance status
        m_currentSession.traderAccepted = theirTrade->IsAccepted();

        // Update state
        if (m_currentSession.state == TradeState::ADDING_ITEMS)
            SetTradeState(TradeState::REVIEWING);

        m_currentSession.updateCount++;
        m_currentSession.lastUpdate = std::chrono::steady_clock::now();
    }

    void BotTradeManager::OnTradeAccepted()
    {
        SetTradeState(TradeState::ACCEPTING);
        LogTradeAction("TRADE_ACCEPTED", "Both parties accepted");
    }

    void BotTradeManager::OnTradeCancelled()
    {
        ProcessTradeCancellation("Trade cancelled by other party");
    }

    void BotTradeManager::OnTradeCompleted()
    {
        ProcessTradeCompletion();
    }

    void BotTradeManager::Update(uint32 diff)
    {
        m_updateTimer += diff;

        // Process pending trade requests
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end();)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.requestTime).count();

            if (elapsed > TRADE_REQUEST_TIMEOUT)
            {
                LogTradeAction("REQUEST_TIMEOUT", "Request from " + it->first.ToString() + " timed out");
                it = m_pendingRequests.erase(it);
            }
            else
            {
                // Auto-accept logic
                if (it->second.isAutoAccept)
                {
                    AcceptTradeRequest(it->first);
                    it = m_pendingRequests.erase(it);
                }
                else
                    ++it;
            }
        }

        // Update current trade
        if (IsTrading())
        {
            if (m_updateTimer >= TRADE_UPDATE_INTERVAL)
            {
                m_updateTimer = 0;
                ProcessTradeUpdate(diff);
            }

            // Check for timeout
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_currentSession.startTime).count();

            if (elapsed > TRADE_TIMEOUT)
            {
                LogTradeAction("TRADE_TIMEOUT", "Trade timed out after " + std::to_string(elapsed) + "ms");
                CancelTrade("Trade timeout");
            }
        }

        // Process pending item transfers
        if (!m_pendingTransfers.empty() && !IsTrading())
        {
            auto& transfer = m_pendingTransfers.front();
            Player* recipient = ObjectAccessor::FindPlayer(transfer.second);

            if (recipient && transfer.first)
            {
                if (SendItemToPlayer(transfer.first, recipient))
                {
                    LogTradeAction("ITEM_TRANSFER", "Sent item " +
                        std::to_string(transfer.first->GetEntry()) + " to " + recipient->GetName());
                }
            }

            m_pendingTransfers.pop();
        }
    }

    bool BotTradeManager::DistributeLoot(std::vector<Item*> const& items, bool useNeedGreed)
    {
        if (!m_bot || items.empty())
            return false;

        Group* group = m_bot->GetGroup();
        if (!group)
        {
            LogTradeAction("DISTRIBUTE_FAILED", "Bot not in group");
            return false;
        }

        // Create distribution plan
        m_currentDistribution = std::make_unique<LootDistribution>();
        m_currentDistribution->items = items;
        m_currentDistribution->useRoundRobin = !useNeedGreed;
        m_currentDistribution->considerSpec = useNeedGreed;

        // Build distribution plan
        BuildLootDistributionPlan(*m_currentDistribution);

        // Execute distribution
        uint32 distributedCount = 0;
        for (Item* item : items)
        {
            if (!item)
                continue;

            // Find best recipient
            std::vector<Player*> candidates;
            for (auto member = group->GetFirstMember(); member != nullptr; member = member->next())
            {
                if (Player* player = member->GetSource())
                {
                    if (player != m_bot && CanPlayerUseItem(item, player))
                        candidates.push_back(player);
                }
            }

            if (candidates.empty())
                continue;

            Player* recipient = SelectBestRecipient(item, candidates);
            if (recipient)
            {
                m_pendingTransfers.push({item, recipient->GetGUID()});
                distributedCount++;
            }
        }

        LogTradeAction("DISTRIBUTE_LOOT", "Distributed " + std::to_string(distributedCount) +
            " items to group");
        return distributedCount > 0;
    }

    bool BotTradeManager::SendItemToPlayer(Item* item, Player* recipient)
    {
        if (!item || !recipient || !m_bot)
            return false;

        // Check if we can trade with recipient
        if (!ValidateTradeTarget(recipient))
            return false;

        // Initiate trade
        if (!InitiateTrade(recipient, "Sending item"))
            return false;

        // Add item to trade
        if (!AddItemToTrade(item))
        {
            CancelTrade("Failed to add item");
            return false;
        }

        // Auto-accept after a short delay
        m_currentSession.isAccepted = true;

        return true;
    }

    bool BotTradeManager::RequestItemFromPlayer(uint32 itemEntry, Player* owner)
    {
        if (!owner || !m_bot || itemEntry == 0)
            return false;

        // Check if owner has the item
        bool hasItem = false;
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* item = owner->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (item->GetEntry() == itemEntry)
                {
                    hasItem = true;
                    break;
                }
            }
        }

        if (!hasItem)
        {
            LogTradeAction("REQUEST_ITEM_FAILED", "Owner doesn't have item " + std::to_string(itemEntry));
            return false;
        }

        // Initiate trade
        return InitiateTrade(owner, "Requesting item " + std::to_string(itemEntry));
    }

    bool BotTradeManager::ValidateTradeTarget(Player* target) const
    {
        if (!target || !m_bot)
            return false;

        // Check blacklist
        if (IsBlacklisted(target->GetGUID()))
        {
            LogTradeAction("VALIDATE_TARGET_FAILED", "Target is blacklisted");
            return false;
        }

        // Check permissions based on security level
        if (m_securityLevel >= TradeSecurity::BASIC)
        {
            // Must be in same group or guild
            Group* myGroup = m_bot->GetGroup();
            Group* theirGroup = target->GetGroup();

            bool sameGroup = myGroup && theirGroup && myGroup == theirGroup;
            bool sameGuild = m_bot->GetGuildId() &&
                           m_bot->GetGuildId() == target->GetGuildId();

            if (!sameGroup && !sameGuild && !IsWhitelisted(target->GetGUID()))
            {
                LogTradeAction("VALIDATE_TARGET_FAILED", "Target not in group/guild/whitelist");
                return false;
            }
        }

        // Check if target is the group leader (acting as owner)
        Group* myGroup = m_bot->GetGroup();
        if (myGroup && myGroup->GetLeaderGUID() == target->GetGUID())
            return true;

        // Check whitelist requirement
        if (m_securityLevel >= TradeSecurity::STRICT)
        {
            if (!IsWhitelisted(target->GetGUID()))
            {
                LogTradeAction("VALIDATE_TARGET_FAILED", "Strict mode - target not whitelisted");
                return false;
            }
        }

        return true;
    }

    bool BotTradeManager::ValidateTradeItems() const
    {
        if (!m_bot)
            return false;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return true;

        // Check each offered item
        for (uint8 slot = 0; slot < MAX_TRADE_ITEMS; ++slot)
        {
            Item* item = myTrade->GetItem(TradeSlots(slot));
            if (!item)
                continue;

            // Validate ownership
            if (!ValidateItemOwnership(item))
            {
                LogTradeAction("VALIDATE_ITEMS_FAILED", "Invalid ownership for item " +
                    std::to_string(item->GetEntry()));
                return false;
            }

            // Check if item is protected
            if (m_protectedItems.count(item->GetEntry()) > 0)
            {
                LogTradeAction("VALIDATE_ITEMS_FAILED", "Protected item " +
                    std::to_string(item->GetEntry()));
                return false;
            }

            // Check item value against max
            uint32 value = EstimateItemValue(item);
            if (value > m_maxTradeValue)
            {
                LogTradeAction("VALIDATE_ITEMS_FAILED", "Item value exceeds maximum: " +
                    std::to_string(value));
                return false;
            }
        }

        return true;
    }

    bool BotTradeManager::ValidateTradeGold(uint64 amount) const
    {
        if (!m_bot)
            return false;

        // Check against bot's money
        if (amount > m_bot->GetMoney())
            return false;

        // Check against max trade value
        if (amount > m_maxTradeValue)
            return false;

        return true;
    }

    bool BotTradeManager::EvaluateTradeFairness() const
    {
        if (m_securityLevel == TradeSecurity::NONE)
            return true;

        // Allow one-sided trades from group leader (acting as owner)
        Group* group = m_bot->GetGroup();
        if (group && group->GetLeaderGUID() == m_currentSession.traderGuid)
            return true;

        // Allow one-sided trades from other group members
        if (group)
        {
            for (auto member = group->GetFirstMember(); member != nullptr; member = member->next())
            {
                if (member->GetSource() && member->GetSource()->GetGUID() == m_currentSession.traderGuid)
                    return true;
            }
        }

        // Check value balance
        return m_currentSession.IsBalanced(0.3f); // 30% tolerance
    }

    bool BotTradeManager::IsTradeScam() const
    {
        if (m_securityLevel < TradeSecurity::STANDARD)
            return false;

        return CheckForScamPatterns();
    }

    bool BotTradeManager::IsTradeSafe() const
    {
        // Check for dangerous patterns
        if (IsTradeScam())
            return false;

        // Check item validity
        if (!ValidateTradeItems())
            return false;

        // Check gold validity
        if (!ValidateTradeGold(m_currentSession.offeredGold))
            return false;

        return true;
    }

    uint32 BotTradeManager::EstimateItemValue(Item* item) const
    {
        if (!item)
            return 0;

        return EstimateItemValue(item->GetEntry(), item->GetCount());
    }

    uint32 BotTradeManager::EstimateItemValue(uint32 itemEntry, uint32 count) const
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry);
        if (!itemTemplate)
            return 0;

        uint32 baseValue = CalculateItemBaseValue(itemTemplate);
        float qualityMult = GetItemQualityMultiplier(ItemQuality(itemTemplate->GetQuality()));
        float levelMult = GetItemLevelMultiplier(itemTemplate->GetBaseItemLevel());

        return uint32(baseValue * qualityMult * levelMult * count);
    }

    bool BotTradeManager::CanTradeItem(Item* item) const
    {
        if (!item || !m_bot)
            return false;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return false;

        // Check if item is tradeable
        if (proto->HasFlag(ITEM_FLAG_NOT_TRADEABLE))
            return false;

        // Check if item is bound
        if (item->IsSoulBound())
            return false;

        // Check if item is quest item (usually not tradeable)
        if (proto->HasFlag(ITEM_FLAG_IS_QUEST_ITEM))
            return false;

        // Check if item is conjured
        if (proto->HasFlag(ITEM_FLAG_IS_CONJURED))
            return false;

        // Validate ownership
        if (!ValidateItemOwnership(item))
            return false;

        return true;
    }

    uint8 BotTradeManager::GetNextFreeTradeSlot() const
    {
        if (!m_bot)
            return MAX_TRADE_ITEMS;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return 0;

        for (uint8 slot = 0; slot < MAX_TRADE_ITEMS; ++slot)
        {
            if (!myTrade->GetItem(TradeSlots(slot)))
                return slot;
        }

        return MAX_TRADE_ITEMS;
    }

    std::vector<Item*> BotTradeManager::GetTradableItems() const
    {
        std::vector<Item*> tradableItems;

        if (!m_bot)
            return tradableItems;

        // Check main inventory
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* item = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (CanTradeItem(item))
                    tradableItems.push_back(item);
            }
        }

        // Check bags
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = m_bot->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    if (Item* item = bag->GetItemByPos(j))
                    {
                        if (CanTradeItem(item))
                            tradableItems.push_back(item);
                    }
                }
            }
        }

        return tradableItems;
    }

    void BotTradeManager::SetTradeState(TradeState newState)
    {
        TradeState oldState = m_currentSession.state;
        m_currentSession.state = newState;

        LogTradeAction("STATE_CHANGE", "State changed from " + std::to_string(uint8(oldState)) +
            " to " + std::to_string(uint8(newState)));
    }

    void BotTradeManager::ResetTradeSession()
    {
        m_currentSession.Reset();
        m_updateTimer = 0;
    }

    void BotTradeManager::UpdateTradeWindow()
    {
        if (!m_bot || !IsTrading())
            return;

        TradeData* myTrade = m_bot->GetTradeData();
        TradeData* theirTrade = myTrade ? myTrade->GetTraderData() : nullptr;

        if (myTrade && theirTrade)
        {
            OnTradeStatusUpdate(myTrade, theirTrade);
        }
    }

    bool BotTradeManager::ProcessTradeUpdate(uint32 diff)
    {
        if (!IsTrading())
            return false;

        UpdateTradeWindow();

        // Auto-accept logic based on state
        if (m_currentSession.state == TradeState::REVIEWING)
        {
            // Check if we should auto-accept
            bool shouldAutoAccept = false;

            // Auto-accept from group leader (acting as owner)
            Group* group = m_bot->GetGroup();
            if (group && group->GetLeaderGUID() == m_currentSession.traderGuid)
                shouldAutoAccept = true;

            // Auto-accept from group members
            if (m_autoAcceptGroup && !shouldAutoAccept)
            {
                Group* group = m_bot->GetGroup();
                if (group)
                {
                    for (auto member = group->GetFirstMember(); member != nullptr; member = member->next())
                    {
                        if (member->GetSource() &&
                            member->GetSource()->GetGUID() == m_currentSession.traderGuid)
                        {
                            shouldAutoAccept = true;
                            break;
                        }
                    }
                }
            }

            // Auto-accept from guild members
            if (m_autoAcceptGuild && !shouldAutoAccept)
            {
                Player* trader = ObjectAccessor::FindPlayer(m_currentSession.traderGuid);
                if (trader && m_bot->GetGuildId() &&
                    m_bot->GetGuildId() == trader->GetGuildId())
                {
                    shouldAutoAccept = true;
                }
            }

            // Auto-accept from whitelist
            if (m_autoAcceptWhitelist && !shouldAutoAccept)
            {
                if (IsWhitelisted(m_currentSession.traderGuid))
                    shouldAutoAccept = true;
            }

            if (shouldAutoAccept && !m_currentSession.isAccepted)
            {
                // Small delay before auto-accepting
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_currentSession.lastUpdate).count();

                if (elapsed > 2000) // 2 second delay
                {
                    AcceptTrade();
                }
            }
        }

        return true;
    }

    uint32 BotTradeManager::CalculateItemBaseValue(ItemTemplate const* itemTemplate) const
    {
        if (!itemTemplate)
            return 0;

        // Use vendor price as base
        uint32 baseValue = itemTemplate->GetSellPrice();

        // If no sell price, estimate based on item level and quality
        if (baseValue == 0)
        {
            baseValue = itemTemplate->GetBaseItemLevel() * 100;

            // Adjust by item class
            switch (itemTemplate->GetClass())
            {
                case ITEM_CLASS_WEAPON:
                    baseValue *= 3;
                    break;
                case ITEM_CLASS_ARMOR:
                    baseValue *= 2;
                    break;
                case ITEM_CLASS_CONSUMABLE:
                    baseValue /= 2;
                    break;
                case ITEM_CLASS_TRADE_GOODS:
                    baseValue /= 3;
                    break;
            }
        }

        return baseValue;
    }

    float BotTradeManager::GetItemQualityMultiplier(ItemQuality quality) const
    {
        switch (quality)
        {
            case ITEM_QUALITY_POOR:      return 0.5f;
            case ITEM_QUALITY_NORMAL:    return 1.0f;
            case ITEM_QUALITY_UNCOMMON:  return 2.5f;
            case ITEM_QUALITY_RARE:      return 5.0f;
            case ITEM_QUALITY_EPIC:      return 10.0f;
            case ITEM_QUALITY_LEGENDARY: return 25.0f;
            case ITEM_QUALITY_ARTIFACT:  return 50.0f;
            default:                     return 1.0f;
        }
    }

    float BotTradeManager::GetItemLevelMultiplier(uint32 itemLevel) const
    {
        if (itemLevel == 0)
            return 1.0f;

        // Scale based on item level relative to max level
        float maxLevel = float(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
        float levelRatio = float(itemLevel) / maxLevel;

        return 1.0f + (levelRatio * 2.0f);
    }

    bool BotTradeManager::IsItemNeededByBot(uint32 itemEntry) const
    {
        if (!m_bot || !m_botAI)
            return false;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry);
        if (!itemTemplate)
            return false;

        // Check if bot can use the item
        if (!m_bot->CanUseItem(itemTemplate))
            return false;

        // Check if it's an upgrade
        // This would need more complex logic based on bot's current gear
        // For now, return true if usable
        return true;
    }

    bool BotTradeManager::IsItemUsableByBot(Item* item) const
    {
        if (!item || !m_bot)
            return false;

        return m_bot->CanUseItem(item) == EQUIP_ERR_OK;
    }

    Player* BotTradeManager::SelectBestRecipient(Item* item, std::vector<Player*> const& candidates) const
    {
        if (candidates.empty() || !item)
            return nullptr;

        Player* bestCandidate = nullptr;
        uint32 highestPriority = 0;

        for (Player* candidate : candidates)
        {
            if (!candidate)
                continue;

            uint32 priority = CalculateItemPriority(item, candidate);
            if (priority > highestPriority)
            {
                highestPriority = priority;
                bestCandidate = candidate;
            }
        }

        return bestCandidate;
    }

    uint32 BotTradeManager::CalculateItemPriority(Item* item, Player* player) const
    {
        if (!item || !player)
            return 0;

        uint32 priority = 0;

        // Check if player can use item
        if (player->CanUseItem(item) != EQUIP_ERR_OK)
            return 0;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            return 0;

        // Base priority on item quality
        priority += uint32(itemTemplate->GetQuality()) * 100;

        // Check if it's an upgrade
        uint16 slot = itemTemplate->GetInventoryType();
        if (slot < EQUIPMENT_SLOT_END)
        {
            if (Item* currentItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                if (itemTemplate->GetBaseItemLevel() > currentItem->GetTemplate()->GetBaseItemLevel())
                    priority += 500;
            }
            else
            {
                // Empty slot, high priority
                priority += 1000;
            }
        }

        // Check class/spec match
        if (itemTemplate->IsUsableBySpecialization(player->GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID), player->GetLevel()))
            priority += 200;

        return priority;
    }

    bool BotTradeManager::CanPlayerUseItem(Item* item, Player* player) const
    {
        if (!item || !player)
            return false;

        return player->CanUseItem(item) == EQUIP_ERR_OK;
    }

    void BotTradeManager::BuildLootDistributionPlan(LootDistribution& distribution)
    {
        if (!m_bot)
            return;

        Group* group = m_bot->GetGroup();
        if (!group)
            return;

        // Build player priorities
        for (auto member = group->GetFirstMember(); member != nullptr; member = member->next())
        {
            if (Player* player = member->GetSource())
            {
                distribution.playerPriorities[player->GetGUID()] = 0;

                // Build needs list
                for (Item* item : distribution.items)
                {
                    if (item && IsItemNeededByBot(item->GetEntry()))
                    {
                        distribution.playerNeeds[player->GetGUID()].push_back(item->GetEntry());
                    }
                }
            }
        }
    }

    bool BotTradeManager::ValidateTradeDistance(Player* trader) const
    {
        if (!trader || !m_bot)
            return false;

        return m_bot->GetDistance(trader) <= m_maxTradeDistance;
    }

    bool BotTradeManager::ValidateTradePermissions(Player* trader) const
    {
        return ValidateTradeTarget(trader);
    }

    bool BotTradeManager::ValidateItemOwnership(Item* item) const
    {
        if (!item || !m_bot)
            return false;

        return item->GetOwnerGUID() == m_bot->GetGUID();
    }

    bool BotTradeManager::CheckForScamPatterns() const
    {
        // Check for common scam patterns

        // Pattern 1: High value given for nothing
        if (m_currentSession.GetTotalOfferedValue() > 1000 * GOLD &&
            m_currentSession.GetTotalReceivedValue() < 100 * GOLD)
        {
            return true;
        }

        // Pattern 2: Suspicious item combinations
        bool hasExpensiveItem = false;
        bool hasJunkItem = false;

        for (auto const& item : m_currentSession.offeredItems)
        {
            if (item.quality >= ITEM_QUALITY_EPIC)
                hasExpensiveItem = true;
            if (item.quality == ITEM_QUALITY_POOR)
                hasJunkItem = true;
        }

        // Offering epic+ items with junk is suspicious
        if (hasExpensiveItem && hasJunkItem)
            return true;

        return false;
    }

    bool BotTradeManager::CheckValueBalance() const
    {
        return m_currentSession.IsBalanced(SCAM_VALUE_THRESHOLD);
    }

    bool BotTradeManager::ExecuteTrade()
    {
        if (!m_bot || !IsTrading())
            return false;

        TradeData* myTrade = m_bot->GetTradeData();
        if (!myTrade)
            return false;

        // Final validation
        if (!IsTradeSafe())
        {
            CancelTrade("Trade failed safety check");
            return false;
        }

        // Set accepted
        myTrade->SetAccepted(true, true);
        return true;
    }

    void BotTradeManager::ProcessTradeCompletion()
    {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - m_currentSession.startTime);

        // Update statistics
        m_statistics.totalTrades++;
        m_statistics.successfulTrades++;
        m_statistics.totalGoldTraded += m_currentSession.offeredGold;
        m_statistics.totalItemsTraded += static_cast<uint32>(m_currentSession.offeredItems.size());
        m_statistics.totalTradeTime += duration;
        m_statistics.lastTradeTime = endTime;

        LogTradeCompletion(true);
        SetTradeState(TradeState::COMPLETED);

        // Clean up trade data
        if (m_bot)
        {
            m_bot->SetTradeData(nullptr);
        }

        ResetTradeSession();
    }

    void BotTradeManager::ProcessTradeCancellation(std::string const& reason)
    {
        // Update statistics
        m_statistics.totalTrades++;
        m_statistics.cancelledTrades++;

        LogTradeAction("TRADE_CANCELLED", reason);
        SetTradeState(TradeState::CANCELLED);

        // Send cancel to other party
        if (m_bot)
        {
            m_bot->TradeCancel(true);
            m_bot->SetTradeData(nullptr);
        }

        ResetTradeSession();
    }

    void BotTradeManager::HandleTradeError(std::string const& error)
    {
        m_statistics.totalTrades++;
        m_statistics.failedTrades++;

        LogTradeAction("TRADE_ERROR", error);
        SetTradeState(TradeState::ERROR);

        CancelTrade(error);
    }

    void BotTradeManager::LogTradeAction(std::string const& action, std::string const& details) const
    {
        if (!m_bot)
            return;

        TC_LOG_DEBUG("bot.trade", "Bot {} - {}: {}",
            m_bot->GetName(), action, details);
    }

    void BotTradeManager::LogTradeItem(Item* item, bool offered) const
    {
        if (!item || !m_bot)
            return;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            return;

        TC_LOG_DEBUG("bot.trade", "Bot {} - {} item: {} x{} (value: {})",
            m_bot->GetName(),
            offered ? "Offering" : "Receiving",
            itemTemplate->GetDefaultLocaleName(),
            item->GetCount(),
            EstimateItemValue(item));
    }

    void BotTradeManager::LogTradeCompletion(bool success) const
    {
        if (!m_bot)
            return;

        if (success)
        {
            TC_LOG_INFO("bot.trade", "Bot {} completed trade - Gave: {} gold, {} items | Received: {} gold, {} items",
                m_bot->GetName(),
                m_currentSession.offeredGold,
                m_currentSession.offeredItems.size(),
                m_currentSession.receivedGold,
                m_currentSession.receivedItems.size());
        }
        else
        {
            TC_LOG_INFO("bot.trade", "Bot {} failed trade with {}",
                m_bot->GetName(),
                m_currentSession.traderGuid.ToString());
        }
    }
}