/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include "BotSpawner.h"
#include "BotSessionMgr.h"
#include "BotAccountMgr.h"
#include "PlayerbotConfig.h"
#include "PlayerbotDatabase.h"
#include "BotCharacterDistribution.h"
#include "BotNameMgr.h"
#include "Config/PlayerbotLog.h"
#include "World.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "CharacterPackets.h"
#include "DatabaseEnv.h"
#include "ObjectGuid.h"
#include "MotionMaster.h"
#include "RealmList.h"
#include "DB2Stores.h"
#include "WorldSession.h"
#include <algorithm>
#include <chrono>

namespace Playerbot
{

BotSpawner::BotSpawner()
{
    TC_LOG_INFO("module.playerbot", "BotSpawner::BotSpawner() constructor called - starting member initialization");
    TC_LOG_INFO("module.playerbot", "BotSpawner::BotSpawner() constructor completed successfully");
}

BotSpawner* BotSpawner::instance()
{
    TC_LOG_INFO("module.playerbot", "BotSpawner::instance() called - about to create static instance");
    static BotSpawner instance;
    TC_LOG_INFO("module.playerbot", "BotSpawner::instance() static instance created successfully");
    return &instance;
}

bool BotSpawner::Initialize()
{
    TC_LOG_PLAYERBOT_INFO("Initializing Bot Spawner...");
    TC_LOG_INFO("module.playerbot", "BotSpawner: About to start initialization steps...");

    TC_LOG_INFO("module.playerbot", "BotSpawner: Step 1 - LoadConfig()...");
    LoadConfig();
    TC_LOG_INFO("module.playerbot", "BotSpawner: LoadConfig() completed successfully");

    TC_LOG_INFO("module.playerbot", "BotSpawner: Step 2 - UpdatePopulationTargets()...");
    // Initialize zone populations
    UpdatePopulationTargets();

    TC_LOG_INFO("module.playerbot",
        "Bot Spawner initialized - Max Total: {}, Max Per Zone: {}, Max Per Map: {}",
        _config.maxBotsTotal, _config.maxBotsPerZone, _config.maxBotsPerMap);

    // Start periodic update timer for automatic spawning
    _lastPopulationUpdate = getMSTime();
    _lastTargetCalculation = getMSTime();

    // Trigger immediate initial spawn check
    TC_LOG_INFO("module.playerbot", "BotSpawner: Step 3 - Check enableDynamicSpawning: {}", _config.enableDynamicSpawning ? "true" : "false");
    if (_config.enableDynamicSpawning)
    {
        TC_LOG_INFO("module.playerbot", "Triggering initial spawn check...");
        TC_LOG_INFO("module.playerbot", "BotSpawner: Step 4 - CalculateZoneTargets()...");
        CalculateZoneTargets();
        TC_LOG_INFO("module.playerbot", "BotSpawner: Step 5 - SpawnToPopulationTarget()...");
        SpawnToPopulationTarget();
    }

    return true;
}

void BotSpawner::Shutdown()
{
    TC_LOG_INFO("module.playerbot.spawner", "Shutting down Bot Spawner...");

    // Despawn all active bots
    DespawnAllBots();

    // Clear data structures
    {
        std::lock_guard<std::mutex> lock(_zoneMutex);
        _zonePopulations.clear();
    }

    {
        std::lock_guard<std::mutex> lock(_botMutex);
        _activeBots.clear();
        _botsByZone.clear();
    }

    TC_LOG_INFO("module.playerbot.spawner", "Bot Spawner shutdown complete");
}

void BotSpawner::Update(uint32 diff)
{
    static uint32 updateCounter = 0;
    if (++updateCounter % 500 == 0) // Log every 500 updates
    {
        printf("=== PLAYERBOT DEBUG: BotSpawner::Update() called #%u (enableDynamicSpawning=%s) ===\n",
               updateCounter, _config.enableDynamicSpawning ? "true" : "false");
        fflush(stdout);
        // DISABLED: TC_LOG_INFO causes crash
        // TC_LOG_INFO("module.playerbot", "BotSpawner::Update called - Update #{}, enableDynamicSpawning: {}",
        //            updateCounter, _config.enableDynamicSpawning ? "true" : "false");
    }

    uint32 currentTime = getMSTime();

    // Process spawn queue (mutex-protected)
    bool queueHasItems = false;
    {
        std::lock_guard<std::mutex> lock(_spawnQueueMutex);
        queueHasItems = !_spawnQueue.empty();
    }

    if (queueHasItems)
    {
        printf("=== PLAYERBOT DEBUG: Queue has items, processing=%s ===\n",
               _processingQueue.load() ? "true" : "false");
        fflush(stdout);
    }

    if (!_processingQueue.load() && queueHasItems)
    {
        printf("=== PLAYERBOT DEBUG: Starting queue processing ===\n");
        fflush(stdout);
        _processingQueue.store(true);

        // Extract batch of requests to minimize lock time
        std::vector<SpawnRequest> requestBatch;
        {
            std::lock_guard<std::mutex> lock(_spawnQueueMutex);
            uint32 batchSize = std::min(_config.spawnBatchSize, static_cast<uint32>(_spawnQueue.size()));
            requestBatch.reserve(batchSize);

            printf("=== PLAYERBOT DEBUG: Queue size: %zu, batch size: %u ===\n",
                   _spawnQueue.size(), batchSize);
            fflush(stdout);

            for (uint32 i = 0; i < batchSize && !_spawnQueue.empty(); ++i)
            {
                requestBatch.push_back(_spawnQueue.front());
                _spawnQueue.pop();
            }
        }

        printf("=== PLAYERBOT DEBUG: Processing %zu spawn requests ===\n", requestBatch.size());
        fflush(stdout);

        // Process requests outside the lock
        for (SpawnRequest const& request : requestBatch)
        {
            printf("=== PLAYERBOT DEBUG: Calling SpawnBotInternal for zone %u ===\n", request.zoneId);
            fflush(stdout);
            SpawnBotInternal(request);
        }

        printf("=== PLAYERBOT DEBUG: Finished processing queue batch ===\n");
        fflush(stdout);
        _processingQueue.store(false);
    }

    // Update zone populations periodically - FIXED VERSION
    if (currentTime - _lastPopulationUpdate > POPULATION_UPDATE_INTERVAL)
    {
        // Collect zone IDs first without holding any locks
        std::vector<std::pair<uint32, uint32>> zonesToUpdate;
        {
            std::lock_guard<std::mutex> lock(_zoneMutex);
            for (auto const& [zoneId, population] : _zonePopulations)
            {
                zonesToUpdate.emplace_back(zoneId, population.mapId);
            }
        } // _zoneMutex released here

        // Update each zone population without holding _zoneMutex
        for (auto const& [zoneId, mapId] : zonesToUpdate)
        {
            UpdateZonePopulationSafe(zoneId, mapId);
        }

        _lastPopulationUpdate = currentTime;
    }

    // Recalculate targets and spawn periodically
    if (currentTime - _lastTargetCalculation > TARGET_CALCULATION_INTERVAL)
    {
        printf("=== PLAYERBOT DEBUG: Target calculation interval reached, enableDynamicSpawning: %s ===\n",
               _config.enableDynamicSpawning ? "true" : "false");
        fflush(stdout);
        // DISABLED: TC_LOG_INFO causes crash
        // TC_LOG_INFO("module.playerbot.spawner", "Target calculation interval reached, enableDynamicSpawning: {}",
        //            _config.enableDynamicSpawning ? "true" : "false");
        if (_config.enableDynamicSpawning)
        {
            printf("=== PLAYERBOT DEBUG: Calling CalculateZoneTargets and SpawnToPopulationTarget ===\n");
            fflush(stdout);
            // DISABLED: TC_LOG_INFO causes crash
            // TC_LOG_INFO("module.playerbot.spawner", "Calling CalculateZoneTargets and SpawnToPopulationTarget");
            CalculateZoneTargets();
            SpawnToPopulationTarget();
        }
        _lastTargetCalculation = currentTime;
    }
}

void BotSpawner::LoadConfig()
{
    _config.maxBotsTotal = sPlayerbotConfig->GetInt("Playerbot.Spawn.MaxTotal", 500);
    _config.maxBotsPerZone = sPlayerbotConfig->GetInt("Playerbot.Spawn.MaxPerZone", 50);
    _config.maxBotsPerMap = sPlayerbotConfig->GetInt("Playerbot.Spawn.MaxPerMap", 200);
    _config.spawnBatchSize = sPlayerbotConfig->GetInt("Playerbot.Spawn.BatchSize", 10);
    _config.spawnDelayMs = sPlayerbotConfig->GetInt("Playerbot.Spawn.DelayMs", 100);
    _config.enableDynamicSpawning = sPlayerbotConfig->GetBool("Playerbot.Spawn.Dynamic", true);
    _config.respectPopulationCaps = sPlayerbotConfig->GetBool("Playerbot.Spawn.RespectCaps", true);
    _config.botToPlayerRatio = sPlayerbotConfig->GetFloat("Playerbot.Spawn.BotToPlayerRatio", 2.0f);

    TC_LOG_DEBUG("module.playerbot.spawner", "Loaded spawn configuration");
}

bool BotSpawner::SpawnBot(SpawnRequest const& request)
{
    if (!ValidateSpawnRequest(request))
    {
        return false;
    }

    return SpawnBotInternal(request);
}

uint32 BotSpawner::SpawnBots(std::vector<SpawnRequest> const& requests)
{
    uint32 successCount = 0;

    // Collect valid requests first
    std::vector<SpawnRequest> validRequests;
    for (SpawnRequest const& request : requests)
    {
        if (ValidateSpawnRequest(request))
        {
            validRequests.push_back(request);
            ++successCount;
        }
    }

    // Add all valid requests to queue in one lock
    if (!validRequests.empty())
    {
        std::lock_guard<std::mutex> lock(_spawnQueueMutex);
        for (SpawnRequest const& request : validRequests)
        {
            _spawnQueue.push(request);
        }
    }

    TC_LOG_DEBUG("module.playerbot.spawner",
        "Queued {} spawn requests ({} total requested)", successCount, requests.size());

    return successCount;
}

bool BotSpawner::SpawnBotInternal(SpawnRequest const& request)
{
    printf("=== PLAYERBOT DEBUG: SpawnBotInternal ENTERED for zone %u, account %u ===\n", request.zoneId, request.accountId);
    fflush(stdout);

    auto startTime = std::chrono::high_resolution_clock::now();
    _stats.spawnAttempts.fetch_add(1);

    // Select character for spawning
    ObjectGuid characterGuid = request.characterGuid;
    if (characterGuid.IsEmpty())
    {
        printf("=== PLAYERBOT DEBUG: CharacterGuid is empty, calling SelectCharacterForSpawn ===\n");
        fflush(stdout);
        characterGuid = SelectCharacterForSpawn(request);
        if (characterGuid.IsEmpty())
        {
            printf("=== PLAYERBOT DEBUG: SelectCharacterForSpawn FAILED - returning false ===\n");
            fflush(stdout);
            // TC_LOG_WARN("module.playerbot.spawner",
            //     "No suitable character found for spawn request (type: {})", static_cast<int>(request.type));
            _stats.failedSpawns.fetch_add(1);
            if (request.callback)
                request.callback(false, ObjectGuid::Empty);
            return false;
        }
        printf("=== PLAYERBOT DEBUG: SelectCharacterForSpawn SUCCESS - got character %s ===\n", characterGuid.ToString().c_str());
        fflush(stdout);
    }

    // Create bot session
    if (!CreateBotSession(request.accountId, characterGuid))
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Failed to create bot session for character {}", characterGuid.ToString());
        _stats.failedSpawns.fetch_add(1);
        if (request.callback)
            request.callback(false, ObjectGuid::Empty);
        return false;
    }

