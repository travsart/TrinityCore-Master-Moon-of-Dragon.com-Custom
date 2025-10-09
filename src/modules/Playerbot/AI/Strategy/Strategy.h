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
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <unordered_map>
#include <functional>

// Forward declarations for TrinityCore types
class Unit;
class Player;
class Quest;

namespace Playerbot
{

// Forward declarations
class BotAI;
class Action;
class Trigger;
class Value;

// Strategy relevance score
struct StrategyRelevance
{
    float combatRelevance = 0.0f;
    float questRelevance = 0.0f;
    float socialRelevance = 0.0f;
    float survivalRelevance = 0.0f;
    float economicRelevance = 0.0f;

    float GetOverallRelevance() const
    {
        return combatRelevance + questRelevance + socialRelevance +
               survivalRelevance + economicRelevance;
    }
};

class TC_GAME_API Strategy
{
public:
    Strategy(std::string const& name);
    virtual ~Strategy() = default;

    // Core strategy interface
    virtual void InitializeActions() = 0;
    virtual void InitializeTriggers() = 0;
    virtual void InitializeValues() = 0;

    // Strategy evaluation
    virtual float GetRelevance(BotAI* ai) const;
    virtual StrategyRelevance CalculateRelevance(BotAI* ai) const;
    virtual bool IsActive(BotAI* ai) const { return _active; }

    // Action management
    void AddAction(std::string const& name, std::shared_ptr<Action> action);
    std::shared_ptr<Action> GetAction(std::string const& name) const;
    std::vector<std::shared_ptr<Action>> GetActions() const;

    // Trigger management
    void AddTrigger(std::shared_ptr<Trigger> trigger);
    std::vector<std::shared_ptr<Trigger>> GetTriggers() const;

    // Value management
    void AddValue(std::string const& name, std::shared_ptr<Value> value);
    std::shared_ptr<Value> GetValue(std::string const& name) const;

    // Strategy metadata
    std::string const& GetName() const { return _name; }
    uint32 GetPriority() const { return _priority; }
    void SetPriority(uint32 priority) { _priority = priority; }

    // Activation control
    virtual void OnActivate(BotAI* ai) {}
    virtual void OnDeactivate(BotAI* ai) {}
    void SetActive(bool active) { _active = active; }

    // Update method for every-frame behavior updates
    // Called from BotAI::UpdateStrategies() every frame when strategy is active
    // No throttling - runs at full frame rate for smooth behavior
    virtual void UpdateBehavior(BotAI* ai, uint32 diff) {}

protected:
    std::string _name;
    uint32 _priority = 100;
    std::atomic<bool> _active{false};

    // Strategy components
    std::unordered_map<std::string, std::shared_ptr<Action>> _actions;
    std::vector<std::shared_ptr<Trigger>> _triggers;
    std::unordered_map<std::string, std::shared_ptr<Value>> _values;
};

// Combat strategy base
class TC_GAME_API CombatStrategy : public Strategy
{
public:
    CombatStrategy(std::string const& name) : Strategy(name) {}

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetRelevance(BotAI* ai) const override;

    // Combat-specific methods
    virtual bool ShouldFlee(BotAI* ai) const;
    virtual ::Unit* SelectTarget(BotAI* ai) const;
    virtual float GetThreatModifier() const { return 1.0f; }
};

// Social strategy base
class TC_GAME_API SocialStrategy : public Strategy
{
public:
    SocialStrategy(std::string const& name) : Strategy(name) {}

    virtual void InitializeActions() override;
    virtual void InitializeTriggers() override;
    virtual float GetRelevance(BotAI* ai) const override;

    // Social-specific methods
    virtual bool ShouldGroupWith(Player* player) const;
    virtual bool ShouldTrade(Player* player) const;
    virtual std::string GenerateResponse(std::string const& message) const;
};

// Strategy factory
class TC_GAME_API StrategyFactory
{
    StrategyFactory() = default;
    ~StrategyFactory() = default;
    StrategyFactory(StrategyFactory const&) = delete;
    StrategyFactory& operator=(StrategyFactory const&) = delete;

public:
    static StrategyFactory* instance();

    // Strategy registration
    void RegisterStrategy(std::string const& name,
                         std::function<std::unique_ptr<Strategy>()> creator);

    // Strategy creation
    std::unique_ptr<Strategy> CreateStrategy(std::string const& name);
    std::vector<std::unique_ptr<Strategy>> CreateClassStrategies(uint8 classId, uint8 spec);
    std::vector<std::unique_ptr<Strategy>> CreateLevelStrategies(uint8 level);
    std::vector<std::unique_ptr<Strategy>> CreatePvPStrategies();
    std::vector<std::unique_ptr<Strategy>> CreatePvEStrategies();

    // Available strategies
    std::vector<std::string> GetAvailableStrategies() const;
    bool HasStrategy(std::string const& name) const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Strategy>()>> _creators;
};

#define sStrategyFactory StrategyFactory::instance()

} // namespace Playerbot