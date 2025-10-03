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

class TC_GAME_API BloodSpecialization : public DeathKnightSpecialization
{
public:
    explicit BloodSpecialization(Player* bot);

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
    DeathKnightSpec GetSpecialization() const override { return DeathKnightSpec::BLOOD; }
    const char* GetSpecializationName() const override { return "Blood"; }

private:
    // Blood-specific mechanics
    void UpdateBloodMechanics();
    void UpdateTankingRotation(::Unit* target);
    void UpdateBloodShield();
    void UpdateDeathStrike();
    void UpdateVampiricBlood();
    void UpdateBoneShield();
    void UpdateDancingRuneWeapon();
    void UpdateHeartStrike(::Unit* target);
    bool ShouldCastHeartStrike(::Unit* target);
    bool ShouldCastDeathStrike(::Unit* target);
    bool ShouldCastBloodBoil();
    bool ShouldCastRuneStrike(::Unit* target);
    bool ShouldCastVampiricBlood();
    bool ShouldCastBoneShield();
    bool ShouldCastDancingRuneWeapon();
    bool ShouldCastIceboundFortitude();

    // Blood tanking mechanics
    void OptimizeBloodTanking(::Unit* target);
    void ManageThreatGeneration();
    void HandleBloodDefensives();
    void OptimizeDeathStrikeTiming(::Unit* target);
    bool IsInDangerousSituation();
    float CalculateIncomingDamage();
    void PredictHealthLoss();

    // Death Strike optimization
    void ManageDeathStrike();
    void CastDeathStrike(::Unit* target);
    void OptimizeDeathStrikeHealing(::Unit* target);
    bool ShouldPrioritizeDeathStrike(::Unit* target);
    uint32 CalculateDeathStrikeHeal(::Unit* target);
    void TrackRecentDamage();

    // Blood Shield mechanics
    void ManageBloodShield();
    void UpdateBloodShieldValue();
    uint32 GetBloodShieldAbsorb();
    bool ShouldRefreshBloodShield();
    void OptimizeBloodShieldUsage();

    // Vampiric Blood system
    void ManageVampiricBlood();
    void CastVampiricBlood();
    bool HasVampiricBlood();
    void OptimizeVampiricBloodUsage();
    float GetVampiricBloodBonus();

    // Bone Shield mechanics
    void ManageBoneShield();
    void CastBoneShield();
    uint32 GetBoneShieldCharges();
    bool ShouldRefreshBoneShield();
    void OptimizeBoneShieldUsage();

    // Dancing Rune Weapon system
    void ManageDancingRuneWeapon();
    void CastDancingRuneWeapon();
    bool HasDancingRuneWeapon();
    void OptimizeDancingRuneWeaponUsage();
    float GetDancingRuneWeaponBonus();

    // Blood Boil mechanics
    void ManageBloodBoil();
    void CastBloodBoil();
    bool ShouldUseBloodBoilForThreat();
    bool ShouldUseBloodBoilForDisease();
    void OptimizeBloodBoilTargeting();

    // Heart Strike optimization
    void ManageHeartStrike();
    void CastHeartStrike(::Unit* target);
    void OptimizeHeartStrikeTargets(::Unit* target);
    bool ShouldPrioritizeHeartStrike(::Unit* target);
    uint32 GetHeartStrikeTargetCount();

    // Rune Strike mechanics
    void ManageRuneStrike();
    void CastRuneStrike(::Unit* target);
    bool ShouldUseRuneStrike(::Unit* target);
    void OptimizeRuneStrikeUsage(::Unit* target);

    // Threat management
    void OptimizeThreatRotation(::Unit* target);
    void HandleThreatEmergency();
    void BuildInitialThreat(::Unit* target);
    void MaintainThreatLead();
    float CalculateCurrentThreat(::Unit* target);

    // Multi-target tanking
    void HandleMultiTargetTanking();
    void OptimizeAoEThreatGeneration();
    void ManageMultipleEnemies();
    bool ShouldUseAoEAbilities();
    void PrioritizeTankTargets();

