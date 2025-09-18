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

class TC_GAME_API HolyPaladinSpecialization : public PaladinSpecialization
{
public:
    explicit HolyPaladinSpecialization(Player* bot);

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
    PaladinSpec GetSpecialization() const override { return PaladinSpec::HOLY; }
    const char* GetSpecializationName() const override { return "Holy"; }

private:
    // Holy-specific mechanics
    void UpdateHolyMechanics();
    void UpdateHealingSystem();
    void UpdateHolyPower();
    void UpdateInfusionOfLight();
    void UpdateBeaconOfLight();
    void UpdateDivineIllumination();
    void UpdateHolyShock();
    bool ShouldCastHolyLight(::Unit* target);
    bool ShouldCastFlashOfLight(::Unit* target);
    bool ShouldCastHolyShock(::Unit* target);
    bool ShouldCastWordOfGlory(::Unit* target);
    bool ShouldCastLayOnHands(::Unit* target);
    bool ShouldCastDivineLight(::Unit* target);
    bool ShouldCastGuardianOfAncientKings();
    bool ShouldCastAuraOfMastery();

    // Healing optimization system
    void OptimizeHealingRotation();
    void PrioritizeHealingTargets();
    void ManageHealingCooldowns();
    void HandleEmergencyHealing();
    void CalculateHealingEfficiency();
    float GetHealingUrgency(::Unit* target);
    bool IsEmergencyHealingNeeded();
    void TriggerBurstHealing();

    // Holy Power system
    void ManageHolyPower();
    void BuildHolyPower();
    void SpendHolyPower(::Unit* target);
    uint32 GetHolyPowerCharges();
    bool ShouldSpendHolyPower();
    void OptimizeHolyPowerUsage();
    uint32 GetOptimalHolyPowerSpender(::Unit* target);

    // Beacon of Light management
    void ManageBeaconOfLight();
    void CastBeaconOfLight(::Unit* target);
    ::Unit* GetBestBeaconTarget();
    bool ShouldMaintainBeacon(::Unit* target);
    void OptimizeBeaconTargets();
    float CalculateBeaconValue(::Unit* target);

    // Infusion of Light mechanics
    void ManageInfusionOfLight();
    bool HasInfusionOfLight();
    void ConsumeInfusionOfLight();
    void OptimizeInfusionUsage();
    bool ShouldUseInfusionOfLight(uint32 spellId);

    // Divine Illumination system
    void ManageDivineIllumination();
    void TriggerDivineIllumination();
    bool HasDivineIllumination();
    void OptimizeDivineIlluminationUsage();
    float GetDivineIlluminationBonus();

    // Holy Shock mechanics
    void ManageHolyShock();
    void CastHolyShockHeal(::Unit* target);
    void CastHolyShockDamage(::Unit* target);
    void OptimizeHolyShockUsage();
    bool ShouldUseHolyShockForHealing();
    bool ShouldUseHolyShockForDamage();

    // Divine Favor system
    void ManageDivineFavor();
    void TriggerDivineFavor();
    bool HasDivineFavor();
    void OptimizeDivineFavorUsage();
    uint32 GetBestSpellForDivineFavor(::Unit* target);

    // Judgement for healing
    void ManageJudgementHealing();
    void CastJudgementOfLight(::Unit* target);
    void CastJudgementOfWisdom(::Unit* target);
    void OptimizeJudgementTargets();
    bool ShouldJudgeForHealing();

    // Emergency healing abilities
    void HandleHolyEmergencies();
    void CastLayOnHands(::Unit* target);
    void UseGuardianOfAncientKings();
    void CastDivineProtection();
    void CastDivineFavor();
    void UseEmergencyHealingCooldowns();

    // Mana management
    void OptimizeHolyMana();
    void UseDivineIllumination();
    void ConserveManaWhenLow();
    void ManageHealingEfficiency();
    float CalculateHealPerMana(uint32 spellId);
    bool ShouldPrioritizeManaEfficiency();

    // Group healing support
    void HandleGroupHealing();
    void OptimizeGroupHealTargeting();
    void ManageHealingAssignments();
    void PrioritizeTankHealing();
    void AssistOtherHealers();

