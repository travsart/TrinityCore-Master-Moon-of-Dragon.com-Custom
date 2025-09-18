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
#include <array>

namespace Playerbot
{

// Shaman specializations
enum class ShamanSpec : uint8
{
    ELEMENTAL = 0,
    ENHANCEMENT = 1,
    RESTORATION = 2
};

// Totem types based on element
enum class TotemType : uint8
{
    FIRE = 0,
    EARTH = 1,
    WATER = 2,
    AIR = 3,
    NONE = 4
};

// Totem behavior states
enum class TotemBehavior : uint8
{
    PASSIVE = 0,
    AGGRESSIVE = 1,
    DEFENSIVE = 2,
    UTILITY = 3
};

// Individual totem information
struct TotemInfo
{
    uint32 spellId;
    TotemType type;
    ::Unit* totem;
    Position position;
    uint32 duration;
    uint32 remainingTime;
    uint32 lastPulse;
    bool isActive;
    float effectRadius;
    TotemBehavior behavior;

    TotemInfo() : spellId(0), type(TotemType::NONE), totem(nullptr), position(),
                  duration(0), remainingTime(0), lastPulse(0), isActive(false),
                  effectRadius(0.0f), behavior(TotemBehavior::PASSIVE) {}

    TotemInfo(uint32 spell, TotemType t, uint32 dur, float radius)
        : spellId(spell), type(t), totem(nullptr), position(), duration(dur),
          remainingTime(dur), lastPulse(getMSTime()), isActive(false),
          effectRadius(radius), behavior(TotemBehavior::PASSIVE) {}
};

// Weapon imbue tracking for enhancement
struct WeaponImbue
{
    uint32 spellId;
    uint32 remainingTime;
    uint32 charges;
    bool isMainHand;

    WeaponImbue() : spellId(0), remainingTime(0), charges(0), isMainHand(true) {}
    WeaponImbue(uint32 spell, uint32 duration, uint32 ch, bool mh)
        : spellId(spell), remainingTime(duration), charges(ch), isMainHand(mh) {}
};

// Shaman AI implementation with full totem and elemental management
class TC_GAME_API ShamanAI : public ClassAI
{
public:
    explicit ShamanAI(Player* bot);
    ~ShamanAI() = default;

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
    // Shaman-specific data
    ShamanSpec _specialization;
    uint32 _manaSpent;
    uint32 _damageDealt;
    uint32 _healingDone;
    uint32 _totemsDeploy;
    uint32 _shocksUsed;

    // Totem management system
    std::array<TotemInfo, 4> _activeTotems; // One per element
    std::unordered_map<TotemType, uint32> _totemCooldowns;
    std::unordered_map<TotemType, Position> _lastTotemPositions;
    uint32 _lastTotemUpdate;
    uint32 _totemCheckInterval;
    bool _needsTotemRefresh;
    Position _optimalTotemPosition;

    // Enhancement specialization tracking
    std::array<WeaponImbue, 2> _weaponImbues; // Main hand + Off hand
    uint32 _stormstrikeCharges;
    uint32 _maelstromWeaponStacks;
    uint32 _unleashedFuryStacks;
    uint32 _lastFlametongueRefresh;
    uint32 _lastWindfuryRefresh;
    bool _dualWielding;

    // Elemental specialization tracking
    uint32 _lightningShieldCharges;
    uint32 _lavaLashStacks;
    uint32 _elementalFocusStacks;
    uint32 _clearcastingProcs;
    uint32 _lastLightningBolt;
    uint32 _lastChainLightning;

    // Restoration tracking
    uint32 _earthShieldCharges;
    uint32 _tidalWaveStacks;
    uint32 _natureSwiftnessReady;
    std::unordered_map<ObjectGuid, uint32> _riptideTimers;
    std::unordered_map<ObjectGuid, uint32> _healingStreamTimers;

