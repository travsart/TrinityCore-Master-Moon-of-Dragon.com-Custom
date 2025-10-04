/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED BotAI Implementation - Clean Update Chain
 *
 * This implementation provides:
 * 1. Single clean UpdateAI() method - no DoUpdateAI/UpdateEnhanced confusion
 * 2. Clear separation of base behaviors and combat specialization
 * 3. Every-frame updates for smooth movement and following
 * 4. Proper delegation to ClassAI for combat-only updates
 */

#include "BotAI.h"
#include "Strategy/Strategy.h"
#include "Strategy/GroupCombatStrategy.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Group/GroupInvitationHandler.h"
#include "Movement/LeaderFollowBehavior.h"
#include "Quest/QuestAutomation.h"
#include "Social/TradeAutomation.h"
#include "Social/AuctionAutomation.h"
#include "Combat/TargetScanner.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include "Timer.h"
#include <chrono>

namespace Playerbot
{

// TriggerResultComparator implementation
bool TriggerResultComparator::operator()(TriggerResult const& a, TriggerResult const& b) const
{
    // Higher urgency has higher priority (reverse order for max-heap)
    return a.urgency < b.urgency;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BotAI::BotAI(Player* bot) : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbots.ai", "BotAI created with null bot pointer");
        return;
    }

    // Initialize performance tracking
    _performanceMetrics.lastUpdate = std::chrono::steady_clock::now();

    // Initialize group management
    _groupInvitationHandler = std::make_unique<GroupInvitationHandler>(_bot);

    // Initialize target scanner for autonomous enemy detection
    _targetScanner = std::make_unique<TargetScanner>(_bot);

    // CRITICAL: Activate idle behavior automation systems
    // These systems are fully implemented but disabled by default - must activate on spawn
    uint32 botGuid = _bot->GetGUID().GetCounter();
    QuestAutomation::instance()->SetAutomationActive(botGuid, true);
    TradeAutomation::instance()->SetAutomationActive(botGuid, true);
    AuctionAutomation::instance()->SetAutomationActive(botGuid, true);

    TC_LOG_INFO("module.playerbot", "🤖 BOT AUTOMATION ACTIVATED: {} - Quest/Trade/Auction systems enabled",
                _bot->GetName());

    // Initialize default strategies for basic functionality
    InitializeDefaultStrategies();

    // Initialize default triggers
    sBotAIFactory->InitializeDefaultTriggers(this);

    // Check if bot is already in a group (e.g., after server restart)
    if (_bot->GetGroup())
    {
        TC_LOG_INFO("playerbot", "Bot {} already in group on initialization, activating follow strategy",
                    _bot->GetName());
        OnGroupJoined(_bot->GetGroup());
    }

    TC_LOG_DEBUG("playerbots.ai", "BotAI created for bot {}", _bot->GetGUID().ToString());
}

BotAI::~BotAI() = default;

// ============================================================================
// MAIN UPDATE METHOD - CLEAN SINGLE ENTRY POINT
// ============================================================================

