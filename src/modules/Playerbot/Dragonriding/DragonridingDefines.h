/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include <cstdint>

namespace Playerbot
{
namespace Dragonriding
{

// ============================================================================
// SPELL IDS
// ============================================================================

// Soar - Dracthyr Evoker racial ability
constexpr uint32 SPELL_SOAR = 369536;

// Glide - Evoker glide ability (shares cooldown with Soar)
constexpr uint32 SPELL_GLIDE = 358733;

// Custom spells for dragonriding system (use 900000+ range to avoid conflicts)
constexpr uint32 SPELL_VIGOR_BUFF = 900001;           // Hidden aura tracking vigor stacks
constexpr uint32 SPELL_SURGE_FORWARD = 900002;        // Speed boost ability
constexpr uint32 SPELL_SKYWARD_ASCENT = 900003;       // Vertical thrust ability
constexpr uint32 SPELL_WHIRLING_SURGE = 900004;       // Barrel roll (talent-locked)
constexpr uint32 SPELL_AERIAL_HALT = 900005;          // Hover in place (talent-locked)
constexpr uint32 SPELL_SURGE_FORWARD_BUFF = 900006;   // Speed boost effect
constexpr uint32 SPELL_SKYWARD_ASCENT_BUFF = 900007;  // Vertical thrust effect
constexpr uint32 SPELL_THRILL_OF_THE_SKIES = 900008;  // High-speed flying regen buff
constexpr uint32 SPELL_GROUND_SKIMMING = 900009;      // Near-ground flying regen buff

// ============================================================================
// FLIGHT CAPABILITY IDS
// From FlightCapability.db2 - these activate dragonriding physics
// ============================================================================

// Standard dragonriding flight capability ID
// Query sFlightCapabilityStore at runtime to find valid IDs
constexpr uint32 FLIGHT_CAPABILITY_SOAR = 1;          // Standard dragonriding physics
constexpr uint32 FLIGHT_CAPABILITY_NORMAL = 0;        // Normal flying (no dragonriding)

// ============================================================================
// BASE VALUES (Retail Accurate - No Talents)
// These are the starting values before any talents are learned
// ============================================================================

// Vigor
constexpr uint32 BASE_MAX_VIGOR = 3;                  // Starting vigor capacity
constexpr uint32 MAX_MAX_VIGOR = 6;                   // Maximum achievable vigor

// Regeneration rates (milliseconds per vigor point)
constexpr uint32 BASE_REGEN_GROUNDED_MS = 30000;      // 30 seconds while grounded
constexpr uint32 BASE_REGEN_FLYING_MS = 15000;        // 15 seconds while flying fast
constexpr uint32 BASE_REGEN_GROUND_SKIM_MS = 30000;   // 30 seconds near ground

// Fully upgraded values (for reference and config override mode)
constexpr uint32 UPGRADED_REGEN_GROUNDED_MS = 15000;  // 15 seconds fully upgraded
constexpr uint32 UPGRADED_REGEN_FLYING_MS = 5000;     // 5 seconds fully upgraded

// ============================================================================
// TALENT IDS
// Matches DragonridingTalent enum in DragonridingMgr.h
// ============================================================================

enum class DragonridingTalentId : uint32
{
    // Invalid/None
    NONE = 0,

    // Vigor Capacity Branch (Left)
    TAKE_TO_THE_SKIES = 1,       // 4 vigor (1 glyph)
    DRAGONRIDING_LEARNER = 2,    // 5 vigor (4 glyphs)
    BEYOND_INFINITY = 3,         // 6 vigor (5 glyphs)

    // Vigor Regen (Grounded) Branch (Middle)
    DYNAMIC_STRETCHING = 10,     // 25s (3 glyphs)
    RESTORATIVE_TRAVELS = 11,    // 20s (4 glyphs)
    YEARNING_FOR_THE_SKY = 12,   // 15s (5 glyphs)

    // Vigor Regen (Flying) Branch (Right)
    THRILL_CHASER = 20,          // 10s (3 glyphs)
    THRILL_SEEKER = 21,          // 5s (5 glyphs)

    // Utility & Abilities
    GROUND_SKIMMING = 30,        // 30s near ground (4 glyphs)
    AIRBORNE_TUMBLING = 31,      // Unlocks Whirling Surge (3 glyphs)
    AT_HOME_ALOFT = 32,          // Unlocks Aerial Halt (2 glyphs)

