/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "../ClassAI.h"
#include "Position.h"
#include "../Combat/FormationManager.h"
#include "../Combat/InterruptManager.h"
#include "../Combat/PositionManager.h"
#include "../Combat/TargetSelector.h"
#include "../Combat/BotThreatManager.h"
#include "MonkSpecialization.h"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <array>
#include <queue>

namespace Playerbot
{

// Monk specializations
enum class MonkSpec : uint8
{
    BREWMASTER = 0,
    MISTWEAVER = 1,
    WINDWALKER = 2
};

// Forward declarations

// Monk martial art forms
enum class MartialForm : uint8
{
    TIGER_FORM = 0,
    CRANE_FORM = 1,
    SERPENT_FORM = 2,
    OX_FORM = 3,
    NONE = 255
};

// Monk AI implementation with enterprise-grade architecture
class TC_GAME_API MonkAI : public ClassAI
{
public:
    explicit MonkAI(Player* bot);
    ~MonkAI() = default;

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization system
    MonkSpec _currentSpec;
    std::unique_ptr<MonkSpecialization> _specialization;

    // Advanced resource management systems
    struct ChiManagementSystem {
        std::atomic<uint32> current{0};
        std::atomic<uint32> maximum{5};
        std::atomic<uint32> generated{0};
        std::atomic<uint32> consumed{0};
        std::atomic<uint32> lastGeneration{0};
        std::atomic<float> efficiency{1.0f};
        std::atomic<uint32> wastedChi{0};
        std::queue<uint32> generationHistory;

        // Make non-copyable due to atomic members
        ChiManagementSystem() = default;
        ChiManagementSystem(const ChiManagementSystem&) = delete;
        ChiManagementSystem& operator=(const ChiManagementSystem&) = delete;

        void GenerateChi(uint32 amount) {
            uint32 oldChi = current.load();
            uint32 newChi = std::min(oldChi + amount, maximum.load());
            current.store(newChi);
            generated.store(generated.load() + amount);
            if (oldChi + amount > maximum.load()) {
                wastedChi.store(wastedChi.load() + (oldChi + amount - maximum.load()));
            }
            lastGeneration.store(getMSTime());
            generationHistory.push(amount);
            while (generationHistory.size() > 10)
                generationHistory.pop();
        }

        bool ConsumeChi(uint32 amount) {
            if (current.load() >= amount) {
                current.store(current.load() - amount);
                consumed.store(consumed.load() + amount);
                return true;
            }
            return false;
        }

        float CalculateEfficiency() const {
            uint32 gen = generated.load();
            if (gen == 0) return 1.0f;
            return 1.0f - (static_cast<float>(wastedChi.load()) / gen);
        }

        void Reset() {
            current.store(0); generated.store(0); consumed.store(0);
            wastedChi.store(0); efficiency.store(1.0f);
            while (!generationHistory.empty())
                generationHistory.pop();
        }
    } _chiManager;

    struct EnergyManagementSystem {
        std::atomic<uint32> current{100};
        std::atomic<uint32> maximum{100};
        std::atomic<float> regenRate{10.0f};
        std::atomic<uint32> lastRegen{0};
        std::atomic<uint32> totalSpent{0};
        std::atomic<uint32> totalRegen{0};
        std::atomic<float> efficiency{1.0f};
        std::queue<std::pair<uint32, uint32>> spendingHistory; // <amount, timestamp>

        // Make non-copyable due to atomic members
        EnergyManagementSystem() = default;
        EnergyManagementSystem(const EnergyManagementSystem&) = delete;
        EnergyManagementSystem& operator=(const EnergyManagementSystem&) = delete;

        void SpendEnergy(uint32 amount) {
            if (current.load() >= amount) {
                current.store(current.load() - amount);
                totalSpent.store(totalSpent.load() + amount);
                spendingHistory.push({amount, getMSTime()});
                while (spendingHistory.size() > 20)
                    spendingHistory.pop();
            }
        }

        void RegenerateEnergy(uint32 diff) {
            float regen = (regenRate * diff) / 1000.0f;
            uint32 oldEnergy = current.load();
            uint32 newEnergy = std::min(static_cast<uint32>(oldEnergy + regen), maximum.load());
            current.store(newEnergy);
            totalRegen.store(totalRegen.load() + (newEnergy - oldEnergy));
            lastRegen.store(getMSTime());
        }

