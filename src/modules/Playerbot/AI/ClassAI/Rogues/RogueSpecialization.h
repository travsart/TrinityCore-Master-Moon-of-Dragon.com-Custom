/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef ROGUE_SPECIALIZATION_H
#define ROGUE_SPECIALIZATION_H

#include "Define.h"

namespace Playerbot
{

/**
 * @brief Shared Rogue Spell IDs for all specializations
 *
 * This enum contains spell IDs used across Assassination, Outlaw, and Subtlety specs.
 * Spec-unique spells are defined in their respective Refactored header files.
 *
 * @note WoW 11.2 (The War Within) - Updated for modern spell IDs
 */
enum RogueSpells : uint32
{
    // ========================================================================
    // CORE ABILITIES (Shared across all specs)
    // ========================================================================
    SINISTER_STRIKE = 1752,
    BACKSTAB = 53,
    EVISCERATE = 2098,
    SLICE_AND_DICE = 5171,
    STEALTH = 1784,
    VANISH = 1856,
    SPRINT = 2983,
    EVASION = 5277,
    KICK = 1766,
    GOUGE = 1776,
    SAP = 6770,
    CHEAP_SHOT = 1833,
    KIDNEY_SHOT = 408,
    BLIND = 2094,
    DISTRACTION = 1725,
    PICK_POCKET = 921,
    PICK_LOCK = 1804,
    DETECT_TRAPS = 2836,
    DISARM_TRAP = 1842,
    EXPOSE_ARMOR = 8647,
    RUPTURE = 1943,
    GARROTE = 703,
    CLOAK_OF_SHADOWS = 31224,
    DISMANTLE = 51722,
    TRICKS_OF_THE_TRADE = 57934,
    FAN_OF_KNIVES = 51723,

    // ========================================================================
    // ASSASSINATION SPECIALIZATION
    // ========================================================================
    MUTILATE = 1329,
    ENVENOM = 32645,
    COLD_BLOOD = 14177,
    VENDETTA = 79140,
    HUNGER_FOR_BLOOD = 51662,
    OVERKILL = 58426,

    // Assassination Talents
    IMPROVED_SAP = 6687,
    RUTHLESSNESS = 14161,
    SEAL_FATE = 14186,
    VIGOR = 14983,
    LETHALITY = 14128,
    VILE_POISONS = 16513,
    MALICE = 14138,
    IMPROVED_EVISCERATE = 14162,
    RELENTLESS_STRIKES = 14179,
    IMPROVED_EXPOSE_ARMOR = 14168,
    IMPROVED_SLICE_AND_DICE = 14165,
    MASTER_POISONER = 58410,
    TURN_THE_TABLES = 51627,
    FIND_WEAKNESS = 91023,

    // ========================================================================
    // OUTLAW SPECIALIZATION (formerly Combat)
    // ========================================================================
    RIPOSTE = 14251,
    ADRENALINE_RUSH = 13750,
    BLADE_FLURRY = 13877,
    KILLING_SPREE = 51690,
    DEADLY_THROW = 48674,

    // Outlaw Talents
    COMBAT_EXPERTISE = 13741,
    IMPROVED_SINISTER_STRIKE = 13732,
    DEFLECTION = 13713,
    IMPROVED_BACKSTAB = 13743,
    DUAL_WIELD_SPECIALIZATION = 13715,
    IMPROVED_SPRINT = 13743,
    ENDURANCE = 13742,
    LIGHTNING_REFLEXES = 13712,
    IMPROVED_GOUGE = 13741,
    WEAPON_EXPERTISE = 13705,
    AGGRESSION = 18427,
    THROWING_SPECIALIZATION = 51698,
    MACE_SPECIALIZATION = 13709,
    SWORD_SPECIALIZATION = 13960,
    FIST_WEAPON_SPECIALIZATION = 31208,
    DAGGER_SPECIALIZATION = 13706,
    PRECISION = 13705,
    CLOSE_QUARTERS_COMBAT = 56814,
    SAVAGE_COMBAT = 51682,
    HACK_AND_SLASH = 13709,
    BLADE_TWISTING = 31124,
    VITALITY = 61329,
    UNFAIR_ADVANTAGE = 51672,
    IMPROVED_KICK = 13754,
    SURPRISE_ATTACKS = 32601,

    // ========================================================================
    // SUBTLETY SPECIALIZATION
    // ========================================================================
    SHADOWSTEP = 36554,
    PREPARATION = 14185,
    PREMEDITATION = 343160,
    AMBUSH = 8676,
    HEMORRHAGE = 16511,
    SHADOWSTRIKE = 185438,
    SHADOW_DANCE = 185313,
    SYMBOLS_OF_DEATH = 212283,

