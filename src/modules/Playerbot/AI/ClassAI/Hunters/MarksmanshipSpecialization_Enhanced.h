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

enum class MarksmanshipPhase : uint8
{
    OPENING         = 0,  // Initial setup and positioning
    BURST_WINDOW    = 1,  // Rapid Fire + cooldown stacking
    STEADY_ROTATION = 2,  // Standard shot rotation
    AIMED_SHOT_BURN = 3,  // Focus on Aimed Shot damage
    UTILITY_PHASE   = 4,  // Traps and crowd control
    KITING_PHASE    = 5,  // Movement and range management
    EMERGENCY       = 6   // Critical health situations
};

enum class ShotPriority : uint8
{
    CRITICAL        = 0,  // Must cast immediately
    HIGH            = 1,  // High priority
    MEDIUM          = 2,  // Standard priority
    LOW             = 3,  // Filler shots
    SITUATIONAL     = 4   // Context-dependent
};

enum class RangeCategory : uint8
{
    MELEE_RANGE     = 0,  // 0-5 yards (dead zone)
    SHORT_RANGE     = 1,  // 5-15 yards
    OPTIMAL_RANGE   = 2,  // 15-30 yards
    LONG_RANGE      = 3,  // 30-40 yards
    MAXIMUM_RANGE   = 4   // 40+ yards
};

struct ShotRotationNode
{
    uint32 spellId;
    ShotPriority priority;
    uint32 castTime;
    uint32 cooldown;
    uint32 manaCost;
    float damageCoefficient;
    bool isInstant;
    bool requiresChanneling;
    std::vector<uint32> prerequisites;

    ShotRotationNode() : spellId(0), priority(ShotPriority::MEDIUM)
        , castTime(0), cooldown(0), manaCost(0), damageCoefficient(1.0f)
        , isInstant(true), requiresChanneling(false) {}
};

/**
 * @brief Enhanced Marksmanship specialization with precision shooting optimization
 *
 * Focuses on maximizing ranged damage through precise shot timing,
 * optimal positioning, and intelligent burst window management.
 */
class TC_GAME_API MarksmanshipSpecialization_Enhanced : public HunterSpecialization
{
public:
    explicit MarksmanshipSpecialization_Enhanced(Player* bot);
    ~MarksmanshipSpecialization_Enhanced() override = default;

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

    // Advanced shot rotation
    void ExecuteOptimalShotSequence(::Unit* target);
    void OptimizeShotTiming(::Unit* target);
    void HandleShotClipping();
    void ManageAutoShotWeaving(::Unit* target);

    // Precision aiming system
    void CalculateOptimalAiming(::Unit* target);
    void AdjustForTargetMovement(::Unit* target);
    void CompensateForLatency();
    void OptimizeCriticalStrikeChance(::Unit* target);

    // Burst window optimization
    void ExecuteBurstSequence(::Unit* target);
    void PrepareBurstWindow();
    bool IsBurstWindowOptimal(::Unit* target) const;
    void StackDamageModifiers();
    float CalculateBurstPotential(::Unit* target) const;

    // Range and positioning mastery
    void OptimizeRangeManagement(::Unit* target);
    void ExecuteKitingStrategy(::Unit* target);
    void HandleDeadZoneEscape(::Unit* target);
    void MaintainOptimalDistance(::Unit* target);

    // Multi-shot optimization
    void HandleMultiTargetEngagement();
    void OptimizeVolleyUsage();
    void PrioritizeMultiShotTargets();
    void CalculateAoEEfficiency();

    // Trap mastery
    void ExecuteAdvancedTrapStrategies();
    void OptimizeTrapPlacement();
    void HandleTrapCombinations();
    void PredictEnemyMovement();

    // Aimed Shot specialization
    void OptimizeAimedShotUsage(::Unit* target);
    void CalculateAimedShotDamage(::Unit* target) const;
    void HandleAimedShotInterruption();
    void TimeAimedShotOptimally(::Unit* target);

