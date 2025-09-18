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

// Demon Hunter specializations
enum class DemonHunterSpec : uint8
{
    HAVOC = 0,
    VENGEANCE = 1
};

// Demon Hunter resource types
enum class DemonHunterResource : uint8
{
    FURY = 0,
    PAIN = 1
};

// Metamorphosis states
enum class MetamorphosisState : uint8
{
    NONE = 0,
    HAVOC_META = 1,
    VENGEANCE_META = 2,
    TRANSITIONING = 3
};

// Soul fragment tracking
struct SoulFragment
{
    Position position;
    uint32 spawnTime;
    uint32 lifetime;
    bool isGreater;
    ::Unit* source;

    SoulFragment() : position(), spawnTime(0), lifetime(10000), isGreater(false), source(nullptr) {}

    SoulFragment(const Position& pos, ::Unit* src, bool greater = false)
        : position(pos), spawnTime(getMSTime()), lifetime(10000), isGreater(greater), source(src) {}

    bool IsExpired() const { return getMSTime() - spawnTime >= lifetime; }
    bool IsInRange(const Position& playerPos, float range = 20.0f) const
    {
        return position.GetExactDist2d(playerPos) <= range;
    }
};

// Fury resource tracking for Havoc
struct FuryInfo
{
    uint32 current;
    uint32 maximum;
    uint32 generation;
    uint32 lastGenerated;
    float generationRate;

    FuryInfo() : current(0), maximum(100), generation(0), lastGenerated(0), generationRate(1.0f) {}

    bool HasFury(uint32 required = 1) const { return current >= required; }
    void SpendFury(uint32 amount) { current = current >= amount ? current - amount : 0; }
    void GenerateFury(uint32 amount) { current = std::min(current + amount, maximum); generation += amount; }
};

// Pain resource tracking for Vengeance
struct PainInfo
{
    uint32 current;
    uint32 maximum;
    uint32 lastDecay;
    uint32 decayRate;
    uint32 generation;

    PainInfo() : current(0), maximum(100), lastDecay(0), decayRate(2000), generation(0) {}

    bool HasPain(uint32 required = 1) const { return current >= required; }
    void SpendPain(uint32 amount) { current = current >= amount ? current - amount : 0; }
    void GeneratePain(uint32 amount) { current = std::min(current + amount, maximum); generation += amount; }
    void DecayPain(uint32 amount) { current = current >= amount ? current - amount : 0; }
};

// Demon Hunter AI implementation with full fury/pain management and metamorphosis
class TC_GAME_API DemonHunterAI : public ClassAI
{
public:
    explicit DemonHunterAI(Player* bot);
    ~DemonHunterAI() = default;

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
    // Demon Hunter-specific data
    DemonHunterSpec _specialization;
    MetamorphosisState _metamorphosisState;
    uint32 _damageDealt;
    uint32 _damageMitigated;
    uint32 _furyGenerated;
    uint32 _painGenerated;
    uint32 _soulFragmentsConsumed;

    // Resource management
    FuryInfo _fury;
    PainInfo _pain;
    uint32 _lastResourceUpdate;
    uint32 _resourceUpdateInterval;

    // Metamorphosis system
    uint32 _metamorphosisRemaining;
    uint32 _lastMetamorphosis;
    uint32 _metamorphosisCooldown;
    bool _inMetamorphosis;
    bool _canMetamorphosis;

    // Soul fragment system
    std::vector<SoulFragment> _soulFragments;
    uint32 _lastSoulFragmentScan;
    uint32 _soulFragmentScanInterval;
    uint32 _soulFragmentsAvailable;
    Position _lastSoulCleaverPosition;

    // Havoc specialization tracking
    uint32 _chaosStrikeCharges;
    uint32 _eyeBeamReady;
    uint32 _bladeGuardStacks;
    uint32 _demonicStacks;
    uint32 _lastChaosStrike;
    uint32 _lastEyeBeam;
    bool _demonicFormActive;

    // Vengeance specialization tracking
    uint32 _soulCleaverCharges;
    uint32 _demonSpikesStacks;
    uint32 _immolationAuraRemaining;
    uint32 _sigilOfFlameCharges;
    uint32 _fieryBrandRemaining;
    uint32 _lastSoulCleaver;
    uint32 _lastDemonSpikes;

