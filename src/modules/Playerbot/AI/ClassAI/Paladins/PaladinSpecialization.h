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

// Paladin specializations
enum class PaladinSpec : uint8
{
    HOLY = 0,
    PROTECTION = 1,
    RETRIBUTION = 2
};

// Paladin auras
enum class PaladinAura : uint8
{
    NONE = 0,
    DEVOTION = 1,
    RETRIBUTION_AURA = 2,
    CONCENTRATION = 3,
    SHADOW_RESISTANCE = 4,
    FROST_RESISTANCE = 5,
    FIRE_RESISTANCE = 6
};

// Base interface for paladin specializations
class TC_GAME_API PaladinSpecialization
{
public:
    explicit PaladinSpecialization(Player* bot);
    virtual ~PaladinSpecialization() = default;

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

    // Aura management
    virtual void UpdateAura() = 0;
    virtual PaladinAura GetOptimalAura() = 0;
    virtual void SwitchAura(PaladinAura aura) = 0;

    // Specialization info
    virtual PaladinSpec GetSpecialization() const = 0;
    virtual const char* GetSpecializationName() const = 0;

protected:
    Player* _bot;

    // Shared paladin utilities
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    bool ShouldConserveMana();

    // Shared paladin abilities
    void CastBlessingOfMight(::Unit* target);
    void CastBlessingOfWisdom(::Unit* target);
    void CastBlessingOfKings(::Unit* target);
    void CastSealOfRighteousness();
    void CastSealOfLight();
    void CastSealOfWisdom();
    void CastJudgement(::Unit* target);

    // Shared healing abilities
    void CastHolyLight(::Unit* target);
    void CastFlashOfLight(::Unit* target);
    void CastLayOnHands(::Unit* target);
    void CastDivineProtection();
    void CastDivineFavor();

    // Shared defensive abilities
    void CastDivineShield();
    void CastBlessingOfProtection(::Unit* target);
    void CastBlessingOfFreedom(::Unit* target);
    void CastCleanse(::Unit* target);

    // Shared utility
    bool IsChanneling();
    bool IsCasting();
    bool CanCastSpell();
    bool IsInDanger();
    std::vector<::Unit*> GetGroupMembers();
    PaladinAura GetCurrentAura();
    void ActivateAura(PaladinAura aura);

    // Common constants
    static constexpr float OPTIMAL_MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr float MINIMUM_SAFE_RANGE = 8.0f;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
    static constexpr float MANA_EMERGENCY_THRESHOLD = 0.15f;

    // Common spell IDs
    enum CommonSpells
    {
        // Blessings
        BLESSING_OF_MIGHT = 19740,
        BLESSING_OF_WISDOM = 19742,
        BLESSING_OF_KINGS = 20217,
        BLESSING_OF_PROTECTION = 1022,
        BLESSING_OF_FREEDOM = 1044,

        // Seals
        SEAL_OF_RIGHTEOUSNESS = 21084,
        SEAL_OF_LIGHT = 20165,
        SEAL_OF_WISDOM = 20166,
        SEAL_OF_JUSTICE = 20164,
        SEAL_OF_THE_CRUSADER = 21082,

        // Judgements
        JUDGEMENT = 20271,

        // Healing spells
        HOLY_LIGHT = 635,
        FLASH_OF_LIGHT = 19750,
        LAY_ON_HANDS = 633,
        DIVINE_FAVOR = 20216,

        // Defensive spells
        DIVINE_SHIELD = 642,
        DIVINE_PROTECTION = 498,
        CLEANSE = 4987,

        // Auras
        DEVOTION_AURA = 465,
        RETRIBUTION_AURA = 7294,
        CONCENTRATION_AURA = 19746,
        SHADOW_RESISTANCE_AURA = 19876,
        FROST_RESISTANCE_AURA = 19888,
        FIRE_RESISTANCE_AURA = 19891
    };
};

} // namespace Playerbot