    // Performance analytics
    struct MarksmanshipMetrics
    {
        std::atomic<uint32> aimedShotsCast{0};
        std::atomic<uint32> steadyShotsCast{0};
        std::atomic<uint32> multiShotsCast{0};
        std::atomic<uint32> rapidFireUsages{0};
        std::atomic<uint32> trapsPlaced{0};
        std::atomic<float> averageRange{25.0f};
        std::atomic<float> criticalStrikeRate{0.25f};
        std::atomic<float> shotAccuracy{0.95f};
        std::atomic<uint32> deadZoneEscapes{0};
        std::atomic<uint32> burstWindowsExecuted{0};
        std::atomic<float> burstWindowEfficiency{0.8f};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            aimedShotsCast = 0; steadyShotsCast = 0; multiShotsCast = 0;
            rapidFireUsages = 0; trapsPlaced = 0; averageRange = 25.0f;
            criticalStrikeRate = 0.25f; shotAccuracy = 0.95f;
            deadZoneEscapes = 0; burstWindowsExecuted = 0;
            burstWindowEfficiency = 0.8f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    MarksmanshipMetrics GetSpecializationMetrics() const { return _metrics; }

    // Talent optimization
    void AnalyzeTalentSynergies();
    void OptimizeForSingleTarget();
    void OptimizeForMultiTarget();
    void RecommendTalentBuilds();

    // Situational adaptation
    void AdaptToEncounterType(uint32 encounterId);
    void HandleMovementHeavyFights();
    void OptimizeForBossMechanics();
    void HandleTargetSwitching();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteBurstPhase(::Unit* target);
    void ExecuteSteadyPhase(::Unit* target);
    void ExecuteAimedShotPhase(::Unit* target);
    void ExecuteUtilityPhase(::Unit* target);
    void ExecuteKitingPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Shot execution optimization
    bool ShouldUseAimedShot(::Unit* target) const;
    bool ShouldUseSteadyShot(::Unit* target) const;
    bool ShouldUseArcaneShot(::Unit* target) const;
    bool ShouldUseMultiShot(::Unit* target) const;
    bool ShouldUseVolley(::Unit* target) const;
    bool ShouldUseSerpentSting(::Unit* target) const;

    // Advanced shot execution
    void ExecuteAimedShot(::Unit* target);
    void ExecuteSteadyShot(::Unit* target);
    void ExecuteArcaneShot(::Unit* target);
    void ExecuteMultiShot(::Unit* target);
    void ExecuteVolley(::Unit* target);
    void ExecuteSerpentSting(::Unit* target);

    // Cooldown optimization
    bool ShouldUseRapidFire() const;
    bool ShouldUseTrueshotAura() const;
    bool ShouldUseReadiness() const;
    bool ShouldUseSilencingShot(::Unit* target) const;
    bool ShouldUseScatterShot(::Unit* target) const;

    // Advanced cooldown execution
    void ExecuteRapidFire();
    void ExecuteTrueshotAura();
    void ExecuteReadiness();
    void ExecuteSilencingShot(::Unit* target);
    void ExecuteScatterShot(::Unit* target);

    // Positioning algorithms
    Position CalculateOptimalPosition(::Unit* target) const;
    Position CalculateKitePosition(::Unit* target) const;
    Position CalculateDeadZoneEscape(::Unit* target) const;
    bool IsPositionOptimal(const Position& pos, ::Unit* target) const;

    // Range analysis
    RangeCategory AnalyzeCurrentRange(::Unit* target) const;
    float CalculateOptimalRange(::Unit* target) const;
    bool IsInDeadZone(::Unit* target) const;
    void HandleRangeAdjustment(::Unit* target);

    // Shot priority system
    void UpdateShotPriorities(::Unit* target);
    ShotRotationNode GetNextOptimalShot(::Unit* target);
    void BuildRotationQueue(::Unit* target);
    void OptimizeRotationOrder();

    // Target analysis
    void AnalyzeTargetCharacteristics(::Unit* target);
    void PredictTargetBehavior(::Unit* target);
    void AssessTargetThreat(::Unit* target);
    void EvaluateTargetValue(::Unit* target);

    // Movement prediction
    void PredictTargetMovement(::Unit* target);
    void CalculateInterceptionPoint(::Unit* target);
    void AdjustAimForMovement(::Unit* target);
    void OptimizeLeadTime(::Unit* target);

