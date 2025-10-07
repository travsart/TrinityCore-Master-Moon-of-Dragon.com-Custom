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
#include "PaladinSpecialization.h"
#include "Position.h"
#include <memory>
#include <chrono>
#include <map>

namespace Playerbot
{

// Forward declarations
class PaladinSpecialization;
class HolyPaladinRefactored;
class ProtectionPaladinRefactored;
class RetributionPaladinRefactored;

// Paladin AI with full CombatBehaviorIntegration
class TC_GAME_API PaladinAI : public ClassAI
{
public:
    explicit PaladinAI(Player* bot);
    ~PaladinAI() = default;

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
    // Paladin-specific spell IDs (comprehensive list for all specs)
    enum PaladinSpells
    {
        // Interrupts
        REBUKE = 96231,  // Retribution interrupt
        HAMMER_OF_JUSTICE = 853,  // Stun (can interrupt)

        // Defensive Cooldowns
        DIVINE_SHIELD = 642,
        SHIELD_OF_VENGEANCE = 184662,
        BLESSING_OF_PROTECTION = 1022,
        ARDENT_DEFENDER = 31850,  // Protection
        GUARDIAN_OF_ANCIENT_KINGS = 86659,  // Protection
        LAY_ON_HANDS = 633,
        DIVINE_PROTECTION = 498,

        // Offensive Cooldowns
        AVENGING_WRATH = 31884,
        CRUSADE = 231895,  // Retribution talent
        HOLY_AVENGER = 105809,  // Talent
        EXECUTION_SENTENCE = 114157,  // Talent

        // Holy Power Generators
        CRUSADER_STRIKE = 35395,
        BLADE_OF_JUSTICE = 184575,
        HAMMER_OF_THE_RIGHTEOUS = 53595,  // Protection
        JUDGMENT = 20271,
        WAKE_OF_ASHES = 255937,  // Retribution

        // Holy Power Spenders
        TEMPLARS_VERDICT = 85256,  // Retribution
        FINAL_VERDICT = 157048,  // Retribution talent
        DIVINE_STORM = 53385,  // Retribution AoE
        SHIELD_OF_THE_RIGHTEOUS = 53600,  // Protection
        WORD_OF_GLORY = 85673,  // Healing spender

        // AoE Abilities
        CONSECRATION = 26573,
        HAMMER_OF_LIGHT = 427445,  // Retribution
        DIVINE_HAMMER = 198034,  // Talent

        // Seals and Auras
        SEAL_OF_COMMAND = 20375,
        SEAL_OF_RIGHTEOUSNESS = 21084,
        RETRIBUTION_AURA = 183435,
        DEVOTION_AURA = 183425,
        CRUSADER_AURA = 32223,

        // Blessings and Buffs
        BLESSING_OF_KINGS = 20217,
        BLESSING_OF_MIGHT = 19740,
        BLESSING_OF_WISDOM = 19742,
        BLESSING_OF_FREEDOM = 1044,
        BLESSING_OF_SANCTUARY = 20911,  // Protection

        // Healing Abilities (for Holy spec)
        FLASH_OF_LIGHT = 19750,
        HOLY_LIGHT = 82326,
        HOLY_SHOCK = 20473,
        LIGHT_OF_DAWN = 85222,
        BEACON_OF_LIGHT = 53651,

        // Utility
        HAND_OF_RECKONING = 62124,  // Taunt
        CLEANSE = 4987,
        HAMMER_OF_WRATH = 24275,
        EXORCISM = 879,
        BLINDING_LIGHT = 115750,  // Talent

        // Movement
        DIVINE_STEED = 190784,
        LONG_ARM_OF_THE_LAW = 87172  // Speed buff
    };

    // Specialization management
    void DetectSpecialization();
    void InitializeSpecialization();
    void UpdateSpecialization();
    PaladinSpec DetectCurrentSpecialization();
    void SwitchSpecialization(PaladinSpec newSpec);
    void DelegateToSpecialization(::Unit* target);

    // Paladin-specific combat logic
    void ExecuteBasicPaladinRotation(::Unit* target);
    void UpdatePaladinBuffs();
    void UseDefensiveCooldowns();
    void UseOffensiveCooldowns();
    void ManageHolyPower(::Unit* target);
    void UpdateBlessingManagement();
    void UpdateAuraManagement();

    // Holy Power management
    uint32 GetHolyPower() const;
    bool HasHolyPowerFor(uint32 spellId) const;
    void GenerateHolyPower(::Unit* target);
    void SpendHolyPower(::Unit* target);
    bool ShouldBuildHolyPower() const;

    // Utility functions
    bool IsInMeleeRange(::Unit* target) const;
    bool CanInterrupt(::Unit* target) const;
    uint32 GetNearbyEnemyCount(float range) const;
    uint32 GetNearbyAllyCount(float range) const;
    bool IsAllyInDanger() const;
    bool ShouldUseLayOnHands() const;
    Position CalculateOptimalMeleePosition(::Unit* target);
    bool IsValidTarget(::Unit* target);

    // Performance tracking
    void RecordAbilityUsage(uint32 spellId);
    void RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success);
    void AnalyzeCombatEffectiveness();
    void UpdateMetrics(uint32 diff);
    float CalculateHolyPowerEfficiency();

    // Constants
    static constexpr float OPTIMAL_MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr float CONSECRATION_RADIUS = 8.0f;
    static constexpr float DIVINE_STORM_RADIUS = 8.0f;
    static constexpr uint32 HOLY_POWER_MAX = 5;
    static constexpr uint32 BLESSING_DURATION = 600000;  // 10 minutes
    static constexpr float HEALTH_CRITICAL_THRESHOLD = 20.0f;
    static constexpr float HEALTH_EMERGENCY_THRESHOLD = 30.0f;
    static constexpr float DEFENSIVE_COOLDOWN_THRESHOLD = 40.0f;
    static constexpr float LAY_ON_HANDS_THRESHOLD = 15.0f;
    static constexpr float HOLY_POWER_EFFICIENCY_TARGET = 0.85f;

    // Member variables
    PaladinSpec _currentSpec;
    std::unique_ptr<PaladinSpecialization> _specialization;

    // Cooldown tracking
    uint32 _lastBlessingTime;
    uint32 _lastAuraChange;
    uint32 _lastConsecration;
    uint32 _lastDivineShield;
    uint32 _lastLayOnHands;

    // State tracking
    bool _needsReposition;
    bool _shouldConserveMana;
    uint32 _currentSeal;
    uint32 _currentAura;
    uint32 _currentBlessing;

    // Combat metrics
    struct PaladinMetrics
    {
        uint32 totalAbilitiesUsed;
        uint32 holyPowerGenerated;
        uint32 holyPowerSpent;
        uint32 healingDone;
        uint32 damageDealt;
        float holyPowerEfficiency;
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastMetricsUpdate;
    };

    PaladinMetrics _paladinMetrics;
    std::map<uint32, uint32> _abilityUsage;
    uint32 _successfulInterrupts;
};

} // namespace Playerbot