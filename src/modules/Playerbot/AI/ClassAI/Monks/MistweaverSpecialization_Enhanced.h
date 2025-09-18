/*
 * Copyright (C) 2024 TrinityCore <https://www.trinityCore.org/>
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
#include <queue>

namespace Playerbot
{

enum class MistweaverPhase : uint8
{
    ASSESSMENT      = 0,  // Analyzing healing needs
    EMERGENCY       = 1,  // Critical healing situations
    FISTWEAVING     = 2,  // Damage-to-healing conversion
    HOT_MANAGEMENT  = 3,  // HoT application and maintenance
    GROUP_HEALING   = 4,  // Multi-target healing
    MANA_RECOVERY   = 5,  // Mana conservation phase
    UTILITY         = 6   // Support and crowd control
};

enum class FistweavingState : uint8
{
    INACTIVE        = 0,  // Pure healing mode
    EVALUATING      = 1,  // Assessing fistweaving viability
    ACTIVE          = 2,  // Currently fistweaving
    TRANSITIONING   = 3,  // Switching between modes
    OPTIMIZING      = 4   // Maximizing fistweaving efficiency
};

enum class HealingPriorityState : uint8
{
    ROUTINE         = 0,  // Normal healing priorities
    EMERGENCY       = 1,  // Emergency healing needed
    TRIAGE          = 2,  // Multiple critical targets
    MAINTENANCE     = 3,  // Maintaining health levels
    PREPARATION     = 4   // Pre-damage healing
};

struct MistweaverTarget
{
    ObjectGuid targetGuid;
    float healthPercentage;
    uint32 missingHealth;
    bool hasRenewingMist;
    bool hasEnvelopingMist;
    bool hasSoothingMist;
    uint32 renewingMistTimeRemaining;
    uint32 envelopingMistTimeRemaining;
    uint32 soothingMistTimeRemaining;
    uint32 lastHealTime;
    float healingPriority;
    bool isInRange;
    bool requiresEmergencyHealing;

    MistweaverTarget() : healthPercentage(100.0f), missingHealth(0)
        , hasRenewingMist(false), hasEnvelopingMist(false), hasSoothingMist(false)
        , renewingMistTimeRemaining(0), envelopingMistTimeRemaining(0)
        , soothingMistTimeRemaining(0), lastHealTime(0), healingPriority(0.0f)
        , isInRange(false), requiresEmergencyHealing(false) {}
};

/**
 * @brief Enhanced Mistweaver specialization with advanced fistweaving and HoT mastery
 *
 * Focuses on sophisticated fistweaving optimization, intelligent HoT management,
 * and adaptive healing strategies for maximum healing efficiency and group support.
 */
class TC_GAME_API MistweaverSpecialization_Enhanced : public MonkSpecialization
{
public:
    explicit MistweaverSpecialization_Enhanced(Player* bot);
    ~MistweaverSpecialization_Enhanced() override = default;

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

    // Advanced fistweaving mastery
    void ManageFistweavingOptimally();
    void OptimizeFistweavingTiming();
    void HandleFistweavingTransitions();
    void CoordinateFistweavingRotation();
    void MaximizeFistweavingEfficiency();

    // Sophisticated HoT management
    void ManageHoTsIntelligently();
    void OptimizeRenewingMistPlacement();
    void HandleEnvelopingMistTiming();
    void CoordinateHoTRefreshes();
    void MaximizeHoTEfficiency();

    // Emergency healing mastery
    void ManageEmergencyHealingOptimally();
    void OptimizeEmergencyResponse();
    void HandleCriticalHealingSituations();
    void CoordinateEmergencyCooldowns();
    void MaximizeEmergencyEffectiveness();

    // Group healing optimization
    void ManageGroupHealingOptimally();
    void OptimizeEssenceFontUsage();
    void HandleAoEHealingPriorities();
    void CoordinateGroupHealingCooldowns();
    void MaximizeGroupHealingEfficiency();

    // Mana management mastery
    void ManageManaOptimally();
    void OptimizeManaEfficiency();
    void HandleManaConservation();
    void CoordinateManaRecovery();
    void MaximizeManaUtilization();

