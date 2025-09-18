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
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class ArcanePhase : uint8
{
    OPENING         = 0,  // Initial engagement setup
    BURN_WINDOW     = 1,  // High mana expenditure phase
    CONSERVE_PHASE  = 2,  // Mana conservation and regeneration
    TRANSITION      = 3,  // Phase switching period
    EMERGENCY       = 4,  // Critical situations
    EXECUTE         = 5   // Low health burn phase
};

enum class ManaState : uint8
{
    ABUNDANT        = 0,  // >80% mana
    COMFORTABLE     = 1,  // 60-80% mana
    MODERATE        = 2,  // 40-60% mana
    LOW             = 3,  // 20-40% mana
    CRITICAL        = 4,  // <20% mana
    EMERGENCY       = 5   // <10% mana
};

enum class ArcaneProc : uint8
{
    NONE            = 0,
    CLEARCASTING    = 1,  // Arcane Missiles proc
    MANA_ADEPT      = 2,  // Arcane Blast enhancement
    TIME_WARP       = 3,  // Haste effect
    ARCANE_POWER    = 4,  // Damage amplification
    PRESENCE_MIND   = 5   // Instant cast buff
};

struct ArcaneChargeState
{
    uint32 currentStacks;
    uint32 lastApplicationTime;
    uint32 decayTime;
    float damageMultiplier;
    float manaCostMultiplier;
    bool shouldMaintain;

    ArcaneChargeState() : currentStacks(0), lastApplicationTime(0), decayTime(0)
        , damageMultiplier(1.0f), manaCostMultiplier(1.0f), shouldMaintain(false) {}
};

/**
 * @brief Enhanced Arcane specialization with intelligent mana management
 *
 * Focuses on sophisticated burn/conserve phase optimization, proc management,
 * and advanced mana efficiency through dynamic rotation adaptation.
 */
class TC_GAME_API ArcaneSpecialization_Enhanced : public MageSpecialization
{
public:
    explicit ArcaneSpecialization_Enhanced(Player* bot);
    ~ArcaneSpecialization_Enhanced() override = default;

    // Core rotation interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Advanced burn phase management
    void ExecuteBurnPhase(::Unit* target);
    void OptimizeBurnWindowTiming();
    void CalculateOptimalBurnDuration();
    bool ShouldInitiateBurnPhase() const;
    bool ShouldExtendBurnPhase() const;
    void PrepareBurnPhaseEntry();

    // Sophisticated conserve phase management
    void ExecuteConservePhase(::Unit* target);
    void OptimizeManaRegeneration();
    void HandleManaEfficiencyRotation(::Unit* target);
    bool ShouldExitConservePhase() const;
    void MaximizeManaEfficiency();

    // Arcane charge mastery
    void ManageArcaneChargesOptimally();
    void OptimizeChargeStacking(::Unit* target);
    void HandleChargeDecayPrevention();
    uint32 CalculateOptimalStackCount() const;
    bool ShouldResetCharges() const;

    // Advanced proc utilization
    void HandleArcaneProcs();
    void OptimizeClearcastingUsage(::Unit* target);
    void ManageManaAdeptProcs();
    void CoordinateProcWindows();
    void MaximizeProcEfficiency();

    // Cooldown optimization mastery
    void ExecuteAdvancedCooldownRotation();
    void OptimizeCooldownSynergy();
    void HandleBurstWindowCoordination();
    void ManageDefensiveCooldowns();

