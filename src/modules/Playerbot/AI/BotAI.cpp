/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotAI.h"
#include "Strategy/Strategy.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Values/Value.h"
#include "Player.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Chat.h"
#include "Log.h"

namespace Playerbot
{

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

    TC_LOG_DEBUG("playerbots.ai", "BotAI created for bot {}", _bot->GetName());
}

void BotAI::UpdateAI(uint32 diff)
{
    if (!_enabled || !_bot)
        return;

    auto startTime = std::chrono::steady_clock::now();

    _timeSinceLastUpdate += diff;

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

        // Update combat if needed
        if (_aiState == BotAIState::COMBAT)
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
                     _bot ? _bot->GetName() : "null", oldState, newState, data);
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

    // Process triggers from all active strategies
    for (Strategy* strategy : GetActiveStrategies())
    {
        auto triggers = strategy->GetTriggers();
        for (auto const& trigger : triggers)
        {
            if (trigger && trigger->IsActive(this))
            {
                std::string actionName = trigger->GetAction();
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

// BotAIFactory implementation
BotAIFactory* BotAIFactory::instance()
{
    static BotAIFactory instance;
    return &instance;
}

std::unique_ptr<BotAI> BotAIFactory::CreateAI(Player* bot)
{
    if (!bot)
        return nullptr;

    // For now, create default AI - will be expanded with specialized AIs
    return std::make_unique<DefaultBotAI>(bot);
}

std::unique_ptr<BotAI> BotAIFactory::CreateClassAI(Player* bot, uint8 classId)
{
    if (!bot)
        return nullptr;

    // This will be expanded with class-specific AI implementations
    return CreateAI(bot);
}

std::unique_ptr<BotAI> BotAIFactory::CreateSpecializedAI(Player* bot, std::string const& type)
{
    if (!bot)
        return nullptr;

    auto it = _creators.find(type);
    if (it != _creators.end())
        return it->second(bot);

    // Fallback to default AI
    return CreateAI(bot);
}

void BotAIFactory::RegisterAICreator(std::string const& type,
                                    std::function<std::unique_ptr<BotAI>(Player*)> creator)
{
    _creators[type] = creator;
    TC_LOG_DEBUG("playerbots.ai", "Registered AI creator for type '{}'", type);
}

// Enhanced BotAI methods
bool BotAI::ExecuteAction(std::string const& name, ActionContext const& context)
{
    if (!_bot || name.empty())
        return false;

    // Find action in active strategies
    for (Strategy* strategy : GetActiveStrategies())
    {
        if (std::shared_ptr<Action> action = strategy->GetAction(name))
        {
            if (action->IsPossible(this))
            {
                ActionResult result = action->Execute(this, context);
                if (result == ActionResult::SUCCESS)
                {
                    _enhancedMetrics.actionsExecuted++;
                    LogAIDecision(name, 1.0f);
                    return true;
                }
            }
        }
    }

    return false;
}

void BotAI::QueueAction(std::shared_ptr<Action> action, ActionContext const& context)
{
    if (action)
    {
        _actionQueue.push({action, context});
    }
}

void BotAI::CancelCurrentAction()
{
    _currentAction = nullptr;
    _currentContext = ActionContext();
}

void BotAI::SetAIState(BotAIState state)
{
    if (_aiState != state)
    {
        BotAIState oldState = _aiState;
        _aiState = state;

        TC_LOG_DEBUG("playerbots.ai", "Bot {} AI state changed from {} to {}",
                     _bot ? _bot->GetName() : "null", static_cast<int>(oldState), static_cast<int>(state));
    }
}

void BotAI::RegisterTrigger(std::shared_ptr<Trigger> trigger)
{
    if (trigger)
    {
        _triggers.push_back(trigger);
        TC_LOG_DEBUG("playerbots.ai", "Registered trigger '{}' for bot {}",
                     trigger->GetName(), _bot ? _bot->GetName() : "null");
    }
}

void BotAI::UnregisterTrigger(std::string const& name)
{
    auto it = std::find_if(_triggers.begin(), _triggers.end(),
        [&name](std::shared_ptr<Trigger> const& trigger) {
            return trigger && trigger->GetName() == name;
        });

    if (it != _triggers.end())
    {
        _triggers.erase(it);
        TC_LOG_DEBUG("playerbots.ai", "Unregistered trigger '{}' from bot {}",
                     name, _bot ? _bot->GetName() : "null");
    }
}

float BotAI::GetValue(std::string const& name) const
{
    auto it = _values.find(name);
    return (it != _values.end()) ? it->second : 0.0f;
}

void BotAI::SetValue(std::string const& name, float value)
{
    _values[name] = value;
}

void BotAI::UpdateValues()
{
    if (!_bot)
        return;

    // Update basic values
    _values["health"] = _bot->GetHealthPct() / 100.0f;
    _values["mana"] = _bot->GetMaxPower(POWER_MANA) > 0 ?
        static_cast<float>(_bot->GetPower(POWER_MANA)) / static_cast<float>(_bot->GetMaxPower(POWER_MANA)) : 1.0f;
    _values["in_combat"] = _bot->IsInCombat() ? 1.0f : 0.0f;
    _values["level"] = static_cast<float>(_bot->GetLevel());

    // Update group values
    if (Group* group = _bot->GetGroup())
    {
        _values["in_group"] = 1.0f;
        _values["group_size"] = static_cast<float>(group->GetMembersCount());
    }
    else
    {
        _values["in_group"] = 0.0f;
        _values["group_size"] = 1.0f;
    }
}

Unit* BotAI::GetTargetUnit() const
{
    if (_currentTarget.IsEmpty())
        return nullptr;

    return ObjectAccessor::GetUnit(*_bot, _currentTarget);
}

void BotAI::MoveTo(float x, float y, float z)
{
    if (!_bot)
        return;

    _bot->GetMotionMaster()->MovePoint(0, x, y, z);
    SetAIState(BotAIState::TRAVELLING);
}

void BotAI::Follow(Unit* target, float distance)
{
    if (!_bot || !target)
        return;

    _bot->GetMotionMaster()->MoveFollow(target, distance, M_PI_F / 2);
    SetAIState(BotAIState::FOLLOWING);
}

void BotAI::StopMovement()
{
    if (!_bot)
        return;

    _bot->GetMotionMaster()->Clear();
    _bot->StopMoving();
}

bool BotAI::IsMoving() const
{
    if (!_bot)
        return false;

    return _bot->isMoving();
}

void BotAI::Say(std::string const& text)
{
    if (!_bot || text.empty())
        return;

    _bot->Say(text, LANG_UNIVERSAL);
}

void BotAI::Whisper(std::string const& text, Player* target)
{
    if (!_bot || !target || text.empty())
        return;

    _bot->Whisper(text, LANG_UNIVERSAL, target);
}

void BotAI::Emote(uint32 emoteId)
{
    if (!_bot)
        return;

    _bot->HandleEmoteCommand(emoteId);
}

// Enhanced internal update methods
void BotAI::UpdateStrategies(uint32 diff)
{
    // Evaluate all strategies and activate/deactivate based on relevance
    for (auto& [name, strategy] : _strategies)
    {
        if (!strategy)
            continue;

        float relevance = strategy->GetRelevance(this);
        bool shouldBeActive = relevance > 10.0f; // Threshold for activation
        bool isActive = std::find(_activeStrategies.begin(), _activeStrategies.end(), name) != _activeStrategies.end();

        if (shouldBeActive && !isActive)
        {
            ActivateStrategy(name);
        }
        else if (!shouldBeActive && isActive)
        {
            DeactivateStrategy(name);
        }
    }

    _enhancedMetrics.strategiesEvaluated += static_cast<uint32>(_strategies.size());
}

void BotAI::UpdateActions(uint32 diff)
{
    // Check current action
    if (_currentAction)
    {
        // Continue executing current action
        ActionResult result = _currentAction->Execute(this, _currentContext);

        if (result == ActionResult::SUCCESS || result == ActionResult::FAILED)
        {
            // Action completed
            _currentAction = nullptr;
            _enhancedMetrics.actionsExecuted++;
        }
        else if (result == ActionResult::CANCELLED)
        {
            _currentAction = nullptr;
        }
        // IN_PROGRESS - continue next update
        return;
    }

    // Select next action
    std::shared_ptr<Action> nextAction;

    // Priority: Triggered actions > Queued actions > Strategy actions
    if (!_triggeredActions.empty())
    {
        TriggerResult triggered = _triggeredActions.top();
        _triggeredActions.pop();
        nextAction = triggered.suggestedAction;
        _currentContext = triggered.context;
    }
    else if (!_actionQueue.empty())
    {
        auto queued = _actionQueue.front();
        _actionQueue.pop();
        nextAction = queued.first;
        _currentContext = queued.second;
    }
    else
    {
        nextAction = SelectNextAction();
    }

    // Execute selected action
    if (nextAction && CanExecuteAction(nextAction.get()))
    {
        _currentAction = nextAction;
        ActionResult result = _currentAction->Execute(this, _currentContext);

        if (result == ActionResult::SUCCESS || result == ActionResult::FAILED)
        {
            _currentAction = nullptr;
            _enhancedMetrics.actionsExecuted++;
        }
    }
}

void BotAI::UpdateMovement(uint32 diff)
{
    // Movement logic will be implemented based on current state
    // For now, just update the AI state based on movement
    if (!_bot)
        return;

    if (_bot->isMoving() && _aiState == BotAIState::IDLE)
        SetAIState(BotAIState::TRAVELLING);
    else if (!_bot->isMoving() && _aiState == BotAIState::TRAVELLING)
        SetAIState(BotAIState::IDLE);
}

void BotAI::UpdateCombat(uint32 diff)
{
    if (!_bot)
        return;

    // Update combat state
    if (!_bot->IsInCombat() && _aiState == BotAIState::COMBAT)
        SetAIState(BotAIState::IDLE);
    else if (_bot->IsInCombat() && _aiState != BotAIState::COMBAT)
        SetAIState(BotAIState::COMBAT);
}

void BotAI::UpdateValuesInternal(uint32 diff)
{
    UpdateValues();
}

std::shared_ptr<Action> BotAI::SelectNextAction()
{
    // Get actions from active strategies
    std::vector<std::shared_ptr<Action>> candidates;

    for (auto const& strategyName : _activeStrategies)
    {
        if (Strategy* strategy = GetStrategy(strategyName))
        {
            auto actions = strategy->GetActions();
            for (auto const& action : actions)
            {
                if (action && action->IsPossible(this) && action->IsUseful(this))
                {
                    candidates.push_back(action);
                }
            }
        }
    }

    if (candidates.empty())
        return nullptr;

    // Sort by relevance
    std::sort(candidates.begin(), candidates.end(),
        [this](auto const& a, auto const& b)
        {
            return a->GetRelevance(this) > b->GetRelevance(this);
        });

    return candidates.front();
}

bool BotAI::CanExecuteAction(Action* action) const
{
    return action && action->IsPossible(const_cast<BotAI*>(this)) && !action->IsOnCooldown();
}

ActionResult BotAI::ExecuteActionInternal(Action* action, ActionContext const& context)
{
    if (!action)
        return ActionResult::FAILED;

    return action->Execute(this, context);
}

// Enhanced BotAIFactory methods
std::unique_ptr<BotAI> BotAIFactory::CreateClassAI(Player* bot, uint8 classId, uint8 spec)
{
    auto ai = std::make_unique<BotAI>(bot);

    // Initialize class strategies
    InitializeClassStrategies(ai.get(), classId, spec);

    // Initialize default triggers
    InitializeDefaultTriggers(ai.get());

    // Initialize default values
    InitializeDefaultValues(ai.get());

    return ai;
}

std::unique_ptr<BotAI> BotAIFactory::CreatePvPAI(Player* bot)
{
    auto ai = CreateAI(bot);
    if (ai)
        ApplyTemplate(ai.get(), "pvp");
    return ai;
}

std::unique_ptr<BotAI> BotAIFactory::CreatePvEAI(Player* bot)
{
    auto ai = CreateAI(bot);
    if (ai)
        ApplyTemplate(ai.get(), "pve");
    return ai;
}

std::unique_ptr<BotAI> BotAIFactory::CreateRaidAI(Player* bot)
{
    auto ai = CreateAI(bot);
    if (ai)
        ApplyTemplate(ai.get(), "raid");
    return ai;
}

void BotAIFactory::RegisterTemplate(std::string const& name,
                                   std::function<void(BotAI*)> initializer)
{
    _templates[name] = initializer;
    TC_LOG_DEBUG("playerbots.ai", "Registered AI template '{}'", name);
}

void BotAIFactory::ApplyTemplate(BotAI* ai, std::string const& templateName)
{
    auto it = _templates.find(templateName);
    if (it != _templates.end())
    {
        it->second(ai);
        TC_LOG_DEBUG("playerbots.ai", "Applied template '{}' to bot {}",
                     templateName, ai->GetBot() ? ai->GetBot()->GetName() : "null");
    }
}

void BotAIFactory::InitializeDefaultStrategies(BotAI* ai)
{
    // Default strategies will be implemented
}

void BotAIFactory::InitializeClassStrategies(BotAI* ai, uint8 classId, uint8 spec)
{
    // Class-specific strategies will be implemented
}

void BotAIFactory::InitializeDefaultTriggers(BotAI* ai)
{
    // Health triggers
    ai->RegisterTrigger(std::make_shared<HealthTrigger>("low_health", 0.3f));
    ai->RegisterTrigger(std::make_shared<HealthTrigger>("critical_health", 0.15f));

    // Combat triggers
    ai->RegisterTrigger(std::make_shared<CombatTrigger>("enemy_too_close"));

    // Timer triggers
    ai->RegisterTrigger(std::make_shared<TimerTrigger>("buff_check", 30000));
    ai->RegisterTrigger(std::make_shared<TimerTrigger>("rest_check", 5000));
}

void BotAIFactory::InitializeDefaultValues(BotAI* ai)
{
    // Initialize basic values
    ai->SetValue("health", 1.0f);
    ai->SetValue("mana", 1.0f);
    ai->SetValue("in_combat", 0.0f);
    ai->SetValue("in_group", 0.0f);
    ai->SetValue("group_size", 1.0f);
}

} // namespace Playerbot