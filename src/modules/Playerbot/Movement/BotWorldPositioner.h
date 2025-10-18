/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include "SharedDefines.h"
#include "Position.h"
#include <atomic>
#include <vector>
#include <unordered_map>
#include <string>

class Player;

namespace Playerbot
{

/**
 * Zone Placement Data
 * Represents a spawn location for bots at specific level ranges
 */
struct ZonePlacement
{
    uint32 zoneId;          // Zone ID from AreaTable.dbc
    uint32 mapId;           // Map ID for teleportation
    float x;                // World coordinates
    float y;
    float z;
    float orientation;
    uint32 minLevel;        // Minimum level for this zone
    uint32 maxLevel;        // Maximum level for this zone
    TeamId faction;         // TEAM_ALLIANCE, TEAM_HORDE, or TEAM_NEUTRAL
    std::string zoneName;   // Human-readable name
    bool isStarterZone;     // True for level 1-4 zones

    bool IsValidForLevel(uint32 level) const
    {
        return level >= minLevel && level <= maxLevel;
    }

    bool IsValidForFaction(TeamId factionTeam) const
    {
        return faction == TEAM_NEUTRAL || faction == factionTeam;
    }

    Position GetPosition() const
    {
        return Position(x, y, z, orientation);
    }
};

/**
 * Zone Selection Result
 * Returned when selecting a zone for bot placement
 */
struct ZoneChoice
{
    ZonePlacement const* placement;
    float suitability;  // 0.0-1.0, how suitable the zone is

    ZoneChoice() : placement(nullptr), suitability(0.0f) {}
    explicit ZoneChoice(ZonePlacement const* p, float suit = 1.0f)
        : placement(p), suitability(suit) {}

    bool IsValid() const { return placement != nullptr; }
};

/**
 * Bot World Positioner - Automated Zone Placement for Bots
 *
 * Purpose: Place bots in level-appropriate zones during world population
 *
 * Features:
 * - Starter zone teleportation (L1-4 bots)
 * - Level-appropriate zone selection (L5+ bots)
 * - Faction-specific placement
 * - Race-specific starter zones
 * - Safe coordinates (verified spawn points)
 * - Immutable zone cache (lock-free reads)
 *
 * Integration:
 * - Uses TrinityCore's AreaTable.dbc for zone data
 * - Uses Player::TeleportTo() for positioning (main thread only)
 * - Compatible with ThreadPool worker threads (cache queries)
 *
 * Thread Safety:
 * - Immutable zone cache after LoadZones()
 * - Lock-free concurrent reads
 * - Atomic initialization flag
 *
 * Performance:
 * - Zone cache build: <1 second
 * - Zone selection: <0.05ms per bot
 * - Teleportation: <1ms per bot (TrinityCore API)
 *
 * Usage Workflow (Two-Phase Bot Creation):
 * 1. Worker Thread: SelectZone() - Choose zone based on level/faction
 * 2. Main Thread: TeleportToZone() - TrinityCore Player API
 *
 * Zone Categories:
 * - Starter Zones (L1-10): Race-specific starting areas
 * - Leveling Zones (L10-60): Open world questing zones
 * - Endgame Zones (L60-80): The War Within content zones
 * - Capital Cities (All levels): Faction capitals
 */
class TC_GAME_API BotWorldPositioner
{
public:
    static BotWorldPositioner* instance();

    // ====================================================================
    // INITIALIZATION (Called once at server startup)
    // ====================================================================

    /**
     * Load zone placements from configuration/database
     * MUST be called before any zone operations
     * Single-threaded execution required
     */
    bool LoadZones();

    /**
     * Reload zones (for hot-reload during development)
     */
    void ReloadZones();

    /**
     * Check if zones are ready
     */
    bool IsReady() const
    {
        return _initialized.load(std::memory_order_acquire);
    }

    // ====================================================================
    // ZONE SELECTION (Thread-safe, for worker threads)
    // ====================================================================

    /**
     * Select zone for bot based on level and faction
     * Thread-safe, can be called from worker threads
     *
     * Selection Logic:
     * - L1-4: Starter zones (race-specific if available)
     * - L5-10: Starter regions
     * - L11-60: Leveling zones (weighted by level range)
     * - L61-80: Endgame zones (The War Within content)
     *
     * @param level         Bot level
     * @param faction       TEAM_ALLIANCE or TEAM_HORDE
     * @param race          Bot race (for starter zone selection)
     * @return              ZoneChoice with selected zone
     */
    ZoneChoice SelectZone(uint32 level, TeamId faction, uint8 race = 0);

    /**
     * Get starter zone for specific race
     * Used for L1-4 bots
     *
     * @param race          Player race
     * @param faction       Faction (for validation)
     * @return              ZoneChoice for starter zone
     */
    ZoneChoice GetStarterZone(uint8 race, TeamId faction);