    // Trap strategy optimization
    void PlanTrapPlacement();
    void ExecuteTrapCombos();
    void HandleTrapTiming();
    Position CalculateOptimalTrapPosition(::Unit* target);

    // Resource optimization
    void OptimizeManaUsage();
    void PredictManaNeeds();
    void HandleManaStarvation();
    void BalanceResourceConsumption();

    // Performance analysis
    void AnalyzeRotationEfficiency();
    void TrackDamageMetrics();
    void OptimizeActionTiming();
    void UpdatePerformanceMetrics();

    // Situational handling
    void HandleInterrupts(::Unit* target);
    void HandleCrowdControl();
    void HandleEmergencyEscape();
    void HandleTargetSwitch();

    // State tracking
    MarksmanshipPhase _currentPhase;
    RangeCategory _currentRange;

    // Shot rotation state
    std::queue<ShotRotationNode> _rotationQueue;
    std::vector<ShotRotationNode> _availableShots;
    uint32 _lastShotTime;
    uint32 _nextShotTime;
    bool _isChanneling;
    uint32 _channelEndTime;

    // Timing and cooldowns
    uint32 _aimedShotCooldown;
    uint32 _rapidFireCooldown;
    uint32 _readinessCooldown;
    uint32 _silencingShotCooldown;
    uint32 _scatterShotCooldown;
    uint32 _lastAutoShot;
    uint32 _autoShotSpeed;
    uint32 _globalCooldown;

    // Positioning data
    Position _optimalPosition;
    Position _lastKnownPosition;
    float _currentRange;
    float _targetLastDistance;
    uint32 _lastRangeCheck;
    bool _isKiting;
    bool _needsPositioning;

    // Target tracking
    ObjectGuid _primaryTarget;
    std::unordered_map<ObjectGuid, uint32> _targetEngagementTime;
    std::unordered_map<ObjectGuid, float> _targetThreatLevel;
    std::unordered_map<ObjectGuid, Position> _targetLastPosition;
    std::unordered_map<ObjectGuid, uint32> _targetMovementSpeed;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalDamageDealt;
    uint32 _totalShotsFired;
    uint32 _totalCriticalHits;
    uint32 _totalMissedShots;
    float _averageDps;
    float _burstDps;

    // Multi-target data
    std::vector<ObjectGuid> _multiTargets;
    uint32 _multiTargetCount;
    bool _useAoERotation;
    uint32 _volleyTargets;

    // Trap management
    std::vector<Position> _activeTrapPositions;
    std::unordered_map<Position, uint32> _trapCooldowns;
    uint32 _lastTrapPlacement;
    bool _trapComboReady;

    // Performance metrics
    MarksmanshipMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _optimalRangePreference{25.0f};
    std::atomic<float> _aimedShotThreshold{0.8f};
    std::atomic<uint32> _burstWindowDuration{15000};
    std::atomic<bool> _enableAdvancedAiming{true};
    std::atomic<bool> _enablePredictiveMovement{true};

    // Constants
    static constexpr float AIMED_SHOT_CAST_TIME = 2500.0f; // 2.5 seconds
    static constexpr float STEADY_SHOT_CAST_TIME = 1500.0f; // 1.5 seconds
    static constexpr uint32 RAPID_FIRE_DURATION = 15000; // 15 seconds
    static constexpr float OPTIMAL_RANGE_MIN = 20.0f;
    static constexpr float OPTIMAL_RANGE_MAX = 35.0f;
    static constexpr float DEAD_ZONE_RANGE = 8.0f;
    static constexpr uint32 BURST_PREPARATION_TIME = 3000; // 3 seconds
    static constexpr float KITING_DISTANCE = 15.0f;
    static constexpr uint32 TRAP_PLACEMENT_COOLDOWN = 30000; // 30 seconds
    static constexpr float MULTI_TARGET_THRESHOLD = 3.0f;
    static constexpr uint32 PHASE_TRANSITION_DELAY = 1500; // 1.5 seconds
};

} // namespace Playerbot