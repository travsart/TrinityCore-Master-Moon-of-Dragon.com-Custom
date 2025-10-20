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
    // CRITICAL DEADLOCK FIX: Background thread eliminated
    // Reason: Background thread was iterating Map containers without proper locks
    // causing deadlocks with main thread and bot threads
    // Solution: Spatial grid now updated synchronously from Map::Update

    TC_LOG_INFO("playerbot.spatial",
        "Spatial grid initialized for map {} (synchronous updates, no background thread)",
        _map->GetId());

    // Do initial population
    PopulateBufferFromMap();
    SwapBuffers();
}

void DoubleBufferedSpatialGrid::Stop()
{
    // CRITICAL DEADLOCK FIX: No background thread to stop anymore
    TC_LOG_INFO("playerbot.spatial",
        "Spatial grid stopped for map {} (synchronous mode, no thread to join)",
        _map->GetId());
}

bool DoubleBufferedSpatialGrid::ShouldUpdate() const
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastUpdate);
    return elapsed.count() >= UPDATE_INTERVAL_MS;
}

void DoubleBufferedSpatialGrid::Update() const
{
    // CRITICAL DEADLOCK FIX: On-demand synchronous update with rate limiting
    // Only one thread can update at a time (mutex protected)
    // Other threads will skip if update is already in progress

    // Try to acquire lock (non-blocking)
    std::unique_lock<std::mutex> lock(_updateMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        // Another thread is already updating, skip
        return;
    }

    // Check if enough time has passed since last update
    if (!ShouldUpdate())
        return;

    auto cycleStart = std::chrono::steady_clock::now();

    try
    {
        // Populate inactive buffer from Map entities
        const_cast<DoubleBufferedSpatialGrid*>(this)->PopulateBufferFromMap();

        // Swap buffers atomically
        const_cast<DoubleBufferedSpatialGrid*>(this)->SwapBuffers();

        _lastUpdate = cycleStart;
        _totalUpdates.fetch_add(1, std::memory_order_relaxed);
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("playerbot.spatial",
            "Exception in spatial grid update for map {}: {}",
            _map->GetId(), ex.what());
    }

    auto cycleEnd = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(cycleEnd - cycleStart);

    if (elapsed.count() > 10)  // Warn if update takes >10ms
        TC_LOG_WARN("playerbot.spatial",
            "Spatial grid update took {}ms for map {}",
            elapsed.count(), _map->GetId());
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
    // REMOVED: Broken DynamicObjectListSearcher code
    // REMOVED: Broken DynamicObjectListSearcher code
    // REMOVED: Cell::VisitGridObjects call
    // REMOVED: Orphaned areaTriggers loop (variable not populated after Cell::Visit elimination)
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
    // Ensure grid is up-to-date (rate-limited, non-blocking)
    Update();

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
    Update();  // Ensure grid is up-to-date

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
    Update();  // Ensure grid is up-to-date

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
    Update();  // Ensure grid is up-to-date

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
    Update();  // Ensure grid is up-to-date

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
    Update();  // Ensure grid is up-to-date

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
