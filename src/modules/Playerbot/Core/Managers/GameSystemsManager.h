/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * GAME SYSTEMS MANAGER - Concrete Facade Implementation
 *
 * Phase 6: BotAI Decoupling & Final Cleanup
 *
 * This class consolidates all 17 manager instances from BotAI into a single
 * facade, reducing BotAI's complexity and improving testability.
 */

#ifndef PLAYERBOT_GAME_SYSTEMS_MANAGER_H
#define PLAYERBOT_GAME_SYSTEMS_MANAGER_H

#include "IGameSystemsManager.h"
#include <memory>

// Manager includes
#include "Game/QuestManager.h"
#include "Social/TradeManager.h"
#include "Professions/GatheringManager.h"
#include "Professions/ProfessionManager.h"
#include "Economy/AuctionManager.h"
#include "Advanced/GroupCoordinator.h"
#include "Lifecycle/DeathRecoveryManager.h"
#include "Movement/UnifiedMovementCoordinator.h"
#include "Combat/CombatStateManager.h"
#include "Combat/TargetScanner.h"
#include "Group/GroupInvitationHandler.h"
#include "Core/Events/EventDispatcher.h"
#include "Core/Managers/ManagerRegistry.h"
#include "Decision/DecisionFusionSystem.h"
#include "Decision/ActionPriorityQueue.h"
#include "Decision/BehaviorTree.h"
#include "Decision/HybridAIController.h"
#include "BehaviorPriorityManager.h"

namespace Playerbot
{

// Forward declaration
class BotAI;

/**
 * @brief Concrete implementation of IGameSystemsManager facade
 *
 * **Ownership:**
 * - Owns all 18 manager instances via std::unique_ptr (including ProfessionManager)
 * - Owned by BotAI via std::unique_ptr<IGameSystemsManager>
 * - Returns raw pointers (non-owning) to external callers
 *
 * **Lifecycle:**
 * 1. BotAI constructs: _gameSystems = std::make_unique<GameSystemsManager>(_bot, botAI)
 * 2. BotAI initializes: _gameSystems->Initialize(_bot)
 * 3. BotAI updates each frame: _gameSystems->Update(diff)
 * 4. BotAI destructs: _gameSystems.reset() auto-destroys all managers
 *
 * **Phase 6 Migration:**
 * All manager initialization, update, and cleanup code moved from BotAI
 * to this facade, reducing BotAI from 3,013 lines to ~1,800 lines (40% reduction).
 *
 * **Phase 1B Integration (2025-11-18):**
 * ProfessionManager converted from global singleton to per-bot instance (18th manager),
 * eliminating singleton anti-pattern and aligning with Phase 6 per-bot architecture.
 */
class TC_GAME_API GameSystemsManager final : public IGameSystemsManager
{
public:
    /**
     * @brief Construct facade for bot's game systems
     * @param bot The bot player this facade serves
     * @param botAI The BotAI instance (needed for manager constructors)
     */
    explicit GameSystemsManager(Player* bot, BotAI* botAI);

    /**
     * @brief Destructor - handles proper cleanup order
     *
     * CRITICAL: Managers destroyed in correct dependency order to prevent
     * access violations when unsubscribing from EventDispatcher.
     */
    ~GameSystemsManager() override;

    // ========================================================================
    // LIFECYCLE (IGameSystemsManager interface)
    // ========================================================================

    void Initialize(Player* bot) override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    // ========================================================================
    // GAME SYSTEM ACCESS (IGameSystemsManager interface)
    // ========================================================================

    QuestManager* GetQuestManager() const override { return _questManager.get(); }
    TradeManager* GetTradeManager() const override { return _tradeManager.get(); }
    GatheringManager* GetGatheringManager() const override { return _gatheringManager.get(); }
    ProfessionManager* GetProfessionManager() const override { return _professionManager.get(); }
    AuctionManager* GetAuctionManager() const override { return _auctionManager.get(); }
    Advanced::GroupCoordinator* GetGroupCoordinator() const override { return _groupCoordinator.get(); }
    DeathRecoveryManager* GetDeathRecoveryManager() const override { return _deathRecoveryManager.get(); }
    UnifiedMovementCoordinator* GetMovementCoordinator() const override { return _unifiedMovementCoordinator.get(); }
    CombatStateManager* GetCombatStateManager() const override { return _combatStateManager.get(); }

