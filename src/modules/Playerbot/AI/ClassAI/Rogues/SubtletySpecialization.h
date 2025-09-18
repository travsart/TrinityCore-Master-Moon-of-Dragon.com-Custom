/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef SUBTLETY_SPECIALIZATION_H
#define SUBTLETY_SPECIALIZATION_H

#include "RogueSpecialization.h"
#include <vector>
#include <unordered_set>
#include <queue>

namespace Playerbot
{

enum class SubtletyRotationPhase : uint8
{
    STEALTH_PREPARATION = 0,
    STEALTH_OPENER = 1,
    SHADOW_DANCE_BURST = 2,
    HEMORRHAGE_APPLICATION = 3,
    COMBO_BUILDING = 4,
    COMBO_SPENDING = 5,
    STEALTH_RESET = 6,
    SHADOWSTEP_POSITIONING = 7,
    DEFENSIVE_STEALTH = 8,
    EXECUTE_PHASE = 9,
    EMERGENCY = 10
};

enum class SubtletyPriority : uint8
{
    EMERGENCY_STEALTH = 0,
    STEALTH_OPENER = 1,
    SHADOW_DANCE_BURST = 2,
    SHADOWSTEP_POSITIONING = 3,
    HEMORRHAGE_REFRESH = 4,
    COMBO_SPEND = 5,
    COMBO_BUILD = 6,
    STEALTH_RESET = 7,
    DEFENSIVE_ABILITIES = 8,
    MOVEMENT = 9
};

struct StealthWindow
{
    uint32 startTime;
    uint32 duration;
    uint32 abilitiesUsed;
    uint32 damageDealt;
    bool fromVanish;
    bool fromShadowDance;
    bool fromPreparation;

    StealthWindow() : startTime(0), duration(0), abilitiesUsed(0), damageDealt(0),
                     fromVanish(false), fromShadowDance(false), fromPreparation(false) {}
};

struct ShadowDanceInfo
{
    bool isActive;
    uint32 remainingTime;
    uint32 lastActivation;
    uint32 abilitiesUsedDuringDance;
    uint32 stealthOpenersDuringDance;
    uint32 totalDamageDealtDuringDance;

    ShadowDanceInfo() : isActive(false), remainingTime(0), lastActivation(0),
                       abilitiesUsedDuringDance(0), stealthOpenersDuringDance(0), totalDamageDealtDuringDance(0) {}
};

struct ShadowstepInfo
{
    uint32 lastUsed;
    uint32 totalUses;
    uint32 successfulPositions;
    bool isOnCooldown;

    ShadowstepInfo() : lastUsed(0), totalUses(0), successfulPositions(0), isOnCooldown(false) {}
};

struct PreparationInfo
{
    uint32 lastUsed;
    uint32 totalUses;
    uint32 cooldownsReset;
    bool hasResetVanish;
    bool hasResetShadowstep;

    PreparationInfo() : lastUsed(0), totalUses(0), cooldownsReset(0), hasResetVanish(false), hasResetShadowstep(false) {}
};

struct HemorrhageInfo
{
    bool isActive;
    uint32 stacks;
    uint32 timeRemaining;
    uint32 lastApplication;
    uint32 totalApplications;
    uint32 totalDamage;

    HemorrhageInfo() : isActive(false), stacks(0), timeRemaining(0), lastApplication(0), totalApplications(0), totalDamage(0) {}
};

struct SubtletyMetrics
{
    uint32 ambushCasts;
    uint32 backstabCasts;
    uint32 hemorrhageCasts;
    uint32 eviscerateCasts;
    uint32 shadowstepUses;
    uint32 vanishUses;
    uint32 shadowDanceActivations;
    uint32 preparationUses;
    uint32 stealthOpeners;
    uint32 totalStealthTime;
    uint32 totalShadowDanceTime;
    float stealthUptime;
    float shadowDanceUptime;
    float hemorrhageUptime;
    float averageStealthWindowDuration;
    float averageDamagePerStealthWindow;
    float positionalAdvantagePercentage;
    uint32 masterOfSubtletyProcs;
    uint32 opportunityProcs;

    SubtletyMetrics() : ambushCasts(0), backstabCasts(0), hemorrhageCasts(0), eviscerateCasts(0),
                       shadowstepUses(0), vanishUses(0), shadowDanceActivations(0), preparationUses(0),
                       stealthOpeners(0), totalStealthTime(0), totalShadowDanceTime(0), stealthUptime(0.0f),
                       shadowDanceUptime(0.0f), hemorrhageUptime(0.0f), averageStealthWindowDuration(0.0f),
                       averageDamagePerStealthWindow(0.0f), positionalAdvantagePercentage(0.0f),
                       masterOfSubtletyProcs(0), opportunityProcs(0) {}
};

class SubtletySpecialization : public RogueSpecialization
{
public:
    explicit SubtletySpecialization(Player* bot);
    ~SubtletySpecialization() override = default;

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