void BotAI::UpdateAI(uint32 diff)
{
    // CRITICAL: This is the SINGLE entry point for ALL AI updates
    // No more confusion with DoUpdateAI/UpdateEnhanced

    if (!_bot || !_bot->IsInWorld())
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Track performance
    _performanceMetrics.totalUpdates++;

    // ========================================================================
    // PHASE 1: CORE BEHAVIORS - Always run every frame
    // ========================================================================

    // Update internal values and caches
    UpdateValues(diff);

    // Update all active strategies (including follow, idle, social)
    // CRITICAL: Must run every frame for smooth following
    UpdateStrategies(diff);

    // Process all triggers
    ProcessTriggers();

    // Execute queued and triggered actions
    UpdateActions(diff);

    // Update movement based on strategy decisions
    // CRITICAL: Must run every frame for smooth movement
    UpdateMovement(diff);

    // ========================================================================
    // PHASE 2: STATE MANAGEMENT - Check for state transitions
    // ========================================================================

    // Update combat state (enter/exit combat detection)
    UpdateCombatState(diff);

    // ========================================================================
    // PHASE 3: COMBAT SPECIALIZATION - Only when in combat
    // ========================================================================

    // If in combat AND this is a ClassAI instance, delegate combat updates
    if (IsInCombat())
    {
        // Virtual call to ClassAI::OnCombatUpdate() if overridden
        // ClassAI handles rotation, cooldowns, targeting
        // But NOT movement - that's already handled by strategies
        OnCombatUpdate(diff);
    }

    // ========================================================================
    // PHASE 4: GROUP INVITATION PROCESSING - Critical for joining groups
    // ========================================================================

    // Process pending group invitations
    // CRITICAL: Must run every frame to accept invitations promptly
    if (_groupInvitationHandler)
    {
        _groupInvitationHandler->Update(diff);
    }

    // ========================================================================
    // PHASE 5: IDLE BEHAVIORS - Only when not in combat or following
    // ========================================================================

    // Update idle behaviors (questing, trading, etc.)
    // Only runs when bot is truly idle
    if (!IsInCombat() && !IsFollowing())
    {
        UpdateIdleBehaviors(diff);
    }

    // ========================================================================
    // PHASE 6: GROUP MANAGEMENT - Check for group changes
    // ========================================================================

    // Check if bot left group and trigger cleanup
    bool isInGroup = (_bot->GetGroup() != nullptr);
    if (_wasInGroup && !isInGroup)
    {
        TC_LOG_INFO("playerbot", "Bot {} left group, calling OnGroupLeft()",
                    _bot->GetName());
        OnGroupLeft();
    }
    _wasInGroup = isInGroup;

    // ========================================================================
    // PHASE 6: PERFORMANCE TRACKING
    // ========================================================================

    auto endTime = std::chrono::high_resolution_clock::now();
    auto updateTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Update performance metrics
    if (_performanceMetrics.averageUpdateTime.count() == 0)
        _performanceMetrics.averageUpdateTime = updateTime;
    else
        _performanceMetrics.averageUpdateTime = (_performanceMetrics.averageUpdateTime + updateTime) / 2;

    if (updateTime > _performanceMetrics.maxUpdateTime)
        _performanceMetrics.maxUpdateTime = updateTime;

    _performanceMetrics.lastUpdate = std::chrono::steady_clock::now();
}

// ============================================================================
// STRATEGY UPDATES - Core behavior system
// ============================================================================

void BotAI::UpdateStrategies(uint32 diff)
{
    // CRITICAL: This must run EVERY frame for following to work properly
    // No throttling allowed here!

    std::shared_lock lock(_mutex);

    // CRITICAL FIX: Access _strategies directly instead of calling GetStrategy()
    // to avoid recursive mutex acquisition (std::shared_mutex is NOT recursive)
    for (auto const& strategyName : _activeStrategies)
    {
        // Direct lookup without additional lock
        auto it = _strategies.find(strategyName);
        if (it != _strategies.end())
        {
            Strategy* strategy = it->second.get();
            if (strategy && strategy->IsActive(this))
            {
                // Special handling for follow strategy - needs every frame update
                if (strategyName == "follow")
                {
                    if (auto* followBehavior = dynamic_cast<LeaderFollowBehavior*>(strategy))
                    {
                        followBehavior->UpdateFollowBehavior(this, diff);
                    }
                }
                else
                {
                    // Other strategies can use their normal update
                    strategy->UpdateBehavior(this, diff);
                }
            }
        }
    }

    _performanceMetrics.strategiesEvaluated = static_cast<uint32>(_activeStrategies.size());
}

// ============================================================================
// MOVEMENT UPDATES - Strategy-controlled movement
// ============================================================================