    // Shock management
    uint32 _lastEarthShock;
    uint32 _lastFlameShock;
    uint32 _lastFrostShock;
    uint32 _shockCooldown;
    uint32 _shockRotationIndex;

    // Utility tracking
    uint32 _lastPurge;
    uint32 _lastGrounding;
    uint32 _lastTremorTotem;
    uint32 _lastBloodlust;
    std::unordered_map<ObjectGuid, uint32> _hexTargets;

    // Rotation methods by specialization
    void UpdateElementalRotation(::Unit* target);
    void UpdateEnhancementRotation(::Unit* target);
    void UpdateRestorationRotation(::Unit* target);

    // Totem management system
    void UpdateTotemManagement();
    void DeployOptimalTotems();
    void RefreshExpiringTotems();
    void ManageTotemPositioning();
    void CheckTotemHealth();
    void DeployTotem(TotemType type, uint32 spellId);
    void RecallTotem(TotemType type);
    void RecallAllTotems();
    bool IsTotemActive(TotemType type);
    uint32 GetTotemRemainingTime(TotemType type);
    Position GetOptimalTotemPosition(TotemType type);

    // Totem selection and strategy
    uint32 GetOptimalFireTotem();
    uint32 GetOptimalEarthTotem();
    uint32 GetOptimalWaterTotem();
    uint32 GetOptimalAirTotem();
    void SelectTotemStrategy();
    bool ShouldDeployOffensiveTotems();
    bool ShouldDeployDefensiveTotems();
    bool ShouldDeployUtilityTotems();

    // Enhancement weapon management
    void UpdateWeaponImbues();
    void ApplyWeaponImbues();
    void CastFlametongueWeapon();
    void CastWindfuryWeapon();
    void CastFrostbrandWeapon();
    void CastEarthlivingWeapon();
    void RefreshWeaponImbue(bool mainHand);
    bool HasWeaponImbue(bool mainHand);
    uint32 GetWeaponImbueRemainingTime(bool mainHand);

    // Maelstrom weapon management
    void UpdateMaelstromWeapon();
    void ConsumeMaelstromWeapon();
    bool ShouldConsumeMaelstromWeapon();
    uint32 GetMaelstromWeaponStacks();

    // Buff management
    void UpdateShamanBuffs();
    void CastLightningShield();
    void CastWaterShield();
    void CastEarthShield();
    void RefreshShields();
    void ManageElementalShields();

    // Shock rotation system
    void UpdateShockRotation(::Unit* target);
    void CastEarthShock(::Unit* target);
    void CastFlameShock(::Unit* target);
    void CastFrostShock(::Unit* target);
    uint32 GetNextShockSpell(::Unit* target);
    bool IsShockOnCooldown();

    // Healing abilities (Restoration)
    void UseRestorationAbilities();
    void CastHealingWave(::Unit* target);
    void CastLesserHealingWave(::Unit* target);
    void CastChainHeal(::Unit* target);
    void CastRiptide(::Unit* target);
    void ManageHealingTotems();
    ::Unit* GetBestHealTarget();

    // Elemental abilities
    void UseElementalAbilities(::Unit* target);
    void CastLightningBolt(::Unit* target);
    void CastChainLightning(const std::vector<::Unit*>& enemies);
    void CastLavaBurst(::Unit* target);
    void CastElementalBlast(::Unit* target);
    void ManageElementalFocus();

    // Enhancement abilities
    void UseEnhancementAbilities(::Unit* target);
    void CastStormstrike(::Unit* target);
    void CastLavaLash(::Unit* target);
    void CastShamanisticRage();
    void CastFeralSpirit();
    void ManageEnhancementProcs();

    // Mana management
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    void OptimizeManaUsage();
    void UseManaRegeneration();
    void CastManaSpringTotem();

    // Utility abilities
    void UseUtilityAbilities();
    void CastPurge(::Unit* target);
    void CastHex(::Unit* target);
    void CastBloodlust();
    void CastHeroism();
    void CastAstralRecall();
    void CastGhostWolf();

