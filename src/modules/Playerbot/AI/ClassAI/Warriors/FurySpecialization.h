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

namespace Playerbot
{

class TC_GAME_API FurySpecialization : public WarriorSpecialization
{
public:
    explicit FurySpecialization(Player* bot);

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
    WarriorSpec GetSpecialization() const override { return WarriorSpec::FURY; }
    const char* GetSpecializationName() const override { return "Fury"; }

private:
    // Fury-specific mechanics
    void UpdateEnrage();
    void UpdateFlurry();
    void UpdateBerserkerRage();
    void UpdateDualWield();
    bool ShouldCastBloodthirst(::Unit* target);
    bool ShouldCastWhirlwind();
    bool ShouldCastRampage(::Unit* target);
    bool ShouldCastExecute(::Unit* target);
    bool ShouldUseBerserkerRage();

    // Dual wield optimization
    void OptimizeDualWield();
    void UpdateOffhandAttacks();
    bool HasDualWieldWeapons();
    void CastFlurry();

    // Enrage mechanics
    void TriggerEnrage();
    void ManageEnrage();
    bool IsEnraged();
    uint32 GetEnrageTimeRemaining();
    void ExtendEnrage();

    // Fury rotation spells
    void CastBloodthirst(::Unit* target);
    void CastRampage(::Unit* target);
    void CastRagingBlow(::Unit* target);
    void CastFuriousSlash(::Unit* target);
    void CastExecute(::Unit* target);
    void CastWhirlwind();

    // Berserker mechanics
    void CastBerserkerRage();
    void UpdateBerserkerStance();
    bool ShouldStayInBerserkerStance();

    // Rage generation optimization
    void OptimizeRageGeneration();
    void BuildRage();
    bool ShouldConserveRage();
    void SpendRageEfficiently();

    // Cooldown management
    void UpdateFuryCooldowns(uint32 diff);
    void UseRecklessness();
    void UseEnragedRegeneration();
    bool ShouldUseRecklessness();
    bool ShouldUseEnragedRegeneration();

    // Execute phase management
    void HandleExecutePhase(::Unit* target);
    bool IsInExecutePhase(::Unit* target);
    void OptimizeExecuteRotation(::Unit* target);

    // Flurry mechanics
    void UpdateFlurryStacks();
    uint32 GetFlurryStacks();
    bool HasFlurryProc();
    void ConsumeFlurry();

    // Fury spell IDs
    enum FurySpells
    {
        BLOODTHIRST = 23881,
        RAMPAGE = 184367,
        RAGING_BLOW = 85288,
        FURIOUS_SLASH = 100130,
        EXECUTE = 5308,
        WHIRLWIND = 1680,
        BERSERKER_RAGE = 18499,
        ENRAGE = 184361,
        FLURRY = 12319,
        RECKLESSNESS = 1719,
        ENRAGED_REGENERATION = 55694,
        DUAL_WIELD = 674,
        TITANS_GRIP = 46917
    };

    // Enhanced state tracking
    std::atomic<bool> _isEnraged{false};
    uint32 _enrageEndTime;
    std::atomic<uint32> _flurryStacks{0};
    std::atomic<bool> _flurryProc{false};
    std::atomic<uint32> _rampageStacks{0};
    uint32 _lastBerserkerRage;
    uint32 _lastBloodthirst;
    uint32 _lastRampage;
    uint32 _lastEnrageTrigger;
    uint32 _enrageCount;
    bool _bloodthirstCritReady;

