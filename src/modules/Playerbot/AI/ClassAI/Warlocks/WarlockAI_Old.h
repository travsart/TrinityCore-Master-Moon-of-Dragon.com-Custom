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
#include "WarlockSpecialization.h"
#include "Position.h"
#include <unordered_map>
#include <memory>

// Forward declarations
class AfflictionSpecialization;
class DemonologySpecialization;
class DestructionSpecialization;

namespace Playerbot
{

// Warlock AI implementation with specialization pattern
class TC_GAME_API WarlockAI : public ClassAI
{
public:
    explicit WarlockAI(Player* bot);
    ~WarlockAI() = default;

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
    WarlockSpec _currentSpec;
    std::unique_ptr<WarlockSpecialization> _specialization;

    // Performance tracking
    uint32 _manaSpent;
    uint32 _damageDealt;
    uint32 _dotDamage;
    uint32 _petDamage;
    std::unordered_map<uint32, uint32> _abilityUsage;

    // Specialization management
    void InitializeSpecialization();
    WarlockSpec DetectCurrentSpecialization();
    void SwitchSpecialization(WarlockSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared warlock utilities
    void UpdateWarlockBuffs();
    void UpdatePetCheck();
    void UpdateSoulShardCheck();
};

} // namespace Playerbot

    // Pet management system
    void UpdatePetManagement();
    void SummonOptimalPet();
    void CommandPet(uint32 action, ::Unit* target = nullptr);
    void ManagePetHealth();
    void ManagePetBehavior();
    void PositionPet();
    WarlockPet GetOptimalPetForSituation();
    bool ShouldResummonPet();

    // Pet commands and abilities
    void PetAttackTarget(::Unit* target);
    void PetMoveToPosition(const Position& pos);
    void PetUseAbility(uint32 spellId, ::Unit* target = nullptr);
    void PetFollow();
    void PetStay();
    void PetPassive();
    void PetDefensive();
    void PetAggressive();

    // DoT management system
    void UpdateDoTManagement();
    void ApplyDoTsToTarget(::Unit* target);
    void RefreshExpiringDoTs();
    void TrackDoTDamage(::Unit* target, uint32 spellId);
    bool ShouldApplyDoT(::Unit* target, uint32 spellId);
    bool IsDoTActive(::Unit* target, uint32 spellId);
    uint32 GetDoTRemainingTime(::Unit* target, uint32 spellId);
    void RemoveExpiredDoTs();

    // Multi-target DoT spreading
    void SpreadDoTs();
    void ApplyDoTsToMultipleTargets();
    ::Unit* GetBestDoTTarget();
    std::vector<::Unit*> GetDoTTargets(uint32 maxTargets = 8);

    // Soul shard management
    void UpdateSoulShardManagement();
    void CreateSoulShard(::Unit* killedTarget);
    void UseSoulShard(uint32 spellId);
    bool HasSoulShardsAvailable(uint32 required = 1);
    void ConserveSoulShards();
    void OptimizeSoulShardUsage();

    // Curse management
    void UpdateCurseManagement();
    void ApplyCurses(::Unit* target);
    void CastCurseOfElements(::Unit* target);
    void CastCurseOfShadow(::Unit* target);
    void CastCurseOfTongues(::Unit* target);
    void CastCurseOfWeakness(::Unit* target);
    void CastCurseOfAgony(::Unit* target);
    uint32 GetOptimalCurseForTarget(::Unit* target);

    // Buff management
    void UpdateWarlockBuffs();
    void CastDemonSkin();
    void CastDemonArmor();
    void CastFelArmor();
    void CastSoulLink();
    void CastAmplifyCurse();
    void UpdateArmorSpells();

    // Defensive abilities
    void UseDefensiveAbilities();
    void CastDrainLife(::Unit* target);
    void CastSiphonSoul(::Unit* target);
    void CastDeathCoil(::Unit* target);
    void CastHowlOfTerror();
    void UseFear(::Unit* target);
    void UseBanish(::Unit* target);

    // AoE and crowd control
    void UseAoEAbilities(const std::vector<::Unit*>& enemies);
    void CastRainOfFire(const std::vector<::Unit*>& enemies);
    void CastSeedOfCorruption(::Unit* target);
    void CastHellfire();
    void UseCrowdControl(::Unit* target);

