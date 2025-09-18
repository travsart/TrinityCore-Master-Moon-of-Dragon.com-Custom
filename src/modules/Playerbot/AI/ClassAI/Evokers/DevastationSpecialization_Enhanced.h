/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "EvokerSpecialization.h"
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class DevastationPhase : uint8
{
    OPENING         = 0,  // Initial essence building
    ESSENCE_MASTERY = 1,  // Essence generation optimization
    EMPOWERMENT     = 2,  // Empowered spell execution
    DRAGONRAGE_BURST = 3,  // Dragonrage burst window
    BURNOUT_MGMT    = 4,  // Burnout stack management
    IRIDESCENCE     = 5,  // Iridescence optimization
    EXECUTE         = 6,  // Low health finishing
    EMERGENCY       = 7   // Critical situations
};

enum class EmpowermentState : uint8
{
    INACTIVE        = 0,  // No empowered spell active
    CHANNELING      = 1,  // Currently channeling
    OPTIMAL_LEVEL   = 2,  // At optimal empowerment level
    MAXIMIZING      = 3,  // Building to max level
    RELEASING       = 4   // About to release
};

enum class BurnoutManagementState : uint8
{
    SAFE            = 0,  // Safe burnout levels
    MODERATE        = 1,  // Moderate burnout risk
    HIGH_RISK       = 2,  // High burnout risk
    CRITICAL        = 3,  // Critical burnout levels
    RECOVERY        = 4   // Recovering from burnout
};

struct DevastationTarget
{
    ObjectGuid targetGuid;
    bool hasShatteringStar;
    uint32 shatteringStarTimeRemaining;
    uint32 lastFireBreathTime;
    uint32 lastEternitysSurgeTime;
    float damageContribution;
    bool isOptimalForEmpowerment;
    uint32 empoweredSpellsUsed;
    bool isPriorityTarget;
    float executePriority;

    DevastationTarget() : hasShatteringStar(false), shatteringStarTimeRemaining(0)
        , lastFireBreathTime(0), lastEternitysSurgeTime(0)
        , damageContribution(0.0f), isOptimalForEmpowerment(false)
        , empoweredSpellsUsed(0), isPriorityTarget(false)
        , executePriority(0.0f) {}
};

/**
 * @brief Enhanced Devastation specialization with advanced essence mastery and empowerment optimization
 *
 * Focuses on sophisticated essence management, empowered spell optimization,
 * and intelligent burnout management for maximum ranged DPS efficiency.
 */
class TC_GAME_API DevastationSpecialization_Enhanced : public EvokerSpecialization
{
public:
    explicit DevastationSpecialization_Enhanced(Player* bot);
    ~DevastationSpecialization_Enhanced() override = default;

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

    // Advanced essence mastery
    void ManageEssenceOptimally();
    void OptimizeEssenceGeneration();
    void HandleEssenceSpendingEfficiency();
    void CoordinateEssenceResources();
    void MaximizeEssenceUtilization();

    // Empowerment optimization
    void ManageEmpowermentOptimally();
    void OptimizeEmpoweredSpellTiming();
    void HandleEmpowermentChanneling();
    void CoordinateEmpoweredRotation();
    void MaximizeEmpowermentEfficiency();

    // Dragonrage mastery
    void ManageDragonrageOptimally();
    void OptimizeDragonrageTiming();
    void HandleDragonrageWindow();
    void CoordinateDragonrageBurst();
    void MaximizeDragonrageDamage();

    // Burnout management mastery
    void ManageBurnoutOptimally();
    void OptimizeBurnoutStacks();
    void HandleBurnoutRecovery();
    void CoordinateBurnoutWithRotation();
    void MaximizeBurnoutSafety();

    // Iridescence optimization
    void ManageIridescenceOptimally();
    void OptimizeIridescenceProcs();
    void HandleIridescenceConsumption();
    void CoordinateIridescenceWithRotation();
    void MaximizeIridescenceValue();

