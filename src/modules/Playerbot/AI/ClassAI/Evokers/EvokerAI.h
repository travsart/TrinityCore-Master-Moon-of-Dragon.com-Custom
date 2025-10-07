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
#include <unordered_map>
#include <queue>
#include <vector>
#include <array>

namespace Playerbot
{

// Evoker specializations
enum class EvokerSpec : uint8
{
    DEVASTATION = 0,
    PRESERVATION = 1,
    AUGMENTATION = 2
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

// Essence tracking system
struct EssenceInfo
{
    uint32 current;
    uint32 maximum;
    uint32 generation;
    uint32 lastGenerated;
    uint32 generationRate;
    bool isRegenerating;

    EssenceInfo() : current(0), maximum(6), generation(0), lastGenerated(0),
                    generationRate(1500), isRegenerating(true) {}

    bool HasEssence(uint32 required = 1) const { return current >= required; }
    void SpendEssence(uint32 amount) { current = current >= amount ? current - amount : 0; }
    void GenerateEssence(uint32 amount) { current = std::min(current + amount, maximum); generation += amount; }
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

    EmpoweredSpell() : spellId(0), currentLevel(EmpowermentLevel::NONE), targetLevel(EmpowermentLevel::NONE),
                       channelStart(0), channelDuration(0), isChanneling(false), target(nullptr) {}

    EmpoweredSpell(uint32 spell, EmpowermentLevel level, ::Unit* tgt)
        : spellId(spell), currentLevel(EmpowermentLevel::NONE), targetLevel(level),
          channelStart(getMSTime()), channelDuration(0), isChanneling(true), target(tgt) {}

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

    Echo() : target(nullptr), remainingHeals(0), healAmount(0), lastHeal(0), healInterval(2000) {}

    Echo(::Unit* tgt, uint32 heals, uint32 amount)
        : target(tgt), remainingHeals(heals), healAmount(amount), lastHeal(getMSTime()), healInterval(2000) {}

    bool ShouldHeal() const { return getMSTime() - lastHeal >= healInterval && remainingHeals > 0; }
    void ProcessHeal() { if (remainingHeals > 0) { remainingHeals--; lastHeal = getMSTime(); } }
};

// Evoker AI implementation with full essence and empowerment management
class TC_GAME_API EvokerAI : public ClassAI
{
public:
    explicit EvokerAI(Player* bot);
    ~EvokerAI() = default;

    // Spell IDs (version-specific)
    enum EvokerSpells
    {
        // Basic abilities
        AZURE_STRIKE = 362969,
        LIVING_FLAME = 361469,
        HOVER = 358267,
        SOAR = 369536,

        // Devastation abilities
        ETERNITYS_SURGE = 359073,
        DISINTEGRATE = 356995,
        PYRE = 357211,
        DEEP_BREATH = 357210,
        FIRE_BREATH = 357208,
        AZURE_STRIKE_DEVASTATION = 362969,

        // Preservation abilities
        DREAM_BREATH = 355936,
        SPIRIT_BLOOM = 367226,
        SPIRITBLOOM = 367226, // Alias for compatibility
        EMERALD_BLOSSOM = 355916,
        VERDANT_EMBRACE = 360995,
        LIFEBIND = 373267,
        EMERALD_COMMUNION = 370960,
        TEMPORAL_ANOMALY = 373861,

        // Augmentation abilities
        EBON_MIGHT = 395152,
        BREATH_OF_EONS = 403631,
        PRESCIENCE = 409311,
        BLISTERING_SCALES = 360827,

        // Utility abilities
        BLESSING_OF_THE_BRONZE = 364342,
        LANDSLIDE = 358385,
        TAIL_SWIPE = 368970,
        WING_BUFFET = 357214,
        SLEEP_WALK = 360806,
        SPELL_QUELL = 351338,       // Interrupt
        SPELL_DRAGONRAGE = 375087,   // Devastation major cooldown

        // Defensive abilities
        OBSIDIAN_SCALES = 363916,
        RENEWING_BLAZE = 374348,
        RESCUE = 370665,

        // Movement abilities
        DEEP_BREATH_MOVEMENT = 357210,
        SOAR_MOVEMENT = 369536,

