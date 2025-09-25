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
#include "BotWorldSessionMgr.h"
#include "BotResourcePool.h"
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
#include "CharacterDatabase.h"
#include "Database/PlayerbotCharacterDBInterface.h"
#include "ObjectGuid.h"
#include "MotionMaster.h"
#include "RealmList.h"
#include "DB2Stores.h"
#include "WorldSession.h"
#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace Playerbot
{

BotSpawner::BotSpawner()
{
    // CRITICAL: No logging in constructor - this runs during static initialization
    // before TrinityCore logging system is ready
}

BotSpawner* BotSpawner::instance()
{
    // CRITICAL: No logging in instance() - this might be called during static initialization
    // before TrinityCore logging system is ready
    static BotSpawner instance;
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

    // Initialize the flag for first player login detection
    _firstPlayerSpawned.store(false);

    // DEFERRED: Don't spawn bots during initialization - wait for first Update() call
    // This prevents crashes when the world isn't fully initialized yet
    TC_LOG_INFO("module.playerbot", "BotSpawner: Step 3 - Check enableDynamicSpawning: {}", _config.enableDynamicSpawning ? "true" : "false");
    if (_config.enableDynamicSpawning)
    {
        TC_LOG_INFO("module.playerbot", "Dynamic spawning enabled - bots will spawn when first player logs in");
        TC_LOG_INFO("module.playerbot", "BotSpawner: Step 4 - CalculateZoneTargets()...");
        CalculateZoneTargets();
        // NOTE: SpawnToPopulationTarget() will be called when first player is detected
        TC_LOG_INFO("module.playerbot", "BotSpawner: Waiting for first player login to trigger spawning");
    }
    else
    {
        TC_LOG_INFO("module.playerbot", "Static spawning enabled - bots will spawn immediately after world initialization");
        TC_LOG_INFO("module.playerbot", "BotSpawner: Step 4 - CalculateZoneTargets()...");
        CalculateZoneTargets();
        TC_LOG_INFO("module.playerbot", "BotSpawner: Static spawning mode - immediate spawning will occur in Update()");
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

void BotSpawner::Update(uint32 /*diff*/)
{
    if (!_enabled.load())
        return;

    // CRITICAL SAFETY: Wrap update in try-catch to prevent crashes
    try
    {

    static uint32 updateCounter = 0;
    ++updateCounter;
    uint32 currentTime = getMSTime();

    // Check for real players and trigger spawning if needed
    CheckAndSpawnForPlayers();

    // Minimal debug logging every 50k updates to prevent spam
    if (updateCounter % 50000 == 0) // Log every 50k updates for status monitoring
    {
        uint32 timeSinceLastSpawn = currentTime - _lastTargetCalculation;
        TC_LOG_DEBUG("module.playerbot.spawner",
            "BotSpawner status #{} - active bots: {}, time since last calculation: {}ms",
            updateCounter, GetActiveBotCount(), timeSinceLastSpawn);
    }

    // Process spawn queue (mutex-protected)
    bool queueHasItems = false;
    {
        std::lock_guard<std::mutex> lock(_spawnQueueMutex);
        queueHasItems = !_spawnQueue.empty();
    }

    if (!_processingQueue.load() && queueHasItems)
    {
        _processingQueue.store(true);

        // Extract batch of requests to minimize lock time
        std::vector<SpawnRequest> requestBatch;
        {
            std::lock_guard<std::mutex> lock(_spawnQueueMutex);
            uint32 batchSize = std::min(_config.spawnBatchSize, static_cast<uint32>(_spawnQueue.size()));
            requestBatch.reserve(batchSize);

            for (uint32 i = 0; i < batchSize && !_spawnQueue.empty(); ++i)
            {
                requestBatch.push_back(_spawnQueue.front());
                _spawnQueue.pop();
            }
        }

        TC_LOG_TRACE("module.playerbot.spawner", "Processing {} spawn requests", requestBatch.size());

        // Process requests outside the lock
        for (SpawnRequest const& request : requestBatch)
        {
            SpawnBotInternal(request);
        }

        _processingQueue.store(false);
    }

    // Update zone populations periodically - THREAD SAFE VERSION
    if (currentTime - _lastPopulationUpdate > POPULATION_UPDATE_INTERVAL)
    {
        // Collect zone IDs first without holding any locks to prevent deadlocks
        std::vector<std::pair<uint32, uint32>> zonesToUpdate;
        {
            std::unique_lock<std::mutex> zoneLock(_zoneMutex, std::try_to_lock);
            if (zoneLock.owns_lock())
            {
                zonesToUpdate.reserve(_zonePopulations.size());
                for (auto const& [zoneId, population] : _zonePopulations)
                {
                    zonesToUpdate.emplace_back(zoneId, population.mapId);
                }
            }
            // If we can't get the lock, skip this update cycle to prevent deadlocks
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
        if (_config.enableDynamicSpawning)
        {
            TC_LOG_INFO("module.playerbot.spawner", "*** DYNAMIC SPAWNING CYCLE: Recalculating zone targets and spawning to population targets");
            CalculateZoneTargets();
            SpawnToPopulationTarget();
            TC_LOG_INFO("module.playerbot.spawner", "*** DYNAMIC SPAWNING CYCLE: Completed spawn cycle");
        }
        else
        {
            TC_LOG_INFO("module.playerbot.spawner", "*** STATIC SPAWNING CYCLE: Recalculating zone targets and spawning to population targets");
            CalculateZoneTargets();
            SpawnToPopulationTarget();
            TC_LOG_INFO("module.playerbot.spawner", "*** STATIC SPAWNING CYCLE: Completed spawn cycle");
        }
        _lastTargetCalculation = currentTime;
    }
    else if (updateCounter % 10000 == 0)
    {
        uint32 timeLeft = TARGET_CALCULATION_INTERVAL - (currentTime - _lastTargetCalculation);
        TC_LOG_INFO("module.playerbot.spawner", "*** SPAWNING CYCLE: {} ms until next spawn cycle", timeLeft);
    }

    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("module.playerbot.spawner", "CRITICAL EXCEPTION in BotSpawner::Update: {}", ex.what());
        TC_LOG_ERROR("module.playerbot.spawner", "Disabling spawner to prevent further crashes");
        _enabled.store(false);
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.spawner", "CRITICAL UNKNOWN EXCEPTION in BotSpawner::Update");
        TC_LOG_ERROR("module.playerbot.spawner", "Disabling spawner to prevent further crashes");
        _enabled.store(false);
    }
}

CharacterDatabasePreparedStatement* BotSpawner::GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName) const
{
    // CRITICAL FIX: Add comprehensive statement index validation to prevent assertion failure m_mStmt
    if (statementId >= MAX_CHARACTERDATABASE_STATEMENTS) {
        TC_LOG_ERROR("module.playerbot.spawner", "BotSpawner::GetSafePreparedStatement: Invalid statement index {} >= {} for {}",
                     static_cast<uint32>(statementId), MAX_CHARACTERDATABASE_STATEMENTS, statementName);
        return nullptr;
    }

    // CRITICAL FIX: All statements should be properly prepared by Trinity's DoPrepareStatements()
    TC_LOG_DEBUG("module.playerbot.spawner",
        "Accessing statement {} ({}) - ensuring Trinity connection preparation worked",
        static_cast<uint32>(statementId), statementName);

    // Use PlayerbotCharacterDBInterface for safe statement access with sync/async routing
    CharacterDatabasePreparedStatement* stmt = sPlayerbotCharDB->GetPreparedStatement(statementId);
    if (!stmt) {
        TC_LOG_ERROR("module.playerbot.spawner", "BotSpawner::GetSafePreparedStatement: Failed to get prepared statement {} (index: {})",
                     statementName, static_cast<uint32>(statementId));
        return nullptr;
    }
    return stmt;
}

LoginDatabasePreparedStatement* BotSpawner::GetSafeLoginPreparedStatement(LoginDatabaseStatements statementId, const char* statementName) const
{
    // CRITICAL FIX: Add comprehensive statement index validation to prevent assertion failure m_mStmt for LoginDatabase
    if (statementId >= MAX_LOGINDATABASE_STATEMENTS) {
        TC_LOG_ERROR("module.playerbot.spawner", "BotSpawner::GetSafeLoginPreparedStatement: Invalid statement index {} >= {} for {}",
                     static_cast<uint32>(statementId), MAX_LOGINDATABASE_STATEMENTS, statementName);
        return nullptr;
    }

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(statementId);
    if (!stmt) {
        TC_LOG_ERROR("module.playerbot.spawner", "BotSpawner::GetSafeLoginPreparedStatement: Failed to get prepared statement {} (index: {})",
                     statementName, static_cast<uint32>(statementId));
        return nullptr;
    }
    return stmt;
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
    TC_LOG_TRACE("module.playerbot.spawner", "SpawnBotInternal called for zone {}, account {}", request.zoneId, request.accountId);

    auto startTime = std::chrono::high_resolution_clock::now();
    _stats.spawnAttempts.fetch_add(1);

    // Select character for spawning - ASYNC PATTERN for 5000 bot scalability
    ObjectGuid characterGuid = request.characterGuid;
    if (characterGuid.IsEmpty())
    {
        // ASYNC CHARACTER SELECTION - no blocking for 5000 bots
        SelectCharacterForSpawnAsync(request, [this, request, startTime](ObjectGuid selectedGuid) {
            if (selectedGuid.IsEmpty())
            {
                TC_LOG_WARN("module.playerbot.spawner",
                    "No suitable character found for spawn request (type: {})", static_cast<int>(request.type));
                _stats.failedSpawns.fetch_add(1);
                if (request.callback)
                    request.callback(false, ObjectGuid::Empty);
                return;
            }

            // Continue with spawn process asynchronously
            ContinueSpawnWithCharacter(selectedGuid, request);
        });
        return true; // Return immediately - async operation continues in callback
    }
    else
    {
        // Character already specified - continue directly
        ContinueSpawnWithCharacter(characterGuid, request);
        return true;
    }
}

bool BotSpawner::CreateBotSession(uint32 accountId, ObjectGuid characterGuid)
{
    TC_LOG_INFO("module.playerbot.spawner", "🎮 Creating bot session for account {}, character {}", accountId, characterGuid.ToString());

    // DISABLED: Legacy BotSessionMgr creates invalid account IDs
    // Use the BotSessionMgr to create a new bot session with ASYNC character login (legacy approach)
    // BotSession* session = sBotSessionMgr->CreateAsyncSession(accountId, characterGuid);
    // if (!session)
    // {
    //     TC_LOG_ERROR("module.playerbot.spawner",
    //         "🎮 Failed to create async bot session for account {}", accountId);
    //     return false;
    // }

    // PRIMARY: Use the fixed native TrinityCore login approach with proper account IDs
    if (!Playerbot::sBotWorldSessionMgr->AddPlayerBot(characterGuid, accountId))
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "🎮 Failed to create native WorldSession for character {}", characterGuid.ToString());
        return false; // Fail if the primary system fails
    }

    TC_LOG_INFO("module.playerbot.spawner",
        "🎮 Successfully created bot session and started async login for character {} for account {}",
        characterGuid.ToString(), accountId);

    return true;
}

bool BotSpawner::ValidateSpawnRequest(SpawnRequest const& request) const
{
    // Comprehensive validation for 5000 bot scalability

    // Check if spawning is enabled
    if (!_enabled.load())
    {
        TC_LOG_DEBUG("module.playerbot.spawner", "Spawn request rejected: spawning disabled");
        return false;
    }

    // Validate GUID ranges for security
    if (!request.characterGuid.IsEmpty() && request.characterGuid.GetTypeName() != "Player")
    {
        TC_LOG_WARN("module.playerbot.spawner", "Invalid character GUID type: {}", request.characterGuid.GetTypeName());
        return false;
    }

    // Validate account ownership if specified
    if (request.accountId != 0 && !request.characterGuid.IsEmpty())
    {
        uint32 actualAccountId = GetAccountIdFromCharacter(request.characterGuid);
        if (actualAccountId != 0 && actualAccountId != request.accountId)
        {
            TC_LOG_WARN("module.playerbot.spawner",
                "Account ownership mismatch: character {} belongs to account {}, not {}",
                request.characterGuid.ToString(), actualAccountId, request.accountId);
            return false;
        }
    }

    // Validate level ranges
    if (request.minLevel > request.maxLevel && request.maxLevel != 0)
    {
        TC_LOG_WARN("module.playerbot.spawner", "Invalid level range: {} > {}", request.minLevel, request.maxLevel);
        return false;
    }

    // Check global population caps
    if (_config.respectPopulationCaps && !CanSpawnMore())
    {
        TC_LOG_DEBUG("module.playerbot.spawner", "Spawn request rejected: global bot limit reached");
        return false;
    }

    // Check zone-specific caps
    if (request.zoneId != 0 && _config.respectPopulationCaps && !CanSpawnInZone(request.zoneId))
    {
        TC_LOG_DEBUG("module.playerbot.spawner", "Spawn request rejected: zone {} bot limit reached", request.zoneId);
        return false;
    }

    // Check map-specific caps
    if (request.mapId != 0 && _config.respectPopulationCaps && !CanSpawnOnMap(request.mapId))
    {
        TC_LOG_DEBUG("module.playerbot.spawner", "Spawn request rejected: map {} bot limit reached", request.mapId);
        return false;
    }

    return true;
}

ObjectGuid BotSpawner::SelectCharacterForSpawn(SpawnRequest const& request)
{
    TC_LOG_TRACE("module.playerbot.spawner", "Selecting character for spawn request");

    // Get available accounts if not specified
    std::vector<uint32> accounts;
    if (request.accountId != 0)
    {
        TC_LOG_TRACE("module.playerbot.spawner", "Using specified account {}", request.accountId);
        accounts.push_back(request.accountId);
    }
    else
    {
        // Use AcquireAccount to get available accounts
        uint32 accountId = sBotAccountMgr->AcquireAccount();
        if (accountId != 0)
        {
            TC_LOG_TRACE("module.playerbot.spawner", "Acquired account {} for bot spawning", accountId);
            accounts.push_back(accountId);
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.spawner", "No accounts available for bot spawning");
        }
    }

    if (accounts.empty())
    {
        TC_LOG_WARN("module.playerbot.spawner", "No available accounts for bot spawning");
        return ObjectGuid::Empty;
    }

    // Try each account until we find a suitable character
    for (uint32 accountId : accounts)
    {
        TC_LOG_TRACE("module.playerbot.spawner", "Checking account {} for characters", accountId);
        std::vector<ObjectGuid> characters = GetAvailableCharacters(accountId, request);
        if (!characters.empty())
        {
            TC_LOG_TRACE("module.playerbot.spawner", "Found {} existing characters for account {}", characters.size(), accountId);
            // CRITICAL FIX: Use DETERMINISTIC character selection instead of random to prevent session conflicts
            // Always pick the first character (lowest GUID) to ensure consistency
            TC_LOG_INFO("module.playerbot.spawner", "🎲 DETERMINISTIC: Selecting first character {} from {} available for account {}",
                characters[0].ToString(), characters.size(), accountId);
            return characters[0];
        }
        else
        {
            // No existing characters found - create a new one
            TC_LOG_INFO("module.playerbot.spawner",
                "No characters found for account {}, attempting to create new character", accountId);

            ObjectGuid newCharacterGuid = CreateCharacterForAccount(accountId, request);
            if (!newCharacterGuid.IsEmpty())
            {
                TC_LOG_INFO("module.playerbot.spawner",
                    "Successfully created new character {} for account {}",
                    newCharacterGuid.ToString(), accountId);
                return newCharacterGuid;
            }
            else
            {
                TC_LOG_WARN("module.playerbot.spawner",
                    "Failed to create character for account {}", accountId);
            }
        }
    }

    return ObjectGuid::Empty;
}

void BotSpawner::SelectCharacterForSpawnAsync(SpawnRequest const& request, std::function<void(ObjectGuid)> callback)
{
    TC_LOG_TRACE("module.playerbot.spawner", "Selecting character for spawn request asynchronously");

    // Get available accounts if not specified
    std::vector<uint32> accounts;
    if (request.accountId != 0)
    {
        TC_LOG_TRACE("module.playerbot.spawner", "Using specified account {}", request.accountId);
        accounts.push_back(request.accountId);
    }
    else
    {
        // Use AcquireAccount to get available accounts
        uint32 accountId = sBotAccountMgr->AcquireAccount();
        if (accountId != 0)
        {
            TC_LOG_TRACE("module.playerbot.spawner", "Acquired account {} for bot spawning", accountId);
            accounts.push_back(accountId);
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.spawner", "No accounts available for bot spawning");
        }
    }

    if (accounts.empty())
    {
        TC_LOG_WARN("module.playerbot.spawner", "No available accounts for bot spawning");
        callback(ObjectGuid::Empty);
        return;
    }

    // Start async recursive character selection for 5000 bot scalability
    SelectCharacterAsyncRecursive(std::move(accounts), 0, request, std::move(callback));
}

std::vector<ObjectGuid> BotSpawner::GetAvailableCharacters(uint32 accountId, SpawnRequest const& request)
{
    std::vector<ObjectGuid> availableCharacters;

    // ASYNC DATABASE QUERY for 5000 bot scalability - use safe statement access to prevent memory corruption
    CharacterDatabasePreparedStatement* stmt = GetSafePreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID, "CHAR_SEL_CHARS_BY_ACCOUNT_ID");
    if (!stmt) {
        return availableCharacters;
    }
    stmt->setUInt32(0, accountId);

    try
    {
        // Use PlayerbotCharacterDBInterface for safe synchronous execution
        PreparedQueryResult result = sPlayerbotCharDB->ExecuteSync(stmt);
        if (result)
        {
            availableCharacters.reserve(result->GetRowCount());
            do
            {
                Field* fields = result->Fetch();
                ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64());

                // TODO: Add level/race/class filtering when we have a proper query
                // For now, accept all characters on the account
                availableCharacters.push_back(characterGuid);
            } while (result->NextRow());
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Database error while getting characters for account {}: {}", accountId, e.what());
        return availableCharacters; // Return empty vector on error
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

void BotSpawner::GetAvailableCharactersAsync(uint32 accountId, SpawnRequest const& request, std::function<void(std::vector<ObjectGuid>)> callback)
{
    // FULLY ASYNC DATABASE QUERY for 5000 bot scalability - no blocking - use safe statement access to prevent memory corruption
    CharacterDatabasePreparedStatement* stmt = GetSafePreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID, "CHAR_SEL_CHARS_BY_ACCOUNT_ID");
    if (!stmt) {
        callback(std::vector<ObjectGuid>());
        return;
    }
    stmt->setUInt32(0, accountId);

    auto queryCallback = [this, accountId, request, callback](PreparedQueryResult result) mutable
    {
        TC_LOG_INFO("module.playerbot.spawner", "🔧 GetAvailableCharactersAsync callback for account {}, result: {}",
            accountId, result ? "has data" : "null");

        std::vector<ObjectGuid> availableCharacters;

        try
        {
            if (result)
            {
                availableCharacters.reserve(result->GetRowCount());
                TC_LOG_INFO("module.playerbot.spawner", "🔧 Found {} characters for account {}", result->GetRowCount(), accountId);
                do
                {
                    Field* fields = result->Fetch();
                    ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64());
                    availableCharacters.push_back(characterGuid);
                    TC_LOG_INFO("module.playerbot.spawner", "🔧 Character found: {}", characterGuid.ToString());
                } while (result->NextRow());
            }
            else
            {
                TC_LOG_INFO("module.playerbot.spawner", "🔧 No characters found for account {}", accountId);
            }
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.spawner",
                "Database error while processing characters for account {}: {}", accountId, e.what());
        }

        // Handle auto-character creation if enabled and no characters found
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
        }

        // Always call callback with results (empty on error)
        callback(std::move(availableCharacters));
    };

    // Use PlayerbotCharacterDBInterface for safe async execution with automatic sync/async routing
    TC_LOG_INFO("module.playerbot.spawner",
        "🔍 About to execute AsyncQuery for CHAR_SEL_CHARS_BY_ACCOUNT_ID (statement {}) on playerbot_characters database through PlayerbotCharacterDBInterface",
        static_cast<uint32>(CHAR_SEL_CHARS_BY_ACCOUNT_ID));

    sPlayerbotCharDB->ExecuteAsync(stmt, std::move(queryCallback));
}