    // Specialization-specific abilities
    // Affliction
    void UseAfflictionAbilities(::Unit* target);
    void CastCorruption(::Unit* target);
    void CastUnstableAffliction(::Unit* target);
    void CastDrainSoul(::Unit* target);
    void CastDarkRitual();

    // Demonology
    void UseDemonologyAbilities(::Unit* target);
    void CastDemonicEmpowerment();
    void CastMetamorphosis();
    void CastSoulBurn(::Unit* target);
    void CastSummonFelguard();
    void ManageDemonForm();

    // Destruction
    void UseDestructionAbilities(::Unit* target);
    void CastImmolate(::Unit* target);
    void CastIncinerate(::Unit* target);
    void CastConflagrate(::Unit* target);
    void CastShadowBurn(::Unit* target);
    void CastChaosLight(::Unit* target);

    // Mana and health management
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    void OptimizeManaUsage();
    void UseManaRegeneration();
    void CastLifeTap();
    void CastDrainMana(::Unit* target);

    // Positioning and movement
    void UpdateWarlockPositioning();
    bool IsAtOptimalRange(::Unit* target);
    bool NeedsToKite(::Unit* target);
    void PerformKiting(::Unit* target);
    void MaintainPetPosition();

    // Threat and aggro management
    void ManageThreat();
    bool HasTooMuchThreat();
    void ReduceThreat();
    void UseSoulshatter();

    // Target selection and priorities
    ::Unit* GetBestCurseTarget();
    ::Unit* GetBestFearTarget();
    ::Unit* GetBestBanishTarget();
    ::Unit* GetBestDrainTarget();
    ::Unit* GetHighestPriorityTarget();

    // Emergency responses
    void HandleEmergencySituation();
    bool IsInCriticalDanger();
    void UseEmergencyAbilities();
    void PetEmergencyTaunt();

    // Performance optimization
    void RecordDamageDealt(uint32 damage, ::Unit* target);
    void AnalyzeDamageEffectiveness();
    void OptimizeRotationEfficiency();

    // Helper methods
    bool IsPetAlive();
    bool IsPetInRange();
    uint32 GetPetSpellId(WarlockPet petType, uint32 abilityIndex);
    bool IsChanneling();
    bool CanCastWithoutInterruption();
    WarlockSpec DetectSpecialization();

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr float PET_FOLLOW_DISTANCE = 3.0f;
    static constexpr float PET_COMMAND_RANGE = 40.0f;
    static constexpr uint32 DOT_CHECK_INTERVAL = 2000; // 2 seconds
    static constexpr uint32 PET_HEALTH_CHECK_INTERVAL = 3000; // 3 seconds
    static constexpr uint32 SOUL_SHARD_CONSERVATION_THRESHOLD = 5;
    static constexpr uint32 MAX_DOT_TARGETS = 8;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr uint32 FEAR_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 BANISH_COOLDOWN = 30000; // 30 seconds

    // Spell IDs (version-specific)
    enum WarlockSpells
    {
        // Pet summons
        SUMMON_IMP = 688,
        SUMMON_VOIDWALKER = 697,
        SUMMON_SUCCUBUS = 712,
        SUMMON_FELHUNTER = 691,
        SUMMON_FELGUARD = 30146,
        RITUAL_OF_SUMMONING = 698,

        // DoT spells
        CORRUPTION = 172,
        CURSE_OF_AGONY = 980,
        IMMOLATE = 348,
        UNSTABLE_AFFLICTION = 30108,
        SEED_OF_CORRUPTION = 27243,

        // Direct damage
        SHADOW_BOLT = 686,
        INCINERATE = 29722,
        SEARING_PAIN = 5676,
        SOUL_FIRE = 6353,
        SHADOW_BURN = 17877,
        CONFLAGRATE = 17962,

        // Channeled spells
        DRAIN_LIFE = 689,
        DRAIN_SOUL = 1120,
        DRAIN_MANA = 5138,
        RAIN_OF_FIRE = 5740,
        HELLFIRE = 1949,

