/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DemonHunterSpecialization.h"
#include <map>
#include <vector>

namespace Playerbot
{

class TC_GAME_API HavocSpecialization : public DemonHunterSpecialization
{
public:
    explicit HavocSpecialization(Player* bot);

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

    // Metamorphosis management
    void UpdateMetamorphosis() override;
    bool ShouldUseMetamorphosis() override;
    void TriggerMetamorphosis() override;
    MetamorphosisState GetMetamorphosisState() const override;

    // Soul fragment management
    void UpdateSoulFragments() override;
    void ConsumeSoulFragments() override;
    bool ShouldConsumeSoulFragments() override;
    uint32 GetAvailableSoulFragments() const override;

    // Specialization info
    DemonHunterSpec GetSpecialization() const override { return DemonHunterSpec::HAVOC; }
    const char* GetSpecializationName() const override { return "Havoc"; }

private:
    // Havoc-specific mechanics
    void UpdateFuryManagement();
    void UpdateMobilityRotation();
    void UpdateOffensiveCooldowns();
    bool ShouldCastDemonsBite(::Unit* target);
    bool ShouldCastChaosStrike(::Unit* target);
    bool ShouldCastBladeDance();
    bool ShouldCastEyeBeam(::Unit* target);
    bool ShouldCastFelRush(::Unit* target);
    bool ShouldCastVengefulRetreat();

    // Fury management
    void GenerateFuryFromAbility(uint32 spellId);
    bool HasEnoughFury(uint32 required);
    uint32 GetFury();
    float GetFuryPercent();

    // Havoc abilities
    void CastDemonsBite(::Unit* target);
    void CastChaosStrike(::Unit* target);
    void CastBladeDance();
    void CastEyeBeam(::Unit* target);
    void CastFelRush(::Unit* target);
    void CastVengefulRetreat();
    void CastThrowGlaive(::Unit* target);
    void CastFelblade(::Unit* target);

    // Metamorphosis abilities
    void EnterHavocMetamorphosis();
    void CastDeathSweep();
    void CastAnnihilation(::Unit* target);

    // Cooldown management
    void UseOffensiveCooldowns();
    void CastNemesis(::Unit* target);
    void CastChaosBlades();

    // Havoc spell IDs
    enum HavocSpells
    {
        CHAOS_STRIKE = 162794,
        BLADE_DANCE = 188499,
        EYE_BEAM = 198013,
        NEMESIS = 206491,
        CHAOS_BLADES = 211048,
        ANNIHILATION = 201427,
        DEATH_SWEEP = 210152,
        MOMENTUM = 206476,
        PREPARED = 203650
    };

    // Fury system
    uint32 _fury;
    uint32 _maxFury;
    uint32 _lastFuryRegen;

    // Metamorphosis tracking
    uint32 _havocMetaRemaining;
    bool _inHavocMeta;
    uint32 _lastHavocMeta;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;
    uint32 _nemesisReady;
    uint32 _chaosBladesReady;
    uint32 _eyeBeamReady;

    // Mobility tracking
    uint32 _lastFelRush;
    uint32 _lastVengefulRetreat;
    uint32 _felRushCharges;
    Position _lastPosition;

    // Performance tracking
    uint32 _totalDamageDealt;
    uint32 _furySpent;
    uint32 _soulFragmentsConsumed;

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 FURY_MAX = 120;
    static constexpr uint32 HAVOC_META_DURATION = 30000; // 30 seconds
    static constexpr uint32 NEMESIS_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 CHAOS_BLADES_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 EYE_BEAM_COOLDOWN = 45000; // 45 seconds
    static constexpr uint32 FEL_RUSH_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 VENGEFUL_RETREAT_COOLDOWN = 25000; // 25 seconds
    static constexpr float FURY_GENERATION_THRESHOLD = 0.7f; // 70%
    static constexpr uint32 SOUL_FRAGMENT_CONSUME_THRESHOLD = 3;
};

} // namespace Playerbot