    // Update tracking data
    uint32 zoneId = request.zoneId;
    if (zoneId == 0)
    {
        // Get zone from character data - simplified for now
        zoneId = 1; // Default to first zone
    }

    {
        std::lock_guard<std::mutex> lock(_botMutex);
        _activeBots[characterGuid] = zoneId;
        _botsByZone[zoneId].push_back(characterGuid);
    }

    // Update statistics
    _stats.totalSpawned.fetch_add(1);
    uint32 currentActive = _stats.currentlyActive.fetch_add(1) + 1;
    uint32 currentPeak = _stats.peakConcurrent.load();
    while (currentActive > currentPeak && !_stats.peakConcurrent.compare_exchange_weak(currentPeak, currentActive))
    {
        // Retry until we successfully update the peak or find a higher value
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    _stats.totalSpawnTime.fetch_add(duration.count());

    TC_LOG_INFO("module.playerbot.spawner",
        "Successfully spawned bot {} in zone {} (spawn time: {:.2f}ms)",
        characterGuid.ToString(), zoneId, duration.count() / 1000.0f);

    if (request.callback)
        request.callback(true, characterGuid);

    return true;
}

bool BotSpawner::CreateBotSession(uint32 accountId, ObjectGuid characterGuid)
{
    // Use the BotSessionMgr to create a new bot session
    BotSession* session = sBotSessionMgr->CreateSession(accountId);
    if (!session)
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Failed to create bot session for account {}", accountId);
        return false;
    }

