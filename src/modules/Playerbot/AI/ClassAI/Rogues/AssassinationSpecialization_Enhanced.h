/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "RogueSpecialization.h"
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class AssassinationPhase : uint8
{
    OPENING         = 0,  // Initial stealth setup and opener
    DOT_SETUP       = 1,  // Establishing DoT effects
    POISON_STACKING = 2,  // Building poison stacks
    MAINTAIN_PHASE  = 3,  // Maintaining DoTs and poisons
    BURST_WINDOW    = 4,  // Cold Blood/Vendetta burst
    EXECUTE         = 5,  // Low health assassination
    EMERGENCY       = 6   // Critical situations
};

enum class PoisonStackingState : uint8
{
    NONE            = 0,  // No poison stacks
    BUILDING        = 1,  // Building initial stacks
    MAINTAINED      = 2,  // Maintaining optimal stacks
    REFRESHING      = 3,  // Refreshing existing stacks
    STACKED         = 4   // Full stack achieved
};

enum class DotManagementState : uint8
{
    SETUP           = 0,  // Initial DoT application
    MAINTAIN        = 1,  // Maintaining existing DoTs
    REFRESH         = 2,  // Refreshing expiring DoTs
    PANDEMIC        = 3,  // Pandemic refresh timing
    EXECUTE         = 4   // Execute phase DoT priorities
};

struct AssassinationTarget
{
    ObjectGuid targetGuid;
    bool hasRupture;
    bool hasGarrote;
    uint32 ruptureStacks;
    uint32 garroteStacks;
    uint32 ruptureTimeRemaining;
    uint32 garroteTimeRemaining;
    uint32 poisonStacks;
    uint32 lastMutilateTime;
    uint32 lastEnvenomTime;
    float executePriority;
    bool isMarkedForExecution;

    AssassinationTarget() : hasRupture(false), hasGarrote(false), ruptureStacks(0)
        , garroteStacks(0), ruptureTimeRemaining(0), garroteTimeRemaining(0)
        , poisonStacks(0), lastMutilateTime(0), lastEnvenomTime(0)
        , executePriority(0.0f), isMarkedForExecution(false) {}
};

/**
 * @brief Enhanced Assassination specialization with advanced DoT and poison mastery
 *
 * Focuses on sophisticated poison stacking, DoT management optimization,
 * and intelligent burst window coordination for maximum sustained damage.
 */
class TC_GAME_API AssassinationSpecialization_Enhanced : public RogueSpecialization
{
public:
    explicit AssassinationSpecialization_Enhanced(Player* bot);
    ~AssassinationSpecialization_Enhanced() override = default;

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

    // Advanced poison mastery
    void ManagePoisonStackingOptimally();
    void OptimizePoisonApplication();
    void HandlePoisonRefreshTiming();
    void CoordinatePoisonTypes();
    void MaximizePoisonEfficiency();

    // Sophisticated DoT management
    void ManageDoTsIntelligently();
    void OptimizeRuptureUsage(::Unit* target);
    void HandleGarroteOptimally(::Unit* target);
    void CoordinateDoTRefreshes();
    void MaximizeDoTUptime();

    // Advanced mutilate/envenom optimization
    void ExecuteOptimalMutilateSequence(::Unit* target);
    void OptimizeEnvenomTiming(::Unit* target);
    void HandleComboPointEfficiency();
    void CoordinateFinisherUsage();
    void MaximizePoisonSynergy();

    // Burst window mastery
    void ExecuteColdBloodSequence();
    void OptimizeVendettaTiming();
    void CoordinateBurstCooldowns();
    void HandleBurstWindowOptimization();
    void MaximizeBurstDamage();

    // Stealth and opener optimization
    void ExecutePerfectStealthOpener(::Unit* target);
    void OptimizeOpenerSelection(::Unit* target);
    void HandleStealthAdvantage();
    void CoordinateStealthCooldowns();
    void MaximizeOpenerDamage();

