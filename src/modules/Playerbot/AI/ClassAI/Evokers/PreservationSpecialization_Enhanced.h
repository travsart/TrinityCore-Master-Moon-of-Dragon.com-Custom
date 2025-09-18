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
#include <queue>

namespace Playerbot
{

enum class PreservationPhase : uint8
{
    ASSESSMENT      = 0,  // Initial healing assessment
    ECHO_MASTERY    = 1,  // Echo management and optimization
    EMPOWERED_HEAL  = 2,  // Empowered healing execution
    EMERGENCY_HEAL  = 3,  // Emergency healing response
    GROUP_HEALING   = 4,  // Group healing optimization
    TEMPORAL_MGMT   = 5,  // Temporal ability management
    SUSTAIN_HEAL    = 6,  // Sustained healing maintenance
    EMERGENCY       = 7   // Critical situations
};

enum class EchoManagementState : uint8
{
    INACTIVE        = 0,  // No echoes active
    SPREADING       = 1,  // Spreading echoes to targets
    OPTIMIZING      = 2,  // Optimizing echo placement
    MAINTAINING     = 3,  // Maintaining active echoes
    MAXIMIZING      = 4   // Maximizing echo efficiency
};

enum class HealingPriorityState : uint8
{
    STABLE          = 0,  // Group health stable
    MODERATE_DAMAGE = 1,  // Moderate group damage
    HIGH_DAMAGE     = 2,  // High group damage
    CRITICAL_HEAL   = 3,  // Critical healing needed
    EMERGENCY       = 4   // Emergency healing required
};

struct PreservationTarget
{
    ObjectGuid targetGuid;
    bool hasEcho;
    uint32 echoTimeRemaining;
    uint32 echoHealCount;
    uint32 lastHealTime;
    float healingPriority;
    bool isEmergencyTarget;
    uint32 healthDeficit;
    bool isOptimalForGroupHeal;
    float temporalAnomaly;

    PreservationTarget() : hasEcho(false), echoTimeRemaining(0)
        , echoHealCount(0), lastHealTime(0), healingPriority(0.0f)
        , isEmergencyTarget(false), healthDeficit(0)
        , isOptimalForGroupHeal(false), temporalAnomaly(0.0f) {}
};

/**
 * @brief Enhanced Preservation specialization with advanced echo mastery and temporal healing
 *
 * Focuses on sophisticated essence management, echo optimization,
 * and intelligent temporal ability usage for maximum healing efficiency.
 */
class TC_GAME_API PreservationSpecialization_Enhanced : public EvokerSpecialization
{
public:
    explicit PreservationSpecialization_Enhanced(Player* bot);
    ~PreservationSpecialization_Enhanced() override = default;

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
    void OptimizeEssenceForHealing();
    void HandleEssenceSpendingEfficiency();
    void CoordinateEssenceResources();
    void MaximizeEssenceUtilization();

    // Echo management mastery
    void ManageEchoesOptimally();
    void OptimizeEchoPlacement();
    void HandleEchoSpreading();
    void CoordinateEchoHealing();
    void MaximizeEchoEfficiency();

    // Empowered healing mastery
    void ManageEmpoweredHealingOptimally();
    void OptimizeEmpoweredSpellTiming();
    void HandleEmpoweredChanneling();
    void CoordinateEmpoweredHealing();
    void MaximizeEmpoweredEfficiency();

    // Temporal magic mastery
    void ManageTemporalMagicOptimally();
    void OptimizeTemporalAnomaly();
    void HandleTemporalCompression();
    void CoordinateTemporalAbilities();
    void MaximizeTemporalValue();

    // Group healing optimization
    void ManageGroupHealingOptimally();
    void OptimizeGroupHealTargeting();
    void HandleGroupHealPrioritization();
    void CoordinateGroupHealing();
    void MaximizeGroupHealEfficiency();