    // The session manager handles the rest of the session setup
    return true;
}

bool BotSpawner::ValidateSpawnRequest(SpawnRequest const& request) const
{
    // Check global population caps
    if (_config.respectPopulationCaps && !CanSpawnMore())
    {
        return false;
    }

    // Check zone-specific caps
    if (request.zoneId != 0 && _config.respectPopulationCaps && !CanSpawnInZone(request.zoneId))
    {
        return false;
    }

    // Check map-specific caps
    if (request.mapId != 0 && _config.respectPopulationCaps && !CanSpawnOnMap(request.mapId))
    {
        return false;
    }

    return true;
}

ObjectGuid BotSpawner::SelectCharacterForSpawn(SpawnRequest const& request)
{
    printf("=== PLAYERBOT DEBUG: SelectCharacterForSpawn ENTERED ===\n");
    fflush(stdout);

    // Get available accounts if not specified
    std::vector<uint32> accounts;
    if (request.accountId != 0)
    {
        printf("=== PLAYERBOT DEBUG: Using specified account %u ===\n", request.accountId);
        accounts.push_back(request.accountId);
    }
    else
    {
        printf("=== PLAYERBOT DEBUG: No account specified, calling AcquireAccount ===\n");
        fflush(stdout);
        // Use AcquireAccount to get available accounts
        uint32 accountId = sBotAccountMgr->AcquireAccount();
        if (accountId != 0)
        {
            printf("=== PLAYERBOT DEBUG: AcquireAccount returned account %u ===\n", accountId);
            accounts.push_back(accountId);
        }
        else
        {
            printf("=== PLAYERBOT DEBUG: AcquireAccount returned 0 - no accounts available ===\n");
        }
        fflush(stdout);
    }

    if (accounts.empty())
    {
        printf("=== PLAYERBOT DEBUG: No accounts available - returning empty GUID ===\n");
        fflush(stdout);
        // TC_LOG_WARN("module.playerbot.spawner", "No available accounts for bot spawning");
        return ObjectGuid::Empty;
    }

    // Try each account until we find a suitable character
    for (uint32 accountId : accounts)
    {
        printf("=== PLAYERBOT DEBUG: Checking account %u for characters ===\n", accountId);
        fflush(stdout);
        std::vector<ObjectGuid> characters = GetAvailableCharacters(accountId, request);
        if (!characters.empty())
        {
            printf("=== PLAYERBOT DEBUG: Found %zu existing characters for account %u ===\n", characters.size(), accountId);
            fflush(stdout);
            // Pick a random character from available ones
            uint32 index = urand(0, characters.size() - 1);
            return characters[index];
        }
        else
        {
            printf("=== PLAYERBOT DEBUG: No existing characters for account %u, calling CreateCharacterForAccount ===\n", accountId);
            fflush(stdout);
            // No existing characters found - create a new one
            // TC_LOG_INFO("module.playerbot.spawner",
            //     "No characters found for account {}, attempting to create new character", accountId);

            ObjectGuid newCharacterGuid = CreateCharacterForAccount(accountId, request);
            if (!newCharacterGuid.IsEmpty())
            {
                printf("=== PLAYERBOT DEBUG: CreateCharacterForAccount SUCCESS - created character %s ===\n", newCharacterGuid.ToString().c_str());
                fflush(stdout);
                // TC_LOG_INFO("module.playerbot.spawner",
                //     "Successfully created new character {} for account {}",
                //     newCharacterGuid.ToString(), accountId);
                return newCharacterGuid;
            }
            else
            {
                printf("=== PLAYERBOT DEBUG: CreateCharacterForAccount FAILED for account %u ===\n", accountId);
                fflush(stdout);
                // TC_LOG_WARN("module.playerbot.spawner",
                //     "Failed to create character for account {}", accountId);
            }
        }
    }

    return ObjectGuid::Empty;
}