    // Holy spell IDs
    enum HolySpells
    {
        HOLY_LIGHT = 635,
        FLASH_OF_LIGHT = 19750,
        DIVINE_LIGHT = 82326,
        HOLY_SHOCK = 20473,
        WORD_OF_GLORY = 85673,
        LAY_ON_HANDS = 633,
        BEACON_OF_LIGHT = 53563,
        DIVINE_FAVOR = 20216,
        DIVINE_ILLUMINATION = 31842,
        INFUSION_OF_LIGHT = 53576,
        HOLY_POWER = 85696,
        GUARDIAN_OF_ANCIENT_KINGS = 86669,
        AURA_MASTERY = 31821,
        LIGHT_OF_DAWN = 85222,
        PROTECTOR_OF_THE_INNOCENT = 85416,
        TOWER_OF_RADIANCE = 85512,
        SPEED_OF_LIGHT = 85499,
        DENOUNCE = 85509,
        WALK_IN_THE_LIGHT = 85487,
        BLESSED_LIFE = 85433,
        PURE_OF_HEART = 85458,
        CLARITY_OF_PURPOSE = 85461,
        LAST_WORD = 85466,
        ENLIGHTENED_JUDGEMENTS = 53695,
        JUDGEMENTS_OF_THE_PURE = 54151,
        SEALS_OF_THE_PURE = 20224,
        HEALING_LIGHT = 20237,
        DIVINE_INTELLECT = 1180,
        UNYIELDING_FAITH = 31229
    };

    // Enhanced mana system
    std::atomic<uint32> _mana{0};
    std::atomic<uint32> _maxMana{0};
    std::atomic<uint32> _holyPower{0};
    std::atomic<uint32> _maxHolyPower{3};
    std::atomic<bool> _divineIlluminationActive{false};
    std::atomic<uint32> _divineIlluminationEndTime{0};
    std::atomic<bool> _divineFavorActive{false};
    std::atomic<bool> _infusionOfLightActive{false};
    std::atomic<uint32> _infusionOfLightStacks{0};

    // Performance metrics
    struct HolyMetrics {
        std::atomic<uint32> totalHealingDone{0};
        std::atomic<uint32> totalOverhealing{0};
        std::atomic<uint32> beaconHealingDone{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> holyPowerGenerated{0};
        std::atomic<uint32> holyPowerSpent{0};
        std::atomic<uint32> holyShockCasts{0};
        std::atomic<uint32> wordOfGloryCasts{0};
        std::atomic<uint32> layOnHandsUses{0};
        std::atomic<uint32> divineFavorUses{0};
        std::atomic<uint32> divineIlluminationUses{0};
        std::atomic<uint32> infusionOfLightProcs{0};
        std::atomic<float> healingEfficiency{0.0f};
        std::atomic<float> manaEfficiency{0.0f};
        std::atomic<float> beaconUptime{0.0f};
        std::atomic<float> overhealingPercent{0.0f};
        std::atomic<float> holyPowerEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        void Reset() {
            totalHealingDone = 0; totalOverhealing = 0; beaconHealingDone = 0;
            manaSpent = 0; holyPowerGenerated = 0; holyPowerSpent = 0;
            holyShockCasts = 0; wordOfGloryCasts = 0; layOnHandsUses = 0;
            divineFavorUses = 0; divineIlluminationUses = 0; infusionOfLightProcs = 0;
            healingEfficiency = 0.0f; manaEfficiency = 0.0f; beaconUptime = 0.0f;
            overhealingPercent = 0.0f; holyPowerEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
        }
    } _holyMetrics;

    // Advanced Beacon tracking system
    struct BeaconTracker {
        std::atomic<uint64> beaconTarget{0};
        std::atomic<uint32> beaconExpiry{0};
        std::atomic<uint32> beaconHealingDone{0};
        mutable std::mutex beaconMutex;
        void SetBeacon(uint64 targetGuid, uint32 duration) {
            std::lock_guard<std::mutex> lock(beaconMutex);
            beaconTarget = targetGuid;
            beaconExpiry = getMSTime() + duration;
        }
        bool HasBeacon(uint64 targetGuid) const {
            std::lock_guard<std::mutex> lock(beaconMutex);
            return beaconTarget.load() == targetGuid && beaconExpiry.load() > getMSTime();
        }
        uint64 GetBeaconTarget() const {
            std::lock_guard<std::mutex> lock(beaconMutex);
            return beaconExpiry.load() > getMSTime() ? beaconTarget.load() : 0;
        }
        uint32 GetBeaconTimeRemaining() const {
            std::lock_guard<std::mutex> lock(beaconMutex);
            uint32 currentTime = getMSTime();
            uint32 expiry = beaconExpiry.load();
            return expiry > currentTime ? expiry - currentTime : 0;
        }
        bool ShouldRefreshBeacon(uint32 refreshThreshold = 30000) const {
            return GetBeaconTimeRemaining() <= refreshThreshold;
        }
        void RecordBeaconHealing(uint32 amount) {
            beaconHealingDone += amount;
        }
    } _beaconTracker;

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
                if (healthPercent < 20.0f)
                    priority = HealPriority::EMERGENCY;
                else if (healthPercent < 40.0f)
                    priority = HealPriority::CRITICAL;
                else if (healthPercent < 70.0f)
                    priority = HealPriority::MODERATE;
                else if (healthPercent < 85.0f)
                    priority = HealPriority::MAINTENANCE;