    // Decision systems
    bot::ai::DecisionFusionSystem* GetDecisionFusion() const override { return _decisionFusion.get(); }
    bot::ai::ActionPriorityQueue* GetActionPriorityQueue() const override { return _actionPriorityQueue.get(); }
    bot::ai::BehaviorTree* GetBehaviorTree() const override { return _behaviorTree.get(); }

    // Event system
    Events::EventDispatcher* GetEventDispatcher() const override { return _eventDispatcher.get(); }
    ManagerRegistry* GetManagerRegistry() const override { return _managerRegistry.get(); }

    // Helper systems
    TargetScanner* GetTargetScanner() const override { return _targetScanner.get(); }
    GroupInvitationHandler* GetGroupInvitationHandler() const override { return _groupInvitationHandler.get(); }
    bot::ai::HybridAIController* GetHybridAI() const override { return _hybridAI.get(); }
    bot::ai::BehaviorPriorityManager* GetPriorityManager() const override { return _priorityManager.get(); }

private:
    // ========================================================================
    // MANAGER INSTANCES - All 18 managers owned by facade
    // ========================================================================

    // Core game systems
    std::unique_ptr<QuestManager> _questManager;
    std::unique_ptr<TradeManager> _tradeManager;
    std::unique_ptr<GatheringManager> _gatheringManager;
    std::unique_ptr<ProfessionManager> _professionManager;
    std::unique_ptr<AuctionManager> _auctionManager;
    std::unique_ptr<Advanced::GroupCoordinator> _groupCoordinator;

    // Lifecycle systems
    std::unique_ptr<DeathRecoveryManager> _deathRecoveryManager;

    // Movement system
    std::unique_ptr<UnifiedMovementCoordinator> _unifiedMovementCoordinator;

    // Combat systems
    std::unique_ptr<CombatStateManager> _combatStateManager;
    std::unique_ptr<TargetScanner> _targetScanner;

    // Group systems
    std::unique_ptr<GroupInvitationHandler> _groupInvitationHandler;

    // Event and registry systems
    std::unique_ptr<Events::EventDispatcher> _eventDispatcher;
    std::unique_ptr<ManagerRegistry> _managerRegistry;

    // Decision systems
    std::unique_ptr<bot::ai::DecisionFusionSystem> _decisionFusion;
    std::unique_ptr<bot::ai::ActionPriorityQueue> _actionPriorityQueue;
    std::unique_ptr<bot::ai::BehaviorTree> _behaviorTree;
    std::unique_ptr<bot::ai::HybridAIController> _hybridAI;

    // Behavior management
    std::unique_ptr<bot::ai::BehaviorPriorityManager> _priorityManager;

    // ========================================================================
    // INTERNAL STATE
    // ========================================================================

    Player* _bot;                   // Bot player reference (non-owning)
    BotAI* _botAI;                  // BotAI reference (non-owning, needed for initialization)
    bool _initialized{false};       // Initialization state flag

    // Update throttling timers (moved from BotAI)
    uint32 _equipmentCheckTimer{0};
    uint32 _professionCheckTimer{0};
    uint32 _bankingCheckTimer{0};
    uint32 _debugLogAccumulator{0};

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Initialize Hybrid AI system (Decision Fusion + Behavior Tree)
     * Separated from main initialization for clarity
     */
    void InitializeHybridAI();

    /**
     * @brief Subscribe managers to EventDispatcher
     * Handles all event type registrations
     */
    void SubscribeManagersToEvents();

    /**
     * @brief Update all managers that require per-frame processing
     * @param diff Time delta in milliseconds
     */
    void UpdateManagers(uint32 diff);

    // Non-copyable
    GameSystemsManager(GameSystemsManager const&) = delete;
    GameSystemsManager& operator=(GameSystemsManager const&) = delete;
};

} // namespace Playerbot

#endif // PLAYERBOT_GAME_SYSTEMS_MANAGER_H