std::vector<ObjectGuid> BotSpawner::GetAvailableCharacters(uint32 accountId, SpawnRequest const& request)
{
    std::vector<ObjectGuid> availableCharacters;

    // Query existing characters on this account
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
    stmt->setUInt32(0, accountId);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64());

            // TODO: Add level/race/class filtering when we have a proper query
            // For now, accept all characters on the account
            availableCharacters.push_back(characterGuid);
        } while (result->NextRow());
    }

    // If no characters found and AutoCreateCharacters is enabled, create one
    if (availableCharacters.empty() && sPlayerbotConfig->GetBool("Playerbot.AutoCreateCharacters", false))
    {
        TC_LOG_DEBUG("module.playerbot.spawner",
            "No characters found for account {}, attempting to create new character", accountId);

        ObjectGuid newCharacterGuid = CreateBotCharacter(accountId);
        if (!newCharacterGuid.IsEmpty())
        {
            availableCharacters.push_back(newCharacterGuid);
            TC_LOG_INFO("module.playerbot.spawner",
                "Successfully created new bot character {} for account {}",
                newCharacterGuid.ToString(), accountId);
        }
        else
        {
            TC_LOG_WARN("module.playerbot.spawner",
                "Failed to create character for account {}", accountId);
        }
    }

    return availableCharacters;
}

