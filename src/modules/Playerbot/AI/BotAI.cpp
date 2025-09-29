/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Combat/ThreatManager.h removed - not used in this file
#include "BotAI.h"
#include "Strategy/Strategy.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Values/Value.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Chat.h"
#include "SharedDefines.h"
#include "Opcodes.h"
#include "Log.h"
#include "Group/GroupInvitationHandler.h"
#include "Movement/LeaderFollowBehavior.h"
#include "Combat/GroupCombatTrigger.h"

namespace Playerbot
{

// Forward declarations for strategy classes
class IdleStrategy;
class FollowStrategy;

// TriggerResultComparator implementation
bool TriggerResultComparator::operator()(TriggerResult const& a, TriggerResult const& b) const
{
    // Higher urgency has higher priority (reverse order for max-heap)
    return a.urgency < b.urgency;
}

// BotAI implementation
BotAI::BotAI(Player* bot) : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbots.ai", "BotAI created with null bot pointer");
        return;
    }

    _state.lastStateChange = std::chrono::steady_clock::now();
    _performanceData.lastUpdate = std::chrono::steady_clock::now();

    // Initialize group management components
    _groupInvitationHandler = std::make_unique<GroupInvitationHandler>(_bot);

    // Initialize default strategies for basic bot functionality
    InitializeDefaultStrategies();

    // Initialize default triggers for automation and basic AI
    sBotAIFactory->InitializeDefaultTriggers(this);

    TC_LOG_DEBUG("playerbots.ai", "BotAI created for bot {}", _bot ? _bot->GetGUID().ToString() : "null");
}

// Destructor - required for unique_ptr with forward declaration
BotAI::~BotAI() = default;

void BotAI::UpdateAI(uint32 diff)
{
    // CRITICAL DEBUG: Test if this method is even being called properly
    static uint32 testCount = 0;
    testCount++;

    // Debug early exit conditions
    static uint32 lastErrorLogTime = 0;
    uint32 currentTime = getMSTime();

    if (!_enabled || !_bot)
    {
        if (currentTime - lastErrorLogTime > 10000) // Every 10 seconds
        {
            // Use the same logging mechanism as other parts of the system
            TC_LOG_INFO("module.playerbot.debug", "🔍 CRITICAL DEBUG: BotAI::UpdateAI early exit - enabled: {}, bot: {}, call count: {}",
                _enabled.load() ? "true" : "false",
                _bot ? _bot->GetName() : "null", testCount);
            lastErrorLogTime = currentTime;
        }
        return;
    }

    // CRITICAL DEBUG: We made it past the early exit!
    static uint32 successCount = 0;
    successCount++;

    // Debug: Log UpdateAI calls periodically
    static uint32 lastLogTime = 0;
    static uint32 totalUpdates = 0;
    totalUpdates++;

    uint32 updateTime = getMSTime();
    if (updateTime - lastLogTime > 10000) // Every 10 seconds
    {
        TC_LOG_INFO("module.playerbot.debug", "✅ CRITICAL DEBUG: BotAI::UpdateAI success for bot {} (total updates: {}, enabled: {}, diff: {}ms, success count: {})",
            _bot->GetName(), totalUpdates, _enabled.load(), diff, successCount);
        lastLogTime = updateTime;
    }

    auto startTime = std::chrono::steady_clock::now();

    _timeSinceLastUpdate += diff;

    // Update group invitation handler (always update for quick response)
    if (_groupInvitationHandler)
    {
        // Update group invitation handler silently

        _groupInvitationHandler->Update(diff);
    }

    // Only update at specified interval
    if (_timeSinceLastUpdate < _updateIntervalMs)
        return;

    // Reset the update timer
    _timeSinceLastUpdate = 0;

    try
    {
        DoUpdateAI(diff);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots.ai", "Exception in BotAI::UpdateAI for bot {}: {}",
                     _bot->GetName(), e.what());
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    UpdatePerformanceMetrics(static_cast<uint32>(duration.count()));
}

