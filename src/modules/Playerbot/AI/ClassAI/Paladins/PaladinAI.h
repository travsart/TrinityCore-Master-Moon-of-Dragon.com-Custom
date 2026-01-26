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
#include "../SpellValidation_WoW112_Part2.h"
#include "Position.h"
#include <memory>
#include <chrono>
#include <map>

namespace Playerbot
{

// Forward declarations (kept for potential future use in refactored specs)

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
    // Paladin Spell IDs - Using central registry (WoW 11.2)
    enum PaladinSpells
    {
        // Interrupts
        REBUKE = WoW112Spells::Paladin::Common::REBUKE,
        HAMMER_OF_JUSTICE = WoW112Spells::Paladin::Common::HAMMER_OF_JUSTICE,

        // Defensive Cooldowns
        DIVINE_SHIELD = WoW112Spells::Paladin::Common::DIVINE_SHIELD,
        SHIELD_OF_VENGEANCE = WoW112Spells::Paladin::Common::SHIELD_OF_VENGEANCE,
        BLESSING_OF_PROTECTION = WoW112Spells::Paladin::Common::BLESSING_OF_PROTECTION,
        ARDENT_DEFENDER = WoW112Spells::Paladin::Common::ARDENT_DEFENDER,
        GUARDIAN_OF_ANCIENT_KINGS = WoW112Spells::Paladin::Common::GUARDIAN_OF_ANCIENT_KINGS,
        LAY_ON_HANDS = WoW112Spells::Paladin::Common::LAY_ON_HANDS,
        DIVINE_PROTECTION = WoW112Spells::Paladin::Common::DIVINE_PROTECTION,

        // Offensive Cooldowns
        AVENGING_WRATH = WoW112Spells::Paladin::Common::AVENGING_WRATH,
        CRUSADE = WoW112Spells::Paladin::Common::CRUSADE,
        HOLY_AVENGER = WoW112Spells::Paladin::Common::HOLY_AVENGER,
        EXECUTION_SENTENCE = WoW112Spells::Paladin::Common::EXECUTION_SENTENCE,

        // Holy Power Generators
        CRUSADER_STRIKE = WoW112Spells::Paladin::Common::CRUSADER_STRIKE,
        BLADE_OF_JUSTICE = WoW112Spells::Paladin::Common::BLADE_OF_JUSTICE,
        HAMMER_OF_THE_RIGHTEOUS = WoW112Spells::Paladin::Common::HAMMER_OF_THE_RIGHTEOUS,
        JUDGMENT = WoW112Spells::Paladin::Common::JUDGMENT,
        WAKE_OF_ASHES = WoW112Spells::Paladin::Common::WAKE_OF_ASHES,

        // Holy Power Spenders
        TEMPLARS_VERDICT = WoW112Spells::Paladin::Common::TEMPLARS_VERDICT,
        FINAL_VERDICT = WoW112Spells::Paladin::Common::FINAL_VERDICT,
        DIVINE_STORM = WoW112Spells::Paladin::Common::DIVINE_STORM,
        SHIELD_OF_THE_RIGHTEOUS = WoW112Spells::Paladin::Common::SHIELD_OF_THE_RIGHTEOUS,
        WORD_OF_GLORY = WoW112Spells::Paladin::Common::WORD_OF_GLORY,

        // AoE Abilities
        CONSECRATION = WoW112Spells::Paladin::Common::CONSECRATION,
        HAMMER_OF_LIGHT = 427445, // Hero talent
        DIVINE_HAMMER = 198034, // Removed in modern WoW

        // Seals and Auras (most removed in modern WoW)
        SEAL_OF_COMMAND = 20375,
        SEAL_OF_RIGHTEOUSNESS = 21084,
        RETRIBUTION_AURA = WoW112Spells::Paladin::Common::RETRIBUTION_AURA,
        DEVOTION_AURA = WoW112Spells::Paladin::Common::DEVOTION_AURA,
        CRUSADER_AURA = WoW112Spells::Paladin::Common::CRUSADER_AURA,

        // Blessings and Buffs (most removed in modern WoW)
        BLESSING_OF_KINGS = 20217,
        BLESSING_OF_MIGHT = 19740,
        BLESSING_OF_WISDOM = 19742,
        BLESSING_OF_FREEDOM = WoW112Spells::Paladin::Common::BLESSING_OF_FREEDOM,
        BLESSING_OF_SANCTUARY = 20911,

        // Healing Abilities (for Holy spec)
        FLASH_OF_LIGHT = WoW112Spells::Paladin::Common::FLASH_OF_LIGHT,
        HOLY_LIGHT = WoW112Spells::Paladin::Common::HOLY_LIGHT,
        HOLY_SHOCK = WoW112Spells::Paladin::Common::HOLY_SHOCK,
        LIGHT_OF_DAWN = WoW112Spells::Paladin::Common::LIGHT_OF_DAWN,
        BEACON_OF_LIGHT = WoW112Spells::Paladin::Common::BEACON_OF_LIGHT,

        // Utility
        HAND_OF_RECKONING = WoW112Spells::Paladin::Common::HAND_OF_RECKONING,
        CLEANSE = WoW112Spells::Paladin::Common::CLEANSE,
        HAMMER_OF_WRATH = WoW112Spells::Paladin::Common::HAMMER_OF_WRATH,
        EXORCISM = 879, // Removed in modern WoW
        BLINDING_LIGHT = WoW112Spells::Paladin::Common::BLINDING_LIGHT,

        // Movement
        DIVINE_STEED = WoW112Spells::Paladin::Common::DIVINE_STEED,
        LONG_ARM_OF_THE_LAW = 87172 // Passive talent
    };


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
        ::std::chrono::steady_clock::time_point combatStartTime;
        ::std::chrono::steady_clock::time_point lastMetricsUpdate;
    };

    PaladinMetrics _paladinMetrics;
    ::std::map<uint32, uint32> _abilityUsage;
    uint32 _successfulInterrupts;
};

} // namespace Playerbot
