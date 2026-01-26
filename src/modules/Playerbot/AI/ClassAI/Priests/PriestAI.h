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
#include "../BaselineRotationManager.h"
#include "Position.h"
#include <memory>

namespace Playerbot
{

// Forward declarations for specialization classes (QW-4 FIX)
class DisciplinePriestRefactored;
class HolyPriestRefactored;
class ShadowPriestRefactored;

// Type aliases for consistency with base naming
using DisciplinePriest = DisciplinePriestRefactored;
using HolyPriest = HolyPriestRefactored;
using ShadowPriest = ShadowPriestRefactored;

// Priest AI - Thin coordinator that delegates to specializations
class TC_GAME_API PriestAI : public ClassAI
{
public:
    explicit PriestAI(Player* bot);
    ~PriestAI();

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

    // Combat behavior integration - Priority handlers
    bool HandleCombatBehaviorPriorities(::Unit* target);
    bool HandleInterruptPriority(::Unit* target);
    bool HandleDefensivePriority();
    bool HandlePositioningPriority(::Unit* target);
    bool HandleDispelPriority();
    bool HandleTargetSwitchPriority(::Unit*& target);
    bool HandleCrowdControlPriority(::Unit* target);
    bool HandleAoEPriority(::Unit* target);
    bool HandleCooldownPriority(::Unit* target);
    bool HandleResourceManagement();
    bool ExecuteNormalRotation(::Unit* target);

private:
    // ========================================================================
    // QW-4 FIX: Per-instance specialization objects
    // Each bot has its own specialization object initialized with correct bot pointer
    // ========================================================================

    ::std::unique_ptr<DisciplinePriest> _disciplineSpec;
    ::std::unique_ptr<HolyPriest> _holySpec;
    ::std::unique_ptr<ShadowPriest> _shadowSpec;

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // QW-4 FIX: Per-instance rotation manager (was static - caused cross-bot contamination)
    BaselineRotationManager _rotationManager;

    // Performance tracking
    uint32 _manaSpent;
    uint32 _healingDone;
    uint32 _damageDealt;
    uint32 _playersHealed;
    uint32 _damagePrevented;

    // Shared utility tracking
    uint32 _lastDispel;
    uint32 _lastFearWard;
    uint32 _lastPsychicScream;
    uint32 _lastInnerFire;
    uint32 _lastSilence;
    uint32 _lastMassDispel;
    uint32 _lastDesperatePrayer;
    uint32 _lastFade;

    // Shared priest utilities
    void UpdatePriestBuffs();
    void CastInnerFire();
    void UpdateFortitudeBuffs();
    void CastPowerWordFortitude();

    // Mana management
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    void OptimizeManaUsage();
    bool ShouldConserveMana();
    void UseManaRegeneration();

    // Defensive abilities
    void CastPsychicScream();
    void CastFade();
    void CastDispelMagic();
    void CastFearWard();
    void CastDesperatePrayer();

    // Target selection
    ::Unit* GetBestHealTarget();
    ::Unit* GetBestDispelTarget();
    ::Unit* FindGroupTank();
    ::Unit* GetLowestHealthAlly(float maxRange);

    // Utility methods
    bool HasDispellableDebuff(::Unit* unit);
    bool IsTank(::Unit* unit);
    bool IsHealer(::Unit* unit);
    uint32 CountUnbuffedGroupMembers(uint32 spellId);
    bool IsHealingSpell(uint32 spellId);
    bool IsDamageSpell(uint32 spellId);
    bool IsEmergencyHeal(uint32 spellId);
    bool ShouldUseShadowProtection();

    // Performance calculation methods
    uint32 CalculateDamageDealt(::Unit* target) const;
    uint32 CalculateHealingDone() const;
    uint32 CalculateManaUsage() const;
    void AnalyzeHealingEfficiency();
    void HealGroupMembers();

    // Constants
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr float OPTIMAL_DPS_RANGE = 30.0f;
    static constexpr float SAFE_DISTANCE = 20.0f;
    static constexpr uint32 DISPEL_COOLDOWN = 8000; // 8 seconds
    static constexpr uint32 PSYCHIC_SCREAM_COOLDOWN = 30000; // 30 seconds
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25%
    // Note: FEAR_WARD_COOLDOWN and INNER_FIRE_DURATION removed - spells no longer exist in WoW 11.2

