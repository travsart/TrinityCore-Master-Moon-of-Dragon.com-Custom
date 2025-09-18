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

class TC_GAME_API FeralSpecialization : public DruidSpecialization
{
public:
    explicit FeralSpecialization(Player* bot);

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
    DruidSpec GetSpecialization() const override { return DruidSpec::FERAL; }
    const char* GetSpecializationName() const override { return "Feral"; }

private:
    // Feral-specific mechanics
    void UpdateComboPointSystem();
    void UpdateEnergyManagement();
    void UpdateFeralBuffs();
    bool ShouldCastShred(::Unit* target);
    bool ShouldCastMangle(::Unit* target);
    bool ShouldCastRake(::Unit* target);
    bool ShouldCastRip(::Unit* target);
    bool ShouldCastFerociosBite(::Unit* target);
    bool ShouldCastSavageRoar();
    bool ShouldCastTigersFury();

    // Combo point management
    void GenerateComboPoint(::Unit* target);
    void SpendComboPoints(::Unit* target);
    bool ShouldSpendComboPoints();
    void OptimizeComboPointUsage();

    // Energy management
    bool HasEnoughEnergy(uint32 required);
    void SpendEnergy(uint32 amount);
    uint32 GetEnergy();
    float GetEnergyPercent();

    // Feral rotation abilities
    void CastShred(::Unit* target);
    void CastMangle(::Unit* target);
    void CastRake(::Unit* target);
    void CastRip(::Unit* target);
    void CastFerociosBite(::Unit* target);
    void CastSavageRoar();
    void CastTigersFury();

    // Cat form management
    void EnterCatForm();
    bool ShouldUseCatForm();

    // Stealth management
    void ManageStealth();
    void CastProwl();
    bool ShouldUseStealth();

    // Feral spell IDs
    enum FeralSpells
    {
        SHRED = 5221,
        MANGLE_CAT = 33876,
        RAKE = 1822,
        RIP = 1079,
        FEROCIOUS_BITE = 22568,
        SAVAGE_ROAR = 52610,
        TIGERS_FURY = 5217,
        DASH = 1850,
        PROWL = 5215,
        POUNCE = 9005
    };

    // Enhanced combo point system
    std::atomic<uint32> _comboPoints{0};
    std::atomic<uint32> _lastComboPointGenerated{0};
    std::atomic<uint32> _lastComboPointSpent{0};
    std::atomic<bool> _clearcastingProc{false};
    std::atomic<uint32> _predatoryStrikesProc{0};
    std::atomic<bool> _bloodInTheWaterProc{false};

    // Enhanced energy system
    std::atomic<uint32> _energy{100};
    std::atomic<uint32> _maxEnergy{100};
    std::atomic<uint32> _lastEnergyRegen{0};
    std::atomic<uint32> _energyRegenRate{10};
    std::atomic<float> _energyRegenModifier{1.0f};
    std::atomic<bool> _berserkActive{false};
    std::atomic<uint32> _berserkEndTime{0};

    // Feral buffs and debuffs
    uint32 _tigersFuryReady;
    uint32 _savageRoarRemaining;
    uint32 _lastTigersFury;
    uint32 _lastSavageRoar;

    // DoT tracking
    std::unordered_map<ObjectGuid, uint32> _rakeTimers;
    std::unordered_map<ObjectGuid, uint32> _ripTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance metrics
    struct FeralMetrics {
        std::atomic<uint32> totalMeleeDamage{0};
        std::atomic<uint32> comboPointsGenerated{0};
        std::atomic<uint32> comboPointsSpent{0};
        std::atomic<uint32> energySpent{0};
        std::atomic<uint32> shredCrits{0};
        std::atomic<uint32> ripTicks{0};
        std::atomic<uint32> ferociosBiteDamage{0};
        std::atomic<uint32> tigersFuryUses{0};
        std::atomic<float> energyEfficiency{0.0f};
        std::atomic<float> comboPointEfficiency{0.0f};
        std::atomic<float> savageRoarUptime{0.0f};
        std::atomic<float> ripUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalMeleeDamage = 0; comboPointsGenerated = 0; comboPointsSpent = 0;
            energySpent = 0; shredCrits = 0; ripTicks = 0; ferociosBiteDamage = 0;
            tigersFuryUses = 0; energyEfficiency = 0.0f; comboPointEfficiency = 0.0f;
            savageRoarUptime = 0.0f; ripUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _feralMetrics;

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

    // Advanced Feral mechanics
    void OptimizeComboPointGeneration();
    void ManageEnergyEfficiently();
    void OptimizeFinisherTiming();
    void HandleFeralProcs();
    void ManageBerserkTiming();
    void OptimizePositioningFeral();
    void HandleStealthOpportunities();
    void ManageFeralCooldowns();
    void OptimizeBleedManagement();
    void HandleClearcastingProc();
    void ManagePredatoryStrikes();
    void OptimizeSavageRoarUptime();
    void HandleBloodInTheWater();
    void OptimizeFerociosBite();
    void ManageFeralRotationPriority();
    void HandleMultiTargetFeral();
    void OptimizeEnergyPrediction();
    void ManageFeralBuffs();
    void OptimizeCatFormUptime();
    void HandleFeralInterrupts();
    float CalculateFeralDPS();
    bool ShouldPoolEnergy();
    uint32 PredictEnergyInTime(uint32 milliseconds);

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 COMBO_POINTS_MAX = 5;
    static constexpr uint32 ENERGY_MAX = 100;
    static constexpr uint32 ENERGY_REGEN_RATE = 10; // per second
    static constexpr uint32 TIGERS_FURY_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SAVAGE_ROAR_DURATION = 34000; // 34 seconds at max combo points
    static constexpr uint32 RAKE_DURATION = 15000; // 15 seconds
    static constexpr uint32 RIP_DURATION = 22000; // 22 seconds at max combo points
    static constexpr uint32 BERSERK = 50334; // Add missing spell IDs
    static constexpr uint32 BERSERK_DURATION = 15000; // 15 seconds
    static constexpr uint32 CLEARCASTING = 135700; // Clearcasting proc
    static constexpr uint32 PREDATORY_STRIKES = 16972;
    static constexpr uint32 BLOOD_IN_THE_WATER = 80318;
    static constexpr uint32 SHRED_ENERGY_COST = 60;
    static constexpr uint32 MANGLE_ENERGY_COST = 45;
    static constexpr uint32 RAKE_ENERGY_COST = 35;
    static constexpr uint32 RIP_ENERGY_COST = 30;
    static constexpr uint32 FEROCIOUS_BITE_ENERGY_COST = 25;
    static constexpr uint32 SAVAGE_ROAR_ENERGY_COST = 25;
    static constexpr uint32 TIGERS_FURY_ENERGY_COST = 0;
    static constexpr float ENERGY_POOLING_THRESHOLD = 80.0f;
    static constexpr float OPTIMAL_COMBO_POINT_USAGE = 4.5f; // 4-5 points
    static constexpr uint32 PROWL_ENERGY_BONUS = 60;
    static constexpr float BEHIND_TARGET_BONUS = 1.5f; // 50% more damage from behind
    static constexpr uint32 STEALTH_OPPORTUNITY_WINDOW = 6000; // 6 seconds
};

} // namespace Playerbot