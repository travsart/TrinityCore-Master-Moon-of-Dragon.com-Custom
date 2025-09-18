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

// Healing priority levels for shamans
enum class ShamanHealPriority : uint8
{
    EMERGENCY = 0,      // <20% health, imminent death
    CRITICAL = 1,       // 20-40% health, needs immediate attention
    MODERATE = 2,       // 40-70% health, should heal soon
    MAINTENANCE = 3,    // 70-90% health, top off when convenient
    FULL = 4           // >90% health, no healing needed
};

// Shaman heal target info for priority queue
struct ShamanHealTarget
{
    ::Unit* target;
    ShamanHealPriority priority;
    float healthPercent;
    uint32 missingHealth;
    bool inCombat;
    bool hasEarthShield;
    bool hasRiptide;
    uint32 timestamp;
    float threatLevel;

    ShamanHealTarget() : target(nullptr), priority(ShamanHealPriority::FULL), healthPercent(100.0f),
                   missingHealth(0), inCombat(false), hasEarthShield(false), hasRiptide(false),
                   timestamp(0), threatLevel(0.0f) {}

    ShamanHealTarget(::Unit* t, ShamanHealPriority p, float hp, uint32 missing)
        : target(t), priority(p), healthPercent(hp), missingHealth(missing),
          inCombat(t ? t->IsInCombat() : false), hasEarthShield(false), hasRiptide(false),
          timestamp(getMSTime()), threatLevel(0.0f) {}

    bool operator<(const ShamanHealTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;

        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent;

        return timestamp > other.timestamp;
    }
};

class TC_GAME_API RestorationSpecialization : public ShamanSpecialization
{
public:
    explicit RestorationSpecialization(Player* bot);

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
    ShamanSpec GetSpecialization() const override { return ShamanSpec::RESTORATION; }
    const char* GetSpecializationName() const override { return "Restoration"; }

private:
    // Restoration-specific mechanics
    void UpdateRestorationMechanics();
    void UpdateHealingSystem();
    void UpdateEarthShield();
    void UpdateRiptide();
    void UpdateChainHeal();
    void UpdateHealingRain();
    void UpdateNatureSwiftness();
    void UpdateTidalForce();
    void UpdateAncestralAwakening();
    void UpdateSpiritLinkTotem();
    bool ShouldCastHealingWave(::Unit* target);
    bool ShouldCastGreaterHealingWave(::Unit* target);
    bool ShouldCastLesserHealingWave(::Unit* target);
    bool ShouldCastChainHeal();
    bool ShouldCastRiptide(::Unit* target);
    bool ShouldCastHealingRain();
    bool ShouldCastNatureSwiftness();
    bool ShouldCastAncestralSpirit(::Unit* target);

    // Healing optimization system
    void OptimizeHealingRotation();
    void PrioritizeHealingTargets();
    void ManageHealingCooldowns();
    void HandleEmergencyHealing();
    void CalculateHealingEfficiency();
    float GetHealingUrgency(::Unit* target);
    bool IsEmergencyHealingNeeded();
    void TriggerBurstHealing();

    // Earth Shield management
    void ManageEarthShield();
    void CastEarthShield(::Unit* target);
    ::Unit* GetBestEarthShieldTarget();
    bool ShouldMaintainEarthShield(::Unit* target);
    void OptimizeEarthShieldTargets();
    uint32 GetEarthShieldCharges(::Unit* target);
    bool ShouldRefreshEarthShield(::Unit* target);

    // Riptide HoT management
    void ManageRiptide();
    void CastRiptide(::Unit* target);
    void OptimizeRiptideTargets();
    bool ShouldCastRiptideForHealing(::Unit* target);
    bool ShouldCastRiptideForTidalWaves();
    uint32 GetRiptideTimeRemaining(::Unit* target);

    // Chain Heal mechanics
    void ManageChainHeal();
    void CastChainHeal();
    ::Unit* GetOptimalChainHealTarget();
    void OptimizeChainHealTargeting();
    bool ShouldUseChainHeal();
    std::vector<::Unit*> PredictChainHealTargets(::Unit* primaryTarget);

    // Tidal Waves mechanics
    void ManageTidalWaves();
    bool HasTidalWavesProc();
    void ConsumeTidalWavesProc();
    void OptimizeTidalWavesUsage();
    bool ShouldUseTidalWaves(uint32 spellId);

    // Nature's Swiftness system
    void ManageNatureSwiftness();
    void CastNatureSwiftness();
    bool HasNatureSwiftness();
    void OptimizeNatureSwiftnessUsage();
    uint32 GetBestSpellForNatureSwiftness(::Unit* target);

