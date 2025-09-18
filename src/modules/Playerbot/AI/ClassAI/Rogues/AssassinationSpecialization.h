/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef ASSASSINATION_SPECIALIZATION_H
#define ASSASSINATION_SPECIALIZATION_H

#include "RogueSpecialization.h"
#include <vector>
#include <unordered_set>

namespace Playerbot
{

enum class AssassinationRotationPhase : uint8
{
    STEALTH_OPENER = 0,
    GARROTE_APPLICATION = 1,
    POISON_BUILDING = 2,
    MUTILATE_SPAM = 3,
    COMBO_SPENDING = 4,
    DOT_REFRESH = 5,
    BURST_PHASE = 6,
    EXECUTE_PHASE = 7,
    AOE_PHASE = 8,
    EMERGENCY = 9
};

enum class AssassinationPriority : uint8
{
    STEALTH_OPENER = 0,
    EMERGENCY_HEAL = 1,
    INTERRUPT = 2,
    CROWD_CONTROL = 3,
    DOT_REFRESH = 4,
    COMBO_SPEND = 5,
    COMBO_BUILD = 6,
    POISON_APPLICATION = 7,
    BUFF_MAINTENANCE = 8,
    MOVEMENT = 9
};

struct DotInfo
{
    uint32 spellId;
    uint32 duration;
    uint32 tickDamage;
    uint32 timeRemaining;
    bool isActive;
    uint8 stacks;

    DotInfo() : spellId(0), duration(0), tickDamage(0), timeRemaining(0), isActive(false), stacks(0) {}
    DotInfo(uint32 spell, uint32 dur) : spellId(spell), duration(dur), tickDamage(0), timeRemaining(0), isActive(false), stacks(0) {}
};

struct PoisonStack
{
    PoisonType type;
    uint8 stacks;
    uint32 timeRemaining;
    uint32 lastApplication;

    PoisonStack() : type(PoisonType::NONE), stacks(0), timeRemaining(0), lastApplication(0) {}
};

struct AssassinationMetrics
{
    uint32 mutilateCasts;
    uint32 backStabCasts;
    uint32 envenom Casts;
    uint32 ruptureApplications;
    uint32 garroteApplications;
    uint32 poisonApplications;
    uint32 totalDotTicks;
    uint32 coldBloodUsages;
    uint32 vanishEscapes;
    uint32 totalStealthTime;
    float poisonUptime;
    float ruptureUptime;
    float garroteUptime;
    float averageComboPointsOnSpend;

    AssassinationMetrics() : mutilateCasts(0), backStabCasts(0), envenomCasts(0), ruptureApplications(0),
                           garroteApplications(0), poisonApplications(0), totalDotTicks(0), coldBloodUsages(0),
                           vanishEscapes(0), totalStealthTime(0), poisonUptime(0.0f), ruptureUptime(0.0f),
                           garroteUptime(0.0f), averageComboPointsOnSpend(0.0f) {}
};

class AssassinationSpecialization : public RogueSpecialization
{
public:
    explicit AssassinationSpecialization(Player* bot);
    ~AssassinationSpecialization() override = default;

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

    // Stealth Management Implementation
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

    // Poison Management Implementation
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
    // Assassination-specific systems
    void UpdateDotManagement();
    void UpdatePoisonStacks();
    void UpdateStealthAdvantage();
    void UpdateBurstPhase();
    void UpdateExecutePhase();
    void UpdateAoEPhase();
    void UpdateEmergencyPhase();

    // Rotation helpers
    void ExecuteStealthRotation(::Unit* target);
    void ExecuteGarrotePhase(::Unit* target);
    void ExecutePoisonBuildingPhase(::Unit* target);
    void ExecuteMutilatePhase(::Unit* target);
    void ExecuteComboSpendingPhase(::Unit* target);
    void ExecuteDotRefreshPhase(::Unit* target);
    void ExecuteBurstPhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteAoEPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // DoT management
    void RefreshRupture(::Unit* target);
    void RefreshGarrote(::Unit* target);
    void UpdateDotTicks();
    bool ShouldRefreshRupture(::Unit* target);
    bool ShouldRefreshGarrote(::Unit* target);
    uint32 GetOptimalRuptureComboPoints();

