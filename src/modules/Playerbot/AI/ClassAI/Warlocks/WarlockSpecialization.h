/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Position.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Timer.h"
#include <vector>
#include <unordered_map>

// Forward declarations
class Player;
class Unit;

namespace Playerbot
{

// Warlock specializations
enum class WarlockSpec : uint8
{
    AFFLICTION = 0,
    DEMONOLOGY = 1,
    DESTRUCTION = 2
};

// Pet types for warlocks
enum class WarlockPet : uint8
{
    NONE = 0,
    IMP = 1,
    VOIDWALKER = 2,
    SUCCUBUS = 3,
    FELHUNTER = 4,
    FELGUARD = 5,
    INFERNAL = 6,
    DOOMGUARD = 7
};

// Pet behavior modes
enum class PetBehavior : uint8
{
    PASSIVE = 0,
    DEFENSIVE = 1,
    AGGRESSIVE = 2
};

// DoT tracking for affliction
struct DoTInfo
{
    uint32 spellId;
    ::Unit* target;
    uint32 remainingTime;
    uint32 ticksRemaining;
    uint32 lastTick;
    bool needsRefresh;

    DoTInfo() : spellId(0), target(nullptr), remainingTime(0), ticksRemaining(0), lastTick(0), needsRefresh(false) {}
    DoTInfo(uint32 spell, ::Unit* tgt, uint32 duration) : spellId(spell), target(tgt), remainingTime(duration),
                                                          ticksRemaining(duration / 3000), lastTick(getMSTime()), needsRefresh(false) {}
};

// Soul shard management
struct SoulShardInfo
{
    uint32 count;
    uint32 maxCount;
    uint32 lastUsed;
    bool conserveMode;

    SoulShardInfo() : count(0), maxCount(32), lastUsed(0), conserveMode(false) {}
};

// Base specialization interface for all Warlock specs
class TC_GAME_API WarlockSpecialization
{
public:
    explicit WarlockSpecialization(Player* bot) : _bot(bot) {}
    virtual ~WarlockSpecialization() = default;

    // Core specialization interface
    virtual void UpdateRotation(::Unit* target) {}
    virtual void UpdateBuffs() {}
    virtual void UpdateCooldowns(uint32 diff) {}
    virtual bool CanUseAbility(uint32 spellId) { return false; }

    // Combat callbacks
    virtual void OnCombatStart(::Unit* target) {}
    virtual void OnCombatEnd() {}

    // Resource management
    virtual bool HasEnoughResource(uint32 spellId) { return false; }
    virtual void ConsumeResource(uint32 spellId) {}

    // Positioning
    virtual Position GetOptimalPosition(::Unit* target) { return Position(); }
    virtual float GetOptimalRange(::Unit* target) { return 30.0f; }

    // Pet management - core to all warlock specs
    virtual void UpdatePetManagement() {}
    virtual void SummonOptimalPet() {}
    virtual WarlockPet GetOptimalPetForSituation() { return WarlockPet::IMP; }
    virtual void CommandPet(uint32 action, ::Unit* target = nullptr) {}

    // DoT management - available to all specs
    virtual void UpdateDoTManagement() {}
    virtual void ApplyDoTsToTarget(::Unit* target) {}
    virtual bool ShouldApplyDoT(::Unit* target, uint32 spellId) { return false; }

    // Curse management - available to all specs
    virtual void UpdateCurseManagement() {}
    virtual uint32 GetOptimalCurseForTarget(::Unit* target) { return 0; }

    // Soul shard management
    virtual void UpdateSoulShardManagement() {}
    bool HasSoulShardsAvailable(uint32 required = 1);
    void UseSoulShard(uint32 spellId);

    // Specialization info
    virtual WarlockSpec GetSpecialization() const { return WarlockSpec::AFFLICTION; }
    virtual const char* GetSpecializationName() const { return "Affliction"; }

protected:
    Player* GetBot() const { return _bot; }

    // Helper methods for positioning and casting
    Position GetOptimalCastingPosition(::Unit* target);
    bool IsInCastingRange(::Unit* target, uint32 spellId);
    bool IsChanneling();
    bool IsCasting();
    bool CanCast();
    bool UseEmergencyAbilities();

    // Shared pet management
    WarlockPet _currentPet;
    ::Unit* _petUnit;
    PetBehavior _petBehavior;
    uint32 _lastPetCommand;

    // Shared DoT tracking
    std::unordered_map<ObjectGuid, std::vector<DoTInfo>> _activeDoTs;
    uint32 _lastDoTCheck;

    // Shared soul shard tracking
    SoulShardInfo _soulShards;

    // Common methods available to all specializations
    void SummonPet(WarlockPet petType);
    void PetAttackTarget(::Unit* target);
    void PetFollow();
    bool IsPetAlive();
    bool IsDoTActive(::Unit* target, uint32 spellId);
    uint32 GetDoTRemainingTime(::Unit* target, uint32 spellId);
    void CastCurse(::Unit* target, uint32 curseId);
    ::Unit* GetBestDirectDamageTarget();
    ::Unit* GetBestDoTTarget();
    bool CastDeathCoil(::Unit* target);
    bool CastBanish(::Unit* target);
    bool CastFear(::Unit* target);
    void UpdateArmor();
    uint32 GetMana() const;
    uint32 GetMaxMana() const;
    float GetManaPercent() const;
    bool HasEnoughMana(uint32 amount);
    void CastLifeTap();

    // Shared spell IDs
    enum SharedSpells
    {
        // Pet summons
        SUMMON_IMP = 688,
        SUMMON_VOIDWALKER = 697,
        SUMMON_SUCCUBUS = 712,
        SUMMON_FELHUNTER = 691,
        SUMMON_FELGUARD = 30146,

        // DoT spells
        CORRUPTION = 172,
        CURSE_OF_AGONY = 980,
        IMMOLATE = 348,

        // Direct damage
        SHADOW_BOLT = 686,
        SEARING_PAIN = 5676,

        // Curses
        CURSE_OF_ELEMENTS = 1490,
        CURSE_OF_SHADOW = 17937,
        CURSE_OF_TONGUES = 1714,
        CURSE_OF_WEAKNESS = 702,

        // Crowd control
        FEAR = 5782,
        BANISH = 710,
        DEATH_COIL = 6789,

        // Buffs
        DEMON_SKIN = 687,
        DEMON_ARMOR = 706,
        FEL_ARMOR = 28176,

        // Utility
        LIFE_TAP = 1454,
        SOULSHATTER = 32835,

        // Pet commands
        PET_ATTACK = 7812,
        PET_FOLLOW = 7813,
        PET_PASSIVE = 7815,
        PET_DEFENSIVE = 7816,
        PET_AGGRESSIVE = 7817
    };

private:
    Player* _bot;
};

} // namespace Playerbot