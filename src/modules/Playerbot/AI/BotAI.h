/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED BotAI - Clean Update Chain Architecture
 *
 * This refactored version provides:
 * 1. Single, clean update path without DoUpdateAI/UpdateEnhanced confusion
 * 2. Clear separation between base behaviors and combat specialization
 * 3. No throttling that breaks movement/following
 * 4. Proper virtual method hierarchy for class-specific overrides
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Strategy/Strategy.h"
#include "ObjectCache.h"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include <queue>
#include <shared_mutex>

namespace Playerbot
{

// Forward declarations
class Value;
class GroupInvitationHandler;
class TargetScanner;
class QuestManager;
class TradeManager;
class GatheringManager;
class AuctionManager;

// TriggerResult comparator for priority queue
struct TriggerResultComparator
{
    bool operator()(TriggerResult const& a, TriggerResult const& b) const;
};

// Enhanced AI state enum
enum class BotAIState
{
    IDLE,
    COMBAT,
    DEAD,
    TRAVELLING,
    QUESTING,
    GATHERING,
    TRADING,
    FOLLOWING,
    FLEEING,
    RESTING
};

// AI update result for performance tracking
struct AIUpdateResult
{
    uint32 actionsExecuted = 0;
    uint32 triggersChecked = 0;
    uint32 strategiesEvaluated = 0;
    std::chrono::microseconds updateTime{0};
};

class TC_GAME_API BotAI
{
public:
    explicit BotAI(Player* bot);
    virtual ~BotAI();

    // ========================================================================
    // CLEAN UPDATE INTERFACE - Single entry point, no confusion
    // ========================================================================

    /**
     * Main update method - SINGLE ENTRY POINT for all AI updates
     * This is the ONLY method called by BotSession::Update()
     *
     * Update flow:
     * 1. Update core behaviors (strategies, movement, idle)
     * 2. Check combat state transitions
     * 3. If in combat AND derived class exists, call OnCombatUpdate()
     *
     * CRITICAL: This method is NOT throttled and runs every frame
     * to ensure smooth movement and responsive behavior
     */
    virtual void UpdateAI(uint32 diff);

    /**
     * Virtual method for class-specific COMBAT ONLY updates
     * Called by UpdateAI() when bot is in combat
     *
     * ClassAI implementations should override this for:
     * - Combat rotations
     * - Target selection
     * - Cooldown management
     * - Resource management
     *
     * MUST NOT:
     * - Control movement (handled by strategies)
     * - Throttle updates (causes following issues)
     * - Call base UpdateAI (would cause recursion)
     */
    virtual void OnCombatUpdate(uint32 diff) {}

    // ========================================================================
    // STATE TRANSITIONS - Clean lifecycle management
    // ========================================================================

    virtual void Reset();
    virtual void OnDeath();
    virtual void OnRespawn();
    virtual void OnCombatStart(::Unit* target);
    virtual void OnCombatEnd();

    // ========================================================================
    // STRATEGY MANAGEMENT - Core behavior system
    // ========================================================================

    void AddStrategy(std::unique_ptr<Strategy> strategy);
    void RemoveStrategy(std::string const& name);
    Strategy* GetStrategy(std::string const& name) const;
    std::vector<Strategy*> GetActiveStrategies() const;
    void ActivateStrategy(std::string const& name);
    void DeactivateStrategy(std::string const& name);

    // ========================================================================
    // ACTION EXECUTION - Command pattern implementation
    // ========================================================================

    bool ExecuteAction(std::string const& actionName);
    bool ExecuteAction(std::string const& name, ActionContext const& context);
    bool IsActionPossible(std::string const& actionName) const;
    uint32 GetActionPriority(std::string const& actionName) const;

    void QueueAction(std::shared_ptr<Action> action, ActionContext const& context = {});
    void CancelCurrentAction();
    bool IsActionInProgress() const { return _currentAction != nullptr; }

    // ========================================================================
    // STATE MANAGEMENT - AI state tracking
    // ========================================================================

    BotAIState GetAIState() const { return _aiState; }
    void SetAIState(BotAIState state);
    bool IsInCombat() const { return _aiState == BotAIState::COMBAT; }
    bool IsIdle() const { return _aiState == BotAIState::IDLE; }
    bool IsFollowing() const { return _aiState == BotAIState::FOLLOWING; }

    // ========================================================================
    // BOT ACCESS - Core entity access
    // ========================================================================

