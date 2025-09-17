/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <memory>
#include <vector>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <tbb/concurrent_queue.h>

namespace Playerbot
{

// Forward declarations
class BotSession;
class BotScheduler;

// Spawn request structure
struct SpawnRequest
{
    enum Type { RANDOM, SPECIFIC_CHARACTER, SPECIFIC_ZONE, GROUP_MEMBER };

    Type type = RANDOM;
    uint32 accountId = 0;
    ObjectGuid characterGuid;
    uint32 zoneId = 0;
    uint32 mapId = 0;
    uint8 minLevel = 0;
    uint8 maxLevel = 0;
    uint8 classFilter = 0;
    uint8 raceFilter = 0;

    // Callback on spawn completion
    std::function<void(bool success, ObjectGuid guid)> callback;
};

// Spawn configuration
struct SpawnConfig
{
    uint32 maxBotsTotal = 500;
    uint32 maxBotsPerZone = 50;
    uint32 maxBotsPerMap = 200;
    uint32 spawnBatchSize = 10;
    uint32 spawnDelayMs = 100;
    bool enableDynamicSpawning = true;
    bool respectPopulationCaps = true;
    float botToPlayerRatio = 2.0f;
};

// Zone population data
struct ZonePopulation
{
    uint32 zoneId;
    uint32 mapId;
    uint32 playerCount;
    uint32 botCount;
    uint32 targetBotCount;
    uint8 minLevel;
    uint8 maxLevel;
    float botDensity;
    std::chrono::system_clock::time_point lastUpdate;
};

// Spawn statistics
struct SpawnStats
{
    std::atomic<uint32> totalSpawned{0};
    std::atomic<uint32> totalDespawned{0};
    std::atomic<uint32> currentlyActive{0};
    std::atomic<uint32> peakConcurrent{0};
    std::atomic<uint32> failedSpawns{0};
    std::atomic<uint64> totalSpawnTime{0}; // microseconds
    std::atomic<uint32> spawnAttempts{0};

    float GetAverageSpawnTime() const {
        uint32 attempts = spawnAttempts.load();
        return attempts > 0 ? static_cast<float>(totalSpawnTime.load()) / attempts / 1000.0f : 0.0f; // ms
    }

    float GetSuccessRate() const {
        uint32 attempts = spawnAttempts.load();
        uint32 failures = failedSpawns.load();
        return attempts > 0 ? static_cast<float>(attempts - failures) / attempts * 100.0f : 0.0f;
    }
};

class TC_GAME_API BotSpawner
{
public:
    BotSpawner(BotSpawner const&) = delete;
    BotSpawner& operator=(BotSpawner const&) = delete;

    static BotSpawner* instance();

    bool Initialize();
    void Shutdown();
    void Update(uint32 diff);

    // Configuration
    void LoadConfig();
    SpawnConfig const& GetConfig() const { return _config; }
    void SetConfig(SpawnConfig const& config) { _config = config; }

    // Single bot spawning
    bool SpawnBot(SpawnRequest const& request);

    // Batch spawning
    uint32 SpawnBots(std::vector<SpawnRequest> const& requests);

    // Population management
    void SpawnToPopulationTarget();
    void UpdatePopulationTargets();
    void DespawnBot(ObjectGuid guid, bool forced = false);
    void DespawnAllBots();

    // Zone management
    void UpdateZonePopulation(uint32 zoneId, uint32 mapId);
    ZonePopulation GetZonePopulation(uint32 zoneId) const;
    std::vector<ZonePopulation> GetAllZonePopulations() const;

    // Bot tracking
    bool IsBotActive(ObjectGuid guid) const;
    uint32 GetActiveBotCount() const;
    uint32 GetActiveBotCount(uint32 zoneId) const;
    uint32 GetActiveBotCount(uint32 mapId, bool useMapId) const;
    std::vector<ObjectGuid> GetActiveBotsInZone(uint32 zoneId) const;

    // Statistics
    SpawnStats const& GetStats() const { return _stats; }
    void ResetStats();

    // Population caps
    bool CanSpawnMore() const;
    bool CanSpawnInZone(uint32 zoneId) const;
    bool CanSpawnOnMap(uint32 mapId) const;

private:
    BotSpawner() = default;
    ~BotSpawner() = default;

    // Internal spawning
    bool SpawnBotInternal(SpawnRequest const& request);
    bool CreateBotSession(uint32 accountId, ObjectGuid characterGuid);
    bool ValidateSpawnRequest(SpawnRequest const& request) const;

    // Character selection
    ObjectGuid SelectCharacterForSpawn(SpawnRequest const& request);
    std::vector<ObjectGuid> GetAvailableCharacters(uint32 accountId, SpawnRequest const& request);

    // Population calculations
    void CalculateZoneTargets();
    uint32 CalculateTargetBotCount(ZonePopulation const& zone) const;

    // Internal data
    SpawnConfig _config;
    SpawnStats _stats;

    mutable std::mutex _zoneMutex;
    std::unordered_map<uint32, ZonePopulation> _zonePopulations; // zoneId -> population data

    mutable std::mutex _botMutex;
    std::unordered_map<ObjectGuid, uint32> _activeBots; // guid -> zoneId
    std::unordered_map<uint32, std::vector<ObjectGuid>> _botsByZone; // zoneId -> bot guids

    // Async spawning queue
    tbb::concurrent_queue<SpawnRequest> _spawnQueue;
    std::atomic<bool> _processingQueue{false};

    // Timing
    uint32 _lastPopulationUpdate = 0;
    uint32 _lastTargetCalculation = 0;

    static constexpr uint32 POPULATION_UPDATE_INTERVAL = 10000; // 10 seconds
    static constexpr uint32 TARGET_CALCULATION_INTERVAL = 60000; // 1 minute
};

#define sBotSpawner BotSpawner::instance()

} // namespace Playerbot