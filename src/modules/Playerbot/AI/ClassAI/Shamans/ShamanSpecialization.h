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
#include <vector>
#include <array>
#include <unordered_map>

// Forward declarations
class Player;
class Unit;

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

// Base specialization interface for all Shaman specs
class TC_GAME_API ShamanSpecialization
{
public:
    explicit ShamanSpecialization(Player* bot) : _bot(bot) {}
    virtual ~ShamanSpecialization() = default;

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

    // Totem management - core to all shaman specs
    virtual void UpdateTotemManagement() = 0;
    virtual void DeployOptimalTotems() = 0;
    virtual uint32 GetOptimalFireTotem() = 0;
    virtual uint32 GetOptimalEarthTotem() = 0;
    virtual uint32 GetOptimalWaterTotem() = 0;
    virtual uint32 GetOptimalAirTotem() = 0;

    // Shock rotation - available to all specs
    virtual void UpdateShockRotation(::Unit* target) = 0;
    virtual uint32 GetNextShockSpell(::Unit* target) = 0;

    // Specialization info
    virtual ShamanSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* GetBot() const { return _bot; }

    // Shared totem management
    std::array<TotemInfo, 4> _activeTotems;
    std::unordered_map<TotemType, uint32> _totemCooldowns;
    uint32 _lastTotemUpdate;

    // Shared shock tracking
    uint32 _lastEarthShock;
    uint32 _lastFlameShock;
    uint32 _lastFrostShock;
    uint32 _shockCooldown;

    // Common methods available to all specializations
    void DeployTotem(TotemType type, uint32 spellId);
    void RecallTotem(TotemType type);
    bool IsTotemActive(TotemType type);
    uint32 GetTotemRemainingTime(TotemType type);
    Position GetOptimalTotemPosition(TotemType type);

    void CastEarthShock(::Unit* target);
    void CastFlameShock(::Unit* target);
    void CastFrostShock(::Unit* target);
    bool IsShockOnCooldown();

    // Shared spell IDs
    enum SharedSpells
    {
        // Shock spells
        EARTH_SHOCK = 8042,
        FLAME_SHOCK = 8050,
        FROST_SHOCK = 8056,

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

        // Shield spells
        LIGHTNING_SHIELD = 324,
        WATER_SHIELD = 52127,
        EARTH_SHIELD = 974,

        // Utility spells
        PURGE = 370,
        HEX = 51514,
        BLOODLUST = 2825,
        HEROISM = 32182,
        GHOST_WOLF = 2645
    };

private:
    Player* _bot;
};

} // namespace Playerbot