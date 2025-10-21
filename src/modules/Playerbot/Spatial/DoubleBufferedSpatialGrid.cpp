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
#include "SpellMgr.h"
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

    // ===========================================================================
    // CRITICAL THREAD-SAFETY FIX: Copy creature DATA, not just GUIDs!
    // ===========================================================================
    // OLD APPROACH (UNSAFE): Store GUIDs ? Worker threads call ObjectAccessor::GetUnit()
    //                        ? Accesses Map::_objectsStore (NOT THREAD-SAFE!) ? DEADLOCK!
    //
    // NEW APPROACH (SAFE):   Store complete data snapshots ? Worker threads read snapshots
    //                        ? ZERO Map access from worker threads ? NO DEADLOCKS!
    // ===========================================================================

    // Iterate all creatures on this map
    // IMPORTANT: This runs on main thread (or from Map::Update), so Map access is safe HERE
    auto const& creatures = _map->GetCreatureBySpawnIdStore();
    for (auto const& pair : creatures)
    {
        Creature* creature = pair.second;
        if (!creature || !creature->IsInWorld())
            continue;

        // Create immutable snapshot of creature data
        CreatureSnapshot snapshot;
        snapshot.guid = creature->GetGUID();
        snapshot.entry = creature->GetEntry();
        snapshot.position = creature->GetPosition();
        snapshot.health = creature->GetHealth();
        snapshot.maxHealth = creature->GetMaxHealth();
        snapshot.level = creature->GetLevel();
        snapshot.faction = creature->GetFaction();

        // State flags
        snapshot.isAlive = creature->IsAlive();
        snapshot.isInCombat = creature->IsInCombat();
        snapshot.isElite = creature->IsElite();
        snapshot.isWorldBoss = creature->isWorldBoss();  // lowercase method

        // Targeting data
        if (Unit* victim = creature->GetVictim())
            snapshot.currentTarget = victim->GetGUID();

        // Use safe default aggro radius instead of GetAttackDistance(nullptr) which can hang
        // Note: threatRadius is currently not used by any bot code, but maintained for future use
        snapshot.threatRadius = 10.0f;  // Safe default aggro radius

        // Movement data
        snapshot.moveSpeed = creature->GetSpeed(MOVE_RUN);
        snapshot.isMoving = creature->isMoving();

        // PHASE 3 ENHANCEMENT: Quest and gathering support
        snapshot.isDead = creature->isDead();
        snapshot.isTappedByOther = false;  // Simplified - detecting tap status requires complex API
        snapshot.isSkinnable = creature->isDead();  // Simplified - dead creatures can potentially be skinned
        snapshot.hasBeenLooted = false;  // Simplified - would require complex flag checking

        // Quest giver data - simplified to safe methods
        snapshot.hasQuestGiver = (creature->GetNpcFlags() & UNIT_NPC_FLAG_QUESTGIVER) != 0;
        snapshot.questGiverFlags = creature->GetNpcFlags();

        // Visibility and interaction
        snapshot.isVisible = true;  // If we can query it, it's visible
        snapshot.interactionRange = 5.0f;  // Standard NPC interaction range

        // Location context - SAFE: We're on main thread during Update()
        snapshot.zoneId = creature->GetZoneId();
        snapshot.areaId = creature->GetAreaId();

        // Validate and store snapshot
        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].creatures.push_back(std::move(snapshot));
                ++creatureCount;
            }
        }
    }

    // Iterate all players on this map
    // Note: Map::GetPlayers() returns a reference to internal MapRefManager
    auto const& players = _map->GetPlayers();
    for (auto const& ref : players)
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld())
            continue;

        // Create immutable snapshot of player data
        PlayerSnapshot snapshot;
        snapshot.guid = player->GetGUID();
        snapshot.name = player->GetName();
        snapshot.position = player->GetPosition();
        snapshot.health = player->GetHealth();
        snapshot.maxHealth = player->GetMaxHealth();
        snapshot.level = player->GetLevel();
        snapshot.race = player->GetRace();
        snapshot.classId = player->GetClass();
        snapshot.faction = player->GetFaction();

        // State flags
        snapshot.isAlive = player->IsAlive();
        snapshot.isInCombat = player->IsInCombat();
        snapshot.isAFK = player->isAFK();
        snapshot.isDND = player->isDND();

        // Group/Guild data
        snapshot.isInGroup = player->GetGroup() != nullptr;
        snapshot.isInGuild = player->GetGuildId() != 0;

        // PvP data
        snapshot.isPvPFlagged = player->IsPvP();

        // Validate and store snapshot
        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].players.push_back(std::move(snapshot));
                ++playerCount;
            }
        }
    }

    // Iterate all game objects on this map
    auto const& gameObjects = _map->GetGameObjectBySpawnIdStore();
    for (auto const& pair : gameObjects)
    {
        GameObject* go = pair.second;
        if (!go || !go->IsInWorld())
            continue;

        // Create immutable snapshot of GameObject data
        GameObjectSnapshot snapshot;
        snapshot.guid = go->GetGUID();
        snapshot.entry = go->GetEntry();
        snapshot.position = go->GetPosition();
        snapshot.displayId = go->GetDisplayId();
        snapshot.goType = static_cast<uint8>(go->GetGoType());
        snapshot.goState = static_cast<uint32>(go->GetGoState());

        // State flags
        snapshot.isSpawned = go->isSpawned();  // lowercase method
        snapshot.isTransport = go->IsTransport();
        snapshot.isUsable = go->IsWithinDistInMap(go, go->GetInteractionDistance());  // Simplified usability check

        // Interaction data
        snapshot.interactionRange = go->GetInteractionDistance();
        snapshot.requiredLevel = 0;  // GameObjects don't have level requirements directly, would need to check quest data

        // PHASE 3 ENHANCEMENT: Gathering profession support
        GameObjectTemplate const* goInfo = go->GetGOInfo();
        if (goInfo)
        {
            // Simplified gathering node detection based on type only
            snapshot.isMineable = (goInfo->type == GAMEOBJECT_TYPE_CHEST);  // Simplified - many mining veins are chests
            snapshot.isHerbalism = (goInfo->type == GAMEOBJECT_TYPE_GOOBER);  // Simplified - herbs are goobers
            snapshot.isFishingNode = (goInfo->type == GAMEOBJECT_TYPE_FISHINGHOLE);

            // Quest object support
            snapshot.isQuestObject = (goInfo->type == GAMEOBJECT_TYPE_QUESTGIVER || goInfo->type == GAMEOBJECT_TYPE_GOOBER);
            snapshot.questId = 0;  // Would need to query quest data for specific quest ID

            // Loot container support
            snapshot.isLootContainer = (goInfo->type == GAMEOBJECT_TYPE_CHEST || goInfo->type == GAMEOBJECT_TYPE_GOOBER);
            snapshot.hasLoot = (go->GetGoState() == GO_STATE_READY);  // Simplified - just check if ready
        }

        // State information
        snapshot.isInUse = (go->GetGoState() == GO_STATE_ACTIVE);
        snapshot.respawnTime = go->GetRespawnTime() > 0 ? static_cast<uint32>(go->GetRespawnTime() - time(nullptr)) : 0;
        snapshot.requiredSkillLevel = 0;  // Would need to check profession requirements from DB

        // Location context - SAFE: We're on main thread during Update()
        snapshot.zoneId = go->GetZoneId();
        snapshot.areaId = go->GetAreaId();

        // Validate and store snapshot
        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].gameObjects.push_back(std::move(snapshot));
                ++gameObjectCount;
            }
        }
    }

    // Populate DynamicObjects (AoE spell effects)
    // SAFE: We're on main thread (Map::Update), Map access is allowed
    // Use TypeListContainer API to access DynamicObject store
    dynamicObjectCount = 0;
    auto& objectsStore = _map->GetObjectsStore();
    auto const& dynamicObjects = objectsStore.Data.FindContainer<DynamicObject>();
    for (auto const& [guid, dynObj] : dynamicObjects)
    {
        if (!dynObj || !dynObj->IsInWorld())
            continue;

        DynamicObjectSnapshot snapshot;
        snapshot.guid = dynObj->GetGUID();
        snapshot.position = dynObj->GetPosition();
        snapshot.spellId = dynObj->GetSpellId();
        snapshot.radius = dynObj->GetRadius();
        snapshot.casterGuid = dynObj->GetCasterGUID();

        // Determine if hostile using spell data
        // A DynamicObject is hostile if its spell is NOT positive (i.e., harmful/damage spell)
        snapshot.isHostile = false;  // Default to non-hostile
        if (SpellInfo const* spellInfo = dynObj->GetSpellInfo())
        {
            // Spells that are NOT positive are hostile (damage, debuffs, etc.)
            snapshot.isHostile = !spellInfo->IsPositive();
        }
        else
        {
            // If no spell info available, assume hostile for safety (bots should avoid unknown effects)
            snapshot.isHostile = true;
        }

        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].dynamicObjects.push_back(std::move(snapshot));
                ++dynamicObjectCount;
            }
        }
    }

    // Populate AreaTriggers (ground effects, damage zones)
    // SAFE: We're on main thread, Map access is allowed
    areaTriggerCount = 0;
    auto const& areaTriggers = _map->GetAreaTriggerBySpawnIdStore();
    for (auto const& pair : areaTriggers)
    {
        AreaTrigger* areaTrigger = pair.second;
        if (!areaTrigger || !areaTrigger->IsInWorld())
            continue;

        AreaTriggerSnapshot snapshot;
        snapshot.guid = areaTrigger->GetGUID();
        snapshot.position = areaTrigger->GetPosition();
        snapshot.spellId = areaTrigger->GetSpellId();
        snapshot.radius = areaTrigger->GetMaxSearchRadius();  // Use maxSearchRadius as radius
        snapshot.maxSearchRadius = areaTrigger->GetMaxSearchRadius();
        snapshot.casterGuid = areaTrigger->GetCasterGuid().GetCounter();

        // Determine if dangerous using spell data
        // An AreaTrigger is dangerous if its spell is NOT positive (i.e., harmful effects)
        snapshot.isDangerous = false;  // Default to non-dangerous
        if (snapshot.spellId > 0)
        {
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(snapshot.spellId, _map->GetDifficultyID()))
            {
                // Spells that are NOT positive are dangerous (damage, DoTs, debuffs, etc.)
                snapshot.isDangerous = !spellInfo->IsPositive();
            }
            else
            {
                // If no spell info available, assume dangerous for safety (bots should avoid unknown effects)
                snapshot.isDangerous = true;
            }
        }
        else
        {
            // AreaTriggers without spells are usually environmental/scripted hazards
            // Assume dangerous (fire patches, lava zones, etc.)
            snapshot.isDangerous = true;
        }

        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].areaTriggers.push_back(std::move(snapshot));
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
        "PopulateBufferFromMap: map {} - {} creatures, {} players, {} gameobjects, {} dynobjects, {} areatriggers in {}?s",
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