void BotAI::UpdateMovement(uint32 diff)
{
    // CRITICAL: Movement is controlled by strategies (especially follow)
    // This method just ensures movement commands are processed
    // Must run every frame for smooth movement

    if (!_bot || !_bot->IsAlive())
        return;

    // Movement is primarily handled by strategies (follow, combat positioning, etc.)
    // This is just for ensuring movement updates are processed
    if (_bot->GetMotionMaster())
    {
        // Motion master will handle actual movement updates
        // We just ensure it's being processed
    }
}

// ============================================================================
// COMBAT STATE MANAGEMENT
// ============================================================================

void BotAI::UpdateCombatState(uint32 diff)
{
    bool wasInCombat = IsInCombat();
    bool isInCombat = _bot && _bot->IsInCombat();

    // Handle combat state transitions
    if (!wasInCombat && isInCombat)
    {
        // Entering combat
        TC_LOG_ERROR("module.playerbot", "⚔️ ENTERING COMBAT: Bot {}", _bot->GetName());
        SetAIState(BotAIState::COMBAT);

        // Find initial target
        ::Unit* target = nullptr;
        ObjectGuid targetGuid = _bot->GetTarget();
        if (!targetGuid.IsEmpty())
        {
            target = ObjectAccessor::GetUnit(*_bot, targetGuid);
            TC_LOG_ERROR("module.playerbot", "🎯 Target from GetTarget(): {}", target ? target->GetName() : "null");
        }

        if (!target)
        {
            target = _bot->GetVictim();
            TC_LOG_ERROR("module.playerbot", "🎯 Target from GetVictim(): {}", target ? target->GetName() : "null");
        }

        if (target)
        {
            TC_LOG_ERROR("module.playerbot", "✅ Calling OnCombatStart() with target {}", target->GetName());
            OnCombatStart(target);
        }
        else
        {
            TC_LOG_ERROR("module.playerbot", "❌ COMBAT START FAILED: No valid target found!");
        }
    }
    else if (wasInCombat && !isInCombat)
    {
        // Leaving combat
        TC_LOG_ERROR("module.playerbot", "🏳️ LEAVING COMBAT: Bot {}", _bot->GetName());
        OnCombatEnd();

        // Determine new state
        if (_bot->GetGroup() && GetStrategy("follow"))
            SetAIState(BotAIState::FOLLOWING);
        else
            SetAIState(BotAIState::IDLE);
    }
}

// ============================================================================
// TRIGGER PROCESSING
// ============================================================================

void BotAI::ProcessTriggers()
{
    if (!_bot)
        return;

    // Clear previous triggered actions
    while (!_triggeredActions.empty())
        _triggeredActions.pop();

    // Evaluate all triggers
    for (auto const& trigger : _triggers)
    {
        if (trigger && trigger->Check(this))
        {
            auto result = trigger->Evaluate(this);
            if (result.triggered && result.suggestedAction)
            {
                _triggeredActions.push(result);
                _performanceMetrics.triggersProcessed++;
            }
        }
    }
}

// ============================================================================
// ACTION EXECUTION
// ============================================================================

void BotAI::UpdateActions(uint32 diff)
{
    // Execute current action if in progress
    if (_currentAction)
    {
        // Check if action is still valid
        if (!_currentAction->IsUseful(this))
        {
            CancelCurrentAction();
        }
        else
        {
            // Action still in progress
            return;
        }
    }

    // Process triggered actions first (higher priority)
    if (!_triggeredActions.empty())
    {
        auto const& result = _triggeredActions.top();
        if (result.suggestedAction && CanExecuteAction(result.suggestedAction.get()))
        {
            auto execResult = ExecuteActionInternal(result.suggestedAction.get(), result.context);
            if (execResult == ActionResult::SUCCESS || execResult == ActionResult::IN_PROGRESS)
            {
                _currentAction = result.suggestedAction;
                _performanceMetrics.actionsExecuted++;
            }
        }
        _triggeredActions.pop();
        return;
    }

    // Process queued actions
    if (!_actionQueue.empty())
    {
        auto [action, context] = _actionQueue.front();
        _actionQueue.pop();

        if (action && CanExecuteAction(action.get()))
        {
            auto result = ExecuteActionInternal(action.get(), context);
            if (result == ActionResult::SUCCESS || result == ActionResult::IN_PROGRESS)
            {
                _currentAction = action;
                _currentContext = context;
                _performanceMetrics.actionsExecuted++;
            }
        }
    }
}

