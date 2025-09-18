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
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

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

    // Enhanced state tracking
    std::atomic<uint32> _demonicEmpowermentStacks{0};
    std::atomic<uint32> _lastMetamorphosis{0};
    std::atomic<uint32> _felguardCommands{0};
    std::atomic<uint32> _lastDemonicEmpowerment{0};
    std::atomic<bool> _demonFormActive{false};
    std::atomic<bool> _petEnhanced{false};
    std::atomic<bool> _felguardActive{false};
    std::atomic<uint32> _metamorphosisDuration{0};
    std::atomic<bool> _soulLinkActive{false};
    std::atomic<uint32> _masterDemonologistStacks{0};

    // Performance metrics
    struct DemonologyMetrics {
        std::atomic<uint32> petDamageDealt{0};
        std::atomic<uint32> metamorphosisCasts{0};
        std::atomic<uint32> demonicEmpowermentCasts{0};
        std::atomic<uint32> felguardCommands{0};
        std::atomic<uint32> soulBurnApplications{0};
        std::atomic<float> petUptime{0.0f};
        std::atomic<float> metamorphosisUptime{0.0f};
        std::atomic<float> demonicEmpowermentUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            petDamageDealt = 0; metamorphosisCasts = 0; demonicEmpowermentCasts = 0;
            felguardCommands = 0; soulBurnApplications = 0;
            petUptime = 0.0f; metamorphosisUptime = 0.0f; demonicEmpowermentUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _demonologyMetrics;

    // Pet management system
    struct PetManager {
        std::atomic<uint32> petHealthPercent{0};
        std::atomic<uint32> petManaPercent{0};
        std::atomic<bool> petInCombat{false};
        std::chrono::steady_clock::time_point lastPetCommand;
        std::chrono::steady_clock::time_point lastHealthCheck;
        WarlockPet currentPet;
        ::Unit* petTarget;
        void UpdatePetStatus(Pet* pet) {
            if (pet && pet->IsAlive()) {
                petHealthPercent = pet->GetHealthPct();
                petManaPercent = pet->GetPowerPct(POWER_MANA);
                petInCombat = pet->IsInCombat();
                lastHealthCheck = std::chrono::steady_clock::now();
            }
        }
    } _petManager;

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Demonology mechanics
    void OptimizePetAI();
    void ManagePetTalents();
    void OptimizeFelguardRotation();
    void HandleMetamorphosisPhase();
    void ManageDemonicEmpowermentTiming();
    void OptimizePetPositioning();
    void HandlePetSpecialAbilities();
    void ManageSoulLinkHealing();
    void OptimizeMasterDemonologist();
    void HandleDemonFormTransition();
    void ManagePetSurvival();
    void OptimizeSoulBurnTiming();
    void HandlePetDeath();
    void ManagePetMana();
    void OptimizePetTargeting();
    void HandleAdvancedPetCommands();
    void ManageFelguardIntercept();
    void OptimizeDemonicPower();
    void HandlePetBuffs();
    void ManageSummoningOptimization();
    float CalculatePetDamageContribution();
    void OptimizePetCombatRole();

    // Enhanced constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr uint32 METAMORPHOSIS_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 DEMONIC_EMPOWERMENT_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 FELGUARD_COMMAND_INTERVAL = 2000; // 2 seconds optimized
    static constexpr uint32 METAMORPHOSIS_DURATION = 30000; // 30 seconds
    static constexpr uint32 DEMONIC_EMPOWERMENT_DURATION = 30000; // 30 seconds
    static constexpr float PET_HEALTH_THRESHOLD = 50.0f; // 50% for healing
    static constexpr float PET_MANA_THRESHOLD = 30.0f; // 30% for mana management
    static constexpr uint32 PET_SUMMON_CAST_TIME = 6000; // 6 seconds
    static constexpr uint32 SOUL_LINK_HEALING_THRESHOLD = 70.0f; // 70% health
    static constexpr uint32 MASTER_DEMONOLOGIST_MAX_STACKS = 5;
    static constexpr float FELGUARD_OPTIMAL_RANGE = 5.0f;
    static constexpr uint32 PET_COMMAND_QUEUE_SIZE = 3;
    static constexpr uint32 FELGUARD_CLEAVE_TARGETS = 3;
    static constexpr float IMMOLATION_AURA_RANGE = 8.0f;
    static constexpr uint32 DEMON_CHARGE_RANGE = 25;
    static constexpr float PET_POSITIONING_TOLERANCE = 3.0f
};

} // namespace Playerbot