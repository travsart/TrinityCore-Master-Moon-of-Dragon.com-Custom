/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "MonkSpecialization.h"
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class BrewmasterPhase : uint8
{
    OPENING         = 0,  // Initial threat establishment
    STAGGER_CONTROL = 1,  // Active stagger management
    BREW_OPTIMIZE   = 2,  // Brew charge optimization
    THREAT_MAINTAIN = 3,  // Sustained threat generation
    DEFENSIVE_BURST = 4,  // Emergency defensive phase
    AOE_CONTROL     = 5,  // Multi-target management
    EMERGENCY       = 6   // Critical survival situations
};

enum class StaggerState : uint8
{
    NONE            = 0,  // No stagger active
    LIGHT           = 1,  // Light stagger (manageable)
    MODERATE        = 2,  // Moderate stagger (attention needed)
    HEAVY           = 3,  // Heavy stagger (urgent action)
    CRITICAL        = 4   // Critical stagger (immediate purge)
};

enum class BrewChargeState : uint8
{
    DEPLETED        = 0,  // No charges available
    LOW             = 1,  // 1 charge available
    MODERATE        = 2,  // 2 charges available
    FULL            = 3,  // 3+ charges available
    CAPPED          = 4   // At maximum charges
};

struct BrewmasterTarget
{
    ObjectGuid targetGuid;
    float threatLevel;
    uint32 lastTauntTime;
    uint32 lastKegSmashTime;
    bool hasBreathOfFire;
    uint32 breathOfFireTimeRemaining;
    float damageContribution;
    bool isPrimaryThreatTarget;
    uint32 staggerDamageDealt;

    BrewmasterTarget() : threatLevel(0.0f), lastTauntTime(0), lastKegSmashTime(0)
        , hasBreathOfFire(false), breathOfFireTimeRemaining(0)
        , damageContribution(0.0f), isPrimaryThreatTarget(false)
        , staggerDamageDealt(0) {}
};

/**
 * @brief Enhanced Brewmaster specialization with advanced stagger mastery and threat optimization
 *
 * Focuses on sophisticated stagger damage management, brew charge optimization,
 * and intelligent threat control for maximum tanking effectiveness and survivability.
 */
class TC_GAME_API BrewmasterSpecialization_Enhanced : public MonkSpecialization
{
public:
    explicit BrewmasterSpecialization_Enhanced(Player* bot);
    ~BrewmasterSpecialization_Enhanced() override = default;

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

    // Advanced stagger mastery
    void ManageStaggerOptimally();
    void OptimizeStaggerPurification();
    void HandleStaggerDamageIntelligently();
    void CoordinateStaggerMitigation();
    void MaximizeStaggerEfficiency();

    // Brew charge optimization
    void ManageBrewChargesOptimally();
    void OptimizeBrewUsageTiming();
    void HandleBrewRechargeEfficiency();
    void CoordinateBrewCooldowns();
    void MaximizeBrewUptime();

    // Advanced threat management
    void ManageThreatOptimally();
    void OptimizeThreatGeneration(::Unit* target);
    void HandleMultiTargetThreat();
    void CoordinateThreatRotation();
    void MaximizeThreatEfficiency();

    // Keg Smash and AoE mastery
    void ManageKegSmashOptimally();
    void OptimizeKegSmashTargeting();
    void HandleAoEThreatGeneration();
    void CoordinateAoERotation();
    void MaximizeAoEEffectiveness();

    // Defensive cooldown mastery
    void ManageDefensiveCooldownsOptimally();
    void OptimizeDefensiveTiming();
    void HandleEmergencyDefensives();
    void CoordinateDefensiveRotation();
    void MaximizeDefensiveValue();

