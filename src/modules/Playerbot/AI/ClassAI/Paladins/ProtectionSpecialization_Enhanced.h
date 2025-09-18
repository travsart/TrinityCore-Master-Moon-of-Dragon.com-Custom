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

class TC_GAME_API ProtectionPaladinSpecialization : public PaladinSpecialization
{
public:
    explicit ProtectionPaladinSpecialization(Player* bot);

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
    PaladinSpec GetSpecialization() const override { return PaladinSpec::PROTECTION; }
    const char* GetSpecializationName() const override { return "Protection"; }

private:
    // Protection-specific mechanics
    void UpdateProtectionMechanics();
    void UpdateTankingRotation();
    void UpdateThreatManagement();
    void UpdateDefensiveCooldowns();
    void UpdateHolyPower();
    void UpdateShieldOfVengeance();
    void UpdateArdentDefender();
    void UpdateGuardianOfAncientKings();
    bool ShouldCastHammerOfTheRighteous(::Unit* target);
    bool ShouldCastShieldOfTheRighteous();
    bool ShouldCastAvengersShield(::Unit* target);
    bool ShouldCastConsecration();
    bool ShouldCastHolyWrath();
    bool ShouldCastWordOfGlory(::Unit* target);
    bool ShouldCastArdentDefender();
    bool ShouldCastGuardianOfAncientKings();

    // Threat generation optimization
    void OptimizeThreatGeneration();
    void PrioritizeThreatTargets();
    void ManageAggro();
    void HandleThreatEmergency();
    bool ShouldTaunt(::Unit* target);
    void CastRighteousFury(::Unit* target);
    void CastHandOfReckoning(::Unit* target);
    float CalculateThreatLevel(::Unit* target);
    void UpdateThreatPriorities();

    // Holy Power system for tanking
    void ManageProtectionHolyPower();
    void BuildHolyPowerThroughCombat();
    void SpendHolyPowerOnDefense();
    void OptimizeHolyPowerForTanking();
    bool ShouldSpendHolyPowerOnShieldOfTheRighteous();
    bool ShouldSpendHolyPowerOnWordOfGlory();

    // Shield mechanics
    void ManageShieldUsage();
    void CastShieldOfTheRighteous();
    void CastAvengersShield(::Unit* target);
    void OptimizeShieldCooldowns();
    bool ShouldBlockAttack();
    float CalculateBlockValue();

    // Defensive cooldown management
    void HandleProtectionDefensives();
    void CastArdentDefender();
    void CastGuardianOfAncientKings();
    void CastDivineProtection();
    void CastLayOnHands(::Unit* target);
    void UseDivineShield();
    void CooldownPriority();
    bool IsInDanger();
    float GetHealthPercent();

    // Consecration management
    void ManageConsecration();
    void CastConsecration();
    bool ShouldUseConsecration();
    void OptimizeConsecrationTiming();
    bool IsConsecrationActive();

    // Multi-target tanking
    void HandleMultiTargetTanking();
    void OptimizeAoEThreat();
    void ManageMultipleThreats();
    void PrioritizeTankTargets();
    bool ShouldUseAoEAbilities();
    void HandleAoEThreatGeneration();

    // Seal and Judgement for tanking
    void ManageTankingSealsAndJudgements();
    void CastSealOfInsight();
    void CastSealOfTruth();
    void CastJudgement(::Unit* target);
    void OptimizeSealForSituation();
    bool ShouldJudgeForThreat();
    bool ShouldJudgeForHealing();

    // Positioning and movement
    void OptimizePositioningForTanking();
    void MaintainTankPosition();
    void HandleMobPositioning();
    bool ShouldMoveToOptimalPosition();
    void AvoidCleaveAttacks();
    void PositionForGroupProtection();

    // Emergency tanking abilities
    void HandleTankingEmergencies();
    void UseEmergencyDefensives();
    void CastDivineShield();
    void UseBlessingOfProtection(::Unit* target);
    void CastTaunt(::Unit* target);
    void HandleTankDeath();

    // Mana management for tanking
    void OptimizeProtectionMana();
    void ConserveManaForDefensives();
    void ManageTankingEfficiency();
    bool ShouldPrioritizeManaForDefensives();
    float CalculateManaForTanking();

