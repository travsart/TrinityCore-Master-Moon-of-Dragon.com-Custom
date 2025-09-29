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
#include <bitset>

namespace Playerbot
{

// Death Knight specializations
enum class DeathKnightSpec : uint8
{
    BLOOD = 0,
    FROST = 1,
    UNHOLY = 2
};

// Rune types
enum class RuneType : uint8
{
    BLOOD = 0,
    FROST = 1,
    UNHOLY = 2,
    DEATH = 3
};

// Disease types
enum class DiseaseType : uint8
{
    NONE = 0,
    BLOOD_PLAGUE = 1,
    FROST_FEVER = 2,
    NECROTIC_STRIKE = 3
};

// Rune state tracking
struct RuneInfo
{
    RuneType type;
    bool available;
    uint32 cooldownRemaining;
    uint32 lastUsed;

    RuneInfo() : type(RuneType::BLOOD), available(true), cooldownRemaining(0), lastUsed(0) {}
    RuneInfo(RuneType t) : type(t), available(true), cooldownRemaining(0), lastUsed(0) {}

    bool IsReady() const { return available && cooldownRemaining == 0; }
    void Use() { available = false; lastUsed = getMSTime(); cooldownRemaining = 10000; } // 10 second cooldown
};

// Disease tracking on targets
struct DiseaseInfo
{
    DiseaseType type;
    uint32 spellId;
    uint32 remainingTime;
    uint32 ticksRemaining;
    uint32 damagePerTick;
    uint32 lastTick;

    DiseaseInfo() : type(DiseaseType::NONE), spellId(0), remainingTime(0),
                    ticksRemaining(0), damagePerTick(0), lastTick(0) {}

    DiseaseInfo(DiseaseType t, uint32 spell, uint32 duration, uint32 damage)
        : type(t), spellId(spell), remainingTime(duration),
          ticksRemaining(duration / 3000), damagePerTick(damage), lastTick(getMSTime()) {}

    bool IsActive() const { return remainingTime > 0; }
    bool NeedsRefresh() const { return remainingTime < 6000; } // Refresh with <6 seconds remaining
};

// Base class for all Death Knight specializations
class TC_GAME_API DeathKnightSpecialization
{
public:
    explicit DeathKnightSpecialization(Player* bot);
    virtual ~DeathKnightSpecialization() = default;

    // Core specialization interface
    virtual void UpdateRotation(::Unit* target) = 0;
    virtual void UpdateBuffs() = 0;
    virtual void UpdateCooldowns(uint32 diff) = 0;
    virtual bool CanUseAbility(uint32 spellId) = 0;

    // Combat callbacks
    virtual void OnCombatStart(::Unit* target) = 0;
    virtual void OnCombatEnd() = 0;

    // Resource management
    virtual bool HasEnoughResource(uint32 spellId) = 0;
    virtual void ConsumeResource(uint32 spellId) = 0;

    // Positioning
    virtual Position GetOptimalPosition(::Unit* target) = 0;
    virtual float GetOptimalRange(::Unit* target) = 0;

    // Rune management
    virtual void UpdateRuneManagement() = 0;
    virtual bool HasAvailableRunes(RuneType type, uint32 count = 1) = 0;
    virtual void ConsumeRunes(RuneType type, uint32 count = 1) = 0;
    virtual uint32 GetAvailableRunes(RuneType type) const = 0;

    // Runic Power management
    virtual void UpdateRunicPowerManagement() = 0;
    virtual void GenerateRunicPower(uint32 amount) = 0;
    virtual void SpendRunicPower(uint32 amount) = 0;
    virtual uint32 GetRunicPower() const = 0;
    virtual bool HasEnoughRunicPower(uint32 required) const = 0;

    // Disease management
    virtual void UpdateDiseaseManagement() = 0;
    virtual void ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId) = 0;
    virtual bool HasDisease(::Unit* target, DiseaseType type) const = 0;
    virtual bool ShouldApplyDisease(::Unit* target, DiseaseType type) const = 0;
    virtual void RefreshExpringDiseases() = 0;

    // Death and Decay management
    virtual void UpdateDeathAndDecay() = 0;
    virtual bool ShouldCastDeathAndDecay() const = 0;
    virtual void CastDeathAndDecay(Position targetPos) = 0;

    // Specialization info
    virtual DeathKnightSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* GetBot() const { return _bot; }

    // Rune helpers
    void RegenerateRunes(uint32 diff);
    bool CanConvertRune(RuneType from, RuneType to) const;
    void ConvertRune(RuneType from, RuneType to);
    uint32 GetTotalAvailableRunes() const;

    // Disease helpers
    void UpdateDiseaseTimers(uint32 diff);
    void RemoveExpiredDiseases();
    std::vector<DiseaseInfo> GetActiveDiseases(::Unit* target) const;
    uint32 GetDiseaseRemainingTime(::Unit* target, DiseaseType type) const;

    // Death Grip and utility
    bool ShouldUseDeathGrip(::Unit* target) const;
    void CastDeathGrip(::Unit* target);
    bool ShouldUseDeathCoil(::Unit* target) const;
    void CastDeathCoil(::Unit* target);

    // Common spell constants
    enum CommonSpells
    {
        // Basic abilities
        DEATH_STRIKE = 49998,
        DEATH_COIL = 47541,
        DEATH_GRIP = 49576,
        DEATH_AND_DECAY = 43265,

        // Diseases
        PLAGUE_STRIKE = 45462,
        ICY_TOUCH = 45477,
        BLOOD_BOIL = 48721,

        // Buffs
        BONE_ARMOR = 49222,
        HORN_OF_WINTER = 57330,
        UNHOLY_PRESENCE = 48265,
        BLOOD_PRESENCE = 48266,
        FROST_PRESENCE = 48263,

        // Death Runes
        DEATH_RUNE_MASTERY = 49467
    };

private:
    Player* _bot;

    // Rune system (6 runes total: 2 Blood, 2 Frost, 2 Unholy)
    std::array<RuneInfo, 6> _runes;
    uint32 _lastRuneRegen;

    // Runic Power system
    uint32 _runicPower;
    uint32 _maxRunicPower;
    uint32 _lastRunicPowerDecay;

    // Disease tracking per target
    std::unordered_map<ObjectGuid, std::vector<DiseaseInfo>> _activeDiseases;
    uint32 _lastDiseaseUpdate;

    // Death and Decay tracking
    Position _deathAndDecayPos;
    uint32 _deathAndDecayRemaining;
    uint32 _lastDeathAndDecay;

    // Constants
    static constexpr uint32 RUNE_COOLDOWN = 10000; // 10 seconds
    static constexpr uint32 RUNIC_POWER_MAX = 130;
    static constexpr uint32 RUNIC_POWER_DECAY_RATE = 2; // per second out of combat
    static constexpr uint32 DISEASE_REFRESH_THRESHOLD = 6000; // 6 seconds
    static constexpr uint32 DEATH_AND_DECAY_DURATION = 30000; // 30 seconds
    static constexpr uint32 DEATH_AND_DECAY_COOLDOWN = 30000; // 30 seconds
};

} // namespace Playerbot