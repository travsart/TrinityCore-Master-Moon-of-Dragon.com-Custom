/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "Phase3SystemIntegration.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Movement/Core/MovementManager.h"
#include "Interaction/Core/InteractionManager.h"
#include "Quest/QuestCompletion.h"
#include "Quest/QuestTurnIn.h"
#include "AI/Combat/CombatAIIntegrator.h"
#include "Performance/ThreadPool/ThreadPool.h"

namespace Playerbot
{

/**
 * Example integration demonstrating how Phase 3 systems work together
 * This shows the complete flow from BotAI through all subsystems
 */

class Phase3IntegratedBotAI : public BotAI
{
public:
    Phase3IntegratedBotAI(Player* bot) : BotAI(bot)
    {
        InitializeSystems();
    }

    ~Phase3IntegratedBotAI()
    {
        ShutdownSystems();
    }

    void UpdateAI(uint32 diff) override
    {
        // Base update
        BotAI::UpdateAI(diff);

        // Phase 3 System Updates in priority order
        UpdateCombatSystems(diff);     // Highest priority - survival
        UpdateMovementSystems(diff);    // Movement to objectives
        UpdateInteractionSystems(diff); // NPC interactions
        UpdateQuestSystems(diff);       // Quest progress
    }

private:
    void InitializeSystems()
    {
        // Initialize movement system
        m_movementManager = MovementManager::Instance();
        if (!m_movementManager)
        {
            LOG_ERROR("playerbot", "Failed to initialize MovementManager for bot {}", me->GetName());
            return;
        }

        // Initialize interaction system
        m_interactionManager = InteractionManager::Instance();
        if (!m_interactionManager)
        {
            LOG_ERROR("playerbot", "Failed to initialize InteractionManager for bot {}", me->GetName());
            return;
        }

        // Initialize quest completion system
        m_questCompletion = std::make_unique<QuestCompletionManager>();
        m_questTurnIn = std::make_unique<QuestTurnInManager>();

        // Configure bot preferences
        ConfigureBotBehavior();
    }

    void ShutdownSystems()
    {
        // Clean shutdown of all systems
        if (m_movementManager)
            m_movementManager->Stop(me);

        if (m_interactionManager)
            m_interactionManager->ResetInteraction(me);

        m_questCompletion.reset();
        m_questTurnIn.reset();
    }

    void ConfigureBotBehavior()
    {
        // Configure movement preferences
        if (m_movementManager)
        {
            m_movementManager->EnableCollisionAvoidance(me, true);
            m_movementManager->SetCollisionRadius(me, 1.0f);
        }

        // Configure interaction preferences
        if (m_interactionManager)
        {
            m_interactionManager->EnableAutoSell(me, true);
            m_interactionManager->EnableAutoRepair(me, true);
            m_interactionManager->EnableAutoTrain(me, true);
            m_interactionManager->SetInteractionDelay(500); // 0.5s delay between interactions
        }
    }

    void UpdateCombatSystems(uint32 diff)
    {
        if (!me->IsInCombat())
            return;

        // Combat takes priority over all other systems
        // Movement system switches to combat mode automatically
        if (m_movementManager)
        {
            // Combat movement handled by CombatAIIntegrator
            Unit* target = me->GetVictim();
            if (target)
            {
                // Determine combat positioning
                float optimalRange = GetOptimalCombatRange();

                // Use priority movement for combat
                MovementRequest combatMove;
                combatMove.destination = target->GetPosition();
                combatMove.stopDistance = optimalRange;
                combatMove.priority = MovementPriority::COMBAT;
                combatMove.allowPartialPath = true;

                m_movementManager->MoveToWithPriority(me, combatMove);
            }
        }
    }

    void UpdateMovementSystems(uint32 diff)
    {
        if (me->IsInCombat())
            return; // Combat movement has priority

        // Main movement update
        if (m_movementManager)
        {
            m_movementManager->UpdateMovement(me, diff);

            // Check if we need to move somewhere
            if (!m_movementManager->IsMoving(me))
            {
                DetermineNextDestination();
            }
            else
            {
                // Check if stuck
                if (m_movementManager->GetMovementState(me) == MovementState::FAILED)
                {
                    HandleMovementFailure();
                }
            }
        }
    }