        // Curses
        CURSE_OF_ELEMENTS = 1490,
        CURSE_OF_SHADOW = 17937,
        CURSE_OF_TONGUES = 1714,
        CURSE_OF_WEAKNESS = 702,
        CURSE_OF_RECKLESSNESS = 704,

        // Crowd control
        FEAR = 5782,
        HOWL_OF_TERROR = 5484,
        BANISH = 710,
        DEATH_COIL = 6789,
        SHADOWFLAME = 5781,

        // Buffs and armor
        DEMON_SKIN = 687,
        DEMON_ARMOR = 706,
        FEL_ARMOR = 28176,
        SOUL_LINK = 19028,
        AMPLIFY_CURSE = 18288,

        // Utility
        LIFE_TAP = 1454,
        SOULSHATTER = 32835,
        CREATE_HEALTHSTONE = 6201,
        CREATE_SOULSTONE = 693,
        RITUAL_OF_SOULS = 29893,

        // Specialization abilities
        DARK_RITUAL = 7728,
        METAMORPHOSIS = 59672,
        DEMONIC_EMPOWERMENT = 47193,
        CHAOS_BOLT = 50796,

        // Pet commands
        PET_ATTACK = 7812,
        PET_FOLLOW = 7813,
        PET_STAY = 7814,
        PET_PASSIVE = 7815,
        PET_DEFENSIVE = 7816,
        PET_AGGRESSIVE = 7817
    };
};

// Utility class for warlock damage calculations
class TC_GAME_API WarlockSpellCalculator
{
public:
    // Damage calculations
    static uint32 CalculateShadowBoltDamage(Player* caster, ::Unit* target);
    static uint32 CalculateCorruptionDamage(Player* caster, ::Unit* target);
    static uint32 CalculateImmolateDoTDamage(Player* caster, ::Unit* target);

    // DoT efficiency
    static float CalculateDoTEfficiency(uint32 spellId, Player* caster, ::Unit* target);
    static bool IsDoTWorthCasting(uint32 spellId, Player* caster, ::Unit* target);
    static uint32 GetOptimalDoTDuration(uint32 spellId, ::Unit* target);

    // Pet calculations
    static uint32 CalculatePetDamage(WarlockPet petType, Player* caster);
    static WarlockPet GetOptimalPetForSpecialization(WarlockSpec spec);
    static uint32 GetPetSummonCost(WarlockPet petType);

    // Soul shard optimization
    static bool ShouldUseSoulShardForSpell(uint32 spellId, Player* caster);
    static uint32 GetSoulShardPriority(uint32 spellId);

    // Curse effectiveness
    static uint32 GetOptimalCurseForTarget(::Unit* target, Player* caster);
    static float CalculateCurseEffectiveness(uint32 curseId, ::Unit* target);

private:
    // Cache for warlock calculations
    static std::unordered_map<uint32, uint32> _dotDamageCache;
    static std::unordered_map<WarlockPet, uint32> _petDamageCache;
    static std::mutex _cacheMutex;

    static void CacheWarlockData();
};

// Pet AI controller for warlock pets
class TC_GAME_API WarlockPetController
{
public:
    WarlockPetController(WarlockAI* owner, ::Unit* pet);
    ~WarlockPetController() = default;

    // Pet behavior management
    void Update(uint32 diff);
    void SetBehavior(PetBehavior behavior);
    void AttackTarget(::Unit* target);
    void MoveToPosition(const Position& pos);
    void UseAbility(uint32 spellId, ::Unit* target = nullptr);

    // Pet state
    bool IsAlive() const;
    bool IsInCombat() const;
    uint32 GetHealthPercent() const;
    Position GetPosition() const;

    // Pet-specific abilities
    void ImpFirebolt(::Unit* target);
    void VoidwalkerTorment();
    void SuccubusSeduction(::Unit* target);
    void FelhunterDevourMagic(::Unit* target);
    void FelguardCleave();

private:
    WarlockAI* _owner;
    ::Unit* _pet;
    PetBehavior _behavior;
    ::Unit* _currentTarget;
    uint32 _lastCommand;
    uint32 _lastAbilityUse;

    // Pet AI logic
    void UpdatePetCombat();
    void UpdatePetMovement();
    void UpdatePetAbilities();
    bool ShouldUsePetAbility(uint32 spellId);
};

} // namespace Playerbot