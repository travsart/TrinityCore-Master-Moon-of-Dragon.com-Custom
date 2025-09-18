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
#include <vector>

// Forward declarations
class Player;
class Unit;

namespace Playerbot
{

// Warrior specializations
enum class WarriorSpec : uint8
{
    ARMS = 0,
    FURY = 1,
    PROTECTION = 2
};

// Warrior stances
enum class WarriorStance : uint8
{
    NONE = 0,
    BATTLE = 1,
    DEFENSIVE = 2,
    BERSERKER = 3
};

// Base interface for warrior specializations
class TC_GAME_API WarriorSpecialization
{
public:
    explicit WarriorSpecialization(Player* bot);
    virtual ~WarriorSpecialization() = default;

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

    // Stance management
    virtual void UpdateStance() = 0;
    virtual WarriorStance GetOptimalStance(::Unit* target) = 0;
    virtual void SwitchStance(WarriorStance stance) = 0;

    // Specialization info
    virtual WarriorSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* _bot;

    // Shared warrior utilities
    bool HasEnoughRage(uint32 amount);
    uint32 GetRage();
    uint32 GetMaxRage();
    float GetRagePercent();
    bool ShouldConserveRage();

    // Shared warrior abilities
    void CastCharge(::Unit* target);
    void CastIntercept(::Unit* target);
    void CastHeroicLeap(::Unit* target);
    void CastThunderClap();
    void CastShout();
    void CastRend(::Unit* target);

    // Stance management
    void EnterBattleStance();
    void EnterDefensiveStance();
    void EnterBerserkerStance();
    WarriorStance GetCurrentStance();
    bool IsInStance(WarriorStance stance);

    // Shared defensive abilities
    void UseShieldWall();
    void UseLastStand();
    void UseEnragedRegeneration();
    void UseSpellReflection();

    // Shared utility
    bool IsChanneling();
    bool IsCasting();
    bool CanUseAbility();
    bool IsInDanger();
    bool IsInMeleeRange(::Unit* target);

    // Common constants
    static constexpr float OPTIMAL_MELEE_RANGE = 5.0f;
    static constexpr float CHARGE_RANGE = 25.0f;
    static constexpr float MINIMUM_SAFE_RANGE = 8.0f;
    static constexpr float RAGE_CONSERVATION_THRESHOLD = 20.0f;
    static constexpr float RAGE_DUMP_THRESHOLD = 80.0f;

    // Common spell IDs
    enum CommonSpells
    {
        // Movement abilities
        CHARGE = 100,
        INTERCEPT = 20252,
        HEROIC_LEAP = 6544,

        // Basic attacks
        HEROIC_STRIKE = 78,
        REND = 772,
        THUNDER_CLAP = 6343,
        CLEAVE = 845,

        // Shouts
        BATTLE_SHOUT = 6673,
        COMMANDING_SHOUT = 469,
        DEMORALIZING_SHOUT = 1160,
        INTIMIDATING_SHOUT = 5246,

        // Defensive abilities
        SHIELD_WALL = 871,
        LAST_STAND = 12975,
        ENRAGED_REGENERATION = 55694,
        SPELL_REFLECTION = 23920,

        // Stances
        BATTLE_STANCE = 2457,
        DEFENSIVE_STANCE = 71,
        BERSERKER_STANCE = 2458
    };
};

} // namespace Playerbot