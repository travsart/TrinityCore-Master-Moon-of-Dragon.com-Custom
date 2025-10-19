/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_DOUBLE_BUFFERED_SPATIAL_GRID_H
#define PLAYERBOT_DOUBLE_BUFFERED_SPATIAL_GRID_H

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <array>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>

class Map;
class Creature;
class Player;
class GameObject;
class DynamicObject;
class AreaTrigger;

namespace Playerbot
{

/**
 * @class DoubleBufferedSpatialGrid
 * @brief Lock-free spatial grid for 5000+ concurrent bot queries with ZERO TrinityCore core modifications
 *
 * ARCHITECTURE:
 * - Two complete world grids (Buffer A and Buffer B)
 * - Background worker thread updates inactive buffer every 100ms
 * - Reads Map entities using ObjectAccessor (thread-safe)
 * - Atomic buffer swap after update complete
 * - Bots query active buffer with zero lock contention
 *
 * PERFORMANCE TARGETS:
 * - Query latency: 1-5μs per bot
 * - Memory: ~500MB for double buffers
 * - Update overhead: 2-5ms per 100ms tick
 * - Scales linearly to 10,000+ bots
 *
 * THREAD SAFETY:
 * - Worker thread: Reads Map data, writes to inactive buffer
 * - Bot threads: Read from active buffer (atomic load, no locks)
 * - Main thread: Never blocks, never touched by spatial grid
 *
 * MODULE INTEGRATION:
 * - Lives in src/modules/Playerbot/Spatial/ (NO core files modified)
 * - Created by BotWorldSessionMgr::Initialize()
 * - Auto-stops worker thread in destructor
 * - Uses TrinityCore's existing ObjectAccessor for thread-safe reads
 */
class TC_GAME_API DoubleBufferedSpatialGrid
{
public:
    // Grid constants (matches TrinityCore's spatial layout)
    static constexpr uint32 CELLS_PER_GRID = 8;
    static constexpr uint32 GRIDS_PER_MAP = 64;
    static constexpr uint32 TOTAL_CELLS = GRIDS_PER_MAP * CELLS_PER_GRID; // 512
    static constexpr float CELL_SIZE = 66.6666f; // yards
    static constexpr uint32 UPDATE_INTERVAL_MS = 100; // 10 Hz update rate

    /**
     * @brief Contents of a single cell
     * Stores GUIDs instead of pointers (thread-safe, no dangling references)
     */
    struct CellContents
    {
        std::vector<ObjectGuid> creatures;
        std::vector<ObjectGuid> players;
        std::vector<ObjectGuid> gameObjects;
        std::vector<ObjectGuid> dynamicObjects;
        std::vector<ObjectGuid> areaTriggers;

        void Clear()
        {
            creatures.clear();
            players.clear();
            gameObjects.clear();
            dynamicObjects.clear();
            areaTriggers.clear();
        }

        size_t GetTotalCount() const
        {
            return creatures.size() + players.size() + gameObjects.size() +
                   dynamicObjects.size() + areaTriggers.size();
        }
    };

    /**
     * @brief One complete spatial grid buffer
     * 512x512 cells = 262,144 cells covering entire map
     */
    struct GridBuffer
    {
        std::array<std::array<CellContents, TOTAL_CELLS>, TOTAL_CELLS> cells;
        uint32 populationCount{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Clear()
        {
            for (auto& row : cells)
                for (auto& cell : row)
                    cell.Clear();
            populationCount = 0;
        }
    };

    /**
     * @brief Query statistics for monitoring
     */
    struct Statistics
    {
        uint64_t totalQueries{0};
        uint64_t totalUpdates{0};
        uint64_t totalSwaps{0};
        uint32 lastUpdateDurationUs{0};
        uint32 currentPopulation{0};
        std::chrono::steady_clock::time_point startTime;
    };

    // Constructor/Destructor
    explicit DoubleBufferedSpatialGrid(Map* map);
    ~DoubleBufferedSpatialGrid();

    // Lifecycle
    void Start();
    void Stop();
    bool IsRunning() const { return _running.load(std::memory_order_acquire); }

    // Bot query interface (thread-safe, lock-free)
    std::vector<ObjectGuid> QueryNearbyCreatures(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyPlayers(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyGameObjects(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyDynamicObjects(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyAreaTriggers(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyAll(Position const& pos, float radius) const;

    // Cell-level query (for advanced usage)
    CellContents const& GetCell(uint32 x, uint32 y) const;

    // Statistics
    Statistics GetStatistics() const;
    uint32 GetPopulationCount() const;

private:
    // Worker thread main loop
    void UpdateWorkerThread();

    // Populate write buffer from Map entities
    void PopulateBufferFromMap();

    // Swap buffers atomically
    void SwapBuffers();

    // Helper: Get write buffer (inactive)
    GridBuffer& GetWriteBuffer()
    {
        return _buffers[1 - _readBufferIndex.load(std::memory_order_relaxed)];
    }

    // Helper: Get read buffer (active)
    GridBuffer const& GetReadBuffer() const
    {
        return _buffers[_readBufferIndex.load(std::memory_order_acquire)];
    }

    // Coordinate conversion helpers
    std::pair<uint32, uint32> GetCellCoords(Position const& pos) const;
    std::vector<std::pair<uint32, uint32>> GetCellsInRadius(Position const& center, float radius) const;

    // Thread-safe Map entity access
    std::vector<Creature*> GetCreaturesInArea(Position const& center, float radius);
    std::vector<Player*> GetPlayersInArea(Position const& center, float radius);
    std::vector<GameObject*> GetGameObjectsInArea(Position const& center, float radius);
    std::vector<DynamicObject*> GetDynamicObjectsInArea(Position const& center, float radius);
    std::vector<AreaTrigger*> GetAreaTriggersInArea(Position const& center, float radius);

    // Data members
    Map* _map;
    GridBuffer _buffers[2];
    std::atomic<uint32> _readBufferIndex{0};

    // Worker thread
    std::unique_ptr<std::thread> _updateThread;
    std::atomic<bool> _running{false};

    // Statistics (atomic for thread-safe access)
    mutable std::atomic<uint64_t> _totalQueries{0};
    std::atomic<uint64_t> _totalUpdates{0};
    std::atomic<uint64_t> _totalSwaps{0};
    std::atomic<uint32> _lastUpdateDurationUs{0};

    std::chrono::steady_clock::time_point _startTime;

    // Non-copyable, non-movable
    DoubleBufferedSpatialGrid(DoubleBufferedSpatialGrid const&) = delete;
    DoubleBufferedSpatialGrid& operator=(DoubleBufferedSpatialGrid const&) = delete;
    DoubleBufferedSpatialGrid(DoubleBufferedSpatialGrid&&) = delete;
    DoubleBufferedSpatialGrid& operator=(DoubleBufferedSpatialGrid&&) = delete;
};

} // namespace Playerbot

#endif // PLAYERBOT_DOUBLE_BUFFERED_SPATIAL_GRID_H
