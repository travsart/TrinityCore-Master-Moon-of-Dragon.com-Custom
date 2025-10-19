/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef PLAYERBOT_SPATIAL_GRID_MANAGER_H
#define PLAYERBOT_SPATIAL_GRID_MANAGER_H

#include "Define.h"
#include "DoubleBufferedSpatialGrid.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class Map;

namespace Playerbot
{

/**
 * @brief Global manager for all spatial grids across all maps
 * Singleton pattern for easy access from TargetScanner
 */
class TC_GAME_API SpatialGridManager
{
public:
    static SpatialGridManager& Instance()
    {
        static SpatialGridManager instance;
        return instance;
    }

    // Create spatial grid for a map
    void CreateGrid(Map* map);

    // Destroy spatial grid for a map
    void DestroyGrid(uint32 mapId);

    // Get spatial grid for a map (returns nullptr if not found)
    DoubleBufferedSpatialGrid* GetGrid(uint32 mapId);
    DoubleBufferedSpatialGrid* GetGrid(Map* map);

    // Destroy all grids
    void DestroyAllGrids();

    // Statistics
    size_t GetGridCount() const;

private:
    SpatialGridManager() = default;
    ~SpatialGridManager() { DestroyAllGrids(); }

    std::unordered_map<uint32, std::unique_ptr<DoubleBufferedSpatialGrid>> _grids;
    mutable std::mutex _mutex;

    // Non-copyable
    SpatialGridManager(SpatialGridManager const&) = delete;
    SpatialGridManager& operator=(SpatialGridManager const&) = delete;
};

// Convenience accessor
#define sSpatialGridManager Playerbot::SpatialGridManager::Instance()

} // namespace Playerbot

#endif // PLAYERBOT_SPATIAL_GRID_MANAGER_H