    // Subtlety Talents
    GHOST_STRIKE = 14278,
    IMPROVED_AMBUSH = 14079,
    CAMOUFLAGE = 13975,
    INITIATIVE = 13976,
    IMPROVED_STEALTH = 14076,
    MASTER_OF_DISGUISE = 31208,
    SLEIGHT_OF_HAND = 30892,
    DIRTY_FIGHTING = 14067,
    SERRATED_BLADES = 14171,
    HEIGHTENED_SENSES = 30894,
    SETUP = 13983,
    IMPROVED_CHEAP_SHOT = 14082,
    DEADLINESS = 30902,
    ENVELOPING_SHADOWS = 31216,
    SHADOW_MASTERY = 31221,
    IMPROVED_SHADOW_STEP = 31222,
    FILTHY_TRICKS = 31208,
    WAYLAY = 51692,
    HONOR_AMONG_THIEVES = 51701,
    MASTER_OF_SUBTLETY = 31223,
    OPPORTUNITY = 51672,
    SINISTER_CALLING = 31216,
    CHEAT_DEATH = 31230,
    FOCUSED_ATTACKS = 51634,

    // ========================================================================
    // RACIAL ABILITIES
    // ========================================================================
    SHADOWMELD = 58984,  // Night Elf racial stealth

    // ========================================================================
    // POISON SYSTEM (WoW 11.2 - The War Within)
    // ========================================================================

    // Lethal Poisons (Modern)
    DEADLY_POISON_MODERN = 2823,
    AMPLIFYING_POISON = 381664,
    INSTANT_POISON_MODERN = 315584,
    WOUND_POISON_MODERN = 8679,

    // Non-Lethal Poisons (Modern)
    CRIPPLING_POISON_MODERN = 3408,
    NUMBING_POISON = 5761,
    ATROPHIC_POISON = 381637,

    // Legacy Poisons (WotLK-era, kept for backward compatibility)
    POISON_WEAPON = 6499,
    INSTANT_POISON = 8681,
    DEADLY_POISON = 2823,
    WOUND_POISON = 8679,
    MIND_NUMBING_POISON = 5761,
    CRIPPLING_POISON = 3408,

    // Poison Ranks (Legacy)
    INSTANT_POISON_1 = 8681,
    INSTANT_POISON_2 = 8684,
    INSTANT_POISON_3 = 8685,
    INSTANT_POISON_4 = 11335,
    INSTANT_POISON_5 = 11336,
    INSTANT_POISON_6 = 11337,
    INSTANT_POISON_7 = 26785,
    INSTANT_POISON_8 = 26786,
    INSTANT_POISON_9 = 43230,
    INSTANT_POISON_10 = 43231,

    DEADLY_POISON_1 = 2823,
    DEADLY_POISON_2 = 2824,
    DEADLY_POISON_3 = 11355,
    DEADLY_POISON_4 = 11356,
    DEADLY_POISON_5 = 25349,
    DEADLY_POISON_6 = 26968,
    DEADLY_POISON_7 = 27187,
    DEADLY_POISON_8 = 43232,
    DEADLY_POISON_9 = 43233,

    WOUND_POISON_1 = 8679,
    WOUND_POISON_2 = 8680,
    WOUND_POISON_3 = 10022,
    WOUND_POISON_4 = 10023,
    WOUND_POISON_5 = 10024,

    MIND_NUMBING_POISON_1 = 5761,
    MIND_NUMBING_POISON_2 = 8692,
    MIND_NUMBING_POISON_3 = 11398,

    CRIPPLING_POISON_1 = 3408,
    CRIPPLING_POISON_2 = 11201,

    ANESTHETIC_POISON = 26785,
    PARALYTIC_POISON = 26969,

    // ========================================================================
    // BUFF/DEBUFF EFFECTS
    // ========================================================================
    TURN_THE_TABLES_EFFECT = 51627,
    MASTER_OF_SUBTLETY_EFFECT = 31665,
    SHADOWSTEP_EFFECT = 36563,
    INITIATIVE_EFFECT = 13980,
    COLD_BLOOD_EFFECT = 14177,
    ADRENALINE_RUSH_EFFECT = 13750,
    BLADE_FLURRY_EFFECT = 13877,
    PREPARATION_EFFECT = 14185,
    EVASION_EFFECT = 5277,
    SPRINT_EFFECT = 2983,
    VANISH_EFFECT = 11327,
    STEALTH_EFFECT = 1784,
    GHOST_STRIKE_EFFECT = 14278,
    RIPOSTE_EFFECT = 14251,

    // ========================================================================
    // IMPROVED/TALENT VARIANTS
    // ========================================================================
    IMPROVED_INSTANT_POISON = 14113,
    IMPROVED_DEADLY_POISON = 19216,
    IMPROVED_POISONS = 14113,
    SHADOW_CLONE = 36554
};

} // namespace Playerbot

#endif // ROGUE_SPECIALIZATION_H