        float GetEnergyPercent() const {
            return maximum > 0 ? (static_cast<float>(current) / maximum) : 0.0f;
        }

        void Reset() {
            current.store(maximum.load()); totalSpent.store(0); totalRegen.store(0);
            efficiency.store(1.0f);
            while (!spendingHistory.empty())
                spendingHistory.pop();
        }
    } _energyManager;

    // Stagger management system (Brewmaster)
    struct StaggerManagementSystem {
        std::atomic<uint32> currentStagger{0};
        std::atomic<uint32> totalAbsorbed{0};
        std::atomic<uint32> totalPurified{0};
        std::atomic<uint32> heavyStaggerTime{0};
        std::atomic<uint32> moderateStaggerTime{0};
        std::atomic<uint32> lightStaggerTime{0};
        std::atomic<float> mitigationRatio{0.0f};
        std::queue<std::pair<uint32, uint32>> staggerHistory; // <damage, timestamp>

        // Make non-copyable due to atomic members
        StaggerManagementSystem() = default;
        StaggerManagementSystem(const StaggerManagementSystem&) = delete;
        StaggerManagementSystem& operator=(const StaggerManagementSystem&) = delete;

        enum StaggerLevel {
            NONE = 0,
            LIGHT = 1,
            MODERATE = 2,
            HEAVY = 3
        };

        StaggerLevel GetStaggerLevel(float maxHealth) const {
            float ratio = currentStagger.load() / maxHealth;
            if (ratio >= 0.6f) return HEAVY;
            if (ratio >= 0.3f) return MODERATE;
            if (ratio > 0.0f) return LIGHT;
            return NONE;
        }

        void AddStagger(uint32 damage) {
            currentStagger.store(currentStagger.load() + damage);
            totalAbsorbed.store(totalAbsorbed.load() + damage);
            staggerHistory.push({damage, getMSTime()});
            while (staggerHistory.size() > 30)
                staggerHistory.pop();
        }

        void PurifyStagger(float percent) {
            uint32 purified = static_cast<uint32>(currentStagger.load() * percent);
            currentStagger.store(currentStagger.load() - purified);
            totalPurified.store(totalPurified.load() + purified);
        }

        void UpdateStaggerTracking(StaggerLevel level, uint32 diff) {
            switch(level) {
                case HEAVY: heavyStaggerTime.store(heavyStaggerTime.load() + diff); break;
                case MODERATE: moderateStaggerTime.store(moderateStaggerTime.load() + diff); break;
                case LIGHT: lightStaggerTime.store(lightStaggerTime.load() + diff); break;
                default: break;
            }
        }

        float CalculateMitigationEfficiency() const {
            uint32 absorbed = totalAbsorbed.load();
            if (absorbed == 0) return 0.0f;
            return static_cast<float>(totalPurified.load()) / absorbed;
        }

        void Reset() {
            currentStagger.store(0); totalAbsorbed.store(0); totalPurified.store(0);
            heavyStaggerTime.store(0); moderateStaggerTime.store(0); lightStaggerTime.store(0);
            mitigationRatio.store(0.0f);
            while (!staggerHistory.empty())
                staggerHistory.pop();
        }
    } _staggerManager;

    // Martial arts form management
    struct FormManagementSystem {
        std::atomic<MartialForm> currentForm{MartialForm::NONE};
        std::atomic<uint32> lastFormChange{0};
        std::atomic<uint32> formChanges{0};
        std::array<std::atomic<uint32>, 4> formDuration{{0, 0, 0, 0}};
        std::array<std::atomic<float>, 4> formEfficiency{{1.0f, 1.0f, 1.0f, 1.0f}};

        // Make non-copyable due to atomic members
        FormManagementSystem() = default;
        FormManagementSystem(const FormManagementSystem&) = delete;
        FormManagementSystem& operator=(const FormManagementSystem&) = delete;

        void ChangeForm(MartialForm newForm) {
            if (currentForm.load() != newForm) {
                currentForm.store(newForm);
                lastFormChange.store(getMSTime());
                formChanges.store(formChanges.load() + 1);
            }
        }

        void UpdateFormDuration(uint32 diff) {
            MartialForm current = currentForm.load();
            if (current != MartialForm::NONE) {
                uint8 index = static_cast<uint8>(current);
                formDuration[index].store(formDuration[index].load() + diff);
            }
        }

