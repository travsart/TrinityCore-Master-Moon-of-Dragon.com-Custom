/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ClassAI.h"
#include "WarriorSpecialization.h"
#include "Position.h"
#include "BotThreatManager.h"
#include "TargetSelector.h"
#include "PositionManager.h"
#include "InterruptManager.h"
#include "FormationManager.h"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>

namespace Playerbot
{

// Forward declarations
class ArmsSpecialization;
class FurySpecialization;
class ProtectionSpecialization;

// Warrior AI implementation
class TC_GAME_API WarriorAI : public ClassAI
{
public:
    explicit WarriorAI(Player* bot);
    ~WarriorAI() = default;

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization system
    WarriorSpec _currentSpec;
    std::unique_ptr<WarriorSpecialization> _specialization;

    // Enhanced performance tracking
    std::atomic<uint32> _rageSpent{0};
    std::atomic<uint32> _damageDealt{0};
    std::atomic<uint32> _damageAbsorbed{0};
    std::atomic<uint32> _threatGenerated{0};
    std::atomic<uint32> _successfulCharges{0};
    std::atomic<uint32> _successfulInterrupts{0};
    uint32 _lastStanceChange;

    // Combat system integration
    std::unique_ptr<ThreatManager> _threatManager;
    std::unique_ptr<TargetSelector> _targetSelector;
    std::unique_ptr<PositionManager> _positionManager;
    std::unique_ptr<InterruptManager> _interruptManager;
    std::unique_ptr<FormationManager> _formationManager;

    // Shared utility tracking
    std::unordered_map<uint32, uint32> _abilityUsage;
    uint32 _lastBattleShout;
    uint32 _lastCommandingShout;
    bool _needsIntercept;
    bool _needsCharge;
    ::Unit* _lastChargeTarget;
    uint32 _lastChargeTime;

