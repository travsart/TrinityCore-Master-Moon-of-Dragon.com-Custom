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
#include "DiseaseManager.h"
#include "DeathKnightTypes.h"
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

// Disease types defined in DiseaseManager.h

// Disease tracking defined in DiseaseManager.h

// Base class for all Death Knight specializations
class TC_GAME_API DeathKnightSpecialization
{
public:
    explicit DeathKnightSpecialization(Player* bot);
    virtual ~DeathKnightSpecialization() = default;

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
    virtual float GetOptimalRange(::Unit* target) { return 0.0f; }

    // Rune management
    virtual void UpdateRuneManagement() {}
    virtual bool HasAvailableRunes(RuneType type, uint32 count = 1) { return false; }
    virtual void ConsumeRunes(RuneType type, uint32 count = 1) {}
    virtual uint32 GetAvailableRunes(RuneType type) const { return 0; }

    // Runic Power management
    virtual void UpdateRunicPowerManagement() {}
    virtual void GenerateRunicPower(uint32 amount) {}
    virtual void SpendRunicPower(uint32 amount) {}
    virtual uint32 GetRunicPower() const { return 0; }
    virtual bool HasEnoughRunicPower(uint32 required) const { return false; }

    // Disease management
    virtual void UpdateDiseaseManagement() {}
    virtual void ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId) {}
    virtual bool HasDisease(::Unit* target, DiseaseType type) const { return false; }
    virtual bool ShouldApplyDisease(::Unit* target, DiseaseType type) const { return false; }
    virtual void RefreshExpringDiseases() {}

    // Death and Decay management
    virtual void UpdateDeathAndDecay() {}
    virtual bool ShouldCastDeathAndDecay() const { return false; }
    virtual void CastDeathAndDecay(Position targetPos) {}

    // Specialization info
    virtual DeathKnightSpec GetSpecialization() const { return DeathKnightSpec::BLOOD; }
    virtual const char* GetSpecializationName() const { return "Death Knight"; }

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
        PESTILENCE = 50842,

        // Buffs
        BONE_ARMOR = 195181, // Updated for WoW 11.2
        HORN_OF_WINTER = 57330,
        UNHOLY_PRESENCE = 48265,
        BLOOD_PRESENCE = 48266,
        FROST_PRESENCE = 48263,

        // Death Runes
        DEATH_RUNE_MASTERY = 49467,

        // Additional Death Knight abilities
        DEATH_PACT = 48743,
        MIND_FREEZE = 47528,
        RAISE_DEAD = 46584,
        ANTI_MAGIC_SHELL = 48707,

        // Additional spell constants
        UNHOLY_FRENZY = 49016,
        BLOOD_STRIKE = 45902
    };

protected:
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