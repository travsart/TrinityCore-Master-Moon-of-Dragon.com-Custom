/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MONK_SPECIALIZATION_H
#define MONK_SPECIALIZATION_H

#include "ClassAI.h"
#include "Position.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <queue>

namespace Playerbot
{

// Forward declarations
enum class MonkSpec : uint8;

// Chi resource management
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

// Energy resource management
struct EnergyInfo
{
    uint32 current;
    uint32 maximum;
    uint32 regenRate;
    uint32 lastRegen;
    bool isRegenerating;

    EnergyInfo() : current(100), maximum(100), regenRate(10), lastRegen(0), isRegenerating(true) {}

    bool HasEnergy(uint32 required) const { return current >= required; }
    void SpendEnergy(uint32 amount) { current = current >= amount ? current - amount : 0; }
    void RegenEnergy(uint32 amount) { current = std::min(current + amount, maximum); }
    float GetPercent() const { return maximum > 0 ? (float)current / maximum : 0.0f; }
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

    void UpdateStaggerLevel()
    {
        isHeavy = tickDamage > 1000;
        isModerate = !isHeavy && tickDamage > 500;
        isLight = !isHeavy && !isModerate && tickDamage > 0;
    }
};

// Brew management for Brewmaster
struct BrewInfo
{
    uint32 ironskinCharges;
    uint32 purifyingCharges;
    uint32 maxCharges;
    uint32 rechargeTime;
    uint32 lastRecharge;

    BrewInfo() : ironskinCharges(3), purifyingCharges(3), maxCharges(3), rechargeTime(20000), lastRecharge(0) {}

    bool HasIronskinCharges() const { return ironskinCharges > 0; }
    bool HasPurifyingCharges() const { return purifyingCharges > 0; }
    void UseIronskinBrew() { if (ironskinCharges > 0) ironskinCharges--; }
    void UsePurifyingBrew() { if (purifyingCharges > 0) purifyingCharges--; }
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

// Fistweaving information
struct FistweavingInfo
{
    bool isActive;
    uint32 lastToggle;
    uint32 fistweavingHealing;
    uint32 directHealing;
    uint32 evaluationPeriod;
    float efficiency;

    FistweavingInfo() : isActive(false), lastToggle(0), fistweavingHealing(0),
                        directHealing(0), evaluationPeriod(30000), efficiency(1.0f) {}
};

// Combo point system for Windwalker
struct ComboInfo
{
    uint32 comboPower;
    uint32 tigerPalmStacks;
    uint32 markOfTheCraneStacks;
    uint32 lastComboSpender;
    bool stormEarthAndFireActive;
    uint32 touchOfDeathReady;

    ComboInfo() : comboPower(0), tigerPalmStacks(0), markOfTheCraneStacks(0),
                  lastComboSpender(0), stormEarthAndFireActive(false), touchOfDeathReady(0) {}
};

class MonkSpecialization
{
public:
    explicit MonkSpecialization(Player* bot);
    virtual ~MonkSpecialization() = default;

    // Core interface - must be implemented by specializations
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual void UpdateCooldowns(uint32 diff) = 0;
    virtual bool CanUseAbility(uint32 spellId) = 0;
    virtual void OnCombatStart(::Unit* target) = 0;
    virtual void OnCombatEnd() = 0;

    // Resource management interface
    virtual bool HasEnoughResource(uint32 spellId) = 0;
    virtual void ConsumeResource(uint32 spellId) = 0;

    // Positioning interface
    virtual Position GetOptimalPosition(::Unit* target) = 0;
    virtual float GetOptimalRange(::Unit* target) = 0;

    // Target selection interface
    virtual ::Unit* GetBestTarget() = 0;

protected:
    // Shared resource management
    void UpdateChiManagement();
    void UpdateEnergyManagement();

    // Chi system
    bool HasChi(uint32 required = 1) const;
    uint32 GetChi() const;
    uint32 GetMaxChi() const;
    void SpendChi(uint32 amount);
    void GenerateChi(uint32 amount);

    // Energy system
    bool HasEnergy(uint32 required) const;
    uint32 GetEnergy() const;
    uint32 GetMaxEnergy() const;
    void SpendEnergy(uint32 amount);
    void RegenEnergy(uint32 amount);
    float GetEnergyPercent() const;

    // Shared combat abilities
    bool CastSpell(uint32 spellId, ::Unit* target = nullptr);
    bool HasSpell(uint32 spellId) const;
    bool HasAura(uint32 spellId, ::Unit* target = nullptr) const;
    uint32 GetSpellCooldown(uint32 spellId) const;
    bool IsSpellReady(uint32 spellId) const;

    // Target selection helpers
    std::vector<::Unit*> GetNearbyEnemies(float range = 30.0f) const;
    std::vector<::Unit*> GetNearbyAllies(float range = 40.0f) const;
    std::vector<::Unit*> GetAoETargets(float range = 8.0f) const;
    ::Unit* GetCurrentTarget() const;

    // Positioning helpers
    bool IsInMeleeRange(::Unit* target) const;
    bool IsAtOptimalRange(::Unit* target) const;
    float GetDistance(::Unit* target) const;

    // Common buff management
    void UpdateSharedBuffs();
    void CastLegacyOfTheWhiteTiger();
    void CastLegacyOfTheEmperor();

    // Utility abilities
    void CastRoll();
    void CastTeleport();
    void CastTranscendence();
    void CastTranscendenceTransfer();
    void CastParalysis(::Unit* target);
    void CastLegSweep();
    void CastSpearHandStrike(::Unit* target);

    // Debug and logging
    void LogRotationDecision(const std::string& decision, const std::string& reason);

    // Bot reference
    Player* _bot;

    // Shared resource state
    ChiInfo _chi;
    EnergyInfo _energy;
    uint32 _mana;
    uint32 _maxMana;

    // Combat tracking
    uint32 _combatStartTime;
    uint32 _averageCombatTime;
    ::Unit* _currentTarget;

    // Timing variables
    uint32 _lastChiGeneration;
    uint32 _lastEnergyRegen;
    uint32 _lastBuffUpdate;
    uint32 _lastUtilityUse;

    // Mobility tracking
    uint32 _lastRoll;
    uint32 _lastTeleport;
    uint32 _lastTranscendence;
    Position _transcendencePosition;
    bool _inTranscendence;

    // Performance tracking
    uint32 _damageDealt;
    uint32 _healingDone;
    uint32 _damageMitigated;
    uint32 _chiGenerated;
    uint32 _energySpent;

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_HEAL_RANGE = 40.0f;
    static constexpr uint32 CHI_GENERATION_INTERVAL = 4000; // 4 seconds
    static constexpr uint32 ENERGY_REGEN_RATE = 100; // per second
    static constexpr float CHI_CONSERVATION_THRESHOLD = 0.5f; // 50%
    static constexpr float ENERGY_CONSERVATION_THRESHOLD = 0.3f; // 30%

    // Monk spell IDs
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

} // namespace Playerbot

#endif // MONK_SPECIALIZATION_H