    MAX_TALENT = 33
};

// ============================================================================
// TALENT COSTS (Dragon Glyphs Required)
// ============================================================================

struct TalentCost
{
    DragonridingTalentId talentId;
    uint32 glyphCost;
    DragonridingTalentId prerequisite;
    uint32 effectValue;  // New vigor/regen value or spell ID
};

// Retail-accurate talent costs
constexpr TalentCost TALENT_COSTS[] = {
    // Vigor capacity branch
    { DragonridingTalentId::TAKE_TO_THE_SKIES, 1, DragonridingTalentId::NONE, 4 },
    { DragonridingTalentId::DRAGONRIDING_LEARNER, 4, DragonridingTalentId::TAKE_TO_THE_SKIES, 5 },
    { DragonridingTalentId::BEYOND_INFINITY, 5, DragonridingTalentId::DRAGONRIDING_LEARNER, 6 },

    // Grounded regen branch
    { DragonridingTalentId::DYNAMIC_STRETCHING, 3, DragonridingTalentId::NONE, 25000 },
    { DragonridingTalentId::RESTORATIVE_TRAVELS, 4, DragonridingTalentId::DYNAMIC_STRETCHING, 20000 },
    { DragonridingTalentId::YEARNING_FOR_THE_SKY, 5, DragonridingTalentId::RESTORATIVE_TRAVELS, 15000 },

    // Flying regen branch
    { DragonridingTalentId::THRILL_CHASER, 3, DragonridingTalentId::NONE, 10000 },
    { DragonridingTalentId::THRILL_SEEKER, 5, DragonridingTalentId::THRILL_CHASER, 5000 },

    // Utility & abilities
    { DragonridingTalentId::GROUND_SKIMMING, 4, DragonridingTalentId::NONE, 30000 },
    { DragonridingTalentId::AIRBORNE_TUMBLING, 3, DragonridingTalentId::NONE, SPELL_WHIRLING_SURGE },
    { DragonridingTalentId::AT_HOME_ALOFT, 2, DragonridingTalentId::NONE, SPELL_AERIAL_HALT },
};

constexpr size_t TALENT_COUNT = sizeof(TALENT_COSTS) / sizeof(TalentCost);

// ============================================================================
// GLYPH CONFIGURATION
// ============================================================================

// Total number of glyphs across all Dragon Isles zones
constexpr uint32 TOTAL_GLYPHS = 74;  // 16 + 12 + 14 + 14 + 8 + 10

// Zone IDs for Dragon Isles
constexpr uint32 MAP_DRAGON_ISLES = 2444;
constexpr uint32 ZONE_WAKING_SHORES = 13644;
constexpr uint32 ZONE_OHNAHRAN_PLAINS = 13645;
constexpr uint32 ZONE_AZURE_SPAN = 13646;
constexpr uint32 ZONE_THALDRASZUS = 13647;
constexpr uint32 ZONE_FORBIDDEN_REACH = 14022;
constexpr uint32 ZONE_ZARALEK_CAVERN = 14529;

// Glyph collection radius (yards)
constexpr float GLYPH_COLLECTION_RADIUS = 10.0f;

// ============================================================================
// PHYSICS THRESHOLDS
// ============================================================================

// Speed threshold to trigger "Thrill of the Skies" fast regen (% of max velocity)
constexpr float THRILL_SPEED_THRESHOLD = 0.70f;  // 70% of max velocity

// Height threshold for "Ground Skimming" regen (yards above ground)
constexpr float GROUND_SKIM_HEIGHT = 10.0f;

// ============================================================================
// UPDATE INTERVALS
// ============================================================================

// How often to check vigor regeneration conditions (ms)
constexpr uint32 VIGOR_UPDATE_INTERVAL_MS = 1000;  // 1 second

// How often to check glyph collection proximity (ms)
constexpr uint32 GLYPH_CHECK_INTERVAL_MS = 500;    // 0.5 seconds

// ============================================================================
// CONFIG KEYS (worldserver.conf)
// ============================================================================

constexpr const char* CONFIG_DRAGONRIDING_ENABLED = "Playerbot.GameSystems.Dragonriding.Enable";
constexpr const char* CONFIG_PROGRESSION_ENABLED = "Playerbot.GameSystems.Dragonriding.ProgressionEnabled";
constexpr const char* CONFIG_STARTING_GLYPHS = "Playerbot.GameSystems.Dragonriding.StartingGlyphs";
constexpr const char* CONFIG_BOT_AUTO_BOOST = "Playerbot.GameSystems.Dragonriding.Bot.AutoBoost";
constexpr const char* CONFIG_THRILL_SPEED_THRESHOLD = "Playerbot.GameSystems.Dragonriding.ThrillSpeedThreshold";
constexpr const char* CONFIG_GROUND_SKIM_HEIGHT = "Playerbot.GameSystems.Dragonriding.GroundSkimHeight";
constexpr const char* CONFIG_UPDATE_INTERVAL = "Playerbot.GameSystems.Dragonriding.UpdateInterval";
constexpr const char* CONFIG_GLYPH_CHECK_INTERVAL = "Playerbot.GameSystems.Dragonriding.GlyphCheckInterval";

// ============================================================================
// RACE/CLASS RESTRICTIONS
// ============================================================================

// Dracthyr race IDs (from RaceMask.h)
constexpr uint32 RACE_DRACTHYR_ALLIANCE = 52;
constexpr uint32 RACE_DRACTHYR_HORDE = 70;

// Evoker class ID (from SharedDefines.h)
constexpr uint32 CLASS_EVOKER = 13;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

inline bool IsDracthyr(uint8 race)
{
    return race == RACE_DRACTHYR_ALLIANCE || race == RACE_DRACTHYR_HORDE;
}

inline bool IsEvoker(uint8 playerClass)
{
    return playerClass == CLASS_EVOKER;
}

inline bool CanUseSoar(uint8 race, uint8 playerClass)
{
    // Soar is a Dracthyr Evoker racial ability
    return IsDracthyr(race) && IsEvoker(playerClass);
}

inline const TalentCost* GetTalentCost(DragonridingTalentId talentId)
{
    for (size_t i = 0; i < TALENT_COUNT; ++i)
    {
        if (TALENT_COSTS[i].talentId == talentId)
            return &TALENT_COSTS[i];
    }
    return nullptr;
}

} // namespace Dragonriding
} // namespace Playerbot