    // Stealth Management Implementation (Core for Subtlety)
    void UpdateStealthManagement() override;
    bool ShouldEnterStealth() override;
    bool CanBreakStealth() override;
    void ExecuteStealthOpener(::Unit* target) override;

    // Combo Point Management Implementation
    void UpdateComboPointManagement() override;
    bool ShouldBuildComboPoints() override;
    bool ShouldSpendComboPoints() override;
    void ExecuteComboBuilder(::Unit* target) override;
    void ExecuteComboSpender(::Unit* target) override;

    // Poison Management Implementation (Minimal for Subtlety)
    void UpdatePoisonManagement() override;
    void ApplyPoisons() override;
    PoisonType GetOptimalMainHandPoison() override;
    PoisonType GetOptimalOffHandPoison() override;

    // Debuff Management Implementation
    void UpdateDebuffManagement() override;
    bool ShouldRefreshDebuff(uint32 spellId) override;
    void ApplyDebuffs(::Unit* target) override;

    // Energy Management Implementation
    void UpdateEnergyManagement() override;
    bool HasEnoughEnergyFor(uint32 spellId) override;
    uint32 GetEnergyCost(uint32 spellId) override;
    bool ShouldWaitForEnergy() override;

    // Cooldown Management Implementation
    void UpdateCooldownTracking(uint32 diff) override;
    bool IsSpellReady(uint32 spellId) override;
    void StartCooldown(uint32 spellId) override;
    uint32 GetCooldownRemaining(uint32 spellId) override;

    // Combat Phase Management Implementation
    void UpdateCombatPhase() override;
    CombatPhase GetCurrentPhase() override;
    bool ShouldExecuteBurstRotation() override;

private:
    // Subtlety-specific systems
    void UpdateShadowDanceManagement();
    void UpdateShadowstepManagement();
    void UpdatePreparationManagement();
    void UpdateHemorrhageManagement();
    void UpdateStealthWindows();
    void UpdateMasterOfSubtletyBuff();
    void UpdateOpportunityTracking();

