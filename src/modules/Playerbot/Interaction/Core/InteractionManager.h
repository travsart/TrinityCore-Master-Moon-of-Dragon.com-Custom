/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _PLAYERBOT_INTERACTIONMANAGER_H_
#define _PLAYERBOT_INTERACTIONMANAGER_H_

#include "Define.h"
#include "ObjectGuid.h"
#include "ItemTemplate.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <shared_mutex>
#include <functional>

class Player;
class Creature;
class GameObject;
class Item;
class Quest;
class SpellInfo;
struct GossipMenuItem;
struct TrainerSpell;

namespace Playerbot
{
    // Forward declarations
    class VendorDatabase;
    class TrainerDatabase;
    class GossipHandler;
    class QuestDialogHandler;

    enum class NPCType : uint8
    {
        VENDOR              = 0,
        TRAINER             = 1,
        QUEST_GIVER         = 2,
        INNKEEPER           = 3,
        FLIGHT_MASTER       = 4,
        STABLE_MASTER       = 5,
        BANKER              = 6,
        AUCTIONEER          = 7,
        MAILBOX             = 8,
        BATTLEMASTER        = 9,
        REPAIR              = 10,
        TRANSMOGRIFIER      = 11,
        REFORGER            = 12,
        PORTAL              = 13,
        DUNGEON_FINDER      = 14,
        GENERAL             = 15
    };

    enum class InteractionType : uint8
    {
        NONE                = 0,
        GOSSIP              = 1,
        VENDOR_BUY          = 2,
        VENDOR_SELL         = 3,
        REPAIR              = 4,
        TRAIN_SPELL         = 5,
        TRAIN_PROFESSION    = 6,
        QUEST_ACCEPT        = 7,
        QUEST_COMPLETE      = 8,
        QUEST_TURNIN        = 9,
        SET_HEARTHSTONE     = 10,
        FLIGHT_PATH         = 11,
        STABLE_PET          = 12,
        BANK_DEPOSIT        = 13,
        BANK_WITHDRAW       = 14,
        AUCTION             = 15,
        MAIL                = 16,
        BATTLEGROUND_QUEUE  = 17
    };

    enum class InteractionResult : uint8
    {
        SUCCESS             = 0,
        FAILED              = 1,
        IN_PROGRESS         = 2,
        INVALID_TARGET      = 3,
        OUT_OF_RANGE        = 4,
        NO_MONEY            = 5,
        INVENTORY_FULL      = 6,
        REQUIREMENT_NOT_MET = 7,
        ON_COOLDOWN         = 8,
        ALREADY_KNOWN       = 9,
        NOT_AVAILABLE       = 10,
        BUSY                = 11,
        CANCELLED           = 12
    };

    struct InteractionRequest
    {
        ObjectGuid targetGuid;
        InteractionType type;
        uint32 param1 = 0;  // itemId, spellId, questId, etc.
        uint32 param2 = 0;  // count, choice, etc.
        uint32 param3 = 0;  // additional parameter
        uint32 priority = 0;
        uint32 timeoutMs = 10000;
        std::function<void(InteractionResult)> callback;
    };

    struct VendorItem
    {
        uint32 itemId;
        uint32 maxCount;
        uint32 incrTime;
        uint32 extendedCost;
        int32 price;
        uint8 type;

        bool IsAvailable() const;
        bool CanAfford(Player* player, uint32 count) const;
    };

    struct TrainerSpellInfo
    {
        uint32 spellId;
        uint32 reqSkill;
        uint32 reqSkillValue;
        uint32 reqLevel;
        uint32 cost;
        uint32 reqSpell;

        bool CanLearn(Player* player) const;
        bool IsAlreadyKnown(Player* player) const;
    };

    struct NPCInteractionData
    {
        ObjectGuid guid;
        std::string name;
        NPCType type;
        Position position;
        float interactionRange;
        std::vector<uint32> services;  // Available services/flags
        uint32 lastInteraction = 0;
        bool isAvailable = true;
    };

    class InteractionManager final
    {
        InteractionManager();
        ~InteractionManager();

    public:
        static InteractionManager* Instance();

        // Initialize and cleanup
        bool Initialize();
        void Shutdown();

        // Main update - called from BotAI
        void UpdateInteractions(Player* bot, uint32 diff);

        // Process queued interactions
        void ProcessPendingInteractions(Player* bot);

        // Main interaction entry point
        InteractionResult InteractWithNPC(Player* bot, Creature* npc);
        InteractionResult InteractWithGameObject(Player* bot, GameObject* go);

        // Queue interaction for processing
        void QueueInteraction(Player* bot, InteractionRequest const& request);
        void CancelInteraction(Player* bot, ObjectGuid targetGuid);
        void ClearInteractionQueue(Player* bot);