AIUpdateResult BotAI::UpdateEnhanced(uint32 diff)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    AIUpdateResult result;

    if (!_enabled || !_bot)
        return result;

    // DEBUG: Log UpdateEnhanced calls
    static uint32 lastEnhancedLog = 0;
    static uint32 enhancedCallCount = 0;
    enhancedCallCount++;
    uint32 currentTime = getMSTime();
    if (currentTime - lastEnhancedLog > 5000) // Every 5 seconds
    {
        TC_LOG_INFO("module.playerbot.combat", "UpdateEnhanced called for {} - Count: {}, Enabled: {}, InCombat: {}",
                    _bot->GetName(), enhancedCallCount, _enabled.load() ? "Yes" : "No",
                    _bot->IsInCombat() ? "Yes" : "No");
        lastEnhancedLog = currentTime;
    }

    // Update group invitation handler (always update for quick response)
    if (_groupInvitationHandler)
    {
        _groupInvitationHandler->Update(diff);
    }

    // Check if update is needed
    _lastUpdate += diff;
    if (_lastUpdate < _updateIntervalMs)
        return result;

    _lastUpdate = 0;
    _enhancedMetrics.totalUpdates++;

    std::shared_lock lock(_mutex);

    try
    {
        // Update subsystems
        UpdateValuesInternal(diff);
        UpdateStrategies(diff);

        // Process triggers
        ProcessTriggers();
        result.triggersChecked = static_cast<uint32>(_triggers.size());

        // Execute actions
        UpdateActions(diff);

        // Update movement
        UpdateMovement(diff);

        // Always update combat to check state transitions
        UpdateCombat(diff);

        // Calculate metrics
        auto endTime = std::chrono::high_resolution_clock::now();
        result.updateTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

        // Update performance metrics
        if (_enhancedMetrics.averageUpdateTime.count() == 0)
            _enhancedMetrics.averageUpdateTime = result.updateTime;
        else
            _enhancedMetrics.averageUpdateTime = (_enhancedMetrics.averageUpdateTime + result.updateTime) / 2;

        if (result.updateTime > _enhancedMetrics.maxUpdateTime)
            _enhancedMetrics.maxUpdateTime = result.updateTime;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("playerbots.ai", "Exception in BotAI::UpdateEnhanced for bot {}: {}",
                     _bot->GetName(), e.what());
    }

    return result;
}


void BotAI::OnDeath()
{
    SetAIState(BotAIState::DEAD);
    CancelCurrentAction();

    // Clear action queue
    while (!_actionQueue.empty())
        _actionQueue.pop();

    TC_LOG_DEBUG("playerbots.ai", "Bot {} died, AI state reset", _bot ? _bot->GetName() : "null");
}

void BotAI::OnRespawn()
{
    SetAIState(BotAIState::IDLE);
    Reset();

    TC_LOG_DEBUG("playerbots.ai", "Bot {} respawned, AI reset", _bot ? _bot->GetName() : "null");
}

void BotAI::Reset()
{
    _state.currentState = AIState::STATE_IDLE;
    _state.lastStateChange = std::chrono::steady_clock::now();
    _state.stateData.clear();
    _state.stateCounter = 0;

    _selectedAction.clear();
    _selectedActionScore = 0.0f;

    // Reset all strategies
    for (auto& [name, strategy] : _strategies)
    {
        if (strategy && strategy->IsActive(this))
            strategy->OnDeactivate(this);
        strategy->SetActive(false);
    }

    _activeStrategies.clear();

    TC_LOG_DEBUG("playerbots.ai", "BotAI reset for bot {}", _bot ? _bot->GetName() : "null");
}

void BotAI::AddStrategy(std::unique_ptr<Strategy> strategy)
{
    if (!strategy)
        return;

    std::string name = strategy->GetName();
    _strategies[name] = std::move(strategy);

    TC_LOG_DEBUG("playerbots.ai", "Added strategy '{}' to bot {}",
                 name, _bot ? _bot->GetName() : "null");
}

void BotAI::RemoveStrategy(std::string const& name)
{
    auto it = _strategies.find(name);
    if (it != _strategies.end())
    {
        if (it->second && it->second->IsActive(this))
        {
            it->second->OnDeactivate(this);
            it->second->SetActive(false);
        }

        _strategies.erase(it);

        // Remove from active strategies
        auto activeIt = std::find(_activeStrategies.begin(), _activeStrategies.end(), name);
        if (activeIt != _activeStrategies.end())
            _activeStrategies.erase(activeIt);

        TC_LOG_DEBUG("playerbots.ai", "Removed strategy '{}' from bot {}",
                     name, _bot ? _bot->GetName() : "null");
    }
}

Strategy* BotAI::GetStrategy(std::string const& name) const
{
    auto it = _strategies.find(name);
    return (it != _strategies.end()) ? it->second.get() : nullptr;
}

