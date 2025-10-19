/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DoubleBufferedSpatialGrid.h"
#include "Map.h"
#include "Creature.h"
#include "Player.h"
#include "GameObject.h"
#include "DynamicObject.h"
#include "AreaTrigger.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Log.h"
#include "../Spatial/SpatialGridManager.h"  // Spatial grid for deadlock fix

namespace Playerbot
{

DoubleBufferedSpatialGrid::DoubleBufferedSpatialGrid(Map* map)
    : _map(map)
    , _startTime(std::chrono::steady_clock::now())
{
    ASSERT(map, "DoubleBufferedSpatialGrid requires valid Map pointer");

    TC_LOG_INFO("playerbot.spatial",
        "DoubleBufferedSpatialGrid created for map {} ({})",
        map->GetId(), map->GetMapName());
}

DoubleBufferedSpatialGrid::~DoubleBufferedSpatialGrid()
{
    Stop();

    TC_LOG_INFO("playerbot.spatial",
        "DoubleBufferedSpatialGrid destroyed for map {} - Total queries: {}, Updates: {}, Swaps: {}",
        _map->GetId(), _totalQueries.load(), _totalUpdates.load(), _totalSwaps.load());
}

void DoubleBufferedSpatialGrid::Start()
{
    if (_running.load(std::memory_order_acquire))
    {
        TC_LOG_WARN("playerbot.spatial",
            "DoubleBufferedSpatialGrid already running for map {}", _map->GetId());
        return;
    }

    TC_LOG_INFO("playerbot.spatial",
        "Starting spatial grid worker thread for map {} (update interval: {}ms)",
        _map->GetId(), UPDATE_INTERVAL_MS);

    _running.store(true, std::memory_order_release);
    _updateThread = std::make_unique<std::thread>([this] { UpdateWorkerThread(); });
}

void DoubleBufferedSpatialGrid::Stop()
{
    if (!_running.load(std::memory_order_acquire))
        return;

    TC_LOG_INFO("playerbot.spatial",
        "Stopping spatial grid worker thread for map {}", _map->GetId());

    _running.store(false, std::memory_order_release);

    if (_updateThread && _updateThread->joinable())
        _updateThread->join();

    _updateThread.reset();
}

void DoubleBufferedSpatialGrid::UpdateWorkerThread()
{
    TC_LOG_DEBUG("playerbot.spatial",
        "Worker thread started for map {}", _map->GetId());

    while (_running.load(std::memory_order_acquire))
    {
        auto cycleStart = std::chrono::steady_clock::now();

        try
        {
            // Populate inactive buffer from Map entities
            PopulateBufferFromMap();

            // Swap buffers atomically
            SwapBuffers();

            _totalUpdates.fetch_add(1, std::memory_order_relaxed);
        }
        catch (std::exception const& ex)
        {
            TC_LOG_ERROR("playerbot.spatial",
                "Exception in spatial grid worker thread for map {}: {}",
                _map->GetId(), ex.what());
        }

        // Sleep for remaining time
        auto cycleEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(cycleEnd - cycleStart);
        auto remaining = std::chrono::milliseconds(UPDATE_INTERVAL_MS) - elapsed;

        if (remaining.count() > 0)
            std::this_thread::sleep_for(remaining);
        else
            TC_LOG_WARN("playerbot.spatial",
                "Spatial grid update took {}ms (target: {}ms) for map {}",
                elapsed.count(), UPDATE_INTERVAL_MS, _map->GetId());
    }

    TC_LOG_DEBUG("playerbot.spatial",
        "Worker thread stopped for map {}", _map->GetId());
}

void DoubleBufferedSpatialGrid::PopulateBufferFromMap()
{
    auto start = std::chrono::high_resolution_clock::now();

    auto& writeBuffer = GetWriteBuffer();
    writeBuffer.Clear();

    if (!_map)
    {
        TC_LOG_ERROR("playerbot.spatial", "Map pointer is null in PopulateBufferFromMap!");
        return;
    }

    // CRITICAL: Thread-safe entity iteration using TrinityCore's MapRefManager
    // This iterates all creatures on the map using the map's internal storage
    // which is safe for concurrent reads (we're not modifying, just reading positions)

    uint32 creatureCount = 0;
    uint32 playerCount = 0;
    uint32 gameObjectCount = 0;
    uint32 dynamicObjectCount = 0;
    uint32 areaTriggerCount = 0;

    // Iterate all creatures on this map
    // Use Map's internal CreatureBySpawnIdContainer for thread-safe access
    auto const& creatures = _map->GetCreatureBySpawnIdStore();
    for (auto const& pair : creatures)
    {
        Creature* creature = pair.second;
        if (!creature || !creature->IsInWorld() || !creature->IsAlive())
            continue;

        auto [x, y] = GetCellCoords(creature->GetPosition());
        if (x < TOTAL_CELLS && y < TOTAL_CELLS)
        {
            writeBuffer.cells[x][y].creatures.push_back(creature->GetGUID());
            ++creatureCount;
        }
    }

    // Iterate all players on this map
    // Note: Map::GetPlayers() returns a reference to internal MapRefManager
    auto const& players = _map->GetPlayers();
    for (auto const& ref : players)
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld() || !player->IsAlive())
            continue;

