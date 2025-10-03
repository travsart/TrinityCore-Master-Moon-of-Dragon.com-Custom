/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_BOT_NPC_INTERACTION_MANAGER_H
#define TRINITYCORE_BOT_NPC_INTERACTION_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <chrono>

class Player;
class Creature;
class GameObject;
class WorldObject;
class Item;
class ItemTemplate;
struct TrainerSpell;
struct VendorItem;

namespace Playerbot
{
    class BotAI;

    /**
     * NPCInteractionManager - Automated NPC interaction system for PlayerBots
     *
     * Handles all NPC interactions including:
     * - Quest givers (accept/complete quests)
     * - Vendors (buy/sell items, repair equipment)
     * - Trainers (learn spells and abilities)
     * - Service NPCs (innkeepers, flight masters, auctioneers)
     */
    class NPCInteractionManager
    {
    public:
        explicit NPCInteractionManager(Player* bot, BotAI* ai);
        ~NPCInteractionManager();

        // Core lifecycle
        void Initialize();
        void Update(uint32 diff);
        void Reset();
        void Shutdown();

        // Quest giver interactions
        bool InteractWithQuestGiver(Creature* questGiver);
        bool AcceptAvailableQuests(Creature* questGiver);
        bool CompleteQuests(Creature* questGiver);
        bool TurnInCompletedQuests(Creature* questGiver);

        // Vendor interactions
        bool InteractWithVendor(Creature* vendor);
        bool BuyFromVendor(Creature* vendor, std::vector<uint32> const& itemsToBuy);
        bool SellToVendor(Creature* vendor);
        bool RepairAtVendor(Creature* vendor);
        bool RestockReagents(Creature* vendor);

        // Trainer interactions
        bool InteractWithTrainer(Creature* trainer);
        bool LearnAvailableSpells(Creature* trainer);
        bool CanAffordTraining() const;
        std::vector<uint32> GetAffordableSpells(Creature* trainer) const;

        // Service NPC interactions
        bool InteractWithInnkeeper(Creature* innkeeper);
        bool SetHearthstone(Creature* innkeeper);
        bool InteractWithFlightMaster(Creature* flightMaster);
        bool FlyToLocation(Creature* flightMaster, uint32 destinationNode);
        bool InteractWithAuctioneer(Creature* auctioneer);

        // NPC discovery
        Creature* FindNearestQuestGiver() const;
        Creature* FindNearestVendor() const;
        Creature* FindNearestTrainer() const;
        Creature* FindNearestRepairVendor() const;
        Creature* FindNearestInnkeeper() const;
        Creature* FindNearestFlightMaster() const;
        Creature* FindNearestAuctioneer() const;

        // Utility methods
        bool MoveToNPC(Creature* npc);
        bool IsInInteractionRange(Creature* npc) const;
        bool CanInteractWithNPC(Creature* npc) const;

        // Vendor evaluation
        bool NeedToRepair() const;
        bool NeedToBuyReagents() const;
        bool NeedToSellJunk() const;
        uint32 GetRepairCost() const;

        // Trainer evaluation
        bool NeedTraining() const;
        uint32 GetTrainingCost(Creature* trainer) const;

        // Configuration
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool enabled) { m_enabled = enabled; }

        void SetAutoRepair(bool enable) { m_autoRepair = enable; }
        void SetAutoTrain(bool enable) { m_autoTrain = enable; }
        void SetAutoSellJunk(bool enable) { m_autoSellJunk = enable; }
        void SetAutoRestockReagents(bool enable) { m_autoRestockReagents = enable; }

        // Statistics
        struct Statistics
        {
            uint32 questsAcceptedFromNPCs = 0;
            uint32 questsCompletedToNPCs = 0;
            uint32 itemsBought = 0;
            uint32 itemsSold = 0;
            uint32 repairCount = 0;
            uint32 spellsLearned = 0;
            uint32 flightsTaken = 0;
            uint64 totalGoldSpent = 0;
            uint64 totalGoldEarned = 0;
        };

        Statistics const& GetStatistics() const { return m_stats; }

        // Performance metrics
        float GetCPUUsage() const { return m_cpuUsage; }
        size_t GetMemoryUsage() const;

    private:
        // Interaction state machine
        enum class InteractionPhase
        {
            IDLE,
            MOVING_TO_NPC,
            INTERACTING,
            PROCESSING,
            COMPLETING,
            FAILED
        };

        // NPC type classification
        enum class NPCType
        {
            UNKNOWN,
            QUEST_GIVER,
            VENDOR,
            TRAINER,
            REPAIR_VENDOR,
            INNKEEPER,
            FLIGHT_MASTER,
            AUCTIONEER,
            BANKER
        };