        MartialForm GetOptimalForm(::Unit* target, bool isTank, bool isDPS, bool isHealer) {
            if (isTank) return MartialForm::OX_FORM;
            if (isHealer) return MartialForm::SERPENT_FORM;
            if (isDPS) {
                // Dynamic form selection based on situation
                if (target && target->GetHealthPct() < 35.0f)
                    return MartialForm::TIGER_FORM; // Execute phase
                return MartialForm::CRANE_FORM; // Normal DPS
            }
            return MartialForm::TIGER_FORM;
        }

        void Reset() {
            currentForm.store(MartialForm::NONE);
            formChanges.store(0);
            for (auto& duration : formDuration) duration.store(0);
            for (auto& efficiency : formEfficiency) efficiency.store(1.0f);
        }
    } _formManager;

    // Enhanced performance tracking
    std::atomic<uint32> _chiGenerated{0};
    std::atomic<uint32> _chiSpent{0};
    std::atomic<uint32> _energySpent{0};
    std::atomic<uint32> _damageDealt{0};
    std::atomic<uint32> _healingDone{0};
    std::atomic<uint32> _staggerAbsorbed{0};
    std::atomic<uint32> _staggerPurified{0};
    std::atomic<uint32> _successfulComboStrikes{0};
    std::atomic<uint32> _successfulInterrupts{0};
    std::atomic<uint32> _mobilityUsages{0};
    std::atomic<uint32> _defensiveCooldownsUsed{0};

    // Combat system integration
    std::unique_ptr<BotThreatManager> _threatManager;
    std::unique_ptr<TargetSelector> _targetSelector;
    std::unique_ptr<PositionManager> _positionManager;
    std::unique_ptr<InterruptManager> _interruptManager;
    std::unique_ptr<FormationManager> _formationManager;

    // Combo strike tracking (Windwalker)
    struct ComboStrikeTracker {
        std::atomic<uint32> lastAbility{0};
        std::atomic<uint32> comboCount{0};
        std::atomic<float> comboDamageBonus{1.0f};
        std::queue<uint32> abilityHistory;
        std::atomic<uint32> perfectCombos{0};

        // Make non-copyable due to atomic members
        ComboStrikeTracker() = default;
        ComboStrikeTracker(const ComboStrikeTracker&) = delete;
        ComboStrikeTracker& operator=(const ComboStrikeTracker&) = delete;

        bool WillBreakCombo(uint32 spellId) const {
            return lastAbility.load() == spellId;
        }

        void RecordAbility(uint32 spellId) {
            if (!WillBreakCombo(spellId)) {
                uint32 newComboCount = comboCount.load() + 1;
                comboCount.store(newComboCount);
                comboDamageBonus.store(1.0f + (newComboCount * 0.02f)); // 2% per combo
                if (newComboCount >= 10) perfectCombos.store(perfectCombos.load() + 1);
            } else {
                comboCount.store(0);
                comboDamageBonus.store(1.0f);
            }
            lastAbility.store(spellId);
            abilityHistory.push(spellId);
            while (abilityHistory.size() > 10)
                abilityHistory.pop();
        }

        void Reset() {
            lastAbility.store(0); comboCount.store(0); comboDamageBonus.store(1.0f);
            perfectCombos.store(0);
            while (!abilityHistory.empty())
                abilityHistory.pop();
        }
    } _comboTracker;

    // Healing management (Mistweaver)
    struct HealingPrioritySystem {
        // Make non-copyable due to atomic members
        HealingPrioritySystem() = default;
        HealingPrioritySystem(const HealingPrioritySystem&) = delete;
        HealingPrioritySystem& operator=(const HealingPrioritySystem&) = delete;
        struct HealTarget {
            ::Unit* target;
            float healthPercent;
            uint32 missingHealth;
            uint32 priority;
            bool hasRenewingMist;
            bool hasEnvelopingMist;
            uint32 timestamp;

            bool operator<(const HealTarget& other) const {
                if (priority != other.priority) return priority < other.priority;
                return healthPercent > other.healthPercent;
            }
        };

        std::priority_queue<HealTarget> healQueue;
        std::atomic<uint32> totalHealing{0};
        std::atomic<uint32> overhealingDone{0};
        std::atomic<float> healingEfficiency{1.0f};
        std::atomic<bool> fistweavingMode{false};