    // Healing Rain mechanics
    void ManageHealingRain();
    void CastHealingRain();
    Position GetOptimalHealingRainPosition();
    bool ShouldUseHealingRain();
    void OptimizeHealingRainPlacement();

    // Ancestral Awakening system
    void ManageAncestralAwakening();
    void TriggerAncestralAwakening();
    bool HasAncestralAwakeningProc();
    void OptimizeAncestralAwakeningHeals();

    // Cleansing and utility
    void HandleRestorationUtility();
    void CastCleanseSpirit(::Unit* target);
    void CastCurePoison(::Unit* target);
    void CastCureDisease(::Unit* target);
    void OptimizeCleansingPriority();
    bool ShouldCleanse(::Unit* target);

    // Spirit Link Totem mechanics
    void ManageSpiritLinkTotem();
    void CastSpiritLinkTotem();
    bool ShouldUseSpiritLinkTotem();
    void OptimizeSpiritLinkTiming();
    Position GetOptimalSpiritLinkPosition();

    // Mana management for healer
    void OptimizeRestorationMana();
    void UseWaterShieldForMana();
    void ManageHealingEfficiency();
    bool ShouldConserveManaForEmergencies();
    float CalculateHealPerMana(uint32 spellId);
    void UseManaTideTotem();

    // Group healing optimization
    void HandleGroupHealing();
    void OptimizeGroupHealTargeting();
    void ManageHealingAssignments();
    void PrioritizeTankHealing();
    void AssistOtherHealers();
    std::vector<::Unit*> GetInjuredGroupMembers(float threshold = 80.0f);

    // Emergency healing abilities
    void HandleRestorationEmergencies();
    void UseAncestralGuidance();
    void CastNatureGuardian();
    void UseEmergencyHealingCooldowns();
    void TriggerHealingCooldowns();

    // Positioning for ranged healer
    void OptimizeRestorationPositioning();
    void MaintainHealingPosition();
    void HandleHealerMovement();
    bool ShouldMoveForOptimalRange(::Unit* target);
    void AvoidDamageWhileHealing();

    // Restoration spell IDs
    enum RestorationSpells
    {
        HEALING_WAVE = 331,
        GREATER_HEALING_WAVE = 77472,
        LESSER_HEALING_WAVE = 8004,
        CHAIN_HEAL = 1064,
        RIPTIDE = 61295,
        HEALING_RAIN = 73920,
        EARTH_SHIELD = 974,
        NATURE_SWIFTNESS = 16188,
        TIDAL_FORCE = 55198,
        ANCESTRAL_AWAKENING = 51558,
        SPIRIT_LINK_TOTEM = 98008,
        CLEANSE_SPIRIT = 51886,
        CURE_POISON = 526,
        CURE_DISEASE = 2870,
        ANCESTRAL_SPIRIT = 2008,
        WATER_SHIELD = 52127,
        MANA_TIDE_TOTEM = 16190,
        HEALING_STREAM_TOTEM = 5394,
        NATURE_GUARDIAN = 30894,
        ANCESTRAL_GUIDANCE = 16240,
        PURIFICATION = 16213,
        TIDAL_MASTERY = 16182,
        HEALING_FOCUS = 16240,
        TOTEMIC_FOCUS = 16173,
        IMPROVED_HEALING_WAVE = 16187,
        TIDAL_WAVES = 51562,
        FOCUSED_INSIGHT = 77794,
        TELLURIC_CURRENTS = 82987,
        BLESSING_OF_THE_ETERNALS = 51554,
        SPARK_OF_LIFE = 84846,
        ANCESTRAL_RESOLVE = 86908,
        DEEP_HEALING = 77226,
        NATURE_BLESSING = 30867,
        HEALING_GRACE = 16160
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<bool> _natureSwiftnessActive{false};
    std::atomic<bool> _tidalWavesActive{false};
    std::atomic<uint32> _tidalWavesStacks{0};
    std::atomic<bool> _ancestralAwakeningActive{false};
    std::atomic<bool> _waterShieldActive{false};
    std::atomic<uint32> _waterShieldCharges{0};

