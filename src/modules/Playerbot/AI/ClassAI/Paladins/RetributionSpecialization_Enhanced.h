/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PaladinSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API RetributionPaladinSpecialization : public PaladinSpecialization
{
public:
    explicit RetributionPaladinSpecialization(Player* bot);

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

    // Aura management
    void UpdateAura() override;
    PaladinAura GetOptimalAura() override;
    void SwitchAura(PaladinAura aura) override;

    // Specialization info
    PaladinSpec GetSpecialization() const override { return PaladinSpec::RETRIBUTION; }
    const char* GetSpecializationName() const override { return "Retribution"; }

private:
    // Retribution-specific mechanics
    void UpdateRetributionMechanics();
    void UpdateDPSRotation();
    void UpdateHolyPower();
    void UpdateZealotry();
    void UpdateArtOfWar();
    void UpdateSealTwisting();
    void UpdateInquisition();
    void UpdateGuardianOfAncientKings();
    bool ShouldCastCrusaderStrike(::Unit* target);
    bool ShouldCastTemplarsVerdict(::Unit* target);
    bool ShouldCastDivineStorm();
    bool ShouldCastExorcism(::Unit* target);
    bool ShouldCastHammerOfWrath(::Unit* target);
    bool ShouldCastConsecration();
    bool ShouldCastInquisition();
    bool ShouldCastZealotry();
    bool ShouldCastGuardianOfAncientKings();

    // DPS optimization system
    void OptimizeDPSRotation();
    void PrioritizeDPSTargets();
    void ManageDPSCooldowns();
    void HandleBurstPhase();
    void CalculateDPSEfficiency();
    float GetDPSPriority(::Unit* target);
    bool IsBurstPhaseActive();
    void TriggerBurstCooldowns();

    // Holy Power system for DPS
    void ManageRetributionHolyPower();
    void BuildHolyPowerThroughAttacks();
    void SpendHolyPowerOnDPS(::Unit* target);
    void OptimizeHolyPowerForDPS();
    bool ShouldSpendHolyPowerOnTemplarsVerdict();
    bool ShouldSpendHolyPowerOnDivineStorm();
    bool ShouldSpendHolyPowerOnInquisition();

    // Zealotry mechanics
    void ManageZealotry();
    void BuildZealotryStacks();
    void ConsumeZealotryStacks();
    uint32 GetZealotryStacks();
    bool ShouldUseZealotry();
    void OptimizeZealotryUsage();

    // Art of War procs
    void ManageArtOfWar();
    bool HasArtOfWarProc();
    void ConsumeArtOfWarProc();
    void OptimizeArtOfWarUsage();
    bool ShouldUseArtOfWarProc(uint32 spellId);

    // Seal management for DPS
    void ManageSealsForDPS();
    void CastSealOfTruth();
    void CastSealOfRighteousness();
    void CastSealOfCommand();
    void OptimizeSealForSituation();
    bool ShouldTwistSeals();
    void PerformSealTwist();

    // Judgement optimization
    void ManageJudgements();
    void CastJudgement(::Unit* target);
    void OptimizeJudgementTargets();
    bool ShouldJudgeForDPS();
    uint32 GetOptimalJudgementSeal();

    // Inquisition management
    void ManageInquisition();
    void CastInquisition();
    bool ShouldMaintainInquisition();
    void OptimizeInquisitionTiming();
    uint32 GetInquisitionTimeRemaining();
    bool ShouldRefreshInquisition();

    // Multi-target DPS
    void HandleMultiTargetDPS();
    void OptimizeDivineStormUsage();
    void CastDivineStorm();
    bool ShouldUseDivineStorm();
    void ApplyConsecrationForAoE();
    void HandleAoERotation();

    // Execute phase mechanics
    void HandleExecutePhase(::Unit* target);
    bool IsTargetInExecuteRange(::Unit* target);
    void OptimizeHammerOfWrath(::Unit* target);
    void ManageExecuteRotation(::Unit* target);

    // Retribution offensive cooldowns
    void HandleRetributionCooldowns();
    void CastAvengedWrath();
    void CastGuardianOfAncientKings();
    void UseZealotry();
    void CastDivineProtection();
    void OptimizeCooldownUsage();

    // Positioning for melee DPS
    void OptimizePositioningForDPS();
    void MaintainMeleeRange(::Unit* target);
    void HandleMeleeMovement();
    bool ShouldMoveToTarget(::Unit* target);
    void AvoidMeleeHazards();

    // Mana management for Retribution
    void OptimizeRetributionMana();
    void ConserveManaForBurst();
    void ManageDPSEfficiency();
    bool ShouldPrioritizeManaForDPS();
    float CalculateManaForRetribution();

    // Self-healing during combat
    void HandleRetributionSelfHealing();
    void CastWordOfGlory();
    void UseArtOfWarForHealing();
    bool ShouldSelfHeal();
    ::Unit* GetBestHealTarget();

    // Retribution spell IDs
    enum RetributionSpells
    {
        CRUSADER_STRIKE = 35395,
        TEMPLARS_VERDICT = 85256,
        DIVINE_STORM = 53385,
        EXORCISM = 879,
        HAMMER_OF_WRATH = 24275,
        CONSECRATION = 26573,
        INQUISITION = 84963,
        ZEALOTRY = 85696,
        AVENGED_WRATH = 31884,
        GUARDIAN_OF_ANCIENT_KINGS = 86698,
        ART_OF_WAR = 53489,
        PURSUIT_OF_JUSTICE = 26022,
        HEART_OF_THE_CRUSADER = 20335,
        IMPROVED_SANCTITY_AURA = 31869,
        SANCTIFIED_WRATH = 53375,
        SWIFT_RETRIBUTION = 53379,
        COMMUNION = 31876,
        EYE_FOR_AN_EYE = 9799,
        VINDICATION = 9452,
        CONVICTION = 20117,
        SEAL_OF_TRUTH = 31801,
        SEAL_OF_RIGHTEOUSNESS = 25742,
        SEAL_OF_COMMAND = 20375,
        SEAL_OF_JUSTICE = 20164,
        JUDGEMENT = 20271,
        WORD_OF_GLORY = 85673,
        DIVINE_PURPOSE = 86172,
        INQUIRY_OF_FAITH = 84963,
        LONG_ARM_OF_THE_LAW = 87168,
        REBUKE = 96231,
        SHIELD_OF_VENGEANCE = 184662,
        DIVINE_PROTECTION = 498
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _holyPower{0};
    std::atomic<uint32> _maxHolyPower{3};
    std::atomic<uint32> _zealotryStacks{0};
    std::atomic<bool> _artOfWarActive{false};
    std::atomic<bool> _zealotryActive{false};
    std::atomic<uint32> _zealotryEndTime{0};
    std::atomic<bool> _avengedWrathActive{false};
    std::atomic<uint32> _avengedWrathEndTime{0};

    // Performance metrics
    struct RetributionMetrics {
        std::atomic<uint32> totalDamageDealt{0};
        std::atomic<uint32> meleeDamage{0};
        std::atomic<uint32> spellDamage{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> holyPowerGenerated{0};
        std::atomic<uint32> holyPowerSpent{0};
        std::atomic<uint32> templarsVerdictCasts{0};
        std::atomic<uint32> divineStormCasts{0};
        std::atomic<uint32> zealotryUses{0};
        std::atomic<uint32> artOfWarProcs{0};
        std::atomic<uint32> judgementCasts{0};
        std::atomic<uint32> hammerOfWrathCasts{0};
        std::atomic<float> dpsEfficiency{0.0f};
        std::atomic<float> holyPowerEfficiency{0.0f};
        std::atomic<float> zealotryUptime{0.0f};
        std::atomic<float> inquisitionUptime{0.0f};
        std::atomic<float> artOfWarProcRate{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageDealt = 0; meleeDamage = 0; spellDamage = 0;
            manaSpent = 0; holyPowerGenerated = 0; holyPowerSpent = 0;
            templarsVerdictCasts = 0; divineStormCasts = 0; zealotryUses = 0;
            artOfWarProcs = 0; judgementCasts = 0; hammerOfWrathCasts = 0;
            dpsEfficiency = 0.0f; holyPowerEfficiency = 0.0f; zealotryUptime = 0.0f;
            inquisitionUptime = 0.0f; artOfWarProcRate = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _retributionMetrics;

    // Advanced Inquisition tracking
    struct InquisitionTracker {
        std::atomic<bool> isActive{false};
        std::atomic<uint32> expiry{0};
        std::atomic<uint32> lastCast{0};
        std::atomic<uint32> holyPowerUsed{0};
        void Cast(uint32 holyPower, uint32 baseDuration) {
            isActive = true;
            holyPowerUsed = holyPower;
            uint32 duration = baseDuration + (holyPower - 1) * 10000; // +10s per HP beyond first
            expiry = getMSTime() + duration;
            lastCast = getMSTime();
        }
        bool IsActive() const {
            return isActive.load() && expiry.load() > getMSTime();
        }
        uint32 GetTimeRemaining() const {
            uint32 currentTime = getMSTime();
            uint32 expiryTime = expiry.load();
            return expiryTime > currentTime ? expiryTime - currentTime : 0;
        }
        bool ShouldRefresh(uint32 refreshThreshold = 5000) const {
            return GetTimeRemaining() <= refreshThreshold;
        }
        float GetDamageBonus() const {
            return IsActive() ? 0.30f : 0.0f; // 30% damage bonus
        }
    } _inquisitionTracker;

    // Seal tracking system
    struct SealTracker {
        std::atomic<uint32> currentSeal{0};
        std::atomic<uint32> lastSealCast{0};
        std::atomic<uint32> sealTwistCount{0};
        mutable std::mutex sealMutex;
        void SetSeal(uint32 sealId) {
            std::lock_guard<std::mutex> lock(sealMutex);
            if (currentSeal.load() != sealId && currentSeal.load() != 0)
                sealTwistCount++;
            currentSeal = sealId;
            lastSealCast = getMSTime();
        }
        uint32 GetCurrentSeal() const {
            std::lock_guard<std::mutex> lock(sealMutex);
            return currentSeal.load();
        }
        bool CanTwistSeals() const {
            std::lock_guard<std::mutex> lock(sealMutex);
            return getMSTime() - lastSealCast.load() >= 1500; // 1.5s GCD
        }
        uint32 GetSealTwistCount() const {
            return sealTwistCount.load();
        }
    } _sealTracker;

    // Burst phase manager
    struct BurstPhaseManager {
        std::atomic<bool> burstActive{false};
        std::atomic<uint32> burstStartTime{0};
        std::atomic<uint32> burstDuration{0};
        std::atomic<uint32> cooldownsUsed{0};
        void StartBurst(uint32 duration) {
            burstActive = true;
            burstStartTime = getMSTime();
            burstDuration = duration;
            cooldownsUsed = 0;
        }
        void EndBurst() {
            burstActive = false;
            burstStartTime = 0;
            burstDuration = 0;
            cooldownsUsed = 0;
        }
        bool IsBurstActive() const {
            if (!burstActive.load()) return false;
            uint32 elapsed = getMSTime() - burstStartTime.load();
            return elapsed < burstDuration.load();
        }
        bool ShouldUseCooldown() const {
            return IsBurstActive() && cooldownsUsed.load() < 3; // Max 3 cooldowns per burst
        }
        void UseCooldown() {
            if (IsBurstActive())
                cooldownsUsed++;
        }
        uint32 GetBurstTimeRemaining() const {
            if (!IsBurstActive()) return 0;
            uint32 elapsed = getMSTime() - burstStartTime.load();
            uint32 duration = burstDuration.load();
            return duration > elapsed ? duration - elapsed : 0;
        }
    } _burstPhaseManager;

    // Retribution buff tracking
    uint32 _lastZealotry;
    uint32 _lastAvengedWrath;
    uint32 _lastGuardianOfAncientKings;
    uint32 _lastInquisition;
    uint32 _lastDivineProtection;
    std::atomic<bool> _guardianOfAncientKingsActive{false};
    std::atomic<bool> _divineProtectionActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Retribution mechanics
    void OptimizeRetributionRotation(::Unit* target);
    void HandleRetributionCooldownPhases();
    void ManageHolyPowerPriorities();
    void OptimizeSealTwistTiming();
    void HandleInquisitionPandemic();
    void ManageExecutePhasePriorities();
    float CalculateRetributionEfficiency();

    // Enhanced constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 HOLY_POWER_MAX = 3;
    static constexpr uint32 ZEALOTRY_MAX_STACKS = 3;
    static constexpr uint32 ZEALOTRY_DURATION = 20000; // 20 seconds
    static constexpr uint32 INQUISITION_BASE_DURATION = 20000; // 20 seconds
    static constexpr uint32 AVENGED_WRATH_DURATION = 20000; // 20 seconds
    static constexpr uint32 ART_OF_WAR_DURATION = 15000; // 15 seconds
    static constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS_DURATION = 30000; // 30 seconds
    static constexpr uint32 ZEALOTRY_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 AVENGED_WRATH_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 HAMMER_OF_WRATH_COOLDOWN = 45000; // 45 seconds
    static constexpr uint32 CRUSADER_STRIKE_MANA_COST = 150;
    static constexpr uint32 TEMPLARS_VERDICT_MANA_COST = 0; // Uses Holy Power
    static constexpr uint32 DIVINE_STORM_MANA_COST = 0; // Uses Holy Power
    static constexpr uint32 EXORCISM_MANA_COST = 200;
    static constexpr uint32 HAMMER_OF_WRATH_MANA_COST = 180;
    static constexpr uint32 CONSECRATION_MANA_COST = 450;
    static constexpr uint32 INQUISITION_MANA_COST = 0; // Uses Holy Power
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 20.0f; // Execute phase below 20%
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 4; // 4+ targets for Divine Storm
    static constexpr float RETRIBUTION_MANA_THRESHOLD = 15.0f; // Conservative mana usage below 15%
    static constexpr uint32 HOLY_POWER_EMERGENCY_THRESHOLD = 2; // Save HP for burst
    static constexpr float INQUISITION_REFRESH_THRESHOLD = 5.0f; // Refresh Inquisition with 5s remaining
    static constexpr uint32 BURST_COOLDOWN_THRESHOLD = 30000; // Use burst cooldowns within 30s
    static constexpr float ART_OF_WAR_PROC_CHANCE = 0.20f; // 20% proc chance on crits
};

} // namespace Playerbot