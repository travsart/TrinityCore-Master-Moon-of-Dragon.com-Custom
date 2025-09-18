/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PriestSpecialization.h"
#include <map>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class TC_GAME_API HolySpecialization : public PriestSpecialization
{
public:
    explicit HolySpecialization(Player* bot);

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

    // Healing interface
    void UpdateHealing() override;
    bool ShouldHeal() override;
    ::Unit* GetBestHealTarget() override;
    void HealTarget(::Unit* target) override;

    // Role management
    PriestRole GetCurrentRole() override;
    void SetRole(PriestRole role) override;

    // Specialization info
    PriestSpec GetSpecialization() const override { return PriestSpec::HOLY; }
    const char* GetSpecializationName() const override { return "Holy"; }

private:
    // Holy-specific mechanics
    void UpdateHolyMechanics();
    void UpdateReactiveHealing();
    void UpdateHealOverTime();
    void UpdateSerendipity();
    void UpdateSpiritOfRedemption();
    void UpdateEmpoweredHealing();
    void UpdateChakra();
    bool ShouldCastCircleOfHealing();
    bool ShouldCastPrayerOfHealing();
    bool ShouldCastBindingHeal();
    bool ShouldCastPrayerOfMending(::Unit* target);
    bool ShouldCastRenew(::Unit* target);
    bool ShouldCastGuardianSpirit(::Unit* target);
    bool ShouldCastDivineHymn();

    // Reactive healing optimization
    void OptimizeReactiveHealing();
    void PrioritizeHealingTargets();
    void ManageHealingCooldowns();
    void HandleEmergencyHealing();
    void TriggerBurstHealing();
    void CalculateHealingPriorities();
    float GetHealingUrgency(::Unit* target);
    bool IsEmergencyHealingNeeded();

    // Heal over Time management
    void ManageHealOverTime();
    void CastRenew(::Unit* target);
    void OptimizeRenewTargets();
    void ManageRenewStacks();
    bool TargetHasRenew(::Unit* target);
    uint32 GetRenewTimeRemaining(::Unit* target);
    void RefreshRenewIfNeeded(::Unit* target);

    // Prayer of Mending system
    void ManagePrayerOfMending();
    void CastPrayerOfMending(::Unit* target);
    void OptimizeMendingBounces();
    void TrackMendingTargets();
    bool ShouldRefreshPrayerOfMending();
    ::Unit* GetBestMendingTarget();

    // Serendipity mechanics
    void ManageSerendipity();
    void BuildSerendipityStacks();
    void ConsumeSerendipityStacks();
    uint32 GetSerendipityStacks();
    bool ShouldUseSerendipity();
    void OptimizeSerendipityUsage();

    // Chakra system
    void ManageChakra();
    void EnterChakraSerenity();
    void EnterChakraSanctuary();
    void EnterChakraChastise();
    bool ShouldSwitchChakra();
    void OptimizeChakraState();
    uint32 GetCurrentChakra();

    // Group healing mechanics
    void HandleGroupHealing();
    void CastCircleOfHealing();
    void CastPrayerOfHealing();
    void CastDivineHymn();
    void OptimizeGroupHealTargeting();
    std::vector<::Unit*> GetInjuredGroupMembers(float threshold = 80.0f);
    bool ShouldUseGroupHeal();

    // Emergency healing abilities
    void HandleHolyEmergencies();
    void CastGuardianSpirit(::Unit* target);
    void UseHolyWordSerenity(::Unit* target);
    void UseHolyWordSanctuary();
    void TriggerDesperatePrayer();
    void UseEmergencyHealingCooldowns();

    // Empowered healing
    void ManageEmpoweredHealing();
    void TriggerEmpoweredHeal();
    bool HasEmpoweredHealing();
    void OptimizeEmpoweredSpells();
    float GetEmpoweredHealingBonus();

    // Spirit of Redemption
    void HandleSpiritOfRedemption();
    bool IsSpiritOfRedemptionActive();
    void MaximizeSpiritHealing();
    void UseSpiritCooldowns();

    // Holy offensive support
    void UseHolyOffensiveSpells(::Unit* target);
    void CastHolyFire(::Unit* target);
    void CastSmite(::Unit* target);
    void CastHolyNova();
    bool ShouldUseOffensiveSpells();

    // Mana management
    void OptimizeHolyMana();
    void UseHymnOfHope();
    void ManageHealingEfficiency();
    void ConserveManaDuringDowntime();
    float CalculateHealPerMana(uint32 spellId);
    bool ShouldPrioritizeManaEfficiency();

    // Holy spell IDs
    enum HolySpells
    {
        HEAL = 2054,
        GREATER_HEAL = 2060,
        FLASH_HEAL = 2061,
        BINDING_HEAL = 32546,
        RENEW = 139,
        PRAYER_OF_HEALING = 596,
        PRAYER_OF_MENDING = 33076,
        CIRCLE_OF_HEALING = 34861,
        GUARDIAN_SPIRIT = 47788,
        DIVINE_HYMN = 64843,
        HYMN_OF_HOPE = 64901,
        DESPERATE_PRAYER = 19236,
        SPIRIT_OF_REDEMPTION = 20711,
        SERENDIPITY = 63731,
        EMPOWERED_HEALING = 33158,
        CHAKRA = 14751,
        CHAKRA_SERENITY = 81208,
        CHAKRA_SANCTUARY = 81206,
        CHAKRA_CHASTISE = 81209,
        HOLY_WORD_SERENITY = 88684,
        HOLY_WORD_SANCTUARY = 88685,
        HOLY_WORD_CHASTISE = 88625,
        BODY_AND_SOUL = 64129,
        SURGE_OF_LIGHT = 33150,
        APOTHEOSIS = 10060,
        HOLY_FIRE = 14914,
        SMITE = 585,
        HOLY_NOVA = 15237
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _lastManaRegen{0};
    std::atomic<float> _manaRegenRate{0.0f};
    std::atomic<bool> _spiritOfRedemptionActive{false};
    std::atomic<uint32> _spiritOfRedemptionEndTime{0};
    std::atomic<uint32> _serendipityStacks{0};
    std::atomic<uint32> _currentChakra{0};

    // Performance metrics
    struct HolyMetrics {
        std::atomic<uint32> totalHealingDone{0};
        std::atomic<uint32> totalOverhealing{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> renewsCast{0};
        std::atomic<uint32> prayerOfMendingBounces{0};
        std::atomic<uint32> circleOfHealingCasts{0};
        std::atomic<uint32> guardianSpiritUses{0};
        std::atomic<uint32> divineHymnUses{0};
        std::atomic<uint32> serendipityStacksUsed{0};
        std::atomic<uint32> chakraSwitches{0};
        std::atomic<float> healingEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> groupHealingRatio{0.0f};
        std::atomic<float> reactiveHealingRatio{0.0f};
        std::atomic<float> overhealingPercent{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalHealingDone = 0; totalOverhealing = 0; manaSpent = 0;
            renewsCast = 0; prayerOfMendingBounces = 0; circleOfHealingCasts = 0;
            guardianSpiritUses = 0; divineHymnUses = 0; serendipityStacksUsed = 0;
            chakraSwitches = 0; healingEfficiency = 0.0f; manaEfficiency = 0.0f;
            groupHealingRatio = 0.0f; reactiveHealingRatio = 0.0f; overhealingPercent = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _holyMetrics;

    // Advanced HoT tracking system
    struct HoTTracker {
        std::unordered_map<uint64, uint32> renewExpiry;
        std::unordered_map<uint64, uint32> prayerOfMendingCharges;
        std::unordered_map<uint64, uint32> prayerOfMendingExpiry;
        mutable std::mutex hotMutex;
        void UpdateRenew(uint64 targetGuid, uint32 duration) {
            std::lock_guard<std::mutex> lock(hotMutex);
            renewExpiry[targetGuid] = getMSTime() + duration;
        }
        void UpdatePrayerOfMending(uint64 targetGuid, uint32 charges, uint32 duration) {
            std::lock_guard<std::mutex> lock(hotMutex);
            prayerOfMendingCharges[targetGuid] = charges;
            prayerOfMendingExpiry[targetGuid] = getMSTime() + duration;
        }
        bool HasRenew(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            auto it = renewExpiry.find(targetGuid);
            return it != renewExpiry.end() && it->second > getMSTime();
        }
        bool HasPrayerOfMending(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            auto it = prayerOfMendingExpiry.find(targetGuid);
            return it != prayerOfMendingExpiry.end() && it->second > getMSTime();
        }
        uint32 GetRenewTimeRemaining(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            auto it = renewExpiry.find(targetGuid);
            if (it != renewExpiry.end()) {
                uint32 currentTime = getMSTime();
                return it->second > currentTime ? it->second - currentTime : 0;
            }
            return 0;
        }
        uint32 GetPrayerOfMendingCharges(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(hotMutex);
            auto it = prayerOfMendingCharges.find(targetGuid);
            return it != prayerOfMendingCharges.end() ? it->second : 0;
        }
    } _hotTracker;

    // Healing priority system
    struct HealingPriorityManager {
        std::priority_queue<HealTarget> healingQueue;
        mutable std::mutex queueMutex;
        std::atomic<uint32> lastUpdate{0};
        void UpdatePriorities(const std::vector<::Unit*>& groupMembers) {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::priority_queue<HealTarget> newQueue;
            uint32 currentTime = getMSTime();

            for (auto* member : groupMembers) {
                if (!member || member->GetHealth() == member->GetMaxHealth())
                    continue;

                float healthPercent = (float)member->GetHealth() / member->GetMaxHealth() * 100.0f;
                uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();

                HealPriority priority = HealPriority::FULL;
                if (healthPercent < 25.0f)
                    priority = HealPriority::EMERGENCY;
                else if (healthPercent < 50.0f)
                    priority = HealPriority::CRITICAL;
                else if (healthPercent < 75.0f)
                    priority = HealPriority::MODERATE;
                else if (healthPercent < 90.0f)
                    priority = HealPriority::MAINTENANCE;

                newQueue.emplace(member, priority, healthPercent, missingHealth);
            }

            healingQueue = std::move(newQueue);
            lastUpdate = currentTime;
        }
        HealTarget GetNextHealTarget() {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!healingQueue.empty()) {
                HealTarget target = healingQueue.top();
                healingQueue.pop();
                return target;
            }
            return HealTarget();
        }
        bool HasHealTargets() const {
            std::lock_guard<std::mutex> lock(queueMutex);
            return !healingQueue.empty();
        }
    } _healingPriorityManager;

    // Holy buff tracking
    uint32 _lastGuardianSpirit;
    uint32 _lastDivineHymn;
    uint32 _lastHymnOfHope;
    uint32 _lastDesperatePrayer;
    uint32 _lastApotheosis;
    std::atomic<bool> _empoweredHealingActive{false};
    std::atomic<bool> _surgeOfLightActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Holy mechanics
    void OptimizeHolyRotation();
    void HandleHolyCooldowns();
    void ManageTriageHealing();
    void OptimizeHealingThroughput();
    void ManageHolyWordCooldowns();
    void OptimizeSpiritOfRedemptionTime();
    float CalculateHolyEfficiency();

    // Enhanced constants
    static constexpr float HEALING_RANGE = 40.0f;
    static constexpr uint32 SERENDIPITY_MAX_STACKS = 2;
    static constexpr uint32 SERENDIPITY_DURATION = 20000; // 20 seconds
    static constexpr uint32 RENEW_DURATION = 15000; // 15 seconds
    static constexpr uint32 PRAYER_OF_MENDING_DURATION = 30000; // 30 seconds
    static constexpr uint32 PRAYER_OF_MENDING_MAX_CHARGES = 5;
    static constexpr uint32 GUARDIAN_SPIRIT_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 DIVINE_HYMN_COOLDOWN = 480000; // 8 minutes
    static constexpr uint32 HYMN_OF_HOPE_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 SPIRIT_OF_REDEMPTION_DURATION = 15000; // 15 seconds
    static constexpr uint32 GREATER_HEAL_MANA_COST = 370;
    static constexpr uint32 FLASH_HEAL_MANA_COST = 380;
    static constexpr uint32 HEAL_MANA_COST = 200;
    static constexpr uint32 RENEW_MANA_COST = 170;
    static constexpr uint32 PRAYER_OF_HEALING_MANA_COST = 560;
    static constexpr uint32 CIRCLE_OF_HEALING_MANA_COST = 500;
    static constexpr float GROUP_HEAL_THRESHOLD = 3.0f; // Use group heals when 3+ injured
    static constexpr float EMERGENCY_HEAL_THRESHOLD = 25.0f; // Emergency healing below 25%
    static constexpr float REACTIVE_HEALING_RATIO = 0.8f; // 80% reactive vs preventive
    static constexpr float HOLY_MANA_THRESHOLD = 15.0f; // Conservative mana usage below 15%
    static constexpr uint32 CHAKRA_COOLDOWN = 30000; // 30 seconds between chakra switches
};

} // namespace Playerbot