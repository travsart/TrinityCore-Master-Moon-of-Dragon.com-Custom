/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Interfaces/IBotSpawner.h"
#include "Define.h"
#include <memory>

namespace Playerbot
{

// Forward declarations
class BotSpawnOrchestrator;

/**
 * @class BotSpawnerAdapter
 * @brief Adapter that makes BotSpawnOrchestrator implement IBotSpawner interface
 *
 * ADAPTER PATTERN: Provides backward compatibility by adapting the new
 * orchestrator-based architecture to the legacy BotSpawner interface.
 *
 * Benefits:
 * - Maintains API compatibility during refactoring
 * - Allows gradual migration to new architecture
 * - Provides clean interface for external consumers
 * - Enables dependency injection and testing
 * - Isolates interface changes from implementation changes
 *
 * Usage:
 * ```cpp
 * auto spawner = std::make_unique<BotSpawnerAdapter>();
 * spawner->Initialize();
 * spawner->SpawnBot(request);  // Delegates to orchestrator
 * ```
 */
class TC_GAME_API BotSpawnerAdapter : public IBotSpawner
{
public:
    BotSpawnerAdapter();
    ~BotSpawnerAdapter() override;

    // === LIFECYCLE (IBotSpawner) ===
    bool Initialize() override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    // === SPAWNING INTERFACE (IBotSpawner) ===
    bool SpawnBot(SpawnRequest const& request) override;
    uint32 SpawnBots(std::vector<SpawnRequest> const& requests) override;

    // === POPULATION MANAGEMENT (IBotSpawner) ===
    void SpawnToPopulationTarget() override;
    void UpdatePopulationTargets() override;
    bool DespawnBot(ObjectGuid guid, std::string const& reason) override;
    void DespawnBot(ObjectGuid guid, bool forced = false) override;

    // === QUERIES (IBotSpawner) ===
    uint32 GetActiveBotCount() const override;
    uint32 GetActiveBotCount(uint32 zoneId) const override;
    bool CanSpawnMore() const override;
    bool CanSpawnInZone(uint32 zoneId) const override;

    // === CONFIGURATION (IBotSpawner) ===
    void SetMaxBots(uint32 maxBots) override;
    void SetBotToPlayerRatio(float ratio) override;
    bool IsEnabled() const override;
    void SetEnabled(bool enabled) override;

    // === ADAPTER-SPECIFIC METHODS ===
    BotSpawnOrchestrator* GetOrchestrator() const { return _orchestrator.get(); }

    // Performance metrics access
    struct AdapterStats
    {
        uint32 callsToSpawnBot = 0;
        uint32 callsToSpawnBots = 0;
        uint32 callsToDespawnBot = 0;
        uint32 queryCalls = 0;
        uint64 avgCallDurationUs = 0;
    };

    AdapterStats const& GetAdapterStats() const { return _stats; }
    void ResetAdapterStats();

private:
    // === ORCHESTRATOR DELEGATION ===
    std::unique_ptr<BotSpawnOrchestrator> _orchestrator;

    // === CONFIGURATION STATE ===
    bool _enabled = true;
    uint32 _maxBots = 1000;
    float _botToPlayerRatio = 2.0f;

    // === PERFORMANCE TRACKING ===
    mutable AdapterStats _stats;
    void RecordApiCall(uint64 durationMicroseconds);

    // === INITIALIZATION HELPERS ===
    bool InitializeOrchestrator();
    void ConfigureOrchestrator();

    // Non-copyable
    BotSpawnerAdapter(BotSpawnerAdapter const&) = delete;
    BotSpawnerAdapter& operator=(BotSpawnerAdapter const&) = delete;
};

/**
 * @class LegacyBotSpawnerAdapter
 * @brief Adapter for legacy BotSpawner class during migration
 *
 * This adapter allows the old BotSpawner to work with the new interface
 * during the transition period. It can be removed once migration is complete.
 */
class TC_GAME_API LegacyBotSpawnerAdapter : public IBotSpawner
{
public:
    LegacyBotSpawnerAdapter();
    ~LegacyBotSpawnerAdapter() override;

    // IBotSpawner implementation delegating to legacy BotSpawner
    bool Initialize() override;
    void Shutdown() override;
    void Update(uint32 diff) override;

    bool SpawnBot(SpawnRequest const& request) override;
    uint32 SpawnBots(std::vector<SpawnRequest> const& requests) override;

    void SpawnToPopulationTarget() override;
    void UpdatePopulationTargets() override;
    bool DespawnBot(ObjectGuid guid, std::string const& reason) override;
    void DespawnBot(ObjectGuid guid, bool forced = false) override;

    uint32 GetActiveBotCount() const override;
    uint32 GetActiveBotCount(uint32 zoneId) const override;
    bool CanSpawnMore() const override;
    bool CanSpawnInZone(uint32 zoneId) const override;

    void SetMaxBots(uint32 maxBots) override;
    void SetBotToPlayerRatio(float ratio) override;
    bool IsEnabled() const override;
    void SetEnabled(bool enabled) override;

private:
    std::unique_ptr<class BotSpawner> _legacySpawner;
    bool _migrationMode = true;  // Flag to indicate we're in migration mode
};

/**
 * @class BotSpawnerFactory
 * @brief Factory for creating appropriate bot spawner implementations
 *
 * Allows runtime selection of spawner implementation based on configuration.
 */
class TC_GAME_API BotSpawnerFactory
{
public:
    enum class SpawnerType
    {
        LEGACY,      // Use old BotSpawner implementation
        ORCHESTRATED, // Use new orchestrator-based implementation
        AUTO         // Auto-detect based on configuration
    };

    static std::unique_ptr<IBotSpawner> CreateSpawner(SpawnerType type = SpawnerType::AUTO);
    static SpawnerType DetectBestSpawnerType();
    static std::string GetSpawnerTypeName(SpawnerType type);

private:
    static bool IsOrchestratorAvailable();
    static bool ShouldUseLegacySpawner();
};

} // namespace Playerbot