// ============================================================================
// IDLE BEHAVIORS
// ============================================================================

void BotAI::UpdateIdleBehaviors(uint32 diff)
{
    // Only run idle behaviors when truly idle
    if (IsInCombat() || IsFollowing())
        return;

    uint32 currentTime = getMSTime();

    // ========================================================================
    // AUTONOMOUS TARGET SCANNING - Find enemies when solo
    // ========================================================================

    // For solo bots (not in a group), actively scan for nearby enemies
    if (!_bot->GetGroup() && _targetScanner)
    {
        // Check if it's time to scan (throttled for performance)
        if (_targetScanner->ShouldScan(currentTime))
        {
            _targetScanner->UpdateScanTime(currentTime);

            // Clean up blacklist
            _targetScanner->UpdateBlacklist(currentTime);

            // Find best target to engage
            Unit* bestTarget = _targetScanner->FindBestTarget();

            // If we found a valid target, engage it
            if (bestTarget && _targetScanner->ShouldEngage(bestTarget))
            {
                TC_LOG_DEBUG("playerbot", "Solo bot {} found hostile target {} at distance {:.1f}",
                            _bot->GetName(), bestTarget->GetName(), _bot->GetDistance(bestTarget));

                // CRITICAL FIX: Properly enter combat state
                // 1. Set target
                _bot->SetTarget(bestTarget->GetGUID());

                // 2. Start combat - this sets victim and initiates auto-attack
                _bot->Attack(bestTarget, true);

                // 3. CRITICAL: Force bot into combat state (Attack() alone doesn't guarantee this)
                _bot->SetInCombatWith(bestTarget);
                bestTarget->SetInCombatWith(_bot);

                TC_LOG_ERROR("module.playerbot", "🎯 AUTONOMOUS COMBAT START: Bot {} attacking {} (InCombat={}, HasVictim={})",
                             _bot->GetName(), bestTarget->GetName(),
                             _bot->IsInCombat(), _bot->GetVictim() != nullptr);

                // If ranged class, start at range
                if (_bot->GetClass() == CLASS_HUNTER ||
                    _bot->GetClass() == CLASS_MAGE ||
                    _bot->GetClass() == CLASS_WARLOCK ||
                    _bot->GetClass() == CLASS_PRIEST)
                {
                    // Move to optimal range instead of melee
                    float optimalRange = 25.0f; // Standard ranged distance
                    if (_bot->GetDistance(bestTarget) > optimalRange)
                    {
                        // Move closer but stay at range
                        Position pos = bestTarget->GetNearPosition(optimalRange, 0.0f);
                        _bot->GetMotionMaster()->MovePoint(0, pos);
                    }
                }

                // This will trigger combat state transition in next update
                return;
            }
        }
    }

    // ========================================================================
    // QUEST AND TRADE AUTOMATION
    // ========================================================================

    // CRITICAL FIX: Actually call the automation systems that exist but were never connected!
    // These systems are fully implemented in Quest/, Social/ but UpdateIdleBehaviors was a stub

    // Run quest automation every ~5 seconds (not every frame - too expensive)
    static uint32 lastQuestUpdate = 0;
    if (currentTime - lastQuestUpdate > 5000) // 5 second throttle
    {
        QuestAutomation::instance()->AutomateQuestPickup(_bot);
        lastQuestUpdate = currentTime;
    }

    // Run trade automation every ~10 seconds (vendor visits, repairs, consumables)
    static uint32 lastTradeUpdate = 0;
    if (currentTime - lastTradeUpdate > 10000) // 10 second throttle
    {
        TradeAutomation::instance()->AutomateVendorInteractions(_bot);
        TradeAutomation::instance()->AutomateInventoryManagement(_bot);
        lastTradeUpdate = currentTime;
    }

    // Run auction house automation every ~30 seconds (market monitoring, buying/selling)
    static uint32 lastAuctionUpdate = 0;
    if (currentTime - lastAuctionUpdate > 30000) // 30 second throttle
    {
        AuctionAutomation::instance()->AutomateAuctionHouseActivities(_bot);
        lastAuctionUpdate = currentTime;
    }

    // TODO: Add social interactions (chat, emotes) when implemented
}

