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
#include <vector>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API AfflictionSpecialization : public WarlockSpecialization
{
public:
    explicit AfflictionSpecialization(Player* bot);

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
    WarlockSpec GetSpecialization() const override { return WarlockSpec::AFFLICTION; }
    const char* GetSpecializationName() const override { return "Affliction"; }

private:
    // Affliction-specific mechanics
    void UpdateUnstableAffliction();
    void UpdateDrainRotation();
    void UpdateSeedOfCorruption();
    void UpdateDarkRitual();
    bool ShouldCastCorruption(::Unit* target);
    bool ShouldCastUnstableAffliction(::Unit* target);
    bool ShouldCastCurseOfAgony(::Unit* target);
    bool ShouldCastDrainLife(::Unit* target);
    bool ShouldCastDrainSoul(::Unit* target);
    bool ShouldCastSeedOfCorruption(::Unit* target);

    // DoT spreading and multi-target management
    void SpreadDoTsToMultipleTargets();
    void RefreshExpiringDoTs();
    ::Unit* GetBestDoTTarget();
    std::vector<::Unit*> GetDoTTargets(uint32 maxTargets = 8);
    bool IsTargetWorthDoTting(::Unit* target);

    // Drain mechanics
    void CastDrainLife(::Unit* target);
    void CastDrainSoul(::Unit* target);
    void CastDrainMana(::Unit* target);
    ::Unit* GetBestDrainTarget();
    bool ShouldChannelDrain();

    // Affliction spell rotation
    void CastCorruption(::Unit* target);
    void CastUnstableAffliction(::Unit* target);
    void CastCurseOfAgony(::Unit* target);
    void CastSeedOfCorruption(::Unit* target);
    void CastShadowBolt(::Unit* target);

    // Life tap optimization for affliction
    void ManageLifeTap();
    bool ShouldUseLifeTap();
    void CastLifeTap();

    // Dark ritual and soul management
    void CastDarkRitual();
    bool ShouldUseDarkRitual();

    // Affliction spell IDs
    enum AfflictionSpells
    {
        UNSTABLE_AFFLICTION = 30108,
        DRAIN_LIFE = 689,
        DRAIN_SOUL = 1120,
        DRAIN_MANA = 5138,
        SEED_OF_CORRUPTION = 27243,
        DARK_RITUAL = 7728,
        SIPHON_SOUL = 17804,
        HAUNT = 48181
    };

    // Enhanced state tracking
    std::atomic<uint32> _corruptionTargets{0};
    std::atomic<uint32> _curseOfAgonyTargets{0};
    std::atomic<uint32> _unstableAfflictionStacks{0};
    std::atomic<uint32> _lastDrainLife{0};
    std::atomic<uint32> _lastDrainSoul{0};
    std::atomic<uint32> _lastDarkRitual{0};
    std::atomic<uint32> _lastLifeTap{0};
    std::atomic<bool> _isChanneling{false};
    ::Unit* _drainTarget;
    std::atomic<bool> _shadowTranceProc{false};
    std::atomic<uint32> _nightfallStacks{0};
    std::atomic<bool> _drainSoulExecuteMode{false};

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance metrics
    struct AfflictionMetrics {
        std::atomic<uint32> totalDoTDamage{0};
        std::atomic<uint32> totalDrainDamage{0};
        std::atomic<uint32> manaFromLifeTap{0};
        std::atomic<uint32> corruptionTicks{0};
        std::atomic<uint32> unstableAfflictionTicks{0};
        std::atomic<uint32> drainLifeHealing{0};
        std::atomic<uint32> soulShardGeneration{0};
        std::atomic<float> dotUptimePercentage{0.0f};
        std::atomic<float> channelEfficiency{0.0f};
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDoTDamage = 0; totalDrainDamage = 0; manaFromLifeTap = 0;
            corruptionTicks = 0; unstableAfflictionTicks = 0; drainLifeHealing = 0;
            soulShardGeneration = 0; dotUptimePercentage = 0.0f; channelEfficiency = 0.0f;
            lastUpdate = std::chrono::steady_clock::now();
        }
    } _afflictionMetrics;

    // Multi-target DoT management
    std::atomic<uint32> _maxDoTTargets{MAX_DOT_TARGETS};
    std::vector<::Unit*> _dotTargets;
    std::atomic<uint32> _lastDoTSpread{0};
    mutable std::mutex _dotTargetsMutex;

    // DoT tracking system
    struct DoTTracker {
        std::unordered_map<uint64, uint32> corruptionExpiry;
        std::unordered_map<uint64, uint32> agonyExpiry;
        std::unordered_map<uint64, uint32> unstableAfflictionExpiry;
        std::unordered_map<uint64, uint32> seedExpiry;
        void UpdateDoT(uint64 targetGuid, uint32 spellId, uint32 duration) {
            uint32 expiry = getMSTime() + duration;
            switch (spellId) {
                case CORRUPTION: corruptionExpiry[targetGuid] = expiry; break;
                case CURSE_OF_AGONY: agonyExpiry[targetGuid] = expiry; break;
                case UNSTABLE_AFFLICTION: unstableAfflictionExpiry[targetGuid] = expiry; break;
                case SEED_OF_CORRUPTION: seedExpiry[targetGuid] = expiry; break;
            }
        }
        bool HasDoT(uint64 targetGuid, uint32 spellId) const {
            uint32 currentTime = getMSTime();
            switch (spellId) {
                case CORRUPTION: {
                    auto it = corruptionExpiry.find(targetGuid);
                    return it != corruptionExpiry.end() && it->second > currentTime;
                }
                case CURSE_OF_AGONY: {
                    auto it = agonyExpiry.find(targetGuid);
                    return it != agonyExpiry.end() && it->second > currentTime;
                }
                case UNSTABLE_AFFLICTION: {
                    auto it = unstableAfflictionExpiry.find(targetGuid);
                    return it != unstableAfflictionExpiry.end() && it->second > currentTime;
                }
                case SEED_OF_CORRUPTION: {
                    auto it = seedExpiry.find(targetGuid);
                    return it != seedExpiry.end() && it->second > currentTime;
                }
            }
            return false;
        }
        uint32 GetTimeRemaining(uint64 targetGuid, uint32 spellId) const {
            uint32 currentTime = getMSTime();
            uint32 expiry = 0;
            switch (spellId) {
                case CORRUPTION: {
                    auto it = corruptionExpiry.find(targetGuid);
                    expiry = it != corruptionExpiry.end() ? it->second : 0;
                    break;
                }
                case CURSE_OF_AGONY: {
                    auto it = agonyExpiry.find(targetGuid);
                    expiry = it != agonyExpiry.end() ? it->second : 0;
                    break;
                }
                case UNSTABLE_AFFLICTION: {
                    auto it = unstableAfflictionExpiry.find(targetGuid);
                    expiry = it != unstableAfflictionExpiry.end() ? it->second : 0;
                    break;
                }
            }
            return expiry > currentTime ? expiry - currentTime : 0;
        }
    } _dotTracker;

    // Advanced Affliction mechanics
    void OptimizeDoTRotationPriority();
    void ManageMultiTargetAffliction();
    void OptimizeDrainChanneling();
    void HandleShadowTranceProccs();
    void ManageNightfallStacks();
    void OptimizeSoulShardGeneration();
    void HandleAfflictionProcs();
    void OptimizeLifeTapTiming();
    void ManageDoTSnapshotting();
    void OptimizeCorruptionSpread();
    void HandleExecutePhaseAffliction();
    void ManageDoTClipping();
    void OptimizeManaEfficiencyAffliction();
    void HandlePandemic();
    float CalculateDoTDPS(::Unit* target, uint32 spellId);
    bool ShouldRefreshDoT(::Unit* target, uint32 spellId);
    void PrioritizeDoTTargets();
    void OptimizeChannelInterruption();
    void ManageAfflictionCooldowns();
    void HandleMultiDoTClipping();

    // Enhanced constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 MAX_DOT_TARGETS = 8;
    static constexpr uint32 DOT_CHECK_INTERVAL = 1000; // 1 second for better precision
    static constexpr uint32 DRAIN_CHANNEL_TIME = 5000; // 5 seconds
    static constexpr float LIFE_TAP_MANA_THRESHOLD = 0.35f; // 35% optimized
    static constexpr float DRAIN_HEALTH_THRESHOLD = 0.6f; // 60%
    static constexpr uint32 DARK_RITUAL_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 UNSTABLE_AFFLICTION_MAX_STACKS = 3;
    static constexpr float PANDEMIC_THRESHOLD = 0.3f; // 30% for pandemic refresh
    static constexpr uint32 CORRUPTION = 172; // Add missing spell IDs
    static constexpr uint32 CURSE_OF_AGONY = 980;
    static constexpr uint32 SHADOW_BOLT = 686;
    static constexpr uint32 LIFE_TAP = 1454;
    static constexpr float DOT_CLIP_THRESHOLD = 2.0f; // 2 seconds remaining
    static constexpr uint32 NIGHTFALL_DURATION = 8000; // 8 seconds
    static constexpr uint32 SHADOW_TRANCE_DURATION = 10000; // 10 seconds
    static constexpr float DRAIN_SOUL_EXECUTE_THRESHOLD = 25.0f; // 25% health
    static constexpr uint32 MAX_UNSTABLE_AFFLICTION_TARGETS = 3;
    static constexpr float OPTIMAL_DOT_UPTIME = 0.95f; // 95% uptime target
};

} // namespace Playerbot