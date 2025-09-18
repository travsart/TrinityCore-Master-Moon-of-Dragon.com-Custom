/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ClassAI.h"
#include "Position.h"
#include <unordered_map>
#include <queue>
#include <vector>

namespace Playerbot
{

// Monk specializations
enum class MonkSpec : uint8
{
    BREWMASTER = 0,
    MISTWEAVER = 1,
    WINDWALKER = 2
};

// Monk resource types
enum class MonkResource : uint8
{
    CHI = 0,
    ENERGY = 1,
    MANA = 2
};

// Stagger damage tracking for Brewmaster
struct StaggerInfo
{
    uint32 totalDamage;
    uint32 tickDamage;
    uint32 remainingTime;
    uint32 lastTick;
    bool isHeavy;
    bool isModerate;
    bool isLight;

    StaggerInfo() : totalDamage(0), tickDamage(0), remainingTime(0), lastTick(0),
                    isHeavy(false), isModerate(false), isLight(false) {}

    StaggerInfo(uint32 total, uint32 tick, uint32 duration)
        : totalDamage(total), tickDamage(tick), remainingTime(duration),
          lastTick(getMSTime()), isHeavy(false), isModerate(false), isLight(false)
    {
        UpdateStaggerLevel();
    }

    void UpdateStaggerLevel()
    {
        isHeavy = tickDamage > 1000;
        isModerate = !isHeavy && tickDamage > 500;
        isLight = !isHeavy && !isModerate && tickDamage > 0;
    }
};

// Chi resource tracking
struct ChiInfo
{
    uint32 current;
    uint32 maximum;
    uint32 lastGenerated;
    uint32 generationRate;
    bool isRegenerating;

    ChiInfo() : current(0), maximum(4), lastGenerated(0), generationRate(4000), isRegenerating(false) {}

    bool HasChi(uint32 required = 1) const { return current >= required; }
    void SpendChi(uint32 amount) { current = current >= amount ? current - amount : 0; }
    void GenerateChi(uint32 amount) { current = std::min(current + amount, maximum); }
};

// Mistweaver healing target info
struct MistweaverTarget
{
    ::Unit* target;
    float healthPercent;
    uint32 missingHealth;
    bool hasHoTs;
    bool inMeleeRange;
    uint32 priority;
    uint32 timestamp;

    MistweaverTarget() : target(nullptr), healthPercent(100.0f), missingHealth(0),
                         hasHoTs(false), inMeleeRange(false), priority(0), timestamp(0) {}

    MistweaverTarget(::Unit* t, float hp, uint32 missing)
        : target(t), healthPercent(hp), missingHealth(missing),
          hasHoTs(false), inMeleeRange(false), priority(0), timestamp(getMSTime()) {}

    bool operator<(const MistweaverTarget& other) const
    {
        if (priority != other.priority)
            return priority < other.priority;
        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent;
        return timestamp > other.timestamp;
    }
};

// Monk AI implementation with full chi, stagger, and fistweaving
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
    // Monk-specific data
    MonkSpec _specialization;
    uint32 _damageDealt;
    uint32 _healingDone;
    uint32 _damageMitigated;
    uint32 _chiGenerated;
    uint32 _energySpent;

    // Resource management
    ChiInfo _chi;
    uint32 _energy;
    uint32 _maxEnergy;
    uint32 _mana;
    uint32 _maxMana;
    uint32 _lastEnergyRegen;
    uint32 _lastChiGeneration;

    // Brewmaster stagger system
    StaggerInfo _stagger;
    uint32 _lastStaggerClear;
    uint32 _staggerCheckInterval;
    bool _needsStaggerManagement;
    uint32 _ironskinBrewCharges;
    uint32 _purifyingBrewCharges;

    // Mistweaver healing system
    std::priority_queue<MistweaverTarget> _healingTargets;
    std::unordered_map<ObjectGuid, uint32> _renewingMistTimers;
    std::unordered_map<ObjectGuid, uint32> _envelopingMistTimers;
    uint32 _lastHealingScan;
    bool _fistweavingMode;
    uint32 _soothinMistChanneling;

