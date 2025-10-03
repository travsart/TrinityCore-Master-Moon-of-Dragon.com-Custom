/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_HUNTERSPECIALIZATION_H
#define TRINITY_HUNTERSPECIALIZATION_H

#include "Define.h"
#include "Position.h"
#include "ObjectGuid.h"
#include "Timer.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

class Player;
class Unit;

namespace Playerbot
{

// Hunter spell IDs - core abilities
enum HunterSpells : uint32
{
    // Shots and attacks
    AUTO_SHOT = 75,
    HUNTER_S_MARK = 1130,
    ARCANE_SHOT = 3044,
    CONCUSSIVE_SHOT = 5116,
    SERPENT_STING = 1978,
    MULTI_SHOT = 2643,
    AIMED_SHOT = 19434,
    EXPLOSIVE_SHOT = 60053,
    BLACK_ARROW = 3674,

    // Pet abilities
    CALL_PET = 883,
    DISMISS_PET = 2641,
    REVIVE_PET = 982,
    MEND_PET = 136,
    BESTIAL_WRATH = 19574,
    INTIMIDATION = 19577,

    // Traps
    FREEZING_TRAP = 1499,
    EXPLOSIVE_TRAP = 13813,
    IMMOLATION_TRAP = 13795,
    FROST_TRAP = 13809,
    SNAKE_TRAP = 34600,

    // Aspects
    ASPECT_OF_THE_HAWK = 13165,
    ASPECT_OF_THE_MONKEY = 13163,
    ASPECT_OF_THE_CHEETAH = 5118,
    ASPECT_OF_THE_PACK = 13159,
    ASPECT_OF_THE_WILD = 20043,
    ASPECT_OF_THE_VIPER = 34074,
    ASPECT_OF_THE_DRAGONHAWK = 61846,

    // Stings
    VIPER_STING = 3034,
    SCORPID_STING = 3043,
    WYVERN_STING = 19386,

    // Utility
    TRACK_BEASTS = 1494,
    TRACK_HUMANOIDS = 19883,
    TRACK_UNDEAD = 19884,
    TRACK_HIDDEN = 19885,
    TRACK_ELEMENTALS = 19879,
    TRACK_DEMONS = 19878,
    TRACK_GIANTS = 19882,
    TRACK_DRAGONKIN = 19880,
    DISENGAGE = 781,
    FEIGN_DEATH = 5384,
    DETERRENCE = 19263,

    // Ranged weapon abilities
    STEADY_SHOT = 34120,
    KILL_SHOT = 53351,
    CHIMERA_SHOT = 53209,

    // Melee abilities (Survival)
    RAPTOR_STRIKE = 2973,
    MONGOOSE_BITE = 1495,
    WING_CLIP = 2974,
    COUNTERATTACK = 19306,

    // Cooldowns
    RAPID_FIRE = 3045,
    READINESS = 23989,
    KILL_COMMAND = 34026,

    // Buffs
    EAGLE_EYE = 6197,
    FAR_SIGHT = 6196,
    HUNTERS_MARK = 1130
};

// Pet types for different situations
enum class PetType : uint8
{
    NONE = 0,
    FEROCITY = 1,   // DPS pets (cat, wolf, raptor)
    TENACITY = 2,   // Tank pets (bear, turtle, crab)
    CUNNING = 3     // PvP/Utility pets (spider, bat, serpent)
};

// Pet abilities
enum PetSpells : uint32
{
    // Universal pet abilities
    PET_ATTACK = 7769,
    PET_FOLLOW = 1792,
    PET_STAY = 1793,
    PET_PASSIVE = 1794,
    PET_DEFENSIVE = 1795,
    PET_AGGRESSIVE = 1796,

    // Ferocity abilities
    CLAW = 16827,
    BITE = 17253,
    DASH = 23099,
    PROWL = 24450,

    // Tenacity abilities
    GROWL = 2649,
    COWER = 1753,
    THUNDERSTOMP = 26187,

    // Cunning abilities
    WEB = 4167,
    POISON_SPIT = 24640,
    SCREECH = 24423
};

// Hunter constants
enum HunterConstants : uint32 {
    RANGED_ATTACK_RANGE_INT = 35,
    MELEE_RANGE_INT = 5,
    DEAD_ZONE_MIN_INT = 5,
    DEAD_ZONE_MAX_INT = 8,
    OPTIMAL_RANGE_INT = 25,
    PET_COMMAND_RANGE_INT = 50
};

// Note: MELEE_RANGE is defined in ObjectDefines.h (5.0f)
#define HUNTER_RANGED_ATTACK_RANGE 35.0f
#define HUNTER_DEAD_ZONE_MIN 5.0f
#define HUNTER_DEAD_ZONE_MAX 8.0f
#define HUNTER_OPTIMAL_RANGE 25.0f
#define HUNTER_PET_COMMAND_RANGE 50.0f

// Timing constants
enum HunterTimingConstants : uint32 {
    PET_CHECK_INTERVAL = 2000,        // 2 seconds
    TRAP_COOLDOWN_TIME = 30000,       // 30 seconds
    ASPECT_CHECK_INTERVAL = 5000,     // 5 seconds
    TRACKING_UPDATE_INTERVAL = 10000, // 10 seconds
    ROTATION_UPDATE_INTERVAL = 200    // 200ms
};

// Trap information
struct TrapInfo
{
    uint32 spellId;
    uint32 lastUsed;
    Position position;
    uint32 duration;

    TrapInfo(uint32 spell = 0, uint32 last = 0, Position pos = Position(), uint32 dur = 30000)
        : spellId(spell), lastUsed(last), position(pos), duration(dur) {}

