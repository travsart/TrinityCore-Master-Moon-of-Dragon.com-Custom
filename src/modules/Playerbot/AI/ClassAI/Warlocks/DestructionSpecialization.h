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

namespace Playerbot
{

class TC_GAME_API DestructionSpecialization : public WarlockSpecialization
{
public:
    explicit DestructionSpecialization(Player* bot);

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
    WarlockSpec GetSpecialization() const override { return WarlockSpec::DESTRUCTION; }
    const char* GetSpecializationName() const override { return "Destruction"; }

private:
    // Destruction-specific mechanics
    void UpdateBackdraft();
    void UpdateConflagrate();
    void UpdateShadowBurn();
    bool ShouldCastImmolate(::Unit* target);
    bool ShouldCastIncinerate(::Unit* target);
    bool ShouldCastConflagrate(::Unit* target);
    bool ShouldCastShadowBurn(::Unit* target);
    bool ShouldCastChaosBolt(::Unit* target);

    // Fire-based damage rotation
    void CastImmolate(::Unit* target);
    void CastIncinerate(::Unit* target);
    void CastConflagrate(::Unit* target);
    void CastShadowBurn(::Unit* target);
    void CastChaosBolt(::Unit* target);

    // Destruction spell IDs
    enum DestructionSpells
    {
        INCINERATE = 29722,
        CONFLAGRATE = 17962,
        SHADOW_BURN = 17877,
        CHAOS_BOLT = 50796,
        BACKDRAFT = 47258,
        SOUL_FIRE = 6353
    };

    // State tracking
    uint32 _shadowBurnCharges;
    uint32 _backdraftStacks;
    uint32 _conflagrateCharges;
    uint32 _lastImmolate;
    uint32 _lastConflagrate;
    uint32 _lastShadowBurn;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 CONFLAGRATE_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 SHADOW_BURN_COOLDOWN = 15000; // 15 seconds
    static constexpr uint32 CHAOS_BOLT_COOLDOWN = 12000; // 12 seconds
};

} // namespace Playerbot