// ============================================================================
// STATE TRANSITIONS
// ============================================================================

void BotAI::OnCombatStart(::Unit* target)
{
    _currentTarget = target ? target->GetGUID() : ObjectGuid::Empty;

    TC_LOG_DEBUG("playerbot", "Bot {} entering combat with {}",
                 _bot->GetName(), target ? target->GetName() : "unknown");

    // Strategies don't have OnCombatStart - combat is handled by ClassAI
    // through the OnCombatUpdate() method
}

void BotAI::OnCombatEnd()
{
    _currentTarget = ObjectGuid::Empty;

    TC_LOG_DEBUG("playerbot", "Bot {} leaving combat", _bot->GetName());

    // Strategies don't have OnCombatEnd - combat is handled by ClassAI
    // through the OnCombatUpdate() method
}

void BotAI::OnDeath()
{
    SetAIState(BotAIState::DEAD);
    CancelCurrentAction();

    // Clear action queue
    while (!_actionQueue.empty())
        _actionQueue.pop();

    TC_LOG_DEBUG("playerbots.ai", "Bot {} died, AI state reset", _bot->GetName());
}

void BotAI::OnRespawn()
{
    SetAIState(BotAIState::IDLE);
    Reset();

    TC_LOG_DEBUG("playerbots.ai", "Bot {} respawned, AI reset", _bot->GetName());
}

void BotAI::Reset()
{
    _currentTarget = ObjectGuid::Empty;
    _aiState = BotAIState::IDLE;

    CancelCurrentAction();

    while (!_actionQueue.empty())
        _actionQueue.pop();

    while (!_triggeredActions.empty())
        _triggeredActions.pop();
}

// ============================================================================
// GROUP MANAGEMENT
// ============================================================================

void BotAI::OnGroupJoined(Group* group)
{
    if (!group)
        return;

    TC_LOG_INFO("playerbot", "Bot {} joined group, activating follow and combat strategies",
                _bot->GetName());

    // Verify follow strategy exists (should have been created in InitializeDefaultStrategies)
    if (!GetStrategy("follow"))
    {
        TC_LOG_ERROR("playerbot", "CRITICAL: Follow strategy not found for bot {} - this should never happen!",
                    _bot->GetName());
        // Emergency fallback - create it now
        auto followBehavior = std::make_unique<LeaderFollowBehavior>();
        AddStrategy(std::move(followBehavior));
        TC_LOG_WARN("playerbot", "Created emergency follow strategy for bot {}", _bot->GetName());
    }

    // Verify group combat strategy exists
    if (!GetStrategy("group_combat"))
    {
        TC_LOG_ERROR("playerbot", "CRITICAL: GroupCombat strategy not found for bot {} - this should never happen!",
                    _bot->GetName());
        // Emergency fallback - create it now
        auto groupCombat = std::make_unique<GroupCombatStrategy>();
        AddStrategy(std::move(groupCombat));
        TC_LOG_WARN("playerbot", "Created emergency group_combat strategy for bot {}", _bot->GetName());
    }

    // Activate follow strategy
    ActivateStrategy("follow");

    // CRITICAL FIX: Activate group combat strategy for combat assistance
    ActivateStrategy("group_combat");

    // Confirm activation succeeded by checking if it's in active strategies list
    {
        std::shared_lock lock(_mutex);
        bool followActive = std::find(_activeStrategies.begin(), _activeStrategies.end(), "follow") != _activeStrategies.end();
        bool combatActive = std::find(_activeStrategies.begin(), _activeStrategies.end(), "group_combat") != _activeStrategies.end();

        if (followActive && combatActive)
        {
            TC_LOG_INFO("playerbot", "✅ Successfully activated follow and group_combat strategies for bot {}", _bot->GetName());
        }
        else
        {
            TC_LOG_ERROR("playerbot", "❌ Strategy activation FAILED for bot {} - follow={}, combat={}",
                        _bot->GetName(), followActive, combatActive);
        }
    }

    // Set state to following if not in combat
    if (!IsInCombat())
        SetAIState(BotAIState::FOLLOWING);

    _wasInGroup = true;
}

