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
    bool HasSoulShardsAvailable(uint32 required = 1) override;
    void UseSoulShard(uint32 spellId) override;

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

    // State tracking
    uint32 _corruptionTargets;
    uint32 _curseOfAgonyTargets;
    uint32 _unstableAfflictionStacks;
    uint32 _lastDrainLife;
    uint32 _lastDrainSoul;
    uint32 _lastDarkRitual;
    uint32 _lastLifeTap;
    bool _isChanneling;
    ::Unit* _drainTarget;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // DoT efficiency tracking
    uint32 _totalDoTDamage;
    uint32 _totalDrainDamage;
    uint32 _manaFromLifeTap;

    // Multi-target DoT management
    uint32 _maxDoTTargets;
    std::vector<::Unit*> _dotTargets;
    uint32 _lastDoTSpread;

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 MAX_DOT_TARGETS = 8;
    static constexpr uint32 DOT_CHECK_INTERVAL = 2000; // 2 seconds
    static constexpr uint32 DRAIN_CHANNEL_TIME = 5000; // 5 seconds
    static constexpr float LIFE_TAP_MANA_THRESHOLD = 0.3f; // 30%
    static constexpr float DRAIN_HEALTH_THRESHOLD = 0.6f; // 60%
    static constexpr uint32 DARK_RITUAL_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 UNSTABLE_AFFLICTION_MAX_STACKS = 3;
};

} // namespace Playerbot