void BotSpawner::DespawnBot(ObjectGuid guid, bool forced)
{
    {
        std::lock_guard<std::mutex> lock(_botMutex);
        auto it = _activeBots.find(guid);
        if (it == _activeBots.end())
        {
            TC_LOG_DEBUG("module.playerbot.spawner",
                "Attempted to despawn non-active bot {}", guid.ToString());
            return;
        }

        uint32 zoneId = it->second;
        _activeBots.erase(it);

        // Remove from zone tracking
        auto zoneIt = _botsByZone.find(zoneId);
        if (zoneIt != _botsByZone.end())
        {
            auto& bots = zoneIt->second;
            bots.erase(std::remove(bots.begin(), bots.end(), guid), bots.end());
        }
    }

    // Remove the bot session
    // Note: We need to get the account ID from the guid to release the session
    // For now, this is simplified - in a full implementation we'd track guid->accountId mapping
    // sBotSessionMgr->ReleaseSession(accountId);

    // Update statistics
    _stats.totalDespawned.fetch_add(1);
    _stats.currentlyActive.fetch_sub(1);

    TC_LOG_INFO("module.playerbot.spawner",
        "Despawned bot {} (forced: {})", guid.ToString(), forced);
}

void BotSpawner::DespawnAllBots()
{
    std::vector<ObjectGuid> botsToRemove;
    {
        std::lock_guard<std::mutex> lock(_botMutex);
        for (auto const& [guid, zoneId] : _activeBots)
        {
            botsToRemove.push_back(guid);
        }
    }

    for (ObjectGuid guid : botsToRemove)
    {
        DespawnBot(guid, true);
    }

    TC_LOG_INFO("module.playerbot.spawner",
        "Despawned all {} active bots", botsToRemove.size());
}

void BotSpawner::UpdateZonePopulation(uint32 zoneId, uint32 mapId)
{
    // Count real players in this zone
    uint32 playerCount = 0;
    // TODO: Implement player counting in zone
    // This would require iterating through active sessions and checking their zone

    // Count bots in this zone (get count first, before locking _zoneMutex)
    uint32 botCount = 0;
    {
        std::lock_guard<std::mutex> botLock(_botMutex);
        auto it = _botsByZone.find(zoneId);
        botCount = it != _botsByZone.end() ? it->second.size() : 0;
    } // _botMutex released here

    // Now update zone population with _zoneMutex
    {
        std::lock_guard<std::mutex> zoneLock(_zoneMutex);
        auto it = _zonePopulations.find(zoneId);
        if (it != _zonePopulations.end())
        {
            it->second.playerCount = playerCount;
            it->second.botCount = botCount;
            it->second.lastUpdate = std::chrono::system_clock::now();
        }
    }
}

void BotSpawner::UpdateZonePopulationSafe(uint32 zoneId, uint32 mapId)
{
    // Count real players in this zone
    uint32 playerCount = 0;
    // TODO: Implement player counting in zone
    // This would require iterating through active sessions and checking their zone

    // Count bots in this zone safely (separate lock scope)
    uint32 botCount = 0;
    {
        std::lock_guard<std::mutex> botLock(_botMutex);
        auto it = _botsByZone.find(zoneId);
        botCount = it != _botsByZone.end() ? it->second.size() : 0;
    } // _botMutex released here

    // Update zone population (separate lock scope)
    {
        std::lock_guard<std::mutex> zoneLock(_zoneMutex);
        auto it = _zonePopulations.find(zoneId);
        if (it != _zonePopulations.end())
        {
            it->second.playerCount = playerCount;
            it->second.botCount = botCount;
            it->second.lastUpdate = std::chrono::system_clock::now();
        }
    } // _zoneMutex released here
}

ZonePopulation BotSpawner::GetZonePopulation(uint32 zoneId) const
{
    std::lock_guard<std::mutex> lock(_zoneMutex);
    auto it = _zonePopulations.find(zoneId);
    if (it != _zonePopulations.end())
    {
        return it->second;
    }
    return ZonePopulation{};
}

uint32 BotSpawner::GetActiveBotCount() const
{
    std::lock_guard<std::mutex> lock(_botMutex);
    return _activeBots.size();
}

uint32 BotSpawner::GetActiveBotCount(uint32 zoneId) const
{
    std::lock_guard<std::mutex> lock(_botMutex);
    auto it = _botsByZone.find(zoneId);
    return it != _botsByZone.end() ? it->second.size() : 0;
}

bool BotSpawner::CanSpawnMore() const
{
    return GetActiveBotCount() < _config.maxBotsTotal;
}

bool BotSpawner::CanSpawnInZone(uint32 zoneId) const
{
    return GetActiveBotCount(zoneId) < _config.maxBotsPerZone;
}