void BotSpawner::SelectCharacterAsyncRecursive(std::vector<uint32> accounts, size_t index, SpawnRequest const& request, std::function<void(ObjectGuid)> callback)
{
    if (index >= accounts.size())
    {
        // No more accounts to check
        callback(ObjectGuid::Empty);
        return;
    }

    uint32 accountId = accounts[index];
    TC_LOG_TRACE("module.playerbot.spawner", "Async checking account {} for characters", accountId);

    GetAvailableCharactersAsync(accountId, request, [this, accounts = std::move(accounts), index, request, callback](std::vector<ObjectGuid> characters) mutable
    {
        if (!characters.empty())
        {
            TC_LOG_INFO("module.playerbot.spawner", "🎯 Found {} existing characters for account {}", characters.size(), accounts[index]);
            // Pick a random character from available ones
            uint32 charIndex = urand(0, characters.size() - 1);
            ObjectGuid selectedGuid = characters[charIndex];
            TC_LOG_INFO("module.playerbot.spawner", "🎯 Selected character {} for spawning", selectedGuid.ToString());
            callback(selectedGuid);
        }
        else
        {
            TC_LOG_INFO("module.playerbot.spawner", "🎯 No characters found for account {}, trying next account", accounts[index]);
            // Try next account
            SelectCharacterAsyncRecursive(std::move(accounts), index + 1, request, callback);
        }
    });
}

