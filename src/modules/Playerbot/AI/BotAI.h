/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Strategy/Strategy.h"
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

// TriggerResult comparator for priority queue
struct TriggerResultComparator
{
    bool operator()(TriggerResult const& a, TriggerResult const& b) const;
};

// AI state information
struct AIState
{
    enum Type
    {
        STATE_IDLE,
        STATE_COMBAT,
        STATE_QUESTING,
        STATE_SOCIAL,
        STATE_TRAVELING,
        STATE_MAINTENANCE
    };

    Type currentState = STATE_IDLE;
    std::chrono::steady_clock::time_point lastStateChange;
    std::string stateData;
    uint32 stateCounter = 0;
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

// AI update result
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
    virtual ~BotAI() = default;

    // Core AI interface
    virtual void UpdateAI(uint32 diff);
    virtual void Reset();
    virtual void OnDeath();
    virtual void OnRespawn();

    // Enhanced AI loop with performance metrics
    virtual AIUpdateResult UpdateEnhanced(uint32 diff);

    // Legacy compatibility
    virtual void Update(uint32 diff) { UpdateAI(diff); }
    Player* GetPlayer() const { return GetBot(); }

    // Strategy management
    void AddStrategy(std::unique_ptr<Strategy> strategy);
    void RemoveStrategy(std::string const& name);
    Strategy* GetStrategy(std::string const& name) const;
    std::vector<Strategy*> GetActiveStrategies() const;
    void ActivateStrategy(std::string const& name);
    void DeactivateStrategy(std::string const& name);

    // Action execution
    bool ExecuteAction(std::string const& actionName);
    bool ExecuteAction(std::string const& name, ActionContext const& context);
    bool IsActionPossible(std::string const& actionName) const;
    uint32 GetActionPriority(std::string const& actionName) const;

    // Enhanced action management
    void QueueAction(std::shared_ptr<Action> action, ActionContext const& context = {});
    void CancelCurrentAction();
    bool IsActionInProgress() const { return _currentAction != nullptr; }

    // State management
    AIState const& GetState() const { return _state; }
    void SetState(AIState::Type newState, std::string const& data = "");
    bool IsInState(AIState::Type state) const { return _state.currentState == state; }

    // Enhanced state management
    BotAIState GetAIState() const { return _aiState; }
    void SetAIState(BotAIState state);
    bool IsInCombat() const { return _aiState == BotAIState::COMBAT; }
    bool IsIdle() const { return _aiState == BotAIState::IDLE; }

    // Bot access
    Player* GetBot() const { return _bot; }
    ObjectGuid GetBotGuid() const { return _bot ? _bot->GetGUID() : ObjectGuid::Empty; }

    // Decision making
    float EvaluateAction(std::string const& actionName) const;
    std::string SelectBestAction() const;
    void UpdateStrategies();

    // Configuration
    void SetUpdateInterval(uint32 intervalMs) { _updateIntervalMs = intervalMs; }
    uint32 GetUpdateInterval() const { return _updateIntervalMs; }

    void SetEnabled(bool enabled) { _enabled = enabled; }
    bool IsEnabled() const { return _enabled; }

    // Performance metrics
    struct PerformanceData
    {
        std::atomic<uint32> actionsExecuted{0};
        std::atomic<uint32> strategiesEvaluated{0};
        std::atomic<uint32> averageUpdateTimeMs{0};
        std::atomic<uint32> lastUpdateTimeMs{0};
        std::chrono::steady_clock::time_point lastUpdate;
    };

    PerformanceData const& GetPerformanceData() const { return _performanceData; }

    // Enhanced trigger management
    void RegisterTrigger(std::shared_ptr<Trigger> trigger);
    void UnregisterTrigger(std::string const& name);

    // Enhanced value system
    float GetValue(std::string const& name) const;
    void SetValue(std::string const& name, float value);
    void UpdateValues();

    // Target management
    void SetTarget(ObjectGuid guid) { _currentTarget = guid; }
    ObjectGuid GetTarget() const { return _currentTarget; }
    ::Unit* GetTargetUnit() const;

    // Movement control
    void MoveTo(float x, float y, float z);
    void Follow(::Unit* target, float distance = 5.0f);
    void StopMovement();
    bool IsMoving() const;

    // Communication
    void Say(std::string const& text);
    void Whisper(std::string const& text, Player* target);
    void PlayEmote(uint32 emoteId);

