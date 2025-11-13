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
#include "ObjectGuid.h"
#include <string>
#include <vector>

namespace Playerbot
{

// Forward declarations for types defined in BotSpawner.h
struct SpawnRequest;
struct SpawnConfig;
struct SpawnStats;
struct ZonePopulation;

/**
 * @brief Interface for Bot Spawner
 *
 * Manages bot spawning, population management, and zone distribution with:
 * - Single and batch bot spawning
 * - Population-based bot management
 * - Zone and map population tracking
 * - Adaptive throttling and resource monitoring
 * - Statistics and performance metrics
 *
 * Thread Safety: Methods are thread-safe unless noted otherwise
 */
class TC_GAME_API IBotSpawner
{
public:
    virtual ~IBotSpawner() = default;

    // ====================================================================
    // INITIALIZATION & LIFECYCLE
    // ====================================================================

    /**
     * @brief Initialize spawner system
     * @return true if successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown spawner system
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Update spawner (called periodically)
     * @param diff Time since last update in milliseconds
     */
    virtual void Update(uint32 diff) = 0;

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    /**
     * @brief Load configuration from file
     */
    virtual void LoadConfig() = 0;

    /**
     * @brief Get current configuration
     * @return Reference to spawn configuration
     */
    virtual SpawnConfig const& GetConfig() const = 0;

    /**
     * @brief Set configuration
     * @param config New spawn configuration
     */
    virtual void SetConfig(SpawnConfig const& config) = 0;

    // ====================================================================
    // BOT SPAWNING
    // ====================================================================

    /**
     * @brief Spawn single bot
     * @param request Spawn request with bot parameters
     * @return true if successful, false otherwise
     */
    virtual bool SpawnBot(SpawnRequest const& request) = 0;

    /**
     * @brief Spawn multiple bots in batch
     * @param requests Vector of spawn requests
     * @return Number of bots successfully spawned
     */
    virtual uint32 SpawnBots(std::vector<SpawnRequest> const& requests) = 0;

    /**
     * @brief Create new bot character and spawn it
     * @param masterAccountId Account ID
     * @param classId Character class
     * @param race Character race
     * @param gender Character gender
     * @param name Character name
     * @param outCharacterGuid Output parameter for created character GUID
     * @return true if successful, false otherwise
     */
    virtual bool CreateAndSpawnBot(uint32 masterAccountId, uint8 classId, uint8 race, uint8 gender, std::string const& name, ObjectGuid& outCharacterGuid) = 0;

    // ====================================================================
    // POPULATION MANAGEMENT
    // ====================================================================

    /**
     * @brief Spawn bots to reach population target
     */
    virtual void SpawnToPopulationTarget() = 0;

    /**
     * @brief Update population targets based on real player distribution
     */
    virtual void UpdatePopulationTargets() = 0;

    /**
     * @brief Despawn bot
     * @param guid Bot character GUID
     * @param forced Force despawn even if in combat
     */
    virtual void DespawnBot(ObjectGuid guid, bool forced = false) = 0;

    /**
     * @brief Despawn bot with reason
     * @param guid Bot character GUID
     * @param reason Despawn reason for logging
     * @return true if successful, false otherwise
     */
    virtual bool DespawnBot(ObjectGuid guid, std::string const& reason) = 0;

    /**
     * @brief Despawn all bots
     */
    virtual void DespawnAllBots() = 0;

    // ====================================================================
    // ZONE MANAGEMENT
    // ====================================================================

    /**
     * @brief Update zone population
     * @param zoneId Zone ID
     * @param mapId Map ID
     */
    virtual void UpdateZonePopulation(uint32 zoneId, uint32 mapId) = 0;

    /**
     * @brief Update zone population (thread-safe variant)
     * @param zoneId Zone ID
     * @param mapId Map ID
     */
    virtual void UpdateZonePopulationSafe(uint32 zoneId, uint32 mapId) = 0;

    /**
     * @brief Get zone population data
     * @param zoneId Zone ID
     * @return Zone population structure
     */
    virtual ZonePopulation GetZonePopulation(uint32 zoneId) const = 0;

    /**
     * @brief Get all zone populations
     * @return Vector of zone population data
     */
    virtual std::vector<ZonePopulation> GetAllZonePopulations() const = 0;

    // ====================================================================
    // BOT TRACKING
    // ====================================================================

    /**
     * @brief Check if bot is active
     * @param guid Bot character GUID
     * @return true if bot is active, false otherwise
     */
    virtual bool IsBotActive(ObjectGuid guid) const = 0;

    /**
     * @brief Get total active bot count
     * @return Number of active bots
     */
    virtual uint32 GetActiveBotCount() const = 0;

    /**
     * @brief Get active bot count in zone
     * @param zoneId Zone ID
     * @return Number of active bots in zone
     */
    virtual uint32 GetActiveBotCount(uint32 zoneId) const = 0;

    /**
     * @brief Get active bot count on map
     * @param mapId Map ID
     * @param useMapId If true, count by map instead of zone
     * @return Number of active bots on map
     */
    virtual uint32 GetActiveBotCount(uint32 mapId, bool useMapId) const = 0;

    /**
     * @brief Get all active bots in zone
     * @param zoneId Zone ID
     * @return Vector of bot character GUIDs
     */
    virtual std::vector<ObjectGuid> GetActiveBotsInZone(uint32 zoneId) const = 0;

    // ====================================================================
    // STATISTICS
    // ====================================================================

    /**
     * @brief Get spawn statistics
     * @return Reference to spawn statistics
     */
    virtual SpawnStats const& GetStats() const = 0;

    /**
     * @brief Reset spawn statistics
     */
    virtual void ResetStats() = 0;

    // ====================================================================
    // PLAYER EVENTS
    // ====================================================================

    /**
     * @brief Handle player login event
     */
    virtual void OnPlayerLogin() = 0;

    /**
     * @brief Check and spawn bots for active players
     */
    virtual void CheckAndSpawnForPlayers() = 0;

    // ====================================================================
    // POPULATION CAPS
    // ====================================================================

    /**
     * @brief Check if more bots can be spawned globally
     * @return true if under global cap, false otherwise
     */
    virtual bool CanSpawnMore() const = 0;

    /**
     * @brief Check if more bots can be spawned in zone
     * @param zoneId Zone ID
     * @return true if under zone cap, false otherwise
     */
    virtual bool CanSpawnInZone(uint32 zoneId) const = 0;

    /**
     * @brief Check if more bots can be spawned on map
     * @param mapId Map ID
     * @return true if under map cap, false otherwise
     */
    virtual bool CanSpawnOnMap(uint32 mapId) const = 0;

    // ====================================================================
    // RUNTIME CONTROL
    // ====================================================================

    /**
     * @brief Enable or disable spawner
     * @param enabled true to enable, false to disable
     */
    virtual void SetEnabled(bool enabled) = 0;

    /**
     * @brief Check if spawner is enabled
     * @return true if enabled, false otherwise
     */
    virtual bool IsEnabled() const = 0;

    /**
     * @brief Set maximum bot count
     * @param maxBots Maximum number of bots
     */
    virtual void SetMaxBots(uint32 maxBots) = 0;

    /**
     * @brief Set bot-to-player ratio
     * @param ratio Ratio of bots to real players
     */
    virtual void SetBotToPlayerRatio(float ratio) = 0;
};

} // namespace Playerbot