void BotSpawner::ContinueSpawnWithCharacter(ObjectGuid characterGuid, SpawnRequest const& request)
{
    TC_LOG_INFO("module.playerbot.spawner", "🚀 ContinueSpawnWithCharacter called for {}", characterGuid.ToString());

    // Get the actual account ID from the character
    uint32 actualAccountId = GetAccountIdFromCharacter(characterGuid);
    if (actualAccountId == 0)
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Failed to get account ID for character {}", characterGuid.ToString());
        _stats.failedSpawns.fetch_add(1);
        if (request.callback)
            request.callback(false, ObjectGuid::Empty);
        return;
    }

    TC_LOG_INFO("module.playerbot.spawner", "🚀 Continuing spawn with character {} for account {}", characterGuid.ToString(), actualAccountId);

    // Create bot session
    if (!CreateBotSession(actualAccountId, characterGuid))
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Failed to create bot session for character {}", characterGuid.ToString());
        _stats.failedSpawns.fetch_add(1);
        if (request.callback)
            request.callback(false, ObjectGuid::Empty);
        return;
    }

    // Update tracking data
    uint32 zoneId = request.zoneId;
    if (zoneId == 0)
    {
        zoneId = 1; // Default to first zone
    }

    {
        std::lock_guard<std::mutex> lock(_botMutex);
        _activeBots[characterGuid] = zoneId;
        _botsByZone[zoneId].push_back(characterGuid);

        // LOCK-FREE OPTIMIZATION: Update atomic counter for hot path access
        _activeBotCount.fetch_add(1, std::memory_order_release);
    }

    // Update statistics
    _stats.totalSpawned.fetch_add(1);
    uint32 currentActive = _stats.currentlyActive.fetch_add(1) + 1;
    uint32 currentPeak = _stats.peakConcurrent.load();
    while (currentActive > currentPeak && !_stats.peakConcurrent.compare_exchange_weak(currentPeak, currentActive))
    {
        // Retry until we successfully update the peak or find a higher value
    }

    TC_LOG_INFO("module.playerbot.spawner",
        "Successfully spawned bot {} in zone {} (async)", characterGuid.ToString(), zoneId);

    if (request.callback)
        request.callback(true, characterGuid);
}

