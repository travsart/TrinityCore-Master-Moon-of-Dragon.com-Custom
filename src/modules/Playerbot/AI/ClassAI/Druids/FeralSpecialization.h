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

namespace Playerbot
{

class TC_GAME_API FeralSpecialization : public DruidSpecialization
{
public:
    explicit FeralSpecialization(Player* bot);

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
    DruidSpec GetSpecialization() const override { return DruidSpec::FERAL; }
    const char* GetSpecializationName() const override { return "Feral"; }

private:
    // Feral-specific mechanics
    void UpdateComboPointSystem();
    void UpdateEnergyManagement();
    void UpdateFeralBuffs();
    bool ShouldCastShred(::Unit* target);
    bool ShouldCastMangle(::Unit* target);
    bool ShouldCastRake(::Unit* target);
    bool ShouldCastRip(::Unit* target);
    bool ShouldCastFerociosBite(::Unit* target);
    bool ShouldCastSavageRoar();
    bool ShouldCastTigersFury();

    // Combo point management
    void GenerateComboPoint(::Unit* target);
    void SpendComboPoints(::Unit* target);
    bool ShouldSpendComboPoints();
    void OptimizeComboPointUsage();

    // Energy management
    bool HasEnoughEnergy(uint32 required);
    void SpendEnergy(uint32 amount);
    uint32 GetEnergy();
    float GetEnergyPercent();

    // Feral rotation abilities
    void CastShred(::Unit* target);
    void CastMangle(::Unit* target);
    void CastRake(::Unit* target);
    void CastRip(::Unit* target);
    void CastFerociosBite(::Unit* target);
    void CastSavageRoar();
    void CastTigersFury();

    // Cat form management
    void EnterCatForm();
    bool ShouldUseCatForm();

    // Stealth management
    void ManageStealth();
    void CastProwl();
    bool ShouldUseStealth();

    // Feral spell IDs
    enum FeralSpells
    {
        SHRED = 5221,
        MANGLE_CAT = 33876,
        RAKE = 1822,
        RIP = 1079,
        FEROCIOUS_BITE = 22568,
        SAVAGE_ROAR = 52610,
        TIGERS_FURY = 5217,
        DASH = 1850,
        PROWL = 5215,
        POUNCE = 9005
    };

    // Combo point system
    ComboPointInfo _comboPoints;
    uint32 _lastComboPointGenerated;
    uint32 _lastComboPointSpent;

    // Energy system
    uint32 _energy;
    uint32 _maxEnergy;
    uint32 _lastEnergyRegen;
    uint32 _energyRegenRate;

    // Feral buffs and debuffs
    uint32 _tigersFuryReady;
    uint32 _savageRoarRemaining;
    uint32 _lastTigersFury;
    uint32 _lastSavageRoar;

    // DoT tracking
    std::unordered_map<ObjectGuid, uint32> _rakeTimers;
    std::unordered_map<ObjectGuid, uint32> _ripTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalMeleeDamage;
    uint32 _comboPointsGenerated;
    uint32 _comboPointsSpent;
    uint32 _energySpent;

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 COMBO_POINTS_MAX = 5;
    static constexpr uint32 ENERGY_MAX = 100;
    static constexpr uint32 ENERGY_REGEN_RATE = 10; // per second
    static constexpr uint32 TIGERS_FURY_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SAVAGE_ROAR_DURATION = 34000; // 34 seconds at max combo points
    static constexpr uint32 RAKE_DURATION = 15000; // 15 seconds
    static constexpr uint32 RIP_DURATION = 22000; // 22 seconds at max combo points
};

} // namespace Playerbot