    // Spell IDs - Using central registry (WoW 11.2)
    enum PriestSpells
    {
        // Utility spells
        DISPEL_MAGIC = WoW112Spells::Priest::Common::DISPEL_MAGIC,
        MASS_DISPEL = WoW112Spells::Priest::Common::MASS_DISPEL,
        PSYCHIC_SCREAM = WoW112Spells::Priest::Common::PSYCHIC_SCREAM,
        FADE = WoW112Spells::Priest::Common::FADE,
        DESPERATE_PRAYER = WoW112Spells::Priest::Common::DESPERATE_PRAYER,
        SILENCE = WoW112Spells::Priest::Common::SILENCE,
        PURIFY = WoW112Spells::Priest::Common::PURIFY,
        DISPERSION = WoW112Spells::Priest::Common::DISPERSION,
        LEAP_OF_FAITH = WoW112Spells::Priest::Common::LEAP_OF_FAITH,
        SHACKLE_UNDEAD = WoW112Spells::Priest::Common::SHACKLE_UNDEAD,

        // Buffs
        POWER_WORD_FORTITUDE = WoW112Spells::Priest::Common::POWER_WORD_FORTITUDE,
        POWER_INFUSION = WoW112Spells::Priest::Common::POWER_INFUSION,
        LEVITATE = WoW112Spells::Priest::Common::LEVITATE,
        SYMBOL_OF_HOPE = WoW112Spells::Priest::HolyPriest::SYMBOL_OF_HOPE,

        // Healing spells (Common)
        FLASH_HEAL = WoW112Spells::Priest::Common::FLASH_HEAL,
        HEAL = WoW112Spells::Priest::Common::HEAL,
        RENEW = WoW112Spells::Priest::Common::RENEW,
        PRAYER_OF_MENDING = WoW112Spells::Priest::Common::PRAYER_OF_MENDING,
        POWER_WORD_SHIELD = WoW112Spells::Priest::Common::POWER_WORD_SHIELD,

        // Damage spells (Common)
        SMITE = WoW112Spells::Priest::Common::SMITE,
        SHADOW_WORD_PAIN = WoW112Spells::Priest::Common::SHADOW_WORD_PAIN,
        SHADOW_WORD_DEATH = WoW112Spells::Priest::Common::SHADOW_WORD_DEATH,
        MIND_BLAST = WoW112Spells::Priest::Common::MIND_BLAST,
        HOLY_FIRE = WoW112Spells::Priest::Common::HOLY_FIRE,
        VAMPIRIC_TOUCH = WoW112Spells::Priest::Common::VAMPIRIC_TOUCH,
        MIND_FLAY = WoW112Spells::Priest::Common::MIND_FLAY,

        // Holy spec
        HOLY_WORD_SERENITY = WoW112Spells::Priest::Common::HOLY_WORD_SERENITY,
        HOLY_WORD_SANCTIFY = WoW112Spells::Priest::Common::HOLY_WORD_SANCTIFY,
        CIRCLE_OF_HEALING = WoW112Spells::Priest::Common::CIRCLE_OF_HEALING,
        GUARDIAN_SPIRIT = WoW112Spells::Priest::HolyPriest::GUARDIAN_SPIRIT,
        DIVINE_HYMN = WoW112Spells::Priest::HolyPriest::DIVINE_HYMN,
        PRAYER_OF_HEALING = WoW112Spells::Priest::HolyPriest::PRAYER_OF_HEALING,

        // Discipline spec
        PENANCE = WoW112Spells::Priest::Discipline::PENANCE,
        SHADOW_MEND = WoW112Spells::Priest::Discipline::SHADOW_MEND,
        PAIN_SUPPRESSION = WoW112Spells::Priest::Discipline::PAIN_SUPPRESSION,
        POWER_WORD_BARRIER = WoW112Spells::Priest::Discipline::POWER_WORD_BARRIER,
        RAPTURE = WoW112Spells::Priest::Discipline::RAPTURE,

        // Shadow spec
        SHADOWFORM = WoW112Spells::Priest::Common::SHADOWFORM,
        DEVOURING_PLAGUE = WoW112Spells::Priest::Common::DEVOURING_PLAGUE,
        VAMPIRIC_EMBRACE = WoW112Spells::Priest::Shadow::VAMPIRIC_EMBRACE,
        VOID_ERUPTION = WoW112Spells::Priest::Shadow::VOID_ERUPTION,
        VOID_BOLT = WoW112Spells::Priest::Shadow::VOID_BOLT
    };
};

} // namespace Playerbot