    // Performance analytics
    struct PreservationMetrics
    {
        std::atomic<uint32> emeraldBlossomCasts{0};
        std::atomic<uint32> verdantEmbraceCasts{0};
        std::atomic<uint32> dreamBreathCasts{0};
        std::atomic<uint32> spiritBloomCasts{0};
        std::atomic<uint32> temporalAnomalyCasts{0};
        std::atomic<uint32> renewingBlazeCasts{0};
        std::atomic<uint32> echoesCreated{0};
        std::atomic<uint32> echoHealsPerformed{0};
        std::atomic<uint32> dreamFlightActivations{0};
        std::atomic<uint32> reversionCasts{0};
        std::atomic<float> essenceEfficiency{0.9f};
        std::atomic<float> echoEfficiency{0.85f};
        std::atomic<float> healingEfficiency{0.9f};
        std::atomic<float> temporalOptimization{0.8f};
        std::atomic<float> groupHealEfficiency{0.75f};
        std::atomic<uint32> emergencyHealsUsed{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            emeraldBlossomCasts = 0; verdantEmbraceCasts = 0; dreamBreathCasts = 0;
            spiritBloomCasts = 0; temporalAnomalyCasts = 0; renewingBlazeCasts = 0;
            echoesCreated = 0; echoHealsPerformed = 0; dreamFlightActivations = 0;
            reversionCasts = 0; essenceEfficiency = 0.9f; echoEfficiency = 0.85f;
            healingEfficiency = 0.9f; temporalOptimization = 0.8f; groupHealEfficiency = 0.75f;
            emergencyHealsUsed = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    PreservationMetrics GetSpecializationMetrics() const { return _metrics; }

    // Dream Flight optimization
    void ManageDreamFlightOptimally();
    void OptimizeDreamFlightTiming();
    void HandleDreamFlightExecution();
    void CoordinateDreamFlightWithRotation();

    // Reversion and HoT management
    void ManageHoTsOptimally();
    void OptimizeReversionTiming();
    void HandleHoTRefreshing();
    void CoordinateHoTManagement();

    // Call of Ysera optimization
    void ManageCallOfYseraOptimally();
    void OptimizeCallOfYseraStacks();
    void HandleCallOfYseraProcs();
    void CoordinateCallOfYseraWithRotation();

private:
    // Enhanced rotation phases
    void ExecuteAssessmentPhase(::Unit* target);
    void ExecuteEchoMasteryPhase(::Unit* target);
    void ExecuteEmpoweredHealPhase(::Unit* target);
    void ExecuteEmergencyHealPhase(::Unit* target);
    void ExecuteGroupHealingPhase(::Unit* target);
    void ExecuteTemporalManagementPhase(::Unit* target);
    void ExecuteSustainHealPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Healing spell execution optimization
    bool ShouldCastEmeraldBlossom() const;
    bool ShouldCastVerdantEmbrace(::Unit* target) const;
    bool ShouldCastReversion(::Unit* target) const;
    bool ShouldCastRenewingBlaze(::Unit* target) const;
    bool ShouldCastLifebind(::Unit* target) const;

    // Advanced healing execution
    void ExecuteEmeraldBlossom();
    void ExecuteVerdantEmbrace(::Unit* target);
    void ExecuteReversion(::Unit* target);
    void ExecuteRenewingBlaze(::Unit* target);
    void ExecuteLifebind(::Unit* target);

    // Empowered healing management
    bool ShouldCastEmpoweredDreamBreath(::Unit* target) const;
    bool ShouldCastEmpoweredSpiritBloom(::Unit* target) const;
    void ExecuteEmpoweredDreamBreath(::Unit* target, EmpowermentLevel level);
    void ExecuteEmpoweredSpiritBloom(::Unit* target, EmpowermentLevel level);

    // Temporal ability management
    bool ShouldCastTemporalAnomaly() const;
    bool ShouldCastStasis(::Unit* target) const;
    bool ShouldCastTimeDilation(::Unit* target) const;
    void ExecuteTemporalAnomaly();
    void ExecuteStasis(::Unit* target);
    void ExecuteTimeDilation(::Unit* target);

    // Major cooldown management
    bool ShouldUseDreamFlight() const;
    bool ShouldUseFieldOfDreams() const;
    void ExecuteDreamFlight();
    void ExecuteFieldOfDreams();

    // Essence management implementations
    void UpdateEssenceTracking();
    void OptimizeEssenceSpending();
    void HandleEssenceGeneration();
    void CalculateOptimalEssenceUsage();

    // Echo management implementations
    void UpdateEchoTracking();
    void CreateOptimalEcho(::Unit* target);
    void ProcessEchoHealing();
    void OptimizeEchoDistribution();

    // Healing target selection
    ::Unit* GetBestHealTarget();
    ::Unit* GetMostCriticalTarget();
    ::Unit* GetBestGroupHealTarget();
    std::vector<::Unit*> GetHealingTargets(float healthThreshold = 0.8f);
    std::vector<::Unit*> GetGroupHealTargets(::Unit* center, float range = 30.0f);

    // Healing priority management
    void UpdateHealingPriorities();
    void AssessGroupHealthStatus();
    void PrioritizeHealingTargets();
    float CalculateHealingPriority(::Unit* target);

    // Empowerment implementations
    void UpdateEmpowermentTracking();
    void StartOptimalEmpowerment(uint32 spellId, ::Unit* target);
    void OptimizeEmpowermentLevel(uint32 spellId, ::Unit* target);
    void HandleEmpowermentRelease();

    // Temporal magic implementations
    void UpdateTemporalTracking();
    void OptimizeTemporalAbilities();
    void HandleTemporalCompression();
    void CoordinateTemporalEffects();

    // Group healing implementations
    void UpdateGroupHealingTracking();
    void OptimizeGroupHealRotation();
    void HandleGroupEmergencies();
    bool ShouldUseGroupHealing();

    // Target analysis for preservation
    void AnalyzeTargetForPreservation(::Unit* target);
    void AssessHealingRequirements(::Unit* target);
    void PredictDamageIncoming(::Unit* target);
    void OptimizeTargetHealing(::Unit* target);

    // Multi-target healing
    void HandleMultiTargetHealing();
    void OptimizeAoEHealing();
    void CoordinateMultiTargetEchoes();
    void ManageGroupWideHealing();

    // Position optimization
    void OptimizePreservationPositioning(::Unit* target);
    void MaintainOptimalHealingRange();
    void HandlePositionalRequirements();
    void ExecuteHealerPositioning();

    // Performance tracking
    void TrackPreservationPerformance();
    void AnalyzeHealingEfficiency();
    void UpdateEchoMetrics();
    void OptimizeBasedOnPreservationMetrics();

    // Emergency handling
    void HandleLowHealthPreservationEmergency();
    void HandleGroupEmergency();
    void ExecuteEmergencyHealing();
    void HandleCriticalHealingSituation();

    // State tracking
    PreservationPhase _currentPhase;
    EchoManagementState _echoState;
    HealingPriorityState _healingPriorityState;

    // Target tracking
    std::unordered_map<ObjectGuid, PreservationTarget> _preservationTargets;
    ObjectGuid _primaryHealTarget;
    std::vector<ObjectGuid> _criticalTargets;

    // Essence tracking
    uint32 _currentEssence;
    uint32 _essenceGenerated;
    uint32 _essenceSpent;
    float _essenceEfficiencyRatio;

    // Echo tracking
    uint32 _activeEchoes;
    uint32 _echoesCreated;
    uint32 _echoHealsPerformed;
    uint32 _lastEchoCreation;

    // Empowerment tracking
    uint32 _currentEmpowermentLevel;
    uint32 _empoweredHealsUsed;
    uint32 _perfectEmpowerments;
    uint32 _lastEmpowermentTime;

    // Temporal tracking
    uint32 _temporalCompressionStacks;
    uint32 _temporalCompressionTimeRemaining;
    uint32 _lastTemporalAnomalyTime;
    bool _temporalAnomalyActive;

    // Group healing tracking
    uint32 _groupHealthAssessment;
    uint32 _lastGroupHealTime;
    uint32 _criticalTargetCount;
    bool _groupEmergencyActive;

    // Call of Ysera tracking
    uint32 _callOfYseraStacks;
    uint32 _callOfYseraTimeRemaining;
    uint32 _lastCallOfYseraProc;
    bool _callOfYseraActive;

    // Dream Flight tracking
    uint32 _lastDreamFlightTime;
    bool _dreamFlightActive;
    uint32 _dreamFlightTimeRemaining;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalHealingDone;
    uint32 _totalOverhealing;
    uint32 _totalEchoHealing;
    float _averageHealingPerSecond;

    // Performance metrics
    PreservationMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _healingEfficiencyThreshold{0.85f};
    std::atomic<uint32> _optimalEchoCount{5};
    std::atomic<float> _criticalHealthThreshold{0.3f};
    std::atomic<bool> _enableAdvancedEchoManagement{true};
    std::atomic<bool> _enableOptimalGroupHealing{true};

    // Constants
    static constexpr uint32 MAX_ESSENCE = 5;
    static constexpr uint32 MAX_ECHOES = 8;
    static constexpr uint32 ECHO_DURATION = 30000; // 30 seconds
    static constexpr uint32 ECHO_HEAL_INTERVAL = 2000; // 2 seconds
    static constexpr uint32 TEMPORAL_ANOMALY_DURATION = 8000; // 8 seconds
    static constexpr uint32 TEMPORAL_COMPRESSION_DURATION = 10000; // 10 seconds
    static constexpr uint32 CALL_OF_YSERA_DURATION = 15000; // 15 seconds
    static constexpr uint32 DREAM_FLIGHT_DURATION = 6000; // 6 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr float CRITICAL_HEALTH_THRESHOLD = 0.4f; // 40% health
    static constexpr float GROUP_HEAL_THRESHOLD = 0.7f; // 70% health
    static constexpr uint32 GROUP_HEAL_COUNT_THRESHOLD = 3; // 3+ injured for group heals
    static constexpr float OPTIMAL_PRESERVATION_RANGE = 30.0f;
};

} // namespace Playerbot