uint32 BotSpawner::GetAccountIdFromCharacter(ObjectGuid characterGuid) const
{
    if (characterGuid.IsEmpty())
        return 0;

    try
    {
        // Query the account ID from the characters table using CHAR_SEL_CHAR_PINFO - use safe statement access to prevent memory corruption
        // This query returns: totaltime, level, money, account, race, class, map, zone, gender, health, playerFlags
        CharacterDatabasePreparedStatement* stmt = GetSafePreparedStatement(CHAR_SEL_CHAR_PINFO, "CHAR_SEL_CHAR_PINFO");
        if (!stmt) {
            return 0;
        }
        stmt->setUInt64(0, characterGuid.GetCounter());
        // Use PlayerbotCharacterDBInterface for safe synchronous execution
        PreparedQueryResult result = sPlayerbotCharDB->ExecuteSync(stmt);

        if (result)
        {
            Field* fields = result->Fetch();
            uint32 accountId = fields[3].GetUInt32(); // account is the 4th field (index 3)
            TC_LOG_TRACE("module.playerbot.spawner", "Character {} belongs to account {}", characterGuid.ToString(), accountId);
            return accountId;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.spawner",
            "Database error while getting account ID for character {}: {}", characterGuid.ToString(), e.what());
    }

    TC_LOG_DEBUG("module.playerbot.spawner", "Character {} not found in database", characterGuid.ToString());
    return 0;
}

