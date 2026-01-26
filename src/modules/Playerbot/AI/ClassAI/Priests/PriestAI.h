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
    static constexpr uint32 FEAR_WARD_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 PSYCHIC_SCREAM_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 INNER_FIRE_DURATION = 600000; // 10 minutes
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25%

    // Spell IDs - Using central registry (WoW 11.2)
    enum PriestSpells
    {
        // Utility spells
        DISPEL_MAGIC = WoW112Spells::Priest::Common::DISPEL_MAGIC,
        MASS_DISPEL = WoW112Spells::Priest::Common::MASS_DISPEL,
        PSYCHIC_SCREAM = WoW112Spells::Priest::Common::PSYCHIC_SCREAM,
        FADE = WoW112Spells::Priest::Common::FADE,
        FEAR_WARD = 6346, // Removed in modern WoW
        DESPERATE_PRAYER = WoW112Spells::Priest::Common::DESPERATE_PRAYER,
        SILENCE = WoW112Spells::Priest::Common::SILENCE,
        PURIFY = WoW112Spells::Priest::Common::PURIFY,
        DISPERSION = WoW112Spells::Priest::Common::DISPERSION,

        // Buffs
        POWER_WORD_FORTITUDE = WoW112Spells::Priest::Common::POWER_WORD_FORTITUDE,
        INNER_FIRE = 588, // Removed in modern WoW
        SHADOW_PROTECTION = 976, // Removed in modern WoW
        DIVINE_SPIRIT = 14752, // Removed in modern WoW
        HYMN_OF_HOPE = 64901 // Renamed to SYMBOL_OF_HOPE in modern WoW
    };
};

} // namespace Playerbot