    // Mobility and utility tracking
    uint32 _felRushCharges;
    uint32 _vengefulRetreatReady;
    uint32 _glideRemaining;
    uint32 _lastFelRush;
    uint32 _lastVengefulRetreat;
    uint32 _doubleJumpReady;
    bool _isGliding;

    // Defensive tracking
    uint32 _blurReady;
    uint32 _netherwalkReady;
    uint32 _darknessReady;
    uint32 _lastBlur;
    uint32 _lastNetherwalk;
    uint32 _lastDarkness;

    // Crowd control tracking
    uint32 _imprisonReady;
    uint32 _chaosNovaReady;
    uint32 _disruptReady;
    uint32 _lastImprison;
    uint32 _lastChaosNova;
    uint32 _lastDisrupt;

    // Rotation methods by specialization
    void UpdateHavocRotation(::Unit* target);
    void UpdateVengeanceRotation(::Unit* target);

    // Resource management systems
    void UpdateFuryManagement();
    void UpdatePainManagement();
    void GenerateFury(uint32 amount);
    void SpendFury(uint32 amount);
    void GeneratePain(uint32 amount);
    void SpendPain(uint32 amount);
    void OptimizeResourceUsage();

    // Fury management (Havoc)
    bool HasFury(uint32 required);
    uint32 GetFury();
    uint32 GetMaxFury();
    float GetFuryPercent();
    void RegenerateFury();

    // Pain management (Vengeance)
    bool HasPain(uint32 required);
    uint32 GetPain();
    uint32 GetMaxPain();
    float GetPainPercent();
    void DecayPain();

    // Metamorphosis system
    void UpdateMetamorphosisSystem();
    void EnterMetamorphosis();
    void ExitMetamorphosis();
    bool CanUseMetamorphosis();
    bool ShouldUseMetamorphosis();
    void ManageMetamorphosisDuration();
    void HandleMetamorphosisAbilities(::Unit* target);

    // Soul fragment system
    void UpdateSoulFragmentSystem();
    void ScanForSoulFragments();
    void ConsumeSoulFragments();
    void GenerateSoulFragment(const Position& pos, ::Unit* source, bool greater = false);
    void RemoveExpiredSoulFragments();
    uint32 GetAvailableSoulFragments();
    SoulFragment* GetNearestSoulFragment();
    bool ShouldConsumeSoulFragments();

    // Havoc abilities
    void UseHavocAbilities(::Unit* target);
    void CastDemonsBite(::Unit* target);
    void CastChaosStrike(::Unit* target);
    void CastBladeDance(::Unit* target);
    void CastEyeBeam(::Unit* target);
    void CastMetamorphosisHavoc();
    void ManageHavocResources();

    // Vengeance abilities
    void UseVengeanceAbilities(::Unit* target);
    void CastShear(::Unit* target);
    void CastSoulCleave(::Unit* target);
    void CastImmolationAura();
    void CastDemonSpikes();
    void CastSigilOfFlame(::Unit* target);
    void CastFieryBrand(::Unit* target);
    void CastMetamorphosisVengeance();
    void ManageVengeanceResources();

    // Mobility abilities
    void UseMobilityAbilities();
    void CastFelRush();
    void CastVengefulRetreat();
    void CastGlide();
    void CastDoubleJump();
    void ManageMobility();
    void OptimizePositioning(::Unit* target);

    // Defensive abilities
    void UseDefensiveAbilities();
    void CastBlur();
    void CastNetherwalk();
    void CastDarkness();
    void HandleEmergencyDefensives();
    bool IsInDanger();

    // Crowd control abilities
    void UseCrowdControlAbilities();
    void CastImprison(::Unit* target);
    void CastChaosNova();
    void CastDisrupt(::Unit* target);
    ::Unit* GetBestCrowdControlTarget();
    ::Unit* GetBestInterruptTarget();

    // Positioning and movement
    void UpdateDemonHunterPositioning();
    bool IsAtOptimalRange(::Unit* target);
    void MaintainMeleeRange(::Unit* target);
    bool NeedsRepositioning();
    void ExecuteMobilityRotation(::Unit* target);

