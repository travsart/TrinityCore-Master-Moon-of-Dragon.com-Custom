/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PriestSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API DisciplineSpecialization : public PriestSpecialization
{
public:
    explicit DisciplineSpecialization(Player* bot);

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

    // Healing interface
    void UpdateHealing() override;
    bool ShouldHeal() override;
    ::Unit* GetBestHealTarget() override;
    void HealTarget(::Unit* target) override;

    // Role management
    PriestRole GetCurrentRole() override;
    void SetRole(PriestRole role) override;

    // Specialization info
    PriestSpec GetSpecialization() const override { return PriestSpec::DISCIPLINE; }
    const char* GetSpecializationName() const override { return "Discipline"; }

private:
    // Discipline-specific mechanics
    void UpdateDisciplineMechanics();
    void UpdatePreventiveHealing();
    void UpdatePowerWordShield();
    void UpdatePenance();
    void UpdateBorrowedTime();
    void UpdateGrace();
    void UpdateAtonement();
    bool ShouldCastPowerWordShield(::Unit* target);
    bool ShouldCastPenance(::Unit* target);
    bool ShouldCastInnerFocus();
    bool ShouldCastPainSuppression(::Unit* target);
    bool ShouldCastPowerInfusion(::Unit* target);
    bool ShouldCastBarrier();

    // Preventive healing optimization
    void OptimizePreventiveHealing();
    void PrioritizeShieldTargets();
    void ManageShieldCooldowns();
    void HandleDamageAbsorption();
    void PredictIncomingDamage();
    void ApplyPreventiveMeasures();
    float CalculateShieldValue(::Unit* target);
    bool ShouldPreventiveBuff(::Unit* target);

    // Power Word: Shield management
    void ManagePowerWordShield();
    void CastPowerWordShield(::Unit* target);
    bool TargetHasWeakenedSoul(::Unit* target);
    void TrackShieldEffectiveness();
    void OptimizeShieldTargets();
    uint32 GetShieldAbsorbRemaining(::Unit* target);
    void RefreshShieldIfNeeded(::Unit* target);

    // Penance mechanics
    void ManagePenanceHealing();
    void CastPenanceHeal(::Unit* target);
    void CastPenanceDamage(::Unit* target);
    void OptimizePenanceTargeting();
    bool IsPenanceChanneling();
    void InterruptPenanceIfNeeded();

    // Grace stacking system
    void ManageGraceStacks();
    void ApplyGraceStack(::Unit* target);
    uint32 GetGraceStacks(::Unit* target);
    bool ShouldMaintainGrace(::Unit* target);
    void OptimizeGraceTargets();
    float GetGraceHealingBonus(::Unit* target);

    // Borrowed Time mechanics
    void ManageBorrowedTime();
    void TriggerBorrowedTime();
    bool HasBorrowedTime();
    void OptimizeBorrowedTimeUsage();
    float GetBorrowedTimeBonus();

    // Atonement healing
    void ManageAtonementHealing();
    void CastAtonementDamage(::Unit* target);
    void OptimizeAtonementTargets();
    bool ShouldUseAtonement();
    ::Unit* GetBestAtonementDamageTarget();
    float CalculateAtonementHealing(uint32 damage);

    // Defensive cooldowns
    void HandleDisciplineDefensives();
    void CastPainSuppression(::Unit* target);
    void CastPowerInfusion(::Unit* target);
    void CastInnerFocus();
    void CastBarrier();
    void CastGuardianSpirit(::Unit* target);
    void UseDisciplineEmergencyHealing();

    // Mana management
    void OptimizeDisciplineMana();
    void UseInnerFocus();
    void ManageManaEfficiency();
    bool ShouldUseManaRegeneration();
    void ConserveManaWhenLow();
    float CalculateManaEfficiency(uint32 spellId);

    // Discipline spell IDs
    enum DisciplineSpells
    {
        POWER_WORD_SHIELD = 17,
        PENANCE = 47540,
        FLASH_HEAL = 2061,
        GREATER_HEAL = 2060,
        HEAL = 2054,
        RENEW = 139,
        PRAYER_OF_HEALING = 596,
        CIRCLE_OF_HEALING = 34861,
        PAIN_SUPPRESSION = 33206,
        POWER_INFUSION = 10060,
        INNER_FOCUS = 14751,
        DIVINE_AEGIS = 47515,
        GRACE = 47516,
        BORROWED_TIME = 52795,
        ATONEMENT = 81749,
        BARRIER = 62618,
        GUARDIAN_SPIRIT = 47788,
        WEAKENED_SOUL = 6788,
        ARCHANGEL = 81700,
        EVANGELISM = 81661,
        BINDING_HEAL = 32546,
        PRAYER_OF_MENDING = 33076
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _lastManaRegen{0};
    std::atomic<float> _manaRegenRate{0.0f};
    std::atomic<bool> _innerFocusActive{false};
    std::atomic<uint32> _innerFocusEndTime{0};
    std::atomic<bool> _borrowedTimeActive{false};
    std::atomic<uint32> _borrowedTimeEndTime{0};

    // Performance metrics
    struct DisciplineMetrics {
        std::atomic<uint32> totalHealingDone{0};
        std::atomic<uint32> totalShieldingDone{0};
        std::atomic<uint32> damagePrevented{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> shieldsCast{0};
        std::atomic<uint32> penanceCasts{0};
        std::atomic<uint32> graceStacksApplied{0};
        std::atomic<uint32> atonementHealing{0};
        std::atomic<uint32> painSuppressionUses{0};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> healingEfficiency{0.0f};
        std::atomic<float> shieldEfficiency{0.0f};
        std::atomic<float> preventiveHealingRatio{0.0f};
        std::atomic<float> overhealingPercent{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalHealingDone = 0; totalShieldingDone = 0; damagePrevented = 0;
            manaSpent = 0; shieldsCast = 0; penanceCasts = 0; graceStacksApplied = 0;
            atonementHealing = 0; painSuppressionUses = 0; manaEfficiency = 0.0f;
            healingEfficiency = 0.0f; shieldEfficiency = 0.0f; preventiveHealingRatio = 0.0f;
            overhealingPercent = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _disciplineMetrics;

    // Advanced shield tracking system
    struct ShieldTracker {
        std::unordered_map<uint64, uint32> shieldExpiry;
        std::unordered_map<uint64, uint32> shieldAmount;
        std::unordered_map<uint64, uint32> weakenedSoulExpiry;
        mutable std::mutex shieldMutex;
        void UpdateShield(uint64 targetGuid, uint32 amount, uint32 duration) {
            std::lock_guard<std::mutex> lock(shieldMutex);
            shieldExpiry[targetGuid] = getMSTime() + duration;
            shieldAmount[targetGuid] = amount;
        }
        void UpdateWeakenedSoul(uint64 targetGuid, uint32 duration) {
            std::lock_guard<std::mutex> lock(shieldMutex);
            weakenedSoulExpiry[targetGuid] = getMSTime() + duration;
        }
        bool HasShield(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(shieldMutex);
            auto it = shieldExpiry.find(targetGuid);
            return it != shieldExpiry.end() && it->second > getMSTime();
        }
        bool HasWeakenedSoul(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(shieldMutex);
            auto it = weakenedSoulExpiry.find(targetGuid);
            return it != weakenedSoulExpiry.end() && it->second > getMSTime();
        }
        uint32 GetShieldAmount(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(shieldMutex);
            auto it = shieldAmount.find(targetGuid);
            return it != shieldAmount.end() ? it->second : 0;
        }
    } _shieldTracker;

    // Grace tracking system
    struct GraceTracker {
        std::unordered_map<uint64, uint32> stacks;
        std::unordered_map<uint64, uint32> expiry;
        mutable std::mutex graceMutex;
        void UpdateGrace(uint64 targetGuid, uint32 stackCount, uint32 duration) {
            std::lock_guard<std::mutex> lock(graceMutex);
            stacks[targetGuid] = stackCount;
            expiry[targetGuid] = getMSTime() + duration;
        }
        uint32 GetStacks(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(graceMutex);
            auto it = stacks.find(targetGuid);
            if (it != stacks.end()) {
                auto expiryIt = expiry.find(targetGuid);
                if (expiryIt != expiry.end() && expiryIt->second > getMSTime())
                    return it->second;
            }
            return 0;
        }
        float GetHealingBonus(uint64 targetGuid) const {
            uint32 stackCount = GetStacks(targetGuid);
            return stackCount * 0.03f; // 3% per stack
        }
    } _graceTracker;

    // Discipline buff tracking
    uint32 _lastPowerInfusion;
    uint32 _lastInnerFocus;
    uint32 _lastPainSuppression;
    uint32 _lastBarrier;
    uint32 _lastGuardianSpirit;
    std::atomic<uint32> _evangelismStacks{0};
    std::atomic<uint32> _archangelStacks{0};

    // Healing priority queue
    std::priority_queue<HealTarget> _healingQueue;
    mutable std::mutex _healingQueueMutex;

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Discipline mechanics
    void OptimizeDisciplineRotation();
    void HandleDisciplineCooldowns();
    void ManageEvangelismStacks();
    void UseArchangel();
    void OptimizeShieldAbsorption();
    void PredictDamagePatterns();
    void ManageThroughputVsPrevention();
    float CalculateDisciplineEfficiency();

    // Enhanced constants
    static constexpr float HEALING_RANGE = 40.0f;
    static constexpr uint32 GRACE_MAX_STACKS = 3;
    static constexpr uint32 GRACE_DURATION = 15000; // 15 seconds
    static constexpr uint32 WEAKENED_SOUL_DURATION = 15000; // 15 seconds
    static constexpr uint32 BORROWED_TIME_DURATION = 6000; // 6 seconds
    static constexpr uint32 INNER_FOCUS_DURATION = 8000; // 8 seconds
    static constexpr uint32 PAIN_SUPPRESSION_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 POWER_INFUSION_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 INNER_FOCUS_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 BARRIER_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 GUARDIAN_SPIRIT_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 POWER_WORD_SHIELD_MANA_COST = 500;
    static constexpr uint32 PENANCE_MANA_COST = 400;
    static constexpr uint32 FLASH_HEAL_MANA_COST = 380;
    static constexpr uint32 GREATER_HEAL_MANA_COST = 370;
    static constexpr uint32 HEAL_MANA_COST = 200;
    static constexpr float SHIELD_EFFICIENCY_TARGET = 85.0f; // Target 85% shield absorption
    static constexpr float PREVENTIVE_HEALING_RATIO = 0.6f; // 60% preventive vs reactive
    static constexpr uint32 EVANGELISM_MAX_STACKS = 5;
    static constexpr float ATONEMENT_HEALING_RATIO = 0.5f; // 50% of damage as healing
    static constexpr float DISCIPLINE_MANA_THRESHOLD = 20.0f; // Conservative mana usage below 20%
};

} // namespace Playerbot