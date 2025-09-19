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
#include "Log.h"
#include "World.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "CharacterPackets.h"
#include "DatabaseEnv.h"
#include <algorithm>
#include <chrono>

namespace Playerbot
{

BotSpawner* BotSpawner::instance()
{
    static BotSpawner instance;
    return &instance;
}

bool BotSpawner::Initialize()
{
    TC_LOG_INFO("module.playerbot.spawner", "Initializing Bot Spawner...");

    LoadConfig();

    // Initialize zone populations
    UpdatePopulationTargets();

    TC_LOG_INFO("module.playerbot.spawner",
        "Bot Spawner initialized - Max Total: {}, Max Per Zone: {}, Max Per Map: {}",
        _config.maxBotsTotal, _config.maxBotsPerZone, _config.maxBotsPerMap);

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
    uint32 currentTime = getMSTime();

    // Process spawn queue
    if (!_processingQueue.load() && !_spawnQueue.empty())
    {
        _processingQueue.store(true);

        SpawnRequest request;
        uint32 processed = 0;
        while (_spawnQueue.try_pop(request) && processed < _config.spawnBatchSize)
        {
            SpawnBotInternal(request);
            ++processed;
        }

        _processingQueue.store(false);
    }

    // Update zone populations periodically
    if (currentTime - _lastPopulationUpdate > POPULATION_UPDATE_INTERVAL)
    {
        // Update population counts for all active zones
        std::lock_guard<std::mutex> lock(_zoneMutex);
        for (auto& [zoneId, population] : _zonePopulations)
        {
            UpdateZonePopulation(zoneId, population.mapId);
        }
        _lastPopulationUpdate = currentTime;
    }

    // Recalculate targets periodically
    if (currentTime - _lastTargetCalculation > TARGET_CALCULATION_INTERVAL)
    {
        if (_config.enableDynamicSpawning)
        {
            CalculateZoneTargets();
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

    for (SpawnRequest const& request : requests)
    {
        if (ValidateSpawnRequest(request))
        {
            _spawnQueue.push(request);
            ++successCount;
        }
    }

    TC_LOG_DEBUG("module.playerbot.spawner",
        "Queued {} spawn requests ({} total requested)", successCount, requests.size());

    return successCount;
}

bool BotSpawner::SpawnBotInternal(SpawnRequest const& request)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    _stats.spawnAttempts.fetch_add(1);

    // Select character for spawning
    ObjectGuid characterGuid = request.characterGuid;
    if (characterGuid.IsEmpty())
    {
        characterGuid = SelectCharacterForSpawn(request);
        if (characterGuid.IsEmpty())
        {
            TC_LOG_WARN("module.playerbot.spawner",
                "No suitable character found for spawn request (type: {})", static_cast<int>(request.type));
            _stats.failedSpawns.fetch_add(1);
            if (request.callback)
                request.callback(false, ObjectGuid::Empty);
            return false;
        }
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
    // Get available accounts if not specified
    std::vector<uint32> accounts;
    if (request.accountId != 0)
    {
        accounts.push_back(request.accountId);
    }
    else
    {
        // Use AcquireAccount to get available accounts
        uint32 accountId = sBotAccountMgr->AcquireAccount();
        if (accountId != 0)
            accounts.push_back(accountId);
    }

    if (accounts.empty())
    {
        TC_LOG_WARN("module.playerbot.spawner", "No available accounts for bot spawning");
        return ObjectGuid::Empty;
    }

    // Try each account until we find a suitable character
    for (uint32 accountId : accounts)
    {
        std::vector<ObjectGuid> characters = GetAvailableCharacters(accountId, request);
        if (!characters.empty())
        {
            // Pick a random character from available ones
            uint32 index = urand(0, characters.size() - 1);
            return characters[index];
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
            ObjectGuid characterGuid(HighGuid::Player, fields[0].GetUInt64());
            uint8 level = fields[1].GetUInt8();
            uint8 race = fields[2].GetUInt8();
            uint8 classId = fields[3].GetUInt8();

            // Apply filters if specified
            bool matches = true;
            if (request.minLevel > 0 && level < request.minLevel) matches = false;
            if (request.maxLevel > 0 && level > request.maxLevel) matches = false;
            if (request.raceFilter > 0 && race != request.raceFilter) matches = false;
            if (request.classFilter > 0 && classId != request.classFilter) matches = false;

            if (matches)
            {
                availableCharacters.push_back(characterGuid);
            }
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

    // Count bots in this zone
    uint32 botCount = GetActiveBotCount(zoneId);

    std::lock_guard<std::mutex> lock(_zoneMutex);
    auto it = _zonePopulations.find(zoneId);
    if (it != _zonePopulations.end())
    {
        it->second.playerCount = playerCount;
        it->second.botCount = botCount;
        it->second.lastUpdate = std::chrono::system_clock::now();
    }
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

    // Apply zone caps
    baseTarget = std::min(baseTarget, _config.maxBotsPerZone);

    return baseTarget;
}

void BotSpawner::SpawnToPopulationTarget()
{
    if (!_config.enableDynamicSpawning)
    {
        return;
    }

    std::vector<SpawnRequest> spawnRequests;

    {
        std::lock_guard<std::mutex> lock(_zoneMutex);
        for (auto const& [zoneId, population] : _zonePopulations)
        {
            if (population.botCount < population.targetBotCount)
            {
                uint32 needed = population.targetBotCount - population.botCount;
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

    if (!spawnRequests.empty())
    {
        uint32 queued = SpawnBots(spawnRequests);
        TC_LOG_DEBUG("module.playerbot.spawner",
            "Queued {} bots for population balancing", queued);
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

ObjectGuid BotSpawner::CreateBotCharacter(uint32 accountId)
{
    TC_LOG_DEBUG("module.playerbot.spawner", "Creating new bot character for account {}", accountId);

    try
    {
        // Get race/class distribution
        auto [race, classId] = sBotCharacterDistribution->GetRandomRaceClassByDistribution();
        if (race == RACE_NONE || classId == CLASS_NONE)
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to get valid race/class for bot character creation");
            return ObjectGuid::Empty;
        }

        // Get gender (simplified - random between male/female)
        uint8 gender = urand(0, 1) ? GENDER_MALE : GENDER_FEMALE;

        // Get a unique name
        std::string name = sBotNameMgr->AllocateName(gender, 0);
        if (name.empty())
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to allocate name for bot character creation");
            return ObjectGuid::Empty;
        }

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
            TC_LOG_ERROR("module.playerbot.spawner",
                "Invalid race ({}) or class ({}) for bot character creation", race, classId);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }

        // Create a temporary session for character creation
        std::shared_ptr<Player> newChar(new Player(nullptr), [](Player* ptr)
        {
            if (ptr)
            {
                ptr->CleanupsBeforeDelete();
                delete ptr;
            }
        });

        newChar->GetMotionMaster()->Initialize();

        // Generate character GUID
        ObjectGuid characterGuid = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();

        if (!newChar->Create(characterGuid, createInfo.get()))
        {
            TC_LOG_ERROR("module.playerbot.spawner",
                "Failed to create Player object for bot character (Race: {}, Class: {})", race, classId);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }

        // Set account ID
        newChar->SetAccountId(accountId);

        // Set starting level if different from 1
        if (startLevel > 1)
        {
            newChar->SetLevel(startLevel);
        }

        newChar->setCinematic(1); // Skip intro cinematics for bots
        newChar->SetAtLoginFlag(AT_LOGIN_FIRST);

        // Save to database
        CharacterDatabaseTransaction characterTransaction = CharacterDatabase.BeginTransaction();
        LoginDatabaseTransaction loginTransaction = LoginDatabase.BeginTransaction();

        newChar->SaveToDB(loginTransaction, characterTransaction, true);

        // Update character count for account
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_REALM_CHARACTERS);
        stmt->setUInt32(0, 1); // Increment by 1
        stmt->setUInt32(1, accountId);
        stmt->setUInt32(2, sRealmList->GetCurrentRealmId().Realm);
        loginTransaction->Append(stmt);

        // Commit transactions
        CharacterDatabase.CommitTransaction(characterTransaction);
        LoginDatabase.CommitTransaction(loginTransaction);

        // Register name as used
        sBotNameMgr->AllocateName(gender, characterGuid.GetCounter());

        TC_LOG_INFO("module.playerbot.spawner",
            "Successfully created bot character: {} ({}) - Race: {}, Class: {}, Level: {} for account {}",
            name, characterGuid.ToString(), uint32(race), uint32(classId), uint32(startLevel), accountId);

        return characterGuid;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Exception during bot character creation for account {}: {}", accountId, e.what());
        return ObjectGuid::Empty;
    }
}

} // namespace Playerbot