    // Rotation phases
    void ExecuteStealthPreparation(::Unit* target);
    void ExecuteStealthOpenerPhase(::Unit* target);
    void ExecuteShadowDanceBurst(::Unit* target);
    void ExecuteHemorrhageApplication(::Unit* target);
    void ExecuteComboBuildingPhase(::Unit* target);
    void ExecuteComboSpendingPhase(::Unit* target);
    void ExecuteStealthReset(::Unit* target);
    void ExecuteShadowstepPositioning(::Unit* target);
    void ExecuteDefensiveStealth(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Stealth management
    void EnterStealth();
    void ActivateVanish();
    void ActivateShadowDance();
    void UsePreparation();
    void PlanStealthWindow(::Unit* target);
    void ExecuteStealthWindow(::Unit* target);
    void OptimizeStealthUsage();
    bool ShouldUseVanish();
    bool ShouldUseShadowDance();
    bool ShouldUsePreparation();

    // Shadowstep management
    void ExecuteShadowstep(::Unit* target);
    void OptimizeShadowstepPositioning(::Unit* target);
    bool ShouldUseShadowstep(::Unit* target);
    bool CanShadowstepToTarget(::Unit* target);
    Position GetShadowstepPosition(::Unit* target);

    // Stealth openers
    void ExecuteAmbushOpener(::Unit* target);
    void ExecuteGarroteOpener(::Unit* target);
    void ExecuteCheapShotOpener(::Unit* target);
    void ExecutePremeditationOpener(::Unit* target);
    bool ShouldUseAmbushOpener(::Unit* target);
    bool ShouldUseGarroteOpener(::Unit* target);
    bool ShouldUseCheapShotOpener(::Unit* target);
    bool ShouldUsePremeditationOpener(::Unit* target);

    // Hemorrhage management
    void ApplyHemorrhage(::Unit* target);
    void RefreshHemorrhage(::Unit* target);
    bool ShouldApplyHemorrhage(::Unit* target);
    bool ShouldRefreshHemorrhage(::Unit* target);
    uint32 GetHemorrhageTimeRemaining(::Unit* target);
    uint8 GetHemorrhageStacks(::Unit* target);

    // Combo point optimization
    void OptimizeComboRotation(::Unit* target);
    bool ShouldUseBackstab(::Unit* target);
    bool ShouldUseHemorrhage(::Unit* target);
    bool ShouldUseEviscerate(::Unit* target);
    bool ShouldUseRupture(::Unit* target);
    bool ShouldUseSliceAndDice();

    // Shadow Dance burst
    void InitiateShadowDanceBurst();
    void ExecuteShadowDanceRotation(::Unit* target);
    void OptimizeShadowDanceWindow(::Unit* target);
    uint32 GetShadowDanceTimeRemaining();

    // Positional advantage
    void OptimizePositionalAdvantage(::Unit* target);
    void MaintainBehindTarget(::Unit* target);
    bool IsInOptimalPosition(::Unit* target);
    bool ShouldRepositionForAdvantage(::Unit* target);

    // Defensive abilities
    void HandleStealthDefense();
    void ExecuteCloak();
    void ExecuteEvasion();
    void ExecuteBlind(::Unit* target);
    void ExecuteSap(::Unit* target);
    bool ShouldUseDefensiveStealth();
    bool ShouldUseCloak();
    bool ShouldUseCrowdControl();

    // Energy optimization
    void OptimizeEnergyForStealth();
    void PlanEnergyForStealthWindow();
    bool ShouldSaveEnergyForStealth();
    uint32 GetEnergyNeededForStealthRotation();

    // Cooldown coordination
    void CoordinateCooldowns();
    void PlanCooldownUsage();
    bool ShouldSaveCooldownForBurst();
    uint32 GetNextBurstWindowTime();

    // Stealth window analysis
    void AnalyzeStealthWindow(const StealthWindow& window);
    void OptimizeStealthWindowUsage();
    float CalculateStealthWindowEfficiency(const StealthWindow& window);

    // Metrics and analysis
    void UpdateSubtletyMetrics();
    void AnalyzeSubtletyEfficiency();
    void LogSubtletyDecision(const std::string& decision, const std::string& reason);

    // State variables
    SubtletyRotationPhase _subtletyPhase;
    ShadowDanceInfo _shadowDance;
    ShadowstepInfo _shadowstep;
    PreparationInfo _preparation;
    HemorrhageInfo _hemorrhage;
    SubtletyMetrics _metrics;

    // Stealth tracking
    std::queue<StealthWindow> _stealthWindows;
    StealthWindow _currentStealthWindow;
    uint32 _lastStealthEntry;
    uint32 _lastStealthExit;
    bool _isPlanningStealth;

    // Timing variables
    uint32 _lastAmbushTime;
    uint32 _lastBackstabTime;
    uint32 _lastHemorrhageTime;
    uint32 _lastEviscerateeTime;
    uint32 _lastShadowstepTime;
    uint32 _lastVanishTime;
    uint32 _lastShadowDanceTime;
    uint32 _lastPreparationTime;

    // Configuration constants
    static constexpr uint32 SHADOW_DANCE_DURATION = 8000;           // 8 seconds
    static constexpr uint32 SHADOWSTEP_COOLDOWN = 30000;           // 30 seconds
    static constexpr uint32 PREPARATION_COOLDOWN = 180000;         // 3 minutes
    static constexpr uint32 HEMORRHAGE_DURATION = 15000;           // 15 seconds
    static constexpr float HEMORRHAGE_REFRESH_THRESHOLD = 0.3f;     // Refresh at 30%
    static constexpr uint32 STEALTH_WINDOW_MIN_DURATION = 3000;     // Minimum 3 second stealth windows
    static constexpr uint32 MASTER_OF_SUBTLETY_DURATION = 6000;     // 6 seconds
    static constexpr uint8 MIN_COMBO_FOR_EVISCERATE = 3;            // Minimum combo points for Eviscerate
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.30f;        // Execute phase at 30% health
    static constexpr uint32 EMERGENCY_HEALTH_THRESHOLD = 30;        // Emergency abilities at 30% health
    static constexpr uint32 STEALTH_ENERGY_RESERVE = 80;            // Energy to save for stealth windows

    // Ability priorities
    std::vector<uint32> _stealthOpeners;
    std::vector<uint32> _comboBuilders;
    std::vector<uint32> _finishers;
    std::vector<uint32> _stealthAbilities;
    std::vector<uint32> _defensiveAbilities;

    // Optimization settings
    bool _prioritizeStealthWindows;
    bool _useAggressivePositioning;
    bool _saveEnergyForBurst;
    uint32 _preferredStealthOpener;
    uint32 _preferredComboBuilder;
    uint32 _preferredFinisher;
};

} // namespace Playerbot

#endif // SUBTLETY_SPECIALIZATION_H