void BotSpawner::DespawnBot(ObjectGuid guid, bool forced)
{
    uint32 accountId = 0;
    uint32 zoneId = 0;

    // Get bot info and remove from tracking in a single critical section
    {
        std::lock_guard<std::mutex> lock(_botMutex);
        auto it = _activeBots.find(guid);
        if (it == _activeBots.end())
        {
            TC_LOG_DEBUG("module.playerbot.spawner",
                "Attempted to despawn non-active bot {}", guid.ToString());
            return;
        }

        zoneId = it->second;
        _activeBots.erase(it);

        // LOCK-FREE OPTIMIZATION: Update atomic counter for hot path access
        _activeBotCount.fetch_sub(1, std::memory_order_release);

        // Remove from zone tracking
        auto zoneIt = _botsByZone.find(zoneId);
        if (zoneIt != _botsByZone.end())
        {
            auto& bots = zoneIt->second;
            bots.erase(std::remove(bots.begin(), bots.end(), guid), bots.end());
        }
    }

    // Get account ID for session cleanup (outside of mutex to prevent deadlocks)
    accountId = GetAccountIdFromCharacter(guid);

    // Remove the bot session properly to prevent memory leaks
    if (accountId != 0)
    {
        sBotSessionMgr->ReleaseSession(accountId);
        TC_LOG_DEBUG("module.playerbot.spawner",
            "Released bot session for account {} (character {})", accountId, guid.ToString());
    }
    else
    {
        TC_LOG_WARN("module.playerbot.spawner",
            "Could not find account ID for character {} during despawn", guid.ToString());
    }

    // Update statistics
    _stats.totalDespawned.fetch_add(1);
    _stats.currentlyActive.fetch_sub(1);

    TC_LOG_INFO("module.playerbot.spawner",
        "Despawned bot {} from zone {} (forced: {})", guid.ToString(), zoneId, forced);
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

    // CRITICAL FIX: Actually count players in the zone
    // Count real (non-bot) players
    uint32 activeSessions = sWorld->GetActiveAndQueuedSessionCount();
    uint32 botSessions = Playerbot::sBotWorldSessionMgr->GetBotCount();
    uint32 realPlayerSessions = (activeSessions > botSessions) ? (activeSessions - botSessions) : 0;

    if (realPlayerSessions > 0)
    {
        // For now, assume players are distributed across starter zones
        // This ensures bots spawn when real players are online
        playerCount = std::max(1u, realPlayerSessions);
        TC_LOG_DEBUG("module.playerbot.spawner", "Zone {} has {} real players (total sessions: {}, bot sessions: {})",
                     zoneId, playerCount, activeSessions, botSessions);
    }

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

    // CRITICAL FIX: Actually count players in the zone (same as UpdateZonePopulation)
    uint32 activeSessions = sWorld->GetActiveSessionCount();
    if (activeSessions > 0)
    {
        // Distribute players across zones for testing (can be improved later with actual zone checking)
        playerCount = std::max(1u, activeSessions); // At least 1 player per zone if anyone is online
    }

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
    // LOCK-FREE OPTIMIZATION: Use atomic counter for hot path
    // This method is called thousands of times per second with 5000 bots
    return _activeBotCount.load(std::memory_order_acquire);
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

    // CRITICAL FIX: Always ensure minimum bots per zone
    // This ensures bots spawn even with ratio = 0 or no players
    uint32 minimumBots = sPlayerbotConfig->GetInt("Playerbot.MinimumBotsPerZone", 10);

    // If we have at least 1 player online anywhere, ensure minimum bots
    if (sWorld->GetActiveSessionCount() > 0)
    {
        baseTarget = std::max(baseTarget, minimumBots);
        TC_LOG_INFO("module.playerbot.spawner", "Zone {} - players: {}, ratio: {}, ratio target: {}, minimum: {}, final target: {}",
               zone.zoneId, zone.playerCount, _config.botToPlayerRatio,
               static_cast<uint32>(zone.playerCount * _config.botToPlayerRatio), minimumBots, baseTarget);
    }

    // Apply zone caps
    baseTarget = std::min(baseTarget, _config.maxBotsPerZone);

    return baseTarget;
}

