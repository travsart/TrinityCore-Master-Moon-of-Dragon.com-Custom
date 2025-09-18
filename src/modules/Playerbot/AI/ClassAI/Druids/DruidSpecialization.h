/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Position.h"
#include "SharedDefines.h"
#include <vector>
#include <unordered_map>
#include <queue>

// Forward declarations
class Player;
class Unit;

namespace Playerbot
{

// Druid specializations
enum class DruidSpec : uint8
{
    BALANCE = 0,
    FERAL = 1,
    GUARDIAN = 2,
    RESTORATION = 3
};

// Druid forms
enum class DruidForm : uint8
{
    HUMANOID = 0,
    BEAR = 1,
    CAT = 2,
    AQUATIC = 3,
    TRAVEL = 4,
    MOONKIN = 5,
    TREE_OF_LIFE = 6,
    FLIGHT = 7
};

// Eclipse states for Balance
enum class EclipseState : uint8
{
    NONE = 0,
    SOLAR = 1,
    LUNAR = 2
};

// Combo point tracking for Feral
struct ComboPointInfo
{
    uint32 current;
    uint32 maximum;
    uint32 lastGenerated;
    ::Unit* target;

    ComboPointInfo() : current(0), maximum(5), lastGenerated(0), target(nullptr) {}

    bool HasComboPoints(uint32 required = 1) const { return current >= required; }
    void AddComboPoint() { current = std::min(current + 1, maximum); }
    void SpendComboPoints() { current = 0; }
    void SetTarget(::Unit* t) { target = t; }
};

// HoT tracking for Restoration
struct HealOverTimeInfo
{
    uint32 spellId;
    ::Unit* target;
    uint32 remainingTime;
    uint32 ticksRemaining;
    uint32 healPerTick;
    uint32 lastTick;

    HealOverTimeInfo() : spellId(0), target(nullptr), remainingTime(0),
                         ticksRemaining(0), healPerTick(0), lastTick(0) {}

    HealOverTimeInfo(uint32 spell, ::Unit* tgt, uint32 duration, uint32 healing)
        : spellId(spell), target(tgt), remainingTime(duration),
          ticksRemaining(duration / 3000), healPerTick(healing), lastTick(getMSTime()) {}
};

// Form transition tracking
struct FormTransition
{
    DruidForm fromForm;
    DruidForm toForm;
    uint32 lastTransition;
    uint32 cooldown;
    bool inProgress;

    FormTransition() : fromForm(DruidForm::HUMANOID), toForm(DruidForm::HUMANOID),
                       lastTransition(0), cooldown(1500), inProgress(false) {}
};

// Base specialization interface for all Druid specs
class TC_GAME_API DruidSpecialization
{
public:
    explicit DruidSpecialization(Player* bot) : _bot(bot) {}
    virtual ~DruidSpecialization() = default;

    // Core specialization interface
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual void UpdateCooldowns(uint32 diff) = 0;
    virtual bool CanUseAbility(uint32 spellId) = 0;

    // Combat callbacks
    virtual void OnCombatStart(::Unit* target) = 0;
    virtual void OnCombatEnd() = 0;

    // Resource management
    virtual bool HasEnoughResource(uint32 spellId) = 0;
    virtual void ConsumeResource(uint32 spellId) = 0;

    // Positioning
    virtual Position GetOptimalPosition(::Unit* target) = 0;
    virtual float GetOptimalRange(::Unit* target) = 0;

    // Form management - core to all druid specs
    virtual void UpdateFormManagement() = 0;
    virtual DruidForm GetOptimalFormForSituation() = 0;
    virtual bool ShouldShiftToForm(DruidForm form) = 0;
    virtual void ShiftToForm(DruidForm form) = 0;

    // DoT/HoT management - available to all specs
    virtual void UpdateDotHotManagement() = 0;
    virtual bool ShouldApplyDoT(::Unit* target, uint32 spellId) = 0;
    virtual bool ShouldApplyHoT(::Unit* target, uint32 spellId) = 0;

    // Specialization info
    virtual DruidSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* GetBot() const { return _bot; }

    // Shared form management
    DruidForm _currentForm;
    DruidForm _previousForm;
    FormTransition _formTransition;
    uint32 _lastFormShift;

    // Shared DoT/HoT tracking
    std::unordered_map<ObjectGuid, uint32> _moonfireTimers;
    std::unordered_map<ObjectGuid, uint32> _rejuvenationTimers;
    std::unordered_map<ObjectGuid, uint32> _lifebloomTimers;

    // Common methods available to all specializations
    void CastShapeshift(DruidForm form);
    bool IsInForm(DruidForm form);
    bool CanCastInCurrentForm(uint32 spellId);
    void ApplyDoT(::Unit* target, uint32 spellId);
    void ApplyHoT(::Unit* target, uint32 spellId);
    uint32 GetDoTRemainingTime(::Unit* target, uint32 spellId);
    uint32 GetHoTRemainingTime(::Unit* target, uint32 spellId);

    // Shared spell IDs
    enum SharedSpells
    {
        // Shapeshift forms
        BEAR_FORM = 5487,
        CAT_FORM = 768,
        AQUATIC_FORM = 1066,
        TRAVEL_FORM = 783,
        MOONKIN_FORM = 24858,
        TREE_OF_LIFE = 33891,
        FLIGHT_FORM = 33943,

        // Common spells
        MOONFIRE = 8921,
        REJUVENATION = 774,
        LIFEBLOOM = 33763,
        HEALING_TOUCH = 5185,

        // Utility spells
        BARKSKIN = 22812,
        ENTANGLING_ROOTS = 339,
        CYCLONE = 33786,
        HIBERNATE = 2637,

        // Buffs
        MARK_OF_THE_WILD = 1126,
        THORNS = 467,
        OMEN_OF_CLARITY = 16864
    };

private:
    Player* _bot;
};

} // namespace Playerbot