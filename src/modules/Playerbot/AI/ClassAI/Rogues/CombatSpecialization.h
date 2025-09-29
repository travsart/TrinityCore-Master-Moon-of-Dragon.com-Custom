/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef COMBAT_SPECIALIZATION_H
#define COMBAT_SPECIALIZATION_H

#include "RogueSpecialization.h"
#include <vector>
#include <unordered_set>

namespace Playerbot
{

enum class CombatRotationPhase : uint8
{
    OPENER = 0,
    SLICE_AND_DICE_SETUP = 1,
    SINISTER_STRIKE_SPAM = 2,
    COMBO_SPENDING = 3,
    ADRENALINE_RUSH_BURST = 4,
    BLADE_FLURRY_AOE = 5,
    EXPOSE_ARMOR_DEBUFF = 6,
    DEFENSIVE_PHASE = 7,
    EXECUTE_PHASE = 8,
    EMERGENCY = 9
};

enum class CombatPriority : uint8
{
    EMERGENCY_DEFENSE = 0,
    INTERRUPT = 1,
    SLICE_AND_DICE_REFRESH = 2,
    ADRENALINE_RUSH_BURST = 3,
    BLADE_FLURRY_AOE = 4,
    EXPOSE_ARMOR_APPLICATION = 5,
    COMBO_SPEND = 6,
    COMBO_BUILD = 7,
    RIPOSTE_COUNTER = 8,
    MOVEMENT = 9
};

struct WeaponSpecialization
{
    bool hasSwordSpec;
    bool hasMaceSpec;
    bool hasDaggerSpec;
    bool hasFistSpec;
    uint32 mainHandType;
    uint32 offHandType;
    float swordSpecProc;
    float maceSpecProc;
    float daggerSpecBonus;
    float fistSpecBonus;

    WeaponSpecialization() : hasSwordSpec(false), hasMaceSpec(false), hasDaggerSpec(false), hasFistSpec(false),
                           mainHandType(0), offHandType(0), swordSpecProc(0.0f), maceSpecProc(0.0f),
                           daggerSpecBonus(0.0f), fistSpecBonus(0.0f) {}
};

struct AdrenalineRushInfo
{
    bool isActive;
    uint32 remainingTime;
    uint32 lastActivation;
    uint32 energyGenerated;
    uint32 abilitiesCastDuringRush;

    AdrenalineRushInfo() : isActive(false), remainingTime(0), lastActivation(0), energyGenerated(0), abilitiesCastDuringRush(0) {}
};

struct BladeFlurryInfo
{
    bool isActive;
    uint32 remainingTime;
    uint32 lastActivation;
    uint32 targetsHit;
    uint32 totalDamageDealt;

    BladeFlurryInfo() : isActive(false), remainingTime(0), lastActivation(0), targetsHit(0), totalDamageDealt(0) {}
};

struct RiposteInfo
{
    bool canRiposte;
    uint32 lastParry;
    uint32 ripostesExecuted;
    uint32 totalRiposteDamage;

    RiposteInfo() : canRiposte(false), lastParry(0), ripostesExecuted(0), totalRiposteDamage(0) {}
};

struct CombatMetrics
{
    uint32 sinisterStrikeCasts;
    uint32 eviscerateCasts;
    uint32 sliceAndDiceApplications;
    uint32 exposeArmorApplications;
    uint32 adrenalineRushActivations;
    uint32 bladeFlurryActivations;
    uint32 riposteExecutions;
    uint32 totalEnergyRegenerated;
    uint32 totalComboPointsGenerated;
    uint32 totalComboPointsSpent;
    float sliceAndDiceUptime;
    float exposeArmorUptime;
    float adrenalineRushUptime;
    float bladeFlurryUptime;
    float averageEnergyEfficiency;
    float weaponSpecializationProcs;

    CombatMetrics() : sinisterStrikeCasts(0), eviscerateCasts(0), sliceAndDiceApplications(0), exposeArmorApplications(0),
                     adrenalineRushActivations(0), bladeFlurryActivations(0), riposteExecutions(0), totalEnergyRegenerated(0),
                     totalComboPointsGenerated(0), totalComboPointsSpent(0), sliceAndDiceUptime(0.0f), exposeArmorUptime(0.0f),
                     adrenalineRushUptime(0.0f), bladeFlurryUptime(0.0f), averageEnergyEfficiency(0.0f), weaponSpecializationProcs(0.0f) {}
};

class CombatSpecialization : public RogueSpecialization
{
public:
    explicit CombatSpecialization(Player* bot);
    ~CombatSpecialization() override = default;

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

    // Stealth Management Implementation (Limited for Combat)
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

    // Poison Management Implementation (Minimal for Combat)
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

    // Utility Functions Implementation
    bool CastSpell(uint32 spellId, ::Unit* target = nullptr) override;
    bool HasSpell(uint32 spellId) override;
    SpellInfo const* GetSpellInfo(uint32 spellId) override;
    uint32 GetSpellCooldown(uint32 spellId) override;

private:
    // Combat-specific systems
    void UpdateWeaponSpecialization();
    void UpdateAdrenalineRushManagement();
    void UpdateBladeFlurryManagement();
    void UpdateRiposteManagement();
    void UpdateSliceAndDiceManagement();
    void UpdateExposeArmorManagement();
    void UpdateDefensiveAbilities();

