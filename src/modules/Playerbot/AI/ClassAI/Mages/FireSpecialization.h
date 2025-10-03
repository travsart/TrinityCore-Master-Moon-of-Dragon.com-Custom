/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "MageSpecialization.h"
#include "ObjectGuid.h"
#include "Timer.h"
#include <map>
#include <vector>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace Playerbot
{

class TC_GAME_API FireSpecialization : public MageSpecialization
{
public:
    explicit FireSpecialization(Player* bot);

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

    // Specialization info
    MageSpec GetSpecialization() const override { return MageSpec::FIRE; }
    const char* GetSpecializationName() const override { return "Fire"; }

private:
    // Fire-specific mechanics
    void UpdateHotStreak();
    void UpdateHeatingUp();
    void UpdateCombustion();
    bool HasHotStreak();
    bool HasHeatingUp();
    bool HasCombustion();
    bool ShouldCastPyroblast();
    bool ShouldCastFireball();
    bool ShouldCastFireBlast();
    bool ShouldCastPhoenixFlames();
    bool ShouldUseCombustion();

    // Spell casting methods
    void CastFireball();
    void CastPyroblast();
    void CastFireBlast();
    void CastPhoenixFlames();
    void CastScorch();
    void CastFlamestrike();
    void CastDragonsBreath();
    void CastCombustion();
    void CastMirrorImage();

    // DoT management
    void UpdateDoTs();
    void ApplyIgnite(::Unit* target);
    void CastLivingBomb(::Unit* target);
    bool HasIgnite(::Unit* target);
    bool HasLivingBomb(::Unit* target);
    uint32 GetIgniteTimeRemaining(::Unit* target);

    // Enhanced AoE and multi-target
    void HandleAoERotation(const std::vector<::Unit*>& targets);
    void OptimizeFlamestrikePlacement(const std::vector<::Unit*>& targets);
    void HandleDragonBreathTiming(const std::vector<::Unit*>& targets);
    void ManageAoEIgnites(const std::vector<::Unit*>& targets);
    void SpreadLivingBombs(const std::vector<::Unit*>& targets);
    Position CalculateOptimalFlamestrikePosition(const std::vector<::Unit*>& targets);
    std::vector<::Unit*> GetNearbyEnemies(float range = 8.0f);
    bool ShouldUseAoE();
    void CastMeteor();
    void CastBlastWave();

    // Cooldown management
    void UpdateFireCooldowns(uint32 diff);
    void CheckFireBuffs();
    bool HasCriticalMass();
    void UseCooldowns();

    // Hot Streak management
    void ProcessHotStreak();
    void ProcessHeatingUp();
    void CheckForInstantPyroblast();

    // Fire spell IDs
    enum FireSpells
    {
        FIREBALL = 133,
        PYROBLAST = 11366,
        FIRE_BLAST = 2136,
        PHOENIX_FLAMES = 257541,
        SCORCH = 2948,
        FLAMESTRIKE = 2120,
        DRAGONS_BREATH = 31661,
        COMBUSTION = 190319,
        MIRROR_IMAGE = 55342,
        IGNITE = 12846,
        LIVING_BOMB = 44457,
        METEOR = 153561,
        BLAST_WAVE = 157981,
        HOT_STREAK = 48108,
        HEATING_UP = 48107,
        CRITICAL_MASS = 117216
    };

    // Enhanced state tracking
    std::atomic<bool> _hasHotStreak{false};
    std::atomic<bool> _hasHeatingUp{false};
    uint32 _lastCritTime;
    uint32 _combustionEndTime;
    std::atomic<bool> _inCombustion{false};
    uint32 _lastPyroblastTime;
    uint32 _consecutiveCrits;
    bool _combustionPrepped;

    // Advanced combustion mechanics
    void OptimizeCombustionPhase(::Unit* target);
    void PrepareCombustionSetup(::Unit* target);
    void ExecuteCombustionRotation(::Unit* target);
    void HandleCombustionEmergency();
    bool IsOptimalCombustionTime();
    uint32 CalculateOptimalCombustionDuration();
    void StackIgniteForCombustion(::Unit* target);

    // Crit fishing and optimization
    void OptimizeCritFishing();
    void HandleHotStreakProc();
    void HandleHeatingUpProc();
    void ChainPyroblasts(::Unit* target);
    void OptimizeFireBlastTiming();

    // DoT and burn management
    void OptimizeIgniteStacking(::Unit* target);
    void ManageLivingBombSpread(const std::vector<::Unit*>& targets);
    void HandleIgniteSnapshot(::Unit* target);
    void UpdateIgniteTracking();

    // Performance metrics
    struct FireMetrics {
        std::atomic<uint32> totalPyroblasts{0};
        std::atomic<uint32> instantPyroblasts{0};
        std::atomic<uint32> hotStreakProcs{0};
        std::atomic<uint32> heatingUpProcs{0};
        std::atomic<uint32> combustionCasts{0};
        std::atomic<uint32> criticalHits{0};
        std::atomic<float> averageCritRate{0.0f};
        std::atomic<float> combustionEfficiency{0.0f};
        std::atomic<float> igniteUptime{0.0f};
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalPyroblasts = 0; instantPyroblasts = 0; hotStreakProcs = 0;
            heatingUpProcs = 0; combustionCasts = 0; criticalHits = 0;
            averageCritRate = 0.0f; combustionEfficiency = 0.0f; igniteUptime = 0.0f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    } _fireMetrics;

    // Combustion state tracking
    struct CombustionState {
        bool inCombustion{false};
        uint32 combustionStartTime{0};
        uint32 igniteStacksAtStart{0};
        uint32 damageDealtDuringCombustion{0};
        std::vector<::Unit*> combustionTargets;
        void Reset() {
            inCombustion = false; combustionStartTime = 0;
            igniteStacksAtStart = 0; damageDealtDuringCombustion = 0;
            combustionTargets.clear();
        }
    } _combustionState;

    // Enhanced DoT tracking system
    struct DotTracker {
        std::unordered_map<ObjectGuid, uint32> igniteExpireTimes;
        std::unordered_map<ObjectGuid, uint32> livingBombExpireTimes;
        std::unordered_map<ObjectGuid, uint32> igniteStacks;
        void UpdateIgnite(ObjectGuid guid, uint32 duration, uint32 stacks) {
            igniteExpireTimes[guid] = getMSTime() + duration;
            igniteStacks[guid] = stacks;
        }
        void UpdateLivingBomb(ObjectGuid guid, uint32 duration) {
            livingBombExpireTimes[guid] = getMSTime() + duration;
        }
        bool HasIgnite(ObjectGuid guid) const {
            auto it = igniteExpireTimes.find(guid);
            return it != igniteExpireTimes.end() && it->second > getMSTime();
        }
        bool HasLivingBomb(ObjectGuid guid) const {
            auto it = livingBombExpireTimes.find(guid);
            return it != livingBombExpireTimes.end() && it->second > getMSTime();
        }
        uint32 GetIgniteStacks(ObjectGuid guid) const {
            auto it = igniteStacks.find(guid);
            return it != igniteStacks.end() ? it->second : 0;
        }
    } _dotTracker;

    std::map<uint64, uint32> _igniteTimers; // Legacy support
    std::map<uint64, uint32> _livingBombTimers; // Legacy support

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastDoTCheck;
    uint32 _lastAoECheck;
    uint32 _lastBuffCheck;
    uint32 _lastRotationUpdate;

    // Multi-target tracking
    std::vector<uint64> _nearbyEnemies;
    uint32 _lastEnemyScan;

    // Enhanced constants
    static constexpr uint32 HOT_STREAK_DURATION = 15000; // 15 seconds (enhanced)
    static constexpr uint32 HEATING_UP_DURATION = 10000; // 10 seconds
    static constexpr uint32 COMBUSTION_DURATION = 10000; // 10 seconds (optimized)
    static constexpr uint32 IGNITE_DURATION = 8000; // 8 seconds
    static constexpr uint32 LIVING_BOMB_DURATION = 12000; // 12 seconds
    static constexpr float AOE_THRESHOLD = 3.0f; // Use AoE with 3+ targets
    static constexpr float SCORCH_RANGE = 40.0f;
    static constexpr float FLAMESTRIKE_RANGE = 8.0f;
    static constexpr float CRIT_THRESHOLD = 0.6f; // 60% crit rate target
    static constexpr float COMBUSTION_CRIT_THRESHOLD = 0.8f; // 80% for combustion
    static constexpr float OPTIMAL_IGNITE_STACKS = 3.0f; // Optimal ignite stacks
    static constexpr uint32 MAX_IGNITE_STACKS = 5; // Maximum ignite stacks
    static constexpr uint32 PYROBLAST_CAST_TIME = 4500; // 4.5 seconds
    static constexpr uint32 FIREBALL_CAST_TIME = 3500; // 3.5 seconds
    static constexpr float COMBUSTION_SETUP_TIME = 5000; // 5 seconds setup
    static constexpr uint32 SCORCH_STACKS_MAX = 5; // Maximum scorch stacks
    static constexpr uint32 FIRE_BLAST_CHARGES = 3; // Maximum Fire Blast charges
    static constexpr uint32 PHOENIX_FLAMES_CHARGES = 3; // Maximum Phoenix Flames charges
};

} // namespace Playerbot