    // Performance metrics
    struct RestorationMetrics {
        std::atomic<uint32> totalHealingDone{0};
        std::atomic<uint32> totalOverhealing{0};
        std::atomic<uint32> earthShieldHealing{0};
        std::atomic<uint32> riptideHealing{0};
        std::atomic<uint32> chainHealBounces{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> manaRegained{0};
        std::atomic<uint32> tidalWavesProcs{0};
        std::atomic<uint32> ancestralAwakeningHeals{0};
        std::atomic<uint32> natureSwiftnessUses{0};
        std::atomic<uint32> spiritLinkTotemUses{0};
        std::atomic<uint32> cleansesCast{0};
        std::atomic<float> healingEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> earthShieldUptime{0.0f};
        std::atomic<float> overhealingPercent{0.0f};
        std::atomic<float> chainHealEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalHealingDone = 0; totalOverhealing = 0; earthShieldHealing = 0;
            riptideHealing = 0; chainHealBounces = 0; manaSpent = 0;
            manaRegained = 0; tidalWavesProcs = 0; ancestralAwakeningHeals = 0;
            natureSwiftnessUses = 0; spiritLinkTotemUses = 0; cleansesCast = 0;
            healingEfficiency = 0.0f; manaEfficiency = 0.0f; earthShieldUptime = 0.0f;
            overhealingPercent = 0.0f; chainHealEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _restorationMetrics;

    // Advanced Earth Shield tracking
    struct EarthShieldTracker {
        std::unordered_map<uint64, uint32> earthShieldExpiry;
        std::unordered_map<uint64, uint32> earthShieldCharges;
        mutable std::mutex earthShieldMutex;
        void SetEarthShield(uint64 targetGuid, uint32 duration, uint32 charges) {
            std::lock_guard<std::mutex> lock(earthShieldMutex);
            earthShieldExpiry[targetGuid] = getMSTime() + duration;
            earthShieldCharges[targetGuid] = charges;
        }
        bool HasEarthShield(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(earthShieldMutex);
            auto it = earthShieldExpiry.find(targetGuid);
            if (it != earthShieldExpiry.end() && it->second > getMSTime()) {
                auto chargeIt = earthShieldCharges.find(targetGuid);
                return chargeIt != earthShieldCharges.end() && chargeIt->second > 0;
            }
            return false;
        }
        uint32 GetCharges(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(earthShieldMutex);
            auto it = earthShieldCharges.find(targetGuid);
            if (it != earthShieldCharges.end()) {
                auto expiryIt = earthShieldExpiry.find(targetGuid);
                if (expiryIt != earthShieldExpiry.end() && expiryIt->second > getMSTime())
                    return it->second;
            }
            return 0;
        }
        void ConsumeCharge(uint64 targetGuid) {
            std::lock_guard<std::mutex> lock(earthShieldMutex);
            auto it = earthShieldCharges.find(targetGuid);
            if (it != earthShieldCharges.end() && it->second > 0)
                it->second--;
        }
        uint32 GetTimeRemaining(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(earthShieldMutex);
            auto it = earthShieldExpiry.find(targetGuid);
            if (it != earthShieldExpiry.end()) {
                uint32 currentTime = getMSTime();
                return it->second > currentTime ? it->second - currentTime : 0;
            }
            return 0;
        }
        bool ShouldRefresh(uint64 targetGuid, uint32 chargeThreshold = 2, uint32 timeThreshold = 30000) const {
            return GetCharges(targetGuid) <= chargeThreshold || GetTimeRemaining(targetGuid) <= timeThreshold;
        }
    } _earthShieldTracker;

    // Riptide HoT tracker
    struct RiptideTracker {
        std::unordered_map<uint64, uint32> riptideExpiry;
        mutable std::mutex riptideMutex;
        void UpdateRiptide(uint64 targetGuid, uint32 duration) {
            std::lock_guard<std::mutex> lock(riptideMutex);
            riptideExpiry[targetGuid] = getMSTime() + duration;
        }
        bool HasRiptide(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(riptideMutex);
            auto it = riptideExpiry.find(targetGuid);
            return it != riptideExpiry.end() && it->second > getMSTime();
        }
        uint32 GetTimeRemaining(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(riptideMutex);
            auto it = riptideExpiry.find(targetGuid);
            if (it != riptideExpiry.end()) {
                uint32 currentTime = getMSTime();
                return it->second > currentTime ? it->second - currentTime : 0;
            }
            return 0;
        }
        bool ShouldRefresh(uint64 targetGuid, uint32 refreshThreshold = 3000) const {
            return GetTimeRemaining(targetGuid) <= refreshThreshold;
        }
    } _riptideTracker;

    // Healing priority system
    struct HealingPriorityManager {
        std::priority_queue<ShamanHealTarget> healingQueue;
        mutable std::mutex queueMutex;
        std::atomic<uint32> lastUpdate{0};
        void UpdatePriorities(const std::vector<::Unit*>& groupMembers) {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::priority_queue<ShamanHealTarget> newQueue;
            uint32 currentTime = getMSTime();

            for (auto* member : groupMembers) {
                if (!member || member->GetHealth() == member->GetMaxHealth())
                    continue;

                float healthPercent = (float)member->GetHealth() / member->GetMaxHealth() * 100.0f;
                uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();

                ShamanHealPriority priority = ShamanHealPriority::FULL;
                if (healthPercent < 20.0f)
                    priority = ShamanHealPriority::EMERGENCY;
                else if (healthPercent < 40.0f)
                    priority = ShamanHealPriority::CRITICAL;
                else if (healthPercent < 70.0f)
                    priority = ShamanHealPriority::MODERATE;
                else if (healthPercent < 90.0f)
                    priority = ShamanHealPriority::MAINTENANCE;

                // Increase priority for tanks
                if (member->HasRole(ROLE_TANK))
                    priority = static_cast<ShamanHealPriority>(std::max(0, static_cast<int>(priority) - 1));

                newQueue.emplace(member, priority, healthPercent, missingHealth);
            }

            healingQueue = std::move(newQueue);
            lastUpdate = currentTime;
        }
        ShamanHealTarget GetNextHealTarget() {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!healingQueue.empty()) {
                ShamanHealTarget target = healingQueue.top();
                healingQueue.pop();
                return target;
            }
            return ShamanHealTarget();
        }
        bool HasHealTargets() const {
            std::lock_guard<std::mutex> lock(queueMutex);
            return !healingQueue.empty();
        }
    } _healingPriorityManager;

