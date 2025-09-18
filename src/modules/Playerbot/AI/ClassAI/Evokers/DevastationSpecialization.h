/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef DEVASTATION_SPECIALIZATION_H
#define DEVASTATION_SPECIALIZATION_H

#include "EvokerSpecialization.h"
#include <vector>
#include <unordered_set>

namespace Playerbot
{

enum class DevastationRotationPhase : uint8
{
    OPENER = 0,
    ESSENCE_GENERATION = 1,
    EMPOWERMENT_WINDOW = 2,
    BURNOUT_MANAGEMENT = 3,
    DRAGONRAGE_BURST = 4,
    SHATTERING_STAR_WINDOW = 5,
    DEEP_BREATH_SETUP = 6,
    AOE_PHASE = 7,
    EXECUTE_PHASE = 8,
    EMERGENCY = 9
};

enum class DevastationPriority : uint8
{
    EMERGENCY_DEFENSE = 0,
    DRAGONRAGE_BURST = 1,
    EMPOWERED_SPELLS = 2,
    SHATTERING_STAR = 3,
    ESSENCE_GENERATION = 4,
    BURNOUT_MANAGEMENT = 5,
    AOE_ABILITIES = 6,
    FILLER_SPELLS = 7,
    UTILITY = 8
};

struct BurnoutInfo
{
    uint8 stacks;
    uint32 timeRemaining;
    uint32 lastApplication;
    bool isActive;

    BurnoutInfo() : stacks(0), timeRemaining(0), lastApplication(0), isActive(false) {}
};

struct EssenceBurstInfo
{
    uint8 stacks;
    uint32 timeRemaining;
    uint32 lastProc;
    bool isActive;

    EssenceBurstInfo() : stacks(0), timeRemaining(0), lastProc(0), isActive(false) {}
};

struct DragonrageInfo
{
    bool isActive;
    uint32 remainingTime;
    uint32 lastActivation;
    uint32 abilitiesUsedDuringRage;
    uint32 totalDamageDealtDuringRage;

    DragonrageInfo() : isActive(false), remainingTime(0), lastActivation(0), abilitiesUsedDuringRage(0), totalDamageDealtDuringRage(0) {}
};

struct IridescenceInfo
{
    bool hasBlue;
    bool hasRed;
    uint32 blueTimeRemaining;
    uint32 redTimeRemaining;
    uint32 lastBlueProc;
    uint32 lastRedProc;

    IridescenceInfo() : hasBlue(false), hasRed(false), blueTimeRemaining(0), redTimeRemaining(0), lastBlueProc(0), lastRedProc(0) {}
};

struct DevastationMetrics
{
    uint32 azureStrikeCasts;
    uint32 livingFlameCasts;
    uint32 disintegrateCasts;
    uint32 pyreCasts;
    uint32 fireBreathCasts;
    uint32 eternitysSurgeCasts;
    uint32 shatteringStarCasts;
    uint32 dragonrageActivations;
    uint32 deepBreathCasts;
    uint32 empoweredSpellsCast;
    uint32 burnoutStacksGenerated;
    uint32 essenceBurstProcs;
    uint32 iridescenceProcs;
    float averageEmpowermentLevel;
    float burnoutUptime;
    float dragonrageUptime;
    float essenceBurstUptime;
    float averageDamagePerSecond;

    DevastationMetrics() : azureStrikeCasts(0), livingFlameCasts(0), disintegrateCasts(0), pyreCasts(0),
                          fireBreathCasts(0), eternitysSurgeCasts(0), shatteringStarCasts(0), dragonrageActivations(0),
                          deepBreathCasts(0), empoweredSpellsCast(0), burnoutStacksGenerated(0), essenceBurstProcs(0),
                          iridescenceProcs(0), averageEmpowermentLevel(0.0f), burnoutUptime(0.0f), dragonrageUptime(0.0f),
                          essenceBurstUptime(0.0f), averageDamagePerSecond(0.0f) {}
};

class DevastationSpecialization : public EvokerSpecialization
{
public:
    explicit DevastationSpecialization(Player* bot);
    ~DevastationSpecialization() override = default;

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

