/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ShamanSpecialization.h"
#include <array>
#include <map>

namespace Playerbot
{

class TC_GAME_API EnhancementSpecialization : public ShamanSpecialization
{
public:
    explicit EnhancementSpecialization(Player* bot);

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

    // Totem management
    void UpdateTotemManagement() override;
    void DeployOptimalTotems() override;
    uint32 GetOptimalFireTotem() override;
    uint32 GetOptimalEarthTotem() override;
    uint32 GetOptimalWaterTotem() override;
    uint32 GetOptimalAirTotem() override;

    // Shock rotation
    void UpdateShockRotation(::Unit* target) override;
    uint32 GetNextShockSpell(::Unit* target) override;

    // Specialization info
    ShamanSpec GetSpecialization() const override { return ShamanSpec::ENHANCEMENT; }
    const char* GetSpecializationName() const override { return "Enhancement"; }

private:
    // Enhancement-specific mechanics
    void UpdateWeaponImbues();
    void UpdateMaelstromWeapon();
    void UpdateStormstrike();
    void UpdateFeralSpirit();
    void UpdateShamanisticRage();
    bool ShouldCastStormstrike(::Unit* target);
    bool ShouldCastLavaLash(::Unit* target);
    bool ShouldCastLightningBolt(::Unit* target);
    bool ShouldCastChainLightning();
    bool ShouldUseShamanisticRage();
    bool ShouldCastFeralSpirit();

    // Weapon imbue management
    void ApplyWeaponImbues();
    void CastFlametongueWeapon();
    void CastWindfuryWeapon();
    void CastFrostbrandWeapon();
    void RefreshWeaponImbue(bool mainHand);
    bool HasWeaponImbue(bool mainHand);
    uint32 GetWeaponImbueRemainingTime(bool mainHand);

    // Maelstrom weapon management
    void ConsumeMaelstromWeapon();
    bool ShouldConsumeMaelstromWeapon();
    uint32 GetMaelstromWeaponStacks();
    void CastInstantLightningBolt(::Unit* target);
    void CastInstantChainLightning();

    // Melee abilities
    void CastStormstrike(::Unit* target);
    void CastLavaLash(::Unit* target);
    void CastPrimalStrike(::Unit* target);
    void CastEarthElemental();

    // Utility abilities
    void CastShamanisticRage();
    void CastFeralSpirit();
    void CastBloodlust();

    // Dual wield mechanics
    bool IsDualWielding();
    void OptimizeDualWield();
    void ManageOffHandAttacks();

    // Positioning for melee
    void UpdateEnhancementPositioning();
    bool IsInMeleeRange(::Unit* target);
    Position GetOptimalMeleePosition(::Unit* target);

    // Enhancement spell IDs
    enum EnhancementSpells
    {
        STORMSTRIKE = 17364,
        LAVA_LASH = 60103,
        PRIMAL_STRIKE = 73899,
        SHAMANISTIC_RAGE = 30823,
        FERAL_SPIRIT = 51533,
        MAELSTROM_WEAPON = 51530,
        WINDFURY_WEAPON = 8232,
        FLAMETONGUE_WEAPON = 8024,
        FROSTBRAND_WEAPON = 8033,
        EARTHLIVING_WEAPON = 51730,
        LIGHTNING_BOLT_INSTANT = 403,
        CHAIN_LIGHTNING_INSTANT = 421,
        EARTH_ELEMENTAL = 2062,
        FIRE_ELEMENTAL = 2894
    };

    // Enhancement state tracking
    std::array<WeaponImbue, 2> _weaponImbues; // Main hand + Off hand
    uint32 _stormstrikeCharges;
    uint32 _maelstromWeaponStacks;
    uint32 _unleashedFuryStacks;
    uint32 _lastFlametongueRefresh;
    uint32 _lastWindfuryRefresh;
    uint32 _lastStormstrike;
    uint32 _lastLavaLash;
    uint32 _lastShamanisticRage;
    uint32 _lastFeralSpirit;
    bool _dualWielding;
    bool _hasShamanisticRage;
    bool _hasFeralSpirit;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalMeleeDamage;
    uint32 _instantSpellsCast;
    uint32 _weaponImbueProcs;

    // Constants
    // static const float MELEE_RANGE;
    static const uint32 MAELSTROM_WEAPON_MAX_STACKS = 5;
    static const uint32 WEAPON_IMBUE_CHECK_INTERVAL = 5000; // 5 seconds
    static const uint32 STORMSTRIKE_COOLDOWN = 8000; // 8 seconds
    static const uint32 LAVA_LASH_COOLDOWN = 6000; // 6 seconds
    static const uint32 SHAMANISTIC_RAGE_COOLDOWN = 60000; // 1 minute
    static const uint32 FERAL_SPIRIT_COOLDOWN = 120000; // 2 minutes
    static const uint32 FLAMETONGUE_DURATION = 3600000; // 1 hour
    static const uint32 WINDFURY_DURATION = 3600000; // 1 hour
    static const uint32 FROSTBRAND_DURATION = 3600000; // 1 hour
};

} // namespace Playerbot