    bool IsReady() const { return (getMSTime() - lastUsed) >= TRAP_COOLDOWN_TIME; }
    bool IsActive() const { return (getMSTime() - lastUsed) < duration; }
};

// Pet information
struct PetInfo
{
    ObjectGuid guid;
    uint32 health;
    uint32 maxHealth;
    uint32 happiness;
    PetType type;
    uint32 lastCommand;
    uint32 lastFeed;
    bool isDead;

    PetInfo() : guid(ObjectGuid::Empty), health(0), maxHealth(0), happiness(0),
                type(PetType::NONE), lastCommand(0), lastFeed(0), isDead(true) {}

    float GetHealthPct() const
    {
        return maxHealth > 0 ? (float(health) / maxHealth * 100.0f) : 0.0f;
    }
};

// Shot rotation tracking
enum class ShotPriority : uint8
{
    KILL_SHOT = 0,      // Execute phase
    AIMED_SHOT = 1,     // High damage
    ARCANE_SHOT = 2,    // Instant damage
    STEADY_SHOT = 3,    // Filler/mana efficient
    MULTI_SHOT = 4,     // AoE
    AUTO_SHOT = 5       // Automatic
};

// Hunter rotation phases
enum class HunterRotationPhase : uint8
{
    OPENING = 0,        // Initial setup
    RANGED_DPS = 1,     // Standard ranged rotation
    MELEE_COMBAT = 2,   // In melee range (Survival)
    KITING = 3,         // Movement-based combat
    EXECUTE = 4,        // Low health target
    AOE = 5,            // Multiple targets
    PET_MANAGEMENT = 6  // Focus on pet
};

class HunterSpecialization
{
public:
    explicit HunterSpecialization(Player* bot);
    virtual ~HunterSpecialization() = default;

    // Core rotation interface
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual Position GetOptimalPosition(::Unit* target) = 0;

    // Note: These methods are provided by template base classes as 'final override'
    // Do not declare as pure virtual here:
    // - UpdateCooldowns(uint32 diff)
    // - CanUseAbility(uint32 spellId)
    // - OnCombatStart(::Unit* target)
    // - OnCombatEnd()
    // - HasEnoughResource(uint32 spellId)
    // - ConsumeResource(uint32 spellId)
    // - GetOptimalRange(::Unit* target)

    // Pet management interface
    virtual void UpdatePetManagement() = 0;
    virtual void SummonPet() = 0;
    virtual void CommandPetAttack(::Unit* target) = 0;
    virtual void CommandPetFollow() = 0;
    virtual void CommandPetStay() = 0;
    virtual void MendPetIfNeeded() = 0;
    virtual void FeedPetIfNeeded() = 0;
    virtual bool HasActivePet() const = 0;
    virtual PetInfo GetPetInfo() const = 0;

    // Trap management interface
    virtual void UpdateTrapManagement() = 0;
    virtual void PlaceTrap(uint32 trapSpell, Position position) = 0;
    virtual bool ShouldPlaceTrap() const = 0;
    virtual uint32 GetOptimalTrapSpell() const = 0;
    virtual std::vector<TrapInfo> GetActiveTraps() const = 0;

    // Aspect management interface
    virtual void UpdateAspectManagement() = 0;
    virtual void SwitchToOptimalAspect() = 0;
    virtual uint32 GetOptimalAspect() const = 0;
    virtual bool HasCorrectAspect() const = 0;

    // Range and positioning
    virtual void UpdateRangeManagement() = 0;
    virtual bool IsInDeadZone(::Unit* target) const = 0;
    virtual bool ShouldKite(::Unit* target) const = 0;
    virtual Position GetKitePosition(::Unit* target) const = 0;
    virtual void HandleDeadZone(::Unit* target) = 0;

    // Tracking management
    virtual void UpdateTracking() = 0;
    virtual uint32 GetOptimalTracking() const = 0;
    virtual void ApplyTracking(uint32 trackingSpell) = 0;

    // Common utility methods
    Player* GetBot() const { return _bot; }
    bool IsRangedWeaponEquipped() const;
    bool HasAmmo() const;
    uint32 GetAmmoCount() const;
    float GetRangedAttackSpeed() const;
    bool CanCastWhileMoving() const;

protected:
    Player* _bot;

    // Shared state tracking
    PetInfo _petInfo;
    std::vector<TrapInfo> _activeTraps;
    uint32 _currentAspect;
    uint32 _currentTracking;
    HunterRotationPhase _rotationPhase;

    // Timing
    uint32 _lastPetCheck;
    uint32 _lastTrapCheck;
    uint32 _lastAspectCheck;
    uint32 _lastTrackingUpdate;
    uint32 _lastRangeCheck;
    uint32 _lastAutoShot;

    // Combat metrics
    uint32 _shotsFired;
    uint32 _petsLost;
    uint32 _trapsPlaced;
    uint32 _manaConsumed;
    uint32 _totalDamageDealt;

    // Cooldown tracking
    std::unordered_map<uint32, uint32> _cooldowns;

    // Common helper methods
    void UpdateCooldown(uint32 spellId, uint32 cooldown);
    bool IsCooldownReady(uint32 spellId) const;
    uint32 GetSpellCooldown(uint32 spellId) const;
    void SetRotationPhase(HunterRotationPhase phase);

    // Pet helper methods
    void UpdatePetInfo();
    bool IsPetAlive() const;
    bool IsPetHappy() const;
    uint32 GetPetHappiness() const;

    // Range helper methods
    float GetDistanceToTarget(::Unit* target) const;
    bool IsInOptimalRange(::Unit* target) const;
    bool IsInMeleeRange(::Unit* target) const;
    bool IsInRangedRange(::Unit* target) const;
};

} // namespace Playerbot

#endif