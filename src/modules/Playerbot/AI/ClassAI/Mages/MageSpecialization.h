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

    // Helper positioning methods
    bool IsInCastingRange(::Unit* target, uint32 spellId);
    Position GetOptimalCastingPosition(::Unit* target);

    // Specialization info
    virtual MageSpec GetSpecialization() const { return MageSpec::ARCANE; }
    virtual const char* GetSpecializationName() const { return "Mage"; }

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
    void UseEvocation();
    void UseEmergencyAbilities();
    bool ShouldKite(::Unit* target);
    void PerformKiting(::Unit* target);
    void UseManaGem();
    void ConjureFood();
    void ConjureWater();
    void UpdateArcaneIntellect();
    ::Unit* GetBestPolymorphTarget();
    bool IsValidPolymorphTarget(::Unit* target);
    float CalculatePolymorphTargetScore(::Unit* target);

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