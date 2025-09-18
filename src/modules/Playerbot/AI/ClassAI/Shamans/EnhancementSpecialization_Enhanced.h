/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ShamanSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API EnhancementSpecialization : public ShamanSpecialization
{
public:
    explicit EnhancementSpecialization(Player* bot);

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

    // Totem management
    void UpdateTotemManagement() override;
    void DeployOptimalTotems() override;
    uint32 GetOptimalFireTotem() override;
    uint32 GetOptimalEarthTotem() override;
    uint32 GetOptimalWaterTotem() override;
    uint32 GetOptimalAirTotem() override;

    // Shock rotation
    void UpdateShockRotation(::Unit* target) override;
    uint32 GetNextShockSpell(::Unit* target) override;

    // Specialization info
    ShamanSpec GetSpecialization() const override { return ShamanSpec::ENHANCEMENT; }
    const char* GetSpecializationName() const override { return "Enhancement"; }

private:
    // Enhancement-specific mechanics
    void UpdateEnhancementMechanics();
    void UpdateMeleeDPSRotation(::Unit* target);
    void UpdateWeaponImbues();
    void UpdateStormstrike(::Unit* target);
    void UpdateLashingStorms();
    void UpdateShamanisticRage();
    void UpdateFeralSpirit();
    void UpdateMaelstromWeapon();
    void UpdateUnleashElements(::Unit* target);
    bool ShouldCastStormstrike(::Unit* target);
    bool ShouldCastLavaLash(::Unit* target);
    bool ShouldCastPrimalStrike(::Unit* target);
    bool ShouldCastShamanisticRage();
    bool ShouldCastFeralSpirit();
    bool ShouldCastFireNova();
    bool ShouldUseUnleashElements();
    bool ShouldUseMaelstromWeapon();

    // Weapon imbue management
    void ManageWeaponImbues();
    void ApplyMainHandImbue();
    void ApplyOffHandImbue();
    bool ShouldRefreshMainHandImbue();
    bool ShouldRefreshOffHandImbue();
    void CastWindfuryWeapon();
    void CastFlametongueWeapon();
    void CastFrostbrandWeapon();
    void CastEarthlivingWeapon();
    uint32 GetOptimalMainHandImbue();
    uint32 GetOptimalOffHandImbue();

    // Stormstrike mechanics
    void ManageStormstrike();
    void CastStormstrike(::Unit* target);
    void TrackStormsstrikeDebuff(::Unit* target);
    bool HasStormsstrikeDebuff(::Unit* target);
    void OptimizeStormsstrikeUsage(::Unit* target);
    float GetStormsstrikeBonus(::Unit* target);

    // Maelstrom Weapon stacking
    void ManageMaelstromWeapon();
    void BuildMaelstromWeaponStacks();
    void SpendMaelstromWeaponStacks(::Unit* target);
    uint32 GetMaelstromWeaponStacks();
    bool ShouldSpendMaelstromWeapon();
    uint32 GetOptimalMaelstromSpell(::Unit* target);
    void OptimizeMaelstromUsage(::Unit* target);

    // Lava Lash mechanics
    void ManageLavaLash();
    void CastLavaLash(::Unit* target);
    bool ShouldPrioritizeLavaLash(::Unit* target);
    void OptimizeLavaLashTiming(::Unit* target);
    float CalculateLavaLashDamage(::Unit* target);

    // Shamanistic Rage management
    void ManageShamanisticRage();
    void CastShamanisticRage();
    bool HasShamanisticRage();
    void OptimizeShamanisticRageUsage();
    float GetShamanisticRageBonus();

    // Feral Spirit mechanics
    void ManageFeralSpirit();
    void CastFeralSpirit();
    bool AreFeralSpiritsActive();
    void OptimizeFeralSpiritUsage();
    uint32 GetFeralSpiritRemainingTime();

    // Unleash Elements system
    void ManageUnleashElements();
    void CastUnleashElements();
    void OptimizeUnleashTiming();
    bool ShouldUnleashForDamage();
    bool ShouldUnleashForMovement();
    uint32 GetUnleashEffectForImbue();

    // Fire Nova chaining
    void ManageFireNovaChaining();
    void CastFireNova();
    void OptimizeFireNovaChains();
    bool ShouldUseFireNova();
    void SetupFireNovaCombo(::Unit* target);

    // Dual wield mechanics
    void OptimizeDualWieldCombat();
    void ManageDualWieldMissChance();
    void HandleOffHandAttacks();
    void OptimizeAttackSpeed();
    float GetDualWieldHitChance();

    // Enhancement cooldowns
    void HandleEnhancementCooldowns();
    void CastElementalMastery();
    void UseFeralSpirit();
    void UseShamanisticRage();
    void OptimizeCooldownUsage();

    // Multi-target Enhancement DPS
    void HandleMultiTargetEnhancement();
    void OptimizeChainLightningWithMaelstrom();
    void UseFireNovaForAoE();
    bool ShouldUseAoERotation();
    void HandleAoEPositioning();

    // Positioning for melee DPS
    void OptimizeEnhancementPositioning();
    void MaintainMeleeRange(::Unit* target);
    void HandleMeleeMovement();
    bool ShouldMoveToTarget(::Unit* target);
    void AvoidMeleeHazards();
    void OptimizeWeaponSwingTiming();

    // Mana management for Enhancement
    void OptimizeEnhancementMana();
    void UseShamanisticRageForMana();
    void ManageManaEfficiency();
    bool ShouldConserveManaForShocks();
    float CalculateEnhancementManaUsage();

    // Enhancement spell IDs
    enum EnhancementSpells
    {
        STORMSTRIKE = 17364,
        LAVA_LASH = 60103,
        PRIMAL_STRIKE = 73899,
        UNLEASH_ELEMENTS = 73680,
        FIRE_NOVA = 1535,
        SHAMANISTIC_RAGE = 30823,
        FERAL_SPIRIT = 51533,
        MAELSTROM_WEAPON = 51530,
        MENTAL_QUICKNESS = 30812,
        UNLEASHED_RAGE = 30802,
        WEAPON_MASTERY = 29082,
        SPIRIT_WEAPONS = 16268,
        ELEMENTAL_WEAPONS = 16266,
        FLURRY = 16257,
        ELEMENTAL_BLAST = 117014,
        ANCESTRAL_GUIDANCE = 16240,
        IMPROVED_STORMSTRIKE = 51521,
        STATIC_SHOCK = 51525,
        FROZEN_POWER = 63373,
        MOLTEN_BLAST = 60188,
        ENHANCED_ELEMENTS = 29192,
        PRIMAL_WISDOM = 51522,
        // Weapon imbues
        WINDFURY_WEAPON = 8232,
        FLAMETONGUE_WEAPON = 8024,
        FROSTBRAND_WEAPON = 8033,
        EARTHLIVING_WEAPON = 51730,
        ROCKBITER_WEAPON = 8017,
        // Shield spells
        LIGHTNING_SHIELD = 324,
        WATER_SHIELD = 52127,
        // Utility
        GHOST_WOLF = 2645,
        SPIRIT_WALK = 58875
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _maelstromWeaponStacks{0};
    std::atomic<bool> _shamanisticRageActive{false};
    std::atomic<uint32> _shamanisticRageEndTime{0};
    std::atomic<bool> _feralSpiritsActive{false};
    std::atomic<uint32> _feralSpiritsEndTime{0};
    std::atomic<bool> _elementalMasteryActive{false};

    // Performance metrics
    struct EnhancementMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> meleeDamage{0};
        std::atomic<uint32> spellDamage{0};
        std::atomic<uint32> stormstrikeCasts{0};
        std::atomic<uint32> lavaLashCasts{0};
        std::atomic<uint32> maelstromWeaponProcs{0};
        std::atomic<uint32> maelstromWeaponSpent{0};
        std::atomic<uint32> fireNovaChains{0};
        std::atomic<uint32> windfuryProcs{0};
        std::atomic<uint32> flametongueProcs{0};
        std::atomic<uint32> shamanisticRageUses{0};
        std::atomic<uint32> feralSpiritUses{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<float> dualWieldHitRate{0.0f};
        std::atomic<float> maelstromProcRate{0.0f};
        std::atomic<float> windfuryProcRate{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> meleeUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; meleeDamage = 0; spellDamage = 0;
            stormstrikeCasts = 0; lavaLashCasts = 0; maelstromWeaponProcs = 0;
            maelstromWeaponSpent = 0; fireNovaChains = 0; windfuryProcs = 0;
            flametongueProcs = 0; shamanisticRageUses = 0; feralSpiritUses = 0;
            manaSpent = 0; dualWieldHitRate = 0.0f; maelstromProcRate = 0.0f;
            windfuryProcRate = 0.0f; manaEfficiency = 0.0f; meleeUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _enhancementMetrics;

    // Advanced weapon imbue tracker
    struct WeaponImbueTracker {
        std::atomic<uint32> mainHandImbue{0};
        std::atomic<uint32> offHandImbue{0};
        std::atomic<uint32> mainHandExpiry{0};
        std::atomic<uint32> offHandExpiry{0};
        std::atomic<uint32> mainHandCharges{0};
        std::atomic<uint32> offHandCharges{0};
        mutable std::mutex imbueMutex;
        void SetMainHandImbue(uint32 spellId, uint32 duration, uint32 charges = 0) {
            std::lock_guard<std::mutex> lock(imbueMutex);
            mainHandImbue = spellId;
            mainHandExpiry = getMSTime() + duration;
            mainHandCharges = charges;
        }
        void SetOffHandImbue(uint32 spellId, uint32 duration, uint32 charges = 0) {
            std::lock_guard<std::mutex> lock(imbueMutex);
            offHandImbue = spellId;
            offHandExpiry = getMSTime() + duration;
            offHandCharges = charges;
        }
        bool HasMainHandImbue() const {
            std::lock_guard<std::mutex> lock(imbueMutex);
            return mainHandImbue.load() != 0 && mainHandExpiry.load() > getMSTime();
        }
        bool HasOffHandImbue() const {
            std::lock_guard<std::mutex> lock(imbueMutex);
            return offHandImbue.load() != 0 && offHandExpiry.load() > getMSTime();
        }
        uint32 GetMainHandTimeRemaining() const {
            std::lock_guard<std::mutex> lock(imbueMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = mainHandExpiry.load();
            return expiry > currentTime ? expiry - currentTime : 0;
        }
        uint32 GetOffHandTimeRemaining() const {
            std::lock_guard<std::mutex> lock(imbueMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = offHandExpiry.load();
            return expiry > currentTime ? expiry - currentTime : 0;
        }
        bool ShouldRefreshMainHand(uint32 refreshThreshold = 30000) const {
            return GetMainHandTimeRemaining() <= refreshThreshold;
        }
        bool ShouldRefreshOffHand(uint32 refreshThreshold = 30000) const {
            return GetOffHandTimeRemaining() <= refreshThreshold;
        }
    } _weaponImbueTracker;

    // Stormstrike debuff tracker
    struct StormstrikeTracker {
        std::unordered_map<uint64, uint32> stormstrikeExpiry;
        mutable std::mutex stormstrikeMutex;
        void ApplyStormstrike(uint64 targetGuid, uint32 duration) {
            std::lock_guard<std::mutex> lock(stormstrikeMutex);
            stormstrikeExpiry[targetGuid] = getMSTime() + duration;
        }
        bool HasStormstrike(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(stormstrikeMutex);
            auto it = stormstrikeExpiry.find(targetGuid);
            return it != stormstrikeExpiry.end() && it->second > getMSTime();
        }
        uint32 GetStormstrikeTimeRemaining(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(stormstrikeMutex);
            auto it = stormstrikeExpiry.find(targetGuid);
            if (it != stormstrikeExpiry.end()) {
                uint32 currentTime = getMSTime();
                return it->second > currentTime ? it->second - currentTime : 0;
            }
            return 0;
        }
        float GetDamageBonus(uint64 targetGuid) const {
            return HasStormstrike(targetGuid) ? 0.20f : 0.0f; // 20% nature damage bonus
        }
    } _stormstrikeTracker;

    // Proc tracking system
    struct ProcTracker {
        std::atomic<uint32> windfuryProcs{0};
        std::atomic<uint32> flametongueProcs{0};
        std::atomic<uint32> maelstromProcs{0};
        std::atomic<uint32> unleashProcs{0};
        std::atomic<uint32> staticShockProcs{0};
        std::atomic<uint32> totalAttacks{0};
        void RecordAttack() { totalAttacks++; }
        void RecordWindfuryProc() { windfuryProcs++; }
        void RecordFlametongueProc() { flametongueProcs++; }
        void RecordMaelstromProc() { maelstromProcs++; }
        void RecordUnleashProc() { unleashProcs++; }
        void RecordStaticShockProc() { staticShockProcs++; }
        float GetWindfuryProcRate() const {
            uint32 total = totalAttacks.load();
            return total > 0 ? (float)windfuryProcs.load() / total : 0.0f;
        }
        float GetMaelstromProcRate() const {
            uint32 total = totalAttacks.load();
            return total > 0 ? (float)maelstromProcs.load() / total : 0.0f;
        }
    } _procTracker;

    // Enhancement buff tracking
    uint32 _lastShamanisticRage;
    uint32 _lastFeralSpirit;
    uint32 _lastElementalMastery;
    uint32 _lastUnleashElements;
    uint32 _lastMainHandImbue;
    uint32 _lastOffHandImbue;
    std::atomic<bool> _spiritWalkActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Enhancement mechanics
    void OptimizeEnhancementRotation(::Unit* target);
    void HandleEnhancementCooldownPhases();
    void ManageMaelstromWeaponPriorities();
    void OptimizeStormstrikeWindows();
    void HandleWeaponImbueRotation();
    void ManageFeralSpiritPhase();
    float CalculateEnhancementDPS();

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 MAELSTROM_WEAPON_MAX_STACKS = 5;
    static constexpr uint32 STORMSTRIKE_COOLDOWN = 8000; // 8 seconds
    static constexpr uint32 LAVA_LASH_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 SHAMANISTIC_RAGE_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 SHAMANISTIC_RAGE_DURATION = 15000; // 15 seconds
    static constexpr uint32 FERAL_SPIRIT_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 FERAL_SPIRIT_DURATION = 30000; // 30 seconds
    static constexpr uint32 UNLEASH_ELEMENTS_COOLDOWN = 15000; // 15 seconds
    static constexpr uint32 STORMSTRIKE_DEBUFF_DURATION = 15000; // 15 seconds
    static constexpr uint32 WEAPON_IMBUE_DURATION = 1800000; // 30 minutes
    static constexpr uint32 STORMSTRIKE_MANA_COST = 225;
    static constexpr uint32 LAVA_LASH_MANA_COST = 200;
    static constexpr uint32 UNLEASH_ELEMENTS_MANA_COST = 180;
    static constexpr uint32 FIRE_NOVA_MANA_COST = 350;
    static constexpr float WINDFURY_PROC_CHANCE = 0.36f; // 36% proc chance
    static constexpr float MAELSTROM_PROC_CHANCE = 0.15f; // 15% proc chance per hit
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 4; // 4+ targets for AoE
    static constexpr float ENHANCEMENT_MANA_THRESHOLD = 20.0f; // Conservative mana usage below 20%
    static constexpr uint32 WEAPON_IMBUE_REFRESH_THRESHOLD = 300000; // Refresh with 5 minutes remaining
    static constexpr float DUAL_WIELD_HIT_PENALTY = 0.19f; // 19% miss chance penalty
    static constexpr float OPTIMAL_ATTACK_SPEED = 2.6f; // Optimal weapon speed for Enhancement
};

} // namespace Playerbot