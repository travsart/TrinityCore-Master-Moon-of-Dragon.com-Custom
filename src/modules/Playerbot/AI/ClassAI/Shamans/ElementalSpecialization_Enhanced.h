/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ShamanSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API ElementalSpecialization : public ShamanSpecialization
{
public:
    explicit ElementalSpecialization(Player* bot);

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

    // Totem management
    void UpdateTotemManagement() override;
    void DeployOptimalTotems() override;
    uint32 GetOptimalFireTotem() override;
    uint32 GetOptimalEarthTotem() override;
    uint32 GetOptimalWaterTotem() override;
    uint32 GetOptimalAirTotem() override;

    // Shock rotation
    void UpdateShockRotation(::Unit* target) override;
    uint32 GetNextShockSpell(::Unit* target) override;

    // Specialization info
    ShamanSpec GetSpecialization() const override { return ShamanSpec::ELEMENTAL; }
    const char* GetSpecializationName() const override { return "Elemental"; }

private:
    // Elemental-specific mechanics
    void UpdateElementalMechanics();
    void UpdateCastingRotation(::Unit* target);
    void UpdateElementalShields();
    void UpdateLightningBolt(::Unit* target);
    void UpdateChainLightning();
    void UpdateLavaBurst(::Unit* target);
    void UpdateElementalMastery();
    void UpdateThunderstorm();
    void UpdateElementalFocus();
    bool ShouldCastLightningBolt(::Unit* target);
    bool ShouldCastChainLightning();
    bool ShouldCastLavaBurst(::Unit* target);
    bool ShouldCastElementalBlast(::Unit* target);
    bool ShouldCastEarthquake();
    bool ShouldCastThunderstorm();
    bool ShouldCastElementalMastery();

    // Spell power and elemental focus
    void ManageElementalFocus();
    void TriggerElementalFocus();
    bool HasElementalFocus();
    void ConsumeElementalFocus();
    void OptimizeElementalFocusUsage();
    float GetElementalFocusBonus();

    // Lightning shield management
    void ManageLightningShield();
    void CastLightningShield();
    uint32 GetLightningShieldCharges();
    bool ShouldRefreshLightningShield();
    void OptimizeLightningShieldUsage();

    // Lava burst and flame shock synergy
    void ManageLavaBurstCombo();
    void CastLavaBurst(::Unit* target);
    bool CanLavaBurstCrit(::Unit* target);
    void OptimizeLavaBurstTiming(::Unit* target);
    void MaintainFlameShockForLavaBurst(::Unit* target);

    // Chain lightning optimization
    void ManageChainLightning();
    void CastChainLightning();
    void OptimizeChainLightningTargets();
    bool ShouldUseChainLightning();
    uint32 GetChainLightningTargetCount();

    // Elemental overload mechanics
    void ManageElementalOverload();
    void TriggerElementalOverload();
    bool HasElementalOverloadProc();
    void OptimizeOverloadProcs();
    float GetOverloadProcChance();

    // Earthquake management
    void ManageEarthquake();
    void CastEarthquake();
    Position GetOptimalEarthquakePosition();
    bool ShouldUseEarthquake();
    void OptimizeEarthquakeUsage();

    // Thunderstorm mechanics
    void ManageThunderstorm();
    void CastThunderstorm();
    bool ShouldUseThunderstormForDamage();
    bool ShouldUseThunderstormForMana();
    bool ShouldUseThunderstormForKnockback();

    // Elemental mastery cooldown
    void ManageElementalMastery();
    void CastElementalMastery();
    bool HasElementalMastery();
    void OptimizeElementalMasteryUsage();
    uint32 GetBestSpellForElementalMastery(::Unit* target);

    // Fire nova mechanics
    void ManageFireNova();
    void CastFireNova();
    bool ShouldUseFireNova();
    void OptimizeFireNovaChaining();

    // Mana management for caster
    void OptimizeElementalMana();
    void UseThunderstormForMana();
    void ManageManaEfficiency();
    bool ShouldConserveManaForBurst();
    float CalculateManaPerSecond();

    // Multi-target elemental DPS
    void HandleMultiTargetElemental();
    void OptimizeAoESpells();
    void CastChainLightningAoE();
    bool ShouldUseAoERotation();
    void HandleAoETotemPlacement();

    // Positioning for ranged caster
    void OptimizeElementalPositioning();
    void MaintainRangedPosition(::Unit* target);
    void HandleCasterMovement();
    bool ShouldMoveForOptimalRange(::Unit* target);
    void AvoidMeleeRange();

    // Elemental spell IDs
    enum ElementalSpells
    {
        LIGHTNING_BOLT = 403,
        CHAIN_LIGHTNING = 421,
        LAVA_BURST = 51505,
        ELEMENTAL_BLAST = 117014,
        EARTHQUAKE = 61882,
        THUNDERSTORM = 51490,
        ELEMENTAL_MASTERY = 16166,
        FIRE_NOVA = 1535,
        LIGHTNING_SHIELD = 324,
        WATER_SHIELD = 52127,
        ELEMENTAL_FOCUS = 16164,
        ELEMENTAL_FURY = 60188,
        ELEMENTAL_PRECISION = 30672,
        CONVECTION = 16039,
        CONCUSSION = 16035,
        CALL_OF_FLAME = 16038,
        ELEMENTAL_DEVASTATION = 30160,
        RESTLESS_TOTEMS = 16223,
        STORM_EARTH_AND_FIRE = 51483,
        LAVA_FLOWS = 51480,
        SHAMANISM = 62099,
        ELEMENTAL_OATH = 51466,
        LIGHTNING_OVERLOAD = 30675,
        FULMINATION = 88766,
        FEEDBACK = 86332,
        ROLLING_THUNDER = 88764,
        EARTHQUAKE_TOTEM = 61882,
        SPIRITWALKERS_GRACE = 79206,
        UNLEASH_ELEMENTS = 73680,
        PRIMAL_ELEMENTALIST = 117013
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<bool> _elementalFocusActive{false};
    std::atomic<uint32> _elementalFocusEndTime{0};
    std::atomic<bool> _elementalMasteryActive{false};
    std::atomic<uint32> _elementalMasteryEndTime{0};
    std::atomic<uint32> _lightningShieldCharges{0};
    std::atomic<bool> _elementalOverloadProc{false};

    // Performance metrics
    struct ElementalMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> lightningBoltCasts{0};
        std::atomic<uint32> chainLightningCasts{0};
        std::atomic<uint32> lavaBurstCasts{0};
        std::atomic<uint32> lavaBurstCrits{0};
        std::atomic<uint32> overloadProcs{0};
        std::atomic<uint32> thunderstormCasts{0};
        std::atomic<uint32> earthquakeCasts{0};
        std::atomic<uint32> elementalMasteryUses{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> manaRegained{0};
        std::atomic<float> castingEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> overloadProcRate{0.0f};
        std::atomic<float> lavaBurstCritRate{0.0f};
        std::atomic<float> lightningShieldUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; lightningBoltCasts = 0; chainLightningCasts = 0;
            lavaBurstCasts = 0; lavaBurstCrits = 0; overloadProcs = 0;
            thunderstormCasts = 0; earthquakeCasts = 0; elementalMasteryUses = 0;
            manaSpent = 0; manaRegained = 0; castingEfficiency = 0.0f;
            manaEfficiency = 0.0f; overloadProcRate = 0.0f; lavaBurstCritRate = 0.0f;
            lightningShieldUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _elementalMetrics;

    // Advanced casting sequence optimizer
    struct CastSequenceOptimizer {
        std::queue<uint32> castingQueue;
        std::atomic<bool> isCasting{false};
        std::atomic<uint32> currentCastSpell{0};
        std::atomic<uint32> castStartTime{0};
        std::atomic<uint32> castEndTime{0};
        mutable std::mutex queueMutex;
        void QueueSpell(uint32 spellId, uint32 castTime) {
            std::lock_guard<std::mutex> lock(queueMutex);
            castingQueue.push(spellId);
        }
        void StartCast(uint32 spellId, uint32 castTime) {
            isCasting = true;
            currentCastSpell = spellId;
            castStartTime = getMSTime();
            castEndTime = getMSTime() + castTime;
        }
        void FinishCast() {
            isCasting = false;
            currentCastSpell = 0;
            castStartTime = 0;
            castEndTime = 0;
        }
        bool IsCasting() const { return isCasting.load(); }
        uint32 GetRemainingCastTime() const {
            if (!IsCasting()) return 0;
            uint32 currentTime = getMSTime();
            uint32 endTime = castEndTime.load();
            return endTime > currentTime ? endTime - currentTime : 0;
        }
        uint32 GetNextSpell() {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!castingQueue.empty()) {
                uint32 spell = castingQueue.front();
                castingQueue.pop();
                return spell;
            }
            return 0;
        }
        bool HasQueuedSpells() const {
            std::lock_guard<std::mutex> lock(queueMutex);
            return !castingQueue.empty();
        }
    } _castSequenceOptimizer;

    // Flame shock DoT tracker
    struct FlameShockTracker {
        std::unordered_map<uint64, uint32> flameShockExpiry;
        mutable std::mutex flameShockMutex;
        void UpdateFlameShock(uint64 targetGuid, uint32 duration) {
            std::lock_guard<std::mutex> lock(flameShockMutex);
            flameShockExpiry[targetGuid] = getMSTime() + duration;
        }
        bool HasFlameShock(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(flameShockMutex);
            auto it = flameShockExpiry.find(targetGuid);
            return it != flameShockExpiry.end() && it->second > getMSTime();
        }
        uint32 GetFlameShockTimeRemaining(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(flameShockMutex);
            auto it = flameShockExpiry.find(targetGuid);
            if (it != flameShockExpiry.end()) {
                uint32 currentTime = getMSTime();
                return it->second > currentTime ? it->second - currentTime : 0;
            }
            return 0;
        }
        bool ShouldRefreshFlameShock(uint64 targetGuid, uint32 refreshThreshold = 3000) const {
            return GetFlameShockTimeRemaining(targetGuid) <= refreshThreshold;
        }
    } _flameShockTracker;

    // Totem effectiveness tracker
    struct TotemEffectivenessTracker {
        std::unordered_map<TotemType, uint32> totemDamage;
        std::unordered_map<TotemType, uint32> totemHealing;
        std::unordered_map<TotemType, uint32> totemManaProvided;
        mutable std::mutex effectivenessMutex;
        void RecordTotemDamage(TotemType type, uint32 damage) {
            std::lock_guard<std::mutex> lock(effectivenessMutex);
            totemDamage[type] += damage;
        }
        void RecordTotemHealing(TotemType type, uint32 healing) {
            std::lock_guard<std::mutex> lock(effectivenessMutex);
            totemHealing[type] += healing;
        }
        void RecordTotemMana(TotemType type, uint32 mana) {
            std::lock_guard<std::mutex> lock(effectivenessMutex);
            totemManaProvided[type] += mana;
        }
        float GetTotemEffectiveness(TotemType type) const {
            std::lock_guard<std::mutex> lock(effectivenessMutex);
            uint32 totalContribution = 0;

            auto damageIt = totemDamage.find(type);
            if (damageIt != totemDamage.end())
                totalContribution += damageIt->second;

            auto healingIt = totemHealing.find(type);
            if (healingIt != totemHealing.end())
                totalContribution += healingIt->second;

            auto manaIt = totemManaProvided.find(type);
            if (manaIt != totemManaProvided.end())
                totalContribution += manaIt->second * 2; // Weight mana higher

            return (float)totalContribution;
        }
    } _totemEffectivenessTracker;

    // Elemental buff tracking
    uint32 _lastElementalMastery;
    uint32 _lastThunderstorm;
    uint32 _lastLightningShield;
    uint32 _lastEarthquake;
    std::atomic<bool> _spiritwalkerGraceActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Elemental mechanics
    void OptimizeElementalRotation(::Unit* target);
    void HandleElementalCooldowns();
    void ManageCastingEfficiency();
    void OptimizeOverloadChaining();
    void HandleLavaBurstWindows();
    void ManageThunderstormTiming();
    float CalculateElementalDPS();

    // Enhanced constants
    static const float RANGED_POSITION = 30.0f;
    static const uint32 LIGHTNING_BOLT_CAST_TIME = 2500; // 2.5 seconds
    static const uint32 CHAIN_LIGHTNING_CAST_TIME = 2000; // 2 seconds
    static const uint32 LAVA_BURST_CAST_TIME = 2000; // 2 seconds
    static const uint32 ELEMENTAL_MASTERY_COOLDOWN = 180000; // 3 minutes
    static const uint32 THUNDERSTORM_COOLDOWN = 45000; // 45 seconds
    static const uint32 EARTHQUAKE_DURATION = 10000; // 10 seconds
    static const uint32 FLAME_SHOCK_DURATION = 30000; // 30 seconds
    static const uint32 LIGHTNING_SHIELD_DURATION = 600000; // 10 minutes
    static const uint32 ELEMENTAL_FOCUS_DURATION = 10000; // 10 seconds
    static const uint32 LIGHTNING_BOLT_MANA_COST = 400;
    static const uint32 CHAIN_LIGHTNING_MANA_COST = 800;
    static const uint32 LAVA_BURST_MANA_COST = 500;
    static const uint32 EARTHQUAKE_MANA_COST = 1200;
    static const uint32 THUNDERSTORM_MANA_COST = 800;
    static const uint32 LIGHTNING_SHIELD_MAX_CHARGES = 9;
    static const float OVERLOAD_PROC_CHANCE = 0.30f; // 30% base proc chance
    static const uint32 MULTI_TARGET_THRESHOLD = 3; // 3+ targets for Chain Lightning
    static const float ELEMENTAL_MANA_THRESHOLD = 25.0f; // Conservative mana usage below 25%
    static const uint32 LAVA_BURST_COOLDOWN = 8000; // 8 seconds
    static const float FLAME_SHOCK_REFRESH_THRESHOLD = 3.0f; // Refresh with 3s remaining
    static const uint32 TOTEM_PLACEMENT_RANGE = 40.0f; // Optimal totem placement range
};

} // namespace Playerbot