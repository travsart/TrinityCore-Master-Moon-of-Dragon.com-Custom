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
#include <vector>

namespace Playerbot
{

// Mage specializations
enum class MageSpec : uint8
{
    ARCANE = 0,
    FIRE = 1,
    FROST = 2
};

// Mage schools for spell priorities
enum class MageSchool : uint8
{
    ARCANE = 0,
    FIRE = 1,
    FROST = 2,
    GENERIC = 3
};

// Mage AI implementation with full spellcaster capabilities
class TC_GAME_API MageAI : public ClassAI
{
public:
    explicit MageAI(Player* bot);
    ~MageAI() = default;

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
    // Mage-specific data
    MageSpec _specialization;
    uint32 _manaSpent;
    uint32 _damageDealt;
    uint32 _spellsCast;
    uint32 _interruptedCasts;
    uint32 _lastPolymorph;
    uint32 _lastCounterspell;
    uint32 _lastBlink;

    // Arcane specialization tracking
    uint32 _arcaneCharges;
    uint32 _arcaneOrbCharges;
    uint32 _arcaneBlastStacks;
    uint32 _lastArcanePower;

    // Fire specialization tracking
    uint32 _combustionStacks;
    uint32 _pyroblastProcs;
    bool _hotStreakAvailable;
    uint32 _lastCombustion;

    // Frost specialization tracking
    uint32 _frostboltCounter;
    uint32 _icicleStacks;
    uint32 _frozenOrbCharges;
    uint32 _lastIcyVeins;
    bool _wintersChill;

    // Crowd control and utility tracking
    std::unordered_map<ObjectGuid, uint32> _polymorphTargets;
    std::unordered_map<ObjectGuid, uint32> _slowTargets;
    uint32 _lastManaShield;
    uint32 _lastIceBarrier;

    // Rotation methods by specialization
    void UpdateArcaneRotation(::Unit* target);
    void UpdateFireRotation(::Unit* target);
    void UpdateFrostRotation(::Unit* target);

    // Mana management
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    void OptimizeManaUsage();
    bool ShouldConserveMana();
    void UseManaRegeneration();

    // Buff management
    void UpdateMageBuffs();
    void CastMageArmor();
    void CastManaShield();
    void CastIceBarrier();
    void CastArcaneIntellect();
    void UpdateArmorSpells();

    // Defensive abilities
    void UseDefensiveAbilities();
    void UseBlink();
    void UseInvisibility();
    void UseIceBlock();
    void UseColdSnap();
    void UseBarrierSpells();

    // Offensive cooldowns
    void UseOffensiveCooldowns();
    void UseArcanePower();
    void UseCombustion();
    void UseIcyVeins();
    void UsePresenceOfMind();
    void UseMirrorImage();

    // Crowd control abilities
    void UseCrowdControl(::Unit* target);
    void UsePolymorph(::Unit* target);
    void UseFrostNova();
    void UseCounterspell(::Unit* target);
    void UseBanish(::Unit* target);

    // AoE abilities
    void UseAoEAbilities(const std::vector<::Unit*>& enemies);
    void UseBlizzard(const std::vector<::Unit*>& enemies);
    void UseFlamestrike(const std::vector<::Unit*>& enemies);
    void UseArcaneExplosion(const std::vector<::Unit*>& enemies);
    void UseConeOfCold(const std::vector<::Unit*>& enemies);

    // Positioning and movement
    void UpdateMagePositioning();
    bool IsAtOptimalRange(::Unit* target);
    bool NeedsToKite(::Unit* target);
    void PerformKiting(::Unit* target);
    bool IsInDanger();
    void FindSafeCastingPosition();

    // Targeting and priorities
    ::Unit* GetBestPolymorphTarget();
    ::Unit* GetBestCounterspellTarget();
    ::Unit* GetBestAoETarget();
    bool ShouldInterrupt(::Unit* target);
    bool CanPolymorphSafely(::Unit* target);

    // Specialization-specific mechanics
    void ManageArcaneCharges();
    void UpdateArcaneOrb();
    void ManageArcaneBlast();

    void ManageCombustion();
    void UpdateHotStreak();
    void ManagePyroblastProcs();

    void ManageFrostbolt();
    void UpdateIcicles();
    void ManageWintersChill();

    // Spell effectiveness tracking
    void RecordSpellCast(uint32 spellId, ::Unit* target);
    void RecordSpellHit(uint32 spellId, ::Unit* target, uint32 damage);
    void RecordSpellCrit(uint32 spellId, ::Unit* target, uint32 damage);
    void AnalyzeCastingEffectiveness();

    // Helper methods
    bool IsChanneling();
    bool IsCasting();
    bool CanCastSpell();
    MageSchool GetSpellSchool(uint32 spellId);
    uint32 GetSpellCastTime(uint32 spellId);
    bool IsSpellInstant(uint32 spellId);