    // Performance metrics
    struct FuryMetrics {
        std::atomic<uint32> totalEnrageTime{0};
        std::atomic<uint32> bloodthirstCrits{0};
        std::atomic<uint32> rampageExecutions{0};
        std::atomic<uint32> whirlwindHits{0};
        std::atomic<float> averageEnrageUptime{0.0f};
        std::atomic<float> dualWieldEfficiency{0.0f};
        std::atomic<float> attackSpeedBonus{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalEnrageTime = 0; bloodthirstCrits = 0; rampageExecutions = 0;
            whirlwindHits = 0; averageEnrageUptime = 0.0f; dualWieldEfficiency = 0.0f;
            attackSpeedBonus = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _furyMetrics;

    // Rampage tracking system
    struct RampageTracker {
        std::queue<uint32> stackBuildTimes;
        uint32 lastRampageTime{0};
        uint32 totalStacks{0};
        void AddStack() {
            stackBuildTimes.push(getMSTime());
            totalStacks++;
            if (stackBuildTimes.size() > 5) // Max 5 stacks
                stackBuildTimes.pop();
        }
        uint32 GetStackCount() const {
            return static_cast<uint32>(stackBuildTimes.size());
        }
        bool HasMaxStacks() const {
            return stackBuildTimes.size() >= 4; // 4 stacks for Rampage
        }
        void ConsumeStacks() {
            while (!stackBuildTimes.empty())
                stackBuildTimes.pop();
            lastRampageTime = getMSTime();
        }
    } _rampageTracker;

    // Advanced Fury mechanics
    void OptimizeRampageStacks(::Unit* target);
    void ManageEnrageUptime();
    void OptimizeDualWieldAttackSpeed();
    void HandleFuryProcs();
    void OptimizeFuryRotation(::Unit* target);
    void ExecuteEnragePhase(::Unit* target);
    void HandleRampageMechanics(::Unit* target);
    void OptimizeExecutePhaseFury(::Unit* target);
    void ManageRageEfficiencyFury();
    void OptimizeOffhandTiming();
    void HandleDualWieldPenalties();
    void MaximizeAttackSpeed();
    float CalculateDualWieldEfficiency();
    void OptimizeEnrageTiming();
    void ExtendEnrageDuration();
    void HandleEnrageProcs();
    void ManageEnrageCooldowns();

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastEnrageCheck;
    uint32 _lastFlurryCheck;
    uint32 _lastDualWieldCheck;
    uint32 _lastRotationUpdate;

    // Execute phase tracking
    bool _inExecutePhase;
    uint32 _executePhaseStartTime;

    // Rage optimization
    uint32 _lastRageOptimization;
    float _averageRageGeneration;

    // Enhanced constants
    static constexpr uint32 ENRAGE_DURATION = 4000; // 4 seconds base
    static constexpr uint32 ENRAGE_EXTENDED_DURATION = 8000; // 8 seconds with talents
    static constexpr uint32 FLURRY_DURATION = 15000; // 15 seconds
    static constexpr uint32 MAX_FLURRY_STACKS = 3;
    static constexpr uint32 RAMPAGE_STACK_REQUIREMENT = 4; // 4 stacks needed
    static constexpr uint32 RAMPAGE_COOLDOWN = 1500; // 1.5 seconds
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 20.0f;
    static constexpr uint32 BLOODTHIRST_RAGE_COST = 30;
    static constexpr uint32 RAMPAGE_RAGE_COST = 85;
    static constexpr uint32 RAGING_BLOW_RAGE_COST = 20;
    static constexpr uint32 WHIRLWIND_RAGE_COST = 30;
    static constexpr float OPTIMAL_RAGE_THRESHOLD = 60.0f;
    static constexpr float RAGE_DUMP_THRESHOLD = 90.0f;
    static constexpr float DUAL_WIELD_PENALTY = 0.19f; // 19% miss chance penalty
    static constexpr float DUAL_WIELD_SPEED_BONUS = 0.5f; // 50% speed bonus
    static constexpr float ENRAGE_DAMAGE_BONUS = 0.25f; // 25% damage bonus
    static constexpr uint32 FURY_PROC_WINDOW = 6000; // 6 seconds
    static constexpr float RAMPAGE_CRIT_BONUS = 0.1f; // 10% crit per stack
    static constexpr uint32 MAX_RAMPAGE_STACKS = 5; // Maximum rampage stacks
};

} // namespace Playerbot