    void UpdateInteractionSystems(uint32 diff)
    {
        if (me->IsInCombat() || !m_interactionManager)
            return;

        // Update ongoing interactions
        m_interactionManager->UpdateInteractions(me, diff);

        // Check for nearby important NPCs
        if (!m_interactionManager->IsInteracting(me))
        {
            CheckForImportantNPCs();
        }
    }

    void UpdateQuestSystems(uint32 diff)
    {
        if (me->IsInCombat())
            return;

        // Update quest objectives
        if (m_questCompletion)
        {
            UpdateActiveQuests();
        }

        // Check for quest turn-ins
        if (m_questTurnIn)
        {
            CheckQuestCompletion();
        }
    }

    void DetermineNextDestination()
    {
        // Priority system for determining where to go

        // 1. Check if we need repairs
        if (NeedsRepair())
        {
            if (Creature* repairNPC = m_interactionManager->FindNearestNPC(me, NPCType::REPAIR, 500.0f))
            {
                m_movementManager->MoveToUnit(me, repairNPC, 3.0f);
                m_nextAction = BotAction::REPAIR;
                return;
            }
        }

        // 2. Check if inventory is full
        if (IsInventoryFull())
        {
            if (Creature* vendor = m_interactionManager->FindNearestNPC(me, NPCType::VENDOR, 500.0f))
            {
                m_movementManager->MoveToUnit(me, vendor, 3.0f);
                m_nextAction = BotAction::SELL_ITEMS;
                return;
            }
        }

        // 3. Check for quest objectives
        if (Position questPos = GetNextQuestObjective())
        {
            m_movementManager->MoveTo(me, questPos);
            m_nextAction = BotAction::QUEST_OBJECTIVE;
            return;
        }

        // 4. Follow leader or wander
        if (Unit* leader = GetLeader())
        {
            m_movementManager->Follow(me, leader, 3.0f, 10.0f);
            m_nextAction = BotAction::FOLLOW_LEADER;
        }
        else
        {
            // Wander around current area
            m_movementManager->Wander(me, 30.0f);
            m_nextAction = BotAction::WANDER;
        }
    }

    void CheckForImportantNPCs()
    {
        // Check for quest givers with available quests
        if (Creature* questGiver = m_interactionManager->FindNearestNPC(me, NPCType::QUEST_GIVER, 10.0f))
        {
            auto availableQuests = m_interactionManager->GetAvailableQuests(me, questGiver);
            if (!availableQuests.empty())
            {
                // Queue quest pickup interaction
                InteractionRequest request;
                request.targetGuid = questGiver->GetGUID();
                request.type = InteractionType::QUEST_ACCEPT;
                request.param1 = availableQuests[0]; // Take first available quest
                request.priority = 10;

                m_interactionManager->QueueInteraction(me, request);
            }
        }

        // Check for trainers if we have skill points
        if (HasUnspentTalentPoints() || CanLearnNewSpells())
        {
            if (Creature* trainer = m_interactionManager->FindNearestNPC(me, NPCType::TRAINER, 10.0f))
            {
                InteractionRequest request;
                request.targetGuid = trainer->GetGUID();
                request.type = InteractionType::TRAIN_SPELL;
                request.priority = 5;

                m_interactionManager->QueueInteraction(me, request);
            }
        }
    }

    void UpdateActiveQuests()
    {
        // Process each active quest
        for (auto const& [questId, status] : me->GetQuestStatusMap())
        {
            if (status.Status != QUEST_STATUS_INCOMPLETE)
                continue;

            // Update quest progress
            if (m_questCompletion->UpdateQuestObjectives(me, questId))
            {
                // Quest objective completed
                LOG_DEBUG("playerbot", "Bot {} completed objective for quest {}",
                         me->GetName(), questId);
            }

            // Determine strategy for this quest
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (quest)
            {
                auto strategy = m_questCompletion->DetermineStrategy(me, quest);
                ApplyQuestStrategy(quest, strategy);
            }
        }
    }

