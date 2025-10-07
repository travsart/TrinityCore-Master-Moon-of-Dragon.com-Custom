/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "../ClassAI.h"
#include "DemonHunterSpecialization.h"
#include <memory>
#include <chrono>
#include <unordered_map>

namespace Playerbot
{

class DemonHunterSpecialization;
class HavocDemonHunterRefactored;
class VengeanceDemonHunterRefactored;

// DemonHunter Spell IDs (WoW 11.2 The War Within)
enum DemonHunterSpells
{
    // Havoc abilities
    CHAOS_STRIKE = 162794,
    BLADE_DANCE = 188499,
    DEATH_SWEEP = 210152,
    ANNIHILATION = 201427,
    METAMORPHOSIS_HAVOC = 191427,
    EYE_BEAM = 198013,
    DEMONS_BITE = 162243,
    FEL_BARRAGE = 258925,

    // Vengeance abilities
    SOUL_CLEAVE = 228477,
    SPIRIT_BOMB = 247454,
    METAMORPHOSIS_VENGEANCE = 187827,
    SHEAR = 203782,
    SOUL_BARRIER = 227225,

    // Defensive abilities
    BLUR = 198589,
    DARKNESS = 196718,
    NETHERWALK = 196555,

    // Shared abilities
    DEMON_SPIKES = 203720,
    IMMOLATION_AURA = 258920,
    FIERY_BRAND = 204021,
    FEL_RUSH = 195072,
    VENGEFUL_RETREAT = 198793,
    CONSUME_MAGIC = 278326,
    SIGIL_OF_FLAME = 204596,
    SIGIL_OF_SILENCE = 202137,
    SIGIL_OF_MISERY = 207684,
    CHAOS_NOVA = 179057,

    // Utility/Interrupts
    DISRUPT = 183752,
    IMPRISON = 217832,

    // Offensive cooldowns
    NEMESIS = 206491,

    // Talent abilities
    MOMENTUM_TALENT = 206476,
    DEMONIC_TALENT = 213410,
    BLIND_FURY_TALENT = 203550
};

class TC_GAME_API DemonHunterAI : public ClassAI
{
public:
    explicit DemonHunterAI(Player* bot);
    ~DemonHunterAI() = default;

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization management
    void DetectSpecialization();
    void InitializeSpecialization();
    void ExitMetamorphosis();
    bool ShouldUseMetamorphosis();
    void CastMetamorphosisHavoc();
    void CastMetamorphosisVengeance();
    void SpendPain(uint32 amount);
    void GeneratePain(uint32 amount);
    bool HasPain(uint32 amount);
    void SpendFury(uint32 amount);
    void GenerateFury(uint32 amount);
    bool HasFury(uint32 amount);
    void UpdatePainManagement(uint32 diff);
    void DecayPain(uint32 diff);
    uint32 GetFury() const;
    uint32 GetMaxFury() const;
    uint32 GetPain() const;
    uint32 GetMaxPain() const;

    // Combat rotation methods
    void UpdateHavocRotation(::Unit* target);
    void UpdateVengeanceRotation(::Unit* target);
    void HandleMetamorphosisAbilities(::Unit* target);
    void ExecuteBasicDemonHunterRotation(::Unit* target);

    // Priority system methods (CombatBehaviorIntegration)
    void HandleInterrupts(::Unit* target);
    void HandleDefensives();
    void HandleTargetSwitching(::Unit*& target);
    void HandleAoEDecisions(::Unit* target);
    void HandleCooldowns(::Unit* target);
    void HandleResourceGeneration(::Unit* target);
    void HandleMobility(::Unit* target);

    // Ability casting methods
    void CastEyeBeam(::Unit* target);
    void CastChaosStrike(::Unit* target);
    void CastBladeDance(::Unit* target);
    void CastDemonsBite(::Unit* target);
    void CastSoulCleave(::Unit* target);
    void CastShear(::Unit* target);

    // Utility methods
    std::vector<::Unit*> GetAoETargets(float range = 8.0f);
    uint32 GetNearbyEnemyCount(float range) const;
    bool IsInMeleeRange(::Unit* target) const;
    bool IsTargetInterruptible(::Unit* target) const;
    void RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success);
    void RecordAbilityUsage(uint32 spellId);
    void OnTargetChanged(::Unit* newTarget);

    DemonHunterSpec GetCurrentSpecialization() const;

    // Member variables
    DemonHunterSpec _detectedSpec;
    std::unique_ptr<DemonHunterSpecialization> _specialization;

    // Combat tracking
    uint32 _lastInterruptTime;
    uint32 _lastDefensiveTime;
    uint32 _lastMobilityTime;
    uint32 _successfulInterrupts;
    std::unordered_map<uint32, uint32> _abilityUsage;

    // Metrics tracking
    struct DemonHunterMetrics
    {
        uint32 totalAbilitiesUsed;
        uint32 interruptsSucceeded;
        uint32 defensivesUsed;
        uint32 mobilityAbilitiesUsed;
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastMetricsUpdate;
    };
    DemonHunterMetrics _dhMetrics;

    // Helper methods
    void SwitchSpecialization(DemonHunterSpec newSpec);
    void DelegateToSpecialization(::Unit* target);
    void UpdateMetrics(uint32 diff);
    void AnalyzeCombatEffectiveness();
};

} // namespace Playerbot