    // Protection spell IDs
    enum ProtectionSpells
    {
        HAMMER_OF_THE_RIGHTEOUS = 53595,
        SHIELD_OF_THE_RIGHTEOUS = 53600,
        AVENGERS_SHIELD = 31935,
        CONSECRATION = 26573,
        HOLY_WRATH = 2812,
        RIGHTEOUS_FURY = 25780,
        HAND_OF_RECKONING = 62124,
        ARDENT_DEFENDER = 31850,
        GUARDIAN_OF_ANCIENT_KINGS = 86659,
        DIVINE_GUARDIAN = 70940,
        SHIELD_OF_VENGEANCE = 184662,
        BLESSED_HAMMER = 204019,
        GRAND_CRUSADER = 85043,
        REDOUBT = 20128,
        RECKONING = 20177,
        SANCTUARY = 20375,
        IMPROVED_RIGHTEOUS_FURY = 25956,
        SPELL_WARDING = 31230,
        SACRED_DUTY = 85433,
        GUARDED_BY_THE_LIGHT = 53592,
        SHIELD_OF_THE_TEMPLAR = 85512,
        JUDGEMENTS_OF_THE_JUST = 53695,
        HAMMER_OF_WRATH = 24275,
        WORD_OF_GLORY = 85673,
        SEAL_OF_INSIGHT = 20165,
        SEAL_OF_TRUTH = 31801,
        LAY_ON_HANDS = 633,
        DIVINE_PROTECTION = 498,
        DIVINE_SHIELD = 642,
        BLESSING_OF_PROTECTION = 1022
    };

    // Threat priority system
    enum class ThreatPriority
    {
        CRITICAL,    // Immediate threat loss
        HIGH,        // Dangerous threat level
        MODERATE,    // Normal threat management
        LOW,         // Stable threat
        EXCESS       // Over-threat (can assist others)
    };

    struct ThreatTarget
    {
        uint64 guid;
        float threatLevel;
        ThreatPriority priority;
        uint32 lastTaunt;
        bool isDangerous;
        float distanceToBot;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _holyPower{0};
    std::atomic<uint32> _maxHolyPower{3};
    std::atomic<bool> _ardentDefenderActive{false};
    std::atomic<uint32> _ardentDefenderEndTime{0};
    std::atomic<bool> _guardianOfAncientKingsActive{false};
    std::atomic<uint32> _guardianOfAncientKingsEndTime{0};

