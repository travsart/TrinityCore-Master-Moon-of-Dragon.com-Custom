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

class TC_GAME_API UnholySpecialization : public DeathKnightSpecialization
{
public:
    explicit UnholySpecialization(Player* bot);

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
    DeathKnightSpec GetSpecialization() const override { return DeathKnightSpec::UNHOLY; }
    const char* GetSpecializationName() const override { return "Unholy"; }

private:
    // Unholy-specific mechanics
    void UpdateUnholyMechanics();
    void UpdateDiseaseTracking();
    void UpdateSuddenDoom();
    void UpdateDarkTransformation();
    void UpdateSummonGargoyle();
    void UpdateCorpseExplosion();
    bool ShouldCastDeathCoil(::Unit* target);
    bool ShouldCastDeathStrike(::Unit* target);
    bool ShouldCastPlagueStrike(::Unit* target);
    bool ShouldCastIcyTouch(::Unit* target);
    bool ShouldCastScourgeStrike(::Unit* target);
    bool ShouldCastDarkTransformation();
    bool ShouldCastSummonGargoyle();
    bool ShouldCastAntiMagicShell();

    // Minion management
    void ManageUnholyMinions();
    void SummonGhoul();
    void ControlGhoul();
    void OptimizeGhoulPositioning();
    void HandleGhoulAttacks();
    void ManageSkeletalMinions();
    bool HasActiveGhoul();
    bool HasActiveGargoyle();
    void CommandMinionAttack(::Unit* target);

    // Disease spread mechanics
    void ManageDiseaseSpread();
    void CastPestilence();
    void OptimizeDiseaseApplication();
    void PrioritizeDiseaseTargets();
    bool ShouldSpreadDiseases();
    uint32 GetDiseasedTargetCount();
    void ApplyBothDiseases(::Unit* target);

    // Death Coil optimization
    void ManageDeathCoil();
    void CastDeathCoil(::Unit* target);
    void OptimizeDeathCoilUsage(::Unit* target);
    bool ShouldHealWithDeathCoil(::Unit* target);
    bool ShouldDamageWithDeathCoil(::Unit* target);
    uint32 CalculateDeathCoilHealing(::Unit* target);

    // Scourge Strike mechanics
    void ManageScourgeStrike();
    void CastScourgeStrike(::Unit* target);
    void OptimizeScourgeStrikeUsage(::Unit* target);
    uint32 CalculateScourgeStrikeDamage(::Unit* target);
    bool ShouldPrioritizeScourgeStrike(::Unit* target);

    // Bone Armor management
    void ManageBoneArmor();
    void CastBoneArmor();
    void RefreshBoneArmor();
    bool HasBoneArmor();
    uint32 GetBoneArmorCharges();

    // Sudden Doom proc system
    void ManageSuddenDoom();
    bool HasSuddenDoomProc();
    void ConsumeSuddenDoomProc();
    void OptimizeSuddenDoomUsage();
    void CastFreeDeathCoil(::Unit* target);

    // Dark Transformation system
    void ManageDarkTransformation();
    void CastDarkTransformation();
    bool IsDarkTransformationActive();
    void OptimizeDarkTransformationTiming();
    float GetDarkTransformationBonus();

    // Gargoyle summoning
    void ManageSummonGargoyle();
    void CastSummonGargoyle();
    void OptimizeGargoyleTiming();
    void ControlGargoyle(::Unit* target);
    bool ShouldSummonGargoyle();

    // Corpse Explosion mechanics
    void ManageCorpseExplosion();
    void CastCorpseExplosion();
    void OptimizeCorpseExplosionTargeting();
    bool ShouldUseCorpseExplosion();
    uint32 GetNearbyCorpseCount();

    // Multi-target Unholy DPS
    void HandleMultiTargetUnholy();
    void OptimizeAoEDiseases();
    void UsePestilenceForSpread();
    bool ShouldUseAoEAbilities();
    void HandleUnholyAoE();

    // Anti-Magic Shell usage
    void ManageAntiMagicShell();
    void CastAntiMagicShell();
    void OptimizeAntiMagicShellTiming();
    bool ShouldUseAntiMagicShell();
    bool IsIncomingMagicDamage();