    Player* GetBot() const { return _bot; }
    ObjectGuid GetBotGuid() const { return _bot ? _bot->GetGUID() : ObjectGuid::Empty; }

    // ========================================================================
    // GROUP MANAGEMENT - Social behavior
    // ========================================================================

    void OnGroupJoined(Group* group);
    void OnGroupLeft();
    void HandleGroupChange();
    GroupInvitationHandler* GetGroupInvitationHandler() { return _groupInvitationHandler.get(); }
    GroupInvitationHandler const* GetGroupInvitationHandler() const { return _groupInvitationHandler.get(); }

    // ========================================================================
    // TARGET MANAGEMENT - Combat targeting
    // ========================================================================

    void SetTarget(ObjectGuid guid) { _currentTarget = guid; }
    ObjectGuid GetTarget() const { return _currentTarget; }
    ::Unit* GetTargetUnit() const;

    // ========================================================================
    // MOVEMENT CONTROL - Strategy-driven movement
    // ========================================================================

    void MoveTo(float x, float y, float z);
    void Follow(::Unit* target, float distance = 5.0f);
    void StopMovement();
    bool IsMoving() const;

    // ========================================================================
    // GAME SYSTEM MANAGERS - Quest, profession, trade management
    // ========================================================================

    QuestManager* GetQuestManager() { return _questManager.get(); }
    QuestManager const* GetQuestManager() const { return _questManager.get(); }

    TradeManager* GetTradeManager() { return _tradeManager.get(); }
    TradeManager const* GetTradeManager() const { return _tradeManager.get(); }

    GatheringManager* GetGatheringManager() { return _gatheringManager.get(); }
    GatheringManager const* GetGatheringManager() const { return _gatheringManager.get(); }

    AuctionManager* GetAuctionManager() { return _auctionManager.get(); }
    AuctionManager const* GetAuctionManager() const { return _auctionManager.get(); }

    // ========================================================================
    // PERFORMANCE METRICS - Monitoring and optimization
    // ========================================================================

    struct PerformanceMetrics
    {
        std::atomic<uint32> totalUpdates{0};
        std::atomic<uint32> actionsExecuted{0};
        std::atomic<uint32> triggersProcessed{0};
        std::atomic<uint32> strategiesEvaluated{0};
        std::chrono::microseconds averageUpdateTime{0};
        std::chrono::microseconds maxUpdateTime{0};
        std::chrono::steady_clock::time_point lastUpdate;
    };

    PerformanceMetrics const& GetPerformanceMetrics() const { return _performanceMetrics; }

protected:
    // ========================================================================
    // INTERNAL UPDATE METHODS - Called by UpdateAI()
    // ========================================================================

    /**
     * Update all active strategies
     * CRITICAL: Must run every frame for following behavior
     */
    void UpdateStrategies(uint32 diff);

    /**
     * Update movement based on active strategies
     * CRITICAL: Must run every frame for smooth movement
     */
    void UpdateMovement(uint32 diff);

    /**
     * Update idle behaviors (questing, trading, etc.)
     * Only runs when not in combat or following
     */
    void UpdateIdleBehaviors(uint32 diff);

    /**
     * Check and handle combat state transitions
     */
    void UpdateCombatState(uint32 diff);

    /**
     * Process all registered triggers
     */
    void ProcessTriggers();

    /**
     * Execute queued and triggered actions
     */
    void UpdateActions(uint32 diff);

    /**
     * Update internal values and caches
     */
    void UpdateValues(uint32 diff);

    /**
     * Update all BehaviorManager-based managers
     */
    void UpdateManagers(uint32 diff);

    // ========================================================================
    // HELPER METHODS - Utilities for derived classes
    // ========================================================================

    Strategy* SelectBestStrategy();
    std::shared_ptr<Action> SelectNextAction();
    bool CanExecuteAction(Action* action) const;
    ActionResult ExecuteActionInternal(Action* action, ActionContext const& context);

    void EvaluateTrigger(Trigger* trigger);
    void HandleTriggeredAction(TriggerResult const& result);

    void InitializeDefaultStrategies();
    void UpdatePerformanceMetrics(uint32 updateTimeMs);
    void LogAIDecision(std::string const& action, float score) const;

protected:
    // Core components
    Player* _bot;
    BotAIState _aiState = BotAIState::IDLE;
    ObjectGuid _currentTarget;

    // Strategy system
    std::unordered_map<std::string, std::unique_ptr<Strategy>> _strategies;
    std::vector<std::string> _activeStrategies;