    // Poison management
    void ApplyInstantPoison();
    void ApplyDeadlyPoison();
    void ApplyWoundPoison();
    void ApplyCripplingPoison();
    void UpdatePoisonCharges();
    bool ShouldApplyPoisons();
    uint32 GetPoisonStacks(::Unit* target, PoisonType type);

    // Stealth and opener management
    void ExecuteGarroteOpener(::Unit* target);
    void ExecuteCheapShotOpener(::Unit* target);
    void ExecuteAmbushOpener(::Unit* target);
    bool ShouldUseGarroteOpener(::Unit* target);
    bool ShouldUseCheapShotOpener(::Unit* target);
    bool ShouldUseAmbushOpener(::Unit* target);

    // Combo point optimization
    void OptimizeComboPointSpending();
    bool ShouldUseEnvenom(::Unit* target);
    bool ShouldUseEviscerate(::Unit* target);
    bool ShouldUseRupture(::Unit* target);
    bool ShouldUseSliceAndDice();
    uint8 GetOptimalComboPointsForFinisher();

    // Burst phase management
    void InitiateBurstPhase();
    void ExecuteColdBloodBurst(::Unit* target);
    void ExecuteVendettaBurst(::Unit* target);
    bool ShouldUseColdBlood();
    bool ShouldUseVendetta();

    // Defensive abilities
    void HandleEmergencySituations();
    void ExecuteVanishEscape();
    void ExecuteEvasion();
    void ExecuteCloakOfShadows();
    bool ShouldVanishEscape();
    bool ShouldUseEvasion();
    bool ShouldUseCloakOfShadows();

    // Utility and positioning
    void OptimizePositioning(::Unit* target);
    void ExecuteKick(::Unit* target);
    void ExecuteBlind(::Unit* target);
    void ExecuteGouge(::Unit* target);
    bool ShouldInterrupt(::Unit* target);
    bool ShouldUseCrowdControl(::Unit* target);

    // Metrics and analysis
    void UpdateCombatMetrics();
    void AnalyzeRotationEfficiency();
    void LogAssassinationDecision(const std::string& decision, const std::string& reason);

    // State variables
    AssassinationRotationPhase _assassinationPhase;
    std::unordered_map<uint32, DotInfo> _dots;
    std::unordered_map<PoisonType, PoisonStack> _poisonStacks;
    AssassinationMetrics _metrics;

    // Timing variables
    uint32 _lastMutilateTime;
    uint32 _lastEnvenomTime;
    uint32 _lastRuptureTime;
    uint32 _lastGarroteTime;
    uint32 _lastPoisonApplicationTime;
    uint32 _burstPhaseStartTime;
    uint32 _lastStealthTime;
    uint32 _lastVanishTime;

    // Configuration
    static constexpr float DOT_REFRESH_THRESHOLD = 0.3f;     // Refresh DoTs when 30% duration remains
    static constexpr uint32 BURST_PHASE_DURATION = 15000;   // 15 second burst windows
    static constexpr uint32 POISON_REAPPLY_INTERVAL = 30000; // 30 seconds
    static constexpr uint8 MIN_COMBO_FOR_RUPTURE = 4;       // Minimum combo points for Rupture
    static constexpr uint8 MIN_COMBO_FOR_ENVENOM = 3;       // Minimum combo points for Envenom
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.35f; // Execute phase at 35% health
    static constexpr uint32 EMERGENCY_HEALTH_THRESHOLD = 30; // Emergency abilities at 30% health

    // Stealth opener priorities
    std::vector<uint32> _stealthOpeners;
    uint32 _preferredOpener;

    // Combo builders priority
    std::vector<uint32> _comboBuilders;
    uint32 _preferredComboBuilder;

    // Finishers priority
    std::vector<uint32> _finishers;
    uint32 _preferredFinisher;
};

} // namespace Playerbot

#endif // ASSASSINATION_SPECIALIZATION_H