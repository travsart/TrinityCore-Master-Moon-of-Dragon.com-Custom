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

class TC_GAME_API BalanceSpecialization : public DruidSpecialization
{
public:
    explicit BalanceSpecialization(Player* bot);

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
    DruidSpec GetSpecialization() const override { return DruidSpec::BALANCE; }
    const char* GetSpecializationName() const override { return "Balance"; }

private:
    // Balance-specific mechanics
    void UpdateEclipseSystem();
    void UpdateStarsurge();
    bool ShouldCastStarfire(::Unit* target);
    bool ShouldCastWrath(::Unit* target);
    bool ShouldCastStarsurge(::Unit* target);
    bool ShouldCastMoonfire(::Unit* target);

    // Eclipse management
    void AdvanceEclipse();
    void CastEclipseSpell(::Unit* target);
    void ManageEclipseState();

    // Balance spell rotation
    void CastStarfire(::Unit* target);
    void CastWrath(::Unit* target);
    void CastStarsurge(::Unit* target);
    void CastMoonfire(::Unit* target);
    void CastForceOfNature();

    // Moonkin form management
    void EnterMoonkinForm();
    bool ShouldUseMoonkinForm();

    // Balance spell IDs
    enum BalanceSpells
    {
        STARFIRE = 2912,
        WRATH = 5176,
        STARSURGE = 78674,
        FORCE_OF_NATURE = 33831,
        ECLIPSE_SOLAR = 48517,
        ECLIPSE_LUNAR = 48518,
        SUNFIRE = 93402
    };

    // Eclipse state tracking
    EclipseState _eclipseState;
    uint32 _solarEnergy;
    uint32 _lunarEnergy;
    uint32 _lastEclipseShift;
    uint32 _starfireCount;
    uint32 _wrathCount;
    bool _eclipseActive;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalDamageDealt;
    uint32 _manaSpent;
    uint32 _eclipseProcs;

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 ECLIPSE_ENERGY_MAX = 100;
    static constexpr uint32 STARSURGE_COOLDOWN = 15000; // 15 seconds
    static constexpr uint32 FORCE_OF_NATURE_COOLDOWN = 180000; // 3 minutes
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
};

} // namespace Playerbot