std::vector<Strategy*> BotAI::GetActiveStrategies() const
{
    std::vector<Strategy*> activeStrategies;

    for (std::string const& name : _activeStrategies)
    {
        if (Strategy* strategy = GetStrategy(name))
            activeStrategies.push_back(strategy);
    }

    return activeStrategies;
}

void BotAI::ActivateStrategy(std::string const& name)
{
    Strategy* strategy = GetStrategy(name);
    if (!strategy)
        return;

    if (!strategy->IsActive(this))
    {
        strategy->SetActive(true);
        strategy->OnActivate(this);

        if (std::find(_activeStrategies.begin(), _activeStrategies.end(), name) == _activeStrategies.end())
            _activeStrategies.push_back(name);

        TC_LOG_DEBUG("playerbots.ai", "Activated strategy '{}' for bot {}",
                     name, _bot ? _bot->GetName() : "null");
    }
}

void BotAI::DeactivateStrategy(std::string const& name)
{
    Strategy* strategy = GetStrategy(name);
    if (!strategy)
        return;

    if (strategy->IsActive(this))
    {
        strategy->OnDeactivate(this);
        strategy->SetActive(false);

        auto it = std::find(_activeStrategies.begin(), _activeStrategies.end(), name);
        if (it != _activeStrategies.end())
            _activeStrategies.erase(it);

        TC_LOG_DEBUG("playerbots.ai", "Deactivated strategy '{}' for bot {}",
                     name, _bot ? _bot->GetName() : "null");
    }
}

bool BotAI::ExecuteAction(std::string const& actionName)
{
    if (!_bot || actionName.empty())
        return false;

    // Find action in active strategies
    for (Strategy* strategy : GetActiveStrategies())
    {
        if (std::shared_ptr<Action> action = strategy->GetAction(actionName))
        {
            if (action->IsPossible(this))
            {
                bool result = action->Execute(this);
                if (result)
                {
                    _performanceData.actionsExecuted++;
                    _lastActionTime = std::chrono::steady_clock::now();
                    LogAIDecision(actionName, 1.0f);
                }
                return result;
            }
        }
    }

    return false;
}

bool BotAI::IsActionPossible(std::string const& actionName) const
{
    if (!_bot || actionName.empty())
        return false;

    // Check if action is possible in any active strategy
    for (Strategy* strategy : GetActiveStrategies())
    {
        if (std::shared_ptr<Action> action = strategy->GetAction(actionName))
        {
            if (action->IsPossible(const_cast<BotAI*>(this)))
                return true;
        }
    }

    return false;
}

uint32 BotAI::GetActionPriority(std::string const& actionName) const
{
    uint32 highestPriority = 0;

    for (Strategy* strategy : GetActiveStrategies())
    {
        if (std::shared_ptr<Action> action = strategy->GetAction(actionName))
        {
            uint32 priority = action->GetPriority();
            if (priority > highestPriority)
                highestPriority = priority;
        }
    }

    return highestPriority;
}

void BotAI::SetState(AIState::Type newState, std::string const& data)
{
    if (_state.currentState != newState)
    {
        AIState::Type oldState = _state.currentState;
        _state.currentState = newState;
        _state.lastStateChange = std::chrono::steady_clock::now();
        _state.stateData = data;
        _state.stateCounter++;

        TC_LOG_DEBUG("playerbots.ai", "Bot {} state changed from {} to {} ({})",
                     _bot ? _bot->GetGUID().ToString() : "null", static_cast<int>(oldState), static_cast<int>(newState), data);
    }
}

float BotAI::EvaluateAction(std::string const& actionName) const
{
    if (!_bot || actionName.empty())
        return 0.0f;

    float bestScore = 0.0f;

    for (Strategy* strategy : GetActiveStrategies())
    {
        if (std::shared_ptr<Action> action = strategy->GetAction(actionName))
        {
            if (action->IsPossible(const_cast<BotAI*>(this)))
            {
                float score = action->GetRelevance(const_cast<BotAI*>(this));
                if (score > bestScore)
                    bestScore = score;
            }
        }
    }

    return bestScore;
}

std::string BotAI::SelectBestAction() const
{
    std::string bestAction;
    float bestScore = 0.0f;

    for (Strategy* strategy : GetActiveStrategies())
    {
        auto actions = strategy->GetActions();
        for (auto const& action : actions)
        {
            if (action && action->IsPossible(const_cast<BotAI*>(this)))
            {
                float score = action->GetRelevance(const_cast<BotAI*>(this));
                if (score > bestScore)
                {
                    bestScore = score;
                    bestAction = action->GetName();
                }
            }
        }
    }

    return bestAction;
}

