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

class TC_GAME_API GuardianSpecialization : public DruidSpecialization
{
public:
    explicit GuardianSpecialization(Player* bot);

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
    DruidSpec GetSpecialization() const override { return DruidSpec::GUARDIAN; }
    const char* GetSpecializationName() const override { return "Guardian"; }

private:
    // Bear Form tanking mechanics
    void UpdateBearFormTanking();
    void UpdateThreatManagement();
    void UpdateRageManagement();
    void UpdateBearBuffs();
    void UpdateDefensiveCooldowns();
    bool ShouldCastMaul(::Unit* target);
    bool ShouldCastMangle(::Unit* target);
    bool ShouldCastLacerate(::Unit* target);
    bool ShouldCastSwipe(::Unit* target);
    bool ShouldCastDemoRoar();
    bool ShouldCastFrenziedRegeneration();
    bool ShouldCastSurvivalInstincts();
    bool ShouldCastBarkskin();

    // Threat management optimization
    void OptimizeThreatGeneration();
    void PrioritizeThreatTargets();
    void ManageAggro();
    void HandleThreatEmergency();
    bool ShouldTaunt(::Unit* target);
    void CastTaunt(::Unit* target);
    void CastChallengingRoar();
    float CalculateThreatLevel(::Unit* target);
    void UpdateThreatPriorities();

    // Rage management
    bool HasEnoughRage(uint32 required);
    void ConsumeRage(uint32 amount);
    uint32 GetRage();
    float GetRagePercent();
    bool ShouldPoolRage();
    uint32 PredictRageInTime(uint32 milliseconds);
    void OptimizeRageUsage();
    void HandleRageStarvation();

    // Bear form abilities
    void CastMaul(::Unit* target);
    void CastMangle(::Unit* target);
    void CastLacerate(::Unit* target);
    void CastSwipe();
    void CastDemoRoar();
    void CastFrenziedRegeneration();
    void CastSurvivalInstincts();
    void CastBarkskin();
    void CastEnrage();

    // Lacerate stacking system
    void ManageLacerateStacks();
    void ApplyLacerate(::Unit* target);
    uint32 GetLacerateStacks(::Unit* target);
    bool ShouldRefreshLacerate(::Unit* target);
    void OptimizeLacerateUptime();

    // Defensive cooldown management
    void HandleDefensiveCooldowns();
    void UseSurvivalInstincts();
    void UseFrenziedRegeneration();
    void UseBarkskin();
    void UseEnrage();
    void CooldownPriority();
    bool IsInDanger();
    float GetHealthPercent();

    // Multi-target tanking
    void HandleMultiTargetTanking();
    void OptimizeSwipeUsage();
    void ManageMultipleThreats();
    void PrioritizeTankTargets();
    bool ShouldUseSwipe();
    void HandleAoEThreat();

    // Positioning and movement
    void OptimizePositioningForTanking();
    void MaintainTankPosition();
    void HandleMobPositioning();
    bool ShouldMoveToOptimalPosition();
    void AvoidCleaveAttacks();

    // Guardian tank spell IDs
    enum GuardianSpells
    {
        MAUL = 6807,
        MANGLE_BEAR = 33878,
        LACERATE = 33745,
        SWIPE_BEAR = 779,
        DEMORALIZING_ROAR = 99,
        GROWL = 6795,
        CHALLENGING_ROAR = 5209,
        FRENZIED_REGENERATION = 22842,
        SURVIVAL_INSTINCTS = 61336,
        BARKSKIN = 22812,
        ENRAGE = 5229,
        BASH = 5211,
        FERAL_CHARGE_BEAR = 16979,
        DIRE_BEAR_FORM = 9634
    };

    // Threat priority system
    enum class GuardianThreatPriority
    {
        CRITICAL,    // Immediate threat loss
        HIGH,        // Dangerous threat level
        MODERATE,    // Normal threat management
        LOW,         // Stable threat
        EXCESS       // Over-threat (can assist others)
    };

