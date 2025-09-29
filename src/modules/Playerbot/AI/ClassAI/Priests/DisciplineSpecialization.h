/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PriestSpecialization.h"
#include <map>
#include <set>

namespace Playerbot
{

class TC_GAME_API DisciplineSpecialization : public PriestSpecialization
{
public:
    explicit DisciplineSpecialization(Player* bot);

    // Core specialization interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Healing interface
    void UpdateHealing() override;
    bool ShouldHeal() override;
    ::Unit* GetBestHealTarget() override;
    void HealTarget(::Unit* target) override;

    // Role management
    PriestRole GetCurrentRole() override;
    void SetRole(PriestRole role) override;

    // Specialization info
    PriestSpec GetSpecialization() const override { return PriestSpec::DISCIPLINE; }
    const char* GetSpecializationName() const override { return "Discipline"; }

private:
    // Discipline-specific mechanics
    void UpdateShields();
    void UpdateAtonement();
    void UpdateGrace();
    bool ShouldCastShield(::Unit* target);
    bool ShouldCastPenance(::Unit* target);
    bool ShouldUseInnerFocus();
    bool ShouldUsePainSuppression(::Unit* target);
    bool ShouldUseGuardianSpirit(::Unit* target);

    // Shield management
    void CastPowerWordShield(::Unit* target);
    void CastPainSuppression(::Unit* target);
    void CastGuardianSpirit(::Unit* target);
    void CastInnerFocus();
    bool HasWeakenedSoul(::Unit* target);
    uint32 GetShieldTimeRemaining(::Unit* target);

    // Discipline healing spells
    void CastPenance(::Unit* target);
    void CastGreaterHeal(::Unit* target);
    void CastFlashHeal(::Unit* target);
    void CastBindingHeal(::Unit* target);
    void CastPrayerOfMending(::Unit* target);

    // Discipline damage spells (for Atonement)
    void CastSmite(::Unit* target);
    void CastHolyFire(::Unit* target);
    void CastMindBlast(::Unit* target);

    // Atonement healing through damage
    void UpdateAtonementHealing();
    bool ShouldUseAtonement();
    void PerformAtonementHealing(::Unit* target);
    ::Unit* GetBestAtonementTarget();

    // Grace stacking
    void UpdateGraceStacks();
    uint32 GetGraceStacks(::Unit* target);
    bool ShouldStackGrace(::Unit* target);

    // Emergency abilities
    void UseEmergencyAbilities();
    void UsePainSuppression();
    void UseGuardianSpirit();
    void UseInnerFocus();

    // Shield tracking
    void TrackShields();
    void RemoveExpiredShields(uint32 diff);

    // Discipline spell IDs
    enum DisciplineSpells
    {
        POWER_WORD_SHIELD = 17,
        PENANCE = 47540,
        PAIN_SUPPRESSION = 33206,
        GUARDIAN_SPIRIT = 47788,
        INNER_FOCUS = 89485,
        PRAYER_OF_MENDING = 33076,
        BINDING_HEAL = 32546,
        WEAKENED_SOUL = 6788,
        GRACE = 77613,
        ATONEMENT = 194384,
        DIVINE_AEGIS = 47509,
        BORROWED_TIME = 59889
    };

    // State tracking
    PriestRole _currentRole;
    bool _atonementMode;
    uint32 _lastInnerFocus;
    uint32 _lastPainSuppression;
    uint32 _lastGuardianSpirit;

    // Shield tracking per target
    std::map<uint64, uint32> _shieldTimers;
    std::map<uint64, uint32> _weakenedSoulTimers;
    std::map<uint64, uint32> _graceStacks;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Priority queue for healing
    std::priority_queue<Playerbot::HealTarget> _healQueue;

    // Performance optimization
    uint32 _lastHealCheck;
    uint32 _lastShieldCheck;
    uint32 _lastAtonementCheck;
    uint32 _lastRotationUpdate;

    // Atonement targets
    std::set<uint64> _atonementTargets;
    uint32 _lastAtonementScan;

    // Constants
    static constexpr uint32 SHIELD_DURATION = 30000; // 30 seconds
    static constexpr uint32 WEAKENED_SOUL_DURATION = 15000; // 15 seconds
    static constexpr uint32 GRACE_DURATION = 8000; // 8 seconds
    static constexpr uint32 MAX_GRACE_STACKS = 3;
    static constexpr uint32 ATONEMENT_DURATION = 15000; // 15 seconds
    static constexpr float SHIELD_HEALTH_THRESHOLD = 90.0f;
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr float ATONEMENT_MANA_THRESHOLD = 0.7f;
};

} // namespace Playerbot