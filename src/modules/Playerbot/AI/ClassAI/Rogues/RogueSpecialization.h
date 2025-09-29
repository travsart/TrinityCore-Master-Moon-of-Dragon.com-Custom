/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef ROGUE_SPECIALIZATION_H
#define ROGUE_SPECIALIZATION_H

#include "Player.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "SharedDefines.h"
#include "../CooldownManager.h"
#include <chrono>
#include <unordered_map>
#include <queue>

namespace Playerbot
{

// Rogue Spells
enum RogueSpells : uint32
{
    // Core Abilities
    SINISTER_STRIKE = 1752,
    BACKSTAB = 53,
    EVISCERATE = 2098,
    SLICE_AND_DICE = 5171,
    STEALTH = 1784,
    VANISH = 1856,
    SPRINT = 2983,
    EVASION = 5277,
    KICK = 1766,
    GOUGE = 1776,
    SAP = 6770,
    CHEAP_SHOT = 1833,
    KIDNEY_SHOT = 408,
    BLIND = 2094,
    DISTRACTION = 1725,
    PICK_POCKET = 921,
    PICK_LOCK = 1804,
    DETECT_TRAPS = 2836,
    DISARM_TRAP = 1842,

    // Assassination Spells
    POISON_WEAPON = 6499,
    INSTANT_POISON = 8681,
    DEADLY_POISON = 2823,
    WOUND_POISON = 8679,
    MIND_NUMBING_POISON = 5761,
    CRIPPLING_POISON = 3408,
    MUTILATE = 1329,
    COLD_BLOOD = 14177,
    IMPROVED_SAP = 6687,
    RUTHLESSNESS = 14161,
    SEAL_FATE = 14186,
    VIGOR = 14983,
    LETHALITY = 14128,
    VILE_POISONS = 16513,
    IMPROVED_INSTANT_POISON = 14113,
    IMPROVED_DEADLY_POISON = 19216,
    MALICE = 14138,
    IMPROVED_EVISCERATE = 14162,
    RELENTLESS_STRIKES = 14179,
    IMPROVED_EXPOSE_ARMOR = 14168,
    IMPROVED_SLICE_AND_DICE = 14165,
    IMPROVED_POISONS = 14113,
    FIND_WEAKNESS = 91023,
    MASTER_POISONER = 58410,
    TURN_THE_TABLES = 51627,
    VENDETTA = 79140,
    SHADOW_CLONE = 36554,

    // Combat Spells
    RIPOSTE = 14251,
    ADRENALINE_RUSH = 13750,
    BLADE_FLURRY = 13877,
    COMBAT_EXPERTISE = 13741,
    IMPROVED_SINISTER_STRIKE = 13732,
    DEFLECTION = 13713,
    IMPROVED_BACKSTAB = 13743,
    DUAL_WIELD_SPECIALIZATION = 13715,
    IMPROVED_SPRINT = 13743,
    ENDURANCE = 13742,
    LIGHTNING_REFLEXES = 13712,
    IMPROVED_GOUGE = 13741,
    WEAPON_EXPERTISE = 13705,
    AGGRESSION = 18427,
    THROWING_SPECIALIZATION = 51698,
    MACE_SPECIALIZATION = 13709,
    SWORD_SPECIALIZATION = 13960,
    FIST_WEAPON_SPECIALIZATION = 31208,
    DAGGER_SPECIALIZATION = 13706,
    PRECISION = 13705,
    CLOSE_QUARTERS_COMBAT = 56814,
    SAVAGE_COMBAT = 51682,
    HACK_AND_SLASH = 13709,
    BLADE_TWISTING = 31124,
    VITALITY = 61329,
    UNFAIR_ADVANTAGE = 51672,
    IMPROVED_KICK = 13754,
    SURPRISE_ATTACKS = 32601,