    // Performance analytics
    struct MistweaverMetrics
    {
        std::atomic<uint32> vivifyCasts{0};
        std::atomic<uint32> envelopingMistCasts{0};
        std::atomic<uint32> renewingMistCasts{0};
        std::atomic<uint32> essenceFontCasts{0};
        std::atomic<uint32> soothingMistChannels{0};
        std::atomic<uint32> fistweavingHealing{0};
        std::atomic<uint32> directHealing{0};
        std::atomic<uint32> hotHealing{0};
        std::atomic<float> healingEfficiency{0.9f};
        std::atomic<float> fistweavingUptime{0.3f};
        std::atomic<float> manaEfficiency{0.85f};
        std::atomic<float> averageGroupHealth{0.8f};
        std::atomic<uint32> emergencyHealsExecuted{0};
        std::atomic<uint32> livesaved{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            vivifyCasts = 0; envelopingMistCasts = 0; renewingMistCasts = 0;
            essenceFontCasts = 0; soothingMistChannels = 0; fistweavingHealing = 0;
            directHealing = 0; hotHealing = 0; healingEfficiency = 0.9f;
            fistweavingUptime = 0.3f; manaEfficiency = 0.85f; averageGroupHealth = 0.8f;
            emergencyHealsExecuted = 0; livesaved = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    MistweaverMetrics GetSpecializationMetrics() const { return _metrics; }

    // Thunder Focus Tea optimization
    void ManageThunderFocusTeaOptimally();
    void OptimizeThunderFocusTeaTiming();
    void CoordinateThunderFocusTeaEffects();
    void MaximizeThunderFocusTeaValue();

    // Soothing Mist channeling optimization
    void ManageSoothingMistOptimally();
    void OptimizeSoothingMistChanneling();
    void HandleSoothingMistInterruptions();
    void CoordinateSoothingMistHealing();

    // Advanced healing target selection
    void OptimizeHealingTargetSelection();
    void HandleHealingPrioritization();
    void ManageTriageSituations();
    void CoordinateHealingDistribution();

private:
    // Enhanced rotation phases
    void ExecuteAssessmentPhase();
    void ExecuteEmergencyPhase();
    void ExecuteFistweavingPhase(::Unit* target);
    void ExecuteHoTManagementPhase();
    void ExecuteGroupHealingPhase();
    void ExecuteManaRecoveryPhase();
    void ExecuteUtilityPhase();

    // Spell execution optimization
    bool ShouldCastVivify(::Unit* target) const;
    bool ShouldCastEnvelopingMist(::Unit* target) const;
    bool ShouldCastRenewingMist(::Unit* target) const;
    bool ShouldCastEssenceFont() const;
    bool ShouldChannelSoothingMist(::Unit* target) const;

    // Advanced spell execution
    void ExecuteVivify(::Unit* target);
    void ExecuteEnvelopingMist(::Unit* target);
    void ExecuteRenewingMist(::Unit* target);
    void ExecuteEssenceFont();
    void ExecuteSoothingMist(::Unit* target);

    // Cooldown management
    bool ShouldUseLifeCocoon(::Unit* target) const;
    bool ShouldUseRevival() const;
    bool ShouldUseThunderFocusTea() const;
    bool ShouldUseManaTea() const;

    // Advanced cooldown execution
    void ExecuteLifeCocoon(::Unit* target);
    void ExecuteRevival();
    void ExecuteThunderFocusTea();
    void ExecuteManaTea();

    // Fistweaving implementations
    void UpdateFistweavingTracking();
    void EvaluateFistweavingViability();
    void ExecuteFistweavingRotation(::Unit* target);
    void OptimizeFistweavingTargets();

    // HoT management implementations
    void UpdateHoTTracking();
    void RefreshExpiringHoTs();
    void OptimizeHoTPlacement();
    void CalculateHoTEfficiency();

    // Emergency healing implementations
    void DetectEmergencyTargets();
    void PrioritizeEmergencyHealing();
    void ExecuteEmergencyProtocols();
    void CoordinateEmergencyResponse();

    // Group healing implementations
    void AnalyzeGroupHealingNeeds();
    void OptimizeGroupHealingSpells();
    void CoordinateGroupHealingTiming();
    void MaximizeGroupHealingCoverage();

    // Target analysis for healing
    void AnalyzeHealingTargets();
    void AssessHealingPriorities();
    void PredictIncomingDamage();
    void OptimizeHealingAllocation();

    // Mana management implementations
    void UpdateManaTracking();
    void OptimizeManaSpending();
    void HandleManaEmergencies();
    void PredictManaNeeds();

    // Position optimization for healing
    void OptimizeMistweaverPositioning();
    void MaintainOptimalHealingRange();
    void HandlePositionForGroupHealing();
    void ExecuteHealerRepositioning();

    // Performance tracking
    void TrackMistweaverPerformance();
    void AnalyzeHealingEfficiency();
    void UpdateFistweavingMetrics();
    void OptimizeBasedOnHealingMetrics();

    // Emergency handling
    void HandleHealingEmergencies();
    void ExecuteLifesavingHealing();
    void HandleLowManaEmergency();
    void CoordinateCriticalHealing();

    // State tracking
    MistweaverPhase _currentPhase;
    FistweavingState _fistweavingState;
    HealingPriorityState _healingPriorityState;

    // Target tracking
    std::unordered_map<ObjectGuid, MistweaverTarget> _mistweaverTargets;
    std::priority_queue<ObjectGuid> _healingPriorityQueue;
    std::vector<ObjectGuid> _emergencyTargets;

    // HoT tracking
    uint32 _renewingMistTargets;
    uint32 _envelopingMistTargets;
    uint32 _soothingMistTarget;
    uint32 _hotRefreshWindow;

    // Fistweaving tracking
    uint32 _fistweavingStartTime;
    uint32 _fistweavingHealing;
    uint32 _fistweavingDamage;
    float _fistweavingEfficiency;
    bool _isFistweaving;

    // Mana tracking
    uint32 _currentMana;
    uint32 _manaSpentOnHealing;
    uint32 _manaRecovered;
    float _manaEfficiencyRatio;

    // Emergency tracking
    uint32 _emergencyHealingCount;
    uint32 _livesavedCount;
    uint32 _lastEmergencyTime;
    bool _inEmergencyMode;

    // Thunder Focus Tea tracking
    uint32 _thunderFocusTeaCharges;
    uint32 _lastThunderFocusTeaUse;
    bool _thunderFocusTeaActive;

    // Soothing Mist tracking
    uint32 _soothingMistChannelStart;
    uint32 _soothingMistChannelDuration;
    bool _isSoothingMistChanneling;
    ObjectGuid _soothingMistTarget;

    // Group healing tracking
    uint32 _lastGroupHealingTime;
    uint32 _injuredGroupMembers;
    float _averageGroupHealthPercentage;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalHealingDone;
    uint32 _totalOverhealing;
    uint32 _totalDamageContributed;
    float _averageHealingPerSecond;

    // Performance metrics
    MistweaverMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _emergencyHealthThreshold{0.25f};
    std::atomic<float> _fistweavingThreshold{0.8f};
    std::atomic<uint32> _optimalHoTTargets{5};
    std::atomic<bool> _enableAdvancedFistweaving{true};
    std::atomic<bool> _enableOptimalHoTManagement{true};

    // Constants
    static constexpr uint32 RENEWING_MIST_DURATION = 20000; // 20 seconds
    static constexpr uint32 ENVELOPING_MIST_DURATION = 18000; // 18 seconds
    static constexpr uint32 SOOTHING_MIST_CHANNEL_MAX = 8000; // 8 seconds
    static constexpr uint32 THUNDER_FOCUS_TEA_DURATION = 30000; // 30 seconds
    static constexpr uint32 LIFE_COCOON_DURATION = 12000; // 12 seconds
    static constexpr uint32 ESSENCE_FONT_COOLDOWN = 12000; // 12 seconds
    static constexpr float HOT_REFRESH_THRESHOLD = 0.3f; // 30% duration
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr float FISTWEAVING_EFFICIENCY_THRESHOLD = 0.7f; // 70% efficiency
    static constexpr uint32 MAX_RENEWING_MIST_TARGETS = 6;
    static constexpr uint32 GROUP_HEAL_THRESHOLD = 3;
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30% mana
};

} // namespace Playerbot