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

class TC_GAME_API DemonologySpecialization : public WarlockSpecialization
{
public:
    explicit DemonologySpecialization(Player* bot);

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

    // Pet management - specialized for Demonology
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
    WarlockSpec GetSpecialization() const override { return WarlockSpec::DEMONOLOGY; }
    const char* GetSpecializationName() const override { return "Demonology"; }

private:
    // Demonology-specific mechanics
    void UpdateDemonicEmpowerment();
    void UpdateMetamorphosis();
    void UpdateFelguardCommands();
    bool ShouldCastDemonicEmpowerment();
    bool ShouldCastMetamorphosis();
    bool ShouldSummonFelguard();

    // Pet enhancement abilities
    void CastDemonicEmpowerment();
    void EnhancePetAbilities();
    void OptimizePetDamage();

    // Demon form abilities
    void CastMetamorphosis();
    void UseDemonFormAbilities(::Unit* target);
    void CastImmolationAura();

    // Felguard specific commands
    void CommandFelguard(::Unit* target);
    void FelguardCleave();
    void FelguardIntercept(::Unit* target);

    // Soul management for summons
    void ManageSoulBurn(::Unit* target);
    void CastSoulBurn(::Unit* target);

    // Demonology spell IDs
    enum DemonologySpells
    {
        DEMONIC_EMPOWERMENT = 47193,
        METAMORPHOSIS = 59672,
        SOUL_BURN = 17877,
        IMMOLATION_AURA = 50589,
        DEMON_CHARGE = 54785,
        FELGUARD_CLEAVE = 30213,
        FELGUARD_INTERCEPT = 30151
    };

    // State tracking
    uint32 _demonicEmpowermentStacks;
    uint32 _lastMetamorphosis;
    uint32 _felguardCommands;
    uint32 _lastDemonicEmpowerment;
    bool _demonFormActive;
    bool _petEnhanced;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 METAMORPHOSIS_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 DEMONIC_EMPOWERMENT_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 FELGUARD_COMMAND_INTERVAL = 3000; // 3 seconds
};

} // namespace Playerbot