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
#include <queue>

namespace Playerbot
{

enum class SubtletyPhase : uint8
{
    STEALTH_SETUP   = 0,  // Preparing stealth windows
    OPENER_EXECUTE  = 1,  // Executing stealth openers
    SHADOW_DANCE    = 2,  // Shadow Dance burst windows
    HEMORRHAGE_MAIN = 3,  // Hemorrhage maintenance
    COMBO_SUSTAIN   = 4,  // Sustained combo point generation
    STEALTH_RESET   = 5,  // Preparation/Vanish resets
    EMERGENCY       = 6   // Critical situations
};

enum class StealthWindowState : uint8
{
    PLANNING        = 0,  // Planning next stealth window
    PREPARING       = 1,  // Setting up for stealth
    ACTIVE          = 2,  // Currently in stealth
    EXECUTING       = 3,  // Executing stealth rotation
    ENDING          = 4,  // Ending stealth window
    COOLDOWN        = 5   // Stealth abilities on cooldown
};

enum class ShadowDanceState : uint8
{
    READY           = 0,  // Shadow Dance available
    PREPARING       = 1,  // Setting up Shadow Dance
    ACTIVE          = 2,  // Shadow Dance active
    OPTIMIZING      = 3,  // Maximizing Shadow Dance window
    COOLDOWN        = 4   // Shadow Dance on cooldown
};

struct SubtletyTarget
{
    ObjectGuid targetGuid;
    bool hasHemorrhage;
    uint32 hemorrhageStacks;
    uint32 hemorrhageTimeRemaining;
    uint32 lastBackstabTime;
    uint32 lastAmbushTime;
    float stealthAdvantage;
    bool isOptimalForStealth;
    uint32 shadowstepOpportunities;
    Position lastKnownPosition;

    SubtletyTarget() : hasHemorrhage(false), hemorrhageStacks(0)
        , hemorrhageTimeRemaining(0), lastBackstabTime(0), lastAmbushTime(0)
        , stealthAdvantage(0.0f), isOptimalForStealth(false)
        , shadowstepOpportunities(0) {}
};

struct StealthWindow
{
    uint32 startTime;
    uint32 duration;
    uint32 plannedAbilities;
    uint32 executedAbilities;
    uint32 damageDealt;
    bool wasOptimal;
    StealthState triggerType;

    StealthWindow() : startTime(0), duration(0), plannedAbilities(0)
        , executedAbilities(0), damageDealt(0), wasOptimal(false)
        , triggerType(StealthState::NONE) {}
};

/**
 * @brief Enhanced Subtlety specialization with advanced stealth mastery and Shadow Dance optimization
 *
 * Focuses on sophisticated stealth window planning, Shadow Dance burst coordination,
 * and intelligent positioning with Shadowstep for maximum burst damage windows.
 */
class TC_GAME_API SubtletySpecialization_Enhanced : public RogueSpecialization
{
public:
    explicit SubtletySpecialization_Enhanced(Player* bot);
    ~SubtletySpecialization_Enhanced() override = default;

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

    // Advanced stealth mastery
    void ManageStealthWindowsOptimally();
    void OptimizeStealthOpenerSelection(::Unit* target);
    void ExecutePerfectStealthSequence(::Unit* target);
    void CoordinateStealthCooldowns();
    void MaximizeStealthAdvantage();

    // Shadow Dance mastery
    void ManageShadowDanceOptimally();
    void OptimizeShadowDanceTiming();
    void ExecutePerfectShadowDanceBurst();
    void CoordinateShadowDanceRotation();
    void MaximizeShadowDanceEfficiency();

    // Shadowstep and positioning mastery
    void ManageShadowstepOptimally();
    void OptimizeShadowstepPositioning(::Unit* target);
    void ExecuteTacticalShadowstep(::Unit* target);
    void CoordinateShadowstepWithBurst();
    void MaximizePositionalAdvantage();

    // Hemorrhage and debuff mastery
    void ManageHemorrhageOptimally();
    void OptimizeHemorrhageStacking(::Unit* target);
    void HandleHemorrhageRefreshes(::Unit* target);
    void CoordinateHemorrhageWithBurst();
    void MaximizeHemorrhageDamage();

    // Preparation and cooldown mastery
    void ManagePreparationOptimally();
    void OptimizeCooldownResets();
    void CoordinatePreparationTiming();
    void HandleEmergencyPreparation();
    void MaximizeCooldownEfficiency();