    // Subtlety Spells
    SHADOWSTEP = 36554,
    PREPARATION = 14185,
    PREMEDITATION = 14183,
    AMBUSH = 8676,
    GHOST_STRIKE = 14278,
    IMPROVED_AMBUSH = 14079,
    CAMOUFLAGE = 13975,
    INITIATIVE = 13976,
    IMPROVED_STEALTH = 14076,
    MASTER_OF_DISGUISE = 31208,
    SLEIGHT_OF_HAND = 30892,
    DIRTY_FIGHTING = 14067,
    HEMORRHAGE = 16511,
    SERRATED_BLADES = 14171,
    HEIGHTENED_SENSES = 30894,
    SETUP = 13983,
    IMPROVED_CHEAP_SHOT = 14082,
    DEADLINESS = 30902,
    ENVELOPING_SHADOWS = 31216,
    SHADOWSTRIKE = 36554,
    SHADOW_MASTERY = 31221,
    IMPROVED_SHADOW_STEP = 31222,
    FILTHY_TRICKS = 31208,
    WAYLAY = 51692,
    HONOR_AMONG_THIEVES = 51701,
    SHADOW_DANCE = 51713,
    MASTER_OF_SUBTLETY = 31221,
    OPPORTUNITY = 51672,
    SINISTER_CALLING = 31216,
    CHEAT_DEATH = 31230,
    FOCUSED_ATTACKS = 51634,
    FIND_WEAKNESS_SUB = 91023,

    // Advanced Abilities
    EXPOSE_ARMOR = 8647,
    RUPTURE = 1943,
    GARROTE = 703,
    CLOAK_OF_SHADOWS = 31224,
    DISMANTLE = 51722,
    SHADOW_STEP = 36554,
    SHADOWMELD = 58984,
    TRICKS_OF_THE_TRADE = 57934,
    FAN_OF_KNIVES = 51723,
    ENVENOM = 32645,
    HUNGER_FOR_BLOOD = 51662,
    OVERKILL = 58426,
    TURN_THE_TABLES_EFFECT = 51627,
    MASTER_OF_SUBTLETY_EFFECT = 31665,
    SHADOWSTEP_EFFECT = 36563,
    INITIATIVE_EFFECT = 13980,
    COLD_BLOOD_EFFECT = 14177,
    ADRENALINE_RUSH_EFFECT = 13750,
    BLADE_FLURRY_EFFECT = 13877,
    PREPARATION_EFFECT = 14185,
    EVASION_EFFECT = 5277,
    SPRINT_EFFECT = 2983,
    VANISH_EFFECT = 11327,
    STEALTH_EFFECT = 1784,
    GHOST_STRIKE_EFFECT = 14278,
    RIPOSTE_EFFECT = 14251,

    // Weapon Enchants/Poisons
    INSTANT_POISON_1 = 8681,
    INSTANT_POISON_2 = 8684,
    INSTANT_POISON_3 = 8685,
    INSTANT_POISON_4 = 11335,
    INSTANT_POISON_5 = 11336,
    INSTANT_POISON_6 = 11337,
    INSTANT_POISON_7 = 26785,
    INSTANT_POISON_8 = 26786,
    INSTANT_POISON_9 = 43230,
    INSTANT_POISON_10 = 43231,

    DEADLY_POISON_1 = 2823,
    DEADLY_POISON_2 = 2824,
    DEADLY_POISON_3 = 11355,
    DEADLY_POISON_4 = 11356,
    DEADLY_POISON_5 = 25349,
    DEADLY_POISON_6 = 26968,
    DEADLY_POISON_7 = 27187,
    DEADLY_POISON_8 = 43232,
    DEADLY_POISON_9 = 43233,

    WOUND_POISON_1 = 8679,
    WOUND_POISON_2 = 8680,
    WOUND_POISON_3 = 10022,
    WOUND_POISON_4 = 10023,
    WOUND_POISON_5 = 10024,

    MIND_NUMBING_POISON_1 = 5761,
    MIND_NUMBING_POISON_2 = 8692,
    MIND_NUMBING_POISON_3 = 11398,