    // Windwalker combo system
    uint32 _comboPower;
    uint32 _tigerPalmStacks;
    uint32 _lastComboSpender;
    uint32 _markOfTheCraneStacks;
    bool _stormEarthAndFireActive;
    uint32 _touchOfDeathReady;

    // Monk-specific buffs and debuffs
    uint32 _lastExpelHarm;
    uint32 _lastFortifyingBrew;
    uint32 _lastZenMeditation;
    uint32 _lastTranscendence;
    std::unordered_map<ObjectGuid, uint32> _markOfTheCraneTargets;

    // Mobility and utility
    uint32 _lastRoll;
    uint32 _lastTeleport;
    uint32 _lastParalysis;
    uint32 _lastSpearHandStrike;
    bool _inTranscendence;
    Position _transcendencePosition;

    // Rotation methods by specialization
    void UpdateBrewmasterRotation(::Unit* target);
    void UpdateMistweaverRotation(::Unit* target);
    void UpdateWindwalkerRotation(::Unit* target);

    // Chi management system
    void UpdateChiManagement();
    void GenerateChi(uint32 amount);
    void SpendChi(uint32 amount);
    bool HasChi(uint32 required = 1);
    void OptimizeChiUsage();
    uint32 GetChi();
    uint32 GetMaxChi();

    // Energy management system
    void UpdateEnergyManagement();
    bool HasEnergy(uint32 required);
    void SpendEnergy(uint32 amount);
    uint32 GetEnergy();
    uint32 GetMaxEnergy();
    float GetEnergyPercent();

    // Stagger management (Brewmaster)
    void UpdateStaggerManagement();
    void ClearStagger();
    void ManageBrews();
    bool ShouldUsePurifyingBrew();
    bool ShouldUseIronskinBrew();
    void CastPurifyingBrew();
    void CastIronskinBrew();
    void UpdateStaggerLevel();

    // Healing system (Mistweaver)
    void UpdateMistweaverHealing();
    void ScanForHealTargets();
    void ExecuteFistweaving(::Unit* target);
    void ExecuteHealingRotation();
    ::Unit* GetBestHealTarget();
    ::Unit* GetBestFistweavingTarget();
    void ToggleFistweavingMode();

    // Mistweaver healing abilities
    void CastEnvelopingMist(::Unit* target);
    void CastRenewingMist(::Unit* target);
    void CastVivify(::Unit* target);
    void CastEssenceFont();
    void CastSoothingMist(::Unit* target);
    void CastLifeCocoon(::Unit* target);

    // Combo system (Windwalker)
    void UpdateComboSystem();
    void BuildCombo();
    void SpendCombo();
    void ManageMarkOfTheCrane();
    void CastTouchOfDeath(::Unit* target);
    bool ShouldUseTouchOfDeath(::Unit* target);

    // Windwalker combat abilities
    void CastTigerPalm(::Unit* target);
    void CastBlackoutKick(::Unit* target);
    void CastRisingSunKick(::Unit* target);
    void CastFistsOfFury(::Unit* target);
    void CastWhirlingDragonPunch();
    void CastStormEarthAndFire();

    // Brewmaster defensive abilities
    void UseBrewmasterDefensives();
    void CastFortifyingBrew();
    void CastZenMeditation();
    void CastExpelHarm();
    void CastDampenHarm();
    void ManageActiveDefensives();

    // Shared combat abilities
    void CastCracklingJadeLightning(::Unit* target);
    void CastSpinningCraneKick();
    void CastLegSweep();
    void CastParalysis(::Unit* target);
    void CastSpearHandStrike(::Unit* target);

    // Mobility abilities
    void UseMobilityAbilities();
    void CastRoll();
    void CastTeleport();
    void CastTranscendence();
    void CastTranscendenceTransfer();
    void UpdateTranscendence();

    // Buff management
    void UpdateMonkBuffs();
    void CastLegacyOfTheWhiteTiger();
    void CastLegacyOfTheEmperor();
    void RefreshSelfBuffs();

    // Resource optimization
    void OptimizeResourceUsage();
    void BalanceChiAndEnergy();
    void ConserveMana();
    bool ShouldConserveResources();

