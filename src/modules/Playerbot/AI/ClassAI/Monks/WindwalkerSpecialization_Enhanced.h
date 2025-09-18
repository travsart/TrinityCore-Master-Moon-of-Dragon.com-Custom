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

enum class WindwalkerPhase : uint8
{
    OPENING         = 0,  // Initial engagement setup
    CHI_GENERATION  = 1,  // Building chi resources
    COMBO_EXECUTION = 2,  // Executing combo sequences
    BURST_WINDOW    = 3,  // Storm, Earth, and Fire burst
    MARK_SPREADING  = 4,  // Mark of the Crane management
    EXECUTE         = 5,  // Low health finishing
    EMERGENCY       = 6   // Critical situations
};

enum class ComboExecutionState : uint8
{
    BUILDING        = 0,  // Building chi for combo
    READY           = 1,  // Chi available for combo
    EXECUTING       = 2,  // Currently executing combo
    FINISHING       = 3,  // Finishing combo sequence
    RESETTING       = 4   // Resetting for next combo
};

enum class MarkOfCraneState : uint8
{
    INACTIVE        = 0,  // No marks applied
    SPREADING       = 1,  // Applying marks to targets
    MAINTAINED      = 2,  // Maintaining existing marks
    OPTIMIZING      = 3,  // Optimizing mark usage
    REFRESHING      = 4   // Refreshing expiring marks
};

struct WindwalkerTarget
{
    ObjectGuid targetGuid;
    bool hasMarkOfCrane;
    uint32 markOfCraneTimeRemaining;
    uint32 lastTigerPalmTime;
    uint32 lastRisingSunKickTime;
    uint32 lastBlackoutKickTime;
    float damageContribution;
    bool isOptimalForCombo;
    uint32 comboSequenceCount;
    bool isBurstTarget;

    WindwalkerTarget() : hasMarkOfCrane(false), markOfCraneTimeRemaining(0)
        , lastTigerPalmTime(0), lastRisingSunKickTime(0), lastBlackoutKickTime(0)
        , damageContribution(0.0f), isOptimalForCombo(false)
        , comboSequenceCount(0), isBurstTarget(false) {}
};

/**
 * @brief Enhanced Windwalker specialization with advanced combo mastery and burst coordination
 *
 * Focuses on sophisticated chi management, combo sequence optimization,
 * and intelligent Mark of the Crane spreading for maximum melee DPS efficiency.
 */
class TC_GAME_API WindwalkerSpecialization_Enhanced : public MonkSpecialization
{
public:
    explicit WindwalkerSpecialization_Enhanced(Player* bot);
    ~WindwalkerSpecialization_Enhanced() override = default;

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

    // Advanced chi mastery
    void ManageChiOptimally();
    void OptimizeChiGeneration();
    void HandleChiResourceEfficiency();
    void CoordinateChiSpending();
    void MaximizeChiUtilization();

    // Combo sequence mastery
    void ManageComboSequencesOptimally();
    void OptimizeComboExecution(::Unit* target);
    void HandleComboSequenceTiming();
    void CoordinateComboRotation();
    void MaximizeComboEfficiency();

    // Mark of the Crane optimization
    void ManageMarkOfCraneOptimally();
    void OptimizeMarkSpreading();
    void HandleMarkRefreshTiming();
    void CoordinateMarkUsage();
    void MaximizeMarkEfficiency();

    // Storm, Earth, and Fire mastery
    void ManageStormEarthFireOptimally();
    void OptimizeBurstWindowTiming();
    void HandleBurstSequenceExecution();
    void CoordinateBurstCooldowns();
    void MaximizeBurstDamage();

    // Touch of Death optimization
    void ManageTouchOfDeathOptimally();
    void OptimizeTouchOfDeathTiming();
    void HandleTouchOfDeathSetup();
    void CoordinateTouchOfDeathExecution();
    void MaximizeTouchOfDeathValue();

