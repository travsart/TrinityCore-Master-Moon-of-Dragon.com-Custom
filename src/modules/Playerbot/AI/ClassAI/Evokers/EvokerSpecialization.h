/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef EVOKER_SPECIALIZATION_H
#define EVOKER_SPECIALIZATION_H

#include "Player.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "SharedDefines.h"
#include <chrono>
#include <unordered_map>
#include <queue>
#include <vector>
#include <array>

namespace Playerbot
{

// Evoker Spells
enum EvokerSpells : uint32
{
    // Basic abilities
    AZURE_STRIKE = 362969,
    LIVING_FLAME = 361469,
    HOVER = 358267,
    SOAR = 369536,
    WING_BUFFET = 357214,
    TAIL_SWIPE = 368970,
    DEEP_BREATH = 353995,

    // Devastation abilities
    DISINTEGRATE = 356995,
    PYRE = 357211,
    FIRE_BREATH = 357208,
    ETERNITYS_SURGE = 359073,
    DRAGONRAGE = 375087,
    SHATTERING_STAR = 370452,
    FIRESTORM = 368847,
    BURNOUT = 375801,
    ESSENCE_BURST = 359618,
    SNAPFIRE = 370783,
    TYRANNY = 376888,
    ANIMOSITY = 375797,
    CHARGED_BLAST = 370455,
    ENGULFING_BLAZE = 370837,
    RUBY_EMBERS = 365937,
    VOLATILITY = 369089,

    // Preservation abilities
    EMERALD_BLOSSOM = 355913,
    VERDANT_EMBRACE = 360995,
    DREAM_BREATH = 355936,
    SPIRIT_BLOOM = 367226,
    TEMPORAL_ANOMALY = 373861,
    RENEWING_BLAZE = 374348,
    ECHO = 364343,
    REVERSION = 366155,
    SPIRITBLOOM = 367226,
    LIFEBIND = 373267,
    TEMPORAL_COMPRESSION = 362877,
    CALL_OF_YSERA = 373834,
    FIELD_OF_DREAMS = 370062,
    DREAM_FLIGHT = 359816,
    TIME_DILATION = 357170,
    STASIS = 370537,
    TEMPORAL_MASTERY = 372677,
    GOLDEN_HOUR = 378196,

    // Augmentation abilities
    EBON_MIGHT = 395152,
    BREATH_OF_EONS = 403631,
    PRESCIENCE = 409311,
    BLISTERY_SCALES = 360827,
    SPATIAL_PARADOX = 406732,
    REACTIVE_HIDE = 409329,
    RICOCHETING_PYROBLAST = 406659,
    IGNITION_RUSH = 408083,
    ESSENCE_ATTUNEMENT = 375722,
    DRACONIC_ATTUNEMENTS = 371448,
    SYMBIOTIC_BLOOM = 410685,
    TREMBLING_EARTH = 409258,
    VOLCANIC_UPSURGE = 408092,
    MOLTEN_EMBERS = 408665,

    // Utility abilities
    RESCUE = 370665,
    TIME_SPIRAL = 374968,
    OBSIDIAN_SCALES = 363916,
    RENEWING_BLAZE_HEAL = 374349,
    CAUTERIZING_FLAME = 374251,
    EXPUNGE = 365585,
    NATURALIZE = 360823,
    SLEEP_WALK = 360806,
    QUELL = 351338,
    UNRAVEL = 368432,
    LANDSLIDE = 358385,
    OPPRESSING_ROAR = 372048,

    // Aspect abilities
    BRONZE_ASPECT = 364342,
    AZURE_ASPECT = 364343,
    GREEN_ASPECT = 364344,
    RED_ASPECT = 364345,
    BLACK_ASPECT = 364346,

    // Empowered versions
    ETERNITYS_SURGE_EMPOWERED = 382411,
    FIRE_BREATH_EMPOWERED = 382266,
    DREAM_BREATH_EMPOWERED = 382614,
    SPIRIT_BLOOM_EMPOWERED = 382731,
    BREATH_OF_EONS_EMPOWERED = 403631,