void BotSpawner::SpawnToPopulationTarget()
{
    TC_LOG_TRACE("module.playerbot.spawner", "SpawnToPopulationTarget called, enableDynamicSpawning: {}", _config.enableDynamicSpawning);

    // Allow spawning in both dynamic and static modes
    // Dynamic: triggered by player login
    // Static: triggered immediately during Update() cycles

    std::vector<SpawnRequest> spawnRequests;

    TC_LOG_TRACE("module.playerbot.spawner", "Checking zone populations, total zones: {}", _zonePopulations.size());

    {
        std::lock_guard<std::mutex> lock(_zoneMutex);

        // CRITICAL FIX: If no zones are populated, add test zones
        if (_zonePopulations.empty())
        {
            // Add some test zones with targets
            ZonePopulation testZone1;
            testZone1.zoneId = 12; // Elwynn Forest
            testZone1.mapId = 0;   // Eastern Kingdoms
            testZone1.botCount = 0;
            testZone1.targetBotCount = 5; // Target 5 bots
            testZone1.minLevel = 1;
            testZone1.maxLevel = 10;
            _zonePopulations[12] = testZone1;

            ZonePopulation testZone2;
            testZone2.zoneId = 1; // Dun Morogh
            testZone2.mapId = 0;
            testZone2.botCount = 0;
            testZone2.targetBotCount = 3; // Target 3 bots
            testZone2.minLevel = 1;
            testZone2.maxLevel = 10;
            _zonePopulations[1] = testZone2;
        }

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

    // Check if bot exists before attempting despawn
    {
        std::lock_guard<std::mutex> lock(_botMutex);
        auto it = _activeBots.find(guid);
        if (it == _activeBots.end())
        {
            TC_LOG_WARN("module.playerbot.spawner", "Attempted to despawn bot {} but it was not found in active bots", guid.ToString());
            return false;
        }
        // Don't erase here - let DespawnBot handle it to avoid race conditions
    }

    // Call the existing forced despawn method (handles all cleanup)
    DespawnBot(guid, true);

    // Log the reason for despawn
    TC_LOG_INFO("module.playerbot.spawner", "Bot {} despawned with reason: {}", guid.ToString(), reason);

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
    TC_LOG_TRACE("module.playerbot.spawner", "Creating bot character for account {}", accountId);

    try
    {
        // Get race/class distribution
        auto [race, classId] = sBotCharacterDistribution->GetRandomRaceClassByDistribution();
        if (race == RACE_NONE || classId == CLASS_NONE)
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to get valid race/class for bot character creation");
            return ObjectGuid::Empty;
        }
        TC_LOG_TRACE("module.playerbot.spawner", "Selected race {} and class {} for bot character", race, classId);

        // Get gender (simplified - random between male/female)
        uint8 gender = urand(0, 1) ? GENDER_MALE : GENDER_FEMALE;

        // Generate character GUID first
        ObjectGuid::LowType guidLow = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
        ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(guidLow);

        // Get a unique name with the proper GUID
        std::string name = sBotNameMgr->AllocateName(gender, characterGuid.GetCounter());
        if (name.empty())
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to allocate name for bot character creation");
            return ObjectGuid::Empty;
        }
        TC_LOG_TRACE("module.playerbot.spawner", "Allocated name '{}' for bot character", name);

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
            TC_LOG_ERROR("module.playerbot.spawner", "Invalid race ({}) or class ({}) for bot character creation", race, classId);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }
        TC_LOG_TRACE("module.playerbot.spawner", "Race/class validation succeeded");

        // Create a bot session for character creation - Player needs valid session for account association
        BotSession* botSession = sBotSessionMgr->CreateSession(accountId);
        if (!botSession)
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to create bot session for character creation (Account: {})", accountId);
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }
        TC_LOG_TRACE("module.playerbot.spawner", "Bot session created successfully for account {}", accountId);

        // Use smart pointer with proper RAII cleanup to prevent memory leaks
        std::unique_ptr<Player> newChar = std::make_unique<Player>(botSession);

        // REMOVED: MotionMaster initialization - this will be handled automatically during Player::Create()
        // The MotionMaster needs the Player to be fully constructed before initialization

        TC_LOG_TRACE("module.playerbot.spawner", "Creating Player object with GUID {}", guidLow);
        if (!newChar->Create(guidLow, createInfo.get()))
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to create Player object for bot character (Race: {}, Class: {})", race, classId);
            sBotNameMgr->ReleaseName(name);
            // Cleanup will be handled automatically by unique_ptr
            return ObjectGuid::Empty;
        }
        TC_LOG_TRACE("module.playerbot.spawner", "Player::Create() succeeded");

        // Account ID is automatically set through the bot session - no manual setting needed

        // Set starting level if different from 1
        TC_LOG_TRACE("module.playerbot.spawner", "Setting character properties (level: {})", startLevel);
        if (startLevel > 1)
        {
            newChar->SetLevel(startLevel);
        }

        newChar->setCinematic(1); // Skip intro cinematics for bots
        newChar->SetAtLoginFlag(AT_LOGIN_FIRST);

        // Save to database
        TC_LOG_TRACE("module.playerbot.spawner", "Saving character to database");
        // Use PlayerbotCharacterDBInterface for safe transaction handling
        CharacterDatabaseTransaction characterTransaction = sPlayerbotCharDB->BeginTransaction();
        LoginDatabaseTransaction loginTransaction = LoginDatabase.BeginTransaction();

        newChar->SaveToDB(loginTransaction, characterTransaction, true);
        TC_LOG_TRACE("module.playerbot.spawner", "SaveToDB() completed");

        // Update character count for account - with safe statement access to prevent memory corruption
        TC_LOG_TRACE("module.playerbot.spawner", "Updating character count for account {}", accountId);
        LoginDatabasePreparedStatement* stmt = GetSafeLoginPreparedStatement(LOGIN_REP_REALM_CHARACTERS, "LOGIN_REP_REALM_CHARACTERS");
        if (!stmt) {
            return ObjectGuid::Empty;
        }
        stmt->setUInt32(0, 1); // Increment by 1
        stmt->setUInt32(1, accountId);
        stmt->setUInt32(2, sRealmList->GetCurrentRealmId().Realm);
        loginTransaction->Append(stmt);

        // Commit transactions with proper error handling
        TC_LOG_TRACE("module.playerbot.spawner", "Committing database transactions");
        try
        {
            // Use PlayerbotCharacterDBInterface for safe transaction commit
            sPlayerbotCharDB->CommitTransaction(characterTransaction);
            LoginDatabase.CommitTransaction(loginTransaction);
            TC_LOG_TRACE("module.playerbot.spawner", "Database transactions committed successfully");
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.spawner", "Failed to commit transactions: {}", e.what());
            sBotNameMgr->ReleaseName(name);
            return ObjectGuid::Empty;
        }

        // Clean up the Player object properly before returning
        newChar->CleanupsBeforeDelete();
        newChar.reset(); // Explicit cleanup

        TC_LOG_INFO("module.playerbot.spawner", "Successfully created bot character: {} ({}) - Race: {}, Class: {}, Level: {} for account {}",
            name, characterGuid.ToString(), uint32(race), uint32(classId), uint32(startLevel), accountId);

        return characterGuid;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.spawner", "Exception during bot character creation for account {}: {}", accountId, e.what());
        return ObjectGuid::Empty;
    }
}

