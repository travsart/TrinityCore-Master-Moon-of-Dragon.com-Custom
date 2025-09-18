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
    ObjectGuid sourceGuid;

    SoulFragment() : spawnTime(0), lifetime(0), isGreater(false), sourceGuid(ObjectGuid::Empty) {}

    SoulFragment(Position pos, bool greater = false)
        : position(pos), spawnTime(getMSTime()), lifetime(8000),
          isGreater(greater), sourceGuid(ObjectGuid::Empty) {}

    bool IsExpired() const { return (getMSTime() - spawnTime) > lifetime; }
};

// Base class for all Demon Hunter specializations
class TC_GAME_API DemonHunterSpecialization
{
public:
    explicit DemonHunterSpecialization(Player* bot);
    virtual ~DemonHunterSpecialization() = default;

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

    // Metamorphosis management
    virtual void UpdateMetamorphosis() = 0;
    virtual bool ShouldUseMetamorphosis() = 0;
    virtual void TriggerMetamorphosis() = 0;
    virtual MetamorphosisState GetMetamorphosisState() const = 0;

    // Soul fragment management
    virtual void UpdateSoulFragments() = 0;
    virtual void ConsumeSoulFragments() = 0;
    virtual bool ShouldConsumeSoulFragments() = 0;
    virtual uint32 GetAvailableSoulFragments() const = 0;

    // Specialization info
    virtual DemonHunterSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* GetBot() const { return _bot; }

    // Resource helpers
    uint32 GetFury() const;
    uint32 GetPain() const;
    void SpendFury(uint32 amount);
    void SpendPain(uint32 amount);
    void GenerateFury(uint32 amount);
    void GeneratePain(uint32 amount);

    // Soul fragment helpers
    void AddSoulFragment(Position pos, bool isGreater = false);
    void RemoveExpiredSoulFragments();
    std::vector<SoulFragment> GetNearbySoulFragments(float range = 20.0f) const;

    // Metamorphosis helpers
    bool IsInMetamorphosis() const;
    uint32 GetMetamorphosisRemaining() const;

    // Common demon hunter mechanics
    bool HasSigil(uint32 sigilType) const;
    void CastSigil(uint32 sigilSpellId, Position targetPos);
    bool ShouldUseDefensiveCooldown() const;

    // Common spell constants
    enum CommonSpells
    {
        // Basic abilities
        DEMONS_BITE = 162243,
        FELBLADE = 232893,
        THROW_GLAIVE = 185123,

        // Movement
        FEL_RUSH = 195072,
        VENGEFUL_RETREAT = 198793,
        INFERNAL_STRIKE = 189110,

        // Metamorphosis
        METAMORPHOSIS_HAVOC = 191427,
        METAMORPHOSIS_VENGEANCE = 187827,

        // Buffs/Debuffs
        IMMOLATION_AURA = 178740,
        DEMON_SPIKES = 203720,
        SOUL_CLEAVE = 228477
    };

private:
    Player* _bot;

    // Soul fragment tracking
    std::vector<SoulFragment> _soulFragments;
    uint32 _lastSoulFragmentUpdate;

    // Metamorphosis tracking
    MetamorphosisState _metamorphosisState;
    uint32 _metamorphosisRemaining;
    uint32 _lastMetamorphosisUpdate;

    // Common constants
    static constexpr uint32 SOUL_FRAGMENT_LIFETIME = 8000; // 8 seconds
    static constexpr uint32 SOUL_FRAGMENT_RANGE = 20.0f;
    static constexpr uint32 MAX_FURY = 120;
    static constexpr uint32 MAX_PAIN = 100;
};

} // namespace Playerbot