    // Totem effectiveness for restoration
    struct RestorationTotemTracker {
        std::atomic<bool> healingStreamActive{false};
        std::atomic<bool> manaTideActive{false};
        std::atomic<bool> spiritLinkActive{false};
        std::atomic<uint32> healingStreamHealing{0};
        std::atomic<uint32> manaTideMana{0};
        std::atomic<uint32> spiritLinkDamageShared{0};
        void RecordHealingStreamHealing(uint32 amount) { healingStreamHealing += amount; }
        void RecordManaTideMana(uint32 amount) { manaTideMana += amount; }
        void RecordSpiritLinkSharing(uint32 amount) { spiritLinkDamageShared += amount; }
        float GetHealingStreamEffectiveness() const { return (float)healingStreamHealing.load(); }
        float GetManaTideEffectiveness() const { return (float)manaTideMana.load(); }
        float GetSpiritLinkEffectiveness() const { return (float)spiritLinkDamageShared.load(); }
    } _restorationTotemTracker;

    // Restoration buff tracking
    uint32 _lastNatureSwiftness;
    uint32 _lastAncestralGuidance;
    uint32 _lastManaTideTotem;
    uint32 _lastSpiritLinkTotem;
    uint32 _lastWaterShield;
    std::atomic<bool> _ancestralGuidanceActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Restoration mechanics
    void OptimizeRestorationRotation();
    void HandleRestorationCooldowns();
    void ManageHealingPriorities();
    void OptimizeEarthShieldPlacement();
    void HandleTidalWavesTiming();
    void ManageChainHealBouncing();
    float CalculateRestorationEfficiency();

    // Enhanced constants
    static constexpr float HEALING_RANGE = 40.0f;
    static constexpr uint32 EARTH_SHIELD_MAX_CHARGES = 9;
    static constexpr uint32 EARTH_SHIELD_DURATION = 600000; // 10 minutes
    static constexpr uint32 RIPTIDE_DURATION = 15000; // 15 seconds
    static constexpr uint32 TIDAL_WAVES_DURATION = 15000; // 15 seconds
    static constexpr uint32 NATURE_SWIFTNESS_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 MANA_TIDE_TOTEM_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 SPIRIT_LINK_TOTEM_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 WATER_SHIELD_MAX_CHARGES = 3;
    static constexpr uint32 WATER_SHIELD_DURATION = 600000; // 10 minutes
    static constexpr uint32 HEALING_WAVE_MANA_COST = 400;
    static constexpr uint32 GREATER_HEALING_WAVE_MANA_COST = 650;
    static constexpr uint32 LESSER_HEALING_WAVE_MANA_COST = 280;
    static constexpr uint32 CHAIN_HEAL_MANA_COST = 620;
    static constexpr uint32 RIPTIDE_MANA_COST = 360;
    static constexpr uint32 HEALING_RAIN_MANA_COST = 800;
    static constexpr float GROUP_HEAL_THRESHOLD = 3.0f; // Use Chain Heal when 3+ injured
    static constexpr float EMERGENCY_HEAL_THRESHOLD = 20.0f; // Emergency healing below 20%
    static constexpr float RESTORATION_MANA_THRESHOLD = 20.0f; // Conservative mana usage below 20%
    static constexpr uint32 EARTH_SHIELD_REFRESH_CHARGES = 3; // Refresh when 3 or fewer charges
    static constexpr uint32 TIDAL_WAVES_MAX_STACKS = 2; // Maximum Tidal Waves stacks
    static constexpr float CHAIN_HEAL_RANGE = 12.5f; // Range for Chain Heal bouncing
};

} // namespace Playerbot