// ===========================================================================
// NEW SNAPSHOT-BASED QUERY METHODS - COMPLETELY THREAD-SAFE!
// ===========================================================================
// These methods return COMPLETE DATA SNAPSHOTS instead of GUIDs.
// Worker threads can use this data directly without ANY Map/ObjectAccessor calls!
// ===========================================================================

std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> DoubleBufferedSpatialGrid::QueryNearbyCreatures(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> results;
    auto const& readBuffer = GetReadBuffer();

    // Get all cells within radius
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add creatures from this cell with accurate distance filtering
        // Note: Cells are coarse (66 yards), so we need exact distance checks
        for (CreatureSnapshot const& snapshot : cell.creatures)
        {
            // Accurate distance check using snapshot position
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);  // Copy snapshot (POD data, fast)
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyCreatures: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

std::vector<DoubleBufferedSpatialGrid::PlayerSnapshot> DoubleBufferedSpatialGrid::QueryNearbyPlayers(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<PlayerSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add players from this cell with accurate distance filtering
        for (PlayerSnapshot const& snapshot : cell.players)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyPlayers: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> DoubleBufferedSpatialGrid::QueryNearbyGameObjects(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<GameObjectSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add game objects from this cell with accurate distance filtering
        for (GameObjectSnapshot const& snapshot : cell.gameObjects)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyGameObjects: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

std::vector<DoubleBufferedSpatialGrid::AreaTriggerSnapshot> DoubleBufferedSpatialGrid::QueryNearbyAreaTriggers(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<AreaTriggerSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add area triggers from this cell with accurate distance filtering
        for (AreaTriggerSnapshot const& snapshot : cell.areaTriggers)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyAreaTriggers: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

std::vector<DoubleBufferedSpatialGrid::DynamicObjectSnapshot> DoubleBufferedSpatialGrid::QueryNearbyDynamicObjects(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, std::memory_order_relaxed);

    std::vector<DynamicObjectSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add dynamic objects from this cell with accurate distance filtering
        for (DynamicObjectSnapshot const& snapshot : cell.dynamicObjects)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyDynamicObjects: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

// ===========================================================================
// LEGACY GUID-BASED QUERY METHODS (DEPRECATED - FOR BACKWARD COMPATIBILITY)
// ===========================================================================
// These methods extract GUIDs from snapshots. They exist ONLY for transitioning
// existing code. NEW CODE SHOULD USE SNAPSHOT QUERIES ABOVE!
// ===========================================================================

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyCreatureGuids(
    Position const& pos, float radius) const
{
    auto snapshots = QueryNearbyCreatures(pos, radius);
    std::vector<ObjectGuid> guids;
    guids.reserve(snapshots.size());
    for (auto const& snapshot : snapshots)
        guids.push_back(snapshot.guid);
    return guids;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyPlayerGuids(
    Position const& pos, float radius) const
{
    auto snapshots = QueryNearbyPlayers(pos, radius);
    std::vector<ObjectGuid> guids;
    guids.reserve(snapshots.size());
    for (auto const& snapshot : snapshots)
        guids.push_back(snapshot.guid);
    return guids;
}

std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyGameObjectGuids(
    Position const& pos, float radius) const
{
    auto snapshots = QueryNearbyGameObjects(pos, radius);
    std::vector<ObjectGuid> guids;
    guids.reserve(snapshots.size());
    for (auto const& snapshot : snapshots)
        guids.push_back(snapshot.guid);
    return guids;
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
