/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DruidSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API FeralDpsSpecialization : public DruidSpecialization
{
public:
    explicit FeralDpsSpecialization(Player* bot);

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

    // Form management
    void UpdateFormManagement() override;
    DruidForm GetOptimalFormForSituation() override;
    bool ShouldShiftToForm(DruidForm form) override;
    void ShiftToForm(DruidForm form) override;

    // DoT/HoT management
    void UpdateDotHotManagement() override;
    bool ShouldApplyDoT(::Unit* target, uint32 spellId) override;
    bool ShouldApplyHoT(::Unit* target, uint32 spellId) override;

    // Specialization info
    DruidSpec GetSpecialization() const override { return DruidSpec::FERAL_DPS; }
    const char* GetSpecializationName() const override { return "Feral DPS"; }

private:
    // Cat Form DPS mechanics
    void UpdateCatFormDPS();
    void UpdateComboPointSystem();
    void UpdateEnergyManagement();
    void UpdateCatBuffs();
    bool ShouldCastShred(::Unit* target);
    bool ShouldCastMangle(::Unit* target);
    bool ShouldCastRake(::Unit* target);
    bool ShouldCastRip(::Unit* target);
    bool ShouldCastFerociosBite(::Unit* target);
    bool ShouldCastSavageRoar();
    bool ShouldCastTigersFury();

    // Combo point optimization
    void OptimizeComboPointGeneration();
    void OptimizeFinisherTiming();
    void SpendComboPoints(::Unit* target);
    bool ShouldSpendComboPoints();
    uint32 GetOptimalFinisher(::Unit* target);
    void ExecuteFinisherRotation(::Unit* target);

    // Energy management
    bool HasEnoughEnergy(uint32 required);
    void ConsumeEnergy(uint32 amount);
    uint32 GetEnergy();
    float GetEnergyPercent();
    bool ShouldPoolEnergy();
    uint32 PredictEnergyInTime(uint32 milliseconds);
    void OptimizeEnergyUsage();

    // Cat form rotation abilities
    void CastShred(::Unit* target);
    void CastMangle(::Unit* target);
    void CastRake(::Unit* target);
    void CastRip(::Unit* target);
    void CastFerociosBite(::Unit* target);
    void CastSavageRoar();
    void CastTigersFury();
    void CastBerserk();

    // Stealth and positioning mechanics
    void ManageStealth();
    void CastProwl();
    bool ShouldUseStealth();
    void HandleStealthOpening(::Unit* target);
    void CastPounce(::Unit* target);
    void OptimizePositioningForDPS(::Unit* target);
    bool ShouldPositionBehindTarget(::Unit* target);

    // DPS optimization
    void OptimizeDPSRotation(::Unit* target);
    void HandleDPSCooldowns();
    void ManageBleedEffects();
    void OptimizeCriticalStrikes();
    void HandleFeralProcs();
    float CalculateDPSEfficiency();

    // Multi-target DPS
    void HandleMultiTargetDPS();
    void OptimizeSwipeUsage();
    bool ShouldUseSwipe();
    void CastSwipe();

    // Feral DPS spell IDs
    enum FeralDpsSpells
    {
        SHRED = 5221,
        MANGLE_CAT = 33876,
        RAKE = 1822,
        RIP = 1079,
        FEROCIOUS_BITE = 22568,
        SAVAGE_ROAR = 52610,
        TIGERS_FURY = 5217,
        BERSERK = 50334,
        DASH = 1850,
        PROWL = 5215,
        POUNCE = 9005,
        SWIPE_CAT = 62078,
        MAIM = 22570
    };

    // Enhanced combo point system
    std::atomic<uint32> _comboPoints{0};
    std::atomic<uint32> _lastComboPointGenerated{0};
    std::atomic<uint32> _lastComboPointSpent{0};
    std::atomic<bool> _clearcastingProc{false};
    std::atomic<uint32> _predatoryStrikesProc{0};
    std::atomic<bool> _bloodInTheWaterProc{false};
    std::atomic<bool> _suddenDeathProc{false};

    // Enhanced energy system
    std::atomic<uint32> _energy{100};
    std::atomic<uint32> _maxEnergy{100};
    std::atomic<uint32> _lastEnergyRegen{0};
    std::atomic<uint32> _energyRegenRate{10};
    std::atomic<float> _energyRegenModifier{1.0f};
    std::atomic<bool> _berserkActive{false};
    std::atomic<uint32> _berserkEndTime{0};

    // Performance metrics
    struct FeralDpsMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> comboPointsGenerated{0};
        std::atomic<uint32> comboPointsSpent{0};
        std::atomic<uint32> energySpent{0};
        std::atomic<uint32> shredCrits{0};
        std::atomic<uint32> ripTicks{0};
        std::atomic<uint32> ferociosBiteDamage{0};
        std::atomic<uint32> tigersFuryUses{0};
        std::atomic<uint32> berserkUses{0};
        std::atomic<float> energyEfficiency{0.0f};
        std::atomic<float> comboPointEfficiency{0.0f};
        std::atomic<float> savageRoarUptime{0.0f};
        std::atomic<float> ripUptime{0.0f};
        std::atomic<float> behindTargetPercentage{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; comboPointsGenerated = 0; comboPointsSpent = 0;
            energySpent = 0; shredCrits = 0; ripTicks = 0; ferociosBiteDamage = 0;
            tigersFuryUses = 0; berserkUses = 0; energyEfficiency = 0.0f;
            comboPointEfficiency = 0.0f; savageRoarUptime = 0.0f; ripUptime = 0.0f;
            behindTargetPercentage = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _feralDpsMetrics;

    // Advanced combo point tracking
    struct ComboPointManager {
        std::atomic<uint32> currentPoints{0};
        std::queue<uint32> pointHistory;
        mutable std::mutex historyMutex;
        std::atomic<uint32> wasted{0};
        std::atomic<uint32> optimal{0};
        void AddPoint() {
            uint32 current = currentPoints.load();
            if (current < COMBO_POINTS_MAX) {
                currentPoints++;
                optimal++;
                std::lock_guard<std::mutex> lock(historyMutex);
                pointHistory.push(getMSTime());
                if (pointHistory.size() > 10) // Keep last 10
                    pointHistory.pop();
            } else {
                wasted++;
            }
        }
        void SpendPoints(uint32 amount) {
            currentPoints = 0; // All points spent
        }
        uint32 GetPoints() const { return currentPoints.load(); }
        float GetEfficiency() const {
            uint32 total = optimal.load() + wasted.load();
            return total > 0 ? (float)optimal.load() / total : 1.0f;
        }
    } _comboPointManager;

    // DoT tracking system (Cat form bleeds)
    struct BleedTracker {
        std::unordered_map<uint64, uint32> rakeExpiry;
        std::unordered_map<uint64, uint32> ripExpiry;
        std::unordered_map<uint64, uint32> mangledExpiry;
        mutable std::mutex bleedMutex;
        void UpdateBleed(uint64 targetGuid, uint32 spellId, uint32 duration) {
            std::lock_guard<std::mutex> lock(bleedMutex);
            uint32 expiry = getMSTime() + duration;
            switch (spellId) {
                case RAKE: rakeExpiry[targetGuid] = expiry; break;
                case RIP: ripExpiry[targetGuid] = expiry; break;
                case MANGLE_CAT: mangledExpiry[targetGuid] = expiry; break;
            }
        }
        bool HasBleed(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(bleedMutex);
            uint32 currentTime = getMSTime();
            switch (spellId) {
                case RAKE: {
                    auto it = rakeExpiry.find(targetGuid);
                    return it != rakeExpiry.end() && it->second > currentTime;
                }
                case RIP: {
                    auto it = ripExpiry.find(targetGuid);
                    return it != ripExpiry.end() && it->second > currentTime;
                }
                case MANGLE_CAT: {
                    auto it = mangledExpiry.find(targetGuid);
                    return it != mangledExpiry.end() && it->second > currentTime;
                }
            }
            return false;
        }
        uint32 GetTimeRemaining(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(bleedMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = 0;
            switch (spellId) {
                case RAKE: {
                    auto it = rakeExpiry.find(targetGuid);
                    expiry = it != rakeExpiry.end() ? it->second : 0;
                    break;
                }
                case RIP: {
                    auto it = ripExpiry.find(targetGuid);
                    expiry = it != ripExpiry.end() ? it->second : 0;
                    break;
                }
                case MANGLE_CAT: {
                    auto it = mangledExpiry.find(targetGuid);
                    expiry = it != mangledExpiry.end() ? it->second : 0;
                    break;
                }
            }
            return expiry > currentTime ? expiry - currentTime : 0;
        }
    } _bleedTracker;

    // Cat form buff tracking
    uint32 _lastTigersFury;
    uint32 _lastSavageRoar;
    uint32 _lastBerserk;
    std::atomic<bool> _prowlActive{false};
    std::atomic<bool> _dashActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced DPS mechanics
    void OptimizeOpenerSequence(::Unit* target);
    void ManageFerociousBiteExcess();
    void HandleExecutePhase(::Unit* target);
    void OptimizeBleedDamage();
    void ManageEnergyPrediction();
    void HandleBerserkPhase();
    void OptimizeStealthWindows();
    void ManageSwipeThreshold();
    float CalculateBleedValue(::Unit* target, uint32 spellId);
    bool ShouldRefreshBleed(::Unit* target, uint32 spellId);
    void PrioritizeDPSTargets();

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 COMBO_POINTS_MAX = 5;
    static constexpr uint32 ENERGY_MAX = 100;
    static constexpr uint32 ENERGY_REGEN_RATE = 10; // per second
    static constexpr uint32 TIGERS_FURY_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SAVAGE_ROAR_DURATION = 34000; // 34 seconds at max combo points
    static constexpr uint32 RAKE_DURATION = 15000; // 15 seconds
    static constexpr uint32 RIP_DURATION = 22000; // 22 seconds at max combo points
    static constexpr uint32 BERSERK_DURATION = 15000; // 15 seconds
    static constexpr uint32 SHRED_ENERGY_COST = 60;
    static constexpr uint32 MANGLE_ENERGY_COST = 45;
    static constexpr uint32 RAKE_ENERGY_COST = 35;
    static constexpr uint32 RIP_ENERGY_COST = 30;
    static constexpr uint32 FEROCIOUS_BITE_ENERGY_COST = 25;
    static constexpr uint32 SAVAGE_ROAR_ENERGY_COST = 25;
    static constexpr uint32 SWIPE_ENERGY_COST = 50;
    static constexpr float ENERGY_POOLING_THRESHOLD = 80.0f;
    static constexpr float OPTIMAL_COMBO_POINT_USAGE = 4.5f; // 4-5 points
    static constexpr uint32 PROWL_ENERGY_BONUS = 60;
    static constexpr float BEHIND_TARGET_BONUS = 1.5f; // 50% more damage from behind
    static constexpr uint32 STEALTH_OPPORTUNITY_WINDOW = 6000; // 6 seconds
    static constexpr float BLEED_PANDEMIC_THRESHOLD = 0.3f; // 30% for pandemic refresh
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 25.0f; // Execute phase below 25%
    static constexpr uint32 MULTI_TARGET_SWIPE_COUNT = 3; // 3+ targets for Swipe
    static constexpr float DPS_EFFICIENCY_TARGET = 150.0f; // DPS per energy target
};

} // namespace Playerbot