    // Defensive abilities
    void UseDefensiveAbilities();
    void CastShamanisticRage();
    void CastNatureResistanceTotem();
    void CastGroundingTotem();
    void CastTremorTotem();
    void CastStoneclawTotem();

    // Positioning and movement
    void UpdateShamanPositioning();
    bool IsAtOptimalRange(::Unit* target);
    void MaintainTotemRange();
    bool NeedsToRepositionForTotems();
    void FindOptimalTotemPosition();

    // Target selection
    ::Unit* GetBestShockTarget();
    ::Unit* GetBestPurgeTarget();
    ::Unit* GetBestHexTarget();
    ::Unit* GetHighestPriorityTarget();
    std::vector<::Unit*> GetChainLightningTargets(::Unit* primary);

    // Resource tracking
    void TrackElementalProcs();
    void TrackEnhancementProcs();
    void UpdateProcCounters();

    // Threat and aggro management
    void ManageThreat();
    bool HasTooMuchThreat();
    void ReduceThreat();

    // Performance optimization
    void RecordDamageDealt(uint32 damage, ::Unit* target);
    void RecordHealingDone(uint32 amount, ::Unit* target);
    void AnalyzeTotemEffectiveness();
    void OptimizeRotationEfficiency();

    // Helper methods
    bool IsChanneling();
    bool CanCastWithoutInterruption();
    ShamanSpec DetectSpecialization();
    bool IsDualWielding();
    uint32 GetShockSpellId(uint32 shockType);

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr float TOTEM_EFFECT_RADIUS = 30.0f;
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr uint32 TOTEM_CHECK_INTERVAL = 3000; // 3 seconds
    static constexpr uint32 WEAPON_IMBUE_CHECK_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 SHOCK_COOLDOWN = 6000; // 6 seconds
    static constexpr uint32 MAELSTROM_WEAPON_MAX_STACKS = 5;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr uint32 TOTEM_REFRESH_THRESHOLD = 30000; // 30 seconds before expiry

    // Spell IDs (version-specific)
    enum ShamanSpells
    {
        // Elemental spells
        LIGHTNING_BOLT = 403,
        CHAIN_LIGHTNING = 421,
        LAVA_BURST = 51505,
        ELEMENTAL_BLAST = 117014,
        THUNDERSTORM = 51490,

        // Enhancement spells
        STORMSTRIKE = 17364,
        LAVA_LASH = 60103,
        SHAMANISTIC_RAGE = 30823,
        FERAL_SPIRIT = 51533,
        WINDFURY_WEAPON = 8232,
        FLAMETONGUE_WEAPON = 8024,
        FROSTBRAND_WEAPON = 8033,
        EARTHLIVING_WEAPON = 51730,

        // Restoration spells
        HEALING_WAVE = 331,
        LESSER_HEALING_WAVE = 8004,
        CHAIN_HEAL = 1064,
        RIPTIDE = 61295,
        ANCESTRAL_SPIRIT = 2008,

        // Shock spells
        EARTH_SHOCK = 8042,
        FLAME_SHOCK = 8050,
        FROST_SHOCK = 8056,

        // Shield spells
        LIGHTNING_SHIELD = 324,
        WATER_SHIELD = 52127,
        EARTH_SHIELD = 974,

        // Fire totems
        SEARING_TOTEM = 3599,
        FIRE_NOVA_TOTEM = 1535,
        MAGMA_TOTEM = 8190,
        FLAMETONGUE_TOTEM = 8227,
        TOTEM_OF_WRATH = 30706,

        // Earth totems
        EARTHBIND_TOTEM = 2484,
        STONESKIN_TOTEM = 8071,
        STONECLAW_TOTEM = 5730,
        STRENGTH_OF_EARTH_TOTEM = 8075,
        TREMOR_TOTEM = 8143,

