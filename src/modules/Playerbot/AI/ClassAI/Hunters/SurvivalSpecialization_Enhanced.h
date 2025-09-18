/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "HunterSpecialization.h"
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class SurvivalPhase : uint8
{
    OPENING         = 0,  // Initial engagement setup
    DOT_BUILDUP     = 1,  // Applying damage over time effects
    BURST_WINDOW    = 2,  // Lock and Load + Explosive Shot
    STEADY_ROTATION = 3,  // Standard rotation maintenance
    TRAP_PHASE      = 4,  // Trap-focused gameplay
    MELEE_HYBRID    = 5,  // Close-range combat
    EMERGENCY       = 6   // Critical situations
};

enum class TrapStrategy : uint8
{
    DEFENSIVE       = 0,  // Protect escape routes
    OFFENSIVE       = 1,  // Maximize damage
    CONTROL         = 2,  // Crowd control focus
    AREA_DENIAL     = 3,  // Zone control
    COMBO_SETUP     = 4,  // Multi-trap combinations
    EMERGENCY       = 5   // Panic trapping
};

enum class CombatRange : uint8
{
    MELEE           = 0,  // 0-5 yards (Wing Clip range)
    CLOSE           = 1,  // 5-10 yards (Raptor Strike range)
    SHORT           = 2,  // 10-15 yards
    MEDIUM          = 3,  // 15-25 yards (optimal)
    LONG            = 4,  // 25-35 yards
    MAXIMUM         = 5   // 35+ yards
};

struct TrapConfiguration
{
    uint32 trapType;
    Position location;
    uint32 duration;
    uint32 cooldownRemaining;
    bool isActive;
    uint32 placementTime;
    float effectiveRadius;
    std::vector<ObjectGuid> affectedTargets;

    TrapConfiguration() : trapType(0), duration(0), cooldownRemaining(0)
        , isActive(false), placementTime(0), effectiveRadius(0.0f) {}
};

/**
 * @brief Enhanced Survival specialization with advanced trap mastery and hybrid combat
 *
 * Focuses on damage over time effects, intelligent trap usage, and
 * hybrid melee/ranged combat optimization for maximum survivability.
 */
class TC_GAME_API SurvivalSpecialization_Enhanced : public HunterSpecialization
{
public:
    explicit SurvivalSpecialization_Enhanced(Player* bot);
    ~SurvivalSpecialization_Enhanced() override = default;

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

    // Advanced trap mastery
    void ExecuteAdvancedTrapStrategy();
    void OptimizeTrapPlacement(::Unit* target);
    void ManageTrapCombinations();
    void HandleTrapTiming();
    void ExecuteTrapRotation();

    // DoT management excellence
    void OptimizeDotManagement(::Unit* target);
    void RefreshDotsOptimally(::Unit* target);
    void TrackDotEffectiveness();
    void HandleDotPandemic();

    // Lock and Load optimization
    void ManageLockAndLoadProcs();
    void OptimizeExplosiveShotUsage(::Unit* target);
    void HandleProcBasedRotation();
    void MaximizeProcEfficiency();

    // Hybrid combat mastery
    void ExecuteHybridCombat(::Unit* target);
    void OptimizeMeleeIntegration(::Unit* target);
    void HandleRangeTransitions(::Unit* target);
    void ManageCombatStance(::Unit* target);

    // Survival tactics
    void ExecuteSurvivalTactics();
    void HandleEmergencySituations();
    void OptimizeDefensiveCooldowns();
    void ManageHealthAndMana();

    // Advanced positioning
    void OptimizeSurvivalPositioning(::Unit* target);
    void ExecuteStrategicRetreat();
    void HandleMultipleAttackers();
    void ManageEscapeRoutes();