    // Performance analytics
    struct SubtletyMetrics
    {
        std::atomic<uint32> stealthWindows{0};
        std::atomic<uint32> shadowDanceActivations{0};
        std::atomic<uint32> shadowstepUses{0};
        std::atomic<uint32> ambushCasts{0};
        std::atomic<uint32> backstabCasts{0};
        std::atomic<uint32> hemorrhageApplications{0};
        std::atomic<uint32> preparationUses{0};
        std::atomic<float> stealthWindowEfficiency{0.9f};
        std::atomic<float> shadowDanceEfficiency{0.95f};
        std::atomic<float> positionalAdvantagePercentage{0.8f};
        std::atomic<float> hemorrhageUptime{0.9f};
        std::atomic<uint32> masterOfSubtletyProcs{0};
        std::atomic<uint32> opportunityProcs{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            stealthWindows = 0; shadowDanceActivations = 0; shadowstepUses = 0;
            ambushCasts = 0; backstabCasts = 0; hemorrhageApplications = 0;
            preparationUses = 0; stealthWindowEfficiency = 0.9f; shadowDanceEfficiency = 0.95f;
            positionalAdvantagePercentage = 0.8f; hemorrhageUptime = 0.9f;
            masterOfSubtletyProcs = 0; opportunityProcs = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    SubtletyMetrics GetSpecializationMetrics() const { return _metrics; }

    // Advanced opener optimization
    void OptimizeStealthOpeners();
    void HandleOpenerVariations(::Unit* target);
    void CoordinateOpenerWithBurst();
    void MaximizeOpenerDamage();

    // Master of Subtlety optimization
    void ManageMasterOfSubtletyOptimally();
    void OptimizeMasterOfSubtletyWindows();
    void CoordinateMasterOfSubtletyWithRotation();
    void MaximizeMasterOfSubtletyBenefit();

    // Energy management for subtlety
    void OptimizeEnergyForSubtlety();
    void HandleEnergyForStealthWindows();
    void PredictEnergyForBurst();
    void BalanceEnergyAndStealth();

private:
    // Enhanced rotation phases
    void ExecuteStealthSetupPhase(::Unit* target);
    void ExecuteOpenerExecutePhase(::Unit* target);
    void ExecuteShadowDancePhase(::Unit* target);
    void ExecuteHemorrhageMainPhase(::Unit* target);
    void ExecuteComboSustainPhase(::Unit* target);
    void ExecuteStealthResetPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastAmbush(::Unit* target) const;
    bool ShouldCastBackstab(::Unit* target) const;
    bool ShouldCastHemorrhage(::Unit* target) const;
    bool ShouldCastEviscerate(::Unit* target) const;
    bool ShouldCastGarrote(::Unit* target) const;

    // Advanced spell execution
    void ExecuteAmbush(::Unit* target);
    void ExecuteBackstab(::Unit* target);
    void ExecuteHemorrhage(::Unit* target);
    void ExecuteEviscerate(::Unit* target);
    void ExecuteGarrote(::Unit* target);

    // Cooldown management
    bool ShouldUseShadowDance() const;
    bool ShouldUseShadowstep(::Unit* target) const;
    bool ShouldUseVanish() const;
    bool ShouldUsePreparation() const;
    bool ShouldUsePremeditation() const;

    // Advanced cooldown execution
    void ExecuteShadowDance();
    void ExecuteShadowstep(::Unit* target);
    void ExecuteVanish();
    void ExecutePreparation();
    void ExecutePremeditation();

    // Stealth window implementations
    void PlanOptimalStealthWindow(::Unit* target);
    void ExecuteStealthWindow(::Unit* target);
    void AnalyzeStealthWindowPerformance(const StealthWindow& window);
    void OptimizeStealthWindowSequence(::Unit* target);

    // Shadow Dance implementations
    void PrepareShadowDanceWindow();
    void ExecuteShadowDanceRotation(::Unit* target);
    void OptimizeShadowDanceDuration();
    bool IsInShadowDanceWindow() const;

    // Shadowstep implementations
    void CalculateOptimalShadowstepPosition(::Unit* target);
    void ExecuteShadowstepSequence(::Unit* target);
    void HandleShadowstepCooldown();
    bool CanShadowstepToTarget(::Unit* target) const;

    // Hemorrhage implementations
    void UpdateHemorrhageTracking();
    void RefreshHemorrhage(::Unit* target);
    void OptimizeHemorrhageStacks(::Unit* target);
    bool ShouldRefreshHemorrhage(::Unit* target) const;

    // Stealth opener implementations
    void ExecuteAmbushOpener(::Unit* target);
    void ExecuteGarroteOpener(::Unit* target);
    void ExecuteCheapShotOpener(::Unit* target);
    void ExecutePremeditationOpener(::Unit* target);

    // Position optimization
    void OptimizeSubtletyPositioning(::Unit* target);
    void MaintainBehindTargetAdvantage(::Unit* target);
    void HandleStealthPositioning();
    void ExecutePositionalCorrection(::Unit* target);

    // Target analysis for subtlety
    void AnalyzeTargetForSubtlety(::Unit* target);
    void AssessStealthOpportunities(::Unit* target);
    void PredictTargetMovementPatterns(::Unit* target);
    void OptimizeTargetSelectionForStealth();

    // Burst coordination
    void CoordinateSubtletyBurstWindows();
    void OptimizeBurstSequencing();
    void HandleBurstCooldownAlignment();
    void MaximizeBurstWindowDamage();

    // Master of Subtlety tracking
    void UpdateMasterOfSubtletyTracking();
    void OptimizeMasterOfSubtletyUsage();
    uint32 GetMasterOfSubtletyTimeRemaining() const;
    bool IsInMasterOfSubtletyWindow() const;

    // Performance tracking
    void TrackSubtletyPerformance();
    void AnalyzeStealthEfficiency();
    void UpdatePositionalMetrics();
    void OptimizeBasedOnSubtletyMetrics();

    // Emergency handling
    void HandleStealthEmergency();
    void ExecuteEmergencyVanish();
    void HandleLowHealthSubtlety();
    void ExecuteEmergencyEscape();

    // State tracking
    SubtletyPhase _currentPhase;
    StealthWindowState _stealthWindowState;
    ShadowDanceState _shadowDanceState;

    // Target tracking
    std::unordered_map<ObjectGuid, SubtletyTarget> _subtletyTargets;
    ObjectGuid _primaryTarget;

    // Stealth window tracking
    std::queue<StealthWindow> _stealthWindowHistory;
    StealthWindow _currentStealthWindow;
    uint32 _plannedStealthWindows;
    uint32 _nextStealthWindowTime;

    // Shadow Dance tracking
    uint32 _shadowDanceStartTime;
    uint32 _shadowDanceDuration;
    bool _shadowDanceActive;
    uint32 _shadowDanceCooldown;

    // Shadowstep tracking
    uint32 _lastShadowstepTime;
    uint32 _shadowstepCooldown;
    Position _shadowstepTargetPosition;
    bool _shadowstepQueued;

    // Hemorrhage tracking
    uint32 _lastHemorrhageTime;
    uint32 _hemorrhageRefreshWindow;
    bool _hemorrhageNeedsRefresh;

    // Stealth tracking
    uint32 _lastStealthTime;
    uint32 _lastVanishTime;
    uint32 _stealthAdvantageWindow;
    bool _hasStealthAdvantage;

    // Master of Subtlety tracking
    uint32 _masterOfSubtletyStartTime;
    uint32 _masterOfSubtletyDuration;
    bool _masterOfSubtletyActive;
    uint32 _masterOfSubtletyProcs;

    // Combo point optimization
    uint32 _lastAmbushTime;
    uint32 _lastBackstabTime;
    uint32 _lastEviscerateeTime;
    uint32 _optimalComboPointsForFinisher;

    // Preparation tracking
    uint32 _lastPreparationTime;
    uint32 _preparationCooldown;
    bool _preparationReady;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalSubtletyDamage;
    uint32 _totalStealthDamage;
    uint32 _totalShadowDanceDamage;
    float _averageSubtletyDps;

    // Performance metrics
    SubtletyMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _hemorrhageRefreshThreshold{0.3f};
    std::atomic<uint32> _optimalStealthWindowDuration{8000};
    std::atomic<uint32> _shadowDanceOptimalDuration{8000};
    std::atomic<bool> _enableAdvancedStealth{true};
    std::atomic<bool> _enableOptimalPositioning{true};

    // Constants
    static constexpr uint32 SHADOW_DANCE_DURATION = 8000; // 8 seconds
    static constexpr uint32 SHADOWSTEP_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 PREPARATION_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 HEMORRHAGE_DURATION = 15000; // 15 seconds
    static constexpr uint32 MASTER_OF_SUBTLETY_DURATION = 6000; // 6 seconds
    static constexpr uint32 STEALTH_DURATION = 10000; // 10 seconds
    static constexpr uint32 VANISH_DURATION = 10000; // 10 seconds
    static constexpr float HEMORRHAGE_PANDEMIC_THRESHOLD = 0.3f; // 30% duration
    static constexpr uint8 OPTIMAL_AMBUSH_COMBO_POINTS = 0; // Ambush doesn't cost combo points
    static constexpr uint8 OPTIMAL_EVISCERATE_COMBO_POINTS = 5;
    static constexpr uint32 STEALTH_WINDOW_PLANNING_TIME = 3000; // 3 seconds
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.30f; // 30% health
    static constexpr uint32 ENERGY_RESERVE_FOR_STEALTH = 60;
    static constexpr float OPTIMAL_SUBTLETY_RANGE = 5.0f;
};

} // namespace Playerbot