/*
 * Copyright (C) 2024 TrinityCore <https://www.trinityCore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DeathKnightSpecialization.h"
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API BloodSpecialization : public DeathKnightSpecialization
{
public:
    explicit BloodSpecialization(Player* bot);

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

    // Rune management
    void UpdateRuneManagement() override;
    bool HasAvailableRunes(RuneType type, uint32 count = 1) override;
    void ConsumeRunes(RuneType type, uint32 count = 1) override;
    uint32 GetAvailableRunes(RuneType type) const override;

    // Runic Power management
    void UpdateRunicPowerManagement() override;
    void GenerateRunicPower(uint32 amount) override;
    void SpendRunicPower(uint32 amount) override;
    uint32 GetRunicPower() const override;
    bool HasEnoughRunicPower(uint32 required) const override;

    // Disease management
    void UpdateDiseaseManagement() override;
    void ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId) override;
    bool HasDisease(::Unit* target, DiseaseType type) const override;
    bool ShouldApplyDisease(::Unit* target, DiseaseType type) const override;
    void RefreshExpringDiseases() override;

    // Death and Decay management
    void UpdateDeathAndDecay() override;
    bool ShouldCastDeathAndDecay() const override;
    void CastDeathAndDecay(Position targetPos) override;

    // Specialization info
    DeathKnightSpec GetSpecialization() const override { return DeathKnightSpec::BLOOD; }
    const char* GetSpecializationName() const override { return "Blood"; }

private:
    // Blood-specific mechanics
    void UpdateThreatManagement();
    void UpdateSelfHealing();
    void UpdateDefensiveCooldowns();
    bool ShouldCastDeathStrike(::Unit* target);
    bool ShouldCastHeartStrike(::Unit* target);
    bool ShouldCastBloodBoil();
    bool ShouldCastVampiricBlood();
    bool ShouldCastBoneShield();
    bool ShouldCastDancingRuneWeapon();

    // Threat management
    void BuildThreat(::Unit* target);
    void MaintainThreat();
    std::vector<::Unit*> GetThreatTargets() const;
    bool NeedsThreat(::Unit* target);

    // Self-healing mechanics
    void ManageSelfHealing();
    bool ShouldSelfHeal();
    uint32 CalculateHealingNeeded();

    // Blood abilities
    void CastDeathStrike(::Unit* target);
    void CastHeartStrike(::Unit* target);
    void CastBloodBoil();
    void CastPlagueStrike(::Unit* target);
    void CastDarkCommand(::Unit* target);
    void CastDeathPact();

    // Defensive abilities
    void CastVampiricBlood();
    void CastBoneShield();
    void CastDancingRuneWeapon();
    void CastIceboundFortitude();
    void CastAntiMagicShell();
    void UseDefensiveCooldowns();
    void ManageEmergencyAbilities();

    // Blood presence management
    void EnterBloodPresence();
    bool ShouldUseBloodPresence();

    // Blood spell IDs
    enum BloodSpells
    {
        HEART_STRIKE = 55050,
        BLOOD_STRIKE = 45902,
        VAMPIRIC_BLOOD = 55233,
        BONE_SHIELD = 195181, // Updated for WoW 11.2
        DANCING_RUNE_WEAPON = 49028,
        DEATH_PACT = 48743,
        DARK_COMMAND = 56222,
        ICEBOUND_FORTITUDE = 48792,
        ANTI_MAGIC_SHELL = 48707,
        MARK_OF_BLOOD = 49005,
        HYSTERIA = 49016,
        CORPSE_EXPLOSION = 49158
    };

    // Threat tracking
    std::vector<::Unit*> _threatTargets;
    uint32 _lastThreatUpdate;

    // Defensive cooldowns
    uint32 _vampiricBloodReady;
    uint32 _boneShieldStacks;
    uint32 _dancingRuneWeaponReady;
    uint32 _iceboundFortitudeReady;
    uint32 _antiMagicShellReady;
    uint32 _lastVampiricBlood;
    uint32 _lastBoneShield;
    uint32 _lastDancingRuneWeapon;
    uint32 _lastIceboundFortitude;
    uint32 _lastAntiMagicShell;

    // Self-healing tracking
    uint32 _lastSelfHeal;
    uint32 _healingReceived;
    uint32 _damageAbsorbed;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalThreatGenerated;
    uint32 _totalSelfHealing;
    uint32 _runicPowerSpent;

    // Constants
    static constexpr float BLOOD_MELEE_RANGE = 5.0f;
    static constexpr uint32 VAMPIRIC_BLOOD_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 BONE_SHIELD_DURATION = 300000; // 5 minutes
    static constexpr uint32 DANCING_RUNE_WEAPON_COOLDOWN = 90000; // 1.5 minutes
    static constexpr uint32 ICEBOUND_FORTITUDE_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 ANTI_MAGIC_SHELL_COOLDOWN = 45000; // 45 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f; // 30%
    static constexpr float SELF_HEAL_THRESHOLD = 0.6f; // 60%
    static constexpr uint32 BONE_SHIELD_MAX_STACKS = 4;
    static constexpr uint32 THREAT_UPDATE_INTERVAL = 1000; // 1 second
};

} // namespace Playerbot