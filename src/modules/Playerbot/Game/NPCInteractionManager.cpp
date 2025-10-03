/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "NPCInteractionManager.h"
#include "BotAI.h"
#include "Player.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Bag.h"
#include "MotionMaster.h"
#include "WorldSession.h"
#include "CreatureData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>

namespace Playerbot
{
    // Configuration constants
    static constexpr float NPC_SCAN_RANGE = 50.0f;
    static constexpr float NPC_INTERACTION_RANGE = 5.0f;
    static constexpr uint32 NPC_SCAN_INTERVAL = 10000;  // 10 seconds
    static constexpr uint32 INTERACTION_COOLDOWN = 5000; // 5 seconds between interactions
    static constexpr uint32 INTERACTION_TIMEOUT = 30000; // 30 seconds
    static constexpr float MIN_DURABILITY_PERCENT = 0.25f; // 25% durability
    static constexpr uint32 MAX_REPAIR_COST = 10000; // 1 gold max repair cost
    static constexpr uint32 MAX_TRAINING_COST = 100000; // 10 gold max training cost
    static constexpr uint32 REAGENT_RESTOCK_AMOUNT = 100;

    NPCInteractionManager::NPCInteractionManager(Player* bot, BotAI* ai)
        : m_bot(bot)
        , m_ai(ai)
        , m_enabled(true)
        , m_currentPhase(InteractionPhase::IDLE)
        , m_phaseTimer(0)
        , m_lastNPCScan(0)
        , m_npcScanInterval(NPC_SCAN_INTERVAL)
        , m_interactionCooldown(INTERACTION_COOLDOWN)
        , m_autoRepair(true)
        , m_autoTrain(true)
        , m_autoSellJunk(true)
        , m_autoRestockReagents(false)
        , m_maxRepairCost(MAX_REPAIR_COST)
        , m_maxTrainingCost(MAX_TRAINING_COST)
        , m_minDurabilityPercent(MIN_DURABILITY_PERCENT)
        , m_reagentRestockAmount(REAGENT_RESTOCK_AMOUNT)
        , m_totalUpdateTime(0)
        , m_updateCount(0)
        , m_cpuUsage(0.0f)
    {
        m_currentInteraction.timeout = INTERACTION_TIMEOUT;
    }

    NPCInteractionManager::~NPCInteractionManager()
    {
        Shutdown();
    }

    void NPCInteractionManager::Initialize()
    {
        UpdateNPCCache();
        TC_LOG_DEBUG("bot.playerbot", "NPCInteractionManager initialized for bot %s", m_bot->GetName().c_str());
    }

    void NPCInteractionManager::Update(uint32 diff)
    {
        if (!m_enabled || !m_bot || !m_bot->IsInWorld())
            return;

        StartPerformanceTimer();

        // Update NPC cache periodically
        if (getMSTime() - m_lastNPCScan > m_npcScanInterval)
        {
            UpdateNPCCache();
            m_lastNPCScan = getMSTime();
        }

        // Update interaction phase
        UpdateInteractionPhase(diff);

        EndPerformanceTimer();
        UpdatePerformanceMetrics();
    }

    void NPCInteractionManager::Reset()
    {
        ClearInteraction();
        ClearNPCCache();
        m_currentPhase = InteractionPhase::IDLE;
        m_phaseTimer = 0;
    }

    void NPCInteractionManager::Shutdown()
    {
        m_enabled = false;
        Reset();
    }

    // Quest Giver Interactions
    bool NPCInteractionManager::InteractWithQuestGiver(Creature* questGiver)
    {
        if (!questGiver || !CanInteractWithNPC(questGiver))
            return false;

        if (!StartInteraction(questGiver))
            return false;

        bool success = false;

        // Accept available quests
        if (AcceptAvailableQuests(questGiver))
            success = true;

        // Turn in completed quests
        if (TurnInCompletedQuests(questGiver))
            success = true;

        EndInteraction();
        return success;
    }

