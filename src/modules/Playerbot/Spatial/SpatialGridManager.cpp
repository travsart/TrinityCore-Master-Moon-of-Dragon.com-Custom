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
    ::std::unique_lock lock(_mutex);  // Exclusive write lock

    uint32 mapId = map->GetId();
    auto now = ::std::chrono::steady_clock::now();

    auto it = _grids.find(mapId);
    if (it != _grids.end())
    {
        // CRITICAL FIX: Update the Map pointer to the new Map instance
        // When a Map is destroyed and recreated (same mapId, new Map object),
        // we must update the grid's Map pointer to avoid dangling pointer crashes
        if (it->second.grid->GetMap() != map)
        {
            TC_LOG_INFO("playerbot.spatial",
                "Updating spatial grid Map pointer for map {} ({}) - Map object changed",
                mapId, map->GetMapName());
            it->second.grid->SetMap(map);
        }
        // Update access time
        it->second.lastAccessTime = now;
        return;
    }

    // Create new grid with metadata
    GridInfo info;
    info.grid = ::std::make_unique<DoubleBufferedSpatialGrid>(map);
    info.grid->Start();  // Initialize (no background thread, synchronous mode)
    info.lastAccessTime = now;
    info.creationTime = now;

    _grids[mapId] = ::std::move(info);

    TC_LOG_INFO("playerbot.spatial",
        "Created spatial grid for map {} ({}) - Total grids: {}",
        mapId, map->GetMapName(), _grids.size());
}

void SpatialGridManager::DestroyGrid(uint32 mapId)
{
    ::std::unique_lock lock(_mutex);  // Exclusive write lock

    auto it = _grids.find(mapId);
    if (it == _grids.end())
    {
        TC_LOG_WARN("playerbot.spatial",
            "Attempted to destroy non-existent spatial grid for map {}", mapId);
        return;
    }

    // Get memory usage before destruction for logging
    size_t memoryUsage = 0;
    if (it->second.grid)
    {
        auto stats = it->second.grid->GetStatistics();
        memoryUsage = stats.memoryUsageBytes;
    }

    it->second.grid->Stop();
    _grids.erase(it);

    TC_LOG_INFO("playerbot.spatial",
        "Destroyed spatial grid for map {} - Freed {:.2f} MB - Remaining grids: {}",
        mapId,
        static_cast<float>(memoryUsage) / (1024.0f * 1024.0f),
        _grids.size());
}

DoubleBufferedSpatialGrid* SpatialGridManager::GetGrid(uint32 mapId)
{
    ::std::shared_lock lock(_mutex);  // Shared read lock - multiple threads can read simultaneously!

    auto it = _grids.find(mapId);
    if (it == _grids.end())
        return nullptr;

    // CRITICAL FIX: Removed lastAccessTime update from GetGrid()
    // PROBLEM: This was a DATA RACE - writing under shared_lock is unsafe!
    // Also caused severe performance issues with 100+ bots calling GetGrid
    // thousands of times per second (each call: acquire lock + chrono::now())
    //
    // SOLUTION: lastAccessTime is now only updated when:
    // - CreateGrid() is called (exclusive lock, grid creation)
    // - UpdateGrid() is called (from spatial grid update cycle)
    // - TouchGrid() is explicitly called (rare, for manual keep-alive)
    //
    // This eliminates ~1000+ chrono::now() calls per second and fixes the data race.

    return it->second.grid.get();
}

DoubleBufferedSpatialGrid* SpatialGridManager::GetGrid(Map* map)
{
    if (!map)
        return nullptr;

    return GetGrid(map->GetId());
}

void SpatialGridManager::DestroyAllGrids()
{
    ::std::unique_lock lock(_mutex);  // Exclusive write lock

    size_t totalMemory = 0;
    for (auto& [mapId, info] : _grids)
    {
        if (info.grid)
        {
            totalMemory += info.grid->GetStatistics().memoryUsageBytes;
            info.grid->Stop();
        }
    }

    TC_LOG_INFO("playerbot.spatial",
        "Destroying all spatial grids ({} total) - Freeing {:.2f} MB",
        _grids.size(),
        static_cast<float>(totalMemory) / (1024.0f * 1024.0f));

    _grids.clear();
}

void SpatialGridManager::UpdateGrid(uint32 mapId)
{
    DoubleBufferedSpatialGrid* grid = nullptr;

    // Phase 1: Get the grid pointer under shared lock (fast path)
    {
        ::std::shared_lock lock(_mutex);
        auto it = _grids.find(mapId);
        if (it == _grids.end())
            return;  // No grid for this map
        grid = it->second.grid.get();
    }

    // Phase 2: Update the grid WITHOUT holding the manager lock
    // Grid has its own internal locking (try_to_lock pattern)
    if (grid)
        grid->Update();

    // Phase 3: Update access time under exclusive lock (write operation)
    // This is called infrequently (once per update cycle, not per bot)
    {
        ::std::unique_lock lock(_mutex);
        auto it = _grids.find(mapId);
        if (it != _grids.end())
            it->second.lastAccessTime = ::std::chrono::steady_clock::now();
    }
}

void SpatialGridManager::UpdateGrid(Map* map)
{
    if (!map)
        return;

    UpdateGrid(map->GetId());
}

size_t SpatialGridManager::GetGridCount() const
{
    ::std::shared_lock lock(_mutex);  // Shared read lock
    return _grids.size();
}

