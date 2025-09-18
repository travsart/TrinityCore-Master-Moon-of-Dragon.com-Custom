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

class TC_GAME_API BalanceSpecialization : public DruidSpecialization
{
public:
    explicit BalanceSpecialization(Player* bot);

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
    DruidSpec GetSpecialization() const override { return DruidSpec::BALANCE; }
    const char* GetSpecializationName() const override { return "Balance"; }

private:
    // Balance-specific mechanics
    void UpdateEclipseSystem();
    void UpdateStarsurge();
    bool ShouldCastStarfire(::Unit* target);
    bool ShouldCastWrath(::Unit* target);
    bool ShouldCastStarsurge(::Unit* target);
    bool ShouldCastMoonfire(::Unit* target);

    // Eclipse management
    void AdvanceEclipse();
    void CastEclipseSpell(::Unit* target);
    void ManageEclipseState();

    // Balance spell rotation
    void CastStarfire(::Unit* target);
    void CastWrath(::Unit* target);
    void CastStarsurge(::Unit* target);
    void CastMoonfire(::Unit* target);
    void CastForceOfNature();

    // Moonkin form management
    void EnterMoonkinForm();
    bool ShouldUseMoonkinForm();

    // Balance spell IDs
    enum BalanceSpells
    {
        STARFIRE = 2912,
        WRATH = 5176,
        STARSURGE = 78674,
        FORCE_OF_NATURE = 33831,
        ECLIPSE_SOLAR = 48517,
        ECLIPSE_LUNAR = 48518,
        SUNFIRE = 93402
    };

