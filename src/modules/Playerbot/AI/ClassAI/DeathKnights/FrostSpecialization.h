/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
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

class TC_GAME_API FrostSpecialization : public DeathKnightSpecialization
{
public:
    explicit FrostSpecialization(Player* bot);

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
    DeathKnightSpec GetSpecialization() const override { return DeathKnightSpec::FROST; }
    const char* GetSpecializationName() const override { return "Frost"; }

private:
    // Frost-specific mechanics
    void UpdateFrostRotation();
    void UpdateKillingMachineProcs();
    void UpdateRimeProcs();
    bool ShouldCastObliterate(::Unit* target);
    bool ShouldCastFrostStrike(::Unit* target);
    bool ShouldCastHowlingBlast(::Unit* target);
    bool ShouldCastUnbreakableWill();
    bool ShouldCastDeathchill();
    bool ShouldCastEmpowerRuneWeapon();

    // Proc management
    void UpdateProcManagement();
    bool HasKillingMachineProc();
    bool HasRimeProc();
    void ConsumeKillingMachineProc();
    void ConsumeRimeProc();

    // Frost abilities
    void CastObliterate(::Unit* target);
    void CastFrostStrike(::Unit* target);
    void CastHowlingBlast(::Unit* target);
    void CastIcyTouch(::Unit* target);
    void CastChainsOfIce(::Unit* target);
    void CastMindFreeze(::Unit* target);

    // Offensive cooldowns
    void CastUnbreakableWill();
    void CastDeathchill();
    void CastEmpowerRuneWeapon();
    void UseOffensiveCooldowns();

    // Frost presence management
    void EnterFrostPresence();
    bool ShouldUseFrostPresence();

    // Dual-wield vs Two-handed management
    void UpdateWeaponStrategy();
    bool IsDualWielding();
    bool ShouldUseDualWieldRotation();

    // Frost spell IDs
    enum FrostSpells
    {
        OBLITERATE = 49020,
        FROST_STRIKE = 49143,
        HOWLING_BLAST = 49184,
        CHAINS_OF_ICE = 45524,
        MIND_FREEZE = 47528,
        UNBREAKABLE_WILL = 51271,
        DEATHCHILL = 49796,
        EMPOWER_RUNE_WEAPON = 47568,
        KILLING_MACHINE = 51128,
        RIME = 59057,
        MERCILESS_COMBAT = 49024,
        BLOOD_OF_THE_NORTH = 54637
    };

    // Proc tracking
    bool _killingMachineActive;
    bool _rimeActive;
    uint32 _killingMachineExpires;
    uint32 _rimeExpires;
    uint32 _lastProcCheck;

    // Offensive cooldowns
    uint32 _unbreakableWillReady;
    uint32 _deathchillReady;
    uint32 _empowerRuneWeaponReady;
    uint32 _lastUnbreakableWill;
    uint32 _lastDeathchill;
    uint32 _lastEmpowerRuneWeapon;

    // Weapon strategy
    bool _isDualWielding;
    bool _preferDualWield;
    uint32 _lastWeaponCheck;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalDamageDealt;
    uint32 _procActivations;
    uint32 _runicPowerSpent;

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 KILLING_MACHINE_DURATION = 30000; // 30 seconds
    static constexpr uint32 RIME_DURATION = 15000; // 15 seconds
    static constexpr uint32 UNBREAKABLE_WILL_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 DEATHCHILL_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 EMPOWER_RUNE_WEAPON_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 PROC_CHECK_INTERVAL = 500; // 0.5 seconds
    static constexpr uint32 WEAPON_CHECK_INTERVAL = 5000; // 5 seconds
    static constexpr float RUNIC_POWER_THRESHOLD = 0.8f; // 80%
};

} // namespace Playerbot