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

namespace Playerbot
{

enum class CombatPhase : uint8
{
    OPENING         = 0,  // Initial setup and buffs
    SLICE_SETUP     = 1,  // Slice and Dice establishment
    SUSTAIN_DPS     = 2,  // Sustained damage phase
    ADRENALINE_RUSH = 3,  // Adrenaline Rush burst window
    BLADE_FLURRY    = 4,  // AoE damage phase
    EXECUTE         = 5,  // Low health finishing
    EMERGENCY       = 6   // Critical situations
};

enum class AdrenalineRushState : uint8
{
    READY           = 0,  // Available for use
    PREPARING       = 1,  // Setting up for activation
    ACTIVE          = 2,  // Currently active
    EXTENDING       = 3,  // Maximizing active window
    COOLDOWN        = 4   // On cooldown
};

enum class BladeFlurryState : uint8
{
    INACTIVE        = 0,  // Not active
    EVALUATING      = 1,  // Checking for AoE opportunities
    ACTIVE          = 2,  // Currently active
    OPTIMIZING      = 3,  // Optimizing AoE rotation
    FINISHING       = 4   // Ending AoE phase
};

struct CombatTarget
{
    ObjectGuid targetGuid;
    bool hasSliceAndDice;
    bool hasExposeArmor;
    uint32 sliceAndDiceRemaining;
    uint32 exposeArmorRemaining;
    uint32 lastSinisterStrike;
    uint32 lastEviscerate;
    float weaponSpecBonus;
    bool isMainTarget;
    uint32 riposteOpportunities;

    CombatTarget() : hasSliceAndDice(false), hasExposeArmor(false)
        , sliceAndDiceRemaining(0), exposeArmorRemaining(0)
        , lastSinisterStrike(0), lastEviscerate(0), weaponSpecBonus(0.0f)
        , isMainTarget(false), riposteOpportunities(0) {}
};

/**
 * @brief Enhanced Combat specialization with advanced weapon mastery and burst coordination
 *
 * Focuses on sophisticated weapon specialization optimization, Adrenaline Rush mastery,
 * and intelligent multi-target combat coordination for sustained high DPS.
 */
class TC_GAME_API CombatSpecialization_Enhanced : public RogueSpecialization
{
public:
    explicit CombatSpecialization_Enhanced(Player* bot);
    ~CombatSpecialization_Enhanced() override = default;

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

    // Advanced weapon specialization mastery
    void ManageWeaponSpecializationOptimally();
    void OptimizeWeaponChoiceForTarget(::Unit* target);
    void HandleWeaponSpecializationProcs();
    void CoordinateWeaponSwapping();
    void MaximizeWeaponSpecializationBonuses();

    // Adrenaline Rush mastery
    void ManageAdrenalineRushOptimally();
    void OptimizeAdrenalineRushTiming();
    void ExecutePerfectAdrenalineRushBurst();
    void CoordinateAdrenalineRushCooldowns();
    void MaximizeAdrenalineRushEfficiency();

    // Blade Flurry and AoE mastery
    void ManageBladeFlurryIntelligently();
    void OptimizeAoETargeting();
    void HandleMultiTargetSituations();
    void CoordinateAoERotation();
    void MaximizeAoEDamageOutput();

    // Slice and Dice optimization
    void ManageSliceAndDiceOptimally();
    void OptimizeSliceAndDiceTiming();
    void HandleSliceAndDiceRefreshes();
    void CoordinateSliceAndDiceWithBurst();
    void MaximizeSliceAndDiceUptime();

    // Riposte and defensive mastery
    void ManageRiposteOptimally();
    void OptimizeDefensiveAbilities();
    void HandleParryAndDodgeProcs();
    void CoordinateDefensiveCooldowns();
    void MaximizeCounterAttackDamage();