    // Unholy spell IDs
    enum UnholySpells
    {
        DEATH_COIL = 47541,
        PLAGUE_STRIKE = 45462,
        ICY_TOUCH = 45477,
        SCOURGE_STRIKE = 55090,
        DEATH_STRIKE = 49998,
        PESTILENCE = 50842,
        BONE_ARMOR = 195181, // Updated for WoW 11.2
        RAISE_DEAD = 46584,
        DARK_TRANSFORMATION = 63560,
        SUMMON_GARGOYLE = 49206,
        CORPSE_EXPLOSION = 49158,
        ANTI_MAGIC_SHELL = 48707,
        SUDDEN_DOOM = 49530,
        UNHOLY_PRESENCE = 48265,
        MAGIC_SUPPRESSION = 49224,
        NECROSIS = 51460,
        RAVENOUS_DEAD = 51468,
        NIGHT_OF_THE_DEAD = 51405,
        MASTER_OF_GHOULS = 52143,
        UNHOLY_FRENZY = 49016,
        CRYPT_FEVER = 49032,
        BONE_PRISON = 49203,
        DESECRATION = 55666,
        WANDERING_PLAGUE = 49217,
        EBON_PLAGUEBRINGER = 51160,
        RAGE_OF_RIVENDARE = 51099,
        SHADOW_INFUSION = 91342,
        DARK_ARBITER = 207349
    };

    // Enhanced rune system for Unholy
    std::atomic<uint32> _bloodRunes{2};
    std::atomic<uint32> _frostRunes{2};
    std::atomic<uint32> _unholyRunes{2};
    std::atomic<uint32> _deathRunes{0};
    std::atomic<uint32> _runicPower{0};
    std::atomic<uint32> _maxRunicPower{130};

    // Disease tracking system
    struct DiseaseTracker {
        std::unordered_map<uint64, uint32> frostFeverTargets;
        std::unordered_map<uint64, uint32> bloodPlagueTargets;
        std::unordered_map<uint64, uint32> cryptFeverTargets;
        mutable std::mutex diseaseMutex;

        void ApplyDisease(uint64 targetGuid, uint32 spellId, uint32 duration) {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            uint32 expireTime = getMSTime() + duration;
            switch (spellId) {
                case 45477: // Icy Touch -> Frost Fever
                    frostFeverTargets[targetGuid] = expireTime;
                    break;
                case 45462: // Plague Strike -> Blood Plague
                    bloodPlagueTargets[targetGuid] = expireTime;
                    break;
                case 49032: // Crypt Fever
                    cryptFeverTargets[targetGuid] = expireTime;
                    break;
            }
        }

        bool HasDisease(uint64 targetGuid, uint32 spellId) const {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            uint32 currentTime = getMSTime();
            switch (spellId) {
                case 45477:
                    return frostFeverTargets.count(targetGuid) &&
                           frostFeverTargets.at(targetGuid) > currentTime;
                case 45462:
                    return bloodPlagueTargets.count(targetGuid) &&
                           bloodPlagueTargets.at(targetGuid) > currentTime;
                case 49032:
                    return cryptFeverTargets.count(targetGuid) &&
                           cryptFeverTargets.at(targetGuid) > currentTime;
            }
            return false;
        }

        bool HasBothDiseases(uint64 targetGuid) const {
            return HasDisease(targetGuid, 45477) && HasDisease(targetGuid, 45462);
        }

        uint32 GetDiseasedTargetCount() const {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            std::set<uint64> uniqueTargets;
            uint32 currentTime = getMSTime();

            for (const auto& pair : frostFeverTargets) {
                if (pair.second > currentTime)
                    uniqueTargets.insert(pair.first);
            }
            for (const auto& pair : bloodPlagueTargets) {
                if (pair.second > currentTime)
                    uniqueTargets.insert(pair.first);
            }

            return uniqueTargets.size();
        }

        void CleanupExpiredDiseases() {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            uint32 currentTime = getMSTime();

            auto it = frostFeverTargets.begin();
            while (it != frostFeverTargets.end()) {
                if (it->second <= currentTime)
                    it = frostFeverTargets.erase(it);
                else
                    ++it;
            }

            it = bloodPlagueTargets.begin();
            while (it != bloodPlagueTargets.end()) {
                if (it->second <= currentTime)
                    it = bloodPlagueTargets.erase(it);
                else
                    ++it;
            }
        }
    } _diseaseTracker;