void BotAI::UpdateStrategies()
{
    if (!_bot)
        return;

    _performanceData.strategiesEvaluated++;

    // Evaluate all strategies and activate/deactivate based on relevance
    for (auto& [name, strategy] : _strategies)
    {
        if (!strategy)
            continue;

        float relevance = strategy->GetRelevance(this);
        bool shouldBeActive = relevance > 10.0f; // Threshold for activation

        if (shouldBeActive && !strategy->IsActive(this))
        {
            ActivateStrategy(name);
        }
        else if (!shouldBeActive && strategy->IsActive(this))
        {
            DeactivateStrategy(name);
        }
    }
}

void BotAI::DoUpdateAI(uint32 diff)
{
    if (!_bot)
        return;

    // Update strategies based on current situation
    UpdateStrategies();

    // Update leader follow behavior if active
    if (auto followStrategy = GetStrategy("leader_follow"))
    {
        if (followStrategy->IsActive(this))
        {
            if (auto followBehavior = dynamic_cast<LeaderFollowBehavior*>(followStrategy))
            {
                followBehavior->UpdateFollowBehavior(this, diff);
            }
        }
    }

    // Process triggers from active strategies
    ProcessTriggers();

    // Select and execute the best action
    ExecuteSelectedAction();
}

void BotAI::EvaluateStrategies()
{
    // This method can be overridden by derived classes for custom strategy evaluation
    UpdateStrategies();
}

void BotAI::ProcessTriggers()
{
    if (!_bot)
        return;

    // CRITICAL DEBUG: Log trigger processing
    static uint32 lastDebugLog = 0;
    uint32 currentTime = getMSTime();
    bool shouldLog = (currentTime - lastDebugLog > 3000); // Log every 3 seconds

    if (shouldLog)
    {
        TC_LOG_INFO("module.playerbot.combat", "ProcessTriggers for {} - Trigger count: {}, InCombat: {}, Group: {}",
                    _bot->GetName(), _triggers.size(),
                    _bot->IsInCombat() ? "Yes" : "No",
                    _bot->GetGroup() ? "Yes" : "No");
        lastDebugLog = currentTime;
    }

    // CRITICAL FIX: Process direct triggers registered with BotAI first
    for (auto const& trigger : _triggers)
    {
        if (!trigger)
            continue;

        // Log each trigger check for debugging
        bool checkResult = trigger->Check(this);
        if (shouldLog)
        {
            TC_LOG_DEBUG("module.playerbot.combat", "  Checking trigger '{}' for {} - Result: {}",
                        trigger->GetName(), _bot->GetName(), checkResult ? "TRUE" : "false");
        }

        if (checkResult)
        {
            TriggerResult result = trigger->Evaluate(this);
            if (result.triggered && result.suggestedAction)
            {
                TC_LOG_INFO("module.playerbot.combat", "TRIGGER FIRED: '{}' for bot {} - Executing action '{}'",
                            trigger->GetName(), _bot->GetName(), result.suggestedAction->GetName());

                // Queue the action for execution
                QueueAction(result.suggestedAction, result.context);

                // Also try immediate execution
                ActionResult actionResult = result.suggestedAction->Execute(this, result.context);
                if (actionResult == ActionResult::SUCCESS)
                {
                    TC_LOG_INFO("module.playerbot.combat", "SUCCESS: Executed action from trigger {} for bot {}",
                                trigger->GetName(), _bot->GetName());
                    return; // Execute only one action per update
                }
                else
                {
                    TC_LOG_DEBUG("module.playerbot.combat", "Action execution returned: {} for trigger {} on bot {}",
                                static_cast<int>(actionResult), trigger->GetName(), _bot->GetName());
                }
            }
        }
    }

    // Process triggers from all active strategies
    for (Strategy* strategy : GetActiveStrategies())
    {
        auto triggers = strategy->GetTriggers();
        for (auto const& trigger : triggers)
        {
            if (trigger && trigger->IsActive(this))
            {
                std::string actionName = trigger->GetActionName();
                if (!actionName.empty() && IsActionPossible(actionName))
                {
                    ExecuteAction(actionName);
                    break; // Execute only one action per update
                }
            }
        }
    }
}