    // Positioning and movement
    void UpdateMonkPositioning();
    bool IsAtOptimalRange(::Unit* target);
    void MaintainMeleeRange(::Unit* target);
    void MaintainHealingPosition();
    bool NeedsRepositioning();

    // Target selection
    ::Unit* GetBestCombatTarget();
    ::Unit* GetBestInterruptTarget();
    ::Unit* GetBestStunTarget();
    ::Unit* GetHighestPriorityThreat();
    std::vector<::Unit*> GetAoETargets();

    // Threat and aggro management
    void ManageThreat();
    void BuildThreat(::Unit* target);
    void ReduceThreat();
    bool HasTooMuchThreat();

    // Utility and crowd control
    void UseUtilityAbilities();
    void UseEmergencyAbilities();
    void InterruptDangerousSpells();
    void HandleCrowdControl();

    // Performance tracking
    void RecordDamageDealt(uint32 damage, ::Unit* target);
    void RecordHealingDone(uint32 amount, ::Unit* target);
    void RecordDamageMitigated(uint32 amount);
    void AnalyzePerformance();

    // Helper methods
    bool IsChanneling();
    bool IsGlobalCooldownActive();
    MonkSpec DetectSpecialization();
    bool HasBrewCharges();
    uint32 GetBrewCharges();
    bool IsInMeleeRange(::Unit* target);

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_HEAL_RANGE = 40.0f;
    static constexpr uint32 CHI_GENERATION_INTERVAL = 4000; // 4 seconds
    static constexpr uint32 ENERGY_REGEN_RATE = 100; // per second
    static constexpr uint32 STAGGER_CHECK_INTERVAL = 1000; // 1 second
    static constexpr uint32 HEAVY_STAGGER_THRESHOLD = 1000;
    static constexpr uint32 MODERATE_STAGGER_THRESHOLD = 500;
    static constexpr float CHI_CONSERVATION_THRESHOLD = 0.5f; // 50%
    static constexpr float ENERGY_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr uint32 BREW_CHARGES_MAX = 3;
    static constexpr uint32 FISTWEAVING_HEAL_THRESHOLD = 80; // Switch when group above 80%

    // Spell IDs (version-specific)
    enum MonkSpells
    {
        // Chi generators
        TIGER_PALM = 100780,
        EXPEL_HARM = 115072,
        CHI_WAVE = 115098,
        CHI_BURST = 123986,

        // Chi spenders - Windwalker
        BLACKOUT_KICK = 100784,
        RISING_SUN_KICK = 107428,
        FISTS_OF_FURY = 113656,
        WHIRLING_DRAGON_PUNCH = 152175,

        // Chi spenders - Brewmaster
        BREATH_OF_FIRE = 115181,
        KEG_SMASH = 121253,
        SPINNING_CRANE_KICK = 101546,

        // Brewmaster defensives
        IRONSKIN_BREW = 115308,
        PURIFYING_BREW = 119582,
        FORTIFYING_BREW = 115203,
        ZEN_MEDITATION = 115176,
        DAMPEN_HARM = 122278,

        // Mistweaver healing
        RENEWING_MIST = 115151,
        ENVELOPING_MIST = 124682,
        VIVIFY = 116670,
        ESSENCE_FONT = 191837,
        SOOTHING_MIST = 115175,
        LIFE_COCOON = 116849,

        // Mobility
        ROLL = 109132,
        CHI_TORPEDO = 115008,
        TRANSCENDENCE = 101643,
        TRANSCENDENCE_TRANSFER = 119996,

        // Utility and crowd control
        PARALYSIS = 115078,
        LEG_SWEEP = 119381,
        SPEAR_HAND_STRIKE = 116705,
        CRACKLING_JADE_LIGHTNING = 117952,

        // Buffs
        LEGACY_OF_THE_WHITE_TIGER = 116781,
        LEGACY_OF_THE_EMPEROR = 118864,

        // Windwalker specific
        STORM_EARTH_AND_FIRE = 137639,
        TOUCH_OF_DEATH = 115080,
        MARK_OF_THE_CRANE = 228287,

