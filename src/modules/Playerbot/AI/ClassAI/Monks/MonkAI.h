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
#include <memory>

namespace Playerbot
{

// Forward declarations
class MonkSpecialization;

// Monk specializations
enum class MonkSpec : uint8
{
    BREWMASTER = 0,
    MISTWEAVER = 1,
    WINDWALKER = 2
};

// Monk AI implementation using specialization pattern
class TC_GAME_API MonkAI : public ClassAI
{
public:
    explicit MonkAI(Player* bot);
    ~MonkAI() = default;

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
    // Specialization detection and management
    MonkSpec DetectSpecialization();
    void InitializeSpecialization();
    MonkSpec GetCurrentSpecialization() const;

    // Specialization delegation
    std::unique_ptr<MonkSpecialization> _specialization;
    MonkSpec _detectedSpec;

    // Spell IDs for detection
    enum MonkSpells
    {
        // Brewmaster specific
        KEG_SMASH = 121253,
        IRONSKIN_BREW = 115308,
        PURIFYING_BREW = 119582,
        BREATH_OF_FIRE = 115181,
        FORTIFYING_BREW = 115203,

        // Mistweaver specific
        VIVIFY = 116670,
        ENVELOPING_MIST = 124682,
        ESSENCE_FONT = 191837,
        RENEWING_MIST = 115151,
        SOOTHING_MIST = 115175,

        // Windwalker specific
        FISTS_OF_FURY = 113656,
        WHIRLING_DRAGON_PUNCH = 152175,
        STORM_EARTH_AND_FIRE = 137639,
        RISING_SUN_KICK = 107428,
        TOUCH_OF_DEATH = 115080,

        // Shared abilities
        TIGER_PALM = 100780,
        BLACKOUT_KICK = 100784
    };
};

} // namespace Playerbot