        // --- Vendor Operations ---
        bool BuyItem(Player* bot, Creature* vendor, uint32 itemId, uint32 count = 1);
        bool SellItem(Player* bot, Creature* vendor, Item* item, uint32 count = 1);
        bool SellAllJunk(Player* bot, Creature* vendor);
        bool RepairAll(Player* bot, Creature* vendor);
        bool RepairItem(Player* bot, Creature* vendor, Item* item);
        std::vector<VendorItem> GetVendorItems(Creature* vendor) const;
        bool CanAffordItem(Player* bot, uint32 itemId, uint32 count = 1) const;

        // --- Trainer Operations ---
        bool LearnSpell(Player* bot, Creature* trainer, uint32 spellId);
        bool LearnAllAvailableSpells(Player* bot, Creature* trainer);
        bool LearnProfession(Player* bot, Creature* trainer, uint32 skillId);
        bool UnlearnProfession(Player* bot, Creature* trainer, uint32 skillId);
        std::vector<TrainerSpellInfo> GetAvailableSpells(Player* bot, Creature* trainer) const;
        bool CanLearnSpell(Player* bot, uint32 spellId) const;
        uint32 GetTrainingCost(Player* bot, Creature* trainer) const;

        // --- Service Operations ---
        bool SetHearthstone(Player* bot, Creature* innkeeper);
        bool DiscoverFlightPath(Player* bot, Creature* flightMaster);
        bool TakeFlightPath(Player* bot, Creature* flightMaster, uint32 nodeId);
        bool StablePet(Player* bot, Creature* stableMaster, uint32 petSlot);
        bool UnstablePet(Player* bot, Creature* stableMaster, uint32 petSlot);
        bool DepositToBank(Player* bot, Creature* banker, Item* item, uint32 count = 1);
        bool WithdrawFromBank(Player* bot, Creature* banker, uint32 itemId, uint32 count = 1);

        // --- Quest Operations ---
        bool AcceptQuest(Player* bot, Object* questGiver, uint32 questId);
        bool CompleteQuest(Player* bot, Object* questGiver, uint32 questId);
        bool TurnInQuest(Player* bot, Object* questGiver, uint32 questId, uint32 rewardChoice = 0);
        bool AbandonQuest(Player* bot, uint32 questId);
        std::vector<uint32> GetAvailableQuests(Player* bot, Object* questGiver) const;
        std::vector<uint32> GetCompletableQuests(Player* bot, Object* questGiver) const;
        uint32 SelectQuestReward(Player* bot, Quest const* quest) const;

        // --- Gossip Handling ---
        bool SelectGossipOption(Player* bot, Creature* npc, uint32 menuId, uint32 option);
        bool CloseGossip(Player* bot, Creature* npc);
        std::vector<GossipMenuItem> GetGossipOptions(Player* bot, Creature* npc) const;
        bool HasGossipOption(Creature* npc, uint32 option) const;
        std::string GetGossipText(Player* bot, Creature* npc) const;

        // --- Auction House ---
        bool CreateAuction(Player* bot, Creature* auctioneer, Item* item, uint32 bid, uint32 buyout, uint32 duration);
        bool BidOnAuction(Player* bot, Creature* auctioneer, uint32 auctionId, uint32 bid);
        bool BuyoutAuction(Player* bot, Creature* auctioneer, uint32 auctionId);
        bool CancelAuction(Player* bot, Creature* auctioneer, uint32 auctionId);

        // --- Mail Operations ---
        bool SendMail(Player* bot, std::string const& recipient, std::string const& subject,
                      std::string const& body, uint32 money = 0, Item* item = nullptr);
        bool TakeMail(Player* bot, uint32 mailId);
        bool DeleteMail(Player* bot, uint32 mailId);
        bool ReturnMail(Player* bot, uint32 mailId);

        // --- Interaction Queries ---
        bool CanInteract(Player* bot, Creature* npc) const;
        bool CanInteract(Player* bot, GameObject* go) const;
        float GetInteractionRange(Creature* npc) const;
        float GetInteractionRange(GameObject* go) const;
        bool IsInInteractionRange(Player* bot, Object* target) const;
        NPCType GetNPCType(Creature* npc) const;
        std::vector<Creature*> FindNearbyNPCs(Player* bot, NPCType type, float range = 50.0f) const;
        Creature* FindNearestNPC(Player* bot, NPCType type, float maxRange = 100.0f) const;
        GameObject* FindNearestGameObject(Player* bot, uint32 entry, float maxRange = 50.0f) const;

        // --- Interaction State ---
        bool IsInteracting(Player* bot) const;
        InteractionType GetCurrentInteraction(Player* bot) const;
        Object* GetInteractionTarget(Player* bot) const;
        uint32 GetInteractionTime(Player* bot) const;
        void ResetInteraction(Player* bot);