    // Positioning for tanking
    void OptimizeBloodPositioning();
    void MaintainTankPosition();
    void HandleTankMovement();
    bool ShouldRepositionForTanking();
    void AvoidHazardsWhileTanking();

    // Emergency tanking abilities
    void HandleBloodEmergencies();
    void CastIceboundFortitude();
    void UseAntiMagicShell();
    void CastArmyOfTheDead();
    void UseEmergencyDefensives();

    // Blood spell IDs
    enum BloodSpells
    {
        HEART_STRIKE = 55050,
        DEATH_STRIKE = 49998,
        BLOOD_STRIKE = 45902,
        BLOOD_BOIL = 48721,
        RUNE_STRIKE = 56815,
        VAMPIRIC_BLOOD = 55233,
        BONE_SHIELD = 195181, // Updated for WoW 11.2
        DANCING_RUNE_WEAPON = 49028,
        ICEBOUND_FORTITUDE = 48792,
        ANTI_MAGIC_SHELL = 48707,
        ARMY_OF_THE_DEAD = 42650,
        WILL_OF_THE_NECROPOLIS = 52284,
        VAMPIRIC_AURA = 55610,
        ABOMINATIONS_MIGHT = 53137,
        BLOOD_PRESENCE = 48266,
        MARK_OF_BLOOD = 49005,
        HYSTERIA = 49016,
        CORPSE_EXPLOSION = 51328,
        BLOOD_WORMS = 50453,
        IMPROVED_BLOOD_PRESENCE = 50365,
        SPELL_DEFLECTION = 49145,
        VENDETTA = 49016,
        BLOOD_GORGED = 61154,
        IMPROVED_DEATH_STRIKE = 62905,
        SUDDEN_DOOM = 49018,
        SCENT_OF_BLOOD = 49005,
        BLOODWORM_INFESTATION = 50453,
        MIGHT_OF_MOGRAINE = 81340,
        SCARLET_FEVER = 81132
    };

    // Enhanced rune system
    std::atomic<uint32> _bloodRunes{2};
    std::atomic<uint32> _frostRunes{2};
    std::atomic<uint32> _unholyRunes{2};
    std::atomic<uint32> _deathRunes{0};
    std::atomic<uint32> _runicPower{0};
    std::atomic<uint32> _maxRunicPower{130};

    // Performance metrics
    struct BloodMetrics {
        std::atomic<uint32> totalDamageTaken{0};
        std::atomic<uint32> totalHealingDone{0};
        std::atomic<uint32> totalThreatGenerated{0};
        std::atomic<uint32> deathStrikeCasts{0};
        std::atomic<uint32> deathStrikeHealing{0};
        std::atomic<uint32> heartStrikeCasts{0};
        std::atomic<uint32> bloodBoilCasts{0};
        std::atomic<uint32> vampiricBloodUses{0};
        std::atomic<uint32> boneShieldChargesConsumed{0};
        std::atomic<uint32> dancingRuneWeaponUses{0};
        std::atomic<uint32> runicPowerGenerated{0};
        std::atomic<uint32> runicPowerSpent{0};
        std::atomic<float> selfHealingRatio{0.0f};
        std::atomic<float> threatEfficiency{0.0f};
        std::atomic<float> runeEfficiency{0.0f};
        std::atomic<float> bloodShieldUptime{0.0f};
        std::atomic<float> boneShieldUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageTaken = 0; totalHealingDone = 0; totalThreatGenerated = 0;
            deathStrikeCasts = 0; deathStrikeHealing = 0; heartStrikeCasts = 0;
            bloodBoilCasts = 0; vampiricBloodUses = 0; boneShieldChargesConsumed = 0;
            dancingRuneWeaponUses = 0; runicPowerGenerated = 0; runicPowerSpent = 0;
            selfHealingRatio = 0.0f; threatEfficiency = 0.0f; runeEfficiency = 0.0f;
            bloodShieldUptime = 0.0f; boneShieldUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _bloodMetrics;