    // Performance analytics
    struct SurvivalMetrics
    {
        std::atomic<uint32> explosiveShotsCast{0};
        std::atomic<uint32> serpentStingsApplied{0};
        std::atomic<uint32> trapsPlaced{0};
        std::atomic<uint32> lockAndLoadProcs{0};
        std::atomic<uint32> meleeAttacks{0};
        std::atomic<float> dotUptimePercentage{0.85f};
        std::atomic<float> trapEffectiveness{0.9f};
        std::atomic<uint32> emergencyEscapes{0};
        std::atomic<uint32> hybridCombatTime{0};
        std::atomic<float> survivalRate{0.95f};
        std::atomic<float> procUtilizationRate{0.8f};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            explosiveShotsCast = 0; serpentStingsApplied = 0; trapsPlaced = 0;
            lockAndLoadProcs = 0; meleeAttacks = 0; dotUptimePercentage = 0.85f;
            trapEffectiveness = 0.9f; emergencyEscapes = 0; hybridCombatTime = 0;
            survivalRate = 0.95f; procUtilizationRate = 0.8f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    SurvivalMetrics GetSpecializationMetrics() const { return _metrics; }

    // Talent synergy optimization
    void OptimizeTalentSynergies();
    void AnalyzePointAllocation();
    void RecommendSpecOptimizations();
    void AdaptToPlaystyle();

    // Multi-target specialization
    void HandleMultiTargetSurvival();
    void OptimizeAoETrapUsage();
    void ManageTargetPriorities();
    void ExecuteGroupCombatTactics();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteDotBuildupPhase(::Unit* target);
    void ExecuteBurstPhase(::Unit* target);
    void ExecuteSteadyPhase(::Unit* target);
    void ExecuteTrapPhase(::Unit* target);
    void ExecuteMeleeHybridPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // DoT optimization
    bool ShouldApplySerpentSting(::Unit* target) const;
    bool ShouldRefreshSerpentSting(::Unit* target) const;
    bool ShouldApplyBlackArrow(::Unit* target) const;
    void ExecuteSerpentSting(::Unit* target);
    void ExecuteBlackArrow(::Unit* target);
    void ManageDotPriorities(::Unit* target);

    // Shot execution
    bool ShouldUseExplosiveShot(::Unit* target) const;
    bool ShouldUseSteadyShot(::Unit* target) const;
    bool ShouldUseArcaneShot(::Unit* target) const;
    bool ShouldUseMultiShot(::Unit* target) const;
    void ExecuteExplosiveShot(::Unit* target);
    void ExecuteSteadyShot(::Unit* target);
    void ExecuteArcaneShot(::Unit* target);
    void ExecuteMultiShot(::Unit* target);

    // Melee integration
    bool ShouldUseRaptorStrike(::Unit* target) const;
    bool ShouldUseWingClip(::Unit* target) const;
    bool ShouldUseMongooseBite(::Unit* target) const;
    void ExecuteRaptorStrike(::Unit* target);
    void ExecuteWingClip(::Unit* target);
    void ExecuteMongooseBite(::Unit* target);

    // Trap execution
    void ExecuteFreezingTrap(const Position& location);
    void ExecuteExplosiveTrap(const Position& location);
    void ExecuteImmolationTrap(const Position& location);
    void ExecuteFrostTrap(const Position& location);
    void ExecuteSnakeTrap(const Position& location);

    // Cooldown management
    bool ShouldUseWyvernSting(::Unit* target) const;
    bool ShouldUseCounterattack(::Unit* target) const;
    bool ShouldUseDeterrence() const;
    void ExecuteWyvernSting(::Unit* target);
    void ExecuteCounterattack(::Unit* target);
    void ExecuteDeterrence();

    // Trap strategy implementation
    void ExecuteDefensiveTrapping();
    void ExecuteOffensiveTrapping(::Unit* target);
    void ExecuteControlTrapping();
    void ExecuteAreaDenialTrapping();
    void ExecuteComboTrapping();
    void ExecuteEmergencyTrapping();

    // Positioning algorithms
    Position CalculateOptimalSurvivalPosition(::Unit* target) const;
    Position CalculateEscapeRoute() const;
    Position CalculateTrapPosition(::Unit* target, uint32 trapType) const;
    bool IsPositionSafe(const Position& pos) const;

    // Range management
    CombatRange DetermineOptimalRange(::Unit* target) const;
    void TransitionToRange(CombatRange targetRange, ::Unit* target);
    bool ShouldEngageMelee(::Unit* target) const;
    void HandleRangeTransition(::Unit* target);

    // Proc management
    void HandleLockAndLoadProc();
    void OptimizeProcWindow();
    void TrackProcDuration();
    void MaximizeProcDamage(::Unit* target);

