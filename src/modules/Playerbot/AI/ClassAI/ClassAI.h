/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Position.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Playerbot
{

// Forward declarations
class ActionPriorityQueue;
class CooldownManager;
class ResourceManager;

// Base class for all class-specific AI implementations
class TC_GAME_API ClassAI : public BotAI
{
public:
    ClassAI(Player* bot);
    virtual ~ClassAI();

    // Core AI interface
    void UpdateAI(uint32 diff) override;

    // Class-specific interface - must be implemented by derived classes
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual void UpdateCooldowns(uint32 diff) = 0;
    virtual bool CanUseAbility(uint32 spellId) = 0;

    // Combat state management
    virtual void OnCombatStart(::Unit* target);
    virtual void OnCombatEnd();
    virtual void OnTargetChanged(::Unit* newTarget);

protected:
    // Resource management interface
    virtual bool HasEnoughResource(uint32 spellId) = 0;
    virtual void ConsumeResource(uint32 spellId) = 0;

    // Positioning interface
    virtual Position GetOptimalPosition(::Unit* target) = 0;
    virtual float GetOptimalRange(::Unit* target) = 0;

    // Utility functions for derived classes
    bool IsSpellReady(uint32 spellId);
    bool IsInRange(::Unit* target, uint32 spellId);
    bool HasLineOfSight(::Unit* target);
    bool IsSpellUsable(uint32 spellId);
    float GetSpellRange(uint32 spellId);
    uint32 GetSpellCooldown(uint32 spellId);

    // Spell casting
    bool CastSpell(::Unit* target, uint32 spellId);
    bool CastSpell(uint32 spellId); // Self-cast

    // Target selection
    ::Unit* GetBestAttackTarget();
    ::Unit* GetBestHealTarget();
    ::Unit* GetNearestEnemy(float maxRange = 30.0f);
    ::Unit* GetLowestHealthAlly(float maxRange = 40.0f);

    // Buff/debuff utilities
    bool HasAura(uint32 spellId, ::Unit* target = nullptr);
    uint32 GetAuraStacks(uint32 spellId, ::Unit* target = nullptr);
    uint32 GetAuraRemainingTime(uint32 spellId, ::Unit* target = nullptr);

    // Movement utilities
    bool IsMoving();
    bool IsInMeleeRange(::Unit* target);
    bool ShouldMoveToTarget(::Unit* target);
    void MoveToTarget(::Unit* target, float distance = 0.0f);
    void StopMovement();

    // Component managers
    std::unique_ptr<ActionPriorityQueue> _actionQueue;
    std::unique_ptr<CooldownManager> _cooldownManager;
    std::unique_ptr<ResourceManager> _resourceManager;

    // Combat state
    ::Unit* _currentTarget;
    bool _inCombat;
    uint32 _combatTime;
    uint32 _lastTargetSwitch;

    // Performance optimization
    uint32 _lastUpdate;
    static constexpr uint32 UPDATE_INTERVAL_MS = 100; // 100ms update rate

private:
    // Internal update methods
    void UpdateTargeting();
    void UpdateMovement(uint32 diff) override;
    void UpdateCombatState(uint32 diff);

    // Performance tracking
    void RecordPerformanceMetric(const std::string& metric, uint32 value);
};

// Factory for creating class-specific AI instances
class TC_GAME_API ClassAIFactory
{
public:
    static std::unique_ptr<ClassAI> CreateClassAI(Player* bot);

private:
    // Class-specific creation methods
    static std::unique_ptr<ClassAI> CreateWarriorAI(Player* bot);
    static std::unique_ptr<ClassAI> CreatePaladinAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateHunterAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateRogueAI(Player* bot);
    static std::unique_ptr<ClassAI> CreatePriestAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateDeathKnightAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateShamanAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateMageAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateWarlockAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateMonkAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateDruidAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateDemonHunterAI(Player* bot);
    static std::unique_ptr<ClassAI> CreateEvokerAI(Player* bot);
};

} // namespace Playerbot