        // Additional constants
        ECHO = 364343,
        BRONZE_ASPECT = 364344,
        AZURE_ASPECT = 364345,
        GREEN_ASPECT = 364346,
        RED_ASPECT = 364347,
        BLACK_ASPECT = 364348
    };

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
    // Evoker-specific data
    EvokerSpec _specialization;
    EvokerAspect _currentAspect;
    uint32 _damageDealt;
    uint32 _healingDone;
    uint32 _essenceGenerated;
    uint32 _empoweredSpellsCast;
    uint32 _echoHealsPerformed;

    // Essence management system
    EssenceInfo _essence;
    uint32 _lastEssenceUpdate;
    uint32 _essenceUpdateInterval;

    // Empowerment system
    EmpoweredSpell _currentEmpoweredSpell;
    std::unordered_map<uint32, EmpowermentLevel> _optimalEmpowermentLevels;
    uint32 _lastEmpoweredSpell;
    bool _isChannelingEmpowered;

    // Echo system (Preservation)
    std::vector<Echo> _activeEchoes;
    uint32 _lastEchoUpdate;
    uint32 _echoUpdateInterval;
    uint32 _maxEchoes;

    // Devastation tracking
    uint32 _burnoutStacks;
    uint32 _essenceBurstStacks;
    uint32 _dragonrageStacks;
    uint32 _lastEternity;
    uint32 _lastDisintegrate;
    bool _eternitysSurgeReady;

    // Preservation tracking
    uint32 _temporalCompressionStacks;
    uint32 _callOfYseraStacks;
    uint32 _lastVerdantEmbrace;
    uint32 _lastTemporalAnomaly;
    std::unordered_map<ObjectGuid, uint32> _rendezvousTargets;

    // Augmentation tracking
    uint32 _prescientStacks;
    uint32 _blisteryScalesStacks;
    uint32 _lastEbon;
    uint32 _lastBreathOfEons;
    std::unordered_map<ObjectGuid, uint32> _augmentationBuffs;

    // Aspect management
    uint32 _aspectDuration;
    uint32 _lastAspectShift;
    uint32 _aspectCooldown;
    bool _canShiftAspect;

    // Utility tracking
    uint32 _lastSoar;
    uint32 _lastHover;
    uint32 _lastRescue;
    uint32 _lastTimeSpiral;
    uint32 _hoverRemaining;
    bool _isHovering;

    // Rotation methods by specialization
    void UpdateDevastationRotation(::Unit* target);
    void UpdatePreservationRotation(::Unit* target);
    void UpdateAugmentationRotation(::Unit* target);

    // Essence management system
    void UpdateEssenceManagement();
    void GenerateEssence(uint32 amount);
    void SpendEssence(uint32 amount);
    bool HasEssence(uint32 required = 1);
    uint32 GetEssence();
    uint32 GetMaxEssence();
    float GetEssencePercent();
    void OptimizeEssenceUsage();

    // Empowerment system
    void UpdateEmpowermentSystem();
    void StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target);
    void UpdateEmpoweredChanneling();
    void ReleaseEmpoweredSpell();
    EmpowermentLevel CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target);
    bool ShouldEmpowerSpell(uint32 spellId);
    bool IsEmpoweredSpell(uint32 spellId);
    uint32 GetEmpowermentChannelTime(EmpowermentLevel level);

    // Echo management system (Preservation)
    void UpdateEchoSystem();
    void CreateEcho(::Unit* target, uint32 healAmount, uint32 numHeals = 3);
    void ProcessEchoHealing();
    void RemoveExpiredEchoes();
    uint32 GetActiveEchoCount();
    bool ShouldCreateEcho(::Unit* target);

    // Aspect management
    void UpdateAspectManagement();
    void ShiftToAspect(EvokerAspect aspect);
    EvokerAspect GetOptimalAspect();
    bool CanShiftAspect();
    uint32 GetAspectShiftCooldown();

    // Devastation abilities
    void UseDevastationAbilities(::Unit* target);
    void CastAzureStrike(::Unit* target);
    void CastLivingFlame(::Unit* target);
    void CastEternitysSurge(::Unit* target);
    void CastDisintegrate(::Unit* target);
    void CastPyre(::Unit* target);
    void CastFireBreath(::Unit* target);
    void ManageDevastationResources();