    // Advanced damage tracking for Death Strike
    struct DamageTracker {
        std::queue<std::pair<uint32, uint32>> recentDamage; // <damage, timestamp>
        mutable std::mutex damageMutex;
        std::atomic<uint32> totalDamage{0};
        void RecordDamage(uint32 damage) {
            std::lock_guard<std::mutex> lock(damageMutex);
            uint32 currentTime = getMSTime();
            recentDamage.push({damage, currentTime});
            totalDamage += damage;

            // Remove damage older than 5 seconds
            while (!recentDamage.empty() && currentTime - recentDamage.front().second > 5000) {
                totalDamage -= recentDamage.front().first;
                recentDamage.pop();
            }
        }
        uint32 GetRecentDamage() const {
            std::lock_guard<std::mutex> lock(damageMutex);
            return totalDamage.load();
        }
        uint32 PredictDeathStrikeHeal() const {
            uint32 damage = GetRecentDamage();
            return std::min(damage * 25 / 100, 7500U); // 25% of damage taken, max 7500
        }
    } _damageTracker;

    // Blood Shield tracking
    struct BloodShieldTracker {
        std::atomic<uint32> currentAbsorb{0};
        std::atomic<uint32> maxAbsorb{0};
        std::atomic<uint32> lastRefresh{0};
        void UpdateShield(uint32 absorb) {
            currentAbsorb = absorb;
            maxAbsorb = std::max(maxAbsorb.load(), absorb);
            lastRefresh = getMSTime();
        }
        void ConsumeAbsorb(uint32 amount) {
            uint32 current = currentAbsorb.load();
            currentAbsorb = current > amount ? current - amount : 0;
        }
        bool ShouldRefresh() const {
            return currentAbsorb.load() < maxAbsorb.load() * 0.3f; // Refresh at 30%
        }
        float GetShieldPercent() const {
            uint32 max = maxAbsorb.load();
            return max > 0 ? (float)currentAbsorb.load() / max : 0.0f;
        }
    } _bloodShieldTracker;

    // Threat tracking system
    struct ThreatTracker {
        std::unordered_map<uint64, float> threatLevels;
        mutable std::mutex threatMutex;
        std::atomic<uint64> primaryTarget{0};
        void UpdateThreat(uint64 targetGuid, float threat) {
            std::lock_guard<std::mutex> lock(threatMutex);
            threatLevels[targetGuid] = threat;
        }
        float GetThreat(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(threatMutex);
            auto it = threatLevels.find(targetGuid);
            return it != threatLevels.end() ? it->second : 0.0f;
        }
        bool IsLosingThreat(uint64 targetGuid) const {
            return GetThreat(targetGuid) < 110.0f; // Losing threat if below 110%
        }
        void SetPrimaryTarget(uint64 targetGuid) {
            primaryTarget = targetGuid;
        }
        uint64 GetPrimaryTarget() const {
            return primaryTarget.load();
        }
    } _threatTracker;