void BotAI::ExecuteSelectedAction()
{
    if (!_bot)
        return;

    // Select the best action if we don't have one
    if (_selectedAction.empty())
    {
        _selectedAction = SelectBestAction();
        _selectedActionScore = EvaluateAction(_selectedAction);
    }

    // Execute the selected action
    if (!_selectedAction.empty() && IsActionPossible(_selectedAction))
    {
        if (ExecuteAction(_selectedAction))
        {
            // Action executed successfully, clear selection for next update
            _selectedAction.clear();
            _selectedActionScore = 0.0f;
        }
    }
    else
    {
        // Action no longer possible, select a new one
        _selectedAction.clear();
        _selectedActionScore = 0.0f;
    }
}

void BotAI::UpdatePerformanceMetrics(uint32 updateTimeMs)
{
    _performanceData.lastUpdateTimeMs = updateTimeMs;

    // Simple moving average for update time
    static constexpr float ALPHA = 0.1f; // Smoothing factor
    uint32 currentAverage = _performanceData.averageUpdateTimeMs.load();
    uint32 newAverage = static_cast<uint32>(ALPHA * updateTimeMs + (1.0f - ALPHA) * currentAverage);
    _performanceData.averageUpdateTimeMs = newAverage;

    _performanceData.lastUpdate = std::chrono::steady_clock::now();
}

void BotAI::LogAIDecision(std::string const& action, float score) const
{
    TC_LOG_TRACE("playerbots.ai", "Bot {} executed action '{}' with score {:.2f}",
                 _bot ? _bot->GetName() : "null", action, score);
}

// Missing method implementations
void BotAI::UpdateValuesInternal(uint32 diff)
{
    // Update internal values based on bot state
    UpdateValues();
}

void BotAI::UpdateCombat(uint32 diff)
{
    if (!_bot || !_bot->IsInCombat())
        return;

    // Update combat state and handle combat actions
    if (_aiState != BotAIState::COMBAT)
        SetAIState(BotAIState::COMBAT);

    // Delegate to class-specific combat routines
    if (::Unit* target = GetTargetUnit())
    {
        UpdateRotation(target);
        OnCombatStart(target);
    }

    UpdateBuffs();
    UpdateCooldowns(diff);
}

void BotAI::UpdateMovement(uint32 diff)
{
    if (!_bot)
        return;

    // Handle basic movement updates
    // Movement is primarily handled by individual strategies and actions

    // Check if bot should follow group leader
    Group* group = _bot->GetGroup();
    if (group && group->GetMembersCount() > 1)
    {
        // Movement handled by LeaderFollowBehavior strategy
    }
}

void BotAI::UpdateActions(uint32 diff)
{
    // Process action queue
    if (!_actionQueue.empty() && !_currentAction)
    {
        auto actionPair = _actionQueue.front();
        _actionQueue.pop();

        _currentAction = actionPair.first;
        _currentContext = actionPair.second;

        if (_currentAction)
        {
            ExecuteActionInternal(_currentAction.get(), _currentContext);
        }
    }
}

void BotAI::UpdateStrategies(uint32 diff)
{
    // Update strategy relevance and activation
    EvaluateStrategies();

    // Update active strategies
    for (auto const& strategyName : _activeStrategies)
    {
        Strategy* strategy = GetStrategy(strategyName);
        if (strategy)
        {
            // Strategy updates are handled during trigger processing
        }
    }
}

void BotAI::OnGroupJoined(Group* group)
{
    if (!group || !_bot)
        return;

    TC_LOG_INFO("playerbots.ai", "Bot {} joined group {}",
                _bot->GetName(), group->GetGUID().ToString());

    // Activate follow strategy when joining a group
    ActivateStrategy("follow");

    // Initialize group-specific behaviors
    if (_groupInvitationHandler)
    {
        // Group invitation handler setup if needed
    }

    // Update AI state
    if (_aiState == BotAIState::IDLE)
        SetAIState(BotAIState::FOLLOWING);
}

void BotAI::SetAIState(BotAIState state)
{
    if (_aiState != state)
    {
        BotAIState oldState = _aiState;
        _aiState = state;

        TC_LOG_DEBUG("playerbots.ai", "Bot {} AI state changed from {} to {}",
                     _bot ? _bot->GetName() : "null",
                     static_cast<int>(oldState), static_cast<int>(state));

        // Handle state-specific logic
        switch (state)
        {
            case BotAIState::COMBAT:
                ActivateStrategy("combat");
                break;
            case BotAIState::FOLLOWING:
                ActivateStrategy("follow");
                break;
            case BotAIState::IDLE:
                ActivateStrategy("idle");
                break;
            default:
                break;
        }
    }
}

