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
#include "../SpellValidation_WoW112.h"
#include <memory>
#include <chrono>

namespace Playerbot
{

// Forward declarations

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


    // Druid forms for shapeshift management
    enum class DruidForm : uint8
    {
        CASTER = 0,      // Humanoid/Caster form
        HUMANOID = 0,    // Alias for CASTER
        BEAR = 1,
        CAT = 2,
        AQUATIC = 3,
        TRAVEL = 4,
        MOONKIN = 5,
        TREE_OF_LIFE = 6,
        FLIGHT = 7
    };

private:
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
    // Druid Spell IDs - Using central registry (WoW 11.2)
    enum DruidSpells
    {
        // Forms
        BEAR_FORM = WoW112Spells::Druid::Common::BEAR_FORM,
        CAT_FORM = WoW112Spells::Druid::Common::CAT_FORM,
        MOONKIN_FORM = WoW112Spells::Druid::Common::MOONKIN_FORM,
        TREE_OF_LIFE = WoW112Spells::Druid::Common::TREE_OF_LIFE,
        TRAVEL_FORM = WoW112Spells::Druid::Common::TRAVEL_FORM,

        // Interrupts
        SKULL_BASH_BEAR = WoW112Spells::Druid::Common::SKULL_BASH_BEAR,
        SKULL_BASH_CAT = WoW112Spells::Druid::Common::SKULL_BASH_CAT,
        SOLAR_BEAM = WoW112Spells::Druid::Common::SOLAR_BEAM,
        TYPHOON = WoW112Spells::Druid::Common::TYPHOON,
        MIGHTY_BASH = WoW112Spells::Druid::Common::MIGHTY_BASH,

        // Defensive abilities
        BARKSKIN = WoW112Spells::Druid::Common::BARKSKIN,
        SURVIVAL_INSTINCTS = WoW112Spells::Druid::Common::SURVIVAL_INSTINCTS,
        FRENZIED_REGENERATION = WoW112Spells::Druid::Common::FRENZIED_REGENERATION,
        IRONBARK = WoW112Spells::Druid::Common::IRONBARK,
        CENARION_WARD = WoW112Spells::Druid::Common::CENARION_WARD,

        // Feral offensive
        TIGERS_FURY = WoW112Spells::Druid::Common::TIGERS_FURY,
        BERSERK_CAT = WoW112Spells::Druid::Common::BERSERK_CAT,
        INCARNATION_KING = WoW112Spells::Druid::Common::INCARNATION_KING,

        // Feral abilities
        SHRED = WoW112Spells::Druid::Common::SHRED,
        RAKE = WoW112Spells::Druid::Common::RAKE,
        RIP = WoW112Spells::Druid::Common::RIP,
        FEROCIOUS_BITE = WoW112Spells::Druid::Common::FEROCIOUS_BITE,
        SAVAGE_ROAR = WoW112Spells::Druid::Common::SAVAGE_ROAR,
        SWIPE_CAT = WoW112Spells::Druid::Common::SWIPE_CAT,
        THRASH_CAT = WoW112Spells::Druid::Common::THRASH_CAT,
        PRIMAL_WRATH = WoW112Spells::Druid::Common::PRIMAL_WRATH,

        // Guardian abilities
        MANGLE_BEAR = WoW112Spells::Druid::Common::MANGLE_BEAR,
        MAUL = WoW112Spells::Druid::Common::MAUL,
        IRONFUR = WoW112Spells::Druid::Common::IRONFUR,
        THRASH_BEAR = WoW112Spells::Druid::Common::THRASH_BEAR,
        SWIPE_BEAR = WoW112Spells::Druid::Common::SWIPE_BEAR,
        BERSERK_BEAR = WoW112Spells::Druid::Common::BERSERK_BEAR,
        INCARNATION_GUARDIAN = WoW112Spells::Druid::Common::INCARNATION_GUARDIAN,
        PULVERIZE = WoW112Spells::Druid::Common::PULVERIZE,

        // Balance abilities
        WRATH = WoW112Spells::Druid::Common::WRATH,
        STARFIRE = WoW112Spells::Druid::Common::STARFIRE,
        MOONFIRE = WoW112Spells::Druid::Common::MOONFIRE,
        SUNFIRE = WoW112Spells::Druid::Common::SUNFIRE,
        STARSURGE = WoW112Spells::Druid::Common::STARSURGE,
        STARFALL = WoW112Spells::Druid::Common::STARFALL,
        CELESTIAL_ALIGNMENT = WoW112Spells::Druid::Common::CELESTIAL_ALIGNMENT,
        INCARNATION_BALANCE = WoW112Spells::Druid::Common::INCARNATION_BALANCE,
        LUNAR_STRIKE = 194153, // Removed in modern WoW (merged with Starfire)
        SOLAR_WRATH = 190984, // Removed in modern WoW (merged with Wrath)

        // Restoration abilities
        REJUVENATION = WoW112Spells::Druid::Common::REJUVENATION,
        REGROWTH = WoW112Spells::Druid::Common::REGROWTH,
        LIFEBLOOM = WoW112Spells::Druid::Common::LIFEBLOOM,
        HEALING_TOUCH = WoW112Spells::Druid::Common::HEALING_TOUCH,
        WILD_GROWTH = WoW112Spells::Druid::Common::WILD_GROWTH,
        SWIFTMEND = WoW112Spells::Druid::Common::SWIFTMEND,
        TRANQUILITY = WoW112Spells::Druid::Common::TRANQUILITY,
        INCARNATION_TREE = WoW112Spells::Druid::Common::INCARNATION_TREE,
        NATURES_SWIFTNESS = WoW112Spells::Druid::Common::NATURES_SWIFTNESS,
        EFFLORESCENCE = WoW112Spells::Druid::Common::EFFLORESCENCE,

        // Utility
        REMOVE_CORRUPTION = WoW112Spells::Druid::Common::REMOVE_CORRUPTION,
        NATURES_CURE = WoW112Spells::Druid::Common::NATURES_CURE,
        REBIRTH = WoW112Spells::Druid::Common::REBIRTH,
        INNERVATE = WoW112Spells::Druid::Common::INNERVATE,
        STAMPEDING_ROAR = WoW112Spells::Druid::Common::STAMPEDING_ROAR
    };
};

} // namespace Playerbot