    // Rotation phases
    void ExecuteOpenerPhase(::Unit* target);
    void ExecuteSliceAndDiceSetup(::Unit* target);
    void ExecuteSinisterStrikeSpam(::Unit* target);
    void ExecuteComboSpendingPhase(::Unit* target);
    void ExecuteAdrenalineRushBurst(::Unit* target);
    void ExecuteBladeFlurryAoE(::Unit* target);
    void ExecuteExposeArmorDebuff(::Unit* target);
    void ExecuteDefensivePhase(::Unit* target);
    void ExecuteExecutePhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Weapon specialization management
    void DetectWeaponTypes();
    void UpdateWeaponSpecializationProcs();
    bool ShouldUseSinisterStrike(::Unit* target);
    bool ShouldUseBackstab(::Unit* target);
    float GetWeaponSpecializationBonus();

    // Adrenaline Rush management
    void ActivateAdrenalineRush();
    void UpdateAdrenalineRushBurst(::Unit* target);
    bool ShouldUseAdrenalineRush();
    void OptimizeAdrenalineRushUsage(::Unit* target);

    // Blade Flurry management
    void ActivateBladeFlurry();
    void UpdateBladeFlurryAoE(::Unit* target);
    bool ShouldUseBladeFlurry();
    uint32 CountNearbyEnemies(::Unit* target);

    // Riposte management
    void ExecuteRiposte(::Unit* target);
    bool CanUseRiposte();
    void UpdateParryTracking();

    // Slice and Dice management
    void RefreshSliceAndDice();
    bool ShouldRefreshSliceAndDice();
    uint32 GetSliceAndDiceTimeRemaining();
    uint8 GetOptimalSliceAndDiceComboPoints();

    // Expose Armor management
    void ApplyExposeArmor(::Unit* target);
    bool ShouldApplyExposeArmor(::Unit* target);
    bool ShouldRefreshExposeArmor(::Unit* target);
    uint32 GetExposeArmorTimeRemaining(::Unit* target);

    // Combo point optimization
    void OptimizeCombatRotation(::Unit* target);
    bool ShouldUseEviscerate(::Unit* target);
    bool ShouldPrioritizeSliceAndDice();
    bool ShouldPrioritizeExposeArmor(::Unit* target);

    // Defensive abilities
    void HandleDefensiveSituations();
    void ExecuteEvasion();
    void ExecuteSprint();
    void ExecuteGouge(::Unit* target);
    void ExecuteKick(::Unit* target);
    bool ShouldUseDefensiveAbility();

    // Energy optimization
    void OptimizeEnergyUsage();
    void PrioritizeEnergySpending(::Unit* target);
    bool ShouldDelayAbilityForEnergy(uint32 spellId);

    // Positioning and movement
    void OptimizeCombatPositioning(::Unit* target);
    void MaintainMeleeRange(::Unit* target);
    bool ShouldRepositionForAdvantage(::Unit* target);

    // Metrics and analysis
    void UpdateCombatMetrics();
    void AnalyzeCombatEfficiency();
    void LogCombatDecision(const std::string& decision, const std::string& reason);

    // State variables
    CombatRotationPhase _combatPhase;
    WeaponSpecialization _weaponSpec;
    AdrenalineRushInfo _adrenalineRush;
    BladeFlurryInfo _bladeFlurry;
    RiposteInfo _riposte;
    CombatMetrics _metrics;

    // Timing variables
    uint32 _lastSinisterStrikeTime;
    uint32 _lastEviscerateeTime;
    uint32 _lastSliceAndDiceTime;
    uint32 _lastExposeArmorTime;
    uint32 _lastRiposteTime;
    uint32 _lastAdrenalineRushTime;
    uint32 _lastBladeFlurryTime;
    uint32 _lastDefensiveAbilityTime;

    // Configuration constants
    static constexpr float SLICE_AND_DICE_REFRESH_THRESHOLD = 0.3f;  // Refresh at 30% duration
    static constexpr float EXPOSE_ARMOR_REFRESH_THRESHOLD = 0.2f;    // Refresh at 20% duration
    static constexpr uint32 ADRENALINE_RUSH_DURATION = 15000;        // 15 seconds
    static constexpr uint32 BLADE_FLURRY_DURATION = 15000;           // 15 seconds
    static constexpr uint32 RIPOSTE_WINDOW = 5000;                   // 5 second window after parry
    static constexpr uint8 MIN_COMBO_FOR_EVISCERATE = 3;             // Minimum combo points for Eviscerate
    static constexpr uint8 OPTIMAL_SLICE_AND_DICE_COMBO = 2;         // Optimal combo points for Slice and Dice
    static constexpr uint32 AOE_ENEMY_THRESHOLD = 3;                 // Minimum enemies for AoE
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.25f;         // Execute phase at 25% health
    static constexpr uint32 EMERGENCY_HEALTH_THRESHOLD = 25;         // Emergency abilities at 25% health

    // Ability priorities
    std::vector<uint32> _comboBuilders;
    std::vector<uint32> _finishers;
    std::vector<uint32> _defensiveAbilities;
    std::vector<uint32> _interruptAbilities;

    // Weapon type priorities
    std::unordered_map<uint32, float> _weaponTypePriorities;
    uint32 _preferredComboBuilder;
    uint32 _preferredFinisher;
};

} // namespace Playerbot

#endif // COMBAT_SPECIALIZATION_H