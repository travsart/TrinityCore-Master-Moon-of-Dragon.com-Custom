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

enum class FirePhase : uint8
{
    OPENING         = 0,  // Initial DoT application and setup
    DOT_MAINTAIN    = 1,  // Living Bomb and ignite management
    BURST_WINDOW    = 2,  // Combustion and critical strike chains
    PYROBLAST_FISH  = 3,  // Hot Streak proc fishing
    AOE_PHASE       = 4,  // Multi-target burning
    EXECUTE         = 5,  // Low health scorch spam
    EMERGENCY       = 6   // Critical situations
};

enum class HotStreakState : uint8
{
    NONE            = 0,  // No streak active
    HEATING_UP      = 1,  // One critical hit
    HOT_STREAK      = 2,  // Two consecutive crits
    CONSUMED        = 3,  // Proc used
    EXPIRED         = 4   // Proc expired
};

enum class CombustionState : uint8
{
    INACTIVE        = 0,  // Combustion not active
    PREPARING       = 1,  // Setting up combustion
    ACTIVE          = 2,  // Combustion window active
    EXTENDING       = 3,  // Extending combustion duration
    ENDING          = 4   // Combustion window ending
};

struct DotTracker
{
    ObjectGuid targetGuid;
    uint32 spellId;
    uint32 applicationTime;
    uint32 duration;
    uint32 tickInterval;
    uint32 nextTickTime;
    uint32 damagePerTick;
    uint32 stackCount;
    bool needsRefresh;

    DotTracker() : spellId(0), applicationTime(0), duration(0), tickInterval(0)
        , nextTickTime(0), damagePerTick(0), stackCount(0), needsRefresh(false) {}
};

/**
 * @brief Enhanced Fire specialization with advanced ignite and combustion mastery
 *
 * Focuses on sophisticated DoT management, Hot Streak optimization,
 * and intelligent combustion window maximization for peak burning damage.
 */
class TC_GAME_API FireSpecialization_Enhanced : public MageSpecialization
{
public:
    explicit FireSpecialization_Enhanced(Player* bot);
    ~FireSpecialization_Enhanced() override = default;

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

    // Advanced DoT management
    void ManageFireDotsOptimally();
    void OptimizeLivingBombPlacement();
    void HandleIgniteStacking();
    void RefreshDotsIntelligently();
    void MaximizeDotEfficiency();

    // Hot Streak mastery
    void ManageHotStreakProcs();
    void OptimizePyrobastUsage(::Unit* target);
    void FishForHotStreakProcs();
    void HandleHotStreakChaining();
    void MaximizeHotStreakValue();

    // Combustion window optimization
    void ExecuteCombustionSequence();
    void PrepareCombustionWindow();
    void OptimizeCombustionTiming();
    void ExtendCombustionDuration();
    void MaximizeCombustionDamage();

    // Critical strike optimization
    void OptimizeCriticalStrikeChaining();
    void HandleCriticalStrikeBonuses();
    void ManageCriticalStrikeBuffs();
    void CoordinateCriticalStrikes();

    // Multi-target fire mastery
    void ExecuteMultiTargetBurning();
    void OptimizeFlamestrikeUsage();
    void HandleDragonBreathTiming();
    void ManageMultiTargetIgnites();
    void CoordinateAoECombustion();

    // Performance analytics
    struct FireMetrics
    {
        std::atomic<uint32> fireballsCast{0};
        std::atomic<uint32> pyroblastsCast{0};
        std::atomic<uint32> hotStreakProcs{0};
        std::atomic<uint32> combustionsExecuted{0};
        std::atomic<uint32> livingBombsApplied{0};
        std::atomic<uint32> igniteStacks{0};
        std::atomic<float> dotUptimePercentage{0.9f};
        std::atomic<float> hotStreakUtilization{0.85f};
        std::atomic<float> combustionEfficiency{0.95f};
        std::atomic<float> criticalStrikeRate{0.4f};
        std::atomic<uint32> multiTargetBurns{0};
        std::atomic<float> averageIgniteDamage{0.0f};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            fireballsCast = 0; pyroblastsCast = 0; hotStreakProcs = 0;
            combustionsExecuted = 0; livingBombsApplied = 0; igniteStacks = 0;
            dotUptimePercentage = 0.9f; hotStreakUtilization = 0.85f;
            combustionEfficiency = 0.95f; criticalStrikeRate = 0.4f;
            multiTargetBurns = 0; averageIgniteDamage = 0.0f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    FireMetrics GetSpecializationMetrics() const { return _metrics; }

