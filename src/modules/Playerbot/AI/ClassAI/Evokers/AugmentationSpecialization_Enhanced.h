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

enum class AugmentationPhase : uint8
{
    ASSESSMENT      = 0,  // Initial ally assessment
    BUFF_MASTERY    = 1,  // Buff application optimization
    EBON_MIGHT_MGMT = 2,  // Ebon Might management
    PRESCIENCE_DIST = 3,  // Prescience distribution
    BREATH_OF_EONS  = 4,  // Breath of Eons execution
    UTILITY_SUPPORT = 5,  // Utility and support
    DAMAGE_CONTRIB  = 6,  // Damage contribution
    EMERGENCY       = 7   // Critical situations
};

enum class BuffOptimizationState : uint8
{
    INACTIVE        = 0,  // No buffs being managed
    ASSESSING       = 1,  // Assessing buff needs
    APPLYING        = 2,  // Applying optimal buffs
    MAINTAINING     = 3,  // Maintaining active buffs
    MAXIMIZING      = 4   // Maximizing buff efficiency
};

enum class EbonMightState : uint8
{
    READY           = 0,  // Ready to cast
    ACTIVE          = 1,  // Currently active on targets
    SPREADING       = 2,  // Spreading to new targets
    REFRESHING      = 3,  // Refreshing expiring buffs
    OPTIMIZING      = 4   // Optimizing target selection
};

struct AugmentationTarget
{
    ObjectGuid targetGuid;
    bool hasEbonMight;
    uint32 ebonMightTimeRemaining;
    bool hasPrescience;
    uint32 prescienceTimeRemaining;
    bool hasBlisteryScales;
    uint32 blisteryScalesTimeRemaining;
    float damageContribution;
    bool isOptimalForBuffs;
    uint32 buffsApplied;
    float buffPriority;

    AugmentationTarget() : hasEbonMight(false), ebonMightTimeRemaining(0)
        , hasPrescience(false), prescienceTimeRemaining(0)
        , hasBlisteryScales(false), blisteryScalesTimeRemaining(0)
        , damageContribution(0.0f), isOptimalForBuffs(false)
        , buffsApplied(0), buffPriority(0.0f) {}
};

/**
 * @brief Enhanced Augmentation specialization with advanced buff optimization and ally empowerment
 *
 * Focuses on sophisticated essence management, buff distribution optimization,
 * and intelligent ally empowerment for maximum group DPS enhancement.
 */
class TC_GAME_API AugmentationSpecialization_Enhanced : public EvokerSpecialization
{
public:
    explicit AugmentationSpecialization_Enhanced(Player* bot);
    ~AugmentationSpecialization_Enhanced() override = default;

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
    void OptimizeEssenceForBuffs();
    void HandleEssenceSpendingEfficiency();
    void CoordinateEssenceResources();
    void MaximizeEssenceUtilization();

    // Buff optimization mastery
    void ManageBuffsOptimally();
    void OptimizeBuffDistribution();
    void HandleBuffPrioritization();
    void CoordinateBuffApplication();
    void MaximizeBuffEfficiency();

    // Ebon Might mastery
    void ManageEbonMightOptimally();
    void OptimizeEbonMightTargeting();
    void HandleEbonMightSpreading();
    void CoordinateEbonMightWithRotation();
    void MaximizeEbonMightValue();

    // Prescience optimization
    void ManagePrescienceOptimally();
    void OptimizePrescienceTargeting();
    void HandlePrescienceDistribution();
    void CoordinatePrescienceWithBurst();
    void MaximizePrescienceEfficiency();

    // Breath of Eons mastery
    void ManageBreathOfEonsOptimally();
    void OptimizeBreathOfEonsTiming();
    void HandleBreathOfEonsEmpowerment();
    void CoordinateBreathOfEonsWithRotation();
    void MaximizeBreathOfEonsImpact();

