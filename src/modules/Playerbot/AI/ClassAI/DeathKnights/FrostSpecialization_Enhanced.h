/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DeathKnightSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API FrostSpecialization : public DeathKnightSpecialization
{
public:
    explicit FrostSpecialization(Player* bot);

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

    // Rune management
    void UpdateRuneManagement() override;
    bool HasAvailableRunes(RuneType type, uint32 count = 1) override;
    void ConsumeRunes(RuneType type, uint32 count = 1) override;
    uint32 GetAvailableRunes(RuneType type) const override;

    // Runic Power management
    void UpdateRunicPowerManagement() override;
    void GenerateRunicPower(uint32 amount) override;
    void SpendRunicPower(uint32 amount) override;
    uint32 GetRunicPower() const override;
    bool HasEnoughRunicPower(uint32 required) const override;

    // Disease management
    void UpdateDiseaseManagement() override;
    void ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId) override;
    bool HasDisease(::Unit* target, DiseaseType type) const override;
    bool ShouldApplyDisease(::Unit* target, DiseaseType type) const override;
    void RefreshExpringDiseases() override;

    // Death and Decay management
    void UpdateDeathAndDecay() override;
    bool ShouldCastDeathAndDecay() const override;
    void CastDeathAndDecay(Position targetPos) override;

    // Specialization info
    DeathKnightSpec GetSpecialization() const override { return DeathKnightSpec::FROST; }
    const char* GetSpecializationName() const override { return "Frost"; }

