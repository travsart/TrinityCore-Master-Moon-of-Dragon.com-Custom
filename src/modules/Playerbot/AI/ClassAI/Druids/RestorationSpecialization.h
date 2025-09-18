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
#include <vector>
#include <queue>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace Playerbot
{

// Healing priority levels for Druid
enum class DruidHealPriority : uint8
{
    EMERGENCY = 0,      // <20% health, imminent death
    CRITICAL = 1,       // 20-40% health, needs immediate attention
    MODERATE = 2,       // 40-70% health, should heal soon
    MAINTENANCE = 3,    // 70-90% health, top off when convenient
    FULL = 4           // >90% health, no healing needed
};

// Heal target info for druid priority queue
struct DruidHealTarget
{
    ::Unit* target;
    DruidHealPriority priority;
    float healthPercent;
    uint32 missingHealth;
    bool inCombat;
    uint32 timestamp;

    DruidHealTarget() : target(nullptr), priority(DruidHealPriority::FULL), healthPercent(100.0f),
                        missingHealth(0), inCombat(false), timestamp(0) {}

    DruidHealTarget(::Unit* t, DruidHealPriority p, float hp, uint32 missing)
        : target(t), priority(p), healthPercent(hp), missingHealth(missing),
          inCombat(t ? t->IsInCombat() : false), timestamp(getMSTime()) {}

    bool operator<(const DruidHealTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent;
        return timestamp > other.timestamp;
    }
};

class TC_GAME_API RestorationSpecialization : public DruidSpecialization
{
public:
    explicit RestorationSpecialization(Player* bot);

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
    DruidSpec GetSpecialization() const override { return DruidSpec::RESTORATION; }
    const char* GetSpecializationName() const override { return "Restoration"; }

private:
    // Restoration-specific mechanics
    void UpdateHealing();
    void UpdateHealOverTimeManagement();
    void UpdateNaturesSwiftness();
    void UpdateTranquility();
    bool ShouldCastHealingTouch(::Unit* target);
    bool ShouldCastRegrowth(::Unit* target);
    bool ShouldCastRejuvenation(::Unit* target);
    bool ShouldCastLifebloom(::Unit* target);
    bool ShouldCastSwiftmend(::Unit* target);
    bool ShouldCastTranquility();
    bool ShouldUseNaturesSwiftness();

    // Healing optimization
    ::Unit* GetBestHealTarget();
    void HealTarget(::Unit* target);
    void PrioritizeHealing();
    uint32 GetOptimalHealSpell(const DruidHealTarget& healTarget);
    void PerformTriage();

    // HoT management
    void ApplyHealingOverTime(::Unit* target, uint32 spellId);
    void RefreshExpiringHoTs();
    uint32 GetHoTRemainingTime(::Unit* target, uint32 spellId);
    void ManageLifebloomStack(::Unit* target);

    // Group healing
    void UpdateGroupHealing();
    bool ShouldUseGroupHeals();
    void HandleEmergencyHealing();
    void UseEmergencyHeals(::Unit* target);
    bool IsEmergencyHealing();

    // Restoration abilities
    void CastHealingTouch(::Unit* target);
    void CastRegrowth(::Unit* target);
    void CastRejuvenation(::Unit* target);
    void CastLifebloom(::Unit* target);
    void CastSwiftmend(::Unit* target);
    void CastTranquility();
    void CastInnervate(::Unit* target);

    // Tree of Life form management
    void EnterTreeOfLifeForm();
    bool ShouldUseTreeForm();
    void ManageTreeForm();

    // Nature's Swiftness management
    void UseNaturesSwiftness();
    bool IsNaturesSwiftnessReady();
    void CastInstantHealingTouch(::Unit* target);

    // Mana management for healers
    void ManageMana();
    void CastInnervateOptimal();
    bool ShouldConserveMana();

    // Restoration spell IDs
    enum RestorationSpells
    {
        HEALING_TOUCH = 5185,
        REGROWTH = 8936,
        SWIFTMEND = 18562,
        TRANQUILITY = 740,
        INNERVATE = 29166,
        NATURES_SWIFTNESS = 17116,
        REMOVE_CURSE = 2782,
        ABOLISH_POISON = 2893
    };

    // Healing tracking
    std::priority_queue<DruidHealTarget> _healQueue;
    std::unordered_map<ObjectGuid, std::vector<HealOverTimeInfo>> _activeHoTs;

    // HoT tracking
    std::unordered_map<ObjectGuid, uint32> _regrowthTimers;
    std::unordered_map<ObjectGuid, uint32> _lifebloomStacks;

    // Enhanced Tree of Life form tracking
    std::atomic<uint32> _treeOfLifeRemaining{0};
    std::atomic<bool> _inTreeForm{false};
    std::atomic<uint32> _lastTreeFormShift{0};
    std::atomic<bool> _treeFormOptimal{false};
    std::atomic<uint32> _treeFormCooldown{0};

    // Enhanced Nature's Swiftness tracking
    std::atomic<uint32> _naturesSwiftnessReady{0};
    std::atomic<uint32> _lastNaturesSwiftness{0};
    std::atomic<bool> _emergencySwiftnessReady{true};
    std::atomic<bool> _swiftnessOnCooldown{false};

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;
    uint32 _tranquilityReady;
    uint32 _lastTranquility;

    // Performance optimization
    uint32 _lastHealCheck;
    uint32 _lastHotCheck;
    uint32 _lastGroupScan;

    // Group member tracking
    std::vector<::Unit*> _groupMembers;

    // Emergency state
    bool _emergencyMode;
    uint32 _emergencyStartTime;

    // Performance metrics
    struct RestorationMetrics {
        std::atomic<uint32> totalHealingDone{0};
        std::atomic<uint32> overhealingDone{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> healingTouchCasts{0};
        std::atomic<uint32> regrowthCasts{0};
        std::atomic<uint32> rejuvenationCasts{0};
        std::atomic<uint32> lifebloomApplications{0};
        std::atomic<uint32> swiftmendCasts{0};
        std::atomic<uint32> innervatesUsed{0};
        std::atomic<float> healingEfficiency{0.0f};
        std::atomic<float> hotUptime{0.0f};
        std::atomic<float> emergencyResponseTime{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalHealingDone = 0; overhealingDone = 0; manaSpent = 0;
            healingTouchCasts = 0; regrowthCasts = 0; rejuvenationCasts = 0;
            lifebloomApplications = 0; swiftmendCasts = 0; innervatesUsed = 0;
            healingEfficiency = 0.0f; hotUptime = 0.0f; emergencyResponseTime = 0.0f;
            manaEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _restorationMetrics;

    // Advanced healing prediction system
    struct HealingPredictor {
        std::unordered_map<uint64, std::queue<float>> damageHistory;
        std::unordered_map<uint64, float> predictedDamage;
        mutable std::mutex predictionMutex;
        void RecordDamage(uint64 unitGuid, float damage) {
            std::lock_guard<std::mutex> lock(predictionMutex);
            auto& history = damageHistory[unitGuid];
            history.push(damage);
            if (history.size() > 5) // Keep last 5 damage events
                history.pop();
            UpdatePrediction(unitGuid);
        }
        void UpdatePrediction(uint64 unitGuid) {
            auto& history = damageHistory[unitGuid];
            if (history.empty()) return;
            float avgDamage = 0.0f;
            auto temp = history;
            int count = 0;
            while (!temp.empty() && count < 3) {
                avgDamage += temp.back();
                temp.pop();
                count++;
            }
            predictedDamage[unitGuid] = count > 0 ? avgDamage / count : 0.0f;
        }
        float GetPredictedDamage(uint64 unitGuid) const {
            std::lock_guard<std::mutex> lock(predictionMutex);
            auto it = predictedDamage.find(unitGuid);
            return it != predictedDamage.end() ? it->second : 0.0f;
        }
    } _healingPredictor;

    // HoT optimization system
    struct HoTOptimizer {
        std::unordered_map<uint64, uint32> rejuvenationExpiry;
        std::unordered_map<uint64, uint32> regrowthExpiry;
        std::unordered_map<uint64, uint32> lifebloomStacks;
        std::unordered_map<uint64, uint32> lifebloomExpiry;
        mutable std::mutex hotMutex;
        void UpdateHoT(uint64 unitGuid, uint32 spellId, uint32 duration, uint32 stacks = 1) {
            std::lock_guard<std::mutex> lock(hotMutex);
            uint32 expiry = getMSTime() + duration;
            switch (spellId) {
                case REJUVENATION:
                    rejuvenationExpiry[unitGuid] = expiry;
                    break;
                case REGROWTH:
                    regrowthExpiry[unitGuid] = expiry;
                    break;
                case LIFEBLOOM:
                    lifebloomExpiry[unitGuid] = expiry;
                    lifebloomStacks[unitGuid] = stacks;
                    break;
            }
        }
        bool HasHoT(uint64 unitGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            uint32 currentTime = getMSTime();
            switch (spellId) {
                case REJUVENATION:
                    return rejuvenationExpiry.count(unitGuid) && rejuvenationExpiry.at(unitGuid) > currentTime;
                case REGROWTH:
                    return regrowthExpiry.count(unitGuid) && regrowthExpiry.at(unitGuid) > currentTime;
                case LIFEBLOOM:
                    return lifebloomExpiry.count(unitGuid) && lifebloomExpiry.at(unitGuid) > currentTime;
            }
            return false;
        }
        uint32 GetTimeRemaining(uint64 unitGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = 0;
            switch (spellId) {
                case REJUVENATION:
                    if (rejuvenationExpiry.count(unitGuid))
                        expiry = rejuvenationExpiry.at(unitGuid);
                    break;
                case REGROWTH:
                    if (regrowthExpiry.count(unitGuid))
                        expiry = regrowthExpiry.at(unitGuid);
                    break;
                case LIFEBLOOM:
                    if (lifebloomExpiry.count(unitGuid))
                        expiry = lifebloomExpiry.at(unitGuid);
                    break;
            }
            return expiry > currentTime ? expiry - currentTime : 0;
        }
        uint32 GetLifebloomStacks(uint64 unitGuid) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            auto it = lifebloomStacks.find(unitGuid);
            return it != lifebloomStacks.end() ? it->second : 0;
        }
    } _hotOptimizer;

    // Advanced Restoration mechanics
    void OptimizeHealingRotation();
    void ManageHealingPriorities();
    void OptimizeHoTApplication();
    void HandleEmergencyHealing();
    void ManageTreeOfLifeForm();
    void OptimizeNaturesSwiftness();
    void HandleGroupHealingOptimization();
    void ManageManaEfficiency();
    void OptimizeSwiftmendTiming();
    void HandleLifebloomStacking();
    void ManageRestorationCooldowns();
    void OptimizeTranquilityTiming();
    void HandleInnervateOptimization();
    void PredictHealingNeeds();
    void ManageOverhealingReduction();
    void OptimizeHealingOutput();
    void HandleDispelling();
    void ManageHealingThroughput();
    void OptimizeResourceAllocation();
    void HandleRaidHealingPatterns();
    float CalculateHealingEfficiency();
    uint32 PredictRequiredHealing(::Unit* target);
    bool ShouldPreemptiveHeal(::Unit* target);

    // Enhanced constants
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr uint32 TREE_OF_LIFE_DURATION = 25000; // 25 seconds
    static constexpr uint32 NATURES_SWIFTNESS_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 TRANQUILITY_COOLDOWN = 480000; // 8 minutes
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr float REGROWTH_THRESHOLD = 50.0f;
    static constexpr float HEALING_TOUCH_THRESHOLD = 70.0f;
    static constexpr uint32 LIFEBLOOM_MAX_STACKS = 3;
    static constexpr uint32 REJUVENATION = 774; // Add missing spell IDs
    static constexpr uint32 LIFEBLOOM = 33763;
    static constexpr uint32 REJUVENATION_DURATION = 12000; // 12 seconds
    static constexpr uint32 REGROWTH_DURATION = 21000; // 21 seconds
    static constexpr uint32 LIFEBLOOM_DURATION = 7000; // 7 seconds
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
    static constexpr float OVERHEALING_THRESHOLD = 0.15f; // 15% overheal tolerance
    static constexpr uint32 HEALING_PREDICTION_WINDOW = 3000; // 3 seconds
    static constexpr float HOT_PANDEMIC_THRESHOLD = 0.3f; // 30% for pandemic
    static constexpr uint32 EMERGENCY_RESPONSE_TARGET = 1500; // 1.5 seconds
    static constexpr float TREE_FORM_EFFICIENCY_THRESHOLD = 0.8f;
    static constexpr uint32 SWIFTMEND_OPTIMAL_HEALTH = 40; // 40% health
    static constexpr float MANA_EFFICIENCY_TARGET = 2.0f; // 2 healing per mana
    static constexpr uint32 GROUP_HEALING_THRESHOLD = 3; // 3+ injured members
    static constexpr float LIFEBLOOM_BLOOM_THRESHOLD = 0.4f; // Let it bloom at 40%
};

} // namespace Playerbot