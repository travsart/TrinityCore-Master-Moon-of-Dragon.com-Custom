/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "WarriorSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>

namespace Playerbot
{

class TC_GAME_API ArmsSpecialization : public WarriorSpecialization
{
public:
    explicit ArmsSpecialization(Player* bot);

    // Core specialization interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Stance management
    void UpdateStance() override;
    WarriorStance GetOptimalStance(::Unit* target) override;
    void SwitchStance(WarriorStance stance) override;

    // Specialization info
    WarriorSpec GetSpecialization() const override { return WarriorSpec::ARMS; }
    const char* GetSpecializationName() const override { return "Arms"; }

private:
    // Arms-specific mechanics
    void UpdateMortalStrike();
    void UpdateOverpower();
    void UpdateDeepWounds();
    void UpdateTacticalMastery();
    bool ShouldCastMortalStrike(::Unit* target);
    bool ShouldCastOverpower(::Unit* target);
    bool ShouldCastExecute(::Unit* target);
    bool ShouldCastColossusSmash(::Unit* target);
    bool ShouldCastWarBreaker(::Unit* target);

    // Two-handed weapon optimization
    void OptimizeTwoHandedWeapon();
    void UpdateWeaponMastery();
    bool HasTwoHandedWeapon();
    void CastSweepingStrikes();

    // Arms rotation spells
    void CastMortalStrike(::Unit* target);
    void CastColossusSmash(::Unit* target);
    void CastOverpower(::Unit* target);
    void CastExecute(::Unit* target);
    void CastWarBreaker(::Unit* target);
    void CastWhirlwind();
    void CastCleave(::Unit* target);

    // Deep Wounds management
    void ApplyDeepWounds(::Unit* target);
    bool HasDeepWounds(::Unit* target);
    uint32 GetDeepWoundsTimeRemaining(::Unit* target);

    // Tactical mastery and stance dancing
    void ManageStanceDancing();
    bool ShouldSwitchToDefensive();
    bool ShouldSwitchToBerserker();
    uint32 GetTacticalMasteryRage();

    // Cooldown management
    void UpdateArmsCooldowns(uint32 diff);
    void UseBladestorm();
    void UseAvatar();
    void UseRecklessness();
    bool ShouldUseBladestorm();
    bool ShouldUseAvatar();

    // Execute phase management
    void HandleExecutePhase(::Unit* target);
    bool IsInExecutePhase(::Unit* target);
    void OptimizeExecuteRotation(::Unit* target);

    // Arms spell IDs
    enum ArmsSpells
    {
        MORTAL_STRIKE = 12294,
        COLOSSUS_SMASH = 86346,
        OVERPOWER = 7384,
        EXECUTE = 5308,
        WAR_BREAKER = 262161,
        WHIRLWIND = 1680,
        SWEEPING_STRIKES = 260708,
        BLADESTORM = 227847,
        AVATAR = 107574,
        RECKLESSNESS = 1719,
        DEEP_WOUNDS = 115767,
        TACTICAL_MASTERY = 12295,
        SUDDEN_DEATH = 29723
    };

    // Enhanced state tracking
    WarriorStance _preferredStance;
    uint32 _lastMortalStrike;
    uint32 _lastColossusSmash;
    uint32 _lastOverpower;
    std::atomic<bool> _overpowerReady{false};
    std::atomic<bool> _suddenDeathProc{false};
    uint32 _lastRendApplication;
    uint32 _consecutiveCrits;
    bool _deepWoundsActive;

    // Advanced Arms mechanics
    void OptimizeColossusSmashTiming(::Unit* target);
    void ManageMortalStrikeDebuff(::Unit* target);
    void HandleOverpowerProc();
    void OptimizeExecutePhaseRotation(::Unit* target);
    void ManageRendDebuff(::Unit* target);
    void HandleSuddenDeathProc(::Unit* target);
    void OptimizeWeaponSwapping();

    // Two-handed weapon mastery
    void UpdateWeaponSpecialization();
    void OptimizeCriticalStrikes();
    void HandleWeaponMasteryProcs();
    float CalculateWeaponDamageBonus();