bool BotSpawner::CanSpawnOnMap(uint32 mapId) const
{
    uint32 mapBotCount = 0;
    std::lock_guard<std::mutex> lock(_zoneMutex);
    for (auto const& [zoneId, population] : _zonePopulations)
    {
        if (population.mapId == mapId)
        {
            mapBotCount += population.botCount;
        }
    }
    return mapBotCount < _config.maxBotsPerMap;
}

void BotSpawner::CalculateZoneTargets()
{
    std::lock_guard<std::mutex> lock(_zoneMutex);
    for (auto& [zoneId, population] : _zonePopulations)
    {
        population.targetBotCount = CalculateTargetBotCount(population);
    }

    TC_LOG_DEBUG("module.playerbot.spawner", "Recalculated zone population targets");
}

uint32 BotSpawner::CalculateTargetBotCount(ZonePopulation const& zone) const
{
    // Base target on player count and ratio
    uint32 baseTarget = static_cast<uint32>(zone.playerCount * _config.botToPlayerRatio);

    // Ensure minimum bots even with no players online (when AutoCreateCharacters is enabled)
    if (sPlayerbotConfig->GetBool("Playerbot.AutoCreateCharacters", false))
    {
        uint32 minimumBots = sPlayerbotConfig->GetInt("Playerbot.MinimumBotsPerZone", 3);
        baseTarget = std::max(baseTarget, minimumBots);
        printf("=== PLAYERBOT DEBUG: Zone %u - players: %u, ratio target: %u, minimum: %u, final target: %u ===\n",
               zone.zoneId, zone.playerCount, static_cast<uint32>(zone.playerCount * _config.botToPlayerRatio), minimumBots, baseTarget);
        fflush(stdout);
    }

    // Apply zone caps
    baseTarget = std::min(baseTarget, _config.maxBotsPerZone);

    return baseTarget;
}

void BotSpawner::SpawnToPopulationTarget()
{
    printf("=== PLAYERBOT DEBUG: SpawnToPopulationTarget called, enableDynamicSpawning: %s ===\n",
           _config.enableDynamicSpawning ? "true" : "false");
    fflush(stdout);
    // DISABLED: TC_LOG_INFO causes crash
    // TC_LOG_INFO("module.playerbot.spawner", "SpawnToPopulationTarget called, enableDynamicSpawning: {}",
    //            _config.enableDynamicSpawning ? "true" : "false");

    if (!_config.enableDynamicSpawning)
    {
        printf("=== PLAYERBOT DEBUG: Dynamic spawning disabled, skipping population target ===\n");
        fflush(stdout);
        // DISABLED: TC_LOG_WARN causes crash
        // TC_LOG_WARN("module.playerbot.spawner", "Dynamic spawning disabled, skipping population target");
        return;
    }

    std::vector<SpawnRequest> spawnRequests;

    printf("=== PLAYERBOT DEBUG: Checking zone populations, total zones: %zu ===\n", _zonePopulations.size());
    fflush(stdout);
    // DISABLED: TC_LOG_INFO causes crash
    // TC_LOG_INFO("module.playerbot.spawner", "Checking zone populations, total zones: {}", _zonePopulations.size());

    {
        std::lock_guard<std::mutex> lock(_zoneMutex);
        for (auto const& [zoneId, population] : _zonePopulations)
        {
            printf("=== PLAYERBOT DEBUG: Zone %u - botCount: %u, targetBotCount: %u ===\n",
                   zoneId, population.botCount, population.targetBotCount);
            fflush(stdout);

            if (population.botCount < population.targetBotCount)
            {
                uint32 needed = population.targetBotCount - population.botCount;
                printf("=== PLAYERBOT DEBUG: Zone %u needs %u bots, creating spawn requests ===\n",
                       zoneId, needed);
                fflush(stdout);

                for (uint32 i = 0; i < needed && spawnRequests.size() < _config.spawnBatchSize; ++i)
                {
                    SpawnRequest request;
                    request.type = SpawnRequest::SPECIFIC_ZONE;
                    request.zoneId = zoneId;
                    request.mapId = population.mapId;
                    request.minLevel = population.minLevel;
                    request.maxLevel = population.maxLevel;
                    spawnRequests.push_back(request);
                }
            }
        }
    }

    printf("=== PLAYERBOT DEBUG: Finished creating spawn requests, vector size: %zu ===\n", spawnRequests.size());
    fflush(stdout);

    if (!spawnRequests.empty())
    {
        printf("=== PLAYERBOT DEBUG: Calling SpawnBots() with %zu requests ===\n", spawnRequests.size());
        fflush(stdout);
        uint32 queued = SpawnBots(spawnRequests);
        printf("=== PLAYERBOT DEBUG: SpawnBots() returned queued count: %u ===\n", queued);
        fflush(stdout);
        // DISABLED: TC_LOG_DEBUG causes issues
        // TC_LOG_DEBUG("module.playerbot.spawner",
        //     "Queued {} bots for population balancing", queued);
    }
    else
    {
        printf("=== PLAYERBOT DEBUG: spawnRequests vector is empty, not calling SpawnBots() ===\n");
        fflush(stdout);
    }
}