    struct GuardianThreatTarget
    {
        uint64 guid;
        float threatLevel;
        GuardianThreatPriority priority;
        uint32 lastTaunt;
        uint32 lacerateStacks;
        uint32 lacerateExpiry;
        bool isDangerous;
        float distanceToBot;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    // Enhanced rage system
    std::atomic<uint32> _rage{0};
    std::atomic<uint32> _maxRage{100};
    std::atomic<uint32> _lastRageGenerated{0};
    std::atomic<uint32> _rageFromDamage{0};
    std::atomic<float> _rageEfficiency{1.0f};
    std::atomic<bool> _enrageActive{false};
    std::atomic<uint32> _enrageEndTime{0};

    // Performance metrics
    struct GuardianMetrics {
        std::atomic<uint32> totalDamageTaken{0};
        std::atomic<uint32> totalThreatGenerated{0};
        std::atomic<uint32> rageGenerated{0};
        std::atomic<uint32> rageSpent{0};
        std::atomic<uint32> maulCasts{0};
        std::atomic<uint32> lacerateStacks{0};
        std::atomic<uint32> tauntUses{0};
        std::atomic<uint32> survivalInstinctUses{0};
        std::atomic<uint32> frenziedRegenerationUses{0};
        std::atomic<float> rageEfficiency{0.0f};
        std::atomic<float> threatEfficiency{0.0f};
        std::atomic<float> lacerateUptime{0.0f};
        std::atomic<float> demoRoarUptime{0.0f};
        std::atomic<float> damageReduction{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageTaken = 0; totalThreatGenerated = 0; rageGenerated = 0;
            rageSpent = 0; maulCasts = 0; lacerateStacks = 0; tauntUses = 0;
            survivalInstinctUses = 0; frenziedRegenerationUses = 0; rageEfficiency = 0.0f;
            threatEfficiency = 0.0f; lacerateUptime = 0.0f; demoRoarUptime = 0.0f;
            damageReduction = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _guardianMetrics;

    // Advanced threat tracking system
    struct ThreatManager {
        std::unordered_map<uint64, GuardianThreatTarget> targets;
        mutable std::mutex threatMutex;
        std::atomic<uint64> primaryTarget{0};
        std::atomic<uint32> activeThreatTargets{0};
        void UpdateThreat(uint64 targetGuid, float threat) {
            std::lock_guard<std::mutex> lock(threatMutex);
            auto& target = targets[targetGuid];
            target.guid = targetGuid;
            target.threatLevel = threat;
            target.lastUpdate = std::chrono::steady_clock::now();

            // Determine priority
            if (threat < 50.0f)
                target.priority = GuardianThreatPriority::CRITICAL;
            else if (threat < 100.0f)
                target.priority = GuardianThreatPriority::HIGH;
            else if (threat < 200.0f)
                target.priority = GuardianThreatPriority::MODERATE;
            else if (threat < 500.0f)
                target.priority = GuardianThreatPriority::LOW;
            else
                target.priority = GuardianThreatPriority::EXCESS;
        }
        GuardianThreatTarget* GetHighestPriorityTarget() {
            std::lock_guard<std::mutex> lock(threatMutex);
            GuardianThreatTarget* highestPriority = nullptr;
            for (auto& [guid, target] : targets) {
                if (!highestPriority || target.priority < highestPriority->priority)
                    highestPriority = &target;
            }
            return highestPriority;
        }
        uint32 GetTargetCount(GuardianThreatPriority priority) const {
            std::lock_guard<std::mutex> lock(threatMutex);
            uint32 count = 0;
            for (const auto& [guid, target] : targets) {
                if (target.priority == priority)
                    count++;
            }
            return count;
        }
    } _threatManager;

    // Lacerate tracking system
    struct LacerateTracker {
        std::unordered_map<uint64, uint32> stacks;
        std::unordered_map<uint64, uint32> expiry;
        mutable std::mutex lacerateMutex;
        void UpdateLacerate(uint64 targetGuid, uint32 stackCount, uint32 duration) {
            std::lock_guard<std::mutex> lock(lacerateMutex);
            stacks[targetGuid] = stackCount;
            expiry[targetGuid] = getMSTime() + duration;
        }
        uint32 GetStacks(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(lacerateMutex);
            auto it = stacks.find(targetGuid);
            if (it != stacks.end()) {
                uint32 currentTime = getMSTime();
                auto expiryIt = expiry.find(targetGuid);
                if (expiryIt != expiry.end() && expiryIt->second > currentTime)
                    return it->second;
            }
            return 0;
        }
        uint32 GetTimeRemaining(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(lacerateMutex);
            auto it = expiry.find(targetGuid);
            if (it != expiry.end()) {
                uint32 currentTime = getMSTime();
                return it->second > currentTime ? it->second - currentTime : 0;
            }
            return 0;
        }
        bool ShouldRefresh(uint64 targetGuid, uint32 pandemicThreshold = 3000) const {
            uint32 remaining = GetTimeRemaining(targetGuid);
            return remaining <= pandemicThreshold;
        }
    } _lacerateTracker;

    // Bear form buff tracking
    uint32 _lastDemoRoar;
    uint32 _lastFrenziedRegen;
    uint32 _lastSurvivalInstincts;
    uint32 _lastBarkskin;
    uint32 _lastEnrage;
    std::atomic<bool> _frenziedRegenActive{false};
    std::atomic<bool> _survivalInstinctsActive{false};
    std::atomic<bool> _barkskinActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Guardian mechanics
    void OptimizeTankRotation(::Unit* target);
    void HandleTankCooldowns();
    void ManageHealthThresholds();
    void OptimizeThreatGeneration();
    void HandleEmergencyDefensives();
    float CalculateTankingEfficiency();
    void PredictDamageIntake();
    void ManageRageGeneration();

    // Tank positioning optimization
    void OptimizeRoomPositioning();
    void HandleMovementForTanking();
    void AvoidGroundEffects();
    void MaintainOptimalFacing();
    bool ShouldReposition();

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 RAGE_MAX = 100;
    static constexpr uint32 LACERATE_MAX_STACKS = 5;
    static constexpr uint32 LACERATE_DURATION = 15000; // 15 seconds
    static constexpr uint32 DEMO_ROAR_DURATION = 30000; // 30 seconds
    static constexpr uint32 SURVIVAL_INSTINCTS_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 FRENZIED_REGENERATION_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 BARKSKIN_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 ENRAGE_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 ENRAGE_DURATION = 10000; // 10 seconds
    static constexpr uint32 MAUL_RAGE_COST = 15;
    static constexpr uint32 MANGLE_RAGE_COST = 15;
    static constexpr uint32 LACERATE_RAGE_COST = 15;
    static constexpr uint32 SWIPE_RAGE_COST = 20;
    static constexpr uint32 DEMO_ROAR_RAGE_COST = 10;
    static constexpr float THREAT_CRITICAL_THRESHOLD = 50.0f;
    static constexpr float THREAT_WARNING_THRESHOLD = 100.0f;
    static constexpr float HEALTH_EMERGENCY_THRESHOLD = 30.0f; // Use emergency cooldowns below 30%
    static constexpr float HEALTH_DEFENSIVE_THRESHOLD = 50.0f; // Use defensives below 50%
    static constexpr uint32 TAUNT_COOLDOWN = 8000; // 8 seconds
    static constexpr uint32 CHALLENGING_ROAR_COOLDOWN = 600000; // 10 minutes
    static constexpr float RAGE_EFFICIENCY_TARGET = 80.0f; // Target 80% rage efficiency
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 3; // 3+ targets for AoE abilities
    static constexpr float LACERATE_PANDEMIC_THRESHOLD = 0.3f; // 30% for pandemic refresh
    static constexpr uint32 POSITIONING_UPDATE_INTERVAL = 500; // Update positioning every 500ms
    static constexpr float OPTIMAL_TANK_DISTANCE = 8.0f; // Optimal distance from group
};

} // namespace Playerbot