/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "../ClassAI.h"
#include "DruidSpecialization.h"
#include <memory>
#include <chrono>

namespace Playerbot
{

// Forward declarations
class BalanceSpecialization;
class FeralSpecialization;
class GuardianSpecialization;
class RestorationSpecialization;

class TC_GAME_API DruidAI : public ClassAI
{
public:
    explicit DruidAI(Player* bot);
    ~DruidAI() = default;

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization system
    DruidSpec _detectedSpec;
    std::unique_ptr<DruidSpecialization> _specialization;

    // Form management
    DruidForm _currentForm;
    uint32 _lastFormShift;

    // Combat state tracking
    uint32 _comboPoints;
    uint32 _energy;
    uint32 _rage;
    uint32 _lastSwipe;
    uint32 _lastThrash;
    bool _hasNaturesSwiftness;

    // Defensive cooldown tracking
    uint32 _lastBarkskin;
    uint32 _lastSurvivalInstincts;
    uint32 _lastFrenziedRegen;

    // Offensive cooldown tracking
    uint32 _lastTigersFury;
    uint32 _lastBerserk;
    uint32 _lastIncarnation;
    uint32 _lastCelestialAlignment;

    // Specialization management
    void InitializeSpecialization();
    void DetectSpecialization();
    void SwitchSpecialization(DruidSpec newSpec);
    void DelegateToSpecialization(::Unit* target);

    // Combat behavior integration helpers
    bool HandleInterrupts(::Unit* target);
    bool HandleDefensives();
    bool HandleTargetSwitching(::Unit*& target);
    bool HandleAoERotation(::Unit* target);
    bool HandleOffensiveCooldowns(::Unit* target);
    void HandleComboPointManagement(::Unit* target);
    void ExecuteSpecializationRotation(::Unit* target);

    // Form management helpers
    bool IsInForm(DruidForm form) const;
    bool ShiftToForm(DruidForm form);
    DruidForm GetCurrentForm() const;
    bool CanShiftToForm(DruidForm form) const;

    // Resource helpers
    void UpdateResources();
    bool HasEnoughEnergy(uint32 amount) const;
    bool HasEnoughRage(uint32 amount) const;
    bool HasEnoughMana(uint32 amount) const;
    uint32 GetComboPoints() const;

    // Spell ID constants
    enum DruidSpells
    {
        // Forms
        BEAR_FORM = 5487,
        CAT_FORM = 768,
        MOONKIN_FORM = 24858,
        TREE_OF_LIFE = 33891,
        TRAVEL_FORM = 783,

        // Interrupts
        SKULL_BASH_BEAR = 106839,
        SKULL_BASH_CAT = 106839,
        SOLAR_BEAM = 78675,
        TYPHOON = 132469,
        MIGHTY_BASH = 5211,

        // Defensive abilities
        BARKSKIN = 22812,
        SURVIVAL_INSTINCTS = 61336,
        FRENZIED_REGENERATION = 22842,
        IRONBARK = 102342,
        CENARION_WARD = 102351,

        // Feral offensive
        TIGERS_FURY = 5217,
        BERSERK_CAT = 106951,
        INCARNATION_KING = 102543,

        // Feral abilities
        SHRED = 5221,
        RAKE = 1822,
        RIP = 1079,
        FEROCIOUS_BITE = 22568,
        SAVAGE_ROAR = 52610,
        SWIPE_CAT = 106830,
        THRASH_CAT = 106832,
        PRIMAL_WRATH = 285381,

        // Guardian abilities
        MANGLE_BEAR = 33917,
        MAUL = 6807,
        IRONFUR = 192081,
        THRASH_BEAR = 77758,
        SWIPE_BEAR = 213764,
        BERSERK_BEAR = 50334,
        INCARNATION_GUARDIAN = 102558,
        PULVERIZE = 80313,

        // Balance abilities
        WRATH = 5176,
        STARFIRE = 2912,
        MOONFIRE = 8921,
        SUNFIRE = 93402,
        STARSURGE = 78674,
        STARFALL = 191034,
        CELESTIAL_ALIGNMENT = 194223,
        INCARNATION_BALANCE = 102560,
        LUNAR_STRIKE = 194153,
        SOLAR_WRATH = 190984,

        // Restoration abilities
        REJUVENATION = 774,
        REGROWTH = 8936,
        LIFEBLOOM = 33763,
        HEALING_TOUCH = 5185,
        WILD_GROWTH = 48438,
        SWIFTMEND = 18562,
        TRANQUILITY = 740,
        INCARNATION_TREE = 33891,
        NATURES_SWIFTNESS = 132158,
        EFFLORESCENCE = 145205,

        // Utility
        REMOVE_CORRUPTION = 2782,
        NATURES_CURE = 88423,
        REBIRTH = 20484,
        INNERVATE = 29166,
        STAMPEDING_ROAR = 106898
    };
};

} // namespace Playerbot