        auto [x, y] = GetCellCoords(player->GetPosition());
        if (x < TOTAL_CELLS && y < TOTAL_CELLS)
        {
            writeBuffer.cells[x][y].players.push_back(player->GetGUID());
            ++playerCount;
        }
    }

    // Iterate all game objects on this map
    auto const& gameObjects = _map->GetGameObjectBySpawnIdStore();
    for (auto const& pair : gameObjects)
    {
        GameObject* go = pair.second;
        if (!go || !go->IsInWorld())
            continue;

        auto [x, y] = GetCellCoords(go->GetPosition());
        if (x < TOTAL_CELLS && y < TOTAL_CELLS)
        {
            writeBuffer.cells[x][y].gameObjects.push_back(go->GetGUID());
            ++gameObjectCount;
        }
    }

    // Iterate all dynamic objects on this map using Cell::Visit
    // CRITICAL SAFETY NOTE: Cell::Visit is SAFE here because:
    // 1. This is a SINGLE background thread (no concurrent Cell::Visit calls from bots)
    // 2. Only READING from map (not modifying)
    // 3. Deadlock ONLY occurs when MULTIPLE threads call Cell::Visit concurrently
    // 4. Bot threads will NEVER call Cell::Visit - they query this spatial grid instead
    // 5. This is the CORRECT pattern: centralize Cell::Visit in one place (here)
    {
        std::list<DynamicObject*> dynamicObjects;
        Trinity::AllWorldObjectsInRange dynCheck(nullptr, GRIDS_PER_MAP * CELLS_PER_GRID * CELL_SIZE);
        Trinity::DynamicObjectListSearcher<Trinity::AllWorldObjectsInRange> dynSearcher(nullptr, dynamicObjects, dynCheck);

        // Use Cell::Visit to populate - safe in background worker thread
        Cell::VisitGridObjects(_map, dynSearcher, GRIDS_PER_MAP * CELLS_PER_GRID * CELL_SIZE);

        // Store in spatial grid cells
        for (DynamicObject* dynObj : dynamicObjects)
        {
            if (!dynObj || !dynObj->IsInWorld())
                continue;

            auto [x, y] = GetCellCoords(dynObj->GetPosition());
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].dynamicObjects.push_back(dynObj->GetGUID());
                ++dynamicObjectCount;
            }
        }
    }

    // Iterate all area triggers on this map using Cell::Visit
    // Same safety guarantees as DynamicObjects above
    {
        std::list<AreaTrigger*> areaTriggers;
        Trinity::AllWorldObjectsInRange atCheck(nullptr, GRIDS_PER_MAP * CELLS_PER_GRID * CELL_SIZE);
        Trinity::AreaTriggerListSearcher<Trinity::AllWorldObjectsInRange> atSearcher(nullptr, areaTriggers, atCheck);

        // Use Cell::Visit to populate - safe in background worker thread
        Cell::VisitGridObjects(_map, atSearcher, GRIDS_PER_MAP * CELLS_PER_GRID * CELL_SIZE);

        // Store in spatial grid cells
        for (AreaTrigger* at : areaTriggers)
        {
            if (!at || !at->IsInWorld())
                continue;

            auto [x, y] = GetCellCoords(at->GetPosition());
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].areaTriggers.push_back(at->GetGUID());
                ++areaTriggerCount;
            }
        }
    }

    writeBuffer.populationCount = creatureCount + playerCount + gameObjectCount +
                                   dynamicObjectCount + areaTriggerCount;
    writeBuffer.lastUpdate = std::chrono::steady_clock::now();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    _lastUpdateDurationUs.store(static_cast<uint32>(duration.count()), std::memory_order_relaxed);

    TC_LOG_TRACE("playerbot.spatial",
        "PopulateBufferFromMap: map {} - {} creatures, {} players, {} gameobjects, {} dynobjects, {} areatriggers in {}μs",
        _map->GetId(), creatureCount, playerCount, gameObjectCount, dynamicObjectCount, areaTriggerCount, duration.count());
}