    // Mastery and proc spells
    MASTERY_GIANTKILLER = 362980,
    MASTERY_LIFEBINDER = 363510,
    MASTERY_TIMEWALKER = 406732,
    LEAPING_FLAMES = 369939,
    PYRE_PROC = 357212,
    CHARGED_BLAST_PROC = 370454,
    ESSENCE_BURST_PROC = 392268,
    BURNOUT_PROC = 375802,
    SNAPFIRE_PROC = 370784,
    IRIDESCENCE_BLUE = 386399,
    IRIDESCENCE_RED = 386353
};

// Empowerment levels for charged abilities
enum class EmpowermentLevel : uint8
{
    NONE = 0,
    RANK_1 = 1,
    RANK_2 = 2,
    RANK_3 = 3,
    RANK_4 = 4
};

// Aspect forms for Evoker
enum class EvokerAspect : uint8
{
    NONE = 0,
    BRONZE = 1,
    AZURE = 2,
    GREEN = 3,
    RED = 4,
    BLACK = 5
};

enum class EssenceState : uint8
{
    CRITICAL = 0,  // < 2
    LOW = 1,       // 2-3
    MEDIUM = 2,    // 4-5
    HIGH = 3,      // 6
    FULL = 4       // 6 (max)
};

enum class CombatPhase : uint8
{
    OPENER = 0,
    BURST_PHASE = 1,
    SUSTAIN_PHASE = 2,
    EXECUTE_PHASE = 3,
    AOE_PHASE = 4,
    EMERGENCY = 5,
    EMPOWERMENT_WINDOW = 6,
    RESOURCE_REGENERATION = 7
};

enum class EmpowermentPriority : uint8
{
    EMERGENCY_HEAL = 0,
    HIGH_DAMAGE = 1,
    AOE_DAMAGE = 2,
    AOE_HEAL = 3,
    SUSTAIN_DAMAGE = 4,
    SUSTAIN_HEAL = 5,
    RESOURCE_GENERATION = 6
};

// Essence tracking system
struct EssenceInfo
{
    uint32 current;
    uint32 maximum;
    uint32 generation;
    uint32 lastGenerated;
    uint32 generationRate;
    EssenceState state;
    bool isRegenerating;

    EssenceInfo() : current(3), maximum(6), generation(0), lastGenerated(0),
                    generationRate(1500), state(EssenceState::MEDIUM), isRegenerating(true) {}

    bool HasEssence(uint32 required = 1) const { return current >= required; }
    void SpendEssence(uint32 amount) { current = current >= amount ? current - amount : 0; UpdateState(); }
    void GenerateEssence(uint32 amount) { current = std::min(current + amount, maximum); generation += amount; UpdateState(); }

    void UpdateState()
    {
        if (current >= 6) state = EssenceState::FULL;
        else if (current >= 4) state = EssenceState::HIGH;
        else if (current >= 2) state = EssenceState::MEDIUM;
        else if (current >= 1) state = EssenceState::LOW;
        else state = EssenceState::CRITICAL;
    }
};

// Empowered spell tracking
struct EmpoweredSpell
{
    uint32 spellId;
    EmpowermentLevel currentLevel;
    EmpowermentLevel targetLevel;
    uint32 channelStart;
    uint32 channelDuration;
    bool isChanneling;
    ::Unit* target;
    float efficiency;

    EmpoweredSpell() : spellId(0), currentLevel(EmpowermentLevel::NONE), targetLevel(EmpowermentLevel::NONE),
                       channelStart(0), channelDuration(0), isChanneling(false), target(nullptr), efficiency(0.0f) {}

    EmpoweredSpell(uint32 spell, EmpowermentLevel level, ::Unit* tgt)
        : spellId(spell), currentLevel(EmpowermentLevel::NONE), targetLevel(level),
          channelStart(getMSTime()), channelDuration(0), isChanneling(true), target(tgt), efficiency(0.0f) {}