    CRIPPLING_POISON_1 = 3408,
    CRIPPLING_POISON_2 = 11201,

    // Additional Weapon Types
    ANESTHETIC_POISON = 26785,
    PARALYTIC_POISON = 26969,

    // Missing spells from WotLK
    DEADLY_THROW = 48674,
    KILLING_SPREE = 51690
};

enum class ComboPointState : uint8
{
    ZERO = 0,
    ONE = 1,
    TWO = 2,
    THREE = 3,
    FOUR = 4,
    FIVE = 5
};

enum class StealthState : uint8
{
    NONE = 0,
    STEALTH = 1,
    VANISH = 2,
    SHADOWSTEP = 3,
    SHADOW_DANCE = 4
};

enum class EnergyState : uint8
{
    CRITICAL = 0,  // < 20
    LOW = 1,       // 20-39
    MEDIUM = 2,    // 40-59
    HIGH = 3,      // 60-79
    FULL = 4       // >= 80
};

enum class CombatPhase : uint8
{
    STEALTH_OPENER = 0,
    COMBO_BUILDING = 1,
    COMBO_SPENDING = 2,
    BURST_PHASE = 3,
    SUSTAIN_PHASE = 4,
    EXECUTE_PHASE = 5,
    AOE_PHASE = 6,
    EMERGENCY = 7
};

enum class PoisonType : uint8
{
    NONE = 0,
    INSTANT = 1,
    DEADLY = 2,
    WOUND = 3,
    MIND_NUMBING = 4,
    CRIPPLING = 5,
    ANESTHETIC = 6,
    PARALYTIC = 7
};

// CooldownInfo is defined in CooldownManager.h

struct ComboPointInfo
{
    uint8 current;
    uint8 maximum;
    uint32 lastSpenderTime;
    uint32 timeToDecay;
    bool shouldSpend;

    ComboPointInfo() : current(0), maximum(5), lastSpenderTime(0), timeToDecay(0), shouldSpend(false) {}
};

struct EnergyInfo
{
    uint32 current;
    uint32 maximum;
    uint32 regenRate;
    uint32 lastRegenTime;
    EnergyState state;

    EnergyInfo() : current(0), maximum(100), regenRate(20), lastRegenTime(0), state(EnergyState::CRITICAL) {}
};

struct StealthInfo
{
    StealthState state;
    uint32 remainingTime;
    bool canOpenFromStealth;
    bool hasAdvantage;
    uint32 lastStealthTime;

    StealthInfo() : state(StealthState::NONE), remainingTime(0), canOpenFromStealth(false), hasAdvantage(false), lastStealthTime(0) {}
};

struct PoisonInfo
{
    PoisonType mainHandPoison;
    PoisonType offHandPoison;
    uint32 mainHandCharges;
    uint32 offHandCharges;
    uint32 lastPoisonApplication;

    PoisonInfo() : mainHandPoison(PoisonType::NONE), offHandPoison(PoisonType::NONE), mainHandCharges(0), offHandCharges(0), lastPoisonApplication(0) {}
};

struct TargetDebuffInfo
{
    bool hasSliceAndDice;
    bool hasRupture;
    bool hasGarrote;
    bool hasExposeArmor;
    bool hasPoison;
    uint32 sliceAndDiceRemaining;
    uint32 ruptureRemaining;
    uint32 garroteRemaining;
    uint32 exposeArmorRemaining;
    uint8 poisonStacks;

    TargetDebuffInfo() : hasSliceAndDice(false), hasRupture(false), hasGarrote(false), hasExposeArmor(false), hasPoison(false),
                        sliceAndDiceRemaining(0), ruptureRemaining(0), garroteRemaining(0), exposeArmorRemaining(0), poisonStacks(0) {}
};

class RogueSpecialization
{
public:
    explicit RogueSpecialization(Player* bot);
    virtual ~RogueSpecialization() = default;

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

