/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include <cstdint>
#include <cstddef>

class Map;

namespace Playerbot
{

class DoubleBufferedSpatialGrid;

/**
 * @brief Interface for Spatial Grid Management
 *
 * Abstracts spatial grid operations to enable dependency injection and testing.
 * The spatial grid manager maintains per-map grids for efficient entity lookups.
 *
 * **Responsibilities:**
 * - Create and destroy spatial grids per map
 * - Provide grid access for queries
 * - Coordinate grid updates
 * - Track grid statistics
 *
 * **Testability:**
 * - Can be mocked for unit testing without real spatial grids
 * - Dependency injection enables testing without map instances
 *
 * **Usage:**
 * @code
 * auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
 * auto grid = spatialMgr->GetGrid(mapId);
 * if (grid)
 * {
 *     // Perform spatial queries
 * }
 * @endcode
 */
class TC_GAME_API ISpatialGridManager
{
public:
    virtual ~ISpatialGridManager() = default;

    /**
     * @brief Create spatial grid for a map
     *
     * Initializes a new double-buffered spatial grid for the specified map.
     * Safe to call multiple times - does nothing if grid already exists.
     *
     * @param map Map instance to create grid for
     *
     * @note Called automatically when map loads
     */
    virtual void CreateGrid(Map* map) = 0;

    /**
     * @brief Destroy spatial grid for a map
     *
     * Removes and destroys the spatial grid for the specified map ID.
     * Safe to call even if grid doesn't exist.
     *
     * @param mapId Map ID to destroy grid for
     *
     * @note Called automatically when map unloads
     */
    virtual void DestroyGrid(uint32 mapId) = 0;

    /**
     * @brief Get spatial grid by map ID
     *
     * Retrieves the spatial grid for the specified map ID.
     * Returns nullptr if no grid exists for that map.
     *
     * @param mapId Map ID to get grid for
     * @return Pointer to spatial grid, or nullptr if not found
     *
     * @note Thread-safe for concurrent reads
     *
     * Example:
     * @code
     * auto grid = spatialMgr->GetGrid(mapId);
     * if (grid)
     * {
     *     auto units = grid->GetUnitsInRadius(pos, 30.0f);
     * }
     * @endcode
     */
    virtual DoubleBufferedSpatialGrid* GetGrid(uint32 mapId) = 0;

    /**
     * @brief Get spatial grid by map instance
     *
     * Convenience overload that extracts map ID from map instance.
     *
     * @param map Map instance to get grid for
     * @return Pointer to spatial grid, or nullptr if not found
     *
     * Example:
     * @code
     * auto grid = spatialMgr->GetGrid(bot->GetMap());
     * @endcode
     */
    virtual DoubleBufferedSpatialGrid* GetGrid(Map* map) = 0;

    /**
     * @brief Get or create spatial grid for a map (OPTIMAL - PREFERRED METHOD)
     *
     * Combines GetGrid() + CreateGrid() into a single optimized operation.
     * Uses double-checked locking for optimal performance.
     *
     * PERFORMANCE: This is the PREFERRED method for accessing grids!
     * - Eliminates the redundant pattern: if (!GetGrid()) { CreateGrid(); GetGrid(); }
     * - Single method call instead of 3
     * - Single lock acquisition in the common case (grid exists)
     *
     * @param map Map instance to get/create grid for
     * @return Pointer to spatial grid (never null if map is valid)
     *
     * Example:
     * @code
     * // OLD pattern (3 lookups, multiple lock acquisitions):
     * auto grid = spatialMgr->GetGrid(map);
     * if (!grid) { spatialMgr->CreateGrid(map); grid = spatialMgr->GetGrid(map); }
     *
     * // NEW pattern (optimal, single lookup):
     * auto grid = spatialMgr->GetOrCreateGrid(map);
     * @endcode
     */
    virtual DoubleBufferedSpatialGrid* GetOrCreateGrid(Map* map) = 0;

    /**
     * @brief Destroy all spatial grids
     *
     * Removes and destroys all spatial grids across all maps.
     * Used during shutdown or testing cleanup.
     *
     * @note Automatically called by SpatialGridManager destructor
     */
    virtual void DestroyAllGrids() = 0;

    /**
     * @brief Update spatial grid by map ID
     *
     * Triggers update for the specified map's spatial grid.
     * This swaps the read/write buffers in the double-buffered grid.
     *
     * @param mapId Map ID to update grid for
     *
     * @note Called from Map::Update() each frame
     */
    virtual void UpdateGrid(uint32 mapId) = 0;

    /**
     * @brief Update spatial grid by map instance
     *
     * Convenience overload that extracts map ID from map instance.
     *
     * @param map Map instance to update grid for
     */
    virtual void UpdateGrid(Map* map) = 0;

    /**
     * @brief Get count of active spatial grids
     *
     * Returns the number of currently initialized spatial grids.
     * Useful for statistics and diagnostics.
     *
     * @return Number of active grids
     *
     * Example:
     * @code
     * TC_LOG_INFO("playerbot", "Active spatial grids: {}", spatialMgr->GetGridCount());
     * @endcode
     */
    virtual size_t GetGridCount() const = 0;
};

} // namespace Playerbot
