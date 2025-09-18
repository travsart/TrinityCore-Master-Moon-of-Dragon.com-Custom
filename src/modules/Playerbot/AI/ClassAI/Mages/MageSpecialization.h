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
#include <vector>

// Forward declarations
class Player;
class Unit;

namespace Playerbot
{

// Mage specializations
enum class MageSpec : uint8
{
    ARCANE = 0,
    FIRE = 1,
    FROST = 2
};

// Base interface for mage specializations
class TC_GAME_API MageSpecialization
{
public:
    explicit MageSpecialization(Player* bot);
    virtual ~MageSpecialization() = default;

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

    // Specialization info
    virtual MageSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* _bot;

    // Shared mage utilities
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    bool ShouldConserveMana();

    // Shared defensive abilities
    void UseBlink();
    void UseIceBlock();
    void UseManaShield();

    // Shared crowd control
    void UsePolymorph(::Unit* target);
    void UseCounterspell(::Unit* target);
    void UseFrostNova();

    // Shared utility
    bool IsChanneling();
    bool IsCasting();
    bool CanCastSpell();
    bool IsInDanger();

    // Common constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr float MINIMUM_SAFE_RANGE = 15.0f;
    static constexpr float KITING_RANGE = 20.0f;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
    static constexpr float MANA_EMERGENCY_THRESHOLD = 0.15f;

    // Common spell IDs
    enum CommonSpells
    {
        BLINK = 1953,
        ICE_BLOCK = 45438,
        MANA_SHIELD = 1463,
        POLYMORPH = 118,
        COUNTERSPELL = 2139,
        FROST_NOVA = 122,
        ARCANE_INTELLECT = 1459
    };
};

} // namespace Playerbot