    // Performance analytics
    struct AugmentationMetrics
    {
        std::atomic<uint32> ebonMightApplications{0};
        std::atomic<uint32> prescienceApplications{0};
        std::atomic<uint32> breathOfEonsCasts{0};
        std::atomic<uint32> blisteryScalesApplications{0};
        std::atomic<uint32> totalBuffsApplied{0};
        std::atomic<uint32> eruptingLashCasts{0};
        std::atomic<uint32> upliftCasts{0};
        std::atomic<uint32> timeSpiralCasts{0};
        std::atomic<float> essenceEfficiency{0.9f};
        std::atomic<float> buffOptimization{0.85f};
        std::atomic<float> ebonMightUptime{0.9f};
        std::atomic<float> prescienceUptime{0.8f};
        std::atomic<float> damageAmplificationProvided{0.75f};
        std::atomic<uint32> allyEmpowerments{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            ebonMightApplications = 0; prescienceApplications = 0; breathOfEonsCasts = 0;
            blisteryScalesApplications = 0; totalBuffsApplied = 0; eruptingLashCasts = 0;
            upliftCasts = 0; timeSpiralCasts = 0; essenceEfficiency = 0.9f;
            buffOptimization = 0.85f; ebonMightUptime = 0.9f; prescienceUptime = 0.8f;
            damageAmplificationProvided = 0.75f; allyEmpowerments = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    AugmentationMetrics GetSpecializationMetrics() const { return _metrics; }

    // Advanced utility optimization
    void ManageUtilityOptimally();
    void OptimizeUtilityTiming();
    void HandleUtilityPrioritization();
    void CoordinateUtilityAbilities();

    // Time Spiral optimization
    void ManageTimeSpiralOptimally();
    void OptimizeTimeSpiralTiming();
    void HandleTimeSpiralExecution();
    void CoordinateTimeSpiralWithRotation();

    // Spatial Paradox optimization
    void ManageSpatialParadoxOptimally();
    void OptimizeSpatialParadoxTiming();
    void HandleSpatialParadoxExecution();
    void CoordinateSpatialParadoxWithRotation();

private:
    // Enhanced rotation phases
    void ExecuteAssessmentPhase(::Unit* target);
    void ExecuteBuffMasteryPhase(::Unit* target);
    void ExecuteEbonMightManagementPhase(::Unit* target);
    void ExecutePrescienceDistributionPhase(::Unit* target);
    void ExecuteBreathOfEonsPhase(::Unit* target);
    void ExecuteUtilitySupportPhase(::Unit* target);
    void ExecuteDamageContributionPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Buff spell execution optimization
    bool ShouldCastEbonMight(::Unit* target) const;
    bool ShouldCastPrescience(::Unit* target) const;
    bool ShouldCastBlisteryScales(::Unit* target) const;
    bool ShouldCastReactiveHide(::Unit* target) const;

    // Advanced buff execution
    void ExecuteEbonMight(::Unit* target);
    void ExecutePrescience(::Unit* target);
    void ExecuteBlisteryScales(::Unit* target);
    void ExecuteReactiveHide(::Unit* target);

    // Empowered ability management
    bool ShouldCastEmpoweredBreathOfEons(::Unit* target) const;
    void ExecuteEmpoweredBreathOfEons(::Unit* target, EmpowermentLevel level);

    // Damage ability management
    bool ShouldCastEruptingLash(::Unit* target) const;
    bool ShouldCastUplift(::Unit* target) const;
    void ExecuteEruptingLash(::Unit* target);
    void ExecuteUplift(::Unit* target);

    // Utility ability management
    bool ShouldCastTimeSpiral() const;
    bool ShouldCastSpatialParadox() const;
    void ExecuteTimeSpiral();
    void ExecuteSpatialParadox();

    // Essence management implementations
    void UpdateEssenceTracking();
    void OptimizeEssenceSpending();
    void HandleEssenceGeneration();
    void CalculateOptimalEssenceUsage();

    // Buff management implementations
    void UpdateBuffTracking();
    void OptimizeBuffRotation();
    void HandleBuffApplication();
    void PrioritizeBuffTargets();

    // Ebon Might implementations
    void UpdateEbonMightTracking();
    void OptimizeEbonMightTargets();
    void HandleEbonMightSpreading();
    void RefreshEbonMightBuffs();

    // Prescience implementations
    void UpdatePrescienceTracking();
    void OptimizePrescienceTargets();
    void HandlePrescienceApplication();
    void CoordinatePrescienceWithBurst();

    // Breath of Eons implementations
    void UpdateBreathOfEonsTracking();
    void OptimizeBreathOfEonsTiming();
    void HandleBreathOfEonsEmpowerment();
    void MaximizeBreathOfEonsTargets();

    // Target selection for augmentation
    ::Unit* GetBestEbonMightTarget();
    ::Unit* GetBestPrescienceTarget();
    ::Unit* GetBestBuffTarget();
    std::vector<::Unit*> GetAlliesForBuffing();
    std::vector<::Unit*> GetDamageDealer();

    // Buff optimization
    void OptimizeBuffPriorities();
    void AssessAllyBuffNeeds();
    float CalculateBuffValue(::Unit* target, uint32 spellId);
    bool ShouldReplaceExistingBuff(::Unit* target, uint32 spellId);

    // Empowerment implementations
    void UpdateEmpowermentTracking();
    void StartOptimalEmpowerment(uint32 spellId, ::Unit* target);
    void OptimizeEmpowermentLevel(uint32 spellId, ::Unit* target);
    void HandleEmpowermentRelease();

    // Utility implementations
    void UpdateUtilityTracking();
    void OptimizeUtilityUsage();
    void HandleUtilityTiming();
    void CoordinateUtilityEffects();

    // Target analysis for augmentation
    void AnalyzeTargetForAugmentation(::Unit* target);
    void AssessAllyEmpowermentNeeds(::Unit* target);
    void PredictAllyDamageWindows(::Unit* target);
    void OptimizeTargetBuffing(::Unit* target);

    // Multi-target buffing
    void HandleMultiTargetBuffing();
    void OptimizeGroupBuffDistribution();
    void CoordinateMultiTargetEmpowerment();
    void ManageGroupWideBuffs();

    // Position optimization
    void OptimizeAugmentationPositioning(::Unit* target);
    void MaintainOptimalSupportRange();
    void HandlePositionalRequirements();
    void ExecuteSupportPositioning();

    // Performance tracking
    void TrackAugmentationPerformance();
    void AnalyzeBuffEfficiency();
    void UpdateEmpowermentMetrics();
    void OptimizeBasedOnAugmentationMetrics();

    // Emergency handling
    void HandleLowHealthAugmentationEmergency();
    void HandleAllyEmergency();
    void ExecuteEmergencyBuffing();
    void HandleCriticalSupportSituation();

    // State tracking
    AugmentationPhase _currentPhase;
    BuffOptimizationState _buffState;
    EbonMightState _ebonMightState;

    // Target tracking
    std::unordered_map<ObjectGuid, AugmentationTarget> _augmentationTargets;
    ObjectGuid _primaryBuffTarget;
    std::vector<ObjectGuid> _allyTargets;

    // Essence tracking
    uint32 _currentEssence;
    uint32 _essenceGenerated;
    uint32 _essenceSpent;
    float _essenceEfficiencyRatio;

    // Buff tracking
    uint32 _activeBuffs;
    uint32 _buffsApplied;
    uint32 _buffsRefreshed;
    uint32 _lastBuffApplication;

    // Ebon Might tracking
    uint32 _ebonMightTargets;
    uint32 _ebonMightApplications;
    uint32 _lastEbonMightTime;
    bool _ebonMightOptimalTargeting;

    // Prescience tracking
    uint32 _prescienceTargets;
    uint32 _prescienceApplications;
    uint32 _lastPrescienceTime;
    bool _prescienceOptimalTargeting;

    // Breath of Eons tracking
    uint32 _lastBreathOfEonsTime;
    uint32 _breathOfEonsTargets;
    bool _breathOfEonsOptimalTiming;
    uint32 _breathOfEonsEmpowermentLevel;

    // Utility tracking
    uint32 _lastTimeSpiralTime;
    uint32 _lastSpatialParadoxTime;
    bool _utilityCooldownsActive;

    // Empowerment tracking
    uint32 _currentEmpowermentLevel;
    uint32 _empoweredAbilitiesUsed;
    uint32 _perfectEmpowerments;
    uint32 _lastEmpowermentTime;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalDamageContributed;
    uint32 _totalDamageAmplified;
    uint32 _totalBuffsProvided;
    float _averageAugmentationContribution;

    // Performance metrics
    AugmentationMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _buffEfficiencyThreshold{0.85f};
    std::atomic<uint32> _optimalEbonMightTargets{4};
    std::atomic<uint32> _optimalPrescienceTargets{2};
    std::atomic<bool> _enableAdvancedBuffOptimization{true};
    std::atomic<bool> _enableOptimalTargetSelection{true};

    // Constants
    static constexpr uint32 MAX_ESSENCE = 5;
    static constexpr uint32 EBON_MIGHT_DURATION = 30000; // 30 seconds
    static constexpr uint32 PRESCIENCE_DURATION = 18000; // 18 seconds
    static constexpr uint32 BLISTERY_SCALES_DURATION = 600000; // 10 minutes
    static constexpr uint32 MAX_EBON_MIGHT_TARGETS = 4;
    static constexpr uint32 MAX_PRESCIENCE_TARGETS = 2;
    static constexpr uint32 BREATH_OF_EONS_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 TIME_SPIRAL_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 SPATIAL_PARADOX_COOLDOWN = 120000; // 2 minutes
    static constexpr float BUFF_REFRESH_THRESHOLD = 0.3f; // 30% duration
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr float OPTIMAL_AUGMENTATION_RANGE = 30.0f;
};

} // namespace Playerbot