void BotAI::CancelCurrentAction()
{
    if (_currentAction)
    {
        TC_LOG_DEBUG("playerbots.ai", "Bot {} cancelling current action",
                     _bot ? _bot->GetName() : "null");

        _currentAction.reset();
        _currentContext = ActionContext{};
    }
}

void BotAI::QueueAction(std::shared_ptr<Action> action, ActionContext const& context)
{
    if (!action)
        return;

    _actionQueue.push(std::make_pair(action, context));

    TC_LOG_TRACE("playerbots.ai", "Bot {} queued action, queue size: {}",
                 _bot ? _bot->GetName() : "null", _actionQueue.size());
}

// Additional missing method implementations
::Unit* BotAI::GetTargetUnit() const
{
    if (_currentTarget.IsEmpty())
        return nullptr;

    return ObjectAccessor::GetUnit(*_bot, _currentTarget);
}

void BotAI::UpdateValues()
{
    // Update internal value system
    for (auto& valuePair : _values)
    {
        // Values are updated based on bot state
        // This is a simplified implementation
    }
}

ActionResult BotAI::ExecuteActionInternal(Action* action, ActionContext const& context)
{
    if (!action || !_bot)
        return ActionResult::FAILED;

    // Try to execute the action with proper parameters
    return action->Execute(this, context);
}


// ============================================================================
// Basic Strategy Implementations
// ============================================================================

// Simple idle strategy for basic bot movement and actions
class IdleStrategy : public Strategy
{
public:
    IdleStrategy() : Strategy("idle") {}

    void InitializeActions() override
    {
        // Add basic idle actions (will be implemented as needed)
        // For now, this strategy just provides a framework
    }

    void InitializeTriggers() override
    {
        // Add basic triggers (will be implemented as needed)
        // For now, this strategy just provides a framework
    }

    void InitializeValues() override
    {
        // Add basic values (will be implemented as needed)
        // For now, this strategy just provides a framework
    }

    StrategyRelevance CalculateRelevance(BotAI* ai) const override
    {
        StrategyRelevance relevance;
        // Idle strategy is always relevant at a low level
        relevance.survivalRelevance = 15.0f; // Above activation threshold (10.0f)
        return relevance;
    }

    void OnActivate(BotAI* ai) override
    {
        TC_LOG_DEBUG("playerbots.ai.strategy", "IdleStrategy activated for bot");
    }

    void OnDeactivate(BotAI* ai) override
    {
        TC_LOG_DEBUG("playerbots.ai.strategy", "IdleStrategy deactivated for bot");
    }
};

// Simple follow strategy for group behavior
class FollowStrategy : public Strategy
{
public:
    FollowStrategy() : Strategy("follow") {}

    void InitializeActions() override
    {
        // Add follow actions (will be implemented as needed)
        // For now, this strategy just provides a framework
    }

    void InitializeTriggers() override
    {
        // Add follow triggers (will be implemented as needed)
        // For now, this strategy just provides a framework
    }

    void InitializeValues() override
    {
        // Add follow values (will be implemented as needed)
        // For now, this strategy just provides a framework
    }

    StrategyRelevance CalculateRelevance(BotAI* ai) const override
    {
        StrategyRelevance relevance;

        // Check if bot is in a group and should follow
        if (ai && ai->GetBot())
        {
            Group* group = ai->GetBot()->GetGroup();
            if (group && group->GetMembersCount() > 1)
            {
                // High relevance when in group
                relevance.socialRelevance = 50.0f; // High priority
            }
        }

        return relevance;
    }

    void OnActivate(BotAI* ai) override
    {
        TC_LOG_DEBUG("playerbots.ai.strategy", "FollowStrategy activated for bot");
    }

    void OnDeactivate(BotAI* ai) override
    {
        TC_LOG_DEBUG("playerbots.ai.strategy", "FollowStrategy deactivated for bot");
    }
};

// BotAI method implementation after class definitions
void BotAI::InitializeDefaultStrategies()
{
    // Add basic strategies for bot functionality
    AddStrategy(std::unique_ptr<Strategy>(new IdleStrategy()));
    AddStrategy(std::unique_ptr<Strategy>(new FollowStrategy()));

    // Activate idle strategy by default
    ActivateStrategy("idle");

    TC_LOG_DEBUG("playerbots.ai", "Initialized default strategies for bot {}",
                 _bot ? _bot->GetName() : "null");
}

} // namespace Playerbot