    bool NPCInteractionManager::AcceptAvailableQuests(Creature* questGiver)
    {
        if (!questGiver)
            return false;

        bool acceptedAny = false;

        // Get available quests from this NPC
        for (uint32 questId : sObjectMgr->GetCreatureQuestRelations(questGiver->GetEntry()))
        {
            if (m_bot->CanAddQuest(sObjectMgr->GetQuestTemplate(questId), true))
            {
                m_bot->AddQuest(sObjectMgr->GetQuestTemplate(questId), questGiver);
                m_stats.questsAcceptedFromNPCs++;
                acceptedAny = true;

                TC_LOG_DEBUG("bot.playerbot", "Bot %s accepted quest %u from NPC %u",
                    m_bot->GetName().c_str(), questId, questGiver->GetEntry());
            }
        }

        return acceptedAny;
    }

    bool NPCInteractionManager::CompleteQuests(Creature* questGiver)
    {
        if (!questGiver)
            return false;

        bool completedAny = false;

        // Check all active quests
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = m_bot->GetQuestSlotQuestId(slot);
            if (questId == 0)
                continue;

            if (m_bot->CanCompleteQuest(questId))
            {
                m_bot->CompleteQuest(questId);
                completedAny = true;
            }
        }

        return completedAny;
    }

    bool NPCInteractionManager::TurnInCompletedQuests(Creature* questGiver)
    {
        if (!questGiver)
            return false;

        bool turnedInAny = false;

        // Get quests this NPC can complete
        for (uint32 questId : sObjectMgr->GetCreatureQuestInvolvedRelations(questGiver->GetEntry()))
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            if (m_bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
            {
                if (m_bot->CanRewardQuest(quest, true))
                {
                    // Select best reward (simple logic - take first item)
                    uint32 rewardChoice = 0;
                    LootItemType rewardType = LootItemType::Item;
                    uint32 rewardId = 0;

                    if (quest->RewardChoiceItemId[0])
                    {
                        rewardType = quest->RewardChoiceItemType[0];
                        rewardId = quest->RewardChoiceItemId[0];
                    }

                    m_bot->RewardQuest(quest, rewardType, rewardId, questGiver, true);
                    m_stats.questsCompletedToNPCs++;
                    turnedInAny = true;

                    TC_LOG_DEBUG("bot.playerbot", "Bot %s turned in quest %u to NPC %u",
                        m_bot->GetName().c_str(), questId, questGiver->GetEntry());
                }
            }
        }

        return turnedInAny;
    }

    // Vendor Interactions
    bool NPCInteractionManager::InteractWithVendor(Creature* vendor)
    {
        if (!vendor || !CanInteractWithNPC(vendor))
            return false;

        if (!StartInteraction(vendor))
            return false;

        bool success = false;

        // Sell junk items if enabled
        if (m_autoSellJunk && SellToVendor(vendor))
            success = true;

        // Repair if needed
        if (m_autoRepair && RepairAtVendor(vendor))
            success = true;

        // Restock reagents if enabled
        if (m_autoRestockReagents && RestockReagents(vendor))
            success = true;

        EndInteraction();
        return success;
    }

    bool NPCInteractionManager::BuyFromVendor(Creature* vendor, std::vector<uint32> const& itemsToBuy)
    {
        if (!vendor || itemsToBuy.empty())
            return false;

        bool boughtAny = false;

        for (uint32 itemId : itemsToBuy)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate)
                continue;

            // Check if we can afford it
            uint32 price = itemTemplate->GetBuyPrice();
            if (m_bot->GetMoney() < price)
                continue;

            // Try to buy the item
            // Simplified - actual vendor purchase would require VendorItemData lookup
            // For now, just record the intention
            TC_LOG_DEBUG("bot.playerbot", "Bot %s would buy item %u from vendor (not implemented)",
                m_bot->GetName().c_str(), itemId);
            boughtAny = false;
        }

        return boughtAny;
    }

    bool NPCInteractionManager::SellToVendor(Creature* vendor)
    {
        if (!vendor)
            return false;

        std::vector<Item*> junkItems = GetJunkItems();
        if (junkItems.empty())
            return false;

        bool soldAny = false;

        for (Item* item : junkItems)
        {
            if (!item)
                continue;

            uint32 sellPrice = item->GetTemplate()->GetSellPrice() * item->GetCount();

            // Sell the item
            m_bot->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            m_bot->ModifyMoney(sellPrice);

            RecordItemSale(item->GetEntry(), sellPrice);
            soldAny = true;

            TC_LOG_DEBUG("bot.playerbot", "Bot %s sold item %u for %u copper",
                m_bot->GetName().c_str(), item->GetEntry(), sellPrice);
        }

        return soldAny;
    }

    bool NPCInteractionManager::RepairAtVendor(Creature* vendor)
    {
        if (!vendor || !IsRepairVendor(vendor))
            return false;

        if (!NeedToRepair())
            return false;

        uint32 repairCost = GetRepairCost();
        if (repairCost == 0 || repairCost > m_maxRepairCost)
            return false;

        if (m_bot->GetMoney() < repairCost)
            return false;

        // Repair all items
        m_bot->DurabilityRepairAll(false, 1.0f, false);
        m_bot->ModifyMoney(-static_cast<int32>(repairCost));

        RecordRepair(repairCost);

        TC_LOG_DEBUG("bot.playerbot", "Bot %s repaired equipment for %u copper",
            m_bot->GetName().c_str(), repairCost);

        return true;
    }

    bool NPCInteractionManager::RestockReagents(Creature* vendor)
    {
        if (!vendor)
            return false;

        std::vector<uint32> requiredReagents = GetRequiredReagents();
        if (requiredReagents.empty())
            return false;

        return BuyFromVendor(vendor, requiredReagents);
    }

    // Trainer Interactions
    bool NPCInteractionManager::InteractWithTrainer(Creature* trainer)
    {
        if (!trainer || !CanInteractWithNPC(trainer))
            return false;

        if (!StartInteraction(trainer))
            return false;

        bool success = LearnAvailableSpells(trainer);

        EndInteraction();
        return success;
    }

    bool NPCInteractionManager::LearnAvailableSpells(Creature* trainer)
    {
        if (!trainer || !IsTrainer(trainer))
            return false;

        if (!m_autoTrain || !CanAffordTraining())
            return false;

        std::vector<TrainerSpellInfo> spells = EvaluateTrainerSpells(trainer);
        if (spells.empty())
            return false;

        bool learnedAny = false;

        for (auto const& spellInfo : spells)
        {
            if (!spellInfo.canLearn)
                continue;

            if (m_bot->GetMoney() < spellInfo.cost)
                break;

            // Learn the spell
            m_bot->LearnSpell(spellInfo.spellId, false);
            m_bot->ModifyMoney(-static_cast<int32>(spellInfo.cost));

            RecordSpellLearned(spellInfo.spellId, spellInfo.cost);
            learnedAny = true;

            TC_LOG_DEBUG("bot.playerbot", "Bot %s learned spell %u for %u copper",
                m_bot->GetName().c_str(), spellInfo.spellId, spellInfo.cost);
        }

        return learnedAny;
    }

    bool NPCInteractionManager::CanAffordTraining() const
    {
        return m_bot->GetMoney() >= m_maxTrainingCost;
    }

    std::vector<uint32> NPCInteractionManager::GetAffordableSpells(Creature* trainer) const
    {
        std::vector<uint32> affordable;

        std::vector<TrainerSpellInfo> spells = EvaluateTrainerSpells(trainer);
        for (auto const& spellInfo : spells)
        {
            if (spellInfo.canLearn && m_bot->GetMoney() >= spellInfo.cost)
                affordable.push_back(spellInfo.spellId);
        }

        return affordable;
    }

    // Service NPC Interactions
    bool NPCInteractionManager::InteractWithInnkeeper(Creature* innkeeper)
    {
        if (!innkeeper || !CanInteractWithNPC(innkeeper))
            return false;

        return SetHearthstone(innkeeper);
    }

    bool NPCInteractionManager::SetHearthstone(Creature* innkeeper)
    {
        if (!innkeeper || !IsInnkeeper(innkeeper))
            return false;

        // Set hearthstone to this location
        WorldLocation loc(innkeeper->GetMapId(), innkeeper->GetPositionX(),
                         innkeeper->GetPositionY(), innkeeper->GetPositionZ(),
                         innkeeper->GetOrientation());
        m_bot->SetHomebind(loc, innkeeper->GetAreaId());

        TC_LOG_DEBUG("bot.playerbot", "Bot %s set hearthstone at innkeeper %u",
            m_bot->GetName().c_str(), innkeeper->GetEntry());

        return true;
    }

    bool NPCInteractionManager::InteractWithFlightMaster(Creature* flightMaster)
    {
        if (!flightMaster || !CanInteractWithNPC(flightMaster))
            return false;

        // Simple implementation - just discover the flight path
        if (!StartInteraction(flightMaster))
            return false;

        TC_LOG_DEBUG("bot.playerbot", "Bot %s interacted with flight master %u",
            m_bot->GetName().c_str(), flightMaster->GetEntry());

        EndInteraction();
        return true;
    }

    bool NPCInteractionManager::FlyToLocation(Creature* flightMaster, uint32 destinationNode)
    {
        if (!flightMaster || !IsFlightMaster(flightMaster))
            return false;

        // This would require TaxiPath integration - simplified for now
        TC_LOG_DEBUG("bot.playerbot", "Bot %s attempting to fly to node %u",
            m_bot->GetName().c_str(), destinationNode);

        return false; // Not implemented yet
    }

    bool NPCInteractionManager::InteractWithAuctioneer(Creature* auctioneer)
    {
        if (!auctioneer || !CanInteractWithNPC(auctioneer))
            return false;

        if (!StartInteraction(auctioneer))
            return false;

        TC_LOG_DEBUG("bot.playerbot", "Bot %s interacted with auctioneer %u",
            m_bot->GetName().c_str(), auctioneer->GetEntry());

        EndInteraction();
        return true;
    }

    // NPC Discovery
    Creature* NPCInteractionManager::FindNearestQuestGiver() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::QUEST_GIVER)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    Creature* NPCInteractionManager::FindNearestVendor() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::VENDOR)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    Creature* NPCInteractionManager::FindNearestTrainer() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::TRAINER)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    Creature* NPCInteractionManager::FindNearestRepairVendor() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::REPAIR_VENDOR)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    Creature* NPCInteractionManager::FindNearestInnkeeper() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::INNKEEPER)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    Creature* NPCInteractionManager::FindNearestFlightMaster() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::FLIGHT_MASTER)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    Creature* NPCInteractionManager::FindNearestAuctioneer() const
    {
        float minDistance = std::numeric_limits<float>::max();
        Creature* nearest = nullptr;

        for (auto const& npcInfo : m_nearbyNPCs)
        {
            if (npcInfo.type != NPCType::AUCTIONEER)
                continue;

            if (npcInfo.distance < minDistance)
            {
                Creature* creature = ObjectAccessor::GetCreature(*m_bot, npcInfo.guid);
                if (creature)
                {
                    minDistance = npcInfo.distance;
                    nearest = creature;
                }
            }
        }

        return nearest;
    }

    // Utility Methods
    bool NPCInteractionManager::MoveToNPC(Creature* npc)
    {
        if (!npc)
            return false;

        if (IsInInteractionRange(npc))
            return true;

        // Move to NPC
        m_bot->GetMotionMaster()->MovePoint(0, npc->GetPositionX(), npc->GetPositionY(), npc->GetPositionZ());
        return false; // Not there yet
    }

    bool NPCInteractionManager::IsInInteractionRange(Creature* npc) const
    {
        if (!npc)
            return false;

        return m_bot->GetDistance(npc) <= GetInteractionRange(npc);
    }

    bool NPCInteractionManager::CanInteractWithNPC(Creature* npc) const
    {
        if (!npc || !npc->IsAlive())
            return false;

        if (!m_bot->IsInMap(npc))
            return false;

        // Check interaction cooldown
        auto it = m_lastInteractionTime.find(npc->GetGUID());
        if (it != m_lastInteractionTime.end())
        {
            if (getMSTime() - it->second < m_interactionCooldown)
                return false;
        }

        return true;
    }

    // Vendor Evaluation
    bool NPCInteractionManager::NeedToRepair() const
    {
        float totalDurability = 0.0f;
        uint32 itemCount = 0;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            uint32 maxDurability = *item->m_itemData->MaxDurability;
            uint32 durability = *item->m_itemData->Durability;

            if (maxDurability > 0)
            {
                totalDurability += static_cast<float>(durability) / static_cast<float>(maxDurability);
                itemCount++;
            }
        }

        if (itemCount == 0)
            return false;

        float avgDurability = totalDurability / itemCount;
        return avgDurability < m_minDurabilityPercent;
    }

    bool NPCInteractionManager::NeedToBuyReagents() const
    {
        // Simplified - check if we have less than required amount
        std::vector<uint32> requiredReagents = GetRequiredReagents();
        return !requiredReagents.empty();
    }

    bool NPCInteractionManager::NeedToSellJunk() const
    {
        return !GetJunkItems().empty();
    }

    uint32 NPCInteractionManager::GetRepairCost() const
    {
        uint32 totalCost = 0;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            uint32 maxDurability = *item->m_itemData->MaxDurability;
            uint32 durability = *item->m_itemData->Durability;

            if (maxDurability > 0 && durability < maxDurability)
            {
                // Simplified repair cost calculation
                uint32 itemLevel = item->GetTemplate()->GetBaseItemLevel();
                uint32 damagePercent = ((maxDurability - durability) * 100) / maxDurability;
                totalCost += (itemLevel * damagePercent) / 10;
            }
        }

        return totalCost;
    }

    // Trainer Evaluation
    bool NPCInteractionManager::NeedTraining() const
    {
        // Check if we're at a level where new spells are available
        uint32 botLevel = m_bot->GetLevel();

        // Every 2 levels, check for training
        return (botLevel % 2 == 0);
    }

    uint32 NPCInteractionManager::GetTrainingCost(Creature* trainer) const
    {
        if (!trainer)
            return 0;

        std::vector<TrainerSpellInfo> spells = EvaluateTrainerSpells(trainer);

        uint32 totalCost = 0;
        for (auto const& spellInfo : spells)
        {
            if (spellInfo.canLearn)
                totalCost += spellInfo.cost;
        }

        return totalCost;
    }

    // Phase Processing
    void NPCInteractionManager::UpdateInteractionPhase(uint32 diff)
    {
        m_phaseTimer += diff;

        switch (m_currentPhase)
        {
            case InteractionPhase::IDLE:
                ProcessIdlePhase();
                break;
            case InteractionPhase::MOVING_TO_NPC:
                ProcessMovingPhase();
                break;
            case InteractionPhase::INTERACTING:
                ProcessInteractingPhase();
                break;
            case InteractionPhase::PROCESSING:
                ProcessProcessingPhase();
                break;
            case InteractionPhase::COMPLETING:
                ProcessCompletingPhase();
                break;
            case InteractionPhase::FAILED:
                ProcessFailedPhase();
                break;
        }
    }

    void NPCInteractionManager::ProcessIdlePhase()
    {
        // Nothing to do in idle phase
    }

    void NPCInteractionManager::ProcessMovingPhase()
    {
        if (!IsInteracting())
        {
            m_currentPhase = InteractionPhase::IDLE;
            return;
        }

        Creature* npc = ObjectAccessor::GetCreature(*m_bot, m_currentInteraction.npc);
        if (!npc)
        {
            m_currentPhase = InteractionPhase::FAILED;
            return;
        }

        if (IsInInteractionRange(npc))
        {
            m_currentPhase = InteractionPhase::INTERACTING;
            m_phaseTimer = 0;
        }
        else if (m_phaseTimer > m_currentInteraction.timeout)
        {
            m_currentPhase = InteractionPhase::FAILED;
        }
    }

    void NPCInteractionManager::ProcessInteractingPhase()
    {
        m_currentPhase = InteractionPhase::PROCESSING;
        m_phaseTimer = 0;
    }

    void NPCInteractionManager::ProcessProcessingPhase()
    {
        // Interaction complete
        m_currentPhase = InteractionPhase::COMPLETING;
        m_phaseTimer = 0;
    }

    void NPCInteractionManager::ProcessCompletingPhase()
    {
        EndInteraction();
        m_currentPhase = InteractionPhase::IDLE;
        m_phaseTimer = 0;
    }

    void NPCInteractionManager::ProcessFailedPhase()
    {
        EndInteraction();
        m_currentPhase = InteractionPhase::IDLE;
        m_phaseTimer = 0;
    }

    // NPC Type Detection
    NPCInteractionManager::NPCType NPCInteractionManager::DetermineNPCType(Creature* npc) const
    {
        if (!npc)
            return NPCType::UNKNOWN;

        if (IsQuestGiver(npc))
            return NPCType::QUEST_GIVER;
        if (IsRepairVendor(npc))
            return NPCType::REPAIR_VENDOR;
        if (IsVendor(npc))
            return NPCType::VENDOR;
        if (IsTrainer(npc))
            return NPCType::TRAINER;
        if (IsInnkeeper(npc))
            return NPCType::INNKEEPER;
        if (IsFlightMaster(npc))
            return NPCType::FLIGHT_MASTER;
        if (IsAuctioneer(npc))
            return NPCType::AUCTIONEER;

        return NPCType::UNKNOWN;
    }

    bool NPCInteractionManager::IsQuestGiver(Creature* npc) const
    {
        return npc && npc->IsQuestGiver();
    }

    bool NPCInteractionManager::IsVendor(Creature* npc) const
    {
        return npc && npc->IsVendor();
    }

    bool NPCInteractionManager::IsTrainer(Creature* npc) const
    {
        return npc && npc->IsTrainer();
    }

    bool NPCInteractionManager::IsRepairVendor(Creature* npc) const
    {
        return npc && npc->IsArmorer();
    }

    bool NPCInteractionManager::IsInnkeeper(Creature* npc) const
    {
        return npc && npc->IsInnkeeper();
    }

    bool NPCInteractionManager::IsFlightMaster(Creature* npc) const
    {
        return npc && npc->IsTaxi();
    }

    bool NPCInteractionManager::IsAuctioneer(Creature* npc) const
    {
        return npc && npc->IsAuctioner();
    }

    // Vendor Logic
    std::vector<NPCInteractionManager::VendorItemInfo> NPCInteractionManager::EvaluateVendorItems(Creature* vendor) const
    {
        std::vector<VendorItemInfo> items;

        if (!vendor || !IsVendor(vendor))
            return items;

        // This would require VendorItemData API integration
        // Simplified for now

        return items;
    }

    std::vector<Item*> NPCInteractionManager::GetJunkItems() const
    {
        std::vector<Item*> junkItems;

        // Check inventory
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* item = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item && IsJunkItem(item))
                junkItems.push_back(item);
        }

        // Check bags
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            Bag* pBag = m_bot->GetBagByPos(bag);
            if (!pBag)
                continue;

            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* item = pBag->GetItemByPos(slot);
                if (item && IsJunkItem(item))
                    junkItems.push_back(item);
            }
        }

        return junkItems;
    }

    std::vector<uint32> NPCInteractionManager::GetRequiredReagents() const
    {
        std::vector<uint32> reagents;

        // This would check class-specific reagents
        // Simplified for now

        return reagents;
    }

    float NPCInteractionManager::CalculateItemPriority(ItemTemplate const* itemTemplate) const
    {
        if (!itemTemplate)
            return 0.0f;

        float priority = 0.0f;

        // Base priority from quality
        switch (itemTemplate->GetQuality())
        {
            case ITEM_QUALITY_POOR:     priority = 1.0f; break;
            case ITEM_QUALITY_NORMAL:   priority = 5.0f; break;
            case ITEM_QUALITY_UNCOMMON: priority = 20.0f; break;
            case ITEM_QUALITY_RARE:     priority = 50.0f; break;
            case ITEM_QUALITY_EPIC:     priority = 100.0f; break;
            default: priority = 1.0f; break;
        }

        return priority;
    }

    bool NPCInteractionManager::ShouldBuyItem(ItemTemplate const* itemTemplate) const
    {
        if (!itemTemplate)
            return false;

        // Don't buy poor quality items
        if (itemTemplate->GetQuality() < ITEM_QUALITY_NORMAL)
            return false;

        // Check if we can afford it
        if (m_bot->GetMoney() < itemTemplate->GetBuyPrice())
            return false;

        return true;
    }

    bool NPCInteractionManager::IsJunkItem(Item* item) const
    {
        if (!item)
            return false;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return false;

        // Poor quality items are junk
        if (proto->GetQuality() == ITEM_QUALITY_POOR)
            return true;

        // Check if it's soulbound and we can't use it
        if (item->IsSoulBound())
        {
            // If it's not usable by our class, it's junk
            if (proto->GetAllowableClass() && !(proto->GetAllowableClass() & m_bot->GetClassMask()))
                return true;
        }

        return false;
    }

    // Trainer Logic
    std::vector<NPCInteractionManager::TrainerSpellInfo> NPCInteractionManager::EvaluateTrainerSpells(Creature* trainer) const
    {
        std::vector<TrainerSpellInfo> spells;

        if (!trainer || !IsTrainer(trainer))
            return spells;

        // This would require TrainerSpellData API integration
        // Simplified for now

        return spells;
    }

    float NPCInteractionManager::CalculateSpellPriority(TrainerSpell const* trainerSpell) const
    {
        // Simplified priority calculation
        return 50.0f;
    }

    bool NPCInteractionManager::ShouldLearnSpell(uint32 spellId) const
    {
        // Check if we already know the spell
        if (m_bot->HasSpell(spellId))
            return false;

        return true;
    }

    bool NPCInteractionManager::MeetsSpellRequirements(TrainerSpell const* trainerSpell) const
    {
        // Simplified - would check level, skill, etc.
        return true;
    }

    // Interaction Helpers
    bool NPCInteractionManager::StartInteraction(Creature* npc)
    {
        if (!npc || IsInteracting())
            return false;

        if (!IsInInteractionRange(npc))
        {
            MoveToNPC(npc);
            m_currentPhase = InteractionPhase::MOVING_TO_NPC;
            return false;
        }

        m_currentInteraction.npc = npc->GetGUID();
        m_currentInteraction.type = DetermineNPCType(npc);
        m_currentInteraction.startTime = getMSTime();

        m_lastInteractionTime[npc->GetGUID()] = getMSTime();

        return true;
    }

    bool NPCInteractionManager::EndInteraction()
    {
        if (!IsInteracting())
            return false;

        ClearInteraction();
        return true;
    }

    void NPCInteractionManager::ClearInteraction()
    {
        m_currentInteraction.npc = ObjectGuid::Empty;
        m_currentInteraction.type = NPCType::UNKNOWN;
        m_currentInteraction.startTime = 0;
    }

    float NPCInteractionManager::GetInteractionRange(Creature* npc) const
    {
        return NPC_INTERACTION_RANGE;
    }

    // Cache Management
    void NPCInteractionManager::UpdateNPCCache()
    {
        m_nearbyNPCs.clear();

        std::list<Creature*> creatures;
        Trinity::AnyUnitInObjectRangeCheck checker(m_bot, NPC_SCAN_RANGE, true, true);
        Trinity::CreatureListSearcher searcher(m_bot, creatures, checker);
        Cell::VisitAllObjects(m_bot, searcher, NPC_SCAN_RANGE);

        for (Creature* creature : creatures)
        {
            if (!creature)
                continue;

            NPCType type = DetermineNPCType(creature);
            if (type == NPCType::UNKNOWN)
                continue;

            NPCInfo info;
            info.guid = creature->GetGUID();
            info.entry = creature->GetEntry();
            info.type = type;
            info.distance = m_bot->GetDistance(creature);
            info.lastInteractionTime = 0;

            m_nearbyNPCs.push_back(info);
        }
    }

    void NPCInteractionManager::ClearNPCCache()
    {
        m_nearbyNPCs.clear();
    }

    NPCInteractionManager::NPCInfo* NPCInteractionManager::GetCachedNPC(ObjectGuid guid)
    {
        for (auto& npc : m_nearbyNPCs)
        {
            if (npc.guid == guid)
                return &npc;
        }
        return nullptr;
    }

    // Statistics Tracking
    void NPCInteractionManager::RecordItemPurchase(uint32 itemId, uint32 cost)
    {
        m_stats.itemsBought++;
        m_stats.totalGoldSpent += cost;
    }

    void NPCInteractionManager::RecordItemSale(uint32 itemId, uint32 value)
    {
        m_stats.itemsSold++;
        m_stats.totalGoldEarned += value;
    }

    void NPCInteractionManager::RecordRepair(uint32 cost)
    {
        m_stats.repairCount++;
        m_stats.totalGoldSpent += cost;
    }

    void NPCInteractionManager::RecordSpellLearned(uint32 spellId, uint32 cost)
    {
        m_stats.spellsLearned++;
        m_stats.totalGoldSpent += cost;
    }

    void NPCInteractionManager::RecordFlight(uint32 cost)
    {
        m_stats.flightsTaken++;
        m_stats.totalGoldSpent += cost;
    }

    // Performance Tracking
    void NPCInteractionManager::StartPerformanceTimer()
    {
        m_performanceStart = std::chrono::high_resolution_clock::now();
    }

    void NPCInteractionManager::EndPerformanceTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        m_lastUpdateDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_performanceStart);
        m_totalUpdateTime += m_lastUpdateDuration;
        m_updateCount++;
    }

    void NPCInteractionManager::UpdatePerformanceMetrics()
    {
        if (m_updateCount > 0)
        {
            auto avgMicros = m_totalUpdateTime.count() / m_updateCount;
            m_cpuUsage = avgMicros / 10000.0f;
        }
    }

    size_t NPCInteractionManager::GetMemoryUsage() const
    {
        size_t memory = sizeof(*this);
        memory += m_nearbyNPCs.size() * sizeof(NPCInfo);
        memory += m_lastInteractionTime.size() * (sizeof(ObjectGuid) + sizeof(uint32));
        return memory;
    }

    // Flight Master Logic (stubs for future implementation)
    std::vector<uint32> NPCInteractionManager::GetKnownFlightPaths() const
    {
        return {};
    }

    uint32 NPCInteractionManager::FindBestFlightDestination(Creature* flightMaster) const
    {
        return 0;
    }

    bool NPCInteractionManager::HasFlightPath(uint32 nodeId) const
    {
        return false;
    }

    float NPCInteractionManager::CalculateFlightPriority(uint32 nodeId) const
    {
        return 0.0f;
    }

    bool NPCInteractionManager::MoveToPosition(float x, float y, float z)
    {
        m_bot->GetMotionMaster()->MovePoint(0, x, y, z);
        return true;
    }

} // namespace Playerbot