    // Performance analytics
    struct BrewmasterMetrics
    {
        std::atomic<uint32> kegSmashCasts{0};
        std::atomic<uint32> breathOfFireCasts{0};
        std::atomic<uint32> ironskinBrewUses{0};
        std::atomic<uint32> purifyingBrewUses{0};
        std::atomic<uint32> staggerDamageMitigated{0};
        std::atomic<uint32> threatGenerated{0};
        std::atomic<uint32> defensiveCooldownsUsed{0};
        std::atomic<float> staggerUptime{0.9f};
        std::atomic<float> brewChargeEfficiency{0.85f};
        std::atomic<float> threatControlEfficiency{0.95f};
        std::atomic<float> damageReductionPercentage{0.4f};
        std::atomic<uint32> emergencyBrewsUsed{0};
        std::atomic<uint32> perfectStaggerPurges{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            kegSmashCasts = 0; breathOfFireCasts = 0; ironskinBrewUses = 0;
            purifyingBrewUses = 0; staggerDamageMitigated = 0; threatGenerated = 0;
            defensiveCooldownsUsed = 0; staggerUptime = 0.9f; brewChargeEfficiency = 0.85f;
            threatControlEfficiency = 0.95f; damageReductionPercentage = 0.4f;
            emergencyBrewsUsed = 0; perfectStaggerPurges = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    BrewmasterMetrics GetSpecializationMetrics() const { return _metrics; }

    // Chi management for tanking
    void OptimizeChiForTanking();
    void HandleChiGenerationForDefense();
    void BalanceChiAndBrewUsage();
    void MaximizeChiEfficiencyInTanking();

    // Position optimization for tanking
    void OptimizeTankPositioning(::Unit* target);
    void HandlePositionalThreatControl();
    void ManageMobPositioning();
    void ExecuteTacticalRepositioning();

    // Advanced target management
    void HandleMultiTargetTanking();
    void OptimizeTargetPrioritization();
    void CoordinateAoEThreatControl();
    void ManageThreatDistribution();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteStaggerControlPhase(::Unit* target);
    void ExecuteBrewOptimizePhase(::Unit* target);
    void ExecuteThreatMaintainPhase(::Unit* target);
    void ExecuteDefensiveBurstPhase(::Unit* target);
    void ExecuteAoEControlPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastKegSmash(::Unit* target) const;
    bool ShouldCastBreathOfFire() const;
    bool ShouldCastIronskinBrew() const;
    bool ShouldCastPurifyingBrew() const;
    bool ShouldCastBlackoutKick(::Unit* target) const;

    // Advanced spell execution
    void ExecuteKegSmash(::Unit* target);
    void ExecuteBreathOfFire();
    void ExecuteIronskinBrew();
    void ExecutePurifyingBrew();
    void ExecuteBlackoutKick(::Unit* target);

    // Cooldown management
    bool ShouldUseFortifyingBrew() const;
    bool ShouldUseZenMeditation() const;
    bool ShouldUseDampenHarm() const;
    bool ShouldUseBlackOxBrew() const;

    // Advanced cooldown execution
    void ExecuteFortifyingBrew();
    void ExecuteZenMeditation();
    void ExecuteDampenHarm();
    void ExecuteBlackOxBrew();

    // Stagger management implementations
    void UpdateStaggerTracking();
    void AnalyzeStaggerDamage();
    void OptimizeStaggerPurging();
    void HandleStaggerEmergency();

    // Brew management implementations
    void UpdateBrewChargeTracking();
    void OptimizeBrewChargeUsage();
    void HandleBrewEmergency();
    void CalculateOptimalBrewTiming();

    // Threat management implementations
    void UpdateThreatTracking();
    void OptimizeThreatRotation(::Unit* target);
    void HandleThreatEmergency();
    void CalculateThreatPriorities();

    // Target analysis for tanking
    void AnalyzeTargetForTanking(::Unit* target);
    void AssessThreatRequirements(::Unit* target);
    void PredictIncomingDamage(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // AoE and multi-target implementations
    void HandleMultiTargetBrewmaster();
    void OptimizeAoEThreatGeneration();
    void CoordinateMultiTargetDefenses();
    void ManageAoEPositioning();

    // Emergency handling
    void HandleLowHealthTankingEmergency();
    void HandleHighStaggerEmergency();
    void ExecuteEmergencyDefensives();
    void HandleThreatLossEmergency();

    // Position optimization
    void OptimizeBrewmasterPositioning(::Unit* target);
    void MaintainOptimalTankPosition(::Unit* target);
    void HandleMobCorralling();
    void ExecutePositionalThreatControl();

    // Performance tracking
    void TrackBrewmasterPerformance();
    void AnalyzeStaggerEfficiency();
    void UpdateThreatMetrics();
    void OptimizeBasedOnTankingMetrics();

    // State tracking
    BrewmasterPhase _currentPhase;
    StaggerState _staggerState;
    BrewChargeState _brewChargeState;

    // Target tracking
    std::unordered_map<ObjectGuid, BrewmasterTarget> _brewmasterTargets;
    ObjectGuid _primaryThreatTarget;
    std::vector<ObjectGuid> _aoeTargets;

    // Stagger tracking
    uint32 _currentStaggerDamage;
    uint32 _staggerTickDamage;
    uint32 _lastStaggerTick;
    uint32 _staggerTimeRemaining;
    uint32 _totalStaggerMitigated;

    // Brew tracking
    uint32 _ironskinCharges;
    uint32 _purifyingCharges;
    uint32 _lastBrewUse;
    uint32 _brewRechargeTime;
    uint32 _optimalBrewUsageTime;

    // Threat tracking
    uint32 _currentThreatLevel;
    uint32 _lastThreatCheck;
    uint32 _threatGenerationRate;
    bool _hasSufficientThreat;

    // Keg Smash optimization
    uint32 _lastKegSmashTime;
    uint32 _kegSmashCooldown;
    uint32 _kegSmashTargetsHit;
    bool _kegSmashReady;

    // Defensive cooldown tracking
    uint32 _lastFortifyingBrew;
    uint32 _lastZenMeditation;
    uint32 _lastDampenHarm;
    uint32 _defensiveCooldownsActive;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalTankingDamage;
    uint32 _totalDamageMitigated;
    uint32 _totalThreatGenerated;
    float _averageTankingDps;

    // Performance metrics
    BrewmasterMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _staggerPurgeThreshold{0.6f};
    std::atomic<float> _brewUsageThreshold{0.8f};
    std::atomic<uint32> _optimalBrewCharges{2};
    std::atomic<bool> _enableAdvancedStaggerManagement{true};
    std::atomic<bool> _enableOptimalBrewTiming{true};

    // Constants
    static constexpr uint32 STAGGER_TICK_INTERVAL = 1000; // 1 second
    static constexpr uint32 BREW_RECHARGE_TIME = 21000; // 21 seconds
    static constexpr uint32 KEG_SMASH_COOLDOWN = 8000; // 8 seconds
    static constexpr uint32 BREATH_OF_FIRE_COOLDOWN = 15000; // 15 seconds
    static constexpr uint32 FORTIFYING_BREW_DURATION = 15000; // 15 seconds
    static constexpr uint32 ZEN_MEDITATION_DURATION = 8000; // 8 seconds
    static constexpr float HEAVY_STAGGER_THRESHOLD = 0.6f; // 60% of max health
    static constexpr float MODERATE_STAGGER_THRESHOLD = 0.3f; // 30% of max health
    static constexpr uint8 OPTIMAL_BREW_CHARGES = 2;
    static constexpr uint32 THREAT_CHECK_INTERVAL = 2000; // 2 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr uint32 AOE_TARGET_THRESHOLD = 3;
    static constexpr float OPTIMAL_TANKING_RANGE = 8.0f;
};

} // namespace Playerbot