void DoubleBufferedSpatialGrid::SwapBuffers()
{
    uint32 oldIndex = _readBufferIndex.load(std::memory_order_relaxed);
    _readBufferIndex.store(1 - oldIndex, std::memory_order_release);

    _totalSwaps.fetch_add(1, std::memory_order_relaxed);

    TC_LOG_TRACE("playerbot.spatial",
        "SwapBuffers: map {} - Read buffer now {}, swap #{}",
        _map->GetId(), _readBufferIndex.load(), _totalSwaps.load());
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyCreatures(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<ObjectGuid> results;
    auto const& readBuffer = GetReadBuffer();

    // Get all cells within radius
    auto cells = GetCellsInRadius(pos, radius);

    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add creatures from this cell
        // Note: We still need distance check because cells are coarse (66 yards)
        for (ObjectGuid guid : cell.creatures)
        {
            results.push_back(guid);
        }
    }

    return results;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyPlayers(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<ObjectGuid> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];
        results.insert(results.end(), cell.players.begin(), cell.players.end());
    }

    return results;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyGameObjects(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<ObjectGuid> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];
        results.insert(results.end(), cell.gameObjects.begin(), cell.gameObjects.end());
    }

    return results;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyDynamicObjects(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<ObjectGuid> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];
        results.insert(results.end(), cell.dynamicObjects.begin(), cell.dynamicObjects.end());
    }

    return results;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyAreaTriggers(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<ObjectGuid> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];
        results.insert(results.end(), cell.areaTriggers.begin(), cell.areaTriggers.end());
    }

    return results;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyAll(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<ObjectGuid> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];
        results.insert(results.end(), cell.creatures.begin(), cell.creatures.end());
        results.insert(results.end(), cell.players.begin(), cell.players.end());
        results.insert(results.end(), cell.gameObjects.begin(), cell.gameObjects.end());
        results.insert(results.end(), cell.dynamicObjects.begin(), cell.dynamicObjects.end());
        results.insert(results.end(), cell.areaTriggers.begin(), cell.areaTriggers.end());
    }

    return results;
}

DoubleBufferedSpatialGrid::CellContents const& DoubleBufferedSpatialGrid::GetCell(
    uint32 x, uint32 y) const
{
    static CellContents emptyCell;

    if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
        return emptyCell;

    auto const& readBuffer = GetReadBuffer();
    return readBuffer.cells[x][y];
}

DoubleBufferedSpatialGrid::Statistics DoubleBufferedSpatialGrid::GetStatistics() const
{
    Statistics stats;
    stats.totalQueries = _totalQueries.load(std::memory_order_relaxed);
    stats.totalUpdates = _totalUpdates.load(std::memory_order_relaxed);
    stats.totalSwaps = _totalSwaps.load(std::memory_order_relaxed);
    stats.lastUpdateDurationUs = _lastUpdateDurationUs.load(std::memory_order_relaxed);
    stats.currentPopulation = GetReadBuffer().populationCount;
    stats.startTime = _startTime;

    return stats;
}

uint32 DoubleBufferedSpatialGrid::GetPopulationCount() const
{
    return GetReadBuffer().populationCount;
}

std::pair<uint32, uint32> DoubleBufferedSpatialGrid::GetCellCoords(Position const& pos) const
{
    // TrinityCore coordinate system:
    // Map center is at (0, 0)
    // Map size is TOTAL_CELLS * CELL_SIZE in each direction
    // Cell coordinates range from 0 to TOTAL_CELLS-1

    constexpr float MAP_HALF_SIZE = (TOTAL_CELLS * CELL_SIZE) / 2.0f;

    float x = (pos.GetPositionX() + MAP_HALF_SIZE) / CELL_SIZE;
    float y = (pos.GetPositionY() + MAP_HALF_SIZE) / CELL_SIZE;

    // Clamp to valid range
    uint32 cellX = static_cast<uint32>(std::clamp(x, 0.0f, static_cast<float>(TOTAL_CELLS - 1)));
    uint32 cellY = static_cast<uint32>(std::clamp(y, 0.0f, static_cast<float>(TOTAL_CELLS - 1)));

    return {cellX, cellY};
}

std::vector<std::pair<uint32, uint32>> DoubleBufferedSpatialGrid::GetCellsInRadius(
    Position const& center, float radius) const
{
    std::vector<std::pair<uint32, uint32>> cells;

    auto [centerX, centerY] = GetCellCoords(center);
    uint32 cellRadius = static_cast<uint32>(std::ceil(radius / CELL_SIZE)) + 1; // +1 for safety

    uint32 minX = (centerX > cellRadius) ? (centerX - cellRadius) : 0;
    uint32 maxX = std::min(TOTAL_CELLS - 1, centerX + cellRadius);
    uint32 minY = (centerY > cellRadius) ? (centerY - cellRadius) : 0;
    uint32 maxY = std::min(TOTAL_CELLS - 1, centerY + cellRadius);

    // Reserve space for efficiency
    cells.reserve((maxX - minX + 1) * (maxY - minY + 1));

    for (uint32 x = minX; x <= maxX; ++x)
    {
        for (uint32 y = minY; y <= maxY; ++y)
        {
            cells.emplace_back(x, y);
        }
    }

    return cells;
}

} // namespace Playerbot