void BotSpawner::UpdatePopulationTargets()
{
    // Initialize zone populations for all known zones
    // This is a simplified version - in reality we'd query the database for all zones
    std::lock_guard<std::mutex> lock(_zoneMutex);

    // Add some default zones if empty
    if (_zonePopulations.empty())
    {
        // These would be loaded from database or configuration
        _zonePopulations[1] = {1, 0, 0, 0, 10, 1, 10, 0.5f, std::chrono::system_clock::now()};
        _zonePopulations[2] = {2, 0, 0, 0, 15, 5, 15, 0.3f, std::chrono::system_clock::now()};
    }
}

void BotSpawner::ResetStats()
{
    _stats.totalSpawned.store(0);
    _stats.totalDespawned.store(0);
    _stats.currentlyActive.store(0);
    _stats.peakConcurrent.store(0);
    _stats.failedSpawns.store(0);
    _stats.totalSpawnTime.store(0);
    _stats.spawnAttempts.store(0);

    TC_LOG_INFO("module.playerbot.spawner", "Spawn statistics reset");
}

bool BotSpawner::DespawnBot(ObjectGuid guid, std::string const& reason)
{
    TC_LOG_DEBUG("module.playerbot.spawner", "Despawning bot {} with reason: {}", guid.ToString(), reason);

    {
        std::lock_guard<std::mutex> lock(_botMutex);
        auto it = _activeBots.find(guid);
        if (it == _activeBots.end())
        {
            TC_LOG_WARN("module.playerbot.spawner", "Attempted to despawn bot {} but it was not found in active bots", guid.ToString());
            return false;
        }

        _activeBots.erase(it);
    }

    // Call the existing forced despawn method
    DespawnBot(guid, true);

    // Log the reason for despawn
    TC_LOG_INFO("module.playerbot.spawner", "Bot {} despawned with reason: {}", guid.ToString(), reason);

    _stats.totalDespawned.fetch_add(1);
    _stats.currentlyActive.store(_activeBots.size());

    TC_LOG_INFO("module.playerbot.spawner", "Bot {} despawned successfully ({})", guid.ToString(), reason);
    return true;
}

ObjectGuid BotSpawner::CreateCharacterForAccount(uint32 accountId, SpawnRequest const& request)
{
    TC_LOG_INFO("module.playerbot.spawner",
        "Creating character for account {} based on spawn request", accountId);

    // Use the existing CreateBotCharacter method which handles all the complexity
    return CreateBotCharacter(accountId);
}