    // Target selection
    ::Unit* GetBestAttackTarget();
    ::Unit* GetHighestThreatTarget();
    ::Unit* GetBestAoETarget();
    std::vector<::Unit*> GetAoETargets();
    ::Unit* GetBestDefensiveTarget();

    // Threat and aggro management
    void ManageThreat();
    void BuildThreat(::Unit* target);
    void ReduceThreat();
    bool HasTooMuchThreat();

    // Utility and support
    void UseUtilityAbilities();
    void OptimizeGlobalCooldowns();
    void ManageAbilityCooldowns();

    // Performance optimization
    void RecordDamageDealt(uint32 damage, ::Unit* target);
    void RecordDamageMitigated(uint32 amount);
    void AnalyzeResourceEfficiency();
    void OptimizeRotationPerformance();

    // Helper methods
    bool IsChanneling();
    bool IsGlobalCooldownActive();
    DemonHunterSpec DetectSpecialization();
    bool HasTalent(uint32 talentId);
    bool IsInMeleeRange(::Unit* target);
    float GetDistanceToTarget(::Unit* target);

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_RANGE = 5.0f;
    static constexpr uint32 FURY_MAX = 100;
    static constexpr uint32 PAIN_MAX = 100;
    static constexpr uint32 FURY_GENERATION_RATE = 20; // per second
    static constexpr uint32 PAIN_DECAY_RATE = 2; // per second
    static constexpr uint32 METAMORPHOSIS_DURATION = 30000; // 30 seconds
    static constexpr uint32 SOUL_FRAGMENT_LIFETIME = 10000; // 10 seconds
    static constexpr uint32 SOUL_FRAGMENT_RANGE = 20.0f;
    static constexpr uint32 RESOURCE_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 SOUL_FRAGMENT_SCAN_INTERVAL = 500; // 0.5 seconds
    static constexpr float FURY_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr float PAIN_GENERATION_THRESHOLD = 0.7f; // 70%

    // Spell IDs (version-specific)
    enum DemonHunterSpells
    {
        // Havoc abilities
        DEMONS_BITE = 162243,
        CHAOS_STRIKE = 162794,
        BLADE_DANCE = 188499,
        EYE_BEAM = 198013,
        METAMORPHOSIS_HAVOC = 191427,
        DEATH_SWEEP = 210152,
        ANNIHILATION = 201427,

        // Vengeance abilities
        SHEAR = 203782,
        SOUL_CLEAVE = 228477,
        IMMOLATION_AURA = 178740,
        DEMON_SPIKES = 203720,
        SIGIL_OF_FLAME = 204596,
        FIERY_BRAND = 204021,
        METAMORPHOSIS_VENGEANCE = 187827,

        // Mobility abilities
        FEL_RUSH = 195072,
        VENGEFUL_RETREAT = 198793,
        GLIDE = 131347,
        DOUBLE_JUMP = 196055,

        // Defensive abilities
        BLUR = 198589,
        NETHERWALK = 196555,
        DARKNESS = 196718,

        // Crowd control
        IMPRISON = 217832,
        CHAOS_NOVA = 179057,
        DISRUPT = 183752,

        // Utility
        SPECTRAL_SIGHT = 188501,
        CONSUME_MAGIC = 278326,
        TORMENT = 185245,

        // Passive abilities
        SOUL_FRAGMENTS = 203981,
        DEMONIC_WARDS = 203513,
        THICK_SKIN = 203953
    };
};

// Utility class for demon hunter calculations
class TC_GAME_API DemonHunterCalculator
{
public:
    // Damage calculations
    static uint32 CalculateDemonsBiteDamage(Player* caster, ::Unit* target);
    static uint32 CalculateChaosStrikeDamage(Player* caster, ::Unit* target);
    static uint32 CalculateEyeBeamDamage(Player* caster, ::Unit* target);

    // Defensive calculations
    static uint32 CalculateSoulCleaveDamage(Player* caster, ::Unit* target);
    static float CalculateDamageReduction(Player* caster);
    static uint32 CalculateSoulFragmentHealing(Player* caster);

