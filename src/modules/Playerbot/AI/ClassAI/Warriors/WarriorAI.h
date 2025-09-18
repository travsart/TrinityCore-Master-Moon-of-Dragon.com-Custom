/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ClassAI.h"
#include "WarriorSpecialization.h"
#include "Position.h"
#include <unordered_map>
#include <memory>

namespace Playerbot
{

// Forward declarations
class ArmsSpecialization;
class FurySpecialization;
class ProtectionSpecialization;

// Warrior AI implementation
class TC_GAME_API WarriorAI : public ClassAI
{
public:
    explicit WarriorAI(Player* bot);
    ~WarriorAI() = default;

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
    WarriorSpec _currentSpec;
    std::unique_ptr<WarriorSpecialization> _specialization;

    // Performance tracking
    uint32 _rageSpent;
    uint32 _damageDealt;
    uint32 _lastStanceChange;

    // Shared utility tracking
    std::unordered_map<uint32, uint32> _abilityUsage;
    uint32 _lastBattleShout;
    uint32 _lastCommandingShout;
    bool _needsIntercept;
    bool _needsCharge;
    ::Unit* _lastChargeTarget;
    uint32 _lastChargeTime;

    // Specialization management
    void InitializeSpecialization();
    void UpdateSpecialization();
    WarriorSpec DetectCurrentSpecialization();
    void SwitchSpecialization(WarriorSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared warrior utilities
    void UpdateWarriorBuffs();
    void CastBattleShout();
    void CastCommandingShout();
    void UseChargeAbilities(::Unit* target);
    bool IsInMeleeRange(::Unit* target) const;
    bool CanCharge(::Unit* target) const;

    // Target evaluation
    bool IsValidTarget(::Unit* target);
    ::Unit* GetBestChargeTarget();
    ::Unit* GetHighestThreatTarget();
    ::Unit* GetLowestHealthEnemy();

    // Performance optimization
    void RecordAbilityUsage(uint32 spellId);
    void AnalyzeCombatEffectiveness();

    // Constants
    static constexpr uint32 STANCE_CHANGE_COOLDOWN = 1000; // 1 second
    static constexpr uint32 CHARGE_MIN_RANGE = 8;
    static constexpr uint32 CHARGE_MAX_RANGE = 25;
    static constexpr uint32 INTERCEPT_MIN_RANGE = 8;
    static constexpr uint32 INTERCEPT_MAX_RANGE = 25;
    static constexpr uint32 BATTLE_SHOUT_DURATION = 120000; // 2 minutes
    static constexpr uint32 COMMANDING_SHOUT_DURATION = 120000; // 2 minutes
    static constexpr float OPTIMAL_MELEE_RANGE = 5.0f;

    // Spell IDs (these would need to be accurate for the WoW version)
    enum WarriorSpells
    {
        // Stances
        BATTLE_STANCE = 2457,
        DEFENSIVE_STANCE = 71,
        BERSERKER_STANCE = 2458,

        // Basic attacks
        HEROIC_STRIKE = 78,
        CLEAVE = 845,
        WHIRLWIND = 1680,

        // Arms abilities
        MORTAL_STRIKE = 12294,
        COLOSSUS_SMASH = 86346,
        OVERPOWER = 7384,
        REND = 772,

        // Fury abilities
        BLOODTHIRST = 23881,
        RAMPAGE = 184367,
        RAGING_BLOW = 85288,
        EXECUTE = 5308,

        // Protection abilities
        SHIELD_SLAM = 23922,
        THUNDER_CLAP = 6343,
        REVENGE = 6572,
        DEVASTATE = 20243,
        SHIELD_BLOCK = 2565,

        // Defensive cooldowns
        SHIELD_WALL = 871,
        LAST_STAND = 12975,
        SPELL_REFLECTION = 23920,

        // Offensive cooldowns
        RECKLESSNESS = 1719,
        BLADESTORM = 46924,
        AVATAR = 107574,

        // Movement abilities
        CHARGE = 100,
        INTERCEPT = 20252,
        HEROIC_LEAP = 6544,

        // Utility
        PUMMEL = 6552,
        DISARM = 676,
        TAUNT = 355,
        SUNDER_ARMOR = 7386,

        // Buffs
        BATTLE_SHOUT = 6673,
        COMMANDING_SHOUT = 469,

        // Weapon buffs (if applicable)
        WEAPON_MASTER = 16538
    };
};

} // namespace Playerbot