    // Performance metrics
    struct ArmsMetrics {
        std::atomic<uint32> totalMortalStrikes{0};
        std::atomic<uint32> colossusSmashUptime{0};
        std::atomic<uint32> overpowerProcs{0};
        std::atomic<uint32> suddenDeathProcs{0};
        std::atomic<uint32> executeKills{0};
        std::atomic<float> weaponDamageEfficiency{0.0f};
        std::atomic<float> executePhaseEfficiency{0.0f};
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalMortalStrikes = 0; colossusSmashUptime = 0; overpowerProcs = 0;
            suddenDeathProcs = 0; executeKills = 0;
            weaponDamageEfficiency = 0.0f; executePhaseEfficiency = 0.0f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    } _armsMetrics;

    // Debuff tracking system
    struct DebuffTracker {
        std::unordered_map<ObjectGuid, uint32> mortalStrikeExpireTimes;
        std::unordered_map<ObjectGuid, uint32> rendExpireTimes;
        std::unordered_map<ObjectGuid, uint32> deepWoundsStacks;
        void UpdateMortalStrike(ObjectGuid guid, uint32 duration) {
            mortalStrikeExpireTimes[guid] = getMSTime() + duration;
        }
        void UpdateRend(ObjectGuid guid, uint32 duration) {
            rendExpireTimes[guid] = getMSTime() + duration;
        }
        void UpdateDeepWounds(ObjectGuid guid, uint32 stacks) {
            deepWoundsStacks[guid] = stacks;
        }
        bool HasMortalStrike(ObjectGuid guid) const {
            auto it = mortalStrikeExpireTimes.find(guid);
            return it != mortalStrikeExpireTimes.end() && it->second > getMSTime();
        }
        bool HasRend(ObjectGuid guid) const {
            auto it = rendExpireTimes.find(guid);
            return it != rendExpireTimes.end() && it->second > getMSTime();
        }
        uint32 GetDeepWoundsStacks(ObjectGuid guid) const {
            auto it = deepWoundsStacks.find(guid);
            return it != deepWoundsStacks.end() ? it->second : 0;
        }
    } _debuffTracker;

    // Deep Wounds tracking per target
    std::map<uint64, uint32> _deepWoundsTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastStanceCheck;
    uint32 _lastWeaponCheck;
    uint32 _lastRotationUpdate;

    // Enhanced execute phase tracking
    std::atomic<bool> _inExecutePhase{false};
    uint32 _executePhaseStartTime;
    uint32 _executeAttempts;
    uint32 _successfulExecutes;
    std::queue<uint32> _executeTimings;

    // Execute phase optimization
    void PrepareForExecutePhase(::Unit* target);
    void MonitorExecuteOpportunities(::Unit* target);
    void OptimizeExecuteRageManagement();
    bool ShouldSaveRageForExecute(::Unit* target);
    void HandleExecutePhaseTransition(::Unit* target);

    // Enhanced constants
    static constexpr uint32 DEEP_WOUNDS_DURATION = 21000; // 21 seconds
    static constexpr uint32 COLOSSUS_SMASH_DURATION = 10000; // 10 seconds
    static constexpr uint32 MORTAL_STRIKE_DURATION = 10000; // 10 seconds
    static constexpr uint32 REND_DURATION = 21000; // 21 seconds
    static constexpr uint32 OVERPOWER_WINDOW = 5000; // 5 seconds
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 20.0f;
    static constexpr float EXECUTE_OPTIMAL_THRESHOLD = 25.0f; // Start preparing at 25%
    static constexpr uint32 MORTAL_STRIKE_RAGE_COST = 30;
    static constexpr uint32 EXECUTE_RAGE_COST = 15;
    static constexpr uint32 EXECUTE_MAX_RAGE_COST = 60; // Maximum execute cost
    static constexpr uint32 TACTICAL_MASTERY_RAGE = 25;
    static constexpr uint32 OVERPOWER_RAGE_COST = 5;
    static constexpr uint32 REND_RAGE_COST = 10;
    static constexpr float TWO_HANDED_DAMAGE_BONUS = 1.15f; // 15% bonus
    static constexpr uint32 WEAPON_MASTERY_WINDOW = 3000; // 3 seconds
    static constexpr float CRITICAL_STRIKE_THRESHOLD = 0.7f; // 70% crit for optimization
};

} // namespace Playerbot