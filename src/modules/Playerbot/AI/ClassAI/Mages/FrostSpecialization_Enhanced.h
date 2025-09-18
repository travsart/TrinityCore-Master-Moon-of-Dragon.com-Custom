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

enum class FrostPhase : uint8
{
    OPENING         = 0,  // Initial setup and water elemental summon
    FREEZE_CONTROL  = 1,  // Crowd control and freezing effects
    SHATTER_COMBO   = 2,  // Shatter mechanics optimization
    DEEP_FREEZE     = 3,  // Deep freeze burst window
    BLIZZARD_PHASE  = 4,  // AoE damage phase
    KITING_PHASE    = 5,  // Movement and slowing
    EMERGENCY       = 6   // Critical situations
};

enum class FreezeState : uint8
{
    NONE            = 0,  // No freeze effects active
    FROST_NOVA      = 1,  // Frost Nova active
    DEEP_FREEZE     = 2,  // Deep Freeze active
    WATER_ELEMENTAL = 3,  // Water Elemental freeze
    ICE_BARRIER     = 4,  // Ice Barrier defensive
    SHATTERED       = 5   // Post-shatter state
};

enum class FingersOfFrostState : uint8
{
    INACTIVE        = 0,  // No charges available
    SINGLE_CHARGE   = 1,  // One charge ready
    DOUBLE_CHARGE   = 2,  // Two charges ready
    CONSUMING       = 3   // Currently using charges
};

struct FrostTarget
{
    ObjectGuid targetGuid;
    bool isFrozen;
    bool hasSlowEffect;
    uint32 freezeDuration;
    uint32 slowDuration;
    uint32 lastFrostbolt;
    uint32 shatterWindow;
    float slownessPercent;
    bool isKited;

    FrostTarget() : isFrozen(false), hasSlowEffect(false), freezeDuration(0)
        , slowDuration(0), lastFrostbolt(0), shatterWindow(0)
        , slownessPercent(0.0f), isKited(false) {}
};

/**
 * @brief Enhanced Frost specialization with advanced crowd control and shatter mechanics
 *
 * Focuses on sophisticated freeze/shatter combinations, water elemental mastery,
 * and intelligent crowd control optimization for maximum battlefield control.
 */
class TC_GAME_API FrostSpecialization_Enhanced : public MageSpecialization
{
public:
    explicit FrostSpecialization_Enhanced(Player* bot);
    ~FrostSpecialization_Enhanced() override = default;

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

    // Advanced freeze mechanics
    void ManageFreezeEffectsOptimally();
    void ExecuteShatterCombinations();
    void OptimizeFrostNovaUsage();
    void HandleDeepFreezeWindows();
    void CoordinateFreezeChaining();

    // Water Elemental mastery
    void ManageWaterElementalOptimally();
    void OptimizeElementalPositioning();
    void CoordinateElementalAttacks();
    void HandleElementalDefense();
    void MaximizeElementalUtility();

    // Shatter mechanics optimization
    void ExecuteOptimalShatterRotation();
    void HandleFingersOfFrostProcs();
    void OptimizeIceLanceUsage();
    void ManageShatterWindows();
    void MaximizeShatterDamage();

    // Advanced crowd control
    void ExecuteAdvancedCrowdControl();
    void ManageMultipleTargetFreezing();
    void OptimizePolymorphChaining();
    void HandleCrowdControlPriorities();
    void CoordinateGroupCrowdControl();

    // Kiting and movement mastery
    void ExecutePerfectKiting(::Unit* target);
    void OptimizeMovementAndSlowing();
    void HandleMultiTargetKiting();
    void ManageKitingResources();
    void ExecuteEmergencyKiting();