    // Enhanced performance metrics
    struct EnhancedMetrics
    {
        std::atomic<uint32> totalUpdates{0};
        std::atomic<uint32> actionsExecuted{0};
        std::atomic<uint32> triggersProcessed{0};
        std::atomic<uint32> strategiesEvaluated{0};
        std::chrono::microseconds averageUpdateTime{0};
        std::chrono::microseconds maxUpdateTime{0};
    };
    EnhancedMetrics const& GetEnhancedMetrics() const { return _enhancedMetrics; }

protected:
    // Internal methods
    virtual void DoUpdateAI(uint32 diff);
    virtual void EvaluateStrategies();
    virtual void ProcessTriggers();
    virtual void ExecuteSelectedAction();

    void UpdatePerformanceMetrics(uint32 updateTimeMs);
    void LogAIDecision(std::string const& action, float score) const;

    // Enhanced internal update methods
    virtual void UpdateStrategies(uint32 diff);
    virtual void UpdateActions(uint32 diff);
    virtual void UpdateMovement(uint32 diff);
    virtual void UpdateCombat(uint32 diff);
    virtual void UpdateValuesInternal(uint32 diff);

    // Enhanced strategy evaluation
    Strategy* SelectBestStrategy();

    // Enhanced action selection
    std::shared_ptr<Action> SelectNextAction();
    bool CanExecuteAction(Action* action) const;
    ActionResult ExecuteActionInternal(Action* action, ActionContext const& context);

    // Enhanced trigger evaluation
    void EvaluateTrigger(Trigger* trigger);
    void HandleTriggeredAction(TriggerResult const& result);

private:
    Player* _bot;
    AIState _state;
    BotAIState _aiState = BotAIState::IDLE;
    ObjectGuid _currentTarget;

    // Strategy collection
    std::unordered_map<std::string, std::unique_ptr<Strategy>> _strategies;
    std::vector<std::string> _activeStrategies;

    // Enhanced action system with concurrent queue
    std::queue<std::pair<std::shared_ptr<Action>, ActionContext>> _actionQueue;
    std::shared_ptr<Action> _currentAction;
    ActionContext _currentContext;

    // Enhanced trigger system
    std::vector<std::shared_ptr<Trigger>> _triggers;
    std::priority_queue<TriggerResult, std::vector<TriggerResult>, TriggerResultComparator> _triggeredActions;

    // Enhanced value system
    std::unordered_map<std::string, float> _values;

    // Configuration
    std::atomic<bool> _enabled{true};
    uint32 _updateIntervalMs = 1000; // 1 second default
    uint32 _timeSinceLastUpdate = 0;
    uint32 _lastUpdate = 0;

    // Performance tracking
    mutable PerformanceData _performanceData;
    mutable EnhancedMetrics _enhancedMetrics;

    // Current action context
    std::string _selectedAction;
    float _selectedActionScore = 0.0f;
    std::chrono::steady_clock::time_point _lastActionTime;

    // Thread safety
    mutable std::shared_mutex _mutex;
};

// Default AI implementation for backward compatibility
class TC_GAME_API DefaultBotAI : public BotAI
{
public:
    explicit DefaultBotAI(Player* player) : BotAI(player) {}

    void UpdateAI(uint32 diff) override
    {
        // Basic AI behavior - will be expanded with strategies
    }
};

// AI factory for creating specialized AI instances
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
    std::unique_ptr<BotAI> CreateSpecializedAI(Player* bot, std::string const& type);
    std::unique_ptr<BotAI> CreatePvPAI(Player* bot);
    std::unique_ptr<BotAI> CreatePvEAI(Player* bot);
    std::unique_ptr<BotAI> CreateRaidAI(Player* bot);

    // AI registration
    void RegisterAICreator(std::string const& type,
                          std::function<std::unique_ptr<BotAI>(Player*)> creator);

    // AI templates
    void RegisterTemplate(std::string const& name,
                         std::function<void(BotAI*)> initializer);
    void ApplyTemplate(BotAI* ai, std::string const& templateName);

private:
    void InitializeDefaultStrategies(BotAI* ai);
    void InitializeClassStrategies(BotAI* ai, uint8 classId, uint8 spec);
    void InitializeDefaultTriggers(BotAI* ai);
    void InitializeDefaultValues(BotAI* ai);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<BotAI>(Player*)>> _creators;
    std::unordered_map<std::string, std::function<void(BotAI*)>> _templates;
};

#define sBotAIFactory BotAIFactory::instance()

} // namespace Playerbot