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
#include <string>
#include <vector>
#include <unordered_map>

class Player;

namespace Playerbot
{

// Forward declarations
struct ZonePlacement;
struct ZoneChoice;

/**
 * @brief Positioner statistics
 */
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

/**
 * @brief Interface for Bot World Positioner
 *
 * Automated zone placement for bots with:
 * - Starter zone teleportation (L1-4 bots)
 * - Level-appropriate zone selection (L5+ bots)
 * - Faction and race-specific placement
 * - Safe coordinate validation
 * - Lock-free zone cache
 *
 * Thread Safety: Selection methods are thread-safe, teleportation requires main thread
 */
class TC_GAME_API IBotWorldPositioner
{
public:
    virtual ~IBotWorldPositioner() = default;

    // ====================================================================
    // INITIALIZATION
    // ====================================================================

    /**
     * @brief Load zone placements from configuration
     * @return true if successful, false otherwise
     */
    virtual bool LoadZones() = 0;

    /**
     * @brief Reload zones for hot-reload
     */
    virtual void ReloadZones() = 0;

    /**
     * @brief Check if zones are ready
     * @return true if initialized, false otherwise
     */
    virtual bool IsReady() const = 0;

    // ====================================================================
    // ZONE SELECTION (Thread-safe)
    // ====================================================================

    /**
     * @brief Select zone for bot based on level and faction
     * @param level Bot level
     * @param faction TEAM_ALLIANCE or TEAM_HORDE
     * @param race Bot race (for starter zone selection)
     * @return ZoneChoice with selected zone
     */
    virtual ZoneChoice SelectZone(uint32 level, TeamId faction, uint8 race = 0) = 0;

    /**
     * @brief Get starter zone for specific race
     * @param race Player race
     * @param faction Faction for validation
     * @return ZoneChoice for starter zone
     */
    virtual ZoneChoice GetStarterZone(uint8 race, TeamId faction) = 0;

    /**
     * @brief Get all zones valid for level and faction
     * @param level Bot level
     * @param faction Bot faction
     * @return Vector of valid zone placements
     */
    virtual std::vector<ZonePlacement const*> GetValidZones(uint32 level, TeamId faction) const = 0;

    /**
     * @brief Get random capital city for faction
     * @param faction TEAM_ALLIANCE or TEAM_HORDE
     * @return ZoneChoice for capital city
     */
    virtual ZoneChoice GetCapitalCity(TeamId faction) = 0;

    // ====================================================================
    // TELEPORTATION (MAIN THREAD ONLY)
    // ====================================================================

    /**
     * @brief Teleport bot to selected zone
     * @param bot Bot player object
     * @param placement Zone placement from SelectZone()
     * @return true if successful, false otherwise
     */
    virtual bool TeleportToZone(Player* bot, ZonePlacement const* placement) = 0;

    /**
     * @brief Complete workflow: Select zone + teleport
     * @param bot Bot player object
     * @param level Bot level
     * @param faction Bot faction
     * @param race Bot race
     * @return true if successful, false otherwise
     */
    virtual bool PlaceBot(Player* bot, uint32 level, TeamId faction, uint8 race) = 0;

    // ====================================================================
    // ZONE QUERIES (Thread-safe)
    // ====================================================================

    /**
     * @brief Get zone placement by zone ID
     * @param zoneId Zone ID
     * @return Pointer to zone placement, or nullptr if not found
     */
    virtual ZonePlacement const* GetZonePlacement(uint32 zoneId) const = 0;

    /**
     * @brief Get zone name by zone ID
     * @param zoneId Zone ID
     * @return Zone name or empty string
     */
    virtual std::string GetZoneName(uint32 zoneId) const = 0;

    /**
     * @brief Check if zone is valid for level/faction
     * @param zoneId Zone ID
     * @param level Bot level
     * @param faction Bot faction
     * @return true if valid, false otherwise
     */
    virtual bool IsZoneValid(uint32 zoneId, uint32 level, TeamId faction) const = 0;

    // ====================================================================
    // STATISTICS & DEBUGGING
    // ====================================================================

    /**
     * @brief Get positioner statistics
     * @return Statistics structure
     */
    virtual PositionerStats GetStats() const = 0;

    /**
     * @brief Print zone report to console
     */
    virtual void PrintZoneReport() const = 0;

    /**
     * @brief Get formatted zone summary
     * @return Summary string
     */
    virtual std::string GetZoneSummary() const = 0;
};

} // namespace Playerbot
