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

    ::std::unique_lock<::std::shared_mutex> lock(_mutex);  // Exclusive write lock

    uint32 mapId = map->GetId();

    if (_grids.find(mapId) != _grids.end())
    {
        TC_LOG_WARN("playerbot.spatial",
            "Spatial grid already exists for map {} ({})",
            mapId, map->GetMapName());
        return;
    }

    auto grid = ::std::make_unique<DoubleBufferedSpatialGrid>(map);
    grid->Start(); // Initialize (no background thread, synchronous mode)

    _grids[mapId] = ::std::move(grid);
    TC_LOG_INFO("playerbot.spatial",
        "Created spatial grid for map {} ({}) in synchronous mode",
        mapId, map->GetMapName());
}

void SpatialGridManager::DestroyGrid(uint32 mapId)
{
    ::std::unique_lock<::std::shared_mutex> lock(_mutex);  // Exclusive write lock

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
    ::std::shared_lock<::std::shared_mutex> lock(_mutex);  // Shared read lock - multiple threads can read simultaneously!

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
    ::std::unique_lock<::std::shared_mutex> lock(_mutex);  // Exclusive write lock

    TC_LOG_INFO("playerbot.spatial",
        "Destroying all spatial grids ({} total)", _grids.size());

    for (auto& pair : _grids)
    {
        pair.second->Stop();
    }

    _grids.clear();
}

void SpatialGridManager::UpdateGrid(uint32 mapId)
{
    ::std::shared_lock<::std::shared_mutex> lock(_mutex);  // Shared read lock

    auto it = _grids.find(mapId);
    if (it == _grids.end())
        return;  // No grid for this map

    it->second->Update();
}

void SpatialGridManager::UpdateGrid(Map* map)
{
    if (!map)
        return;

    UpdateGrid(map->GetId());
}

size_t SpatialGridManager::GetGridCount() const
{
    ::std::shared_lock<::std::shared_mutex> lock(_mutex);  // Shared read lock
    return _grids.size();
}

} // namespace Playerbot