    // Performance analytics
    struct CombatMetrics
    {
        std::atomic<uint32> sinisterStrikeCasts{0};
        std::atomic<uint32> eviscerateCasts{0};
        std::atomic<uint32> sliceAndDiceApplications{0};
        std::atomic<uint32> exposeArmorApplications{0};
        std::atomic<uint32> adrenalineRushActivations{0};
        std::atomic<uint32> bladeFlurryActivations{0};
        std::atomic<uint32> riposteExecutions{0};
        std::atomic<float> sliceAndDiceUptime{0.95f};
        std::atomic<float> adrenalineRushEfficiency{0.9f};
        std::atomic<float> weaponSpecializationProcs{0.15f};
        std::atomic<float> bladeFlurryEfficiency{0.8f};
        std::atomic<uint32> multiTargetKills{0};
        std::atomic<uint32> perfectRipostes{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            sinisterStrikeCasts = 0; eviscerateCasts = 0; sliceAndDiceApplications = 0;
            exposeArmorApplications = 0; adrenalineRushActivations = 0; bladeFlurryActivations = 0;
            riposteExecutions = 0; sliceAndDiceUptime = 0.95f; adrenalineRushEfficiency = 0.9f;
            weaponSpecializationProcs = 0.15f; bladeFlurryEfficiency = 0.8f;
            multiTargetKills = 0; perfectRipostes = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    CombatMetrics GetSpecializationMetrics() const { return _metrics; }

    // Advanced combo point efficiency
    void OptimizeComboPointGeneration();
    void HandleComboPointEfficientSpending();
    void CoordinateComboPointsWithBurst();
    void MaximizeComboPointValue();

    // Expose Armor coordination
    void ManageExposeArmorOptimally();
    void OptimizeExposeArmorTiming();
    void HandleExposeArmorInGroups();
    void CoordinateArmorReduction();

    // Energy management for combat
    void OptimizeEnergyForCombat();
    void HandleEnergyEfficientRotation();
    void PredictEnergyNeeds();
    void BalanceEnergyAndDamage();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteSliceSetupPhase(::Unit* target);
    void ExecuteSustainDPSPhase(::Unit* target);
    void ExecuteAdrenalineRushPhase(::Unit* target);
    void ExecuteBladeFlurryPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastSinisterStrike(::Unit* target) const;
    bool ShouldCastEviscerate(::Unit* target) const;
    bool ShouldCastSliceAndDice() const;
    bool ShouldCastExposeArmor(::Unit* target) const;
    bool ShouldCastRiposte(::Unit* target) const;

    // Advanced spell execution
    void ExecuteSinisterStrike(::Unit* target);
    void ExecuteEviscerate(::Unit* target);
    void ExecuteSliceAndDice();
    void ExecuteExposeArmor(::Unit* target);
    void ExecuteRiposte(::Unit* target);

    // Cooldown management
    bool ShouldUseAdrenalineRush() const;
    bool ShouldUseBladeFlurry() const;
    bool ShouldUseEvasion() const;
    bool ShouldUseSprint() const;

    // Advanced cooldown execution
    void ExecuteAdrenalineRush();
    void ExecuteBladeFlurry();
    void ExecuteEvasion();
    void ExecuteSprint();

    // Weapon specialization implementations
    void UpdateWeaponSpecializationTracking();
    void HandleSwordSpecializationProcs();
    void HandleMaceSpecializationProcs();
    void HandleDaggerSpecializationBonuses();
    void HandleFistWeaponSpecializationBonuses();

    // Adrenaline Rush implementations
    void PrepareAdrenalineRushWindow();
    void ExecuteAdrenalineRushRotation(::Unit* target);
    void OptimizeAdrenalineRushDuration();
    bool IsInAdrenalineRushWindow() const;

    // Blade Flurry implementations
    void EvaluateBladeFlurryOpportunity();
    void ExecuteBladeFlurryRotation(::Unit* target);
    void HandleBladeFlurryTargeting();
    uint32 CountBladeFlurryTargets() const;

    // Slice and Dice implementations
    void UpdateSliceAndDiceTracking();
    void RefreshSliceAndDice();
    uint32 GetSliceAndDiceTimeRemaining() const;
    uint8 GetOptimalSliceAndDiceComboPoints() const;

    // Riposte implementations
    void UpdateParryTracking();
    void HandleRiposteOpportunity(::Unit* target);
    void ExecutePerfectRiposte(::Unit* target);
    bool CanExecuteRiposte() const;

    // Expose Armor implementations
    void UpdateExposeArmorTracking();
    void RefreshExposeArmor(::Unit* target);
    bool ShouldRefreshExposeArmor(::Unit* target) const;
    uint32 GetExposeArmorTimeRemaining(::Unit* target) const;

    // Target analysis for combat
    void AnalyzeTargetForCombat(::Unit* target);
    void AssessWeaponEffectiveness(::Unit* target);
    void PredictCombatDuration(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // Multi-target coordination
    void HandleMultiTargetCombat();
    void PrioritizeMultiTargetAbilities();
    void CoordinateAoEDamage();
    void OptimizeTargetSwitching();

    // Position optimization
    void OptimizeCombatPositioning(::Unit* target);
    void MaintainOptimalMeleeRange(::Unit* target);
    void HandlePositionalRequirements();
    void ExecuteTacticalMovement();

    // Performance tracking
    void TrackCombatPerformance();
    void AnalyzeWeaponSpecializationEfficiency();
    void UpdateBurstWindowMetrics();
    void OptimizeBasedOnCombatMetrics();

    // Emergency handling
    void HandleLowHealthCombatEmergency();
    void HandleMultipleAttackersEmergency();
    void ExecuteEmergencyEvasion();
    void HandleCombatInterrupts();

    // State tracking
    CombatPhase _currentPhase;
    AdrenalineRushState _adrenalineRushState;
    BladeFlurryState _bladeFlurryState;

    // Target tracking
    std::unordered_map<ObjectGuid, CombatTarget> _combatTargets;
    ObjectGuid _primaryTarget;
    std::vector<ObjectGuid> _aoeTargets;

    // Buff tracking
    uint32 _sliceAndDiceTimeRemaining;
    uint32 _lastSliceAndDiceApplication;
    bool _sliceAndDiceActive;

    // Adrenaline Rush tracking
    uint32 _adrenalineRushStartTime;
    uint32 _adrenalineRushDuration;
    bool _adrenalineRushActive;
    uint32 _adrenalineRushCooldown;

    // Blade Flurry tracking
    uint32 _bladeFlurryStartTime;
    uint32 _bladeFlurryDuration;
    bool _bladeFlurryActive;
    uint32 _bladeFlurryTargets;

    // Weapon specialization data
    uint32 _mainHandWeaponType;
    uint32 _offHandWeaponType;
    float _swordSpecializationBonus;
    float _maceSpecializationBonus;
    float _daggerSpecializationBonus;
    float _fistWeaponSpecializationBonus;

    // Combo point optimization
    uint32 _lastSinisterStrikeTime;
    uint32 _lastEviscerateeTime;
    uint32 _comboPointsForSliceAndDice;
    uint32 _comboPointsForEviscerate;

    // Riposte tracking
    uint32 _lastParryTime;
    uint32 _lastRiposteTime;
    uint32 _riposteOpportunities;
    bool _canRiposte;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalCombatDamage;
    uint32 _totalWeaponSpecDamage;
    uint32 _totalBurstDamage;
    float _averageCombatDps;

    // Performance metrics
    CombatMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _sliceAndDiceRefreshThreshold{0.3f};
    std::atomic<float> _exposeArmorRefreshThreshold{0.2f};
    std::atomic<uint32> _adrenalineRushOptimalDuration{15000};
    std::atomic<bool> _enableAdvancedWeaponSpec{true};
    std::atomic<bool> _enableOptimalBurstTiming{true};

    // Constants
    static constexpr uint32 SLICE_AND_DICE_DURATION = 30000; // 30 seconds
    static constexpr uint32 EXPOSE_ARMOR_DURATION = 30000; // 30 seconds
    static constexpr uint32 ADRENALINE_RUSH_DURATION = 15000; // 15 seconds
    static constexpr uint32 BLADE_FLURRY_DURATION = 15000; // 15 seconds
    static constexpr uint32 RIPOSTE_WINDOW = 5000; // 5 seconds
    static constexpr uint32 EVASION_DURATION = 15000; // 15 seconds
    static constexpr float BURST_PREPARATION_THRESHOLD = 0.8f; // 80% energy
    static constexpr uint8 OPTIMAL_SLICE_AND_DICE_COMBO = 2;
    static constexpr uint8 OPTIMAL_EVISCERATE_COMBO = 5;
    static constexpr uint32 AOE_TARGET_THRESHOLD = 3;
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.20f; // 20% health
    static constexpr uint32 WEAPON_SPEC_PROC_WINDOW = 6000; // 6 seconds
    static constexpr float OPTIMAL_COMBAT_RANGE = 5.0f;
};

} // namespace Playerbot