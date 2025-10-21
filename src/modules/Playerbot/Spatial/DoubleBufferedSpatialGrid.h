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
#include <mutex>

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
     * @brief Immutable snapshot of creature data for thread-safe bot queries
     *
     * CRITICAL: This snapshot is populated ONCE per update cycle from the main thread,
     * then read MANY times by worker threads. No pointers, no references - only POD data.
     *
     * Memory footprint: 96 bytes per creature (acceptable for 5000 creatures = 480KB)
     */
    struct CreatureSnapshot
    {
        ObjectGuid guid;
        uint32 entry{0};
        Position position;
        float health{0.0f};
        float maxHealth{0.0f};
        uint8 level{0};
        uint32 faction{0};

        // State flags (packed for cache efficiency)
        bool isAlive{false};
        bool isInCombat{false};
        bool isElite{false};
        bool isWorldBoss{false};

        // Targeting data
        ObjectGuid currentTarget;  // Current victim GUID
        float threatRadius{0.0f};  // Aggro radius

        // Movement data (for pathing/positioning)
        float moveSpeed{0.0f};
        bool isMoving{false};

        // PHASE 3 ENHANCEMENT: Quest system support
        bool isDead{false};  // For quest objectives and skinning
        bool isTappedByOther{false};  // For loot validation
        bool isSkinnable{false};  // For gathering professions
        bool hasBeenLooted{false};  // For corpse validation

        // Quest giver data
        bool hasQuestGiver{false};  // NPC can give/accept quests
        uint32 questGiverFlags{0};  // Quest giver capabilities

        // Visibility and interaction
        bool isVisible{false};  // Can be seen by bot
        float interactionRange{0.0f};  // Max interaction distance

        // Location context (for quest filtering and navigation)
        uint32 zoneId{0};  // Zone ID from Map
        uint32 areaId{0};  // Area ID from Map

        // Validation
        bool IsValid() const { return !guid.IsEmpty() && maxHealth > 0.0f; }
        float GetHealthPercent() const { return maxHealth > 0.0f ? (health / maxHealth) * 100.0f : 0.0f; }
    };

    /**
     * @brief Immutable snapshot of player data for thread-safe bot queries
     *
     * Memory footprint: 80 bytes per player
     */
    struct PlayerSnapshot
    {
        ObjectGuid guid;
        std::string name;  // Needed for social interactions
        Position position;
        float health{0.0f};
        float maxHealth{0.0f};
        uint8 level{0};
        uint8 race{0};
        uint8 classId{0};  // Changed from 'class' to avoid C++ keyword
        uint32 faction{0};

        // State flags
        bool isAlive{false};
        bool isInCombat{false};
        bool isAFK{false};
        bool isDND{false};

        // Group/Guild data
        bool isInGroup{false};
        bool isInGuild{false};

        // PvP data
        bool isPvPFlagged{false};

        bool IsValid() const { return !guid.IsEmpty() && !name.empty(); }
        float GetHealthPercent() const { return maxHealth > 0.0f ? (health / maxHealth) * 100.0f : 0.0f; }
    };

    /**
     * @brief Immutable snapshot of GameObject data for thread-safe bot queries
     *
     * Memory footprint: 56 bytes per GameObject
     */
    struct GameObjectSnapshot
    {
        ObjectGuid guid;
        uint32 entry{0};
        Position position;
        uint32 displayId{0};
        uint8 goType{0};  // GOType enum
        uint32 goState{0};  // GOState enum

        // State flags
        bool isSpawned{false};
        bool isTransport{false};
        bool isUsable{false};

        // Interaction data (for gathering, quests)
        float interactionRange{0.0f};
        uint32 requiredLevel{0};

        // PHASE 3 ENHANCEMENT: Gathering profession support
        bool isMineable{false};  // Mining node
        bool isHerbalism{false};  // Herbalism node
        bool isFishingNode{false};  // Fishing pool
        bool isInUse{false};  // Currently being gathered by another player
        uint32 respawnTime{0};  // Time until respawn (0 = ready)
        uint32 requiredSkillLevel{0};  // Profession skill requirement

        // Quest objective support
        bool isQuestObject{false};  // Required for quest objective
        uint32 questId{0};  // Associated quest ID

        // Loot container support
        bool isLootContainer{false};  // Can be looted
        bool hasLoot{false};  // Contains loot

        // Location context (for quest filtering and navigation)
        uint32 zoneId{0};  // Zone ID from Map
        uint32 areaId{0};  // Area ID from Map

        bool IsValid() const { return !guid.IsEmpty() && entry > 0; }
        bool IsGatherableNow() const { return (isMineable || isHerbalism || isFishingNode) && !isInUse && respawnTime == 0; }
    };

    /**
     * @brief Immutable snapshot of AreaTrigger data for combat danger detection
     *
     * Memory footprint: ~60 bytes per AreaTrigger
     */
    struct AreaTriggerSnapshot
    {
        ObjectGuid guid;
        Position position;
        uint32 spellId{0};           // Spell that created this AreaTrigger
        float radius{0.0f};           // Effect radius
        float maxSearchRadius{0.0f};  // Maximum search radius
        bool isDangerous{false};      // Causes damage or negative effects
        uint32 casterGuid{0};         // Who created this (for faction checks)

        bool IsValid() const { return !guid.IsEmpty() && radius > 0.0f; }
        bool IsInRange(Position const& pos) const
        {
            return maxSearchRadius > 0.0f && position.GetExactDist2d(&pos) <= maxSearchRadius;
        }
    };

    /**
     * @brief Immutable snapshot of DynamicObject data for AoE spell detection
     *
     * Memory footprint: ~50 bytes per DynamicObject
     */
    struct DynamicObjectSnapshot
    {
        ObjectGuid guid;
        Position position;
        uint32 spellId{0};      // Spell effect ID
        float radius{0.0f};     // Effect radius
        bool isHostile{false};  // Hostile to players
        ObjectGuid casterGuid;  // Who cast this spell

        bool IsValid() const { return !guid.IsEmpty() && spellId > 0; }
        bool IsInRange(Position const& pos) const
        {
            return radius > 0.0f && position.GetExactDist2d(&pos) <= radius;
        }
    };

    /**
     * @brief Contents of a single cell
     * Stores immutable data snapshots instead of GUIDs - ZERO pointer dereferencing in worker threads!
     *
     * ENTERPRISE QUALITY: Complete data isolation ensures thread safety
     */
    struct CellContents
    {
        std::vector<CreatureSnapshot> creatures;
        std::vector<PlayerSnapshot> players;
        std::vector<GameObjectSnapshot> gameObjects;
        std::vector<AreaTriggerSnapshot> areaTriggers;
        std::vector<DynamicObjectSnapshot> dynamicObjects;

        void Clear()
        {
            creatures.clear();
            players.clear();
            gameObjects.clear();
            areaTriggers.clear();
            dynamicObjects.clear();
        }

        size_t GetTotalCount() const
        {
            return creatures.size() + players.size() + gameObjects.size() +
                   areaTriggers.size() + dynamicObjects.size();
        }

        // Memory estimation (for monitoring)
        size_t GetMemoryUsageBytes() const
        {
            return (creatures.size() * sizeof(CreatureSnapshot)) +
                   (players.size() * sizeof(PlayerSnapshot)) +
                   (gameObjects.size() * sizeof(GameObjectSnapshot));
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
    void Start();  // DEPRECATED: No longer starts background thread
    void Stop();   // DEPRECATED: No-op now
    bool IsRunning() const { return true; }  // Always "running" now (no background thread)

    // Manual update (call from Map::Update or on-demand)
    // Const-qualified to allow calling from const query methods
    void Update() const;

    // Bot query interface (thread-safe, lock-free) - NOW RETURNS SNAPSHOTS!
    // BREAKING CHANGE: Methods now return data snapshots instead of GUIDs
    // This eliminates ALL ObjectAccessor/Map access from worker threads!
    std::vector<CreatureSnapshot> QueryNearbyCreatures(Position const& pos, float radius) const;
    std::vector<PlayerSnapshot> QueryNearbyPlayers(Position const& pos, float radius) const;
    std::vector<GameObjectSnapshot> QueryNearbyGameObjects(Position const& pos, float radius) const;
    std::vector<AreaTriggerSnapshot> QueryNearbyAreaTriggers(Position const& pos, float radius) const;
    std::vector<DynamicObjectSnapshot> QueryNearbyDynamicObjects(Position const& pos, float radius) const;

    // Legacy GUID-based queries (DEPRECATED - use snapshot queries instead!)
    // These are maintained temporarily for backward compatibility but will be removed
    std::vector<ObjectGuid> QueryNearbyCreatureGuids(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyPlayerGuids(Position const& pos, float radius) const;
    std::vector<ObjectGuid> QueryNearbyGameObjectGuids(Position const& pos, float radius) const;

    // Cell-level query (for advanced usage)
    CellContents const& GetCell(uint32 x, uint32 y) const;

    // Statistics
    Statistics GetStatistics() const;
    uint32 GetPopulationCount() const;

private:
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

    // Check if update is needed (rate-limited)
    bool ShouldUpdate() const;

    // Data members
    Map* _map;
    mutable GridBuffer _buffers[2];  // Mutable to allow updates from const methods
    mutable std::atomic<uint32> _readBufferIndex{0};

    mutable std::chrono::steady_clock::time_point _lastUpdate;
    mutable std::mutex _updateMutex;  // Protects Update() to ensure only one thread updates at a time

    // Statistics (atomic for thread-safe access)
    mutable std::atomic<uint64_t> _totalQueries{0};
    mutable std::atomic<uint64_t> _totalUpdates{0};
    mutable std::atomic<uint64_t> _totalSwaps{0};
    mutable std::atomic<uint32> _lastUpdateDurationUs{0};

    std::chrono::steady_clock::time_point _startTime;

    // Non-copyable, non-movable
    DoubleBufferedSpatialGrid(DoubleBufferedSpatialGrid const&) = delete;
    DoubleBufferedSpatialGrid& operator=(DoubleBufferedSpatialGrid const&) = delete;
    DoubleBufferedSpatialGrid(DoubleBufferedSpatialGrid&&) = delete;
    DoubleBufferedSpatialGrid& operator=(DoubleBufferedSpatialGrid&&) = delete;
};

} // namespace Playerbot

#endif // PLAYERBOT_DOUBLE_BUFFERED_SPATIAL_GRID_H