    void CheckQuestCompletion()
    {
        // Check all quests for completion
        for (auto const& [questId, status] : me->GetQuestStatusMap())
        {
            if (status.Status != QUEST_STATUS_COMPLETE)
                continue;

            // Find quest turn-in NPC
            if (auto turnInNPC = m_questTurnIn->FindQuestTurnInNPC(me, questId))
            {
                // Move to turn-in NPC
                m_movementManager->MoveToUnit(me, turnInNPC, 3.0f);

                // Queue turn-in interaction
                InteractionRequest request;
                request.targetGuid = turnInNPC->GetGUID();
                request.type = InteractionType::QUEST_TURNIN;
                request.param1 = questId;
                request.param2 = m_questTurnIn->SelectBestReward(me, questId);
                request.priority = 15; // High priority

                m_interactionManager->QueueInteraction(me, request);
            }
        }
    }

    void HandleMovementFailure()
    {
        LOG_DEBUG("playerbot", "Bot {} movement failed, attempting recovery", me->GetName());

        // Try different approaches
        switch (m_movementFailureCount++)
        {
            case 0:
                // First failure - try direct path
                if (m_currentDestination.IsPositionValid())
                {
                    MovementRequest request;
                    request.destination = m_currentDestination;
                    request.forceDirect = true;
                    m_movementManager->MoveToWithPriority(me, request);
                }
                break;

            case 1:
                // Second failure - try jumping
                me->HandleEmoteCommand(EMOTE_ONESHOT_JUMP);
                break;

            case 2:
                // Third failure - teleport if stuck
                if (m_lastValidPosition.IsPositionValid())
                {
                    me->NearTeleportTo(m_lastValidPosition);
                    m_movementFailureCount = 0;
                }
                break;

            default:
                // Reset and find new objective
                m_movementManager->Stop(me);
                m_movementFailureCount = 0;
                DetermineNextDestination();
                break;
        }
    }

    void ApplyQuestStrategy(Quest const* quest, CompletionStrategy strategy)
    {
        switch (strategy)
        {
            case CompletionStrategy::COMBAT_FOCUSED:
                // Find combat areas for this quest
                if (Position combatArea = m_questCompletion->FindObjectiveLocation(me, quest, 0))
                {
                    m_movementManager->MoveTo(me, combatArea);
                }
                break;

            case CompletionStrategy::GATHERING_FOCUSED:
                // Find gathering nodes
                EnableGatheringMode();
                break;

            case CompletionStrategy::GROUP_REQUIRED:
                // Find group members or wait
                if (!me->GetGroup())
                {
                    RequestGroupInvite();
                }
                break;

            case CompletionStrategy::INTERACT_OBJECT:
                // Find interactable objects
                if (GameObject* target = FindQuestGameObject(quest))
                {
                    m_movementManager->MoveTo(me, target->GetPosition());
                }
                break;

            case CompletionStrategy::POSTPONE:
                // Skip for now
                LOG_DEBUG("playerbot", "Bot {} postponing quest {} (too high level)",
                         me->GetName(), quest->GetQuestId());
                break;

            default:
                break;
        }
    }

    // Helper methods
    bool NeedsRepair() const
    {
        // Check if equipment durability is below threshold
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (Item* item = me->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                if (item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0)
                {
                    uint32 current = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
                    uint32 max = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
                    if (current < max * 0.3f) // Below 30% durability
                        return true;
                }
            }
        }
        return false;
    }

