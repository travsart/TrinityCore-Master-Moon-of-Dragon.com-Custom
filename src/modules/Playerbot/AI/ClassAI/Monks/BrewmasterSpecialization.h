/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef BREWMASTER_SPECIALIZATION_H
#define BREWMASTER_SPECIALIZATION_H

#include "MonkSpecialization.h"
#include <vector>
#include <unordered_set>

namespace Playerbot
{

enum class BrewmasterRotationPhase : uint8
{
    THREAT_ESTABLISHMENT = 0,
    STAGGER_MANAGEMENT = 1,
    BREW_OPTIMIZATION = 2,
    AOE_CONTROL = 3,
    DEFENSIVE_COOLDOWNS = 4,
    CHI_GENERATION = 5,
    DAMAGE_DEALING = 6,
    EMERGENCY_SURVIVAL = 7
};

struct BrewmasterMetrics
{
    uint32 kegSmashCasts;
    uint32 breathOfFireCasts;
    uint32 tigerPalmCasts;
    uint32 blackoutKickCasts;
    uint32 ironskinBrewUses;
    uint32 purifyingBrewUses;
    uint32 staggerDamageMitigated;
    uint32 totalThreatGenerated;
    float staggerUptime;
    float brewUtilization;
    float averageStaggerLevel;
    float defensiveCooldownUptime;

    BrewmasterMetrics() : kegSmashCasts(0), breathOfFireCasts(0), tigerPalmCasts(0), blackoutKickCasts(0),
                         ironskinBrewUses(0), purifyingBrewUses(0), staggerDamageMitigated(0),
                         totalThreatGenerated(0), staggerUptime(0.0f), brewUtilization(0.0f),
                         averageStaggerLevel(0.0f), defensiveCooldownUptime(0.0f) {}
};

class BrewmasterSpecialization : public MonkSpecialization
{
public:
    explicit BrewmasterSpecialization(Player* bot);
    ~BrewmasterSpecialization() override = default;

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
    // Brewmaster-specific systems
    void UpdateStaggerManagement();
    void UpdateBrewManagement();
    void UpdateThreatManagement();
    void UpdateDefensiveCooldowns();

    // Rotation phases
    void ExecuteThreatEstablishment(::Unit* target);
    void ExecuteStaggerManagement(::Unit* target);
    void ExecuteBrewOptimization(::Unit* target);
    void ExecuteAoEControl(::Unit* target);
    void ExecuteDefensiveCooldowns(::Unit* target);
    void ExecuteChiGeneration(::Unit* target);
    void ExecuteDamageDealing(::Unit* target);
    void ExecuteEmergencySurvival(::Unit* target);

    // Core abilities
    void CastKegSmash(::Unit* target);
    void CastBreathOfFire();
    void CastTigerPalm(::Unit* target);
    void CastBlackoutKick(::Unit* target);
    void CastSpinningCraneKick();

    // Brew abilities
    void CastIronskinBrew();
    void CastPurifyingBrew();
    void CastBlackOxBrew();
    void CastFortifyingBrew();

    // Defensive abilities
    void CastZenMeditation();
    void CastDampenHarm();
    void CastExpelHarm();
    void CastCelestialBrew();

    // Stagger management
    void ProcessStaggerDamage(uint32 incomingDamage);
    void ClearStagger();
    void UpdateStaggerLevel();
    bool ShouldUsePurifyingBrew();
    bool ShouldUseIronskinBrew();
    uint32 CalculateStaggerDamage(uint32 incomingDamage);

    // Brew management
    void RechargeBrews(uint32 diff);
    void OptimizeBrewUsage();
    bool HasBrewCharges();
    uint32 GetBrewCharges();
    void PrioritizeBrewUsage();

    // Threat management
    void BuildThreat(::Unit* target);
    void MaintainAggro();
    void TauntTarget(::Unit* target);
    bool HasSufficientThreat(::Unit* target);
    std::vector<::Unit*> GetThreatTargets();

    // Defensive optimization
    void UseEmergencyDefensives();
    void MaintainDefensiveBuffs();
    void PrioritizeDefensiveCooldowns();
    bool NeedsDefensiveCooldown();

    // AoE and cleave
    void HandleMultipleEnemies();
    void OptimizeAoERotation();
    bool ShouldUseAoE();
    uint32 GetNearbyEnemyCount();

    // Chi optimization for tanking
    void OptimizeChiForTanking();
    void PlanChiSpending();
    bool ShouldConserveChi();

    // Positioning for tanking
    void OptimizeTankPositioning();
    void FaceAwayFromGroup(::Unit* target);
    void MaintainTankPosition(::Unit* target);
    bool IsAtOptimalTankPosition(::Unit* target);

    // Target prioritization
    ::Unit* GetHighestThreatTarget();
    ::Unit* GetLowestThreatTarget();
    ::Unit* GetMostDangerousTarget();
    std::vector<::Unit*> GetUntaggedEnemies();

    // Metrics and analysis
    void UpdateBrewmasterMetrics();
    void AnalyzeTankingEfficiency();
    void LogBrewmasterDecision(const std::string& decision, const std::string& reason);

    // State variables
    BrewmasterRotationPhase _brewmasterPhase;
    StaggerInfo _stagger;
    BrewInfo _brews;
    BrewmasterMetrics _metrics;

    // Timing variables
    uint32 _lastKegSmashTime;
    uint32 _lastBreathOfFireTime;
    uint32 _lastTigerPalmTime;
    uint32 _lastBlackoutKickTime;
    uint32 _lastIronskinBrewTime;
    uint32 _lastPurifyingBrewTime;
    uint32 _lastStaggerUpdate;
    uint32 _lastBrewRecharge;
    uint32 _lastThreatCheck;
    uint32 _lastDefensiveCheck;

    // Configuration constants
    static constexpr uint32 STAGGER_CHECK_INTERVAL = 1000;       // 1 second
    static constexpr uint32 BREW_RECHARGE_TIME = 20000;         // 20 seconds
    static constexpr uint32 HEAVY_STAGGER_THRESHOLD = 1000;     // Damage threshold
    static constexpr uint32 MODERATE_STAGGER_THRESHOLD = 500;   // Damage threshold
    static constexpr float THREAT_CHECK_INTERVAL = 2000;        // 2 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f;   // 30% health
    static constexpr float LOW_HEALTH_THRESHOLD = 0.6f;         // 60% health
    static constexpr uint32 AOE_THRESHOLD = 3;                  // 3+ enemies for AoE
    static constexpr float BREW_EFFICIENCY_TARGET = 0.8f;       // 80% brew utilization

    // Ability priorities
    std::vector<uint32> _threatAbilities;
    std::vector<uint32> _defensiveAbilities;
    std::vector<uint32> _brewAbilities;
    std::vector<uint32> _aoeAbilities;

    // Optimization settings
    bool _prioritizeStaggerManagement;
    bool _aggressiveBrewUsage;
    bool _conserveChiForDefense;
    uint32 _maxStaggerTolerance;
    float _threatMargin;
};

} // namespace Playerbot

#endif // BREWMASTER_SPECIALIZATION_H