    // Specialization management
    void InitializeSpecialization();
    void UpdateSpecialization();
    WarriorSpec DetectCurrentSpecialization();
    void SwitchSpecialization(WarriorSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared warrior utilities
    void UpdateWarriorBuffs();
    void CastBattleShout();
    void CastCommandingShout();
    void UseChargeAbilities(::Unit* target);
    bool IsInMeleeRange(::Unit* target) const;
    bool CanCharge(::Unit* target) const;

    // Enhanced target evaluation and combat AI
    bool IsValidTarget(::Unit* target);
    ::Unit* GetBestChargeTarget();
    ::Unit* GetHighestThreatTarget();
    ::Unit* GetLowestHealthEnemy();
    ::Unit* SelectOptimalTarget(const std::vector<::Unit*>& enemies);
    float CalculateTargetPriority(::Unit* target);

    // Advanced combat mechanics
    void UpdateAdvancedCombatLogic(::Unit* target);
    void HandleMultipleEnemies(const std::vector<::Unit*>& enemies);
    void OptimizeStanceForSituation(::Unit* target);
    void ManageRageEfficiency();
    void ExecuteAdvancedRotation(::Unit* target);

    // Stance dancing optimization
    void OptimizeStanceDancing(::Unit* target);
    WarriorStance DetermineOptimalStance(::Unit* target, const std::vector<::Unit*>& enemies);
    void HandleStanceSpecificAbilities(WarriorStance stance, ::Unit* target);
    void ManageTacticalStanceSwitching();

    // Defensive and survival mechanics
    void HandleDefensiveSituation();
    void UseDefensiveCooldowns();
    void ManageHealthThresholds();
    void HandleCrowdControl();
    void OptimizeInterruptTiming();

    // Group combat optimization
    void HandleGroupCombatRole();
    void ManageThreatInGroup();
    void CoordinateWithGroup();
    void OptimizeFormationPosition();

    // Performance metrics and analytics
    struct WarriorMetrics {
        std::atomic<uint32> totalAbilitiesUsed{0};
        std::atomic<uint32> successfulStanceChanges{0};
        std::atomic<uint32> rageDumpInstances{0};
        std::atomic<float> averageRageEfficiency{0.0f};
        std::atomic<float> stanceOptimizationScore{0.0f};
        std::atomic<float> survivabilityScore{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastMetricsUpdate;
        void Reset() {
            totalAbilitiesUsed = 0; successfulStanceChanges = 0; rageDumpInstances = 0;
            averageRageEfficiency = 0.0f; stanceOptimizationScore = 0.0f; survivabilityScore = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastMetricsUpdate = combatStartTime;
        }
    } _warriorMetrics;

    // Enhanced performance optimization
    void RecordAbilityUsage(uint32 spellId);
    void RecordStanceChange(WarriorStance fromStance, WarriorStance toStance);
    void RecordChargeSuccess(::Unit* target, bool success);
    void RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success);
    void AnalyzeCombatEffectiveness();
    void UpdateMetrics(uint32 diff);
    float CalculateRageEfficiency();
    void OptimizeAbilityPriorities();

    // Rage management optimization
    void OptimizeRageUsage();
    bool ShouldConserveRage();
    bool ShouldDumpRage();
    void HandleRageStarvation();
    void PrioritizeRageSpenders(::Unit* target);

    // Weapon and equipment optimization
    void OptimizeWeaponChoice();
    void HandleWeaponSwapping();
    void UpdateWeaponSpecialization();
    bool ShouldUseShield();

    // Mobility and positioning
    void OptimizeMobility(::Unit* target);
    void HandleChargeOpportunities(::Unit* target);
    void ManageInterceptUsage(::Unit* target);
    void OptimizeHeroicLeapTiming();
    Position CalculateOptimalChargePosition(::Unit* target);

    // Advanced threat management
    void ManageAdvancedThreat();
    void BuildThreatOnTarget(::Unit* target);
    void ReduceThreatWhenNeeded();
    void HandleThreatEmergency();

    // Enhanced constants
    static constexpr uint32 STANCE_CHANGE_COOLDOWN = 1000; // 1 second
    static constexpr uint32 CHARGE_MIN_RANGE = 8;
    static constexpr uint32 CHARGE_MAX_RANGE = 25;
    static constexpr uint32 INTERCEPT_MIN_RANGE = 8;
    static constexpr uint32 INTERCEPT_MAX_RANGE = 25;
    static constexpr uint32 BATTLE_SHOUT_DURATION = 120000; // 2 minutes
    static constexpr uint32 COMMANDING_SHOUT_DURATION = 120000; // 2 minutes
    static constexpr float OPTIMAL_MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_CHARGE_DISTANCE = 15.0f;
    static constexpr float THREAT_MANAGEMENT_RANGE = 30.0f;
    static constexpr float RAGE_EFFICIENCY_TARGET = 0.85f; // 85% efficiency
    static constexpr uint32 RAGE_CONSERVATION_THRESHOLD = 20;
    static constexpr uint32 RAGE_DUMP_THRESHOLD = 80;
    static constexpr uint32 HEALTH_EMERGENCY_THRESHOLD = 25; // 25% health
    static constexpr uint32 DEFENSIVE_COOLDOWN_THRESHOLD = 40; // 40% health
    static constexpr uint32 STANCE_OPTIMIZATION_INTERVAL = 3000; // 3 seconds
    static constexpr float MULTI_TARGET_THRESHOLD = 3.0f; // 3+ enemies
    static constexpr uint32 FORMATION_CHECK_INTERVAL = 2000; // 2 seconds

    // Spell IDs (these would need to be accurate for the WoW version)
    enum WarriorSpells
    {
        // Stances
        BATTLE_STANCE = 2457,
        DEFENSIVE_STANCE = 71,
        BERSERKER_STANCE = 2458,

        // Basic attacks
        HEROIC_STRIKE = 78,
        CLEAVE = 845,
        WHIRLWIND = 1680,

        // Arms abilities
        MORTAL_STRIKE = 12294,
        COLOSSUS_SMASH = 86346,
        OVERPOWER = 7384,
        REND = 772,

        // Fury abilities
        BLOODTHIRST = 23881,
        RAMPAGE = 184367,
        RAGING_BLOW = 85288,
        EXECUTE = 5308,

        // Protection abilities
        SHIELD_SLAM = 23922,
        THUNDER_CLAP = 6343,
        REVENGE = 6572,
        DEVASTATE = 20243,
        SHIELD_BLOCK = 2565,

        // Defensive cooldowns
        SHIELD_WALL = 871,
        LAST_STAND = 12975,
        SPELL_REFLECTION = 23920,

        // Offensive cooldowns
        RECKLESSNESS = 1719,
        BLADESTORM = 46924,
        AVATAR = 107574,

        // Movement abilities
        CHARGE = 100,
        INTERCEPT = 20252,
        HEROIC_LEAP = 6544,

        // Utility
        PUMMEL = 6552,
        DISARM = 676,
        TAUNT = 355,
        SUNDER_ARMOR = 7386,

        // Buffs
        BATTLE_SHOUT = 6673,
        COMMANDING_SHOUT = 469,

        // Weapon buffs (if applicable)
        WEAPON_MASTER = 16538
    };
};

} // namespace Playerbot