    bool IsInventoryFull() const
    {
        // Check free bag slots
        uint32 freeSlots = 0;
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            if (Bag* pBag = me->GetBagByPos(bag))
            {
                freeSlots += pBag->GetFreeSlots();
            }
        }
        return freeSlots < 5; // Less than 5 free slots
    }

    Position GetNextQuestObjective()
    {
        // Get position of next quest objective
        for (auto const& [questId, status] : me->GetQuestStatusMap())
        {
            if (status.Status != QUEST_STATUS_INCOMPLETE)
                continue;

            if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
            {
                // Find incomplete objective
                for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
                {
                    if (!me->IsQuestObjectiveComplete(questId, i))
                    {
                        return m_questCompletion->FindObjectiveLocation(me, quest, i);
                    }
                }
            }
        }
        return Position();
    }

    Unit* GetLeader() const
    {
        if (Group* group = me->GetGroup())
        {
            if (Player* leader = ObjectAccessor::GetPlayer(*me, group->GetLeaderGUID()))
            {
                if (leader != me && leader->IsInWorld() && leader->IsAlive())
                    return leader;
            }
        }
        return nullptr;
    }

    bool HasUnspentTalentPoints() const
    {
        return me->GetFreeTalentPoints() > 0;
    }

    bool CanLearnNewSpells() const
    {
        // Check if there are new spells available at current level
        // This would check trainer spell database
        return me->GetLevel() % 2 == 0; // Simplified: new spells every 2 levels
    }

    void EnableGatheringMode()
    {
        // Enable detection and gathering of resource nodes
        m_gatheringEnabled = true;
    }

    void RequestGroupInvite()
    {
        // Request to join a group for quest
        // This would integrate with group finding system
    }

    GameObject* FindQuestGameObject(Quest const* quest)
    {
        // Find game objects related to quest objectives
        for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (quest->RequiredNpcOrGo[i] < 0) // Negative = GameObject
            {
                uint32 goEntry = -quest->RequiredNpcOrGo[i];
                return m_interactionManager->FindNearestGameObject(me, goEntry, 100.0f);
            }
        }
        return nullptr;
    }

    float GetOptimalCombatRange() const
    {
        // Determine optimal range based on class and spec
        switch (me->GetClass())
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                return MELEE_RANGE;
            case CLASS_HUNTER:
                return 25.0f;
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_PRIEST:
                return 30.0f;
            default:
                return 5.0f;
        }
    }

    enum class BotAction
    {
        NONE,
        FOLLOW_LEADER,
        QUEST_OBJECTIVE,
        REPAIR,
        SELL_ITEMS,
        TRAIN_SPELLS,
        WANDER
    };

private:
    // System pointers
    MovementManager* m_movementManager = nullptr;
    InteractionManager* m_interactionManager = nullptr;
    std::unique_ptr<QuestCompletionManager> m_questCompletion;
    std::unique_ptr<QuestTurnInManager> m_questTurnIn;

    // Bot state
    BotAction m_nextAction = BotAction::NONE;
    Position m_currentDestination;
    Position m_lastValidPosition;
    uint32 m_movementFailureCount = 0;
    bool m_gatheringEnabled = false;

    // Update timers
    uint32 m_questUpdateTimer = 0;
    uint32 m_npcScanTimer = 0;
    uint32 m_inventoryCheckTimer = 0;

    // Constants
    static constexpr float MELEE_RANGE = 2.0f;
    static constexpr uint32 QUEST_UPDATE_INTERVAL = 5000;  // 5 seconds
    static constexpr uint32 NPC_SCAN_INTERVAL = 3000;      // 3 seconds
    static constexpr uint32 INVENTORY_CHECK_INTERVAL = 10000; // 10 seconds
};

// Factory method for creating integrated bot AI
BotAI* CreatePhase3BotAI(Player* bot)
{
    return new Phase3IntegratedBotAI(bot);
}

// Performance-optimized batch update for multiple bots
void UpdateBotBatch(std::vector<Player*> const& bots, uint32 diff)
{
    // Use thread pool for parallel updates
    ThreadPool* pool = ThreadPool::Instance();

    std::vector<std::future<void>> futures;
    futures.reserve(bots.size());

    for (Player* bot : bots)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            continue;

        // Submit bot update to thread pool
        futures.emplace_back(pool->Enqueue([bot, diff]() {
            if (BotAI* ai = bot->GetBotAI())
            {
                ai->UpdateAI(diff);
            }
        }));
    }

    // Wait for all updates to complete
    for (auto& future : futures)
    {
        future.wait();
    }
}

} // namespace Playerbot