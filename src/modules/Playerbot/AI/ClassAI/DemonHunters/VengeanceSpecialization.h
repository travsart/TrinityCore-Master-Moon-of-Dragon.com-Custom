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

class TC_GAME_API VengeanceSpecialization : public DemonHunterSpecialization
{
public:
    explicit VengeanceSpecialization(Player* bot);

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
    DemonHunterSpec GetSpecialization() const override { return DemonHunterSpec::VENGEANCE; }
    const char* GetSpecializationName() const override { return "Vengeance"; }

private:
    // Vengeance-specific mechanics
    void UpdatePainManagement();
    void UpdateThreatManagement();
    void UpdateDefensiveCooldowns();
    bool ShouldCastShear(::Unit* target);
    bool ShouldCastSoulCleave();
    bool ShouldCastImmolationAura();
    bool ShouldCastDemonSpikes();
    bool ShouldCastFieryBrand(::Unit* target);
    bool ShouldCastInfernalStrike(::Unit* target);

    // Pain management
    void GeneratePainFromAbility(uint32 spellId);
    bool HasEnoughPain(uint32 required);
    uint32 GetPain();
    float GetPainPercent();

    // Threat management
    void BuildThreat(::Unit* target);
    void MaintainThreat();
    std::vector<::Unit*> GetThreatTargets();
    bool NeedsThreat(::Unit* target);

    // Vengeance abilities
    void CastShear(::Unit* target);
    void CastSoulCleave();
    void CastImmolationAura();
    void CastSigilOfFlame(Position targetPos);
    void CastInfernalStrike(::Unit* target);
    void CastThrowGlaive(::Unit* target);
    void CastFelblade(::Unit* target);

    // Defensive abilities
    void CastDemonSpikes();
    void CastFieryBrand(::Unit* target);
    void CastSoulBarrier();
    void UseDefensiveCooldowns();
    void ManageEmergencyAbilities();

    // Metamorphosis abilities
    void EnterVengeanceMetamorphosis();
    void CastSoulSunder(::Unit* target);

    // Sigil management
    void UpdateSigilManagement();
    void CastSigilOfSilence(Position targetPos);
    void CastSigilOfMisery(Position targetPos);
    void CastSigilOfChains(Position targetPos);

    // Vengeance spell IDs
    enum VengeanceSpells
    {
        SHEAR = 203782,
        SOUL_CLEAVE = 228477,
        IMMOLATION_AURA = 178740,
        DEMON_SPIKES = 203720,
        FIERY_BRAND = 204021,
        SOUL_BARRIER = 227225,
        SIGIL_OF_FLAME = 204596,
        SIGIL_OF_SILENCE = 202137,
        SIGIL_OF_MISERY = 207684,
        SIGIL_OF_CHAINS = 202138,
        SOUL_SUNDER = 228478,
        FRACTURE = 263642,
        SPIRIT_BOMB = 247454
    };

    // Pain system
    uint32 _pain;
    uint32 _maxPain;
    uint32 _lastPainRegen;

    // Metamorphosis tracking
    uint32 _vengeanceMetaRemaining;
    bool _inVengeanceMeta;
    uint32 _lastVengeanceMeta;

    // Defensive cooldowns
    uint32 _demonSpikesCharges;
    uint32 _demonSpikesReady;
    uint32 _fieryBrandReady;
    uint32 _soulBarrierReady;
    uint32 _lastDemonSpikes;
    uint32 _lastFieryBrand;
    uint32 _lastSoulBarrier;

    // Sigil tracking
    std::unordered_map<uint32, uint32> _sigilCooldowns;
    uint32 _lastSigil;

    // Threat tracking
    std::vector<::Unit*> _threatTargets;
    uint32 _lastThreatUpdate;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance tracking
    uint32 _totalThreatGenerated;
    uint32 _painSpent;
    uint32 _damageAbsorbed;

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 PAIN_MAX = 100;
    static constexpr uint32 VENGEANCE_META_DURATION = 15000; // 15 seconds
    static constexpr uint32 DEMON_SPIKES_DURATION = 6000; // 6 seconds
    static constexpr uint32 DEMON_SPIKES_COOLDOWN = 20000; // 20 seconds
    static constexpr uint32 FIERY_BRAND_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SOUL_BARRIER_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SIGIL_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 INFERNAL_STRIKE_COOLDOWN = 20000; // 20 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f; // 30%
    static constexpr float PAIN_GENERATION_THRESHOLD = 0.8f; // 80%
    static constexpr uint32 SOUL_FRAGMENT_CONSUME_THRESHOLD = 5;
};

} // namespace Playerbot