        // Cached NPC information
        struct NPCInfo
        {
            ObjectGuid guid;
            uint32 entry = 0;
            NPCType type = NPCType::UNKNOWN;
            float distance = 0.0f;
            uint32 lastInteractionTime = 0;
            uint32 interactionCount = 0;
        };

        // Vendor item evaluation
        struct VendorItemInfo
        {
            uint32 itemId = 0;
            uint32 quantity = 0;
            uint32 price = 0;
            float priority = 0.0f;
            bool isReagent = false;
            bool isUpgrade = false;
        };

        // Trainer spell evaluation
        struct TrainerSpellInfo
        {
            uint32 spellId = 0;
            uint32 cost = 0;
            uint32 reqLevel = 0;
            float priority = 0.0f;
            bool canLearn = false;
        };

        // Phase processing
        void UpdateInteractionPhase(uint32 diff);
        void ProcessIdlePhase();
        void ProcessMovingPhase();
        void ProcessInteractingPhase();
        void ProcessProcessingPhase();
        void ProcessCompletingPhase();
        void ProcessFailedPhase();

        // NPC type detection
        NPCType DetermineNPCType(Creature* npc) const;
        bool IsQuestGiver(Creature* npc) const;
        bool IsVendor(Creature* npc) const;
        bool IsTrainer(Creature* npc) const;
        bool IsRepairVendor(Creature* npc) const;
        bool IsInnkeeper(Creature* npc) const;
        bool IsFlightMaster(Creature* npc) const;
        bool IsAuctioneer(Creature* npc) const;

        // Vendor logic
        std::vector<VendorItemInfo> EvaluateVendorItems(Creature* vendor) const;
        std::vector<Item*> GetJunkItems() const;
        std::vector<uint32> GetRequiredReagents() const;
        float CalculateItemPriority(ItemTemplate const* itemTemplate) const;
        bool ShouldBuyItem(ItemTemplate const* itemTemplate) const;
        bool IsJunkItem(Item* item) const;

        // Trainer logic
        std::vector<TrainerSpellInfo> EvaluateTrainerSpells(Creature* trainer) const;
        float CalculateSpellPriority(TrainerSpell const* trainerSpell) const;
        bool ShouldLearnSpell(uint32 spellId) const;
        bool MeetsSpellRequirements(TrainerSpell const* trainerSpell) const;

        // Flight master logic
        std::vector<uint32> GetKnownFlightPaths() const;
        uint32 FindBestFlightDestination(Creature* flightMaster) const;
        bool HasFlightPath(uint32 nodeId) const;
        float CalculateFlightPriority(uint32 nodeId) const;

        // Interaction helpers
        bool StartInteraction(Creature* npc);
        bool EndInteraction();
        bool IsInteracting() const { return m_currentInteraction.npc.IsEmpty() == false; }
        void ClearInteraction();

        // Distance and movement
        bool MoveToPosition(float x, float y, float z);
        float GetInteractionRange(Creature* npc) const;

        // Cache management
        void UpdateNPCCache();
        void ClearNPCCache();
        NPCInfo* GetCachedNPC(ObjectGuid guid);

        // Statistics tracking
        void RecordItemPurchase(uint32 itemId, uint32 cost);
        void RecordItemSale(uint32 itemId, uint32 value);
        void RecordRepair(uint32 cost);
        void RecordSpellLearned(uint32 spellId, uint32 cost);
        void RecordFlight(uint32 cost);

        // Performance tracking
        void StartPerformanceTimer();
        void EndPerformanceTimer();
        void UpdatePerformanceMetrics();

    private:
        Player* m_bot;
        BotAI* m_ai;
        bool m_enabled;

        // Interaction state
        InteractionPhase m_currentPhase;
        uint32 m_phaseTimer;

        struct CurrentInteraction
        {
            ObjectGuid npc;
            NPCType type;
            uint32 startTime;
            uint32 timeout;
        };
        CurrentInteraction m_currentInteraction;

        // NPC cache
        std::vector<NPCInfo> m_nearbyNPCs;
        uint32 m_lastNPCScan;
        uint32 m_npcScanInterval;

        // Interaction history (prevent spam)
        std::unordered_map<ObjectGuid, uint32> m_lastInteractionTime;
        uint32 m_interactionCooldown;

        // Configuration
        bool m_autoRepair;
        bool m_autoTrain;
        bool m_autoSellJunk;
        bool m_autoRestockReagents;
        uint32 m_maxRepairCost;
        uint32 m_maxTrainingCost;
        float m_minDurabilityPercent;
        uint32 m_reagentRestockAmount;

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

#endif // TRINITYCORE_BOT_NPC_INTERACTION_MANAGER_H