ObjectGuid BotSpawner::CreateBotCharacter(uint32 accountId)
{
    printf("=== PLAYERBOT DEBUG: CreateBotCharacter ENTERED for account %u ===\n", accountId);
    fflush(stdout);

    try
    {
        // Get race/class distribution
        printf("=== PLAYERBOT DEBUG: Getting race/class distribution ===\n");
        fflush(stdout);
        auto [race, classId] = sBotCharacterDistribution->GetRandomRaceClassByDistribution();
        if (race == RACE_NONE || classId == CLASS_NONE)
        {
            printf("=== PLAYERBOT DEBUG: Failed to get valid race/class for bot character creation ===\n");
            fflush(stdout);
            return ObjectGuid::Empty;
        }
        printf("=== PLAYERBOT DEBUG: Got race=%u, class=%u ===\n", race, classId);
        fflush(stdout);

        // Get gender (simplified - random between male/female)
        uint8 gender = urand(0, 1) ? GENDER_MALE : GENDER_FEMALE;

        // Generate character GUID first
        ObjectGuid::LowType guidLow = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
        ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(guidLow);

        // Get a unique name with the proper GUID
        printf("=== PLAYERBOT DEBUG: Allocating name for gender=%u, guid=%llu ===\n", gender, characterGuid.GetCounter());
        fflush(stdout);
        std::string name = sBotNameMgr->AllocateName(gender, characterGuid.GetCounter());
        if (name.empty())
        {
            printf("=== PLAYERBOT DEBUG: Failed to allocate name for bot character creation ===\n");
            fflush(stdout);
            return ObjectGuid::Empty;
        }
        printf("=== PLAYERBOT DEBUG: Allocated name: %s ===\n", name.c_str());
        fflush(stdout);

        // Create character info structure
        auto createInfo = std::make_shared<WorldPackets::Character::CharacterCreateInfo>();
        createInfo->Name = name;
        createInfo->Race = race;
        createInfo->Class = classId;
        createInfo->Sex = gender;

        // Set default customizations (simplified - using basic appearance)
        // These would normally be randomized based on the race
        createInfo->Customizations.clear();

        // Get the starting level from config
        uint8 startLevel = sPlayerbotConfig->GetInt("Playerbot.RandomBotLevel.Min", 1);

        // Check if this race/class combination is valid
        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(classId);
        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);

        if (!classEntry || !raceEntry)
        {
            printf("=== PLAYERBOT DEBUG: Invalid race (%u) or class (%u) for bot character creation ===\n", race, classId);
            fflush(stdout);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }
        printf("=== PLAYERBOT DEBUG: Race/class validation succeeded ===\n");
        fflush(stdout);

        // Create a bot session for character creation - Player needs valid session for account association
        printf("=== PLAYERBOT DEBUG: About to create bot session for account %u ===\n", accountId);
        fflush(stdout);
        BotSession* botSession = sBotSessionMgr->CreateSession(accountId);
        if (!botSession)
        {
            printf("=== PLAYERBOT DEBUG: Failed to create bot session for character creation (Account: %u) ===\n", accountId);
            fflush(stdout);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }
        printf("=== PLAYERBOT DEBUG: Bot session created successfully ===\n");
        fflush(stdout);

        printf("=== PLAYERBOT DEBUG: About to create Player object ===\n");
        fflush(stdout);
        std::shared_ptr<Player> newChar(new Player(botSession), [](Player* ptr)
        {
            if (ptr)
            {
                ptr->CleanupsBeforeDelete();
                delete ptr;
            }
        });

        printf("=== PLAYERBOT DEBUG: About to initialize MotionMaster ===\n");
        fflush(stdout);
        newChar->GetMotionMaster()->Initialize();

        printf("=== PLAYERBOT DEBUG: About to call Player::Create() with guidLow=%u ===\n", guidLow);
        fflush(stdout);
        if (!newChar->Create(guidLow, createInfo.get()))
        {
            printf("=== PLAYERBOT DEBUG: Failed to create Player object for bot character (Race: %u, Class: %u) ===\n", race, classId);
            fflush(stdout);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }
        printf("=== PLAYERBOT DEBUG: Player::Create() succeeded ===\n");
        fflush(stdout);

        // Account ID is automatically set through the bot session - no manual setting needed

        // Set starting level if different from 1
        printf("=== PLAYERBOT DEBUG: Setting character properties (level=%u) ===\n", startLevel);
        fflush(stdout);
        if (startLevel > 1)
        {
            newChar->SetLevel(startLevel);
        }

        newChar->setCinematic(1); // Skip intro cinematics for bots
        newChar->SetAtLoginFlag(AT_LOGIN_FIRST);

        // Save to database
        printf("=== PLAYERBOT DEBUG: About to save character to database ===\n");
        fflush(stdout);
        CharacterDatabaseTransaction characterTransaction = CharacterDatabase.BeginTransaction();
        LoginDatabaseTransaction loginTransaction = LoginDatabase.BeginTransaction();

        printf("=== PLAYERBOT DEBUG: About to call SaveToDB() ===\n");
        fflush(stdout);
        newChar->SaveToDB(loginTransaction, characterTransaction, true);
        printf("=== PLAYERBOT DEBUG: SaveToDB() completed ===\n");
        fflush(stdout);

        // Update character count for account
        printf("=== PLAYERBOT DEBUG: About to update character count for account %u ===\n", accountId);
        fflush(stdout);
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_REALM_CHARACTERS);
        stmt->setUInt32(0, 1); // Increment by 1
        stmt->setUInt32(1, accountId);
        stmt->setUInt32(2, sRealmList->GetCurrentRealmId().Realm);
        loginTransaction->Append(stmt);

        // Commit transactions
        printf("=== PLAYERBOT DEBUG: About to commit database transactions ===\n");
        fflush(stdout);
        CharacterDatabase.CommitTransaction(characterTransaction);
        LoginDatabase.CommitTransaction(loginTransaction);
        printf("=== PLAYERBOT DEBUG: Database transactions committed successfully ===\n");
        fflush(stdout);

        printf("=== PLAYERBOT DEBUG: Successfully created bot character: %s (%s) - Race: %u, Class: %u, Level: %u for account %u ===\n",
            name.c_str(), characterGuid.ToString().c_str(), uint32(race), uint32(classId), uint32(startLevel), accountId);
        fflush(stdout);

        return characterGuid;
    }
    catch (std::exception const& e)
    {
        printf("=== PLAYERBOT DEBUG: Exception during bot character creation for account %u: %s ===\n", accountId, e.what());
        fflush(stdout);
        return ObjectGuid::Empty;
    }
}

} // namespace Playerbot