// ============================================================================
// MEMORY LIFECYCLE MANAGEMENT
// ============================================================================

size_t SpatialGridManager::CleanupInactiveGrids()
{
    ::std::unique_lock lock(_mutex);  // Exclusive write lock

    auto now = ::std::chrono::steady_clock::now();
    ::std::vector<uint32> gridsToRemove;

    for (auto const& [mapId, info] : _grids)
    {
        // Check if grid has been inactive for longer than timeout
        auto idleTime = ::std::chrono::duration_cast<::std::chrono::seconds>(
            now - info.lastAccessTime).count();

        if (idleTime >= GRID_IDLE_TIMEOUT_SEC)
        {
            // Also check if grid is empty (no population)
            uint32 population = info.grid ? info.grid->GetPopulationCount() : 0;

            if (population == 0)
            {
                gridsToRemove.push_back(mapId);
                TC_LOG_INFO("playerbot.spatial",
                    "Grid for map {} marked for cleanup - Idle for {}s, Population: {}",
                    mapId, idleTime, population);
            }
        }
    }

    // Remove inactive grids
    size_t freedMemory = 0;
    for (uint32 mapId : gridsToRemove)
    {
        auto it = _grids.find(mapId);
        if (it != _grids.end() && it->second.grid)
        {
            freedMemory += it->second.grid->GetStatistics().memoryUsageBytes;
            it->second.grid->Stop();
            _grids.erase(it);
        }
    }

    if (!gridsToRemove.empty())
    {
        TC_LOG_INFO("playerbot.spatial",
            "Cleaned up {} inactive grids - Freed {:.2f} MB - Remaining grids: {}",
            gridsToRemove.size(),
            static_cast<float>(freedMemory) / (1024.0f * 1024.0f),
            _grids.size());
    }

    return gridsToRemove.size();
}

size_t SpatialGridManager::GetTotalMemoryUsage() const
{
    ::std::shared_lock lock(_mutex);  // Shared read lock

    size_t total = 0;
    for (auto const& [mapId, info] : _grids)
    {
        if (info.grid)
        {
            total += info.grid->GetStatistics().memoryUsageBytes;
        }
    }

    return total;
}

SpatialGridManager::MemoryStats SpatialGridManager::GetMemoryStats() const
{
    ::std::shared_lock lock(_mutex);  // Shared read lock

    MemoryStats stats;
    stats.totalGrids = _grids.size();

    for (auto const& [mapId, info] : _grids)
    {
        if (info.grid)
        {
            auto gridStats = info.grid->GetStatistics();
            stats.totalMemoryBytes += gridStats.memoryUsageBytes;
            stats.totalPopulation += gridStats.currentPopulation;
            stats.totalActiveCells += gridStats.activeCellCount;

            if (gridStats.peakMemoryUsageBytes > stats.peakMemoryBytes)
                stats.peakMemoryBytes = gridStats.peakMemoryUsageBytes;
        }
    }

    // Update peak memory tracking
    size_t currentPeak = _peakMemoryUsage.load(::std::memory_order_relaxed);
    while (stats.totalMemoryBytes > currentPeak &&
           !_peakMemoryUsage.compare_exchange_weak(currentPeak, stats.totalMemoryBytes,
                                                    ::std::memory_order_relaxed,
                                                    ::std::memory_order_relaxed))
    {
        // Retry with updated currentPeak
    }

    stats.peakMemoryBytes = _peakMemoryUsage.load(::std::memory_order_relaxed);

    return stats;
}

void SpatialGridManager::LogMemoryStats() const
{
    auto stats = GetMemoryStats();

    TC_LOG_INFO("playerbot.spatial",
        "Spatial Grid Memory Stats: {} grids, {:.2f} MB current, {:.2f} MB peak, "
        "{} entities, {} active cells",
        stats.totalGrids,
        static_cast<float>(stats.totalMemoryBytes) / (1024.0f * 1024.0f),
        static_cast<float>(stats.peakMemoryBytes) / (1024.0f * 1024.0f),
        stats.totalPopulation,
        stats.totalActiveCells);

    // Log per-grid breakdown if there are multiple grids
    if (stats.totalGrids > 1)
    {
        ::std::shared_lock lock(_mutex);
        for (auto const& [mapId, info] : _grids)
        {
            if (info.grid)
            {
                auto gridStats = info.grid->GetStatistics();
                auto idleTime = ::std::chrono::duration_cast<::std::chrono::seconds>(
                    ::std::chrono::steady_clock::now() - info.lastAccessTime).count();

                TC_LOG_DEBUG("playerbot.spatial",
                    "  Map {}: {:.2f} MB, {} entities, {} cells, idle {}s",
                    mapId,
                    gridStats.GetMemoryUsageMB(),
                    gridStats.currentPopulation,
                    gridStats.activeCellCount,
                    idleTime);
            }
        }
    }
}

void SpatialGridManager::TouchGrid(uint32 mapId)
{
    // CRITICAL FIX: Use exclusive lock for write operation
    // Previously used shared_lock with const_cast which was a data race
    ::std::unique_lock lock(_mutex);

    auto it = _grids.find(mapId);
    if (it != _grids.end())
    {
        it->second.lastAccessTime = ::std::chrono::steady_clock::now();
    }
}

} // namespace Playerbot