    // Specialization detection and optimization
    MageSpec DetectSpecialization();
    void OptimizeForSpecialization();
    bool HasTalent(uint32 talentId);

    // Threat and aggro management
    void ManageThreat();
    bool HasTooMuchThreat();
    void ReduceThreat();

    // Emergency responses
    void HandleEmergencySituation();
    bool IsInCriticalDanger();
    void UseEmergencyEscape();

    // Performance optimization
    void UpdatePerformanceMetrics(uint32 diff);
    void OptimizeCastingSequence();

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr float MINIMUM_SAFE_RANGE = 15.0f;
    static constexpr float KITING_RANGE = 20.0f;
    static constexpr uint32 MAX_ARCANE_CHARGES = 4;
    static constexpr uint32 POLYMORPH_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 COUNTERSPELL_COOLDOWN = 24000; // 24 seconds
    static constexpr uint32 BLINK_COOLDOWN = 15000; // 15 seconds
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr float MANA_EMERGENCY_THRESHOLD = 0.15f; // 15%

    // Spell IDs (version-specific - these need to be accurate for target WoW version)
    enum MageSpells
    {
        // Arcane spells
        ARCANE_MISSILES = 5143,
        ARCANE_BLAST = 30451,
        ARCANE_BARRAGE = 44425,
        ARCANE_ORB = 153626,
        ARCANE_POWER = 12042,
        ARCANE_INTELLECT = 1459,
        ARCANE_EXPLOSION = 1449,

        // Fire spells
        FIREBALL = 133,
        FIRE_BLAST = 2136,
        PYROBLAST = 11366,
        FLAMESTRIKE = 2120,
        SCORCH = 2948,
        COMBUSTION = 190319,
        LIVING_BOMB = 44457,
        DRAGON_BREATH = 31661,

        // Frost spells
        FROSTBOLT = 116,
        ICE_LANCE = 30455,
        FROZEN_ORB = 84714,
        BLIZZARD = 10,
        CONE_OF_COLD = 120,
        ICY_VEINS = 12472,
        WATER_ELEMENTAL = 31687,
        ICE_BARRIER = 11426,

        // Crowd control
        POLYMORPH = 118,
        FROST_NOVA = 122,
        COUNTERSPELL = 2139,
        BANISH = 710,

        // Defensive abilities
        BLINK = 1953,
        INVISIBILITY = 66,
        ICE_BLOCK = 45438,
        COLD_SNAP = 11958,
        MANA_SHIELD = 1463,

        // Utility
        MIRROR_IMAGE = 55342,
        PRESENCE_OF_MIND = 12043,
        TELEPORT_STORMWIND = 3561,
        TELEPORT_IRONFORGE = 3562,
        PORTAL_STORMWIND = 10059,
        PORTAL_IRONFORGE = 11416,

        // Armor spells
        MAGE_ARMOR = 6117,
        FROST_ARMOR = 7301,
        MOLTEN_ARMOR = 30482,

        // Conjure spells
        CONJURE_FOOD = 587,
        CONJURE_WATER = 5504,
        CONJURE_MANA_GEM = 759
    };

    // Spell school mappings
    static const std::unordered_map<uint32, MageSchool> _spellSchools;
};

// Utility class for mage spell calculations
class TC_GAME_API MageSpellCalculator
{
public:
    // Damage calculations
    static uint32 CalculateFireballDamage(Player* caster, ::Unit* target);
    static uint32 CalculateFrostboltDamage(Player* caster, ::Unit* target);
    static uint32 CalculateArcaneMissilesDamage(Player* caster, ::Unit* target);

    // Mana cost calculations
    static uint32 CalculateSpellManaCost(uint32 spellId, Player* caster);
    static uint32 ApplyArcanePowerBonus(uint32 damage, Player* caster);

    // Cast time calculations
    static uint32 CalculateCastTime(uint32 spellId, Player* caster);
    static float GetHasteModifier(Player* caster);

    // Critical hit calculations
    static float CalculateCritChance(uint32 spellId, Player* caster, ::Unit* target);
    static bool WillCriticalHit(uint32 spellId, Player* caster, ::Unit* target);

    // Resistance calculations
    static float CalculateResistance(uint32 spellId, Player* caster, ::Unit* target);
    static uint32 ApplyResistance(uint32 damage, float resistance);

    // Specialization optimization
    static float GetSpecializationBonus(MageSpec spec, uint32 spellId);
    static uint32 GetOptimalRotationSpell(MageSpec spec, Player* caster, ::Unit* target);

private:
    // Cache for spell data
    static std::unordered_map<uint32, uint32> _baseDamageCache;
    static std::unordered_map<uint32, uint32> _manaCostCache;
    static std::unordered_map<uint32, uint32> _castTimeCache;
    static std::mutex _cacheMutex;

    static void CacheSpellData(uint32 spellId);
};

} // namespace Playerbot