/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "MageSpecialization.h"
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API FrostSpecialization : public MageSpecialization
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

    // Specialization info
    MageSpec GetSpecialization() const override { return MageSpec::FROST; }
    const char* GetSpecializationName() const override { return "Frost"; }

private:
    // Frost-specific mechanics
    void UpdateIcicles();
    void UpdateFrozenTargets();
    void UpdateShatter();
    bool HasFingersOfFrost();
    bool HasBrainFreeze();
    bool IsTargetFrozen(::Unit* target);
    bool CanShatter(::Unit* target);
    uint32 GetIcicleCount();

    // Spell priority and conditions
    bool ShouldCastFrostbolt();
    bool ShouldCastIceLance();
    bool ShouldCastFlurry();
    bool ShouldCastGlacialSpike();
    bool ShouldCastConeOfCold();
    bool ShouldCastBlizzard();
    bool ShouldUseIcyVeins();

    // Spell casting methods
    void CastFrostbolt();
    void CastIceLance();
    void CastFlurry();
    void CastGlacialSpike();
    void CastConeOfCold();
    void CastBlizzard();
    void CastFrostNova();
    void CastIceBarrier();
    void CastIcyVeins();
    void CastSummonWaterElemental();

    // Icicle mechanics
    void LaunchIcicles();
    void BuildIcicles();
    bool HasMaxIcicles();

    // Crowd control and kiting
    void HandleKiting(::Unit* target);
    void ApplySlows(::Unit* target);
    bool NeedsToKite(::Unit* target);
    float GetKitingDistance(::Unit* target);
    void CastSlowingSpells(::Unit* target);

    // AoE and multi-target
    void HandleAoERotation();
    std::vector<::Unit*> GetFrozenEnemies();
    bool ShouldUseAoE();
    void CastFrozenOrb();
    void CastIceNova();

    // Pet management (Water Elemental)
    void UpdateWaterElemental();
    void CommandWaterElemental();
    bool HasWaterElemental();
    void SummonWaterElementalIfNeeded();

    // Defensive abilities
    void UpdateDefensiveSpells();
    void CastDefensiveSpells();
    bool ShouldUseIceBlock();
    bool ShouldUseIceBarrier();

    // Cooldown management
    void UpdateFrostCooldowns(uint32 diff);
    void CheckFrostBuffs();
    void UseCooldowns();

    // Shatter combo execution
    void ExecuteShatterCombo(::Unit* target);
    void SetupShatter(::Unit* target);
    bool IsShatterReady(::Unit* target);

    // Frost spell IDs
    enum FrostSpells
    {
        FROSTBOLT = 116,
        ICE_LANCE = 30455,
        FLURRY = 44614,
        GLACIAL_SPIKE = 199786,
        CONE_OF_COLD = 120,
        BLIZZARD = 190356,
        FROST_NOVA = 122,
        ICE_BARRIER = 11426,
        ICY_VEINS = 12472,
        SUMMON_WATER_ELEMENTAL = 31687,
        FROZEN_ORB = 84714,
        ICE_NOVA = 157997,
        FINGERS_OF_FROST = 44544,
        BRAIN_FREEZE = 190446,
        ICICLES = 205473,
        SHATTER = 12982,
        FREEZE = 33395, // Water Elemental ability
        WATER_JET = 135029 // Water Elemental ability
    };

    // State tracking
    uint32 _icicleCount;
    bool _hasFingersOfFrost;
    bool _hasBrainFreeze;
    uint32 _lastShatterTime;
    uint32 _icyVeinsEndTime;
    bool _inIcyVeins;

    // Frozen target tracking
    std::map<uint64, uint32> _frozenTargets;
    std::map<uint64, uint32> _slowedTargets;

    // Water Elemental tracking
    uint64 _waterElementalGuid;
    uint32 _lastElementalCommand;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastFrozenCheck;
    uint32 _lastKitingCheck;
    uint32 _lastElementalCheck;
    uint32 _lastDefensiveCheck;
    uint32 _lastRotationUpdate;

    // Kiting mechanics
    Position _lastKitePosition;
    uint32 _lastKiteTime;
    bool _isKiting;

    // Constants
    static constexpr uint32 MAX_ICICLES = 5;
    static constexpr uint32 FINGERS_OF_FROST_DURATION = 15000; // 15 seconds
    static constexpr uint32 BRAIN_FREEZE_DURATION = 15000; // 15 seconds
    static constexpr uint32 ICY_VEINS_DURATION = 20000; // 20 seconds
    static constexpr uint32 SHATTER_WINDOW = 1500; // 1.5 seconds
    static constexpr float KITING_DISTANCE = 25.0f;
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr float CONE_OF_COLD_RANGE = 10.0f;
    static constexpr float BLIZZARD_RANGE = 8.0f;
    static constexpr uint32 WATER_ELEMENTAL_DURATION = 60000; // 60 seconds
};

} // namespace Playerbot