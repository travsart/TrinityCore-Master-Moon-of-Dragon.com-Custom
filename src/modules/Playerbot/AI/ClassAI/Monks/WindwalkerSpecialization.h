/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef WINDWALKER_SPECIALIZATION_H
#define WINDWALKER_SPECIALIZATION_H

#include "MonkSpecialization.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace Playerbot
{

enum class WindwalkerRotationPhase : uint8
{
    OPENING_SEQUENCE = 0,
    CHI_GENERATION = 1,
    COMBO_BUILDING = 2,
    COMBO_SPENDING = 3,
    BURST_WINDOW = 4,
    AOE_ROTATION = 5,
    EXECUTE_PHASE = 6,
    RESOURCE_RECOVERY = 7,
    EMERGENCY_SURVIVAL = 8
};

enum class ComboState : uint8
{
    BUILDING = 0,
    READY_TO_SPEND = 1,
    SPENDING = 2,
    EMPTY = 3
};

struct WindwalkerMetrics
{
    uint32 tigerPalmCasts;
    uint32 blackoutKickCasts;
    uint32 risingSunKickCasts;
    uint32 fistsOfFuryCasts;
    uint32 whirlingDragonPunchCasts;
    uint32 touchOfDeathCasts;
    uint32 stormEarthAndFireActivations;
    uint32 markOfTheCraneStacks;
    uint32 totalDamageDealt;
    uint32 comboPointsGenerated;
    uint32 comboPointsSpent;
    float chiEfficiency;
    float energyEfficiency;
    float comboUptime;
    float burstWindowUptime;
    float averageDamagePerSecond;

    WindwalkerMetrics() : tigerPalmCasts(0), blackoutKickCasts(0), risingSunKickCasts(0), fistsOfFuryCasts(0),
                         whirlingDragonPunchCasts(0), touchOfDeathCasts(0), stormEarthAndFireActivations(0),
                         markOfTheCraneStacks(0), totalDamageDealt(0), comboPointsGenerated(0), comboPointsSpent(0),
                         chiEfficiency(0.0f), energyEfficiency(0.0f), comboUptime(0.0f), burstWindowUptime(0.0f),
                         averageDamagePerSecond(0.0f) {}
};

class WindwalkerSpecialization : public MonkSpecialization
{
public:
    explicit WindwalkerSpecialization(Player* bot);
    ~WindwalkerSpecialization() override = default;

    // Core Interface Implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource Management Implementation
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning Implementation
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Target Selection Implementation
    ::Unit* GetBestTarget() override;

private:
    // Windwalker-specific systems
    void UpdateComboSystem();
    void UpdateBurstWindows();
    void UpdateMarkOfTheCrane();
    void UpdateTouchOfDeath();
    void UpdateMobility();

    // Rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteChiGeneration(::Unit* target);
    void ExecuteComboBuilding(::Unit* target);
    void ExecuteComboSpending(::Unit* target);
    void ExecuteBurstWindow(::Unit* target);
    void ExecuteAoERotation(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteResourceRecovery(::Unit* target);
    void ExecuteEmergencySurvival(::Unit* target);

    // Core combat abilities
    void CastTigerPalm(::Unit* target);
    void CastBlackoutKick(::Unit* target);
    void CastRisingSunKick(::Unit* target);
    void CastFistsOfFury(::Unit* target);
    void CastWhirlingDragonPunch();
    void CastSpinningCraneKick();

    // Special abilities
    void CastTouchOfDeath(::Unit* target);
    void CastStormEarthAndFire();
    void CastSerenity();
    void CastInvokeXuenTheWhiteTiger();

    // Chi generators
    void CastChiWave();
    void CastChiBurst();
    void CastExpelHarm();
    void CastEnergyingElixir();

    // Defensive abilities
    void CastTouchOfKarma(::Unit* target);
    void CastDiffuseMagic();
    void CastDampenHarm();

    // Combo point system
    void BuildComboPoints();
    void SpendComboPoints();
    uint32 GetComboPoints();
    uint32 GetMaxComboPoints();
    void AddComboPoints(uint32 amount);
    void ResetComboPoints();
    ComboState GetComboState();

    // Mark of the Crane management
    void ApplyMarkOfTheCrane(::Unit* target);
    void RefreshMarkOfTheCrane();
    uint32 GetMarkOfTheCraneStacks();
    std::vector<::Unit*> GetMarkedTargets();
    bool HasMarkOfTheCrane(::Unit* target);

    // Touch of Death optimization
    bool ShouldUseTouchOfDeath(::Unit* target);
    bool CanExecuteTouchOfDeath(::Unit* target);
    void PrepareForTouchOfDeath();

    // Burst window management
    void ActivateBurstWindow();
    void ManageBurstCooldowns();
    bool IsInBurstWindow();
    bool ShouldActivateBurst();
    void OptimizeBurstRotation(::Unit* target);

    // AoE optimization
    void HandleMultipleTargets();
    void OptimizeAoERotation();
    bool ShouldUseAoE();
    uint32 GetNearbyEnemyCount();
    void PrioritizeAoETargets();

    // Chi optimization
    void OptimizeChiUsage();
    void PlanChiSpending();
    bool ShouldConserveChi();
    void BalanceChiAndEnergy();
    float CalculateChiEfficiency();

    // Energy optimization
    void OptimizeEnergyUsage();
    void AvoidEnergyCapping();
    bool ShouldSpendEnergy();
    float CalculateEnergyEfficiency();

    // Positioning for melee DPS
    void OptimizeMeleePositioning();
    void MaintainOptimalMeleeRange(::Unit* target);
    bool IsAtOptimalMeleePosition(::Unit* target);
    void AvoidAoEDamage();

    // Target prioritization
    ::Unit* GetHighestPriorityTarget();
    ::Unit* GetBestExecuteTarget();
    ::Unit* GetBestAoETarget();
    std::vector<::Unit*> GetAoETargets();
    void PrioritizeTargets();

    // Mobility and positioning
    void UseMobilityAbilities();
    void EngageTarget(::Unit* target);
    void DisengageFromDanger();
    bool NeedsRepositioning();

    // Defensive optimization
    void HandleDefensiveCooldowns();
    void UseEmergencyDefensives();
    bool NeedsDefensiveCooldown();

    // Metrics and analysis
    void UpdateWindwalkerMetrics();
    void AnalyzeDamageEfficiency();
    void AnalyzeComboEfficiency();
    void LogWindwalkerDecision(const std::string& decision, const std::string& reason);

    // State variables
    WindwalkerRotationPhase _windwalkerPhase;
    ComboInfo _combo;
    WindwalkerMetrics _metrics;

    // Mark of the Crane tracking
    std::unordered_map<ObjectGuid, uint32> _markOfTheCraneTargets;
    uint32 _markOfTheCraneStacks;

    // Timing variables
    uint32 _lastTigerPalmTime;
    uint32 _lastBlackoutKickTime;
    uint32 _lastRisingSunKickTime;
    uint32 _lastFistsOfFuryTime;
    uint32 _lastWhirlingDragonPunchTime;
    uint32 _lastTouchOfDeathTime;
    uint32 _lastStormEarthAndFireTime;
    uint32 _lastSerenityTime;
    uint32 _lastBurstActivation;
    uint32 _lastComboCheck;

    // Burst window tracking
    bool _inBurstWindow;
    uint32 _burstWindowStart;
    uint32 _burstWindowDuration;

    // Configuration constants
    static constexpr uint32 MAX_COMBO_POINTS = 5;               // Maximum combo points
    static constexpr uint32 OPTIMAL_COMBO_POINTS = 4;          // Optimal spending threshold
    static constexpr uint32 MARK_OF_CRANE_DURATION = 15000;    // 15 seconds
    static constexpr uint32 BURST_WINDOW_DURATION = 30000;     // 30 seconds
    static constexpr float EXECUTE_THRESHOLD = 0.15f;          // 15% health for execute
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f;  // 30% health
    static constexpr uint32 AOE_THRESHOLD = 3;                 // 3+ enemies for AoE
    static constexpr float CHI_EFFICIENCY_TARGET = 0.85f;      // 85% chi efficiency
    static constexpr float ENERGY_EFFICIENCY_TARGET = 0.8f;    // 80% energy efficiency

    // Ability priorities
    std::vector<uint32> _chiGenerators;
    std::vector<uint32> _chiSpenders;
    std::vector<uint32> _aoeAbilities;
    std::vector<uint32> _burstAbilities;
    std::vector<uint32> _defensiveAbilities;

    // Optimization settings
    bool _prioritizeComboBuilding;
    bool _aggressiveBurstUsage;
    bool _conserveResourcesForBurst;
    uint32 _maxMarkTargets;
    float _comboEfficiencyTarget;
};

} // namespace Playerbot

#endif // WINDWALKER_SPECIALIZATION_H