    // Performance metrics
    struct UnholyMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> diseaseSpreadCount{0};
        std::atomic<uint32> deathCoilCasts{0};
        std::atomic<uint32> deathCoilHealing{0};
        std::atomic<uint32> scourgeStrikeCasts{0};
        std::atomic<uint32> scourgeStrikeCrits{0};
        std::atomic<uint32> pestilenceUses{0};
        std::atomic<uint32> suddenDoomProcs{0};
        std::atomic<uint32> suddenDoomUsed{0};
        std::atomic<uint32> darkTransformationUses{0};
        std::atomic<uint32> gargoyleSummons{0};
        std::atomic<uint32> corpseExplosions{0};
        std::atomic<uint32> runicPowerGenerated{0};
        std::atomic<uint32> runicPowerSpent{0};
        std::atomic<float> diseaseUptimePercent{0.0f};
        std::atomic<float> suddenDoomProcRate{0.0f};
        std::atomic<float> minionDamageContribution{0.0f};
        std::atomic<float> criticalStrikeRate{0.0f};
        std::atomic<float> runeEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; diseaseSpreadCount = 0; deathCoilCasts = 0;
            deathCoilHealing = 0; scourgeStrikeCasts = 0; scourgeStrikeCrits = 0;
            pestilenceUses = 0; suddenDoomProcs = 0; suddenDoomUsed = 0;
            darkTransformationUses = 0; gargoyleSummons = 0; corpseExplosions = 0;
            runicPowerGenerated = 0; runicPowerSpent = 0; diseaseUptimePercent = 0.0f;
            suddenDoomProcRate = 0.0f; minionDamageContribution = 0.0f;
            criticalStrikeRate = 0.0f; runeEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _unholyMetrics;

    // Sudden Doom proc tracker
    struct SuddenDoomTracker {
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
    } _suddenDoomTracker;

    // Minion tracking
    struct MinionTracker {
        std::atomic<uint64> ghoulGuid{0};
        std::atomic<uint64> gargoyleGuid{0};
        std::atomic<uint32> ghoulSummonTime{0};
        std::atomic<uint32> gargoyleSummonTime{0};
        std::atomic<bool> darkTransformationActive{false};
        std::atomic<uint32> darkTransformationExpiry{0};

        bool HasGhoul() const { return ghoulGuid.load() != 0; }
        bool HasGargoyle() const { return gargoyleGuid.load() != 0; }
        bool IsDarkTransformationActive() const {
            return darkTransformationActive.load() &&
                   getMSTime() < darkTransformationExpiry.load();
        }

        void SummonGhoul(uint64 guid) {
            ghoulGuid = guid;
            ghoulSummonTime = getMSTime();
        }

        void SummonGargoyle(uint64 guid) {
            gargoyleGuid = guid;
            gargoyleSummonTime = getMSTime();
        }

        void ActivateDarkTransformation(uint32 duration) {
            darkTransformationActive = true;
            darkTransformationExpiry = getMSTime() + duration;
        }
    } _minionTracker;

    // Unholy buff tracking
    uint32 _lastBoneArmor;
    uint32 _lastAntiMagicShell;
    uint32 _lastDarkTransformation;
    uint32 _lastSummonGargoyle;
    std::atomic<bool> _boneArmorActive{false};
    std::atomic<bool> _antiMagicShellActive{false};
    std::atomic<uint32> _boneArmorCharges{15};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Unholy mechanics
    void OptimizeUnholyRotation(::Unit* target);
    void HandleUnholyCooldowns();
    void ManageDiseaseEfficiency();
    void OptimizeRuneUsage();
    void HandleMinionOptimization();
    float CalculateUnholyDPS();

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 DARK_TRANSFORMATION_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 DARK_TRANSFORMATION_DURATION = 30000; // 30 seconds
    static constexpr uint32 SUMMON_GARGOYLE_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 GARGOYLE_DURATION = 30000; // 30 seconds
    static constexpr uint32 ANTI_MAGIC_SHELL_COOLDOWN = 45000; // 45 seconds
    static constexpr uint32 ANTI_MAGIC_SHELL_DURATION = 5000; // 5 seconds
    static constexpr uint32 SUDDEN_DOOM_DURATION = 30000; // 30 seconds
    static constexpr uint32 BONE_ARMOR_DURATION = 300000; // 5 minutes
    static constexpr uint32 DEATH_COIL_RUNIC_POWER_COST = 40;
    static constexpr uint32 SCOURGE_STRIKE_RUNIC_POWER_GENERATION = 15;
    static constexpr uint32 DEATH_STRIKE_RUNIC_POWER_GENERATION = 15;
    static constexpr float SUDDEN_DOOM_PROC_CHANCE = 0.15f; // 15% per auto attack
    static constexpr uint32 DISEASE_DURATION = 21000; // 21 seconds
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 3; // 3+ targets for Pestilence
    static constexpr uint32 RUNIC_POWER_DUMP_THRESHOLD = 80; // Dump RP above 80
    static constexpr uint32 BONE_ARMOR_MAX_CHARGES = 15;
};

} // namespace Playerbot