    // Preservation abilities
    void UsePreservationAbilities();
    void CastEmeraldBlossom();
    void CastVerdantEmbrace(::Unit* target);
    void CastDreamBreath(::Unit* target);
    void CastSpiritBloom(::Unit* target);
    void CastTemporalAnomaly();
    void CastRenewingBlaze(::Unit* target);
    void ManagePreservationHealing();

    // Augmentation abilities
    void UseAugmentationAbilities(::Unit* target);
    void CastEbonMight(::Unit* target);
    void CastBreathOfEons(::Unit* target);
    void CastPrescience(::Unit* target);
    void CastBlisteryScales(::Unit* target);
    void ManageAugmentationBuffs();

    // Empowered ability casts
    void CastEmpoweredEternitysSurge(::Unit* target, EmpowermentLevel level);
    void CastEmpoweredFireBreath(::Unit* target, EmpowermentLevel level);
    void CastEmpoweredDreamBreath(::Unit* target, EmpowermentLevel level);
    void CastEmpoweredSpiritBloom(::Unit* target, EmpowermentLevel level);

    // Utility abilities
    void UseUtilityAbilities();
    void CastSoar();
    void CastHover();
    void CastRescue(::Unit* target);
    void CastTimeSpiral();
    void CastWingBuffet();
    void ManageMovementAbilities();

    // Defensive abilities
    void UseDefensiveAbilities();
    void CastObsidianScales();
    void CastRenewingBlaze();
    void UseEmergencyHealing();

    // Positioning and movement
    void UpdateEvokerPositioning();
    bool IsAtOptimalRange(::Unit* target);
    void MaintainCastingRange(::Unit* target);
    bool ShouldUseHover();
    void OptimizePositionForEmpoweredSpells();

    // Target selection
    ::Unit* GetBestHealTarget();
    ::Unit* GetBestEchoTarget();
    ::Unit* GetBestAugmentationTarget();
    ::Unit* GetHighestPriorityDamageTarget();
    std::vector<::Unit*> GetEmpoweredSpellTargets(uint32 spellId);

    // Buff and debuff management
    void ManageBuffs();
    void ManageDebuffs();
    void UpdateAugmentationBuffs();
    void TrackEchoTargets();

    // Resource optimization
    void OptimizeResourceUsage();
    void BalanceEssenceSpending();
    bool ShouldConserveEssence();

    // Performance tracking
    void RecordDamageDealt(uint32 damage, ::Unit* target);
    void RecordHealingDone(uint32 amount, ::Unit* target);
    void RecordEchoHealing(uint32 amount);
    void AnalyzeEmpowermentEfficiency();

    // Helper methods
    bool IsChanneling();
    bool IsChannelingEmpoweredSpell();
    EvokerSpec DetectSpecialization();
    bool HasTalent(uint32 talentId);
    bool IsInRange(::Unit* target, float range);
    uint32 GetSpellRange(uint32 spellId);

    // Constants
    static constexpr float EVOKER_MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_CASTING_RANGE = 25.0f;
    static constexpr float EMPOWERED_SPELL_RANGE = 30.0f;
    static constexpr uint32 ESSENCE_MAX = 6;
    static constexpr uint32 ESSENCE_GENERATION_RATE = 1500; // 1.5 seconds per essence
    static constexpr uint32 EMPOWERMENT_MAX_LEVEL = 4;
    static constexpr uint32 ECHO_MAX_COUNT = 8;
    static constexpr uint32 ECHO_HEAL_INTERVAL = 2000; // 2 seconds
    static constexpr float ESSENCE_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr uint32 ASPECT_SHIFT_COOLDOWN = 1500; // 1.5 seconds

    // Note: Spell IDs moved to public section to fix access errors
};

// Utility class for evoker calculations
class TC_GAME_API EvokerCalculator
{
public:
    // Damage calculations
    static uint32 CalculateAzureStrikeDamage(Player* caster, ::Unit* target);
    static uint32 CalculateLivingFlameDamage(Player* caster, ::Unit* target);
    static uint32 CalculateEmpoweredSpellDamage(uint32 spellId, EmpowermentLevel level, Player* caster, ::Unit* target);

    // Healing calculations
    static uint32 CalculateEmeraldBlossomHealing(Player* caster);
    static uint32 CalculateVerdantEmbraceHealing(Player* caster, ::Unit* target);
    static uint32 CalculateEchoHealing(Player* caster, ::Unit* target);