    uint32 GetChannelTime() const { return getMSTime() - channelStart; }
    bool ShouldRelease() const { return GetChannelTime() >= GetRequiredChannelTime(); }
    uint32 GetRequiredChannelTime() const { return static_cast<uint32>(targetLevel) * 1000; } // 1 sec per rank
};

// Echo tracking for healing
struct Echo
{
    ::Unit* target;
    uint32 remainingHeals;
    uint32 healAmount;
    uint32 lastHeal;
    uint32 healInterval;
    uint32 creationTime;
    bool isActive;

    Echo() : target(nullptr), remainingHeals(0), healAmount(0), lastHeal(0),
             healInterval(2000), creationTime(0), isActive(false) {}

    Echo(::Unit* tgt, uint32 heals, uint32 amount)
        : target(tgt), remainingHeals(heals), healAmount(amount), lastHeal(getMSTime()),
          healInterval(2000), creationTime(getMSTime()), isActive(true) {}

    bool ShouldHeal() const { return getMSTime() - lastHeal >= healInterval && remainingHeals > 0; }
    void ProcessHeal() { if (remainingHeals > 0) { remainingHeals--; lastHeal = getMSTime(); } }
    bool IsExpired() const { return remainingHeals == 0 || (getMSTime() - creationTime > 30000); }
};

struct CooldownInfo
{
    uint32 spellId;
    uint32 cooldownMs;
    std::chrono::steady_clock::time_point lastUsed;
    bool isReady;

    CooldownInfo() : spellId(0), cooldownMs(0), isReady(true) {}
    CooldownInfo(uint32 spell, uint32 cd) : spellId(spell), cooldownMs(cd), isReady(true) {}
};

struct AspectInfo
{
    EvokerAspect current;
    uint32 duration;
    uint32 lastShift;
    uint32 cooldown;
    bool canShift;

    AspectInfo() : current(EvokerAspect::NONE), duration(0), lastShift(0), cooldown(1500), canShift(true) {}
};

struct TargetInfo
{
    ::Unit* unit;
    float healthPercent;
    float distance;
    bool isInRange;
    bool needsHealing;
    bool needsBuffing;
    uint32 priority;

    TargetInfo() : unit(nullptr), healthPercent(100.0f), distance(0.0f), isInRange(false),
                   needsHealing(false), needsBuffing(false), priority(0) {}
};

class EvokerSpecialization
{
public:
    explicit EvokerSpecialization(Player* bot);
    virtual ~EvokerSpecialization() = default;

    // Core Interface
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual void UpdateCooldowns(uint32 diff) = 0;
    virtual bool CanUseAbility(uint32 spellId) = 0;
    virtual void OnCombatStart(::Unit* target) = 0;
    virtual void OnCombatEnd() = 0;

    // Resource Management
    virtual bool HasEnoughResource(uint32 spellId) = 0;
    virtual void ConsumeResource(uint32 spellId) = 0;

    // Positioning
    virtual Position GetOptimalPosition(::Unit* target) = 0;
    virtual float GetOptimalRange(::Unit* target) = 0;

    // Essence Management
    virtual void UpdateEssenceManagement() = 0;
    virtual bool HasEssence(uint32 required = 1) = 0;
    virtual uint32 GetEssence() = 0;
    virtual void SpendEssence(uint32 amount) = 0;
    virtual void GenerateEssence(uint32 amount) = 0;
    virtual bool ShouldConserveEssence() = 0;

