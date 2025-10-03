/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "WarlockSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API DestructionSpecialization : public WarlockSpecialization
{
public:
    explicit DestructionSpecialization(Player* bot);

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

    // Pet management
    void UpdatePetManagement() override;
    void SummonOptimalPet() override;
    WarlockPet GetOptimalPetForSituation() override;
    void CommandPet(uint32 action, ::Unit* target = nullptr) override;

    // DoT management
    void UpdateDoTManagement() override;
    void ApplyDoTsToTarget(::Unit* target) override;
    bool ShouldApplyDoT(::Unit* target, uint32 spellId) override;

    // Curse management
    void UpdateCurseManagement() override;
    uint32 GetOptimalCurseForTarget(::Unit* target) override;

    // Soul shard management
    void UpdateSoulShardManagement() override;
    bool HasSoulShardsAvailable(uint32 required = 1);
    void UseSoulShard(uint32 spellId);

    // Specialization info
    WarlockSpec GetSpecialization() const override { return WarlockSpec::DESTRUCTION; }
    const char* GetSpecializationName() const override { return "Destruction"; }

private:
    // Destruction-specific mechanics
    void UpdateBackdraft();
    void UpdateConflagrate();
    void UpdateShadowBurn();
    bool ShouldCastImmolate(::Unit* target);
    bool ShouldCastIncinerate(::Unit* target);
    bool ShouldCastConflagrate(::Unit* target);
    bool ShouldCastShadowBurn(::Unit* target);
    bool ShouldCastChaosBolt(::Unit* target);

    // Fire-based damage rotation
    void CastImmolate(::Unit* target);
    void CastIncinerate(::Unit* target);
    void CastConflagrate(::Unit* target);
    void CastShadowBurn(::Unit* target);
    void CastChaosBolt(::Unit* target);

    // Destruction spell IDs
    enum DestructionSpells
    {
        INCINERATE = 29722,
        CONFLAGRATE = 17962,
        SHADOW_BURN = 17877,
        CHAOS_BOLT = 50796,
        BACKDRAFT = 47258,
        SOUL_FIRE = 6353
    };

    // Enhanced state tracking
    std::atomic<uint32> _shadowBurnCharges{0};
    std::atomic<uint32> _backdraftStacks{0};
    std::atomic<uint32> _conflagrateCharges{1};
    std::atomic<uint32> _lastImmolate{0};
    std::atomic<uint32> _lastConflagrate{0};
    std::atomic<uint32> _lastShadowBurn{0};
    std::atomic<bool> _immolateActive{false};
    std::atomic<bool> _pyroblastProc{false};
    std::atomic<uint32> _devastationStacks{0};
    std::atomic<bool> _shadowfuryReady{false};
    std::atomic<uint32> _chaosBoltStacks{0};

    // Performance metrics
    struct DestructionMetrics {
        std::atomic<uint32> totalFireDamage{0};
        std::atomic<uint32> conflagrateCrits{0};
        std::atomic<uint32> chaosBoltCasts{0};
        std::atomic<uint32> shadowBurnKills{0};
        std::atomic<uint32> backdraftConsumed{0};
        std::atomic<float> immolateUptime{0.0f};
        std::atomic<float> criticalStrikeChance{0.0f};
        std::atomic<float> burstDamagePerSecond{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalFireDamage = 0; conflagrateCrits = 0; chaosBoltCasts = 0;
            shadowBurnKills = 0; backdraftConsumed = 0;
            immolateUptime = 0.0f; criticalStrikeChance = 0.0f; burstDamagePerSecond = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _destructionMetrics;

    // Burst window tracking
    struct BurstWindow {
        bool active{false};
        std::chrono::steady_clock::time_point startTime;
        uint32 spellsCast{0};
        uint32 damageDealt{0};
        void StartBurst() {
            active = true;
            startTime = std::chrono::steady_clock::now();
            spellsCast = 0;
            damageDealt = 0;
        }
        void EndBurst() {
            active = false;
        }
        bool IsActive() const { return active; }
        uint32 GetDuration() const {
            if (!active) return 0;
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        }
    } _burstWindow;

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Destruction mechanics
    void OptimizeBurstRotation();
    void ManageBackdraftStacks();
    void OptimizeCriticalStrikeWindow();
    void HandlePyroblastProc();
    void ManageConflagrateCharges();
    void OptimizeShadowBurnTiming();
    void HandleExecutePhaseDestruction();
    void ManageImmolateUptime();
    void OptimizeChaosBottTiming();
    void HandleFireAndBrimstone();
    void ManageDestructionCooldowns();
    void OptimizeManaEfficiencyDestruction();
    void HandleAoEDestruction();
    void ManageEmberGeneration();
    void OptimizeDestructionProcs();
    void HandleBurningEmbers();
    void ManageSoulFireTiming();
    void OptimizeIncinerateCasting();
    void HandleShadowfuryTiming();
    void ManageDestuctionBuffs();
    float CalculateBurstDPS();
    bool ShouldEnterBurstMode();
    void ExecuteBurstSequence();

    // Enhanced constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 CONFLAGRATE_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 SHADOW_BURN_COOLDOWN = 15000; // 15 seconds
    static constexpr uint32 CHAOS_BOLT_COOLDOWN = 12000; // 12 seconds
    // Spell IDs are defined in the enum above
    static constexpr uint32 IMMOLATE_DURATION = 15000; // 15 seconds
    static constexpr uint32 MAX_BACKDRAFT_STACKS = 3;
    static constexpr uint32 BACKDRAFT_DURATION = 15000; // 15 seconds
    static constexpr float EXECUTE_THRESHOLD = 25.0f; // 25% for Shadow Burn
    static constexpr uint32 BURST_WINDOW_DURATION = 15000; // 15 second burst
    static constexpr float CONFLAGRATE_CRIT_THRESHOLD = 0.6f; // 60% crit chance
    static constexpr uint32 MAX_CHAOS_BOLT_STACKS = 4;
    static constexpr float FIRE_AND_BRIMSTONE_THRESHOLD = 4; // 4+ targets
    static constexpr uint32 SHADOWFURY_COOLDOWN = 20000; // 20 seconds
    static constexpr float OPTIMAL_CRIT_CHANCE = 0.4f; // 40% crit for burst
    static constexpr uint32 EMBER_GENERATION_THRESHOLD = 3;
    static constexpr float MANA_BURST_THRESHOLD = 0.6f; // 60% mana for burst
};

} // namespace Playerbot