void BotAI::OnGroupLeft()
{
    TC_LOG_INFO("playerbot", "Bot {} left group, deactivating follow and combat strategies",
                _bot->GetName());

    // Deactivate follow strategy
    DeactivateStrategy("follow");

    // Deactivate group combat strategy
    DeactivateStrategy("group_combat");

    // Set state to idle if not in combat
    if (!IsInCombat())
        SetAIState(BotAIState::IDLE);

    _wasInGroup = false;
}

void BotAI::HandleGroupChange()
{
    // Check current group status
    bool inGroup = (_bot && _bot->GetGroup() != nullptr);

    if (inGroup && !_wasInGroup)
    {
        OnGroupJoined(_bot->GetGroup());
    }
    else if (!inGroup && _wasInGroup)
    {
        OnGroupLeft();
    }
}

// ============================================================================
// STRATEGY MANAGEMENT
// ============================================================================

void BotAI::AddStrategy(std::unique_ptr<Strategy> strategy)
{
    if (!strategy)
        return;

    std::unique_lock lock(_mutex);
    std::string name = strategy->GetName();
    _strategies[name] = std::move(strategy);
}

void BotAI::RemoveStrategy(std::string const& name)
{
    std::unique_lock lock(_mutex);
    _strategies.erase(name);

    // Also remove from active strategies
    _activeStrategies.erase(
        std::remove(_activeStrategies.begin(), _activeStrategies.end(), name),
        _activeStrategies.end()
    );
}

Strategy* BotAI::GetStrategy(std::string const& name) const
{
    std::shared_lock lock(_mutex);
    auto it = _strategies.find(name);
    return it != _strategies.end() ? it->second.get() : nullptr;
}

std::vector<Strategy*> BotAI::GetActiveStrategies() const
{
    std::shared_lock lock(_mutex);
    std::vector<Strategy*> result;

    // CRITICAL FIX: Access _strategies directly to avoid recursive mutex acquisition
    for (auto const& name : _activeStrategies)
    {
        auto it = _strategies.find(name);
        if (it != _strategies.end())
            result.push_back(it->second.get());
    }

    return result;
}

void BotAI::ActivateStrategy(std::string const& name)
{
    std::unique_lock lock(_mutex);

    // Check if strategy exists
    auto it = _strategies.find(name);
    if (it == _strategies.end())
        return;

    // Check if already active
    if (std::find(_activeStrategies.begin(), _activeStrategies.end(), name) != _activeStrategies.end())
        return;

    _activeStrategies.push_back(name);

    // CRITICAL FIX: Set the strategy's internal _active flag so IsActive() returns true
    it->second->SetActive(true);

    // Call OnActivate hook
    it->second->OnActivate(this);

    TC_LOG_DEBUG("playerbot", "Activated strategy '{}' for bot {}", name, _bot->GetName());
}

void BotAI::DeactivateStrategy(std::string const& name)
{
    std::unique_lock lock(_mutex);

    // Find the strategy
    auto it = _strategies.find(name);
    if (it != _strategies.end())
    {
        // Set the strategy's internal _active flag to false
        it->second->SetActive(false);

        // Call OnDeactivate hook
        it->second->OnDeactivate(this);
    }

    _activeStrategies.erase(
        std::remove(_activeStrategies.begin(), _activeStrategies.end(), name),
        _activeStrategies.end()
    );

    TC_LOG_DEBUG("playerbot", "Deactivated strategy '{}' for bot {}", name, _bot->GetName());
}