    // Performance analytics
    struct WindwalkerMetrics
    {
        std::atomic<uint32> tigerPalmCasts{0};
        std::atomic<uint32> blackoutKickCasts{0};
        std::atomic<uint32> risingSunKickCasts{0};
        std::atomic<uint32> fistsOfFuryCasts{0};
        std::atomic<uint32> whirlingDragonPunchCasts{0};
        std::atomic<uint32> touchOfDeathCasts{0};
        std::atomic<uint32> stormEarthFireActivations{0};
        std::atomic<uint32> markOfCraneApplications{0};
        std::atomic<float> chiEfficiency{0.9f};
        std::atomic<float> comboExecutionEfficiency{0.85f};
        std::atomic<float> markOfCraneUptime{0.8f};
        std::atomic<float> burstWindowEfficiency{0.95f};
        std::atomic<uint32> perfectComboSequences{0};
        std::atomic<uint32> touchOfDeathKills{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            tigerPalmCasts = 0; blackoutKickCasts = 0; risingSunKickCasts = 0;
            fistsOfFuryCasts = 0; whirlingDragonPunchCasts = 0; touchOfDeathCasts = 0;
            stormEarthFireActivations = 0; markOfCraneApplications = 0; chiEfficiency = 0.9f;
            comboExecutionEfficiency = 0.85f; markOfCraneUptime = 0.8f; burstWindowEfficiency = 0.95f;
            perfectComboSequences = 0; touchOfDeathKills = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    WindwalkerMetrics GetSpecializationMetrics() const { return _metrics; }

    // Advanced mobility optimization
    void ManageMobilityOptimally();
    void OptimizeMobilityForDPS();
    void HandlePositionalRequirements();
    void CoordinateMovementWithRotation();

    // Serenity vs Storm, Earth, and Fire optimization
    void OptimizeTalentSynergies();
    void HandleSerenityRotation();
    void CoordinateSerenityBurst();
    void MaximizeSerenityValue();

    // Whirling Dragon Punch optimization
    void ManageWhirlingDragonPunchOptimally();
    void OptimizeWhirlingDragonPunchTiming();
    void CoordinateWhirlingDragonPunchWithRotation();
    void MaximizeWhirlingDragonPunchDamage();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteChiGenerationPhase(::Unit* target);
    void ExecuteComboExecutionPhase(::Unit* target);
    void ExecuteBurstWindow(::Unit* target);
    void ExecuteMarkSpreadingPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastTigerPalm(::Unit* target) const;
    bool ShouldCastBlackoutKick(::Unit* target) const;
    bool ShouldCastRisingSunKick(::Unit* target) const;
    bool ShouldCastFistsOfFury(::Unit* target) const;
    bool ShouldCastWhirlingDragonPunch() const;

    // Advanced spell execution
    void ExecuteTigerPalm(::Unit* target);
    void ExecuteBlackoutKick(::Unit* target);
    void ExecuteRisingSunKick(::Unit* target);
    void ExecuteFistsOfFury(::Unit* target);
    void ExecuteWhirlingDragonPunch();

    // Cooldown management
    bool ShouldUseStormEarthAndFire() const;
    bool ShouldUseSerenity() const;
    bool ShouldUseTouchOfDeath(::Unit* target) const;
    bool ShouldUseInvokeXuen() const;

    // Advanced cooldown execution
    void ExecuteStormEarthAndFire();
    void ExecuteSerenity();
    void ExecuteTouchOfDeath(::Unit* target);
    void ExecuteInvokeXuen();

    // Chi management implementations
    void UpdateChiTracking();
    void OptimizeChiSpending();
    void HandleChiGeneration();
    void CalculateOptimalChiUsage();

    // Combo sequence implementations
    void UpdateComboTracking();
    void ExecuteOptimalComboSequence(::Unit* target);
    void HandleComboFinishers(::Unit* target);
    void OptimizeComboTiming();

    // Mark of the Crane implementations
    void UpdateMarkTracking();
    void ApplyMarkOfCrane(::Unit* target);
    void RefreshExpiredMarks();
    void OptimizeMarkTargeting();

    // Burst window implementations
    void PrepareBurstWindow();
    void ExecuteBurstRotation(::Unit* target);
    void OptimizeBurstDuration();
    bool IsInBurstWindow() const;

    // Touch of Death implementations
    void UpdateTouchOfDeathTracking();
    void SetupTouchOfDeath(::Unit* target);
    void ExecuteTouchOfDeathSequence(::Unit* target);
    bool CanExecuteTouchOfDeath(::Unit* target) const;

    // Target analysis for windwalker
    void AnalyzeTargetForWindwalker(::Unit* target);
    void AssessComboViability(::Unit* target);
    void PredictTargetLifetime(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // Multi-target optimization
    void HandleMultiTargetWindwalker();
    void OptimizeAoERotation();
    void CoordinateMarkSpreading();
    void ManageMultiTargetChi();

    // Position optimization
    void OptimizeWindwalkerPositioning(::Unit* target);
    void MaintainOptimalMeleeRange(::Unit* target);
    void HandlePositionalRequirements();
    void ExecuteMobilityOptimization();

    // Performance tracking
    void TrackWindwalkerPerformance();
    void AnalyzeChiEfficiency();
    void UpdateComboMetrics();
    void OptimizeBasedOnWindwalkerMetrics();

    // Emergency handling
    void HandleLowHealthWindwalkerEmergency();
    void HandleResourceEmergency();
    void ExecuteEmergencyBurst();
    void HandleInterruptEmergency();

    // State tracking
    WindwalkerPhase _currentPhase;
    ComboExecutionState _comboState;
    MarkOfCraneState _markState;

    // Target tracking
    std::unordered_map<ObjectGuid, WindwalkerTarget> _windwalkerTargets;
    ObjectGuid _primaryTarget;
    std::vector<ObjectGuid> _markedTargets;

    // Chi tracking
    uint32 _currentChi;
    uint32 _chiGenerated;
    uint32 _chiSpent;
    float _chiEfficiencyRatio;

    // Combo tracking
    uint32 _comboSequencesExecuted;
    uint32 _perfectCombos;
    uint32 _lastComboTime;
    bool _comboWindowActive;

    // Mark of the Crane tracking
    uint32 _markApplications;
    uint32 _markedTargetCount;
    uint32 _markRefreshTime;
    bool _markSpreadingActive;

    // Burst tracking
    uint32 _burstWindowStart;
    uint32 _burstWindowDuration;
    bool _stormEarthFireActive;
    bool _serenityActive;

    // Touch of Death tracking
    uint32 _lastTouchOfDeathTime;
    uint32 _touchOfDeathCooldown;
    bool _touchOfDeathReady;
    ObjectGuid _touchOfDeathTarget;

    // Whirling Dragon Punch tracking
    uint32 _lastWhirlingDragonPunchTime;
    bool _whirlingDragonPunchReady;
    uint32 _whirlingDragonPunchWindow;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalWindwalkerDamage;
    uint32 _totalChiGenerated;
    uint32 _totalChiSpent;
    float _averageWindwalkerDps;

    // Performance metrics
    WindwalkerMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _chiEfficiencyThreshold{0.85f};
    std::atomic<uint32> _optimalComboChiCost{3};
    std::atomic<uint32> _burstWindowOptimalDuration{15000};
    std::atomic<bool> _enableAdvancedComboOptimization{true};
    std::atomic<bool> _enableOptimalMarkSpreading{true};

    // Constants
    static constexpr uint32 MARK_OF_CRANE_DURATION = 15000; // 15 seconds
    static constexpr uint32 STORM_EARTH_FIRE_DURATION = 15000; // 15 seconds
    static constexpr uint32 SERENITY_DURATION = 12000; // 12 seconds
    static constexpr uint32 TOUCH_OF_DEATH_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 FISTS_OF_FURY_CHANNEL = 4000; // 4 seconds
    static constexpr uint32 WHIRLING_DRAGON_PUNCH_WINDOW = 3000; // 3 seconds
    static constexpr float MARK_REFRESH_THRESHOLD = 0.3f; // 30% duration
    static constexpr uint8 OPTIMAL_CHI_FOR_COMBO = 4;
    static constexpr uint8 MAX_CHI = 6;
    static constexpr uint32 COMBO_SEQUENCE_INTERVAL = 8000; // 8 seconds
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.15f; // 15% health
    static constexpr uint32 BURST_PREPARATION_TIME = 3000; // 3 seconds
    static constexpr float OPTIMAL_WINDWALKER_RANGE = 5.0f;
};

} // namespace Playerbot