    // Survival analysis
    void AssessThreatLevel();
    void EvaluateEscapeOptions();
    void PredictDangerZones();
    void CalculateSurvivalProbability();

    // Resource optimization
    void OptimizeManaForSurvival();
    void ManageHealthThresholds();
    void HandleResourceEmergencies();
    void BalanceDamageAndSurvival();

    // Tactical decision making
    void MakeTacticalDecisions(::Unit* target);
    void AdaptToThreatLevel();
    void PrioritizeSurvivalActions();
    void OptimizeActionSequence();

    // Performance tracking
    void TrackRotationEfficiency();
    void AnalyzeSurvivalPatterns();
    void UpdateMetricsData();
    void OptimizeBasedOnMetrics();

    // State tracking
    SurvivalPhase _currentPhase;
    TrapStrategy _currentTrapStrategy;
    CombatRange _currentRange;

    // DoT tracking
    std::unordered_map<ObjectGuid, uint32> _serpentStingDuration;
    std::unordered_map<ObjectGuid, uint32> _blackArrowDuration;
    std::unordered_map<ObjectGuid, uint32> _dotApplicationTime;
    uint32 _dotRefreshWindow;

    // Proc tracking
    bool _lockAndLoadActive;
    uint32 _lockAndLoadExpiry;
    uint32 _lockAndLoadStacks;
    uint32 _lastLockAndLoadProc;
    uint32 _procWindowStart;

    // Trap management
    std::vector<TrapConfiguration> _activeTraps;
    std::unordered_map<uint32, uint32> _trapCooldowns;
    uint32 _lastTrapPlacement;
    Position _lastTrapPosition;
    TrapStrategy _activeTrapStrategy;

    // Cooldown tracking
    uint32 _explosiveShotCooldown;
    uint32 _wyvernStingCooldown;
    uint32 _counterattackCooldown;
    uint32 _deterrenceCooldown;
    uint32 _lastRaptorStrike;
    uint32 _lastWingClip;
    uint32 _lastMongooseBite;

    // Combat state
    bool _inMeleeRange;
    bool _isKiting;
    bool _emergencyMode;
    uint32 _combatStartTime;
    uint32 _lastRangeCheck;
    uint32 _lastPositionUpdate;

    // Target analysis
    std::unordered_map<ObjectGuid, uint32> _targetThreatLevels;
    std::unordered_map<ObjectGuid, float> _targetDistance;
    std::unordered_map<ObjectGuid, uint32> _targetEngagementTime;
    ObjectGuid _primaryTarget;

    // Performance metrics
    SurvivalMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Survival data
    uint32 _totalDamageDealt;
    uint32 _totalDamageTaken;
    uint32 _emergencyActionsUsed;
    uint32 _successfulEscapes;
    float _averageSurvivalTime;

    // Configuration
    std::atomic<float> _dotPriorityWeight{0.8f};
    std::atomic<float> _survivalThreshold{0.3f};
    std::atomic<uint32> _emergencyHealthThreshold{25};
    std::atomic<bool> _enableHybridCombat{true};
    std::atomic<bool> _enableAdvancedTrapping{true};

    // Constants
    static constexpr uint32 SERPENT_STING_DURATION = 15000; // 15 seconds
    static constexpr uint32 BLACK_ARROW_DURATION = 20000; // 20 seconds
    static constexpr uint32 LOCK_AND_LOAD_DURATION = 20000; // 20 seconds
    static constexpr float MELEE_RANGE_THRESHOLD = 5.0f;
    static constexpr float CLOSE_RANGE_THRESHOLD = 10.0f;
    static constexpr float OPTIMAL_RANGE_MIN = 15.0f;
    static constexpr float OPTIMAL_RANGE_MAX = 25.0f;
    static constexpr uint32 TRAP_PLACEMENT_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 DOT_REFRESH_WINDOW = 3000; // 3 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f;
    static constexpr uint32 PROC_UTILIZATION_WINDOW = 5000; // 5 seconds
    static constexpr uint32 HYBRID_COMBAT_THRESHOLD = 8000; // 8 yards
};

} // namespace Playerbot