private:
    // Frost-specific mechanics
    void UpdateFrostMechanics();
    void UpdateDualWieldDPS(::Unit* target);
    void UpdateKillingMachine();
    void UpdateRimeProcs();
    void UpdatePillarOfFrost();
    void UpdateUnbreakableArmor();
    bool ShouldCastObliterate(::Unit* target);
    bool ShouldCastFrostStrike(::Unit* target);
    bool ShouldCastHowlingBlast();
    bool ShouldCastGlacialAdvance(::Unit* target);
    bool ShouldCastPillarOfFrost();
    bool ShouldCastUnbreakableArmor();
    bool ShouldCastEmpowerRuneWeapon();

    // Dual-wield optimization
    void OptimizeDualWieldCombat();
    void ManageWeaponEnchants();
    void HandleOffHandAttacks();
    void OptimizeThreatOfThassarian();
    float GetDualWieldHitChance();

    // Killing Machine mechanics
    void ManageKillingMachine();
    bool HasKillingMachineProc();
    void ConsumeKillingMachineProc();
    void OptimizeKillingMachineUsage();
    bool ShouldUseKillingMachine(uint32 spellId);

    // Rime proc system
    void ManageRimeProcs();
    bool HasRimeProc();
    void ConsumeRimeProc();
    void OptimizeRimeUsage();
    void CastFreeHowlingBlast();

    // Obliterate mechanics
    void ManageObliterate();
    void CastObliterate(::Unit* target);
    void OptimizeObliterateUsage(::Unit* target);
    bool ShouldPrioritizeObliterate(::Unit* target);
    uint32 CalculateObliterateDamage(::Unit* target);

    // Frost Strike optimization
    void ManageFrostStrike();
    void CastFrostStrike(::Unit* target);
    void OptimizeFrostStrikeUsage(::Unit* target);
    bool ShouldDumpRunicPowerWithFrostStrike();

    // Howling Blast mechanics
    void ManageHowlingBlast();
    void CastHowlingBlast();
    void OptimizeHowlingBlastTargeting();
    bool ShouldUseHowlingBlastForAoE();
    uint32 GetHowlingBlastTargetCount();

    // Pillar of Frost system
    void ManagePillarOfFrost();
    void CastPillarOfFrost();
    bool HasPillarOfFrost();
    void OptimizePillarOfFrostUsage();
    float GetPillarOfFrostBonus();

    // Unbreakable Armor mechanics
    void ManageUnbreakableArmor();
    void CastUnbreakableArmor();
    bool HasUnbreakableArmor();
    void OptimizeUnbreakableArmorUsage();
    float GetUnbreakableArmorBonus();

    // Empower Rune Weapon system
    void ManageEmpowerRuneWeapon();
    void CastEmpowerRuneWeapon();
    void OptimizeEmpowerRuneWeaponTiming();
    bool ShouldUseEmpowerRuneWeapon();

    // Multi-target Frost DPS
    void HandleMultiTargetFrost();
    void OptimizeAoERotation();
    void UseHowlingBlastForAoE();
    bool ShouldUseAoEAbilities();
    void HandleFrostAoE();

    // Positioning for melee DPS
    void OptimizeFrostPositioning();
    void MaintainMeleeRange(::Unit* target);
    void HandleFrostMovement();
    bool ShouldMoveToTarget(::Unit* target);
    void AvoidMeleeHazards();

    // Frost spell IDs
    enum FrostSpells
    {
        OBLITERATE = 49020,
        FROST_STRIKE = 55268,
        HOWLING_BLAST = 49184,
        GLACIAL_ADVANCE = 194913,
        PILLAR_OF_FROST = 51271,
        UNBREAKABLE_ARMOR = 51271,
        EMPOWER_RUNE_WEAPON = 47568,
        KILLING_MACHINE = 51128,
        RIME = 59057,
        THREAT_OF_THASSARIAN = 65661,
        FROST_PRESENCE = 48263,
        DEATHCHILL = 49796,
        BLOOD_OF_THE_NORTH = 54637,
        ANNIHILATION = 51410,
        NERVES_OF_COLD_STEEL = 49226,
        ICY_TALONS = 50880,
        IMPROVED_ICY_TALONS = 55610,
        MERCILESS_COMBAT = 49024,
        TUNDRA_STALKER = 49188,
        BLACK_ICE = 49140,
        FRIGID_DREADPLATE = 49226,
        ENDLESS_WINTER = 49137,
        CHILBLAINS = 50041,
        HUNGERING_COLD = 49203,
        IMPROVED_FROST_PRESENCE = 50384,
        ACCLIMATION = 49200
    };

    // Enhanced rune system for Frost
    std::atomic<uint32> _bloodRunes{2};
    std::atomic<uint32> _frostRunes{2};
    std::atomic<uint32> _unholyRunes{2};
    std::atomic<uint32> _deathRunes{0};
    std::atomic<uint32> _runicPower{0};
    std::atomic<uint32> _maxRunicPower{130};

    // Performance metrics
    struct FrostMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> mainHandDamage{0};
        std::atomic<uint32> offHandDamage{0};
        std::atomic<uint32> obliterateCasts{0};
        std::atomic<uint32> obliterateCrits{0};
        std::atomic<uint32> frostStrikeCasts{0};
        std::atomic<uint32> howlingBlastCasts{0};
        std::atomic<uint32> killingMachineProcs{0};
        std::atomic<uint32> killingMachineUsed{0};
        std::atomic<uint32> rimeProcs{0};
        std::atomic<uint32> rimeUsed{0};
        std::atomic<uint32> pillarOfFrostUses{0};
        std::atomic<uint32> empowerRuneWeaponUses{0};
        std::atomic<uint32> runicPowerGenerated{0};
        std::atomic<uint32> runicPowerSpent{0};
        std::atomic<float> dualWieldHitRate{0.0f};
        std::atomic<float> killingMachineProcRate{0.0f};
        std::atomic<float> rimeProcRate{0.0f};
        std::atomic<float> criticalStrikeRate{0.0f};
        std::atomic<float> runeEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; mainHandDamage = 0; offHandDamage = 0;
            obliterateCasts = 0; obliterateCrits = 0; frostStrikeCasts = 0;
            howlingBlastCasts = 0; killingMachineProcs = 0; killingMachineUsed = 0;
            rimeProcs = 0; rimeUsed = 0; pillarOfFrostUses = 0; empowerRuneWeaponUses = 0;
            runicPowerGenerated = 0; runicPowerSpent = 0; dualWieldHitRate = 0.0f;
            killingMachineProcRate = 0.0f; rimeProcRate = 0.0f; criticalStrikeRate = 0.0f;
            runeEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _frostMetrics;

    // Killing Machine proc tracker
    struct KillingMachineTracker {
        std::atomic<bool> hasProc{false};
        std::atomic<uint32> procTime{0};
        std::atomic<uint32> procsGenerated{0};
        std::atomic<uint32> procsUsed{0};
        void TriggerProc() {
            hasProc = true;
            procTime = getMSTime();
            procsGenerated++;
        }
        void ConsumeProc() {
            hasProc = false;
            procTime = 0;
            procsUsed++;
        }
        bool HasProc() const { return hasProc.load(); }
        bool IsExpired(uint32 duration = 30000) const {
            return hasProc.load() && (getMSTime() - procTime.load()) > duration;
        }
        float GetProcRate() const {
            uint32 generated = procsGenerated.load();
            uint32 used = procsUsed.load();
            return generated > 0 ? (float)used / generated : 0.0f;
        }
    } _killingMachineTracker;

    // Rime proc tracker
    struct RimeTracker {
        std::atomic<bool> hasProc{false};
        std::atomic<uint32> procTime{0};
        std::atomic<uint32> procsGenerated{0};
        std::atomic<uint32> procsUsed{0};
        void TriggerProc() {
            hasProc = true;
            procTime = getMSTime();
            procsGenerated++;
        }
        void ConsumeProc() {
            hasProc = false;
            procTime = 0;
            procsUsed++;
        }
        bool HasProc() const { return hasProc.load(); }
        bool IsExpired(uint32 duration = 15000) const {
            return hasProc.load() && (getMSTime() - procTime.load()) > duration;
        }
        float GetProcRate() const {
            uint32 generated = procsGenerated.load();
            uint32 used = procsUsed.load();
            return generated > 0 ? (float)used / generated : 0.0f;
        }
    } _rimeTracker;

    // Frost buff tracking
    uint32 _lastPillarOfFrost;
    uint32 _lastUnbreakableArmor;
    uint32 _lastEmpowerRuneWeapon;
    uint32 _lastHungeringCold;
    std::atomic<bool> _pillarOfFrostActive{false};
    std::atomic<bool> _unbreakableArmorActive{false};
    std::atomic<bool> _icyTalonsActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Frost mechanics
    void OptimizeFrostRotation(::Unit* target);
    void HandleFrostCooldowns();
    void ManageProcPriorities();
    void OptimizeRuneUsage();
    void HandleDualWieldOptimization();
    float CalculateFrostDPS();

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 PILLAR_OF_FROST_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 PILLAR_OF_FROST_DURATION = 20000; // 20 seconds
    static constexpr uint32 UNBREAKABLE_ARMOR_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 UNBREAKABLE_ARMOR_DURATION = 20000; // 20 seconds
    static constexpr uint32 EMPOWER_RUNE_WEAPON_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 KILLING_MACHINE_DURATION = 30000; // 30 seconds
    static constexpr uint32 RIME_DURATION = 15000; // 15 seconds
    static constexpr uint32 FROST_STRIKE_RUNIC_POWER_COST = 40;
    static constexpr uint32 OBLITERATE_RUNIC_POWER_GENERATION = 25;
    static constexpr uint32 HOWLING_BLAST_RUNIC_POWER_GENERATION = 15;
    static constexpr float KILLING_MACHINE_PROC_CHANCE = 0.05f; // 5% per auto attack
    static constexpr float RIME_PROC_CHANCE = 0.15f; // 15% on Obliterate/Frost Strike
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 3; // 3+ targets for Howling Blast spam
    static constexpr float DUAL_WIELD_HIT_PENALTY = 0.19f; // 19% miss chance penalty
    static constexpr uint32 RUNIC_POWER_DUMP_THRESHOLD = 80; // Dump RP above 80
};

} // namespace Playerbot