    // Performance metrics
    struct ProtectionMetrics {
        std::atomic<uint32> totalDamageTaken{0};
        std::atomic<uint32> totalThreatGenerated{0};
        std::atomic<uint32> damageBlocked{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> holyPowerGenerated{0};
        std::atomic<uint32> holyPowerSpent{0};
        std::atomic<uint32> shieldOfTheRighteousCasts{0};
        std::atomic<uint32> avengersShieldCasts{0};
        std::atomic<uint32> consecrationCasts{0};
        std::atomic<uint32> ardentDefenderUses{0};
        std::atomic<uint32> guardianOfAncientKingsUses{0};
        std::atomic<uint32> tauntUses{0};
        std::atomic<float> threatEfficiency{0.0f};
        std::atomic<float> blockEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> holyPowerEfficiency{0.0f};
        std::atomic<float> consecrationUptime{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalDamageTaken = 0; totalThreatGenerated = 0; damageBlocked = 0;
            manaSpent = 0; holyPowerGenerated = 0; holyPowerSpent = 0;
            shieldOfTheRighteousCasts = 0; avengersShieldCasts = 0; consecrationCasts = 0;
            ardentDefenderUses = 0; guardianOfAncientKingsUses = 0; tauntUses = 0;
            threatEfficiency = 0.0f; blockEfficiency = 0.0f; manaEfficiency = 0.0f;
            holyPowerEfficiency = 0.0f; consecrationUptime = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _protectionMetrics;

    // Advanced threat tracking system
    struct ThreatManager {
        std::unordered_map<uint64, ThreatTarget> targets;
        mutable std::mutex threatMutex;
        std::atomic<uint64> primaryTarget{0};
        std::atomic<uint32> activeThreatTargets{0};
        void UpdateThreat(uint64 targetGuid, float threat) {
            std::lock_guard<std::mutex> lock(threatMutex);
            auto& target = targets[targetGuid];
            target.guid = targetGuid;
            target.threatLevel = threat;
            target.lastUpdate = std::chrono::steady_clock::now();

            // Determine priority
            if (threat < 50.0f)
                target.priority = ThreatPriority::CRITICAL;
            else if (threat < 100.0f)
                target.priority = ThreatPriority::HIGH;
            else if (threat < 200.0f)
                target.priority = ThreatPriority::MODERATE;
            else if (threat < 500.0f)
                target.priority = ThreatPriority::LOW;
            else
                target.priority = ThreatPriority::EXCESS;
        }
        ThreatTarget* GetHighestPriorityTarget() {
            std::lock_guard<std::mutex> lock(threatMutex);
            ThreatTarget* highestPriority = nullptr;
            for (auto& [guid, target] : targets) {
                if (!highestPriority || target.priority < highestPriority->priority)
                    highestPriority = &target;
            }
            return highestPriority;
        }
        uint32 GetTargetCount(ThreatPriority priority) const {
            std::lock_guard<std::mutex> lock(threatMutex);
            uint32 count = 0;
            for (const auto& [guid, target] : targets) {
                if (target.priority == priority)
                    count++;
            }
            return count;
        }
    } _threatManager;

    // Consecration tracking
    struct ConsecrationTracker {
        std::atomic<bool> isActive{false};
        std::atomic<uint32> expiry{0};
        std::atomic<float> centerX{0.0f};
        std::atomic<float> centerY{0.0f};
        std::atomic<uint32> lastCast{0};
        void Cast(float x, float y, uint32 duration) {
            isActive = true;
            centerX = x;
            centerY = y;
            expiry = getMSTime() + duration;
            lastCast = getMSTime();
        }
        bool IsActive() const {
            return isActive.load() && expiry.load() > getMSTime();
        }
        bool IsInConsecration(float x, float y, float radius = 8.0f) const {
            if (!IsActive()) return false;
            float dx = x - centerX.load();
            float dy = y - centerY.load();
            return sqrt(dx*dx + dy*dy) <= radius;
        }
        uint32 GetTimeRemaining() const {
            uint32 currentTime = getMSTime();
            uint32 expiryTime = expiry.load();
            return expiryTime > currentTime ? expiryTime - currentTime : 0;
        }
    } _consecrationTracker;

    // Block and avoidance tracking
    struct DefenseTracker {
        std::atomic<uint32> totalAttacks{0};
        std::atomic<uint32> blockedAttacks{0};
        std::atomic<uint32> dodgedAttacks{0};
        std::atomic<uint32> parriedAttacks{0};
        std::atomic<uint32> missedAttacks{0};
        std::atomic<uint32> criticalHits{0};
        void RecordAttack(bool blocked, bool dodged, bool parried, bool missed, bool critical) {
            totalAttacks++;
            if (blocked) blockedAttacks++;
            if (dodged) dodgedAttacks++;
            if (parried) parriedAttacks++;
            if (missed) missedAttacks++;
            if (critical) criticalHits++;
        }
        float GetAvoidancePercent() const {
            uint32 total = totalAttacks.load();
            if (total == 0) return 0.0f;
            uint32 avoided = dodgedAttacks.load() + parriedAttacks.load() + missedAttacks.load();
            return (float)avoided / total * 100.0f;
        }
        float GetBlockPercent() const {
            uint32 total = totalAttacks.load();
            if (total == 0) return 0.0f;
            return (float)blockedAttacks.load() / total * 100.0f;
        }
        float GetCriticalHitPercent() const {
            uint32 total = totalAttacks.load();
            if (total == 0) return 0.0f;
            return (float)criticalHits.load() / total * 100.0f;
        }
    } _defenseTracker;

    // Protection buff tracking
    uint32 _lastArdentDefender;
    uint32 _lastGuardianOfAncientKings;
    uint32 _lastDivineProtection;
    uint32 _lastLayOnHands;
    uint32 _lastDivineShield;
    uint32 _lastConsecration;
    std::atomic<bool> _divineProtectionActive{false};
    std::atomic<bool> _divineShieldActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Protection mechanics
    void OptimizeProtectionRotation(::Unit* target);
    void HandleProtectionCooldowns();
    void ManageHealthThresholds();
    void OptimizeBlockingStrategy();
    void HandleEmergencyDefensives();
    void PredictDamageIntake();
    void ManageHolyPowerForTanking();
    float CalculateProtectionEfficiency();

    // Enhanced constants
    static constexpr float TANK_RANGE = 5.0f;
    static constexpr uint32 HOLY_POWER_MAX = 3;
    static constexpr uint32 CONSECRATION_DURATION = 30000; // 30 seconds
    static constexpr uint32 ARDENT_DEFENDER_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 DIVINE_PROTECTION_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 LAY_ON_HANDS_COOLDOWN = 600000; // 10 minutes
    static constexpr uint32 DIVINE_SHIELD_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 HAND_OF_RECKONING_COOLDOWN = 8000; // 8 seconds
    static constexpr uint32 HAMMER_OF_THE_RIGHTEOUS_MANA_COST = 200;
    static constexpr uint32 SHIELD_OF_THE_RIGHTEOUS_MANA_COST = 0; // Uses Holy Power
    static constexpr uint32 AVENGERS_SHIELD_MANA_COST = 300;
    static constexpr uint32 CONSECRATION_MANA_COST = 450;
    static constexpr uint32 HOLY_WRATH_MANA_COST = 350;
    static constexpr float THREAT_CRITICAL_THRESHOLD = 50.0f;
    static constexpr float THREAT_WARNING_THRESHOLD = 100.0f;
    static constexpr float HEALTH_EMERGENCY_THRESHOLD = 30.0f; // Use emergency cooldowns below 30%
    static constexpr float HEALTH_DEFENSIVE_THRESHOLD = 50.0f; // Use defensives below 50%
    static constexpr uint32 MULTI_TARGET_THRESHOLD = 3; // 3+ targets for AoE abilities
    static constexpr float PROTECTION_MANA_THRESHOLD = 25.0f; // Conservative mana usage below 25%
    static constexpr uint32 HOLY_POWER_EMERGENCY_THRESHOLD = 2; // Save HP for emergencies
    static constexpr float OPTIMAL_TANK_DISTANCE = 8.0f; // Optimal distance from group
    static constexpr uint32 POSITIONING_UPDATE_INTERVAL = 500; // Update positioning every 500ms
};

} // namespace Playerbot