        void UpdateHealingTargets(const std::vector<::Unit*>& allies) {
            while (!healQueue.empty()) healQueue.pop();

            for (auto* ally : allies) {
                if (!ally || ally->isDead()) continue;

                HealTarget target;
                target.target = ally;
                target.healthPercent = ally->GetHealthPct();
                target.missingHealth = ally->GetMaxHealth() - ally->GetHealth();
                target.priority = CalculatePriority(ally);
                target.hasRenewingMist = false; // Check for actual aura
                target.hasEnvelopingMist = false; // Check for actual aura
                target.timestamp = getMSTime();

                healQueue.push(target);
            }
        }

        uint32 CalculatePriority(::Unit* target) {
            uint32 priority = 100;
            float healthPct = target->GetHealthPct();

            if (healthPct < 20.0f) priority = 1000;
            else if (healthPct < 40.0f) priority = 500;
            else if (healthPct < 60.0f) priority = 200;

            // Tank priority boost
            if (target->GetTypeId() == TYPEID_PLAYER) {
                Player* player = target->ToPlayer();
                if (player) // TODO: Implement role detection
                    priority += 50; // Generic player boost
            }

            return priority;
        }

        ::Unit* GetNextHealTarget() {
            if (healQueue.empty()) return nullptr;
            HealTarget top = healQueue.top();
            healQueue.pop();
            return top.target;
        }

        void RecordHealing(uint32 amount, uint32 overheal) {
            totalHealing.store(totalHealing.load() + amount);
            overhealingDone.store(overhealingDone.load() + overheal);
            uint32 currentTotal = totalHealing.load();
            if (currentTotal > 0) {
                healingEfficiency.store(1.0f - (static_cast<float>(overhealingDone.load()) / currentTotal));
            }
        }

        void Reset() {
            while (!healQueue.empty()) healQueue.pop();
            totalHealing.store(0); overhealingDone.store(0);
            healingEfficiency.store(1.0f); fistweavingMode.store(false);
        }
    } _healingSystem;

    // Shared utility tracking
    std::unordered_map<uint32, uint32> _abilityUsage;
    std::unordered_map<uint32, uint32> _abilityCooldowns;
    uint32 _lastLegacyBuff;
    uint32 _lastMobilityUse;
    uint32 _lastDefensiveUse;
    uint32 _lastInterruptAttempt;
    bool _needsRoll;
    bool _needsTranscendence;
    Position _transcendencePosition;
    bool _transcendenceActive;