    // Empowerment Management
    virtual void UpdateEmpowermentSystem() = 0;
    virtual void StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target) = 0;
    virtual void UpdateEmpoweredChanneling() = 0;
    virtual void ReleaseEmpoweredSpell() = 0;
    virtual EmpowermentLevel CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target) = 0;
    virtual bool ShouldEmpowerSpell(uint32 spellId) = 0;

    // Aspect Management
    virtual void UpdateAspectManagement() = 0;
    virtual void ShiftToAspect(EvokerAspect aspect) = 0;
    virtual EvokerAspect GetOptimalAspect() = 0;
    virtual bool CanShiftAspect() = 0;

    // Combat Phase Management
    virtual void UpdateCombatPhase() = 0;
    virtual CombatPhase GetCurrentPhase() = 0;
    virtual bool ShouldExecuteBurstRotation() = 0;

    // Target Selection
    virtual ::Unit* GetBestTarget() = 0;
    virtual std::vector<::Unit*> GetEmpoweredSpellTargets(uint32 spellId) = 0;

    // Utility Functions
    virtual bool CastSpell(uint32 spellId, ::Unit* target = nullptr) = 0;
    virtual bool HasSpell(uint32 spellId) = 0;
    virtual SpellInfo const* GetSpellInfo(uint32 spellId) = 0;
    virtual uint32 GetSpellCooldown(uint32 spellId) = 0;

protected:
    Player* _bot;
    std::unordered_map<uint32, CooldownInfo> _cooldowns;
    EssenceInfo _essence;
    AspectInfo _aspect;
    EmpoweredSpell _currentEmpoweredSpell;
    CombatPhase _combatPhase;
    ::Unit* _currentTarget;

    // Core State Tracking
    uint32 _lastUpdateTime;
    uint32 _combatStartTime;
    uint32 _lastEssenceCheck;
    uint32 _lastAspectCheck;
    uint32 _lastEmpowermentCheck;

    // Combat Metrics
    uint32 _totalDamageDealt;
    uint32 _totalHealingDone;
    uint32 _totalEssenceSpent;
    uint32 _totalEmpoweredSpells;
    uint32 _burstPhaseCount;
    float _averageCombatTime;

    // Helper Methods
    void InitializeCooldowns();
    void UpdateResourceStates();
    void UpdateTargetInfo(::Unit* target);
    void LogRotationDecision(const std::string& decision, const std::string& reason);
    bool IsInRange(::Unit* target, float range);
    bool IsInMeleeRange(::Unit* target);
    bool HasAura(uint32 spellId, ::Unit* unit = nullptr);
    uint32 GetAuraTimeRemaining(uint32 spellId, ::Unit* unit = nullptr);
    uint8 GetAuraStacks(uint32 spellId, ::Unit* unit = nullptr);

    // Empowerment helpers
    uint32 GetEmpowermentChannelTime(EmpowermentLevel level);
    float CalculateEmpowermentEfficiency(uint32 spellId, EmpowermentLevel level, ::Unit* target);
    bool IsChannelingEmpoweredSpell();

    // Essence helpers
    uint32 GetEssenceCost(uint32 spellId);
    uint32 GetEssenceGenerated(uint32 spellId);

    // Target evaluation
    float CalculateTargetPriority(::Unit* target);
    bool IsValidTarget(::Unit* target);
    std::vector<::Unit*> GetNearbyAllies(float range = 30.0f);
    std::vector<::Unit*> GetNearbyEnemies(float range = 30.0f);

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_CASTING_RANGE = 25.0f;
    static constexpr float EMPOWERED_SPELL_RANGE = 30.0f;
    static constexpr uint32 ESSENCE_MAX = 6;
    static constexpr uint32 ESSENCE_GENERATION_RATE = 1500; // 1.5 seconds per essence
    static constexpr uint32 EMPOWERMENT_MAX_LEVEL = 4;
    static constexpr float ESSENCE_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr uint32 ASPECT_SHIFT_COOLDOWN = 1500; // 1.5 seconds
    static constexpr uint32 EMERGENCY_HEALTH_THRESHOLD = 30; // 30%
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 0.35f; // 35%
};

} // namespace Playerbot

#endif // EVOKER_SPECIALIZATION_H