    // Disease effectiveness tracker
    struct DiseaseEffectivenessTracker {
        std::unordered_map<uint64, uint32> bloodPlagueExpiry;
        std::unordered_map<uint64, uint32> frostFeverExpiry;
        mutable std::mutex diseaseMutex;
        void UpdateDisease(uint64 targetGuid, DiseaseType type, uint32 duration) {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            uint32 expiry = getMSTime() + duration;
            switch (type) {
                case DiseaseType::BLOOD_PLAGUE:
                    bloodPlagueExpiry[targetGuid] = expiry;
                    break;
                case DiseaseType::FROST_FEVER:
                    frostFeverExpiry[targetGuid] = expiry;
                    break;
                default:
                    break;
            }
        }
        bool HasDisease(uint64 targetGuid, DiseaseType type) const {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            uint32 currentTime = getMSTime();
            switch (type) {
                case DiseaseType::BLOOD_PLAGUE: {
                    auto it = bloodPlagueExpiry.find(targetGuid);
                    return it != bloodPlagueExpiry.end() && it->second > currentTime;
                }
                case DiseaseType::FROST_FEVER: {
                    auto it = frostFeverExpiry.find(targetGuid);
                    return it != frostFeverExpiry.end() && it->second > currentTime;
                }
                default:
                    return false;
            }
        }
        uint32 GetTimeRemaining(uint64 targetGuid, DiseaseType type) const {
            std::lock_guard<std::mutex> lock(diseaseMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = 0;
            switch (type) {
                case DiseaseType::BLOOD_PLAGUE: {
                    auto it = bloodPlagueExpiry.find(targetGuid);
                    expiry = it != bloodPlagueExpiry.end() ? it->second : 0;
                    break;
                }
                case DiseaseType::FROST_FEVER: {
                    auto it = frostFeverExpiry.find(targetGuid);
                    expiry = it != frostFeverExpiry.end() ? it->second : 0;
                    break;
                }
                default:
                    break;
            }
            return expiry > currentTime ? expiry - currentTime : 0;
        }
        bool ShouldRefresh(uint64 targetGuid, DiseaseType type) const {
            return GetTimeRemaining(targetGuid, type) <= 6000; // Refresh with 6s remaining
        }
    } _diseaseTracker;

    // Blood buff tracking
    uint32 _lastVampiricBlood;
    uint32 _lastBoneShield;
    uint32 _lastDancingRuneWeapon;
    uint32 _lastIceboundFortitude;
    uint32 _lastAntiMagicShell;
    std::atomic<bool> _vampiricBloodActive{false};
    std::atomic<uint32> _boneShieldCharges{0};
    std::atomic<bool> _dancingRuneWeaponActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Blood mechanics
    void OptimizeBloodRotation(::Unit* target);
    void HandleBloodCooldowns();
    void ManageRuneEfficiency();
    void OptimizeDeathStrikeWindows();
    void HandleTankingEmergencies();
    void PredictDamageSpikes();
    float CalculateBloodEfficiency();

    // Enhanced constants
    static constexpr float TANK_RANGE = 5.0f;
    static constexpr uint32 VAMPIRIC_BLOOD_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 VAMPIRIC_BLOOD_DURATION = 10000; // 10 seconds
    static constexpr uint32 BONE_SHIELD_DURATION = 300000; // 5 minutes
    static constexpr uint32 BONE_SHIELD_MAX_CHARGES = 4;
    static constexpr uint32 DANCING_RUNE_WEAPON_COOLDOWN = 90000; // 1.5 minutes
    static constexpr uint32 DANCING_RUNE_WEAPON_DURATION = 17000; // 17 seconds
    static constexpr uint32 ICEBOUND_FORTITUDE_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 ANTI_MAGIC_SHELL_COOLDOWN = 45000; // 45 seconds
    static constexpr uint32 DEATH_STRIKE_RUNIC_POWER_COST = 40;
    static constexpr uint32 RUNE_STRIKE_RUNIC_POWER_COST = 20;
    static constexpr float THREAT_CRITICAL_THRESHOLD = 105.0f; // Emergency threat threshold
    static constexpr float HEALTH_EMERGENCY_THRESHOLD = 35.0f; // Use emergency defensives
    static constexpr float HEALTH_DEFENSIVE_THRESHOLD = 60.0f; // Use defensives below 60%
    static constexpr uint32 RECENT_DAMAGE_WINDOW = 5000; // 5 seconds for Death Strike calculation
    static constexpr float DEATH_STRIKE_HEAL_RATIO = 0.25f; // 25% of recent damage
    static constexpr uint32 DEATH_STRIKE_MAX_HEAL = 7500; // Maximum Death Strike heal
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 3; // 3+ targets for AoE abilities
    static constexpr uint32 BLOOD_SHIELD_REFRESH_THRESHOLD = 30; // Refresh at 30% shield
};

} // namespace Playerbot