    // Scorch weaving for movement
    void HandleMovementWithScorch();
    void OptimizeScorchWeaving();
    void HandleMovementPrediction();
    void ExecuteMobileCasting();

    // Fire shield and defensive optimization
    void ManageFireWard();
    void OptimizeBlastWave();
    void HandleDragonBreathDefense();
    void ExecuteEmergencyDefense();

    // Advanced targeting for fire
    void OptimizeTargetSelection();
    void PrioritizeIgniteTargets();
    void HandleTargetSwitching();
    void ManageMultiTargetPriorities();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteDotMaintenancePhase(::Unit* target);
    void ExecuteBurstWindow(::Unit* target);
    void ExecutePyroblastFishing(::Unit* target);
    void ExecuteAoEPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastFireball(::Unit* target) const;
    bool ShouldCastPyroblast(::Unit* target) const;
    bool ShouldCastScorch(::Unit* target) const;
    bool ShouldCastLivingBomb(::Unit* target) const;
    bool ShouldCastFlamestrike(::Unit* target) const;
    bool ShouldCastDragonBreath(::Unit* target) const;

    // Advanced spell execution
    void ExecuteFireball(::Unit* target);
    void ExecutePyroblast(::Unit* target);
    void ExecuteScorch(::Unit* target);
    void ExecuteLivingBomb(::Unit* target);
    void ExecuteFlamestrike(::Unit* target);
    void ExecuteDragonBreath(::Unit* target);

    // Cooldown management
    bool ShouldUseCombustion() const;
    bool ShouldUseBlastWave() const;
    bool ShouldUseDragonBreath() const;
    bool ShouldUseMirrorImage() const;
    bool ShouldUseIcyVeins() const;

    // Advanced cooldown execution
    void ExecuteCombustion();
    void ExecuteBlastWave();
    void ExecuteDragonBreath();
    void ExecuteMirrorImage();
    void ExecuteIcyVeins();

    // DoT management implementations
    void UpdateDotTracking();
    void RefreshExpiredDots();
    void OptimizeDotRefreshTiming();
    bool ShouldRefreshDot(const DotTracker& dot) const;

    // Hot Streak implementations
    void UpdateHotStreakState();
    void HandleHotStreakProc();
    void ConsumeHotStreakOptimally(::Unit* target);
    bool ShouldDelayHotStreakConsumption() const;

    // Combustion implementations
    void PrepareCombustionSetup();
    void ExecuteCombustionRotation(::Unit* target);
    void HandleCombustionExtension();
    void OptimizeCombustionDamage();

    // Critical strike implementations
    void TrackCriticalStrikes();
    void AnalyzeCriticalStrikePatterns();
    void OptimizeCriticalStrikeTiming();
    void HandleCriticalStrikeChains();

    // Multi-target implementations
    void AnalyzeMultiTargetSituation();
    void ExecuteMultiTargetRotation();
    void OptimizeAoEDamage();
    void HandleMultiTargetCombustion();

    // Target analysis for fire
    void AnalyzeTargetFireResistance(::Unit* target);
    void PredictTargetBurnTime(::Unit* target);
    void AssessIgnitePotential(::Unit* target);
    void EvaluateMultiTargetValue();

    // Resource optimization
    void OptimizeManaForFire();
    void HandleManaEfficiency();
    void PredictManaNeeds();
    void BalanceFireDamageAndMana();

    // Movement and positioning
    void OptimizeFirePositioning(::Unit* target);
    void HandleCastingMovement();
    void ExecuteKitingWithFire();
    void ManageCastingInterruptions();