    /**
     * Get all zones valid for level and faction
     * Useful for debugging and validation
     */
    std::vector<ZonePlacement const*> GetValidZones(uint32 level, TeamId faction) const;

    /**
     * Get random capital city for faction
     * Fallback option when no other zone is suitable
     */
    ZoneChoice GetCapitalCity(TeamId faction);

    // ====================================================================
    // TELEPORTATION (MAIN THREAD ONLY - Player API)
    // ====================================================================

    /**
     * Teleport bot to selected zone
     * MUST be called from main thread (Player API)
     *
     * Workflow:
     * 1. Get zone coordinates from placement
     * 2. Call Player::TeleportTo()
     * 3. Log successful placement
     *
     * NOTE: Call AFTER bot is fully initialized (gear, talents, etc.)
     *
     * @param bot           Bot player object
     * @param placement     Zone placement from SelectZone()
     * @return              True if successful
     */
    bool TeleportToZone(Player* bot, ZonePlacement const* placement);

    /**
     * Complete workflow: Select zone + teleport in one call
     * MUST be called from main thread
     *
     * @param bot           Bot player object
     * @param level         Bot level
     * @param faction       Bot faction
     * @param race          Bot race
     * @return              True if successful
     */
    bool PlaceBot(Player* bot, uint32 level, TeamId faction, uint8 race);

    // ====================================================================
    // ZONE QUERIES (Thread-safe, lock-free cache access)
    // ====================================================================

    /**
     * Get zone placement by zone ID
     * Thread-safe, returns cached placement
     */
    ZonePlacement const* GetZonePlacement(uint32 zoneId) const;

    /**
     * Get zone name by zone ID
     */
    std::string GetZoneName(uint32 zoneId) const;

    /**
     * Check if zone is valid for level/faction
     */
    bool IsZoneValid(uint32 zoneId, uint32 level, TeamId faction) const;

    // ====================================================================
    // STATISTICS & DEBUGGING
    // ====================================================================

    struct PositionerStats
    {
        uint32 totalZones{0};
        uint32 starterZones{0};
        uint32 levelingZones{0};
        uint32 endgameZones{0};
        uint32 capitalCities{0};
        uint32 botsPlaced{0};
        uint32 teleportsFailed{0};
        std::unordered_map<uint32, uint32> placementsPerZone;  // zoneId -> count
    };

    PositionerStats GetStats() const { return _stats; }
    void PrintZoneReport() const;
    std::string GetZoneSummary() const;

private:
    BotWorldPositioner() = default;
    ~BotWorldPositioner() = default;
    BotWorldPositioner(BotWorldPositioner const&) = delete;
    BotWorldPositioner& operator=(BotWorldPositioner const&) = delete;

    // ====================================================================
    // ZONE MANAGEMENT
    // ====================================================================

    void LoadZonesFromConfig();
    void LoadZonesFromDatabase();  // Future: DB-driven zone configuration
    void BuildDefaultZones();      // Fallback if config is empty
    void ValidateZones();
    void BuildZoneCache();         // Build lookup structures

    // ====================================================================
    // ZONE SELECTION HELPERS
    // ====================================================================

    ZoneChoice SelectByLevelRange(uint32 level, TeamId faction) const;
    ZoneChoice SelectWeighted(std::vector<ZonePlacement const*> const& candidates) const;
    float CalculateSuitability(ZonePlacement const* zone, uint32 level) const;

    // ====================================================================
    // RACE TO ZONE MAPPING (Starter Zones)
    // ====================================================================

    void BuildRaceZoneMapping();
    uint32 GetStarterZoneForRace(uint8 race) const;

    // ====================================================================
    // TELEPORTATION HELPERS
    // ====================================================================

    bool ValidateTeleportCoordinates(ZonePlacement const* placement) const;
    void LogPlacement(Player* bot, ZonePlacement const* placement);

    // ====================================================================
    // DATA STORAGE
    // ====================================================================

    // Master zone list (immutable after LoadZones)
    std::vector<ZonePlacement> _zones;

    // Quick lookups (immutable after BuildZoneCache)
    std::unordered_map<uint32, ZonePlacement const*> _zoneById;           // zoneId -> placement
    std::unordered_map<uint32, std::vector<ZonePlacement const*>> _zonesByLevel;  // level -> placements
    std::unordered_map<uint8, std::vector<ZonePlacement const*>> _starterZonesByRace;  // race -> placements

    // Faction-specific lookups
    std::vector<ZonePlacement const*> _allianceCapitals;
    std::vector<ZonePlacement const*> _hordeCapitals;

    // Statistics
    PositionerStats _stats;

    // Initialization flag
    std::atomic<bool> _initialized{false};
};

} // namespace Playerbot

#define sBotWorldPositioner Playerbot::BotWorldPositioner::instance()