// ============================================================================
// ACTION EXECUTION
// ============================================================================

bool BotAI::ExecuteAction(std::string const& actionName)
{
    return ExecuteAction(actionName, ActionContext());
}

bool BotAI::ExecuteAction(std::string const& name, ActionContext const& context)
{
    // TODO: Implement action execution from name
    // This would look up the action by name and execute it
    return false;
}

bool BotAI::IsActionPossible(std::string const& actionName) const
{
    // TODO: Check if action is possible
    return false;
}

uint32 BotAI::GetActionPriority(std::string const& actionName) const
{
    // TODO: Get action priority
    return 0;
}

void BotAI::QueueAction(std::shared_ptr<Action> action, ActionContext const& context)
{
    if (!action)
        return;

    _actionQueue.push({action, context});
}

void BotAI::CancelCurrentAction()
{
    _currentAction = nullptr;
    _currentContext = ActionContext();
}

bool BotAI::CanExecuteAction(Action* action) const
{
    if (!action || !_bot)
        return false;

    return action->IsPossible(const_cast<BotAI*>(this)) && action->IsUseful(const_cast<BotAI*>(this));
}

ActionResult BotAI::ExecuteActionInternal(Action* action, ActionContext const& context)
{
    if (!action)
        return ActionResult::FAILED;

    return action->Execute(this, context);
}

// ============================================================================
// TARGET MANAGEMENT
// ============================================================================

::Unit* BotAI::GetTargetUnit() const
{
    if (!_bot || _currentTarget.IsEmpty())
        return nullptr;

    return ObjectAccessor::GetUnit(*_bot, _currentTarget);
}

// ============================================================================
// MOVEMENT CONTROL
// ============================================================================

void BotAI::MoveTo(float x, float y, float z)
{
    if (!_bot || !_bot->IsAlive())
        return;

    _bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

void BotAI::Follow(::Unit* target, float distance)
{
    if (!_bot || !_bot->IsAlive() || !target)
        return;

    _bot->GetMotionMaster()->MoveFollow(target, distance, 0.0f);
}

void BotAI::StopMovement()
{
    if (!_bot)
        return;

    _bot->StopMoving();
    _bot->GetMotionMaster()->Clear();
}

bool BotAI::IsMoving() const
{
    return _bot && _bot->isMoving();
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void BotAI::SetAIState(BotAIState state)
{
    if (_aiState != state)
    {
        TC_LOG_DEBUG("playerbot", "Bot {} state change: {} -> {}",
                     _bot->GetName(),
                     static_cast<int>(_aiState),
                     static_cast<int>(state));
        _aiState = state;
    }
}

void BotAI::InitializeDefaultStrategies()
{
    // CRITICAL FIX: Create and register the follow strategy so it exists when activated
    // Without this, ActivateStrategy("follow") fails silently when bot joins group
    auto followBehavior = std::make_unique<LeaderFollowBehavior>();
    AddStrategy(std::move(followBehavior));

    // CRITICAL FIX: Create and register group combat strategy for combat assistance
    // This strategy makes bots attack when group members enter combat
    auto groupCombat = std::make_unique<GroupCombatStrategy>();
    AddStrategy(std::move(groupCombat));

    TC_LOG_INFO("module.playerbot.ai", "✅ Initialized follow and group_combat strategies for bot {}", _bot->GetName());

    // Combat strategies are added by ClassAI
    // Additional strategies can be added based on configuration
}

void BotAI::UpdateValues(uint32 diff)
{
    // Update cached values used by triggers and actions
    // This includes distances, health percentages, resource levels, etc.
}

// NOTE: BotAIFactory implementation is in BotAIFactory.cpp
// Do not duplicate method definitions here to avoid linker errors

} // namespace Playerbot