    // Performance analytics
    struct AssassinationMetrics
    {
        std::atomic<uint32> mutilateCasts{0};
        std::atomic<uint32> envenomCasts{0};
        std::atomic<uint32> ruptureApplications{0};
        std::atomic<uint32> garroteApplications{0};
        std::atomic<uint32> poisonApplications{0};
        std::atomic<uint32> coldBloodActivations{0};
        std::atomic<uint32> vendettaActivations{0};
        std::atomic<float> dotUptimePercentage{0.95f};
        std::atomic<float> poisonUptimePercentage{0.98f};
        std::atomic<float> burstWindowEfficiency{0.9f};
        std::atomic<float> comboPointEfficiency{0.85f};
        std::atomic<uint32> stealthOpeners{0};
        std::atomic<uint32> executionKills{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            mutilateCasts = 0; envenomCasts = 0; ruptureApplications = 0;
            garroteApplications = 0; poisonApplications = 0; coldBloodActivations = 0;
            vendettaActivations = 0; dotUptimePercentage = 0.95f; poisonUptimePercentage = 0.98f;
            burstWindowEfficiency = 0.9f; comboPointEfficiency = 0.85f;
            stealthOpeners = 0; executionKills = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    AssassinationMetrics GetSpecializationMetrics() const { return _metrics; }

    // Advanced energy management for assassination
    void OptimizeEnergyForAssassination();
    void HandleEnergyEfficiencyInRotation();
    void PredictEnergyNeeds();
    void BalanceEnergyAndDamage();

    // Multi-target assassination
    void HandleMultiTargetAssassination();
    void OptimizeTargetPrioritization();
    void CoordinateMultiTargetDoTs();
    void HandleAoEPoisoning();

    // Execute phase mastery
    void ExecutePhaseOptimization(::Unit* target);
    void HandleLowHealthTargets();
    void OptimizeExecutionRotation();
    void CoordinateExecutionBurst();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteDoTSetupPhase(::Unit* target);
    void ExecutePoisonStackingPhase(::Unit* target);
    void ExecuteMaintainPhase(::Unit* target);
    void ExecuteBurstWindow(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastMutilate(::Unit* target) const;
    bool ShouldCastEnvenom(::Unit* target) const;
    bool ShouldCastRupture(::Unit* target) const;
    bool ShouldCastGarrote(::Unit* target) const;
    bool ShouldCastBackstab(::Unit* target) const;

    // Advanced spell execution
    void ExecuteMutilate(::Unit* target);
    void ExecuteEnvenom(::Unit* target);
    void ExecuteRupture(::Unit* target);
    void ExecuteGarrote(::Unit* target);
    void ExecuteBackstab(::Unit* target);

    // Cooldown management
    bool ShouldUseColdBlood() const;
    bool ShouldUseVendetta() const;
    bool ShouldUseVanish() const;
    bool ShouldUsePreparation() const;

    // Advanced cooldown execution
    void ExecuteColdBlood();
    void ExecuteVendetta();
    void ExecuteVanish();
    void ExecutePreparation();

    // Poison management implementations
    void UpdatePoisonTracking();
    void ApplyOptimalPoisons();
    void RefreshPoisonCharges();
    bool ShouldRefreshPoisons() const;

    // DoT management implementations
    void UpdateDoTTracking();
    void RefreshExpiringDoTs();
    bool ShouldRefreshRupture(::Unit* target) const;
    bool ShouldRefreshGarrote(::Unit* target) const;

    // Stealth implementations
    void HandleStealthOpenerSequence(::Unit* target);
    void ExecuteGarroteOpener(::Unit* target);
    void ExecuteAmbushOpener(::Unit* target);
    void ExecuteCheapShotOpener(::Unit* target);

    // Combo point optimization
    void OptimizeComboPointGeneration(::Unit* target);
    void HandleComboPointSpending(::Unit* target);
    uint8 GetOptimalFinisherComboPoints() const;
    bool ShouldSpendComboPoints() const;

    // Burst coordination
    void PrepareBurstWindow();
    void ExecuteBurstRotation(::Unit* target);
    void HandleBurstCooldowns();
    bool IsInBurstWindow() const;

    // Target analysis for assassination
    void AnalyzeTargetForAssassination(::Unit* target);
    void PredictTargetLifetime(::Unit* target);
    void AssessExecutionPotential(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // Resource management
    void OptimizeEnergySpending();
    void PredictEnergyRegeneration();
    void HandleEnergyEmergencies();
    void BalanceResourceAllocation();

    // Position optimization
    void OptimizeAssassinationPositioning(::Unit* target);
    void MaintainBehindTargetPosition(::Unit* target);
    void HandlePositionalRequirements();
    void ExecuteStealthPositioning();

    // Performance tracking
    void TrackAssassinationPerformance();
    void AnalyzePoisonEfficiency();
    void UpdateDoTMetrics();
    void OptimizeBasedOnMetrics();

    // Emergency handling
    void HandleLowHealthEmergency();
    void HandleMultipleTargetsEmergency();
    void ExecuteEmergencyVanish();
    void HandleInterruptEmergency();

    // State tracking
    AssassinationPhase _currentPhase;
    PoisonStackingState _poisonState;
    DotManagementState _dotState;

    // Target tracking
    std::unordered_map<ObjectGuid, AssassinationTarget> _assassinationTargets;
    ObjectGuid _primaryTarget;
    uint32 _targetSwitchTime;

    // DoT tracking
    uint32 _lastRuptureTime;
    uint32 _lastGarroteTime;
    uint32 _dotRefreshWindow;
    uint32 _nextDoTRefresh;

    // Poison tracking
    uint32 _lastPoisonApplication;
    uint32 _mainHandCharges;
    uint32 _offHandCharges;
    uint32 _poisonRefreshTime;

    // Combo point data
    uint32 _lastMutilateTime;
    uint32 _lastEnvenomTime;
    uint32 _comboPointsGenerated;
    uint32 _comboPointsSpent;

    // Burst tracking
    uint32 _burstWindowStart;
    uint32 _burstWindowDuration;
    bool _burstWindowActive;
    uint32 _coldBloodCooldown;
    uint32 _vendettaCooldown;

    // Stealth tracking
    uint32 _lastStealthTime;
    uint32 _lastVanishTime;
    uint32 _stealthAdvantageWindow;
    bool _hasStealthAdvantage;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalAssassinationDamage;
    uint32 _totalPoisonDamage;
    uint32 _totalDoTDamage;
    float _averageDps;

    // Performance metrics
    AssassinationMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _dotRefreshThreshold{0.3f};
    std::atomic<float> _poisonRefreshThreshold{0.2f};
    std::atomic<uint32> _burstWindowOptimalDuration{15000};
    std::atomic<bool> _enableAdvancedPoisoning{true};
    std::atomic<bool> _enableOptimalDoTManagement{true};

    // Constants
    static constexpr uint32 RUPTURE_DURATION = 18000; // 18 seconds
    static constexpr uint32 GARROTE_DURATION = 12000; // 12 seconds
    static constexpr uint32 POISON_DURATION = 3600000; // 1 hour
    static constexpr uint32 COLD_BLOOD_DURATION = 30000; // 30 seconds
    static constexpr uint32 VENDETTA_DURATION = 30000; // 30 seconds
    static constexpr uint32 STEALTH_ADVANTAGE_DURATION = 6000; // 6 seconds
    static constexpr float DOT_PANDEMIC_THRESHOLD = 0.3f; // 30% duration
    static constexpr uint8 OPTIMAL_RUPTURE_COMBO_POINTS = 5;
    static constexpr uint8 OPTIMAL_ENVENOM_COMBO_POINTS = 4;
    static constexpr uint32 BURST_PREPARATION_TIME = 3000; // 3 seconds
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr uint32 ENERGY_CONSERVATION_THRESHOLD = 40;
    static constexpr float OPTIMAL_ASSASSINATION_RANGE = 5.0f;
};

} // namespace Playerbot