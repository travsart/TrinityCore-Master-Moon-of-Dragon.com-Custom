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
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class HavocPhase : uint8
{
    OPENING         = 0,  // Initial engagement and momentum building
    FURY_GENERATION = 1,  // Building fury resources
    CHAOS_STRIKE    = 2,  // Chaos Strike spam phase
    METAMORPHOSIS   = 3,  // Metamorphosis burst window
    MOBILITY        = 4,  // Movement and positioning phase
    EXECUTE         = 5,  // Low health finishing
    EMERGENCY       = 6   // Critical situations
};

enum class MomentumState : uint8
{
    INACTIVE        = 0,  // No momentum buffs
    BUILDING        = 1,  // Building momentum stacks
    MAINTAINED      = 2,  // Maintaining momentum
    OPTIMIZING      = 3,  // Optimizing momentum usage
    FADING          = 4   // Momentum about to expire
};

enum class MetamorphosisState : uint8
{
    UNAVAILABLE     = 0,  // On cooldown or not learned
    READY           = 1,  // Available for use
    PREPARING       = 2,  // Setting up for activation
    ACTIVE          = 3,  // Currently active
    OPTIMIZING      = 4,  // Maximizing active window
    ENDING          = 5   // About to end
};

struct HavocTarget
{
    ObjectGuid targetGuid;
    bool hasNemesis;
    uint32 nemesisTimeRemaining;
    uint32 lastChaosStrikeTime;
    uint32 lastBladeDanceTime;
    float damageContribution;
    bool isOptimalForEyeBeam;
    uint32 mobilityCooldownsUsed;
    bool isPriorityTarget;
    float executePriority;

    HavocTarget() : hasNemesis(false), nemesisTimeRemaining(0)
        , lastChaosStrikeTime(0), lastBladeDanceTime(0)
        , damageContribution(0.0f), isOptimalForEyeBeam(false)
        , mobilityCooldownsUsed(0), isPriorityTarget(false)
        , executePriority(0.0f) {}
};

/**
 * @brief Enhanced Havoc specialization with advanced mobility and momentum mastery
 *
 * Focuses on sophisticated fury management, momentum optimization,
 * and intelligent metamorphosis timing for maximum melee DPS efficiency.
 */
class TC_GAME_API HavocSpecialization_Enhanced : public DemonHunterSpecialization
{
public:
    explicit HavocSpecialization_Enhanced(Player* bot);
    ~HavocSpecialization_Enhanced() override = default;

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

    // Advanced fury mastery
    void ManageFuryOptimally();
    void OptimizeFuryGeneration();
    void HandleFurySpendingEfficiency();
    void CoordinateFuryResources();
    void MaximizeFuryUtilization();

    // Momentum optimization
    void ManageMomentumOptimally();
    void OptimizeMomentumBuilding();
    void HandleMomentumMaintenance();
    void CoordinateMomentumWithRotation();
    void MaximizeMomentumEfficiency();

    // Metamorphosis mastery
    void ManageMetamorphosisOptimally();
    void OptimizeMetamorphosisTiming();
    void HandleMetamorphosisWindow();
    void CoordinateMetamorphosisBurst();
    void MaximizeMetamorphosisDamage();

    // Mobility and positioning mastery
    void ManageMobilityOptimally();
    void OptimizeMobilityForDPS();
    void HandleMobilitySequences();
    void CoordinateMobilityWithRotation();
    void MaximizeMobilityEfficiency();

    // Soul fragment optimization
    void ManageSoulFragmentsOptimally();
    void OptimizeSoulFragmentConsumption();
    void HandleSoulFragmentPositioning();
    void CoordinateSoulFragmentUsage();
    void MaximizeSoulFragmentValue();