void BotSpawner::OnPlayerLogin()
{
    if (!_enabled.load() || !_config.enableDynamicSpawning)
        return;

    TC_LOG_INFO("module.playerbot.spawner", "🎮 Player logged in - triggering bot spawn check");

    // Force immediate spawn check
    CheckAndSpawnForPlayers();
}

void BotSpawner::CheckAndSpawnForPlayers()
{
    if (!_enabled.load() || !_config.enableDynamicSpawning)
        return;

    // Count real (non-bot) players
    uint32 activeSessions = sWorld->GetActiveAndQueuedSessionCount();
    uint32 botSessions = Playerbot::sBotWorldSessionMgr->GetBotCount();
    uint32 realPlayerSessions = (activeSessions > botSessions) ? (activeSessions - botSessions) : 0;

    // Check if we have real players but no bots spawned yet
    if (realPlayerSessions > 0)
    {
        uint32 currentBotCount = GetActiveBotCount();
        uint32 targetBotCount = static_cast<uint32>(realPlayerSessions * _config.botToPlayerRatio);

        // Ensure minimum bots
        uint32 minimumBots = sPlayerbotConfig->GetInt("Playerbot.MinimumBotsPerZone", 3);
        targetBotCount = std::max(targetBotCount, minimumBots);

        // Respect maximum limits
        targetBotCount = std::min(targetBotCount, _config.maxBotsTotal);

        if (currentBotCount < targetBotCount)
        {
            TC_LOG_INFO("module.playerbot.spawner",
                "🎮 Real players detected! Players: {}, Current bots: {}, Target bots: {}",
                realPlayerSessions, currentBotCount, targetBotCount);

            // Mark that we've triggered spawning for the first player
            if (!_firstPlayerSpawned.load() && realPlayerSessions > 0)
            {
                _firstPlayerSpawned.store(true);
                TC_LOG_INFO("module.playerbot.spawner", "🎮 First player detected - initiating initial bot spawn");
            }

            // Update zone populations to trigger spawning
            UpdatePopulationTargets();
            CalculateZoneTargets();

            // Spawn bots immediately
            SpawnToPopulationTarget();
        }
    }

    // Store the last known real player count
    _lastRealPlayerCount.store(realPlayerSessions);
}

} // namespace Playerbot