    // Empowerment optimization
    static EmpowermentLevel GetOptimalEmpowermentLevel(uint32 spellId, Player* caster, ::Unit* target);
    static uint32 CalculateEmpowermentChannelTime(EmpowermentLevel level);
    static float CalculateEmpowermentEfficiency(uint32 spellId, EmpowermentLevel level, Player* caster);

    // Essence calculations
    static uint32 CalculateEssenceGeneration(uint32 spellId, Player* caster);
    static float CalculateEssenceEfficiency(uint32 spellId, Player* caster);
    static bool ShouldConserveEssence(Player* caster, uint32 currentEssence);

    // Echo optimization
    static uint32 CalculateOptimalEchoTargets(Player* caster, const std::vector<::Unit*>& allies);
    static bool ShouldCreateEcho(Player* caster, ::Unit* target);
    static uint32 CalculateEchoValue(Player* caster, ::Unit* target);

    // Augmentation calculations
    static uint32 CalculateBuffEfficiency(uint32 spellId, Player* caster, ::Unit* target);
    static ::Unit* GetOptimalAugmentationTarget(Player* caster, const std::vector<::Unit*>& allies);

private:
    // Cache for evoker calculations
    static std::unordered_map<uint32, uint32> _damageCache;
    static std::unordered_map<uint32, uint32> _healingCache;
    static std::unordered_map<EmpowermentLevel, uint32> _empowermentCache;
    static std::mutex _cacheMutex;

    static void CacheEvokerData();
};

// Essence manager for resource optimization
class TC_GAME_API EssenceManager
{
public:
    EssenceManager(EvokerAI* owner);
    ~EssenceManager() = default;

    // Essence management
    void Update(uint32 diff);
    void GenerateEssence(uint32 amount);
    void SpendEssence(uint32 amount);
    bool HasEssence(uint32 required) const;
    uint32 GetEssence() const;
    float GetEssencePercent() const;

    // Optimization
    void OptimizeEssenceUsage();
    bool ShouldConserveEssence() const;
    uint32 GetOptimalEssenceLevel() const;

private:
    EvokerAI* _owner;
    EssenceInfo _essence;
    uint32 _lastUpdate;
    uint32 _updateInterval;

    void UpdateEssenceRegeneration();
    void CalculateOptimalUsage();
};

// Empowerment controller for charged spells
class TC_GAME_API EmpowermentController
{
public:
    EmpowermentController(EvokerAI* owner);
    ~EmpowermentController() = default;

    // Empowerment management
    void Update(uint32 diff);
    void StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target);
    void UpdateChanneling();
    bool ShouldReleaseSpell();
    void ReleaseSpell();

    // Empowerment state
    bool IsChanneling() const;
    EmpowermentLevel GetCurrentLevel() const;
    uint32 GetChannelTime() const;
    uint32 GetSpellId() const;

    // Optimization
    EmpowermentLevel CalculateOptimalLevel(uint32 spellId, ::Unit* target);
    bool ShouldEmpowerSpell(uint32 spellId);

private:
    EvokerAI* _owner;
    EmpoweredSpell _currentSpell;
    uint32 _lastUpdate;

    void UpdateEmpowermentLevel();
    uint32 GetRequiredChannelTime(EmpowermentLevel level);
    float CalculateEmpowermentValue(EmpowermentLevel level);
};

// Echo controller for healing management
class TC_GAME_API EchoController
{
public:
    EchoController(EvokerAI* owner);
    ~EchoController() = default;

    // Echo management
    void Update(uint32 diff);
    void CreateEcho(::Unit* target, uint32 healAmount, uint32 numHeals = 3);
    void ProcessEchoHealing();
    void RemoveExpiredEchoes();

    // Echo state
    uint32 GetActiveEchoCount() const;
    bool HasEcho(::Unit* target) const;
    bool ShouldCreateEcho(::Unit* target) const;

    // Optimization
    void OptimizeEchoPlacement();
    ::Unit* GetBestEchoTarget() const;

private:
    EvokerAI* _owner;
    std::vector<Echo> _echoes;
    uint32 _lastUpdate;
    uint32 _maxEchoes;

    void UpdateEchoStates();
    bool IsEchoExpired(const Echo& echo) const;
    uint32 CalculateEchoValue(::Unit* target) const;
};

} // namespace Playerbot