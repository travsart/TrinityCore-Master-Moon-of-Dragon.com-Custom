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
#include "Lifecycle/SpawnRequest.h"
#include "Lifecycle/BotPopulationManager.h"
#include "DatabaseEnv.h"
#include "CharacterDatabase.h"
#include "LoginDatabase.h"
#include <memory>
#include <vector>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <queue>

namespace Playerbot
{

// Forward declarations
class BotSession;
class BotScheduler;

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

// ZonePopulation defined in BotPopulationManager.h

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
    bool DespawnBot(ObjectGuid guid, std::string const& reason);
    void DespawnAllBots();

    // Zone management
    void UpdateZonePopulation(uint32 zoneId, uint32 mapId);
    void UpdateZonePopulationSafe(uint32 zoneId, uint32 mapId);
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

    // Runtime control
    void SetEnabled(bool enabled) { _enabled = enabled; }
    bool IsEnabled() const { return _enabled.load(); }

    // Configuration methods
    void SetMaxBots(uint32 maxBots) { _config.maxBotsTotal = maxBots; }
    void SetBotToPlayerRatio(float ratio) { _config.botToPlayerRatio = ratio; }

    // Allow adapter access to constructor
    friend class std::unique_ptr<BotSpawner>;
    friend std::unique_ptr<BotSpawner> std::make_unique<BotSpawner>();
    friend class std::default_delete<BotSpawner>;

private:
    BotSpawner();  // Explicit constructor for debugging
    ~BotSpawner() = default;

    // Internal spawning
    bool SpawnBotInternal(SpawnRequest const& request);
    bool CreateBotSession(uint32 accountId, ObjectGuid characterGuid);
    bool ValidateSpawnRequest(SpawnRequest const& request) const;

    // Character selection
    ObjectGuid SelectCharacterForSpawn(SpawnRequest const& request);
    void SelectCharacterForSpawnAsync(SpawnRequest const& request, std::function<void(ObjectGuid)> callback);
    std::vector<ObjectGuid> GetAvailableCharacters(uint32 accountId, SpawnRequest const& request);
    void GetAvailableCharactersAsync(uint32 accountId, SpawnRequest const& request, std::function<void(std::vector<ObjectGuid>)> callback);
    void SelectCharacterAsyncRecursive(std::vector<uint32> accounts, size_t index, SpawnRequest const& request, std::function<void(ObjectGuid)> callback);
    void ContinueSpawnWithCharacter(ObjectGuid characterGuid, SpawnRequest const& request);
    uint32 GetAccountIdFromCharacter(ObjectGuid characterGuid) const;

    // Character creation
    ObjectGuid CreateCharacterForAccount(uint32 accountId, SpawnRequest const& request);
    ObjectGuid CreateBotCharacter(uint32 accountId);

    // Population calculations
    void CalculateZoneTargets();
    uint32 CalculateTargetBotCount(ZonePopulation const& zone) const;

    // Database safety - prevent statement index corruption and memory corruption
    CharacterDatabasePreparedStatement* GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName) const;
    LoginDatabasePreparedStatement* GetSafeLoginPreparedStatement(LoginDatabaseStatements statementId, const char* statementName) const;

    // Internal data
    SpawnConfig _config;
    SpawnStats _stats;

    // LOCK-FREE DATA STRUCTURES for 5000 bot scalability
    // Zone population tracking - lock-free atomic operations
    mutable std::mutex _zoneMutex; // TODO: Replace with lock-free hash map
    std::unordered_map<uint32, ZonePopulation> _zonePopulations; // zoneId -> population data

    // Bot tracking - lock-free concurrent structures
    mutable std::mutex _botMutex; // TODO: Replace with concurrent hash map
    std::unordered_map<ObjectGuid, uint32> _activeBots; // guid -> zoneId
    std::unordered_map<uint32, std::vector<ObjectGuid>> _botsByZone; // zoneId -> bot guids

    // LOCK-FREE async spawning queue for high throughput
    mutable std::mutex _spawnQueueMutex; // TODO: Replace with lock-free queue
    std::queue<SpawnRequest> _spawnQueue;
    std::atomic<bool> _processingQueue{false};

    // Lock-free counters for hot path operations
    std::atomic<uint32> _activeBotCount{0};
    std::atomic<uint32> _totalSpawnRequests{0};

    // Timing
    uint32 _lastPopulationUpdate = 0;
    uint32 _lastTargetCalculation = 0;

    // Runtime state
    std::atomic<bool> _enabled{true};

    static constexpr uint32 POPULATION_UPDATE_INTERVAL = 10000; // 10 seconds
    static constexpr uint32 TARGET_CALCULATION_INTERVAL = 5000; // 5 seconds for testing
};

#define sBotSpawner BotSpawner::instance()

} // namespace Playerbot