    // Specialization management
    void InitializeSpecialization();
    void UpdateSpecialization();
    MonkSpec DetectCurrentSpecialization();
    void SwitchSpecialization(MonkSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared monk utilities
    void UpdateMonkBuffs();
    void CastLegacyBuffs();
    void ManageChiGeneration();
    void ManageEnergyRegeneration(uint32 diff);
    void OptimizeResourceUsage();

    // Mobility management
    void UpdateMobility(::Unit* target);
    void CastRoll(Position targetPos);
    void CastFlyingSerpentKick(::Unit* target);
    void SetupTranscendence();
    void UseTranscendenceTransfer();
    bool ShouldUseMobility(::Unit* target);
    Position CalculateRollDestination(::Unit* target);

    // Enhanced target evaluation
    bool IsValidTarget(::Unit* target);
    ::Unit* GetBestKickTarget();
    ::Unit* GetBestStunTarget();
    ::Unit* GetHighestThreatTarget();
    ::Unit* GetLowestHealthEnemy();
    ::Unit* SelectOptimalTarget(const std::vector<::Unit*>& enemies);
    float CalculateTargetPriority(::Unit* target);

    // Advanced combat mechanics
    void UpdateAdvancedCombatLogic(::Unit* target);
    void HandleMultipleEnemies(const std::vector<::Unit*>& enemies);
    void OptimizeFormForSituation(::Unit* target);
    void ManageResourceEfficiency();
    void ExecuteAdvancedRotation(::Unit* target);

    // Form management and optimization
    void OptimizeFormManagement(::Unit* target);
    MartialForm DetermineOptimalForm(::Unit* target, const std::vector<::Unit*>& enemies);
    void HandleFormSpecificAbilities(MartialForm form, ::Unit* target);
    void ManageTacticalFormSwitching();

    // Chi and energy optimization
    void OptimizeChiUsage();
    void OptimizeEnergyUsage();
    bool ShouldConserveChi();
    bool ShouldConserveEnergy();
    void HandleResourceStarvation();
    void PrioritizeResourceSpenders(::Unit* target);

    // Defensive and survival mechanics
    void HandleDefensiveSituation();
    void UseDefensiveCooldowns();
    void ManageHealthThresholds();
    void HandleCrowdControl();
    void OptimizeInterruptTiming();
    void ManageStaggerMechanics(); // Brewmaster specific

    // Healing optimization (Mistweaver)
    void OptimizeHealingRotation();
    void ManageFistweaving();
    void HandleEmergencyHealing();
    void OptimizeManaUsage();
    void CoordinateGroupHealing();

    // DPS optimization (Windwalker)
    void OptimizeComboStrikes();
    void ManageStormEarthAndFire();
    void HandleTouchOfDeath();
    void OptimizeBurstWindows();
    void ManageMarkOfTheCrane();

    // Tank optimization (Brewmaster)
    void OptimizeBrewUsage();
    void ManageStaggerLevel();
    void HandleActiveMitigation();
    void OptimizeThreatGeneration();
    void ManageBrewCharges();

    // Group combat optimization
    void HandleGroupCombatRole();
    void ManageThreatInGroup();
    void CoordinateWithGroup();
    void OptimizeFormationPosition();
    void ProvideSupportiveBuffs();

    // Performance metrics and analytics
    struct MonkMetrics {
        std::atomic<uint32> totalAbilitiesUsed{0};
        std::atomic<uint32> successfulCombos{0};
        std::atomic<uint32> chiWasted{0};
        std::atomic<uint32> energyWasted{0};
        std::atomic<float> averageChiEfficiency{0.0f};
        std::atomic<float> averageEnergyEfficiency{0.0f};
        std::atomic<float> formOptimizationScore{0.0f};
        std::atomic<float> healingEfficiencyScore{0.0f};
        std::atomic<float> staggerMitigationScore{0.0f};
        std::atomic<float> comboStrikeScore{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastMetricsUpdate;

        // Make non-copyable due to atomic members
        MonkMetrics() = default;
        MonkMetrics(const MonkMetrics&) = delete;
        MonkMetrics& operator=(const MonkMetrics&) = delete;

        void Reset() {
            totalAbilitiesUsed.store(0); successfulCombos.store(0);
            chiWasted.store(0); energyWasted.store(0);
            averageChiEfficiency.store(0.0f); averageEnergyEfficiency.store(0.0f);
            formOptimizationScore.store(0.0f); healingEfficiencyScore.store(0.0f);
            staggerMitigationScore.store(0.0f); comboStrikeScore.store(0.0f);
            combatStartTime = std::chrono::steady_clock::now();
            lastMetricsUpdate = combatStartTime;
        }
    } _monkMetrics;

    // Enhanced performance optimization
    void RecordAbilityUsage(uint32 spellId);
    void RecordResourceGeneration(uint32 chi, uint32 energy);
    void RecordResourceConsumption(uint32 chi, uint32 energy);
    void RecordFormChange(MartialForm fromForm, MartialForm toForm);
    void RecordComboStrike(uint32 spellId, bool successful);
    void RecordHealingDone(::Unit* target, uint32 amount, uint32 overheal);
    void RecordStaggerAbsorption(uint32 damage);
    void RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success);
    void AnalyzeCombatEffectiveness();
    void UpdateMetrics(uint32 diff);
    float CalculateResourceEfficiency();
    void OptimizeAbilityPriorities();

    // Advanced positioning
    void OptimizePositioning(::Unit* target);
    void HandleMeleePositioning(::Unit* target);
    void HandleRangedPositioning(::Unit* target);
    void HandleHealerPositioning();
    void ManageTranscendencePositioning();
    Position CalculateOptimalPosition(::Unit* target, float range);

    // Interrupt coordination
    void ManageInterruptCoordination();
    bool ShouldInterruptSpell(::Unit* caster, uint32 spellId);
    uint32 SelectInterruptAbility();
    void CoordinateGroupInterrupts();

    // Enhanced constants
    static constexpr uint32 FORM_CHANGE_COOLDOWN = 1500; // 1.5 seconds
    static constexpr uint32 ROLL_COOLDOWN = 20000; // 20 seconds
    static constexpr uint32 ROLL_CHARGES = 2;
    static constexpr float ROLL_DISTANCE = 15.0f;
    static constexpr uint32 TRANSCENDENCE_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 MONK_CHI_THRESHOLD = 2;
    static constexpr uint32 MONK_ENERGY_THRESHOLD = 30;
    static constexpr float MONK_OPTIMAL_MELEE_RANGE = 5.0f;
    static constexpr float MONK_OPTIMAL_HEAL_RANGE = 40.0f;
    static constexpr float OPTIMAL_KICK_RANGE = 5.0f;
    static constexpr uint32 HEALTH_EMERGENCY_THRESHOLD = 30; // 30% health
    static constexpr uint32 DEFENSIVE_COOLDOWN_THRESHOLD = 50; // 50% health
    static constexpr uint32 STAGGER_PURIFY_THRESHOLD = 60; // 60% stagger
    static constexpr float CHI_EFFICIENCY_TARGET = 0.90f; // 90% efficiency
    static constexpr float ENERGY_EFFICIENCY_TARGET = 0.85f; // 85% efficiency
    static constexpr uint32 FORM_OPTIMIZATION_INTERVAL = 5000; // 5 seconds
    static constexpr float MULTI_TARGET_THRESHOLD = 3.0f; // 3+ enemies
    static constexpr uint32 COMBO_STRIKE_WINDOW = 15000; // 15 seconds
    static constexpr uint32 TOUCH_OF_DEATH_THRESHOLD = 15; // 15% health

    // Spell IDs
    enum MonkSpells
    {
        // Chi generators
        TIGER_PALM = 100780,
        EXPEL_HARM = 115072,
        CHI_WAVE = 115098,
        CHI_BURST = 123986,
        JAB = 100780,

        // Basic attacks
        BLACKOUT_KICK = 100784,
        RISING_SUN_KICK = 107428,
        SPINNING_CRANE_KICK = 101546,

        // Windwalker abilities
        FISTS_OF_FURY = 113656,
        WHIRLING_DRAGON_PUNCH = 152175,
        STORM_EARTH_AND_FIRE = 137639,
        TOUCH_OF_DEATH = 115080,
        FLYING_SERPENT_KICK = 101545,
        MARK_OF_THE_CRANE = 228287,
        RUSHING_JADE_WIND = 116847,

        // Brewmaster abilities
        KEG_SMASH = 121253,
        BREATH_OF_FIRE = 115181,
        IRONSKIN_BREW = 115308,
        PURIFYING_BREW = 119582,
        FORTIFYING_BREW = 115203,
        BLACK_OX_BREW = 115399,
        STAGGER_HEAVY = 124273,
        STAGGER_MODERATE = 124274,
        STAGGER_LIGHT = 124275,
        ZEN_MEDITATION = 115176,
        DAMPEN_HARM = 122278,
        GUARD = 115295,

        // Mistweaver abilities
        RENEWING_MIST = 115151,
        ENVELOPING_MIST = 124682,
        VIVIFY = 116670,
        ESSENCE_FONT = 191837,
        SOOTHING_MIST = 115175,
        LIFE_COCOON = 116849,
        REVIVAL = 115310,
        THUNDER_FOCUS_TEA = 116680,
        MANA_TEA = 115294,
        TEACHINGS_OF_THE_MONASTERY = 202090,
        SHEILUNS_GIFT = 205406,

        // Mobility
        ROLL = 109132,
        CHI_TORPEDO = 115008,
        TRANSCENDENCE = 101643,
        TRANSCENDENCE_TRANSFER = 119996,
        TIGERS_LUST = 116841,

        // Utility and crowd control
        PARALYSIS = 115078,
        LEG_SWEEP = 119381,
        SPEAR_HAND_STRIKE = 116705,
        RING_OF_PEACE = 116844,
        CRACKLING_JADE_LIGHTNING = 117952,
        DETOX = 115450,
        RESUSCITATE = 115178,

        // Buffs
        LEGACY_OF_THE_WHITE_TIGER = 116781,
        LEGACY_OF_THE_EMPEROR = 115921,

        // Defensive cooldowns
        TOUCH_OF_KARMA = 122470,
        DIFFUSE_MAGIC = 122783,

        // Talents
        CHI_ORBIT = 196743,
        ENERGIZING_ELIXIR = 115288,
        POWER_STRIKES = 121817,
        EYE_OF_THE_TIGER = 196607,
        CHI_EXPLOSION = 152174,
        SERENITY = 152173
    };

    // Advanced specialization management methods
    void HandleAdvancedBrewmasterManagement();
    void HandleAdvancedMistweaverManagement();
    void HandleAdvancedWindwalkerManagement();
};

} // namespace Playerbot