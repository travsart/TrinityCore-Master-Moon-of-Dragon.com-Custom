/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "SpatialGridManager.h"
#include "Map.h"
#include "Log.h"

namespace Playerbot
{

void SpatialGridManager::CreateGrid(Map* map)
{
    if (!map)
    {
        TC_LOG_ERROR("playerbot.spatial", "Attempted to create spatial grid with null map pointer");
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);

    uint32 mapId = map->GetId();

    if (_grids.find(mapId) != _grids.end())
    {
        TC_LOG_WARN("playerbot.spatial",
            "Spatial grid already exists for map {} ({})",
            mapId, map->GetMapName());
        return;
    }

    auto grid = std::make_unique<DoubleBufferedSpatialGrid>(map);
    grid->Start(); // Start worker thread

    _grids[mapId] = std::move(grid);

    TC_LOG_INFO("playerbot.spatial",
        "Created and started spatial grid for map {} ({})",
        mapId, map->GetMapName());
}

void SpatialGridManager::DestroyGrid(uint32 mapId)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _grids.find(mapId);
    if (it == _grids.end())
    {
        TC_LOG_WARN("playerbot.spatial",
            "Attempted to destroy non-existent spatial grid for map {}", mapId);
        return;
    }

    it->second->Stop(); // Stop worker thread
    _grids.erase(it);

    TC_LOG_INFO("playerbot.spatial",
        "Destroyed spatial grid for map {}", mapId);
}

DoubleBufferedSpatialGrid* SpatialGridManager::GetGrid(uint32 mapId)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _grids.find(mapId);
    if (it == _grids.end())
        return nullptr;

    return it->second.get();
}

DoubleBufferedSpatialGrid* SpatialGridManager::GetGrid(Map* map)
{
    if (!map)
        return nullptr;

    return GetGrid(map->GetId());
}

void SpatialGridManager::DestroyAllGrids()
{
    std::lock_guard<std::mutex> lock(_mutex);

    TC_LOG_INFO("playerbot.spatial",
        "Destroying all spatial grids ({} total)", _grids.size());

    for (auto& pair : _grids)
    {
        pair.second->Stop();
    }

    _grids.clear();
}

size_t SpatialGridManager::GetGridCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _grids.size();
}

} // namespace Playerbot
