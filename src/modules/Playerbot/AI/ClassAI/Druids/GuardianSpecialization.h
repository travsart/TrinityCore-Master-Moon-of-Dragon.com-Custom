/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DruidSpecialization.h"
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API GuardianSpecialization : public DruidSpecialization
{
public:
    explicit GuardianSpecialization(Player* bot);

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

    // Form management
    void UpdateFormManagement() override;
    DruidForm GetOptimalFormForSituation() override;
    bool ShouldShiftToForm(DruidForm form) override;
    void ShiftToForm(DruidForm form) override;

    // DoT/HoT management
    void UpdateDotHotManagement() override;
    bool ShouldApplyDoT(::Unit* target, uint32 spellId) override;
    bool ShouldApplyHoT(::Unit* target, uint32 spellId) override;

    // Specialization info
    DruidSpec GetSpecialization() const override { return DruidSpec::GUARDIAN; }
    const char* GetSpecializationName() const override { return "Guardian"; }

private:
    // Guardian-specific mechanics
    void UpdateRageManagement();
    void UpdateThreatManagement();
    void UpdateDefensiveCooldowns();
    bool ShouldCastMaul(::Unit* target);
    bool ShouldCastMangle(::Unit* target);
    bool ShouldCastThrash();
    bool ShouldCastSwipe();
    bool ShouldCastLacerate(::Unit* target);
    bool ShouldCastFrenziedRegeneration();
    bool ShouldCastSurvivalInstincts();

    // Rage management
    void GenerateRage(uint32 amount);
    void SpendRage(uint32 amount);
    bool HasEnoughRage(uint32 required);
    uint32 GetRage();
    float GetRagePercent();

    // Threat management
    void BuildThreat(::Unit* target);
    void MaintainThreat();
    std::vector<::Unit*> GetThreatTargets();
    bool NeedsThreat(::Unit* target);

    // Guardian abilities
    void CastMaul(::Unit* target);
    void CastMangle(::Unit* target);
    void CastThrash();
    void CastSwipe();
    void CastLacerate(::Unit* target);
    void CastFrenziedRegeneration();
    void CastSurvivalInstincts();

    // Bear form management
    void EnterBearForm();
    bool ShouldUseBearForm();

    // Defensive abilities
    void UseDefensiveCooldowns();
    void ManageEmergencyAbilities();

    // Guardian spell IDs
    enum GuardianSpells
    {
        MAUL = 6807,
        MANGLE_BEAR = 33878,
        THRASH = 77758,
        SWIPE = 779,
        LACERATE = 33745,
        FRENZIED_REGENERATION = 22842,
        SURVIVAL_INSTINCTS = 61336,
        DEMORALIZING_ROAR = 99,
        CHALLENGING_ROAR = 5209
    };

    // Rage system
    uint32 _rage;
    uint32 _maxRage;
    uint32 _lastRageDecay;
    uint32 _rageDecayRate;

    // Threat tracking
    uint32 _thrashStacks;
    uint32 _lacerateStacks;
    std::vector<::Unit*> _threatTargets;
    uint32 _lastThreatUpdate;

    // Defensive cooldowns
    uint32 _survivalInstinctsReady;
    uint32 _frenziedRegenerationReady;
    uint32 _lastSurvivalInstincts;
    uint32 _lastFrenziedRegeneration;

    // DoT tracking
    std::unordered_map<ObjectGuid, uint32> _lacerateTimers;
    std::unordered_map<ObjectGuid, uint32> _thrashTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalThreatGenerated;
    uint32 _rageSpent;
    uint32 _damageAbsorbed;

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 RAGE_MAX = 100;
    static constexpr uint32 RAGE_DECAY_RATE = 2; // per second
    static constexpr uint32 SURVIVAL_INSTINCTS_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 FRENZIED_REGENERATION_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 LACERATE_DURATION = 15000; // 15 seconds
    static constexpr uint32 THRASH_DURATION = 16000; // 16 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f; // 30%
};

} // namespace Playerbot