    // Performance analytics
    struct FrostMetrics
    {
        std::atomic<uint32> frostboltsCast{0};
        std::atomic<uint32> iceLancesCast{0};
        std::atomic<uint32> frostNovaCasts{0};
        std::atomic<uint32> deepFreezeCasts{0};
        std::atomic<uint32> shatterCombos{0};
        std::atomic<uint32> fingersOfFrostProcs{0};
        std::atomic<uint32> waterElementalSummons{0};
        std::atomic<float> freezeUptime{0.85f};
        std::atomic<float> shatterCritRate{0.95f};
        std::atomic<float> crowdControlEfficiency{0.9f};
        std::atomic<float> kitingEffectiveness{0.8f};
        std::atomic<uint32> successfulKites{0};
        std::atomic<uint32> emergencyEscapes{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            frostboltsCast = 0; iceLancesCast = 0; frostNovaCasts = 0;
            deepFreezeCasts = 0; shatterCombos = 0; fingersOfFrostProcs = 0;
            waterElementalSummons = 0; freezeUptime = 0.85f; shatterCritRate = 0.95f;
            crowdControlEfficiency = 0.9f; kitingEffectiveness = 0.8f;
            successfulKites = 0; emergencyEscapes = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    FrostMetrics GetSpecializationMetrics() const { return _metrics; }

    // Blizzard and AoE optimization
    void ManageBlizzardOptimally();
    void OptimizeBlizzardPlacement();
    void HandleConeOfColdTiming();
    void ExecuteAoEFreezing();
    void CoordinateAoECombination();

    // Ice Block and defensive mastery
    void ManageIceBlockOptimally();
    void OptimizeIceBarrierUsage();
    void HandleColdSnapStrategy();
    void ExecuteDefensiveFrost();
    void CoordinateDefensiveCooldowns();

    // Advanced frost tactics
    void HandleFrostWarding();
    void OptimizeManaShieldWithFrost();
    void ExecuteFrostTeleportStrategies();
    void ManageFrostArmorOptimally();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteFreezeControlPhase(::Unit* target);
    void ExecuteShatterComboPhase(::Unit* target);
    void ExecuteDeepFreezePhase(::Unit* target);
    void ExecuteBlizzardPhase(::Unit* target);
    void ExecuteKitingPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastFrostbolt(::Unit* target) const;
    bool ShouldCastIceLance(::Unit* target) const;
    bool ShouldCastFrostNova(::Unit* target) const;
    bool ShouldCastDeepFreeze(::Unit* target) const;
    bool ShouldCastBlizzard(::Unit* target) const;
    bool ShouldCastConeOfCold(::Unit* target) const;

    // Advanced spell execution
    void ExecuteFrostbolt(::Unit* target);
    void ExecuteIceLance(::Unit* target);
    void ExecuteFrostNova(::Unit* target);
    void ExecuteDeepFreeze(::Unit* target);
    void ExecuteBlizzard(::Unit* target);
    void ExecuteConeOfCold(::Unit* target);

    // Cooldown management
    bool ShouldSummonWaterElemental() const;
    bool ShouldUseIcyVeins() const;
    bool ShouldUseColdSnap() const;
    bool ShouldUseIceBlock() const;
    bool ShouldUseIceBarrier() const;

    // Advanced cooldown execution
    void ExecuteSummonWaterElemental();
    void ExecuteIcyVeins();
    void ExecuteColdSnap();
    void ExecuteIceBlock();
    void ExecuteIceBarrier();

    // Freeze mechanics implementations
    void UpdateFreezeTracking();
    void ApplyFreezeEffect(::Unit* target, uint32 duration);
    void HandleFreezeExpiry(::Unit* target);
    bool IsTargetFrozen(::Unit* target) const;

    // Shatter implementations
    void ExecuteShatterCombo(::Unit* target);
    void HandleFingersOfFrostConsumption(::Unit* target);
    void OptimizeShatterTiming(::Unit* target);
    bool IsInShatterWindow(::Unit* target) const;

    // Water Elemental implementations
    void UpdateWaterElementalBehavior();
    void CommandElementalFreeze(::Unit* target);
    void PositionElementalOptimally();
    void HandleElementalThreat();

    // Crowd control implementations
    void PrioritizeCrowdControlTargets();
    void ExecuteCrowdControlRotation();
    void HandleCrowdControlDiminishingReturns();
    void CoordinateWithGroupCrowdControl();

    // Kiting implementations
    void CalculateKitingPath(::Unit* target);
    void ExecuteKitingMovement();
    void ApplySlowingEffects(::Unit* target);
    void HandleKitingEmergencies();

    // Target analysis for frost
    void AnalyzeTargetFrostSusceptibility(::Unit* target);
    void TrackTargetMovementPatterns(::Unit* target);
    void PredictTargetBehavior(::Unit* target);
    void AssessKitingViability(::Unit* target);

    // Resource management for frost
    void OptimizeManaForFrost();
    void HandleManaEfficiencyInKiting();
    void BalanceDamageAndControl();
    void PredictControlResourceNeeds();

    // Position optimization
    void OptimizeFrostPositioning(::Unit* target);
    void HandleRangedKitingPosition();
    void ManageSafeDistances();
    void ExecuteStrategicRepositioning();

    // Performance analysis
    void TrackFrostControlEfficiency();
    void AnalyzeShatterPerformance();
    void UpdateKitingMetrics();
    void OptimizeBasedOnFrostMetrics();

    // Emergency frost tactics
    void HandleOverwhelmingSituation();
    void ExecuteEmergencyCrowdControl();
    void HandleLowHealthKiting();
    void ExecuteLastResortFrost();

    // State tracking
    FrostPhase _currentPhase;
    FreezeState _freezeState;
    FingersOfFrostState _fingersOfFrostState;

    // Freeze tracking
    std::unordered_map<ObjectGuid, FrostTarget> _frostTargets;
    uint32 _lastFrostNova;
    uint32 _lastDeepFreeze;
    uint32 _globalFreezeWindow;
    uint32 _shatterWindowStart;

    // Water Elemental tracking
    bool _waterElementalActive;
    ObjectGuid _waterElementalGuid;
    Position _elementalPosition;
    uint32 _elementalLastCommand;
    uint32 _elementalSummonTime;

    // Proc tracking
    uint32 _fingersOfFrostCharges;
    uint32 _lastFingersOfFrostProc;
    uint32 _brainFreezeProc;
    uint32 _lastBrainFreezeUsed;

    // Cooldown tracking
    uint32 _frostboltCooldown;
    uint32 _iceLanceCooldown;
    uint32 _frostNovaCooldown;
    uint32 _deepFreezeCooldown;
    uint32 _blizzardCooldown;
    uint32 _coneOfColdCooldown;
    uint32 _icyVeinsCooldown;
    uint32 _coldSnapCooldown;

    // Kiting data
    bool _isKiting;
    Position _kitingDestination;
    std::vector<Position> _kitingPath;
    uint32 _lastKitingMovement;
    float _optimalKitingRange;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalFrostDamage;
    uint32 _totalControlTime;
    uint32 _totalShatterDamage;
    float _averageControlUptime;
    uint32 _successfulFreezes;

    // Multi-target tracking
    std::vector<ObjectGuid> _controlledTargets;
    std::unordered_map<ObjectGuid, uint32> _targetFreezeExpiry;
    std::unordered_map<ObjectGuid, uint32> _targetSlowExpiry;
    uint32 _multiTargetControlCount;

    // Blizzard management
    Position _blizzardCenter;
    uint32 _blizzardStartTime;
    uint32 _blizzardDuration;
    bool _blizzardActive;
    std::vector<ObjectGuid> _blizzardTargets;

    // Performance metrics
    FrostMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _freezePriorityWeight{0.8f};
    std::atomic<float> _kitingDistanceOptimal{20.0f};
    std::atomic<uint32> _shatterWindowDuration{3000};
    std::atomic<bool> _enableAdvancedKiting{true};
    std::atomic<bool> _enableElementalMicro{true};

    // Constants
    static constexpr uint32 FROST_NOVA_DURATION = 8000; // 8 seconds
    static constexpr uint32 DEEP_FREEZE_DURATION = 5000; // 5 seconds
    static constexpr uint32 FINGERS_OF_FROST_DURATION = 15000; // 15 seconds
    static constexpr uint32 WATER_ELEMENTAL_DURATION = 60000; // 60 seconds
    static constexpr uint32 ICY_VEINS_DURATION = 20000; // 20 seconds
    static constexpr uint32 SHATTER_WINDOW = 3000; // 3 seconds
    static constexpr float KITING_RANGE_MIN = 15.0f;
    static constexpr float KITING_RANGE_MAX = 35.0f;
    static constexpr float OPTIMAL_FROST_RANGE = 25.0f;
    static constexpr uint32 BLIZZARD_CHANNEL_TIME = 8000; // 8 seconds
    static constexpr float FREEZE_UPTIME_TARGET = 0.85f;
    static constexpr uint32 CROWD_CONTROL_PRIORITY_WINDOW = 5000; // 5 seconds
    static constexpr float ELEMENTAL_OPTIMAL_RANGE = 30.0f;
};

} // namespace Playerbot