                // Increase priority for tanks
                if (member->HasRole(ROLE_TANK))
                    priority = static_cast<HealPriority>(std::max(0, static_cast<int>(priority) - 1));

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

    // Spell efficiency calculator
    struct SpellEfficiencyCalculator {
        struct SpellData {
            uint32 manaCost;
            uint32 baseHealing;
            float castTime;
            float efficiency;
            uint32 holyPowerCost;
        };
        std::unordered_map<uint32, SpellData> spellData;
        mutable std::mutex dataMutex;
        void UpdateSpellData(uint32 spellId, uint32 manaCost, uint32 healing, float castTime, uint32 hpCost = 0) {
            std::lock_guard<std::mutex> lock(dataMutex);
            auto& data = spellData[spellId];
            data.manaCost = manaCost;
            data.baseHealing = healing;
            data.castTime = castTime;
            data.holyPowerCost = hpCost;
            data.efficiency = castTime > 0 ? (float)healing / (manaCost * castTime) : 0.0f;
        }
        float GetSpellEfficiency(uint32 spellId) const {
            std::lock_guard<std::mutex> lock(dataMutex);
            auto it = spellData.find(spellId);
            return it != spellData.end() ? it->second.efficiency : 0.0f;
        }
        uint32 GetBestHealForSituation(uint32 missingHealth, uint32 availableMana, uint32 holyPower) const {
            std::lock_guard<std::mutex> lock(dataMutex);
            uint32 bestSpell = 0;
            float bestScore = 0.0f;

            for (const auto& [spellId, data] : spellData) {
                if (data.manaCost > availableMana || data.holyPowerCost > holyPower)
                    continue;

                float healingScore = std::min((float)data.baseHealing, (float)missingHealth);
                float score = healingScore * data.efficiency;

                if (score > bestScore) {
                    bestScore = score;
                    bestSpell = spellId;
                }
            }

            return bestSpell;
        }
    } _spellEfficiencyCalculator;

    // Holy buff tracking
    uint32 _lastDivineFavor;
    uint32 _lastDivineIllumination;
    uint32 _lastLayOnHands;
    uint32 _lastGuardianOfAncientKings;
    uint32 _lastAuraMastery;
    std::atomic<bool> _guardianOfAncientKingsActive{false};
    std::atomic<bool> _auraMasteryActive{false};

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;
    mutable std::mutex _cooldownMutex;

    // Advanced Holy mechanics
    void OptimizeHolyRotation();
    void HandleHolyCooldowns();
    void ManageHolyPowerPriorities();
    void OptimizeBeaconPlacement();
    void ManageInfusionTiming();
    void HandleDivineIlluminationWindows();
    float CalculateHolyEfficiency();

    // Enhanced constants
    static constexpr float HEALING_RANGE = 40.0f;
    static constexpr uint32 HOLY_POWER_MAX = 3;
    static constexpr uint32 BEACON_DURATION = 300000; // 5 minutes
    static constexpr uint32 DIVINE_ILLUMINATION_DURATION = 15000; // 15 seconds
    static constexpr uint32 DIVINE_FAVOR_DURATION = 20000; // 20 seconds
    static constexpr uint32 INFUSION_OF_LIGHT_DURATION = 15000; // 15 seconds
    static constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS_DURATION = 30000; // 30 seconds
    static constexpr uint32 LAY_ON_HANDS_COOLDOWN = 600000; // 10 minutes
    static constexpr uint32 DIVINE_FAVOR_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 DIVINE_ILLUMINATION_COOLDOWN = 180000; // 3 minutes
    static constexpr uint32 GUARDIAN_OF_ANCIENT_KINGS_COOLDOWN = 300000; // 5 minutes
    static constexpr uint32 AURA_MASTERY_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 HOLY_LIGHT_MANA_COST = 800;
    static constexpr uint32 FLASH_OF_LIGHT_MANA_COST = 380;
    static constexpr uint32 DIVINE_LIGHT_MANA_COST = 1200;
    static constexpr uint32 HOLY_SHOCK_MANA_COST = 400;
    static constexpr float EMERGENCY_HEAL_THRESHOLD = 25.0f; // Emergency healing below 25%
    static constexpr float BEACON_REFRESH_THRESHOLD = 30.0f; // Refresh beacon with 30s remaining
    static constexpr float HOLY_MANA_THRESHOLD = 20.0f; // Conservative mana usage below 20%
    static constexpr float INFUSION_OF_LIGHT_THRESHOLD = 60.0f; // Use IoL procs above 60% mana
    static constexpr uint32 HOLY_POWER_EMERGENCY_THRESHOLD = 2; // Save HP for emergencies
};

} // namespace Playerbot