        // Mistweaver specific
        THUNDER_FOCUS_TEA = 116680,
        MANA_TEA = 115294,
        TEACHINGS_OF_THE_MONASTERY = 202090,

        // Brewmaster specific
        STAGGER = 124255,
        HEAVY_STAGGER = 124273,
        MODERATE_STAGGER = 124274,
        LIGHT_STAGGER = 124275
    };
};

// Utility class for monk calculations
class TC_GAME_API MonkCalculator
{
public:
    // Damage calculations
    static uint32 CalculateTigerPalmDamage(Player* caster, ::Unit* target);
    static uint32 CalculateBlackoutKickDamage(Player* caster, ::Unit* target);
    static uint32 CalculateRisingSunKickDamage(Player* caster, ::Unit* target);

    // Healing calculations
    static uint32 CalculateVivifyHealing(Player* caster, ::Unit* target);
    static uint32 CalculateEnvelopingMistHealing(Player* caster, ::Unit* target);

    // Stagger calculations
    static uint32 CalculateStaggerDamage(Player* monk, uint32 incomingDamage);
    static uint32 GetStaggerTickDamage(Player* monk);
    static bool ShouldPurifyStagger(Player* monk, uint32 staggerAmount);

    // Chi efficiency
    static float CalculateChiEfficiency(uint32 spellId, Player* caster);
    static uint32 GetOptimalChiSpender(Player* caster, ::Unit* target);

    // Fistweaving calculations
    static float CalculateFistweavingEfficiency(Player* caster, const std::vector<::Unit*>& healTargets);
    static bool ShouldFistweave(Player* caster, const std::vector<::Unit*>& allies);

private:
    // Cache for monk calculations
    static std::unordered_map<uint32, uint32> _damageCache;
    static std::unordered_map<uint32, uint32> _healingCache;
    static std::mutex _cacheMutex;

    static void CacheMonkData();
};

// Brew management system for Brewmaster
class TC_GAME_API BrewManager
{
public:
    BrewManager(MonkAI* owner);
    ~BrewManager() = default;

    // Brew management
    void Update(uint32 diff);
    void UseIronskinBrew();
    void UsePurifyingBrew();
    bool ShouldUseIronskinBrew() const;
    bool ShouldUsePurifyingBrew() const;

    // Brew state
    uint32 GetBrewCharges() const;
    bool HasBrewCharges() const;
    uint32 GetNextBrewRecharge() const;

    // Stagger management
    void UpdateStagger(uint32 diff);
    bool IsStaggerHeavy() const;
    bool IsStaggerModerate() const;
    uint32 GetStaggerTickDamage() const;

private:
    MonkAI* _owner;
    uint32 _ironskinCharges;
    uint32 _purifyingCharges;
    uint32 _maxCharges;
    uint32 _rechargeTime;
    uint32 _lastRecharge;

    StaggerInfo _currentStagger;
    uint32 _lastStaggerUpdate;

    void RechargeBrews();
    void CalculateStaggerLevel();
};

// Fistweaving controller for Mistweaver
class TC_GAME_API FistweavingController
{
public:
    FistweavingController(MonkAI* owner);
    ~FistweavingController() = default;

    // Fistweaving management
    void Update(uint32 diff);
    void ToggleFistweaving();
    bool ShouldFistweave() const;
    bool IsFistweaving() const;

    // Target selection
    ::Unit* GetFistweavingTarget() const;
    ::Unit* GetHealingTarget() const;

    // Performance tracking
    void RecordFistweavingHealing(uint32 amount);
    void RecordDirectHealing(uint32 amount);
    float GetFistweavingEfficiency() const;

private:
    MonkAI* _owner;
    bool _fistweavingActive;
    uint32 _lastToggle;
    uint32 _fistweavingHealing;
    uint32 _directHealing;
    uint32 _evaluationPeriod;

    void EvaluateFistweavingEffectiveness();
    bool ShouldSwitchToDirectHealing() const;
    bool ShouldSwitchToFistweaving() const;
};

} // namespace Playerbot