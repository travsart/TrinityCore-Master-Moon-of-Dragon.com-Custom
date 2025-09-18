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

class TC_GAME_API ShadowSpecialization : public PriestSpecialization
{
public:
    explicit ShadowSpecialization(Player* bot);

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
    PriestSpec GetSpecialization() const override { return PriestSpec::SHADOW; }
    const char* GetSpecializationName() const override { return "Shadow"; }

private:
    // Shadow-specific mechanics
    void UpdateShadowMechanics();
    void UpdateShadowform();
    void UpdateDoTManagement();
    void UpdateMindFlay();
    void UpdateVampiricEmbrace();
    void UpdateShadowWeaving();
    void UpdateShadowOrbs();
    bool ShouldCastShadowWordPain(::Unit* target);
    bool ShouldCastVampiricTouch(::Unit* target);
    bool ShouldCastDevouringPlague(::Unit* target);
    bool ShouldCastMindBlast(::Unit* target);
    bool ShouldCastMindFlay(::Unit* target);
    bool ShouldCastShadowWordDeath(::Unit* target);
    bool ShouldCastDispersion();
    bool ShouldEnterShadowform();

    // DoT management system
    void ManageDoTEffects();
    void ApplyDoT(::Unit* target, uint32 spellId);
    void RefreshDoTsIfNeeded();
    void OptimizeDoTTargets();
    void PrioritizeDoTRefresh();
    bool ShouldRefreshDoT(::Unit* target, uint32 spellId);
    uint32 GetDoTTimeRemaining(::Unit* target, uint32 spellId);
    float CalculateDoTValue(::Unit* target, uint32 spellId);

    // Mind Flay channeling system
    void ManageMindFlay();
    void CastMindFlay(::Unit* target);
    void OptimizeMindFlayTiming();
    bool IsMindFlayChanneling();
    void InterruptMindFlayIfNeeded();
    void HandleMindFlayMovement();

    // Shadow Weaving mechanics
    void ManageShadowWeaving();
    void ApplyShadowWeaving(::Unit* target);
    uint32 GetShadowWeavingStacks(::Unit* target);
    bool ShouldMaintainShadowWeaving(::Unit* target);
    void OptimizeShadowWeavingTargets();

    // Vampiric Embrace healing
    void ManageVampiricEmbrace();
    void CastVampiricEmbrace(::Unit* target);
    void OptimizeVampiricEmbraceTargets();
    bool ShouldUseVampiricEmbrace();
    ::Unit* GetBestVampiricEmbraceTarget();
    float CalculateVampiricEmbraceHealing(uint32 damage);

    // Shadow Orb system
    void ManageShadowOrbs();
    void BuildShadowOrbs();
    void ConsumeShadowOrbs(uint32 spellId);
    uint32 GetShadowOrbCount();
    bool ShouldSpendShadowOrbs(uint32 spellId);
    void OptimizeShadowOrbUsage();

    // Execute phase mechanics
    void HandleExecutePhase(::Unit* target);
    bool IsTargetInExecuteRange(::Unit* target);
    void OptimizeShadowWordDeath(::Unit* target);
    void ManageExecuteRotation(::Unit* target);

    // Shadow defensive abilities
    void HandleShadowDefensives();
    void CastDispersion();
    void CastPsychicHorror(::Unit* target);
    void UseShadowEscape();
    void ManageThreatInShadowform();

    // Multi-target Shadow DPS
    void HandleMultiTargetShadow();
    void OptimizeMindSearUsage();
    void CastMindSear(::Unit* target);
    bool ShouldUseMindSear();
    void ApplyDoTsToMultipleTargets();

    // Shadowform management
    void ManageShadowform();
    void EnterShadowform();
    void ExitShadowform();
    bool IsInShadowform();
    void OptimizeShadowformUsage();

    // Mana/Shadow Orb efficiency
    void OptimizeShadowResources();
    void ManageManaInShadowform();
    void UseShadowfiend();
    void UseDispersionForMana();
    bool ShouldUseShadowfiend();
    float CalculateShadowDPS();

    // Shadow spell IDs
    enum ShadowSpells
    {
        SHADOWFORM = 15473,
        SHADOW_WORD_PAIN = 589,
        VAMPIRIC_TOUCH = 34914,
        DEVOURING_PLAGUE = 2944,
        MIND_BLAST = 8092,
        MIND_FLAY = 15407,
        SHADOW_WORD_DEATH = 32379,
        VAMPIRIC_EMBRACE = 15286,
        DISPERSION = 47585,
        SHADOWFIEND = 34433,
        MIND_SEAR = 53023,
        PSYCHIC_HORROR = 64044,
        SHADOW_WEAVING = 15257,
        SHADOW_AFFINITY = 18213,
        DARKNESS = 15359,
        SHADOW_POWER = 15316,
        IMPROVED_SHADOWFORM = 47569,
        SHADOW_ORBS = 77487,
        EMPOWERED_SHADOW = 95799,
        MIND_SPIKE = 73510,
        SIN_AND_PUNISHMENT = 87099,
        ARCHANGEL_SHADOW = 87151,
        EVANGELISM_SHADOW = 81662,
        TWISTED_FATE = 109142,
        SHADOW_INSIGHT = 124430,
        MIND_CONTROL = 605,
        SILENCE = 15487
    };

