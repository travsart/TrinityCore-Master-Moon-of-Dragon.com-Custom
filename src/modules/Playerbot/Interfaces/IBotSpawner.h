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
#include <functional>
#include <vector>
#include <memory>

namespace Playerbot
{

/**
 * @interface IBotSpawner
 * @brief Abstract interface for bot spawning operations
 *
 * ABSTRACTION LAYER: Provides clean interface for bot spawning that can be
 * implemented by different spawning strategies (direct, orchestrated, etc.)
 *
 * Benefits:
 * - Clean separation of concerns
 * - Easy testing through mocking
 * - Support for different spawning implementations
 * - Clear API contract for consumers
 * - Facilitates dependency injection
 */
class IBotSpawner
{
public:
    virtual ~IBotSpawner() = default;

    // === LIFECYCLE ===
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // === SPAWNING INTERFACE ===
    virtual bool SpawnBot(SpawnRequest const& request) = 0;
    virtual uint32 SpawnBots(std::vector<SpawnRequest> const& requests) = 0;

    // === POPULATION MANAGEMENT ===
    virtual void SpawnToPopulationTarget() = 0;
    virtual void UpdatePopulationTargets() = 0;
    virtual bool DespawnBot(ObjectGuid guid, std::string const& reason) = 0;
    virtual void DespawnBot(ObjectGuid guid, bool forced = false) = 0;

    // === QUERIES ===
    virtual uint32 GetActiveBotCount() const = 0;
    virtual uint32 GetActiveBotCount(uint32 zoneId) const = 0;
    virtual bool CanSpawnMore() const = 0;
    virtual bool CanSpawnInZone(uint32 zoneId) const = 0;

    // === CONFIGURATION ===
    virtual void SetMaxBots(uint32 maxBots) = 0;
    virtual void SetBotToPlayerRatio(float ratio) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
};

/**
 * @interface IBotSpawnerCallback
 * @brief Callback interface for async spawning operations
 */
class IBotSpawnerCallback
{
public:
    virtual ~IBotSpawnerCallback() = default;

    virtual void OnSpawnCompleted(ObjectGuid botGuid, bool success, std::string const& details = "") = 0;
    virtual void OnSpawnFailed(SpawnRequest const& request, std::string const& reason) = 0;
    virtual void OnPopulationChanged(uint32 zoneId, uint32 oldCount, uint32 newCount) = 0;
};

/**
 * @interface IBotSpawnerEvents
 * @brief Event interface for spawner notifications
 */
class IBotSpawnerEvents
{
public:
    virtual ~IBotSpawnerEvents() = default;

    using SpawnCallback = std::function<void(bool success, ObjectGuid botGuid)>;
    using PopulationCallback = std::function<void(uint32 zoneId, uint32 botCount)>;

    virtual void RegisterSpawnCallback(SpawnCallback callback) = 0;
    virtual void RegisterPopulationCallback(PopulationCallback callback) = 0;
    virtual void UnregisterCallbacks() = 0;
};

} // namespace Playerbot