    // Performance analytics
    struct DevastationMetrics
    {
        std::atomic<uint32> azureStrikeCasts{0};
        std::atomic<uint32> livingFlameCasts{0};
        std::atomic<uint32> disintegrateCasts{0};
        std::atomic<uint32> pyreCasts{0};
        std::atomic<uint32> fireBreathCasts{0};
        std::atomic<uint32> eternitysSurgeCasts{0};
        std::atomic<uint32> shatteringStarCasts{0};
        std::atomic<uint32> dragonrageActivations{0};
        std::atomic<uint32> deepBreathCasts{0};
        std::atomic<uint32> empoweredSpellsCast{0};
        std::atomic<float> essenceEfficiency{0.9f};
        std::atomic<float> empowermentEfficiency{0.85f};
        std::atomic<float> burnoutManagementScore{0.8f};
        std::atomic<float> dragonrageUtilization{0.95f};
        std::atomic<float> iridescenceOptimization{0.75f};
        std::atomic<uint32> perfectEmpowerments{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            azureStrikeCasts = 0; livingFlameCasts = 0; disintegrateCasts = 0;
            pyreCasts = 0; fireBreathCasts = 0; eternitysSurgeCasts = 0;
            shatteringStarCasts = 0; dragonrageActivations = 0; deepBreathCasts = 0;
            empoweredSpellsCast = 0; essenceEfficiency = 0.9f; empowermentEfficiency = 0.85f;
            burnoutManagementScore = 0.8f; dragonrageUtilization = 0.95f; iridescenceOptimization = 0.75f;
            perfectEmpowerments = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    DevastationMetrics GetSpecializationMetrics() const { return _metrics; }

    // Advanced spell optimization
    void ManageSpellRotationOptimally();
    void OptimizeSpellCastingSequence();
    void HandleSpellPrioritization();
    void CoordinateSpellRotation();

    // Area of effect optimization
    void ManageAoEOptimally();
    void OptimizeAoETargeting();
    void HandleAoEEmpowerment();
    void CoordinateAoERotation();

    // Deep Breath mastery
    void ManageDeepBreathOptimally();
    void OptimizeDeepBreathTiming();
    void HandleDeepBreathPositioning();
    void CoordinateDeepBreathWithRotation();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteEssenceMasteryPhase(::Unit* target);
    void ExecuteEmpowermentPhase(::Unit* target);
    void ExecuteDragonrageBurst(::Unit* target);
    void ExecuteBurnoutManagement(::Unit* target);
    void ExecuteIridescencePhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastAzureStrike(::Unit* target) const;
    bool ShouldCastLivingFlame(::Unit* target) const;
    bool ShouldCastDisintegrate(::Unit* target) const;
    bool ShouldCastPyre() const;
    bool ShouldCastShatteringStar(::Unit* target) const;

    // Advanced spell execution
    void ExecuteAzureStrike(::Unit* target);
    void ExecuteLivingFlame(::Unit* target);
    void ExecuteDisintegrate(::Unit* target);
    void ExecutePyre();
    void ExecuteShatteringStar(::Unit* target);

    // Empowered spell management
    bool ShouldCastEmpoweredFireBreath(::Unit* target) const;
    bool ShouldCastEmpoweredEternitysSurge(::Unit* target) const;
    void ExecuteEmpoweredFireBreath(::Unit* target, EmpowermentLevel level);
    void ExecuteEmpoweredEternitysSurge(::Unit* target, EmpowermentLevel level);

    // Cooldown management
    bool ShouldUseDragonrage() const;
    bool ShouldUseDeepBreath(::Unit* target) const;
    bool ShouldUseFirestorm() const;
    void ExecuteDragonrage();
    void ExecuteDeepBreath(::Unit* target);
    void ExecuteFirestorm();

    // Essence management implementations
    void UpdateEssenceTracking();
    void OptimizeEssenceSpending();
    void HandleEssenceGeneration();
    void CalculateOptimalEssenceUsage();

    // Empowerment implementations
    void UpdateEmpowermentTracking();
    void StartOptimalEmpowerment(uint32 spellId, ::Unit* target);
    void OptimizeEmpowermentLevel(uint32 spellId, ::Unit* target);
    void HandleEmpowermentRelease();

    // Dragonrage implementations
    void UpdateDragonrageTracking();
    void PrepareDragonrageWindow();
    void ExecuteDragonrageRotation(::Unit* target);
    void OptimizeDragonrageDuration();

    // Burnout implementations
    void UpdateBurnoutTracking();
    void MonitorBurnoutStacks();
    void HandleBurnoutPrevention();
    void OptimizeBurnoutManagement();

    // Iridescence implementations
    void UpdateIridescenceTracking();
    void HandleIridescenceProcs();
    void OptimizeIridescenceUsage(::Unit* target);
    void CoordinateIridescenceSpells();

    // Target analysis for devastation
    void AnalyzeTargetForDevastation(::Unit* target);
    void AssessEmpowermentViability(::Unit* target);
    void PredictTargetMovement(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // Multi-target optimization
    void HandleMultiTargetDevastation();
    void OptimizeAoERotation();
    void CoordinateMultiTargetEmpowerment();
    void ManageAoEEssence();

    // Position optimization
    void OptimizeDevastationPositioning(::Unit* target);
    void MaintainOptimalCastingRange(::Unit* target);
    void HandlePositionalRequirements();
    void ExecuteMobilityOptimization();

    // Performance tracking
    void TrackDevastationPerformance();
    void AnalyzeEssenceEfficiency();
    void UpdateEmpowermentMetrics();
    void OptimizeBasedOnDevastationMetrics();

    // Emergency handling
    void HandleLowHealthDevastationEmergency();
    void HandleResourceEmergency();
    void ExecuteEmergencyBurst();
    void HandleInterruptEmergency();

    // State tracking
    DevastationPhase _currentPhase;
    EmpowermentState _empowermentState;
    BurnoutManagementState _burnoutState;

    // Target tracking
    std::unordered_map<ObjectGuid, DevastationTarget> _devastationTargets;
    ObjectGuid _primaryTarget;
    std::vector<ObjectGuid> _aoeTargets;

    // Essence tracking
    uint32 _currentEssence;
    uint32 _essenceGenerated;
    uint32 _essenceSpent;
    float _essenceEfficiencyRatio;

    // Empowerment tracking
    uint32 _currentEmpowermentLevel;
    uint32 _empoweredSpellsUsed;
    uint32 _perfectEmpowerments;
    uint32 _lastEmpowermentTime;

    // Dragonrage tracking
    uint32 _dragonrageTimeRemaining;
    uint32 _lastDragonrageActivation;
    bool _dragonrageActive;
    uint32 _dragonrageDamageDealt;

    // Burnout tracking
    uint32 _burnoutStacks;
    uint32 _burnoutTimeRemaining;
    uint32 _lastBurnoutApplication;
    bool _burnoutRecovery;

    // Iridescence tracking
    bool _blueIridescenceActive;
    bool _redIridescenceActive;
    uint32 _blueIridescenceTimeRemaining;
    uint32 _redIridescenceTimeRemaining;

    // Shattering Star tracking
    uint32 _lastShatteringStarTime;
    bool _shatteringStarWindowActive;
    uint32 _shatteringStarWindowTimeRemaining;

    // Deep Breath tracking
    uint32 _lastDeepBreathTime;
    bool _deepBreathChanneling;
    uint32 _deepBreathChannelTime;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalDevastationDamage;
    uint32 _totalEssenceGenerated;
    uint32 _totalEssenceSpent;
    float _averageDevastationDps;

    // Performance metrics
    DevastationMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _essenceEfficiencyThreshold{0.85f};
    std::atomic<uint32> _optimalEmpowermentLevel{3};
    std::atomic<uint32> _burnoutStackThreshold{3};
    std::atomic<bool> _enableAdvancedEmpowerment{true};
    std::atomic<bool> _enableOptimalBurnoutManagement{true};

    // Constants
    static constexpr uint32 MAX_ESSENCE = 5;
    static constexpr uint32 DRAGONRAGE_DURATION = 18000; // 18 seconds
    static constexpr uint32 BURNOUT_DURATION = 15000; // 15 seconds
    static constexpr uint32 BURNOUT_MAX_STACKS = 5;
    static constexpr uint32 IRIDESCENCE_DURATION = 12000; // 12 seconds
    static constexpr uint32 SHATTERING_STAR_WINDOW = 4000; // 4 seconds
    static constexpr uint32 DEEP_BREATH_CHANNEL_TIME = 6000; // 6 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr uint8 OPTIMAL_EMPOWERMENT_LEVEL = 3;
    static constexpr uint32 EMPOWERMENT_CHANNEL_INTERVAL = 1000; // 1 second per level
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr float OPTIMAL_DEVASTATION_RANGE = 25.0f;
};

} // namespace Playerbot