    // Essence Management Implementation
    void UpdateEssenceManagement() override;
    bool HasEssence(uint32 required = 1) override;
    uint32 GetEssence() override;
    void SpendEssence(uint32 amount) override;
    void GenerateEssence(uint32 amount) override;
    bool ShouldConserveEssence() override;

    // Empowerment Management Implementation
    void UpdateEmpowermentSystem() override;
    void StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target) override;
    void UpdateEmpoweredChanneling() override;
    void ReleaseEmpoweredSpell() override;
    EmpowermentLevel CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target) override;
    bool ShouldEmpowerSpell(uint32 spellId) override;

    // Aspect Management Implementation
    void UpdateAspectManagement() override;
    void ShiftToAspect(EvokerAspect aspect) override;
    EvokerAspect GetOptimalAspect() override;
    bool CanShiftAspect() override;

    // Combat Phase Management Implementation
    void UpdateCombatPhase() override;
    CombatPhase GetCurrentPhase() override;
    bool ShouldExecuteBurstRotation() override;

    // Target Selection Implementation
    ::Unit* GetBestTarget() override;
    std::vector<::Unit*> GetEmpoweredSpellTargets(uint32 spellId) override;

private:
    // Devastation-specific systems
    void UpdateBurnoutManagement();
    void UpdateEssenceBurstTracking();
    void UpdateDragonrageManagement();
    void UpdateIridescenceTracking();
    void UpdateShatteringStarWindow();
    void UpdateAoETargeting();

    // Rotation phases
    void ExecuteOpenerPhase(::Unit* target);
    void ExecuteEssenceGenerationPhase(::Unit* target);
    void ExecuteEmpowermentWindow(::Unit* target);
    void ExecuteBurnoutManagement(::Unit* target);
    void ExecuteDragonrageBurst(::Unit* target);
    void ExecuteShatteringStarWindow(::Unit* target);
    void ExecuteDeepBreathSetup(::Unit* target);
    void ExecuteAoEPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Core ability management
    void CastAzureStrike(::Unit* target);
    void CastLivingFlame(::Unit* target);
    void CastDisintegrate(::Unit* target);
    void CastPyre(::Unit* target);
    void CastFireBreath(::Unit* target);
    void CastEternitysSurge(::Unit* target);
    void CastShatteringStar(::Unit* target);
    void CastDeepBreath(::Unit* target);
    void CastFirestorm();

    // Empowered ability management
    void CastEmpoweredFireBreath(::Unit* target, EmpowermentLevel level);
    void CastEmpoweredEternitysSurge(::Unit* target, EmpowermentLevel level);
    void OptimizeEmpoweredSpellUsage(::Unit* target);
    bool ShouldUseEmpoweredFireBreath(::Unit* target);
    bool ShouldUseEmpoweredEternitysSurge(::Unit* target);

    // Dragonrage management
    void ActivateDragonrage();
    void ExecuteDragonrageRotation(::Unit* target);
    void OptimizeDragonrageWindow(::Unit* target);
    bool ShouldActivateDragonrage();
    uint32 GetDragonrageTimeRemaining();

    // Burnout management
    void ManageBurnoutStacks();
    void UpdateBurnoutStacks();
    bool ShouldAvoidBurnout();
    bool CanAffordBurnout();
    uint8 GetOptimalBurnoutStacks();

    // Essence Burst optimization
    void OptimizeEssenceBurstUsage();
    void HandleEssenceBurstProc();
    bool ShouldSpendEssenceBurst();
    uint32 GetEssenceBurstTimeRemaining();

    // Iridescence management
    void UpdateIridescenceBuffs();
    void OptimizeIridescenceUsage(::Unit* target);
    bool ShouldUseBlueIridescence(::Unit* target);
    bool ShouldUseRedIridescence(::Unit* target);

    // AoE and target selection
    void UpdateAoERotation(::Unit* target);
    std::vector<::Unit*> GetAoETargets(float range = 8.0f);
    bool ShouldUseAoERotation();
    uint32 CountNearbyEnemies(::Unit* target, float range = 8.0f);

    // Cooldown management
    void ManageMajorCooldowns();
    bool ShouldUseMajorCooldown(uint32 spellId);
    void CoordinateCooldowns();

    // Positioning optimization
    void OptimizeDevastationPositioning(::Unit* target);
    bool IsAtOptimalRange(::Unit* target);
    void MaintainCastingRange(::Unit* target);
    bool ShouldUseHover();

    // Priority system
    void UpdateAbilityPriorities();
    DevastationPriority GetCurrentPriority();
    bool ShouldPrioritizeEmpowerment();
    bool ShouldPrioritizeBurst();

    // Defensive abilities
    void HandleDefensiveSituations();
    void UseEmergencyAbilities();
    bool ShouldUseObsidianScales();
    bool ShouldUseRenewingBlaze();

    // Resource optimization
    void OptimizeEssenceSpending();
    void PlanEssenceUsage();
    bool ShouldWaitForEssence();
    uint32 GetEssenceNeededForRotation();

    // Metrics and analysis
    void UpdateDevastationMetrics();
    void AnalyzeRotationEfficiency();
    void LogDevastationDecision(const std::string& decision, const std::string& reason);

    // State variables
    DevastationRotationPhase _devastationPhase;
    BurnoutInfo _burnout;
    EssenceBurstInfo _essenceBurst;
    DragonrageInfo _dragonrage;
    IridescenceInfo _iridescence;
    DevastationMetrics _metrics;

    // Timing variables
    uint32 _lastAzureStrikeTime;
    uint32 _lastLivingFlameTime;
    uint32 _lastDisintegrateTime;
    uint32 _lastPyreTime;
    uint32 _lastFireBreathTime;
    uint32 _lastEternitysSurgeTime;
    uint32 _lastShatteringStarTime;
    uint32 _lastDragonrageTime;
    uint32 _lastDeepBreathTime;
    uint32 _lastFirestormTime;

    // Configuration constants
    static constexpr uint32 DRAGONRAGE_DURATION = 18000;           // 18 seconds
    static constexpr uint32 BURNOUT_DURATION = 15000;              // 15 seconds
    static constexpr uint32 ESSENCE_BURST_DURATION = 15000;        // 15 seconds
    static constexpr uint32 IRIDESCENCE_DURATION = 12000;          // 12 seconds
    static constexpr uint8 MAX_BURNOUT_STACKS = 5;                 // Maximum burnout stacks
    static constexpr uint8 SAFE_BURNOUT_STACKS = 3;               // Safe number of stacks
    static constexpr uint32 SHATTERING_STAR_WINDOW = 4000;        // 4 second window
    static constexpr float EMPOWERMENT_EFFICIENCY_THRESHOLD = 0.7f; // Minimum efficiency for empowerment
    static constexpr uint32 AOE_ENEMY_THRESHOLD = 3;               // Minimum enemies for AoE
    static constexpr float OPTIMAL_CASTING_RANGE = 25.0f;          // Optimal casting range
    static constexpr uint32 EMERGENCY_HEALTH_THRESHOLD = 30;       // Emergency abilities at 30% health
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.35f;       // Execute phase at 35% health

    // Ability priorities
    std::vector<uint32> _essenceGenerators;
    std::vector<uint32> _empoweredAbilities;
    std::vector<uint32> _burstAbilities;
    std::vector<uint32> _aoeAbilities;
    std::vector<uint32> _fillerAbilities;

    // Optimization settings
    bool _prioritizeEmpowerment;
    bool _conserveEssenceForBurst;
    bool _useAggressivePositioning;
    uint32 _preferredEmpowermentLevel;
    uint32 _preferredAoEThreshold;
};

} // namespace Playerbot

#endif // DEVASTATION_SPECIALIZATION_H