    // Resource calculations
    static uint32 CalculateFuryGeneration(uint32 spellId, Player* caster);
    static uint32 CalculatePainGeneration(uint32 spellId, Player* caster, uint32 damageTaken);
    static float CalculateResourceEfficiency(uint32 spellId, Player* caster);

    // Metamorphosis calculations
    static uint32 CalculateMetamorphosisDuration(Player* caster);
    static float CalculateMetamorphosisDamageBonus(Player* caster);
    static bool ShouldUseMetamorphosis(Player* caster, const std::vector<::Unit*>& enemies);

    // Soul fragment optimization
    static Position GetOptimalSoulFragmentPosition(Player* caster, const std::vector<::Unit*>& enemies);
    static uint32 CalculateSoulFragmentValue(const SoulFragment& fragment, Player* caster);
    static bool ShouldConsumeSoulFragments(Player* caster, uint32 availableFragments);

private:
    // Cache for demon hunter calculations
    static std::unordered_map<uint32, uint32> _damageCache;
    static std::unordered_map<uint32, uint32> _resourceCache;
    static std::mutex _cacheMutex;

    static void CacheDemonHunterData();
};

// Resource manager for Demon Hunter fury/pain systems
class TC_GAME_API DemonHunterResourceManager
{
public:
    DemonHunterResourceManager(DemonHunterAI* owner);
    ~DemonHunterResourceManager() = default;

    // Resource management
    void Update(uint32 diff);
    void GenerateFury(uint32 amount);
    void SpendFury(uint32 amount);
    void GeneratePain(uint32 amount);
    void SpendPain(uint32 amount);

    // Resource state
    uint32 GetFury() const;
    uint32 GetPain() const;
    bool HasFury(uint32 required) const;
    bool HasPain(uint32 required) const;
    float GetFuryPercent() const;
    float GetPainPercent() const;

    // Resource optimization
    void OptimizeResourceUsage();
    bool ShouldConserveFury() const;
    bool ShouldGeneratePain() const;

private:
    DemonHunterAI* _owner;
    FuryInfo _fury;
    PainInfo _pain;
    uint32 _lastUpdate;

    void UpdateFuryRegeneration();
    void UpdatePainDecay();
    void CalculateOptimalResourceLevels();
};

// Metamorphosis controller for form management
class TC_GAME_API MetamorphosisController
{
public:
    MetamorphosisController(DemonHunterAI* owner);
    ~MetamorphosisController() = default;

    // Metamorphosis management
    void Update(uint32 diff);
    void ActivateMetamorphosis();
    void DeactivateMetamorphosis();
    bool CanUseMetamorphosis() const;
    bool ShouldUseMetamorphosis() const;

    // Metamorphosis state
    bool IsInMetamorphosis() const;
    uint32 GetRemainingTime() const;
    MetamorphosisState GetState() const;

    // Optimization
    void OptimizeMetamorphosisTiming();
    bool IsOptimalTimingForMetamorphosis() const;

private:
    DemonHunterAI* _owner;
    MetamorphosisState _state;
    uint32 _remainingTime;
    uint32 _cooldownRemaining;
    uint32 _lastActivation;

    void UpdateMetamorphosisEffects();
    void CalculateOptimalUsage();
    bool HasSufficientTargets() const;
};

// Soul fragment manager for positioning and consumption
class TC_GAME_API SoulFragmentManager
{
public:
    SoulFragmentManager(DemonHunterAI* owner);
    ~SoulFragmentManager() = default;

    // Fragment management
    void Update(uint32 diff);
    void AddSoulFragment(const Position& pos, ::Unit* source, bool greater = false);
    void ConsumeSoulFragments();
    void RemoveExpiredFragments();

    // Fragment state
    uint32 GetAvailableFragments() const;
    SoulFragment* GetNearestFragment();
    bool HasFragmentsInRange() const;

    // Optimization
    void OptimizeFragmentConsumption();
    bool ShouldConsumeFragments() const;
    Position GetOptimalConsumptionPosition() const;

private:
    DemonHunterAI* _owner;
    std::vector<SoulFragment> _fragments;
    uint32 _lastScan;
    uint32 _scanInterval;

    void ScanForNewFragments();
    void UpdateFragmentStates();
    uint32 CalculateFragmentValue(const SoulFragment& fragment) const;
};

} // namespace Playerbot