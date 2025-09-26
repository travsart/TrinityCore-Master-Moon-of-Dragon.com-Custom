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
#include <atomic>
#include <chrono>
#include <memory>
#include "BotThreatManager.h"

namespace Playerbot
{

class TC_GAME_API ArcaneSpecialization : public MageSpecialization
{
public:
    explicit ArcaneSpecialization(Player* bot);

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
    MageSpec GetSpecialization() const override { return MageSpec::ARCANE; }
    const char* GetSpecializationName() const override { return "Arcane"; }

private:
    // Arcane-specific mechanics
    void UpdateArcaneCharges();
    void UpdateManaGems();
    bool ShouldConserveMana();
    bool ShouldUseManaGem();
    uint32 GetArcaneCharges();
    void CastArcaneMissiles();
    void CastArcaneBlast();
    void CastArcaneOrb();
    void CastArcaneBarrage();
    void CastPresenceOfMind();
    void CastArcanePower();
    void CastManaShield();
    void UseManaGem();

    // Arcane blast stacking mechanics
    uint32 GetArcaneBlastStacks();
    bool ShouldCastArcaneBlast();
    bool ShouldCastArcaneBarrage();
    bool ShouldCastArcaneMissiles();

    // Cooldown and buff management
    void UpdateArcaneCooldowns(uint32 diff);
    void CheckArcaneBuffs();
    bool HasClearcastingProc();
    bool HasPresenceOfMind();
    bool HasArcanePower();

    // Mana management specific to Arcane
    void OptimizeManaUsage();
    bool IsInBurnPhase();
    bool IsInConservePhase();
    void EnterBurnPhase();
    void EnterConservePhase();

    // Arcane spell IDs
    enum ArcaneSpells
    {
        ARCANE_MISSILES = 5143,
        ARCANE_BLAST = 30451,
        ARCANE_BARRAGE = 44425,
        ARCANE_ORB = 153626,
        PRESENCE_OF_MIND = 12043,
        ARCANE_POWER = 12042,
        MANA_SHIELD = 1463,
        MANA_GEM = 759,
        CLEARCASTING = 12536,
        ARCANE_CHARGES = 36032
    };

    // Enhanced state tracking
    std::atomic<uint32> _arcaneBlastStacks{0};
    uint32 _lastArcaneSpellTime;
    std::atomic<bool> _inBurnPhase{false};
    std::atomic<bool> _inConservePhase{true};
    uint32 _burnPhaseStartTime;
    uint32 _conservePhaseStartTime;
    std::chrono::steady_clock::time_point _phaseStartTime;

    // Advanced mechanics
    void UpdateArcaneOrb();
    void ManageArcaneChargeOptimization();
    void HandleArcaneIntellectBuff();
    void OptimizeArcaneMissilesTiming();
    bool ShouldDelayArcaneBlast();
    void HandleManaAdeptProc();
    void UpdateTimeWarpEffects();

    // Burn phase optimization
    void OptimizeBurnPhaseRotation(::Unit* target);
    void CalculateOptimalBurnDuration();
    bool ShouldExtendBurnPhase();
    void HandleBurnPhaseEmergency();

    // Conserve phase optimization
    void OptimizeConservePhaseRotation(::Unit* target);
    void HandleManaRegeneration();
    bool ShouldTransitionToBurn();

    // Advanced cooldown management
    void OptimizeCooldownUsage();
    bool ShouldHoldCooldownsForBurn();
    void HandleCooldownSynergy();

    // Performance metrics
    struct ArcaneMetrics {
        std::atomic<uint32> totalArcaneBlasts{0};
        std::atomic<uint32> fourStackBlasts{0};
        std::atomic<uint32> wasted Charges{0};
        std::atomic<float> averageCharges{0.0f};
        std::atomic<float> burnPhaseEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalArcaneBlasts = 0; fourStackBlasts = 0; wastedCharges = 0;
            averageCharges = 0.0f; burnPhaseEfficiency = 0.0f; manaEfficiency = 0.0f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    } _metrics;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastManaCheck;
    uint32 _lastBuffCheck;
    uint32 _lastRotationUpdate;

    // Enhanced constants
    static constexpr uint32 ARCANE_BLAST_MAX_STACKS = 4;
    static constexpr uint32 OPTIMAL_BURN_DURATION = 12000; // 12 seconds optimal
    static constexpr uint32 MAX_BURN_DURATION = 18000; // 18 seconds maximum
    static constexpr uint32 MIN_CONSERVE_DURATION = 15000; // 15 seconds minimum
    static constexpr float BURN_ENTRY_THRESHOLD = 0.85f; // 85% mana to start burn
    static constexpr float BURN_EXIT_THRESHOLD = 0.25f; // 25% mana to exit burn
    static constexpr float CONSERVE_EXIT_THRESHOLD = 0.80f; // 80% mana to exit conserve
    static constexpr float MANA_GEM_THRESHOLD = 0.15f; // 15% mana for gem usage
    static constexpr float ARCANE_ORB_EFFICIENCY = 1.2f; // Orb efficiency modifier
    static constexpr uint32 ARCANE_CHARGE_DURATION = 10000; // 10 seconds
    static constexpr float MISSILE_PROC_PRIORITY = 1.5f; // Clearcasting priority
    static constexpr uint32 TIME_WARP_DURATION = 40000; // 40 seconds
};

} // namespace Playerbot