    // Action system
    std::queue<std::pair<std::shared_ptr<Action>, ActionContext>> _actionQueue;
    std::shared_ptr<Action> _currentAction;
    ActionContext _currentContext;

    // Trigger system
    std::vector<std::shared_ptr<Trigger>> _triggers;
    std::priority_queue<TriggerResult, std::vector<TriggerResult>, TriggerResultComparator> _triggeredActions;

    // Value cache
    std::unordered_map<std::string, float> _values;

    // Group management
    std::unique_ptr<GroupInvitationHandler> _groupInvitationHandler;
    bool _wasInGroup = false;

    // Target scanning for autonomous engagement
    std::unique_ptr<TargetScanner> _targetScanner;

    // Game system managers
    std::unique_ptr<QuestManager> _questManager;
    std::unique_ptr<TradeManager> _tradeManager;
    std::unique_ptr<GatheringManager> _gatheringManager;
    std::unique_ptr<AuctionManager> _auctionManager;

    // Performance tracking
    mutable PerformanceMetrics _performanceMetrics;

    // CRITICAL FIX #19: ObjectAccessor Deadlock Resolution
    // Cache for all ObjectAccessor results to eliminate recursive TrinityCore
    // std::shared_mutex acquisitions. Refreshed once per UpdateAI() cycle.
    ObjectCache _objectCache;

    // Thread safety
    // DEADLOCK FIX #14: Changed from std::shared_mutex to std::recursive_mutex
    // After 13 failed fixes, the root cause is that std::shared_mutex does NOT
    // support recursive locking (even shared_lock). When the same thread tries
    // to acquire the lock twice (even read-only), it throws "resource deadlock would occur".
    //
    // Despite exhaustive analysis, we could not eliminate ALL possible recursive
    // lock acquisitions across the complex callback chain (strategies -> triggers ->
    // actions -> group handlers -> back to strategies). Some edge case is still
    // triggering recursive acquisition.
    //
    // Solution: Use std::recursive_mutex which ALLOWS the same thread to acquire
    // the lock multiple times. This trades some performance (recursive_mutex is
    // slower than shared_mutex) for correctness and stability.
    //
    // Performance impact: Negligible - lock contention was already minimal, and
    // recursive_mutex overhead is only a few nanoseconds per acquisition.
    mutable std::recursive_mutex _mutex;

    // Debug tracking
    uint32 _lastDebugLogTime = 0;
};

// ========================================================================
// DEFAULT IMPLEMENTATION - For bots without specialized AI
// ========================================================================

class TC_GAME_API DefaultBotAI : public BotAI
{
public:
    explicit DefaultBotAI(Player* player) : BotAI(player) {}

    // Uses base UpdateAI() implementation
    // No combat specialization needed for default bots
};

// ========================================================================
// AI FACTORY - Creates appropriate AI for each class
// ========================================================================

class TC_GAME_API BotAIFactory
{
    BotAIFactory() = default;
    ~BotAIFactory() = default;
    BotAIFactory(BotAIFactory const&) = delete;
    BotAIFactory& operator=(BotAIFactory const&) = delete;

public:
    static BotAIFactory* instance();

    // AI creation
    std::unique_ptr<BotAI> CreateAI(Player* bot);
    std::unique_ptr<BotAI> CreateClassAI(Player* bot, uint8 classId);
    std::unique_ptr<BotAI> CreateClassAI(Player* bot, uint8 classId, uint8 spec);

    // Specialized AI creation
    std::unique_ptr<BotAI> CreateSpecializedAI(Player* bot, std::string const& type);
    std::unique_ptr<BotAI> CreatePvPAI(Player* bot);
    std::unique_ptr<BotAI> CreatePvEAI(Player* bot);
    std::unique_ptr<BotAI> CreateRaidAI(Player* bot);

    // AI registration
    void RegisterAICreator(std::string const& type,
                          std::function<std::unique_ptr<BotAI>(Player*)> creator);

    // Initialization
    void InitializeDefaultTriggers(BotAI* ai);
    void InitializeDefaultValues(BotAI* ai);

private:
    void InitializeDefaultStrategies(BotAI* ai);
    void InitializeClassStrategies(BotAI* ai, uint8 classId, uint8 spec);

    std::unordered_map<std::string, std::function<std::unique_ptr<BotAI>(Player*)>> _creators;
};

#define sBotAIFactory BotAIFactory::instance()

} // namespace Playerbot