    // Stealth Management
    virtual void UpdateStealthManagement() = 0;
    virtual bool ShouldEnterStealth() = 0;
    virtual bool CanBreakStealth() = 0;
    virtual void ExecuteStealthOpener(::Unit* target) = 0;

    // Combo Point Management
    virtual void UpdateComboPointManagement() = 0;
    virtual bool ShouldBuildComboPoints() = 0;
    virtual bool ShouldSpendComboPoints() = 0;
    virtual void ExecuteComboBuilder(::Unit* target) = 0;
    virtual void ExecuteComboSpender(::Unit* target) = 0;

    // Poison Management
    virtual void UpdatePoisonManagement() = 0;
    virtual void ApplyPoisons() = 0;
    virtual PoisonType GetOptimalMainHandPoison() = 0;
    virtual PoisonType GetOptimalOffHandPoison() = 0;

    // Debuff Management
    virtual void UpdateDebuffManagement() = 0;
    virtual bool ShouldRefreshDebuff(uint32 spellId) = 0;
    virtual void ApplyDebuffs(::Unit* target) = 0;

    // Energy Management
    virtual void UpdateEnergyManagement() = 0;
    virtual bool HasEnoughEnergyFor(uint32 spellId) = 0;
    virtual uint32 GetEnergyCost(uint32 spellId) = 0;
    virtual bool ShouldWaitForEnergy() = 0;

    // Cooldown Management
    virtual void UpdateCooldownTracking(uint32 diff) = 0;
    virtual bool IsSpellReady(uint32 spellId) = 0;
    virtual void StartCooldown(uint32 spellId) = 0;
    virtual uint32 GetCooldownRemaining(uint32 spellId) = 0;

    // Combat Phase Management
    virtual void UpdateCombatPhase() = 0;
    virtual CombatPhase GetCurrentPhase() = 0;
    virtual bool ShouldExecuteBurstRotation() = 0;

    // Utility Functions
    virtual bool CastSpell(uint32 spellId, ::Unit* target = nullptr) = 0;
    virtual bool HasSpell(uint32 spellId) = 0;
    virtual SpellInfo const* GetSpellInfo(uint32 spellId) = 0;
    virtual uint32 GetSpellCooldown(uint32 spellId) = 0;

protected:
    Player* _bot;
    std::unordered_map<uint32, CooldownInfo> _cooldowns;
    ComboPointInfo _comboPoints;
    EnergyInfo _energy;
    StealthInfo _stealth;
    PoisonInfo _poisons;
    TargetDebuffInfo _targetDebuffs;
    CombatPhase _combatPhase;
    ::Unit* _currentTarget;

    // Core State Tracking
    uint32 _lastUpdateTime;
    uint32 _combatStartTime;
    uint32 _lastEnergyCheck;
    uint32 _lastComboCheck;
    uint32 _lastStealthCheck;

    // Combat Metrics
    uint32 _totalDamageDealt;
    uint32 _totalEnergySpent;
    uint32 _totalCombosBuilt;
    uint32 _totalCombosSpent;
    uint32 _burstPhaseCount;
    float _averageCombatTime;

    // Helper Methods
    void InitializeCooldowns();
    void UpdateResourceStates();
    void UpdateTargetInfo(::Unit* target);
    void LogRotationDecision(const std::string& decision, const std::string& reason);
    bool IsInMeleeRange(::Unit* target);
    bool IsBehindTarget(::Unit* target);
    bool HasWeaponInMainHand();
    bool HasWeaponInOffHand();
    uint8 GetComboPoints();
    uint32 GetCurrentEnergy();
    bool IsStealthed();
    bool HasAura(uint32 spellId, ::Unit* unit = nullptr);
    uint32 GetAuraTimeRemaining(uint32 spellId, ::Unit* unit = nullptr);
};

} // namespace Playerbot

#endif // ROGUE_SPECIALIZATION_H