        // Water totems
        HEALING_STREAM_TOTEM = 5394,
        MANA_SPRING_TOTEM = 5675,
        POISON_CLEANSING_TOTEM = 8166,
        DISEASE_CLEANSING_TOTEM = 8170,
        FIRE_RESISTANCE_TOTEM = 8184,

        // Air totems
        GROUNDING_TOTEM = 8177,
        NATURE_RESISTANCE_TOTEM = 10595,
        WINDFURY_TOTEM = 8512,
        GRACE_OF_AIR_TOTEM = 8835,
        WRATH_OF_AIR_TOTEM = 3738,

        // Utility spells
        PURGE = 370,
        HEX = 51514,
        BLOODLUST = 2825,
        HEROISM = 32182,
        ASTRAL_RECALL = 556,
        GHOST_WOLF = 2645,

        // Defensive
        NATURE_RESISTANCE = 8182,
        ANCESTRAL_FORTITUDE = 16236
    };

    // Totem spell mappings
    static const std::unordered_map<TotemType, std::vector<uint32>> TOTEM_SPELLS;
};

// Utility class for shaman calculations
class TC_GAME_API ShamanSpellCalculator
{
public:
    // Damage calculations
    static uint32 CalculateLightningBoltDamage(Player* caster, ::Unit* target);
    static uint32 CalculateChainLightningDamage(Player* caster, ::Unit* target, uint32 jumpNumber);
    static uint32 CalculateShockDamage(uint32 shockSpell, Player* caster, ::Unit* target);

    // Healing calculations
    static uint32 CalculateHealingWaveDamage(Player* caster, ::Unit* target);
    static uint32 CalculateChainHealAmount(Player* caster, ::Unit* target, uint32 jumpNumber);

    // Totem effectiveness
    static float CalculateTotemEffectiveness(uint32 totemSpell, const std::vector<::Unit*>& affectedUnits);
    static Position GetOptimalTotemPosition(TotemType type, const std::vector<::Unit*>& allies);
    static bool ShouldReplaceTotem(uint32 currentTotem, uint32 newTotem, Player* caster);

    // Enhancement calculations
    static uint32 CalculateStormstrikeDamage(Player* caster, ::Unit* target);
    static uint32 CalculateLavaLashDamage(Player* caster, ::Unit* target);
    static float CalculateWeaponImbueEffectiveness(uint32 imbueSpell, Player* caster);

    // Mana efficiency
    static float CalculateSpellManaEfficiency(uint32 spellId, Player* caster);
    static uint32 GetOptimalSpellForMana(Player* caster, ::Unit* target, uint32 availableMana);

private:
    // Cache for shaman calculations
    static std::unordered_map<uint32, uint32> _spellDamageCache;
    static std::unordered_map<TotemType, Position> _totemPositionCache;
    static std::mutex _cacheMutex;

    static void CacheShamanData();
};

// Totem AI controller for intelligent totem management
class TC_GAME_API TotemController
{
public:
    TotemController(ShamanAI* owner);
    ~TotemController() = default;

    // Totem management
    void Update(uint32 diff);
    void DeployTotem(TotemType type, uint32 spellId, const Position& position);
    void RecallTotem(TotemType type);
    void RecallAllTotems();

    // Totem state
    bool IsTotemActive(TotemType type) const;
    uint32 GetTotemRemainingTime(TotemType type) const;
    Position GetTotemPosition(TotemType type) const;

    // Strategy
    void SetTotemStrategy(const std::vector<uint32>& totemPriorities);
    void AdaptToSituation(bool inCombat, bool groupHealing, bool needsUtility);

private:
    ShamanAI* _owner;
    std::array<TotemInfo, 4> _totems;
    uint32 _lastUpdate;
    std::vector<uint32> _deploymentPriorities;

    // Totem management logic
    void UpdateTotemStates();
    void CheckTotemEffectiveness();
    void OptimizeTotemPlacement();
    bool ShouldReplaceTotem(TotemType type, uint32 newSpellId);
};

} // namespace Playerbot