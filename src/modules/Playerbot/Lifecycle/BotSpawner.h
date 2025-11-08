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
#include "Threading/LockHierarchy.h"
#include "ObjectGuid.h"
#include "Lifecycle/SpawnRequest.h"
#include "Lifecycle/BotPopulationManager.h"
#include "Core/DI/Interfaces/IBotSpawner.h"
#include "DatabaseEnv.h"
#include "CharacterDatabase.h"
#include "LoginDatabase.h"
// Phase 2: Adaptive Throttling System
#include "Lifecycle/ResourceMonitor.h"
#include "Lifecycle/SpawnCircuitBreaker.h"
#include "Lifecycle/SpawnPriorityQueue.h"
#include "Lifecycle/AdaptiveSpawnThrottler.h"
#include "Lifecycle/StartupSpawnOrchestrator.h"
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

class TC_GAME_API BotSpawner final : public IBotSpawner
{
public:
    BotSpawner(BotSpawner const&) = delete;
    BotSpawner& operator=(BotSpawner const&) = delete;

    static BotSpawner* instance();

    // IBotSpawner interface implementation
    bool Initialize() override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    // Configuration
    void LoadConfig() override;
    SpawnConfig const& GetConfig() const override { return _config; }
    void SetConfig(SpawnConfig const& config) override { _config = config; }

    // Single bot spawning
    bool SpawnBot(SpawnRequest const& request) override;

    // Batch spawning
    uint32 SpawnBots(std::vector<SpawnRequest> const& requests) override;

    // Population management
    void SpawnToPopulationTarget() override;
    void UpdatePopulationTargets() override;
    void DespawnBot(ObjectGuid guid, bool forced = false) override;
    bool DespawnBot(ObjectGuid guid, std::string const& reason) override;
    void DespawnAllBots() override;

    // Zone management
    void UpdateZonePopulation(uint32 zoneId, uint32 mapId) override;
    void UpdateZonePopulationSafe(uint32 zoneId, uint32 mapId) override;
    ZonePopulation GetZonePopulation(uint32 zoneId) const override;
    std::vector<ZonePopulation> GetAllZonePopulations() const override;

    // Bot tracking
    bool IsBotActive(ObjectGuid guid) const override;
    uint32 GetActiveBotCount() const override;
    uint32 GetActiveBotCount(uint32 zoneId) const override;
    uint32 GetActiveBotCount(uint32 mapId, bool useMapId) const override;
    std::vector<ObjectGuid> GetActiveBotsInZone(uint32 zoneId) const override;

    // Statistics
    SpawnStats const& GetStats() const override { return _stats; }
    void ResetStats() override;

    // Player login detection
    void OnPlayerLogin() override;
    void CheckAndSpawnForPlayers() override;

    // Population caps
    bool CanSpawnMore() const override;
    bool CanSpawnInZone(uint32 zoneId) const override;
    bool CanSpawnOnMap(uint32 mapId) const override;

    // Runtime control
    void SetEnabled(bool enabled) override { _enabled = enabled; }
    bool IsEnabled() const override { return _enabled.load(); }

    // Configuration methods
    void SetMaxBots(uint32 maxBots) override { _config.maxBotsTotal = maxBots; }
    void SetBotToPlayerRatio(float ratio) override { _config.botToPlayerRatio = ratio; }

    // Chat command support - Create new bot character and spawn it
    bool CreateAndSpawnBot(uint32 masterAccountId, uint8 classId, uint8 race, uint8 gender, std::string const& name, ObjectGuid& outCharacterGuid) override;

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

    // Phase 2: Priority assignment for spawn requests
    SpawnPriority DeterminePriority(SpawnRequest const& request) const;

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
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_SPAWNER> _zoneMutex; // TODO: Replace with lock-free hash map
    std::unordered_map<uint32, ZonePopulation> _zonePopulations; // zoneId -> population data

    // Bot tracking - lock-free concurrent structures
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_SPAWNER> _botMutex; // TODO: Replace with concurrent hash map
    std::unordered_map<ObjectGuid, uint32> _activeBots; // guid -> zoneId
    std::unordered_map<uint32, std::vector<ObjectGuid>> _botsByZone; // zoneId -> bot guids

    // LOCK-FREE async spawning queue for high throughput
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BOT_SPAWNER> _spawnQueueMutex; // TODO: Replace with lock-free queue
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
    std::atomic<bool> _firstPlayerSpawned{false};
    std::atomic<uint32> _lastRealPlayerCount{0};
    std::atomic<bool> _inCheckAndSpawn{false}; // Prevent reentrant calls causing deadlock
    bool _initialCalculationDone = false; // Track if initial zone calculation happened in Initialize()

    // ========================================================================
    // Phase 2: Adaptive Throttling System Components
    // ========================================================================
    // Enterprise-grade spawn rate control with resource monitoring,
    // circuit breaker protection, priority queueing, and phased startup

    /**
     * @brief Real-time server resource monitor
     *
     * Monitors CPU, memory, DB connections, and map instances.
     * Provides pressure level detection (NORMAL/ELEVATED/HIGH/CRITICAL)
     * and automatic spawn rate multipliers.
     */
    ResourceMonitor _resourceMonitor;

    /**
     * @brief Circuit breaker for spawn failure protection
     *
     * Implements circuit breaker pattern (CLOSED/OPEN/HALF_OPEN).
     * Automatically detects failure patterns and blocks spawning
     * when failure rate exceeds threshold (>10%).
     */
    SpawnCircuitBreaker _circuitBreaker;

    /**
     * @brief Priority-based spawn request queue
     *
     * 4-tier priority system: CRITICAL → HIGH → NORMAL → LOW
     * Ensures guild leaders and raid leaders spawn first.
     */
    SpawnPriorityQueue _priorityQueue;

    /**
     * @brief Adaptive spawn throttler (core throttling logic)
     *
     * Integrates ResourceMonitor and CircuitBreaker to dynamically
     * adjust spawn interval (50ms-5000ms = 20-0.2 bots/sec).
     * Includes burst prevention (max 50 spawns per 10s).
     */
    AdaptiveSpawnThrottler _throttler;

    /**
     * @brief Phased startup orchestrator
     *
     * Manages graduated bot spawning in 4 phases:
     * - Phase 1: CRITICAL bots (0-2min, 100 bots)
     * - Phase 2: HIGH priority (2-5min, 500 bots)
     * - Phase 3: NORMAL bots (5-15min, 3000 bots)
     * - Phase 4: LOW priority (15-30min, 1400 bots)
     */
    StartupSpawnOrchestrator _orchestrator;

    /**
     * @brief Phase 2 initialization status
     *
     * Tracks whether Phase 2 components have been successfully initialized.
     * Used to prevent premature spawning before throttling system is ready.
     */
    bool _phase2Initialized = false;

    // ========================================================================
    // End Phase 2 Components
    // ========================================================================

    static constexpr uint32 POPULATION_UPDATE_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 TARGET_CALCULATION_INTERVAL = 2000; // 2 seconds for faster response
};

#define sBotSpawner BotSpawner::instance()

} // namespace Playerbot