    // Enhanced Eclipse state tracking
    std::atomic<EclipseState> _eclipseState{EclipseState::NONE};
    std::atomic<uint32> _solarEnergy{0};
    std::atomic<uint32> _lunarEnergy{0};
    std::atomic<uint32> _lastEclipseShift{0};
    std::atomic<uint32> _starfireCount{0};
    std::atomic<uint32> _wrathCount{0};
    std::atomic<bool> _eclipseActive{false};
    std::atomic<bool> _shootingStarsProc{false};
    std::atomic<uint32> _eclipseDirection{0}; // 0=neutral, 1=solar, 2=lunar
    std::atomic<bool> _euphoriaTalent{false};
    std::atomic<uint32> _naturesGraceStacks{0};

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance metrics
    struct BalanceMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> eclipseProcs{0};
        std::atomic<uint32> starfireCasts{0};
        std::atomic<uint32> wrathCasts{0};
        std::atomic<uint32> starsurgeCasts{0};
        std::atomic<uint32> moonfireApplications{0};
        std::atomic<uint32> sunfireApplications{0};
        std::atomic<float> eclipseUptime{0.0f};
        std::atomic<float> solarEclipseUptime{0.0f};
        std::atomic<float> lunarEclipseUptime{0.0f};
        std::atomic<float> castEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        std::chrono::steady_clock::time_point eclipseStartTime;
        void Reset() {
            totalDamageDealt = 0; manaSpent = 0; eclipseProcs = 0;
            starfireCasts = 0; wrathCasts = 0; starsurgeCasts = 0;
            moonfireApplications = 0; sunfireApplications = 0;
            eclipseUptime = 0.0f; solarEclipseUptime = 0.0f; lunarEclipseUptime = 0.0f;
            castEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
            eclipseStartTime = combatStartTime;
        }
    } _balanceMetrics;

    // Eclipse optimization system
    struct EclipseOptimizer {
        std::queue<uint32> energyHistory;
        uint32 averageEclipseLength{0};
        uint32 totalEclipses{0};
        float optimalEclipseRatio{0.5f}; // 50% solar, 50% lunar
        void RecordEclipse(EclipseState state, uint32 duration) {
            energyHistory.push(duration);
            if (energyHistory.size() > 5) // Keep last 5 eclipses
                energyHistory.pop();
            totalEclipses++;
            CalculateOptimalRatio();
        }
        void CalculateOptimalRatio() {
            // Complex calculation based on fight dynamics
            // This would be expanded with actual fight analysis
            optimalEclipseRatio = 0.5f; // Simplified for now
        }
        EclipseState GetRecommendedNextEclipse() const {
            return optimalEclipseRatio > 0.5f ? EclipseState::SOLAR : EclipseState::LUNAR;
        }
    } _eclipseOptimizer;

    // DoT tracking system
    struct DoTTracker {
        std::unordered_map<uint64, uint32> moonfireExpiry;
        std::unordered_map<uint64, uint32> sunfireExpiry;
        std::unordered_map<uint64, uint32> insectSwarmExpiry;
        mutable std::mutex dotMutex;
        void UpdateDoT(uint64 targetGuid, uint32 spellId, uint32 duration) {
            std::lock_guard<std::mutex> lock(dotMutex);
            uint32 expiry = getMSTime() + duration;
            switch (spellId) {
                case MOONFIRE: moonfireExpiry[targetGuid] = expiry; break;
                case SUNFIRE: sunfireExpiry[targetGuid] = expiry; break;
                case INSECT_SWARM: insectSwarmExpiry[targetGuid] = expiry; break;
            }
        }
        bool HasDoT(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(dotMutex);
            uint32 currentTime = getMSTime();
            switch (spellId) {
                case MOONFIRE: {
                    auto it = moonfireExpiry.find(targetGuid);
                    return it != moonfireExpiry.end() && it->second > currentTime;
                }
                case SUNFIRE: {
                    auto it = sunfireExpiry.find(targetGuid);
                    return it != sunfireExpiry.end() && it->second > currentTime;
                }
                case INSECT_SWARM: {
                    auto it = insectSwarmExpiry.find(targetGuid);
                    return it != insectSwarmExpiry.end() && it->second > currentTime;
                }
            }
            return false;
        }
        uint32 GetTimeRemaining(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(dotMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = 0;
            switch (spellId) {
                case MOONFIRE: {
                    auto it = moonfireExpiry.find(targetGuid);
                    expiry = it != moonfireExpiry.end() ? it->second : 0;
                    break;
                }
                case SUNFIRE: {
                    auto it = sunfireExpiry.find(targetGuid);
                    expiry = it != sunfireExpiry.end() ? it->second : 0;
                    break;
                }
            }
            return expiry > currentTime ? expiry - currentTime : 0;
        }
    } _dotTracker;

    // Advanced Balance mechanics
    void OptimizeEclipseRotation();
    void ManageEclipseTransitions();
    void OptimizeCastSequencing();
    void HandleShootingStarsProc();
    void ManageNaturesGraceStacks();
    void OptimizeDoTApplication();
    void HandleMultiTargetBalance();
    void ManageEuphoriaProc();
    void OptimizeStarsurgeUsage();
    void HandleSolarEclipse();
    void HandleLunarEclipse();
    void ManageEclipseBuffs();
    void OptimizeMoonkinFormUsage();
    void HandleForceOfNatureTiming();
    void ManageBalanceCooldowns();
    void OptimizeManaEfficiencyBalance();
    void HandleMultiDoTManagement();
    void PredictEclipseTransition();
    float CalculateEclipseEfficiency();
    bool ShouldExtendEclipse();
    void OptimizeSpellPowerUsage();
    void HandleWildMushroomPlacement();

    // Enhanced constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 ECLIPSE_ENERGY_MAX = 100;
    static constexpr uint32 STARSURGE_COOLDOWN = 15000; // 15 seconds
    static constexpr uint32 FORCE_OF_NATURE_COOLDOWN = 180000; // 3 minutes
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
    static constexpr uint32 MOONFIRE = 8921; // Add missing spell IDs
    static constexpr uint32 INSECT_SWARM = 5570;
    static constexpr uint32 WILD_MUSHROOM = 88747;
    static constexpr uint32 MOONFIRE_DURATION = 12000; // 12 seconds
    static constexpr uint32 SUNFIRE_DURATION = 12000; // 12 seconds
    static constexpr uint32 INSECT_SWARM_DURATION = 12000; // 12 seconds
    static constexpr uint32 ECLIPSE_DURATION = 15000; // 15 seconds
    static constexpr uint32 NATURE_GRACE_MAX_STACKS = 3;
    static constexpr uint32 NATURE_GRACE_DURATION = 15000; // 15 seconds
    static constexpr float ECLIPSE_ENERGY_PER_CAST = 20.0f;
    static constexpr float SHOOTING_STARS_PROC_CHANCE = 0.3f; // 30%
    static constexpr uint32 EUPHORIA_ENERGY_BONUS = 20;
    static constexpr float DOT_PANDEMIC_THRESHOLD = 0.3f; // 30% for pandemic
    static constexpr uint32 ECLIPSE_OPTIMIZATION_WINDOW = 5000; // 5 second window
    static constexpr float OPTIMAL_ECLIPSE_UPTIME = 0.8f; // 80% uptime target
};

} // namespace Playerbot