    // Enhanced mana/shadow orb system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _shadowOrbs{0};
    std::atomic<uint32> _maxShadowOrbs{3};
    std::atomic<bool> _shadowformActive{false};
    std::atomic<bool> _dispersionActive{false};
    std::atomic<uint32> _dispersionEndTime{0};
    std::atomic<bool> _vampiricEmbraceActive{false};

    // Performance metrics
    struct ShadowMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> dotDamage{0};
        std::atomic<uint32> directDamage{0};
        std::atomic<uint32> vampiricEmbraceHealing{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> shadowOrbsGenerated{0};
        std::atomic<uint32> shadowOrbsSpent{0};
        std::atomic<uint32> mindFlayTicks{0};
        std::atomic<uint32> shadowWordDeathCasts{0};
        std::atomic<uint32> dispersionUses{0};
        std::atomic<uint32> shadowfiendSummons{0};
        std::atomic<float> shadowformUptime{0.0f};
        std::atomic<float> dotUptime{0.0f};
        std::atomic<float> shadowWeavingUptime{0.0f};
        std::atomic<float> dpsEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; dotDamage = 0; directDamage = 0;
            vampiricEmbraceHealing = 0; manaSpent = 0; shadowOrbsGenerated = 0;
            shadowOrbsSpent = 0; mindFlayTicks = 0; shadowWordDeathCasts = 0;
            dispersionUses = 0; shadowfiendSummons = 0; shadowformUptime = 0.0f;
            dotUptime = 0.0f; shadowWeavingUptime = 0.0f; dpsEfficiency = 0.0f;
            manaEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _shadowMetrics;

    // Advanced DoT tracking system
    struct DoTTracker {
        std::unordered_map<uint64, uint32> shadowWordPainExpiry;
        std::unordered_map<uint64, uint32> vampiricTouchExpiry;
        std::unordered_map<uint64, uint32> devouringPlagueExpiry;
        std::unordered_map<uint64, uint32> shadowWeavingStacks;
        std::unordered_map<uint64, uint32> shadowWeavingExpiry;
        mutable std::mutex dotMutex;
        void UpdateDoT(uint64 targetGuid, uint32 spellId, uint32 duration) {
            std::lock_guard<std::mutex> lock(dotMutex);
            uint32 expiry = getMSTime() + duration;
            switch (spellId) {
                case SHADOW_WORD_PAIN:
                    shadowWordPainExpiry[targetGuid] = expiry;
                    break;
                case VAMPIRIC_TOUCH:
                    vampiricTouchExpiry[targetGuid] = expiry;
                    break;
                case DEVOURING_PLAGUE:
                    devouringPlagueExpiry[targetGuid] = expiry;
                    break;
            }
        }
        void UpdateShadowWeaving(uint64 targetGuid, uint32 stacks, uint32 duration) {
            std::lock_guard<std::mutex> lock(dotMutex);
            shadowWeavingStacks[targetGuid] = stacks;
            shadowWeavingExpiry[targetGuid] = getMSTime() + duration;
        }
        bool HasDoT(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(dotMutex);
            uint32 currentTime = getMSTime();
            switch (spellId) {
                case SHADOW_WORD_PAIN: {
                    auto it = shadowWordPainExpiry.find(targetGuid);
                    return it != shadowWordPainExpiry.end() && it->second > currentTime;
                }
                case VAMPIRIC_TOUCH: {
                    auto it = vampiricTouchExpiry.find(targetGuid);
                    return it != vampiricTouchExpiry.end() && it->second > currentTime;
                }
                case DEVOURING_PLAGUE: {
                    auto it = devouringPlagueExpiry.find(targetGuid);
                    return it != devouringPlagueExpiry.end() && it->second > currentTime;
                }
            }
            return false;
        }
        uint32 GetDoTTimeRemaining(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(dotMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = 0;
            switch (spellId) {
                case SHADOW_WORD_PAIN: {
                    auto it = shadowWordPainExpiry.find(targetGuid);
                    expiry = it != shadowWordPainExpiry.end() ? it->second : 0;
                    break;
                }
                case VAMPIRIC_TOUCH: {
                    auto it = vampiricTouchExpiry.find(targetGuid);
                    expiry = it != vampiricTouchExpiry.end() ? it->second : 0;
                    break;
                }
                case DEVOURING_PLAGUE: {
                    auto it = devouringPlagueExpiry.find(targetGuid);
                    expiry = it != devouringPlagueExpiry.end() ? it->second : 0;
                    break;
                }
            }
            return expiry > currentTime ? expiry - currentTime : 0;
        }
        uint32 GetShadowWeavingStacks(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(dotMutex);
            auto it = shadowWeavingStacks.find(targetGuid);
            if (it != shadowWeavingStacks.end()) {
                auto expiryIt = shadowWeavingExpiry.find(targetGuid);
                if (expiryIt != shadowWeavingExpiry.end() && expiryIt->second > getMSTime())
                    return it->second;
            }
            return 0;
        }
        bool ShouldRefreshDoT(uint64 targetGuid, uint32 spellId, uint32 pandemicThreshold = 5000) const {
            uint32 remaining = GetDoTTimeRemaining(targetGuid, spellId);
            return remaining <= pandemicThreshold;
        }
    } _dotTracker;

    // Mind Flay channeling system
    struct MindFlayManager {
        std::atomic<bool> isChanneling{false};
        std::atomic<uint32> channelStartTime{0};
        std::atomic<uint32> channelDuration{0};
        std::atomic<uint64> channelTarget{0};
        std::atomic<uint32> ticksRemaining{0};
        void StartChannel(uint64 targetGuid, uint32 duration, uint32 ticks) {
            isChanneling = true;
            channelStartTime = getMSTime();
            channelDuration = duration;
            channelTarget = targetGuid;
            ticksRemaining = ticks;
        }
        void StopChannel() {
            isChanneling = false;
            channelStartTime = 0;
            channelDuration = 0;
            channelTarget = 0;
            ticksRemaining = 0;
        }
        bool IsChanneling() const { return isChanneling.load(); }
        bool ShouldInterrupt() const {
            if (!isChanneling.load()) return false;
            uint32 elapsed = getMSTime() - channelStartTime.load();
            return elapsed >= channelDuration.load();
        }
        uint32 GetRemainingTime() const {
            if (!isChanneling.load()) return 0;
            uint32 elapsed = getMSTime() - channelStartTime.load();
            uint32 duration = channelDuration.load();
            return duration > elapsed ? duration - elapsed : 0;
        }
    } _mindFlayManager;

    // Shadow buff tracking
    uint32 _lastShadowfiend;
    uint32 _lastDispersion;
    uint32 _lastVampiricEmbrace;
    uint32 _lastInnerFire;
    std::atomic<uint32> _evangelismStacks{0};
    std::atomic<bool> _archangelActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Shadow mechanics
    void OptimizeShadowRotation(::Unit* target);
    void HandleShadowCooldowns();
    void ManageChannelingOptimization();
    void OptimizeDoTRefreshTiming();
    void HandleShadowOrbPriorities();
    void ManageExecutePhasePriorities();
    float CalculateShadowEfficiency();

    // Enhanced constants
    static constexpr float DPS_RANGE = 30.0f;
    static constexpr uint32 SHADOW_WORD_PAIN_DURATION = 24000; // 24 seconds
    static constexpr uint32 VAMPIRIC_TOUCH_DURATION = 15000; // 15 seconds
    static constexpr uint32 DEVOURING_PLAGUE_DURATION = 24000; // 24 seconds
    static constexpr uint32 SHADOW_WEAVING_MAX_STACKS = 5;
    static constexpr uint32 SHADOW_WEAVING_DURATION = 15000; // 15 seconds
    static constexpr uint32 MIND_FLAY_CHANNEL_TIME = 3000; // 3 seconds
    static constexpr uint32 MIND_FLAY_TICKS = 3;
    static constexpr uint32 DISPERSION_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 SHADOWFIEND_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 VAMPIRIC_EMBRACE_DURATION = 600000; // 10 minutes
    static constexpr uint32 SHADOWFORM_MANA_COST = 320;
    static constexpr uint32 SHADOW_WORD_PAIN_MANA_COST = 230;
    static constexpr uint32 VAMPIRIC_TOUCH_MANA_COST = 200;
    static constexpr uint32 DEVOURING_PLAGUE_MANA_COST = 425;
    static constexpr uint32 MIND_BLAST_MANA_COST = 225;
    static constexpr uint32 MIND_FLAY_MANA_COST = 165;
    static constexpr uint32 SHADOW_WORD_DEATH_MANA_COST = 185;
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 25.0f; // Use SW:Death below 25%
    static constexpr float DOT_PANDEMIC_THRESHOLD = 0.3f; // 30% for pandemic refresh
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 4; // 4+ targets for Mind Sear
    static constexpr float SHADOW_MANA_THRESHOLD = 10.0f; // Very aggressive mana usage in Shadow
    static constexpr float VAMPIRIC_EMBRACE_HEALING_RATIO = 0.25f; // 25% of damage as healing
};

} // namespace Playerbot