    // Performance tracking
    void TrackFirePerformance();
    void AnalyzeBurningEfficiency();
    void UpdateDotMetrics();
    void OptimizeBasedOnAnalytics();

    // State tracking
    FirePhase _currentPhase;
    HotStreakState _hotStreakState;
    CombustionState _combustionState;

    // DoT tracking
    std::unordered_map<ObjectGuid, std::vector<DotTracker>> _activeDots;
    uint32 _lastDotRefresh;
    uint32 _dotMaintenanceInterval;
    uint32 _totalDotsActive;

    // Hot Streak tracking
    uint32 _consecutiveCrits;
    uint32 _lastCriticalHit;
    uint32 _hotStreakExpiry;
    uint32 _hotStreakProcsUsed;
    uint32 _hotStreakProcsWasted;

    // Combustion tracking
    uint32 _combustionStartTime;
    uint32 _combustionDuration;
    uint32 _combustionCooldown;
    uint32 _combustionPreparationTime;
    bool _combustionReady;

    // Cooldown tracking
    uint32 _fireballCooldown;
    uint32 _pyroblastCooldown;
    uint32 _scorchCooldown;
    uint32 _livingBombCooldown;
    uint32 _flamestrikeCooldown;
    uint32 _dragonBreathCooldown;
    uint32 _blastWaveCooldown;

    // Critical strike data
    uint32 _totalCriticalHits;
    uint32 _totalSpellsCast;
    float _currentCritRate;
    uint32 _lastCriticalTime;
    uint32 _criticalStrikeStreak;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalFireDamage;
    uint32 _totalIgniteDamage;
    uint32 _totalDotDamage;
    float _averageDps;
    uint32 _highestSingleHit;

    // Multi-target data
    std::vector<ObjectGuid> _burningTargets;
    std::unordered_map<ObjectGuid, uint32> _igniteStacks;
    std::unordered_map<ObjectGuid, uint32> _livingBombTargets;
    uint32 _multiTargetThreshold;
    bool _useAoERotation;

    // Target tracking
    std::unordered_map<ObjectGuid, float> _targetFireResistance;
    std::unordered_map<ObjectGuid, uint32> _targetBurnDuration;
    std::unordered_map<ObjectGuid, uint32> _targetLastIgniteTime;
    ObjectGuid _primaryBurnTarget;

    // Movement tracking
    bool _isMoving;
    uint32 _lastMovementTime;
    uint32 _predictedMovementDuration;
    Position _lastCastingPosition;

    // Performance metrics
    FireMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _dotRefreshThreshold{0.3f};
    std::atomic<float> _hotStreakDelayThreshold{0.5f};
    std::atomic<uint32> _combustionSetupTime{3000};
    std::atomic<bool> _enableAdvancedIgnite{true};
    std::atomic<bool> _enableHotStreakFishing{true};

    // Constants
    static constexpr uint32 HOT_STREAK_DURATION = 10000; // 10 seconds
    static constexpr uint32 COMBUSTION_DURATION = 10000; // 10 seconds
    static constexpr uint32 LIVING_BOMB_DURATION = 12000; // 12 seconds
    static constexpr uint32 IGNITE_DURATION = 4000; // 4 seconds
    static constexpr float DOT_REFRESH_WINDOW = 0.3f; // 30% of duration
    static constexpr uint32 SCORCH_CAST_TIME = 1500; // 1.5 seconds
    static constexpr uint32 FIREBALL_CAST_TIME = 3500; // 3.5 seconds
    static constexpr uint32 PYROBLAST_CAST_TIME = 6000; // 6 seconds
    static constexpr float MULTI_TARGET_THRESHOLD = 3.0f;
    static constexpr uint32 CRITICAL_STRIKE_CHAIN_WINDOW = 5000; // 5 seconds
    static constexpr float COMBUSTION_PREPARATION_THRESHOLD = 0.8f;
    static constexpr uint32 OPTIMAL_CASTING_RANGE = 30000; // 30 yards
};

} // namespace Playerbot