    // Performance analytics
    struct HavocMetrics
    {
        std::atomic<uint32> demonsBiteCasts{0};
        std::atomic<uint32> chaosStrikeCasts{0};
        std::atomic<uint32> bladeDanceCasts{0};
        std::atomic<uint32> eyeBeamCasts{0};
        std::atomic<uint32> felRushUses{0};
        std::atomic<uint32> vengefulRetreatUses{0};
        std::atomic<uint32> metamorphosisActivations{0};
        std::atomic<uint32> soulFragmentsConsumed{0};
        std::atomic<float> furyEfficiency{0.85f};
        std::atomic<float> momentumUptime{0.7f};
        std::atomic<float> metamorphosisEfficiency{0.95f};
        std::atomic<float> mobilityEfficiency{0.8f};
        std::atomic<uint32> perfectEyeBeams{0};
        std::atomic<uint32> executionKills{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            demonsBiteCasts = 0; chaosStrikeCasts = 0; bladeDanceCasts = 0;
            eyeBeamCasts = 0; felRushUses = 0; vengefulRetreatUses = 0;
            metamorphosisActivations = 0; soulFragmentsConsumed = 0; furyEfficiency = 0.85f;
            momentumUptime = 0.7f; metamorphosisEfficiency = 0.95f; mobilityEfficiency = 0.8f;
            perfectEyeBeams = 0; executionKills = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    HavocMetrics GetSpecializationMetrics() const { return _metrics; }

    // Eye Beam optimization
    void ManageEyeBeamOptimally();
    void OptimizeEyeBeamTargeting();
    void HandleEyeBeamChanneling();
    void CoordinateEyeBeamWithRotation();

    // Nemesis and target marking
    void ManageNemesisOptimally();
    void OptimizeNemesisTargeting();
    void HandleNemesisTiming();
    void CoordinateNemesisWithBurst();

    // Advanced rotation optimization
    void OptimizeRotationForTarget(::Unit* target);
    void HandleMultiTargetHavoc();
    void CoordinateAoERotation();
    void ManageExecutePhase();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteFuryGenerationPhase(::Unit* target);
    void ExecuteChaosStrikePhase(::Unit* target);
    void ExecuteMetamorphosisPhase(::Unit* target);
    void ExecuteMobilityPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastDemonsBite(::Unit* target) const;
    bool ShouldCastChaosStrike(::Unit* target) const;
    bool ShouldCastBladeDance() const;
    bool ShouldCastEyeBeam(::Unit* target) const;
    bool ShouldCastFelRush(::Unit* target) const;

    // Advanced spell execution
    void ExecuteDemonsBite(::Unit* target);
    void ExecuteChaosStrike(::Unit* target);
    void ExecuteBladeDance();
    void ExecuteEyeBeam(::Unit* target);
    void ExecuteFelRush(::Unit* target);

    // Cooldown management
    bool ShouldUseMetamorphosis() const;
    bool ShouldUseNemesis(::Unit* target) const;
    bool ShouldUseChaosBlades() const;
    bool ShouldUseVengefulRetreat() const;

    // Advanced cooldown execution
    void ExecuteMetamorphosis();
    void ExecuteNemesis(::Unit* target);
    void ExecuteChaosBlades();
    void ExecuteVengefulRetreat();

    // Fury management implementations
    void UpdateFuryTracking();
    void OptimizeFurySpending();
    void HandleFuryGeneration();
    void CalculateOptimalFuryUsage();

    // Momentum implementations
    void UpdateMomentumTracking();
    void BuildMomentum();
    void MaintainMomentum();
    void OptimizeMomentumWindows();

    // Metamorphosis implementations
    void UpdateMetamorphosisTracking();
    void PrepareMetamorphosisWindow();
    void ExecuteMetamorphosisRotation(::Unit* target);
    void OptimizeMetamorphosisDuration();

    // Mobility implementations
    void UpdateMobilityTracking();
    void ExecuteMobilitySequence(::Unit* target);
    void OptimizeMobilityTiming();
    void HandleMobilityEmergencies();

    // Soul fragment implementations
    void UpdateSoulFragmentTracking();
    void ConsumeSoulFragmentsOptimally();
    void PositionForSoulFragments();
    void OptimizeSoulFragmentTiming();

    // Target analysis for havoc
    void AnalyzeTargetForHavoc(::Unit* target);
    void AssessExecutionPotential(::Unit* target);
    void PredictTargetMovement(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // Multi-target optimization
    void HandleMultiTargetHavoc();
    void OptimizeAoERotation();
    void CoordinateMultiTargetMobility();
    void ManageAoEFury();

    // Position optimization
    void OptimizeHavocPositioning(::Unit* target);
    void MaintainOptimalMeleeRange(::Unit* target);
    void HandlePositionalRequirements();
    void ExecuteMobilityPositioning();

    // Performance tracking
    void TrackHavocPerformance();
    void AnalyzeFuryEfficiency();
    void UpdateMomentumMetrics();
    void OptimizeBasedOnHavocMetrics();

    // Emergency handling
    void HandleLowHealthHavocEmergency();
    void HandleResourceEmergency();
    void ExecuteEmergencyMobility();
    void HandleCrowdControlEmergency();

    // State tracking
    HavocPhase _currentPhase;
    MomentumState _momentumState;
    MetamorphosisState _metamorphosisState;

    // Target tracking
    std::unordered_map<ObjectGuid, HavocTarget> _havocTargets;
    ObjectGuid _primaryTarget;
    ObjectGuid _nemesisTarget;

    // Fury tracking
    uint32 _currentFury;
    uint32 _furyGenerated;
    uint32 _furySpent;
    float _furyEfficiencyRatio;

    // Momentum tracking
    uint32 _momentumStacks;
    uint32 _momentumTimeRemaining;
    uint32 _lastMomentumGain;
    bool _momentumActive;

    // Metamorphosis tracking
    uint32 _metamorphosisTimeRemaining;
    uint32 _lastMetamorphosisActivation;
    bool _metamorphosisActive;
    uint32 _metamorphosisCooldown;

    // Mobility tracking
    uint32 _felRushCharges;
    uint32 _lastFelRushTime;
    uint32 _lastVengefulRetreatTime;
    Position _lastMobilityPosition;

    // Soul fragment tracking
    uint32 _availableSoulFragments;
    uint32 _soulFragmentsConsumed;
    uint32 _lastSoulFragmentConsumption;
    Position _nearestSoulFragmentPosition;

    // Eye Beam tracking
    uint32 _lastEyeBeamTime;
    uint32 _eyeBeamCooldown;
    bool _eyeBeamChanneling;
    uint32 _eyeBeamChannelTime;

    // Nemesis tracking
    uint32 _lastNemesisTime;
    uint32 _nemesisCooldown;
    uint32 _nemesisTimeRemaining;
    bool _nemesisActive;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalHavocDamage;
    uint32 _totalFuryGenerated;
    uint32 _totalFurySpent;
    float _averageHavocDps;

    // Performance metrics
    HavocMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _furyEfficiencyThreshold{0.8f};
    std::atomic<float> _momentumUptimeTarget{0.7f};
    std::atomic<uint32> _metamorphosisOptimalDuration{30000};
    std::atomic<bool> _enableAdvancedMobility{true};
    std::atomic<bool> _enableOptimalMomentum{true};

    // Constants
    static constexpr uint32 MAX_FURY = 120;
    static constexpr uint32 MOMENTUM_DURATION = 6000; // 6 seconds
    static constexpr uint32 METAMORPHOSIS_DURATION = 30000; // 30 seconds
    static constexpr uint32 FEL_RUSH_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 VENGEFUL_RETREAT_COOLDOWN = 25000; // 25 seconds
    static constexpr uint32 EYE_BEAM_COOLDOWN = 45000; // 45 seconds
    static constexpr uint32 NEMESIS_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 CHAOS_BLADES_COOLDOWN = 120000; // 2 minutes
    static constexpr float MOMENTUM_THRESHOLD = 0.3f; // 30% remaining
    static constexpr uint8 OPTIMAL_FURY_FOR_CHAOS_STRIKE = 40;
    static constexpr uint8 OPTIMAL_SOUL_FRAGMENTS_FOR_CONSUMPTION = 3;
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.15f; // 15% health
    static constexpr uint32 MOBILITY_SEQUENCE_COOLDOWN = 15000; // 15 seconds
    static constexpr float OPTIMAL_HAVOC_RANGE = 5.0f;
};

} // namespace Playerbot