    // Performance analytics
    struct ArcaneMetrics
    {
        std::atomic<uint32> burnPhasesExecuted{0};
        std::atomic<uint32> conservePhasesExecuted{0};
        std::atomic<uint32> arcaneBlastsCast{0};
        std::atomic<uint32> arcaneBarragesCast{0};
        std::atomic<uint32> arcaneMissilesCast{0};
        std::atomic<uint32> clearcastingProcsUsed{0};
        std::atomic<uint32> manaGemsUsed{0};
        std::atomic<float> averageManaEfficiency{0.85f};
        std::atomic<float> burnPhaseEfficiency{0.9f};
        std::atomic<float> procUtilizationRate{0.8f};
        std::atomic<uint32> optimalChargeStacks{0};
        std::atomic<uint32> wastedCharges{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            burnPhasesExecuted = 0; conservePhasesExecuted = 0; arcaneBlastsCast = 0;
            arcaneBarragesCast = 0; arcaneMissilesCast = 0; clearcastingProcsUsed = 0;
            manaGemsUsed = 0; averageManaEfficiency = 0.85f; burnPhaseEfficiency = 0.9f;
            procUtilizationRate = 0.8f; optimalChargeStacks = 0; wastedCharges = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    ArcaneMetrics GetSpecializationMetrics() const { return _metrics; }

    // Mana gem mastery
    void OptimizeManaGemUsage();
    void CreateManaGemsIntelligently();
    bool ShouldUseManaGem() const;
    void HandleManaGemCooldowns();

    // Advanced positioning for arcane casters
    void OptimizeArcanePositioning(::Unit* target);
    void HandleChannelingInterruptions();
    void ManageCastingMovement();
    void ExecuteEmergencyRepositioning();

    // Situational adaptation
    void AdaptToEncounterType(uint32 encounterId);
    void OptimizeForBossPhases();
    void HandleMultiTargetOptimization();
    void AdaptToGroupComposition();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteBurnWindow(::Unit* target);
    void ExecuteConserveWindow(::Unit* target);
    void ExecuteTransitionPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastArcaneBlast(::Unit* target) const;
    bool ShouldCastArcaneBarrage(::Unit* target) const;
    bool ShouldCastArcaneMissiles(::Unit* target) const;
    bool ShouldCastArcaneOrb(::Unit* target) const;
    bool ShouldCastTimeWarp() const;

    // Advanced spell execution
    void ExecuteArcaneBlast(::Unit* target);
    void ExecuteArcaneBarrage(::Unit* target);
    void ExecuteArcaneMissiles(::Unit* target);
    void ExecuteArcaneOrb(::Unit* target);
    void ExecuteTimeWarp();

    // Cooldown management
    bool ShouldUseArcanePower() const;
    bool ShouldUsePresenceOfMind() const;
    bool ShouldUseMirrorImage() const;
    bool ShouldUseIcyVeins() const;
    bool ShouldUseManaShield() const;

    // Advanced cooldown execution
    void ExecuteArcanePower();
    void ExecutePresenceOfMind();
    void ExecuteMirrorImage();
    void ExecuteIcyVeins();
    void ExecuteManaShield();

    // Phase management algorithms
    void AnalyzePhaseTransitionOptimality();
    void CalculatePhaseTimings();
    void OptimizePhaseSequencing();
    ArcanePhase DetermineOptimalPhase() const;

    // Mana management intelligence
    ManaState AnalyzeManaState() const;
    void PredictManaNeeds();
    void OptimizeManaSpending();
    void HandleManaEmergencies();

    // Charge stack optimization
    void UpdateChargeStackAnalysis();
    bool IsChargeStackOptimal() const;
    void OptimizeChargeConsumption(::Unit* target);
    uint32 CalculateOptimalBarrageTiming() const;

    // Proc detection and handling
    void DetectActiveProcs();
    void PrioritizeProcUsage();
    void HandleProcCombinations();
    void MaximizeProcWindows();

    // Target analysis for arcane optimization
    void AnalyzeTargetCharacteristics(::Unit* target);
    void PredictTargetLifetime(::Unit* target);
    void AdaptRotationToTarget(::Unit* target);
    void HandleTargetResistances(::Unit* target);

    // Resource prediction and planning
    void PredictResourceTrajectory();
    void PlanOptimalSpending();
    void HandleResourceBottlenecks();
    void OptimizeResourceAllocation();

    // Performance optimization
    void AnalyzeRotationEfficiency();
    void OptimizeActionTimings();
    void TrackPerformanceMetrics();
    void AdaptBasedOnMetrics();

    // Emergency handling
    void HandleLowManaEmergency();
    void HandleInterruptEmergency();
    void HandleThreatEmergency();
    void ExecuteEmergencyEscape();

    // State tracking
    ArcanePhase _currentPhase;
    ManaState _currentManaState;
    ArcaneChargeState _chargeState;

    // Phase timing data
    uint32 _burnPhaseStartTime;
    uint32 _conservePhaseStartTime;
    uint32 _phaseTransitionTime;
    uint32 _optimalBurnDuration;
    uint32 _optimalConserveDuration;

    // Proc tracking
    std::unordered_map<ArcaneProc, uint32> _activeProcDurations;
    std::unordered_map<ArcaneProc, uint32> _procCooldowns;
    uint32 _lastClearcastingProc;
    uint32 _lastManaAdeptProc;
    bool _timeWarpActive;

    // Cooldown tracking
    uint32 _arcanePowerCooldown;
    uint32 _presenceOfMindCooldown;
    uint32 _mirrorImageCooldown;
    uint32 _icyVeinsCooldown;
    uint32 _manaShieldCooldown;
    uint32 _manaGemCooldown;

    // Mana management data
    uint32 _baseManaRegen;
    uint32 _lastManaCheck;
    float _manaEfficiencyTarget;
    uint32 _totalManaSpent;
    uint32 _totalManaRegenerated;

    // Charge stack data
    uint32 _arcaneCharges;
    uint32 _lastChargeApplication;
    uint32 _chargeDecayTime;
    uint32 _optimalChargeCount;
    uint32 _wastedChargeStacks;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalDamageDealt;
    uint32 _totalSpellsCast;
    uint32 _totalCriticalHits;
    float _averageCastTime;
    uint32 _interruptedCasts;

    // Target tracking
    std::unordered_map<ObjectGuid, uint32> _targetEngagementTime;
    std::unordered_map<ObjectGuid, float> _targetRemainingHealth;
    std::unordered_map<ObjectGuid, uint32> _targetResistanceLevel;
    ObjectGuid _primaryTarget;

    // Performance metrics
    ArcaneMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Positioning data
    Position _optimalCastingPosition;
    bool _isChanneling;
    bool _needsRepositioning;
    uint32 _lastMovementTime;

    // Configuration
    std::atomic<float> _burnThreshold{0.85f};
    std::atomic<float> _conserveThreshold{0.25f};
    std::atomic<uint32> _maxBurnDuration{18000};
    std::atomic<uint32> _minConserveDuration{15000};
    std::atomic<bool> _enableAdvancedPhasing{true};

    // Constants
    static constexpr uint32 MAX_ARCANE_CHARGES = 4;
    static constexpr uint32 CHARGE_DECAY_TIME = 10000; // 10 seconds
    static constexpr uint32 OPTIMAL_BURN_DURATION = 15000; // 15 seconds
    static constexpr uint32 MIN_CONSERVE_DURATION = 12000; // 12 seconds
    static constexpr float BURN_ENTRY_THRESHOLD = 0.85f; // 85% mana
    static constexpr float BURN_EXIT_THRESHOLD = 0.25f; // 25% mana
    static constexpr float CONSERVE_EXIT_THRESHOLD = 0.80f; // 80% mana
    static constexpr float MANA_GEM_THRESHOLD = 0.15f; // 15% mana
    static constexpr uint32 CLEARCASTING_DURATION = 15000; // 15 seconds
    static constexpr uint32 PRESENCE_OF_MIND_DURATION = 10000; // 10 seconds
    static constexpr uint32 ARCANE_POWER_DURATION = 15000; // 15 seconds
    static constexpr float PHASE_TRANSITION_BUFFER = 2000.0f; // 2 second buffer
    static constexpr uint32 OPTIMAL_CASTING_RANGE = 30000; // 30 yards
};

} // namespace Playerbot