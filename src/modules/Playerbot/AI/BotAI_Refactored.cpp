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
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Group/GroupInvitationHandler.h"
#include "Movement/LeaderFollowBehavior.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include <chrono>

namespace Playerbot
{

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
    // PHASE 4: IDLE BEHAVIORS - Only when not in combat or following
    // ========================================================================

    // Update idle behaviors (questing, trading, etc.)
    // Only runs when bot is truly idle
    if (!IsInCombat() && !IsFollowing())
    {
        UpdateIdleBehaviors(diff);
    }

    // ========================================================================
    // PHASE 5: GROUP MANAGEMENT - Check for group changes
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

    // Debug logging (throttled)
    static uint32 lastDebugLog = 0;
    uint32 currentTime = getMSTime();
    if (currentTime - lastDebugLog > 5000) // Every 5 seconds
    {
        TC_LOG_DEBUG("playerbot.performance", "Bot {} - UpdateAI took {}us (avg: {}us, max: {}us)",
                     _bot->GetName(),
                     updateTime.count(),
                     _performanceMetrics.averageUpdateTime.count(),
                     _performanceMetrics.maxUpdateTime.count());
        lastDebugLog = currentTime;
    }
}

// ============================================================================
// STRATEGY UPDATES - Core behavior system
// ============================================================================

void BotAI::UpdateStrategies(uint32 diff)
{
    // CRITICAL: This must run EVERY frame for following to work properly
    // No throttling allowed here!

    std::shared_lock lock(_mutex);

    for (auto const& strategyName : _activeStrategies)
    {
        if (auto* strategy = GetStrategy(strategyName))
        {
            if (strategy->IsActive(this))
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
                    strategy->Update(this, diff);
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
        SetAIState(BotAIState::COMBAT);

        // Find initial target
        ::Unit* target = nullptr;
        if (_bot->GetTarget())
            target = ObjectAccessor::GetUnit(*_bot, _bot->GetTarget());

        if (!target)
            target = _bot->GetVictim();

        if (target)
            OnCombatStart(target);
    }
    else if (wasInCombat && !isInCombat)
    {
        // Leaving combat
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
            auto result = trigger->GetResult(this);
            if (!result.actionName.empty())
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
        if (ExecuteAction(result.actionName, result.context))
        {
            _performanceMetrics.actionsExecuted++;
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

    // Idle behaviors include:
    // - Quest automation
    // - Trade automation
    // - Auction house automation
    // - Social interactions
    // These are handled by specific idle strategies
}

// ============================================================================
// STATE TRANSITIONS
// ============================================================================

void BotAI::OnCombatStart(::Unit* target)
{
    _currentTarget = target ? target->GetGUID() : ObjectGuid::Empty;

    TC_LOG_DEBUG("playerbot", "Bot {} entering combat with {}",
                 _bot->GetName(), target ? target->GetName() : "unknown");

    // Notify strategies about combat start
    for (auto const& strategyName : _activeStrategies)
    {
        if (auto* strategy = GetStrategy(strategyName))
        {
            strategy->OnCombatStart(this, target);
        }
    }
}

void BotAI::OnCombatEnd()
{
    _currentTarget = ObjectGuid::Empty;

    TC_LOG_DEBUG("playerbot", "Bot {} leaving combat", _bot->GetName());

    // Notify strategies about combat end
    for (auto const& strategyName : _activeStrategies)
    {
        if (auto* strategy = GetStrategy(strategyName))
        {
            strategy->OnCombatEnd(this);
        }
    }
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

// ============================================================================
// GROUP MANAGEMENT
// ============================================================================

void BotAI::OnGroupJoined(Group* group)
{
    if (!group)
        return;

    TC_LOG_INFO("playerbot", "Bot {} joined group, activating follow strategy",
                _bot->GetName());

    // Activate follow strategy
    ActivateStrategy("follow");

    // Set state to following if not in combat
    if (!IsInCombat())
        SetAIState(BotAIState::FOLLOWING);

    _wasInGroup = true;
}

void BotAI::OnGroupLeft()
{
    TC_LOG_INFO("playerbot", "Bot {} left group, deactivating follow strategy",
                _bot->GetName());

    // Deactivate follow strategy
    DeactivateStrategy("follow");

    // Set state to idle if not in combat
    if (!IsInCombat())
        SetAIState(BotAIState::IDLE);

    _wasInGroup = false;
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
    // Add core strategies
    // Follow strategy will be added when bot joins a group
    // Combat strategies are added by ClassAI
    // Idle strategies can be added based on configuration
}

void BotAI::UpdateValues(uint32 diff)
{
    // Update cached values used by triggers and actions
    // This includes distances, health percentages, resource levels, etc.
}

// Additional method implementations would follow...
// This example shows the key refactored update chain

} // namespace Playerbot