        // --- Smart Interaction ---
        bool SmartSell(Player* bot);          // Find vendor and sell junk
        bool SmartRepair(Player* bot);        // Find repair NPC and repair
        bool SmartTrain(Player* bot);         // Find trainer and learn spells
        bool SmartBank(Player* bot);          // Find bank and manage items
        bool SmartMail(Player* bot);          // Check and collect mail

        // --- Configuration ---
        void SetInteractionDelay(uint32 delayMs) { m_interactionDelay = delayMs; }
        void SetMaxInteractionAttempts(uint8 attempts) { m_maxAttempts = attempts; }
        void EnableAutoSell(Player* bot, bool enable = true);
        void EnableAutoRepair(Player* bot, bool enable = true);
        void EnableAutoTrain(Player* bot, bool enable = true);

        // --- Performance Monitoring ---
        uint32 GetActiveInteractions() const { return m_activeInteractions; }
        uint32 GetQueuedInteractions() const;
        void GetPerformanceMetrics(uint32& successCount, uint32& failCount, uint32& avgTime) const;

    private:
        struct BotInteractionContext
        {
            Player* bot = nullptr;
            Object* target = nullptr;
            InteractionType type = InteractionType::NONE;
            InteractionResult lastResult = InteractionResult::FAILED;
            uint32 startTime = 0;
            uint32 lastUpdate = 0;
            uint32 attempts = 0;
            uint32 param1 = 0;
            uint32 param2 = 0;
            bool isActive = false;

            // Gossip context
            uint32 gossipMenuId = 0;
            std::vector<uint32> gossipPath;

            // Quest context
            uint32 questId = 0;
            uint32 rewardChoice = 0;

            // Vendor context
            std::vector<uint32> itemsToBuy;
            std::vector<Item*> itemsToSell;

            // Training context
            std::vector<uint32> spellsToLearn;

            // Performance metrics
            uint32 totalInteractions = 0;
            uint32 successfulInteractions = 0;
            uint32 failedInteractions = 0;
            uint32 totalTime = 0;
        };

        // Internal methods
        void InitializeBotContext(Player* bot);
        void CleanupBotContext(Player* bot);
        InteractionResult ProcessInteraction(Player* bot, BotInteractionContext& context);
        InteractionResult HandleGossip(Player* bot, BotInteractionContext& context);
        InteractionResult HandleVendor(Player* bot, BotInteractionContext& context);
        InteractionResult HandleTrainer(Player* bot, BotInteractionContext& context);
        InteractionResult HandleQuest(Player* bot, BotInteractionContext& context);
        InteractionResult HandleService(Player* bot, BotInteractionContext& context);
        bool ValidateInteraction(Player* bot, Object* target, InteractionType type) const;
        void LogInteraction(Player* bot, BotInteractionContext const& context) const;
        bool MoveToInteractionRange(Player* bot, Object* target);
        void UpdateNPCDatabase(Creature* npc);

        // Thread-safe data access
        BotInteractionContext* GetContext(Player* bot);
        BotInteractionContext const* GetContext(Player* bot) const;

    private:
        // Bot interaction contexts
        std::unordered_map<ObjectGuid, std::unique_ptr<BotInteractionContext>> m_botContexts;

        // Interaction queues (per bot)
        std::unordered_map<ObjectGuid, std::queue<InteractionRequest>> m_interactionQueues;

        // NPC database cache
        std::unordered_map<ObjectGuid, NPCInteractionData> m_npcDatabase;

        // Subsystems
        std::unique_ptr<VendorDatabase> m_vendorDB;
        std::unique_ptr<TrainerDatabase> m_trainerDB;
        std::unique_ptr<GossipHandler> m_gossipHandler;
        std::unique_ptr<QuestDialogHandler> m_questHandler;

        // Thread safety
        mutable std::shared_mutex m_mutex;
        mutable std::shared_mutex m_npcMutex;

        // Performance tracking
        std::atomic<uint32> m_activeInteractions{0};
        std::atomic<uint32> m_totalSuccess{0};
        std::atomic<uint32> m_totalFailed{0};
        std::atomic<uint32> m_totalTime{0};

        // Configuration
        uint32 m_interactionDelay = 1000;      // Default 1 second delay
        uint8 m_maxAttempts = 3;               // Max attempts per interaction
        float m_defaultInteractionRange = 5.0f; // Default interaction range

        // Singleton instance
        static std::unique_ptr<InteractionManager> s_instance;
        static std::once_flag s_initFlag;
    };

    // Helper macros for interaction manager access
    #define sInteractionMgr InteractionManager::Instance()
}

#endif // _PLAYERBOT_INTERACTIONMANAGER_H_