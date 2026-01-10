/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file BotCloneEngine.cpp
 * @brief Implementation of the high-performance bot clone engine
 *
 * Uses TrinityCore's character creation APIs to create bots from templates.
 */

#include "BotCloneEngine.h"
#include "BotTemplateRepository.h"
#include "Account/BotAccountMgr.h"
#include "Config/PlayerbotConfig.h"
#include "Database/PlayerbotDatabase.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "StringFormat.h"
#include <random>

namespace Playerbot
{

// ============================================================================
// CLONE RESULT
// ============================================================================

std::string CloneResult::ToString() const
{
    if (success)
    {
        return Trinity::StringFormat("CloneResult[Success: Bot={}, Name={}, Class={}, Level={}, GS={}, Time={}ms]",
            botGuid.ToString(), botName, playerClass, level, gearScore, creationTime.count());
    }
    else
    {
        return Trinity::StringFormat("CloneResult[Failed: {}]", errorMessage);
    }
}

// ============================================================================
// SINGLETON
// ============================================================================

BotCloneEngine* BotCloneEngine::Instance()
{
    static BotCloneEngine instance;
    return &instance;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool BotCloneEngine::Initialize()
{
    if (_initialized.load())
    {
        TC_LOG_WARN("playerbot.clone", "BotCloneEngine::Initialize - Already initialized");
        return true;
    }

    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::Initialize - Starting initialization");

    // Load name pool
    LoadNamePool();

    // NOTE: Account management is now delegated to BotAccountMgr
    // AllocateAccount() will use sBotAccountMgr->AcquireAccount() to get real accounts
    // that exist in the auth database, ensuring bots can actually login
    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::Initialize - Using BotAccountMgr for account allocation");

    // Initialize statistics
    _hourStart = std::chrono::system_clock::now();
    _totalClonesCreated.store(0);
    _clonesThisHour.store(0);
    _failedClonesThisHour.store(0);

    // Start worker thread for async operations
    _running.store(true);
    _workerThread = std::make_unique<std::thread>(&BotCloneEngine::AsyncWorkerThread, this);

    _initialized.store(true);
    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::Initialize - Initialization complete");

    return true;
}

void BotCloneEngine::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::Shutdown - Starting shutdown");

    // Stop worker thread
    _running.store(false);
    if (_workerThread && _workerThread->joinable())
    {
        _workerThread->join();
        _workerThread.reset();
    }

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(_asyncMutex);
        while (!_asyncQueue.empty())
            _asyncQueue.pop();
        while (!_asyncBatchQueue.empty())
            _asyncBatchQueue.pop();
    }

    // NOTE: Account management is delegated to BotAccountMgr, no cleanup needed here

    _initialized.store(false);
    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::Shutdown - Shutdown complete");
}

void BotCloneEngine::Update(uint32 /*diff*/)
{
    if (!_initialized.load())
        return;

    // Reset hourly statistics if needed
    auto now = std::chrono::system_clock::now();
    auto hoursSinceStart = std::chrono::duration_cast<std::chrono::hours>(now - _hourStart).count();
    if (hoursSinceStart >= 1)
    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        _clonesThisHour.store(0);
        _failedClonesThisHour.store(0);
        _hourStart = now;
    }
}

// ============================================================================
// SYNCHRONOUS CLONING
// ============================================================================

CloneResult BotCloneEngine::CloneFromTemplate(
    BotTemplate const* tmpl,
    uint32 targetLevel,
    Faction faction,
    uint32 targetGearScore)
{
    if (!_initialized.load())
    {
        CloneResult result;
        result.success = false;
        result.errorMessage = "BotCloneEngine not initialized";
        return result;
    }

    if (!tmpl)
    {
        CloneResult result;
        result.success = false;
        result.errorMessage = "Invalid template (null)";
        return result;
    }

    return ExecuteClone(tmpl, targetLevel, faction, targetGearScore);
}

CloneResult BotCloneEngine::Clone(
    BotRole role,
    Faction faction,
    uint32 targetLevel,
    uint32 targetGearScore)
{
    if (!_initialized.load())
    {
        CloneResult result;
        result.success = false;
        result.errorMessage = "BotCloneEngine not initialized";
        return result;
    }

    // Select template from repository
    BotTemplate const* tmpl = sBotTemplateRepository->SelectRandomTemplate(role, faction);
    if (!tmpl)
    {
        CloneResult result;
        result.success = false;
        result.errorMessage = Trinity::StringFormat("No template found for role {} faction {}",
            BotRoleToString(role), FactionToString(faction));
        return result;
    }

    return ExecuteClone(tmpl, targetLevel, faction, targetGearScore);
}

std::vector<CloneResult> BotCloneEngine::BatchClone(BatchCloneRequest const& request)
{
    std::vector<CloneResult> results;
    results.reserve(request.count);

    if (!_initialized.load())
    {
        CloneResult failResult;
        failResult.success = false;
        failResult.errorMessage = "BotCloneEngine not initialized";
        for (uint32 i = 0; i < request.count; ++i)
            results.push_back(failResult);
        return results;
    }

    if (!request.IsValid())
    {
        CloneResult failResult;
        failResult.success = false;
        failResult.errorMessage = "Invalid batch request parameters";
        for (uint32 i = 0; i < request.count; ++i)
            results.push_back(failResult);
        return results;
    }

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::BatchClone - Creating {} bots for role {} faction {}",
        request.count, BotRoleToString(request.role), FactionToString(request.faction));

    // Get templates for the role
    std::vector<BotTemplate const*> templates = sBotTemplateRepository->GetTemplatesForRole(request.role);

    // Filter by faction and preferred class
    std::vector<BotTemplate const*> validTemplates;
    for (auto* tmpl : templates)
    {
        if (request.preferredClass != 0 && tmpl->playerClass != request.preferredClass)
            continue;

        // Check faction compatibility
        if (request.faction == Faction::Alliance && tmpl->allianceRaces.empty())
            continue;
        if (request.faction == Faction::Horde && tmpl->hordeRaces.empty())
            continue;

        validTemplates.push_back(tmpl);
    }

    if (validTemplates.empty())
    {
        CloneResult failResult;
        failResult.success = false;
        failResult.errorMessage = "No valid templates found for batch request";
        for (uint32 i = 0; i < request.count; ++i)
            results.push_back(failResult);
        return results;
    }

    // Create bots using round-robin template selection for variety
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, validTemplates.size() - 1);

    for (uint32 i = 0; i < request.count; ++i)
    {
        BotTemplate const* tmpl = validTemplates[dist(gen)];
        CloneResult result = ExecuteClone(tmpl, request.targetLevel, request.faction, request.minGearScore);
        results.push_back(std::move(result));
    }

    // Log summary
    uint32 successCount = 0;
    for (auto const& r : results)
    {
        if (r.success)
            ++successCount;
    }

    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::BatchClone - Completed: {}/{} successful",
        successCount, request.count);

    return results;
}

// ============================================================================
// ASYNCHRONOUS CLONING
// ============================================================================

void BotCloneEngine::CloneAsync(
    BotTemplate const* tmpl,
    uint32 targetLevel,
    Faction faction,
    CloneCallback callback)
{
    if (!_initialized.load())
    {
        if (callback)
        {
            CloneResult result;
            result.success = false;
            result.errorMessage = "BotCloneEngine not initialized";
            callback(result);
        }
        return;
    }

    AsyncCloneTask task;
    task.tmpl = tmpl;
    task.targetLevel = targetLevel;
    task.faction = faction;
    task.targetGearScore = 0;
    task.callback = std::move(callback);

    {
        std::lock_guard<std::mutex> lock(_asyncMutex);
        _asyncQueue.push(std::move(task));
    }
}

void BotCloneEngine::BatchCloneAsync(
    BatchCloneRequest const& request,
    BatchCloneCallback callback)
{
    if (!_initialized.load())
    {
        if (callback)
        {
            std::vector<CloneResult> results;
            CloneResult failResult;
            failResult.success = false;
            failResult.errorMessage = "BotCloneEngine not initialized";
            for (uint32 i = 0; i < request.count; ++i)
                results.push_back(failResult);
            callback(results);
        }
        return;
    }

    AsyncBatchTask task;
    task.request = request;
    task.callback = std::move(callback);

    {
        std::lock_guard<std::mutex> lock(_asyncMutex);
        _asyncBatchQueue.push(std::move(task));
    }
}

// ============================================================================
// QUERIES
// ============================================================================

std::chrono::milliseconds BotCloneEngine::GetEstimatedCloneTime(uint32 count) const
{
    if (count == 0)
        return std::chrono::milliseconds(0);

    // Base estimate: 50ms per bot + 10ms overhead
    // Adjust based on average creation time
    std::chrono::milliseconds avgTime = _avgCreationTime;
    if (avgTime.count() == 0)
        avgTime = std::chrono::milliseconds(50);

    return avgTime * count + std::chrono::milliseconds(10);
}

uint32 BotCloneEngine::GetPendingCloneCount() const
{
    std::lock_guard<std::mutex> lock(_asyncMutex);
    uint32 pending = static_cast<uint32>(_asyncQueue.size());

    // Count batch requests as individual clones
    std::queue<AsyncBatchTask> batchCopy = _asyncBatchQueue;
    while (!batchCopy.empty())
    {
        pending += batchCopy.front().request.count;
        batchCopy.pop();
    }

    return pending;
}

uint32 BotCloneEngine::GetClonesThisHour() const
{
    return _clonesThisHour.load();
}

bool BotCloneEngine::IsBusy() const
{
    return GetPendingCloneCount() > 10;
}

// ============================================================================
// STATISTICS
// ============================================================================

BotCloneEngine::Statistics BotCloneEngine::GetStatistics() const
{
    Statistics stats;

    stats.totalClonesCreated = _totalClonesCreated.load();
    stats.clonesThisHour = _clonesThisHour.load();
    stats.failedClonesThisHour = _failedClonesThisHour.load();
    stats.avgCreationTime = _avgCreationTime;
    stats.peakCreationTime = _peakCreationTime;
    stats.pendingAsyncOperations = GetPendingCloneCount();

    // Get account info from BotAccountMgr
    stats.accountPoolSize = sBotAccountMgr->GetTotalAccountCount();
    stats.availableAccounts = sBotAccountMgr->GetPoolSize();

    return stats;
}

void BotCloneEngine::PrintStatistics() const
{
    Statistics stats = GetStatistics();

    TC_LOG_INFO("playerbot.clone", "=== BotCloneEngine Statistics ===");
    TC_LOG_INFO("playerbot.clone", "Total Clones Created: {}", stats.totalClonesCreated);
    TC_LOG_INFO("playerbot.clone", "Clones This Hour: {}", stats.clonesThisHour);
    TC_LOG_INFO("playerbot.clone", "Failed This Hour: {}", stats.failedClonesThisHour);
    TC_LOG_INFO("playerbot.clone", "Avg Creation Time: {}ms", stats.avgCreationTime.count());
    TC_LOG_INFO("playerbot.clone", "Peak Creation Time: {}ms", stats.peakCreationTime.count());
    TC_LOG_INFO("playerbot.clone", "Pending Operations: {}", stats.pendingAsyncOperations);
    TC_LOG_INFO("playerbot.clone", "Account Pool: {}/{} available",
        stats.availableAccounts, stats.accountPoolSize);
}

// ============================================================================
// INTERNAL METHODS - Resource Allocation
// ============================================================================

ObjectGuid BotCloneEngine::AllocateGuid()
{
    // Get next available GUID from ObjectMgr
    ObjectGuid::LowType lowGuid = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
    return ObjectGuid::Create<HighGuid::Player>(lowGuid);
}

uint32 BotCloneEngine::AllocateAccount()
{
    // Use BotAccountMgr for real accounts instead of fake pool
    // This ensures accounts exist in the auth database and can be used for login
    uint32 accountId = sBotAccountMgr->AcquireAccount();

    if (accountId == 0)
    {
        TC_LOG_ERROR("playerbot.clone", "BotCloneEngine::AllocateAccount - Failed to acquire account from BotAccountMgr!");
        return 0;
    }

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::AllocateAccount - Acquired account {} from BotAccountMgr",
        accountId);
    return accountId;
}

void BotCloneEngine::ReleaseAccount(uint32 accountId)
{
    if (accountId == 0)
        return;

    // Return account to BotAccountMgr pool
    sBotAccountMgr->ReleaseAccount(accountId);
    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ReleaseAccount - Released account {} to BotAccountMgr",
        accountId);
}

std::string BotCloneEngine::GenerateUniqueName(uint8 race, uint8 gender)
{
    std::lock_guard<std::mutex> lock(_nameMutex);

    // Select name pool based on gender
    std::vector<std::string> const& pool = (gender == 0) ? _maleNames : _femaleNames;

    if (pool.empty())
    {
        // Fallback to generated name
        uint32 suffix = _nameSuffix.fetch_add(1);
        return Trinity::StringFormat("Bot{}", suffix);
    }

    // Get base name from pool
    uint32 index = _nameIndex.fetch_add(1) % pool.size();
    std::string baseName = pool[index];

    // Check if name is available, if not add suffix
    std::string name = baseName;
    uint32 attempt = 0;
    while (!IsNameAvailable(name) && attempt < 100)
    {
        uint32 suffix = _nameSuffix.fetch_add(1);
        name = Trinity::StringFormat("{}{}", baseName, suffix % 1000);
        ++attempt;
    }

    return name;
}

// ============================================================================
// INTERNAL METHODS - Clone Execution
// ============================================================================

CloneResult BotCloneEngine::ExecuteClone(
    BotTemplate const* tmpl,
    uint32 targetLevel,
    Faction faction,
    uint32 targetGearScore)
{
    auto startTime = std::chrono::steady_clock::now();
    CloneResult result;

    // Validate template
    if (!tmpl || !tmpl->IsValid())
    {
        result.success = false;
        result.errorMessage = "Invalid or incomplete template";
        _failedClonesThisHour.fetch_add(1);
        return result;
    }

    // Allocate resources
    ObjectGuid guid = AllocateGuid();
    if (guid == ObjectGuid::Empty)
    {
        result.success = false;
        result.errorMessage = "Failed to allocate GUID";
        _failedClonesThisHour.fetch_add(1);
        return result;
    }

    uint32 accountId = AllocateAccount();
    if (accountId == 0)
    {
        result.success = false;
        result.errorMessage = "Failed to allocate account";
        _failedClonesThisHour.fetch_add(1);
        return result;
    }

    // Get race for faction
    uint8 race = tmpl->GetRandomRace(faction);
    if (race == 0)
    {
        ReleaseAccount(accountId);
        result.success = false;
        result.errorMessage = Trinity::StringFormat("No valid race for {} in template {}",
            FactionToString(faction), tmpl->templateName);
        _failedClonesThisHour.fetch_add(1);
        return result;
    }

    // Generate name and gender
    uint8 gender = (std::rand() % 2);
    std::string name = GenerateUniqueName(race, gender);

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ExecuteClone - Creating bot: Name={}, Race={}, Class={}, Level={}",
        name, race, tmpl->playerClass, targetLevel);

    // Create the player object in database
    if (!CreatePlayerObject(guid, accountId, name, race, tmpl->playerClass, gender, targetLevel))
    {
        ReleaseAccount(accountId);
        result.success = false;
        result.errorMessage = "Failed to create player object";
        _failedClonesThisHour.fetch_add(1);
        return result;
    }

    // Apply gear scaling
    ApplyGearScaling(guid, tmpl, targetLevel, targetGearScore);

    // Apply talents
    ApplyTalents(guid, tmpl);

    // Apply action bars
    ApplyActionBars(guid, tmpl);

    // Calculate creation time
    auto endTime = std::chrono::steady_clock::now();
    auto creationTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Update statistics
    _totalClonesCreated.fetch_add(1);
    _clonesThisHour.fetch_add(1);

    // Update timing statistics
    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        ++_creationTimeSamples;

        // Calculate running average
        auto totalMs = _avgCreationTime.count() * (_creationTimeSamples - 1) + creationTime.count();
        _avgCreationTime = std::chrono::milliseconds(totalMs / _creationTimeSamples);

        // Track peak
        if (creationTime > _peakCreationTime)
            _peakCreationTime = creationTime;
    }

    // Record template usage
    sBotTemplateRepository->RecordTemplateUsage(tmpl->templateId, creationTime);

    // Build result
    result.success = true;
    result.botGuid = guid;
    result.accountId = accountId;
    result.botName = name;
    result.race = race;
    result.playerClass = tmpl->playerClass;
    result.specId = tmpl->specId;
    result.role = tmpl->role;
    result.faction = faction;
    result.level = targetLevel;
    result.gearScore = targetGearScore > 0 ? targetGearScore :
        (tmpl->gearSets.empty() ? 0 : tmpl->gearSets.begin()->second.actualGearScore);
    result.creationTime = creationTime;

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ExecuteClone - Bot created: {}",
        result.ToString());

    return result;
}

bool BotCloneEngine::CreatePlayerObject(
    ObjectGuid guid, uint32 accountId,
    std::string const& name,
    uint8 race, uint8 playerClass, uint8 gender, uint32 level)
{
    // Get starting position from PlayerInfo
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(race, playerClass);
    if (!info)
    {
        TC_LOG_ERROR("playerbot.clone", "BotCloneEngine::CreatePlayerObject - "
            "No PlayerInfo for race {} class {}", race, playerClass);
        return false;
    }

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::CreatePlayerObject - "
        "GUID={}, Account={}, Name={}, Race={}, Class={}, Gender={}, Level={}",
        guid.ToString(), accountId, name, race, playerClass, gender, level);

    // Get starting position
    float posX = info->createPosition.Loc.GetPositionX();
    float posY = info->createPosition.Loc.GetPositionY();
    float posZ = info->createPosition.Loc.GetPositionZ();
    float orientation = info->createPosition.Loc.GetOrientation();
    uint32 mapId = info->createPosition.Loc.GetMapId();

    // Insert character into database
    // Match exact column order from CHAR_INS_CHARACTER:
    // guid, account, name, race, class, gender, level, xp, money, inventorySlots, inventoryBagFlags,
    // bagSlotFlags1-5, bankSlots, bankTabs, bankBagFlags, restState, playerFlags, playerFlagsEx,
    // map, instance_id, dungeonDifficulty, raidDifficulty, legacyRaidDifficulty,
    // position_x, position_y, position_z, orientation, trans_x, trans_y, trans_z, trans_o, transguid,
    // taximask, createTime, createMode, cinematic, totaltime, leveltime, rest_bonus, logout_time,
    // is_logout_resting, resettalents_cost, resettalents_time, primarySpecialization,
    // extra_flags, summonedPetNumber, at_login, death_expire_time, taxi_path, totalKills,
    // todayKills, yesterdayKills, chosenTitle, watchedFaction, drunk, health,
    // power1-10, latency, activeTalentGroup, lootSpecId, exploredZones, equipmentCache,
    // knownTitles, actionBars, lastLoginBuild

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER);
    if (!stmt)
    {
        TC_LOG_ERROR("playerbot.clone", "BotCloneEngine::CreatePlayerObject - Failed to get prepared statement");
        return false;
    }

    uint8 index = 0;

    stmt->setUInt64(index++, guid.GetCounter());         // guid
    stmt->setUInt32(index++, accountId);                  // account
    stmt->setString(index++, name);                       // name
    stmt->setUInt8(index++, race);                        // race
    stmt->setUInt8(index++, playerClass);                 // class
    stmt->setUInt8(index++, gender);                      // gender
    stmt->setUInt8(index++, static_cast<uint8>(level));   // level
    stmt->setUInt32(index++, 0);                          // xp
    stmt->setUInt64(index++, 0);                          // money
    stmt->setUInt8(index++, 16);                          // inventorySlots (default)
    stmt->setUInt32(index++, 0);                          // inventoryBagFlags

    // bagSlotFlags1-5
    for (int i = 0; i < 5; ++i)
        stmt->setUInt32(index++, 0);

    stmt->setUInt8(index++, 0);                           // bankSlots
    stmt->setUInt8(index++, 0);                           // bankTabs
    stmt->setUInt32(index++, 0);                          // bankBagFlags
    stmt->setUInt8(index++, 0);                           // restState
    stmt->setUInt32(index++, 0);                          // playerFlags
    stmt->setUInt32(index++, 0);                          // playerFlagsEx
    stmt->setUInt16(index++, static_cast<uint16>(mapId)); // map
    stmt->setUInt32(index++, 0);                          // instance_id
    stmt->setUInt8(index++, 0);                           // dungeonDifficulty
    stmt->setUInt8(index++, 0);                           // raidDifficulty
    stmt->setUInt8(index++, 0);                           // legacyRaidDifficulty
    stmt->setFloat(index++, posX);                        // position_x
    stmt->setFloat(index++, posY);                        // position_y
    stmt->setFloat(index++, posZ);                        // position_z
    stmt->setFloat(index++, orientation);                 // orientation
    stmt->setFloat(index++, 0.0f);                        // trans_x
    stmt->setFloat(index++, 0.0f);                        // trans_y
    stmt->setFloat(index++, 0.0f);                        // trans_z
    stmt->setFloat(index++, 0.0f);                        // trans_o
    stmt->setUInt64(index++, 0);                          // transguid
    stmt->setString(index++, std::string());              // taximask
    stmt->setInt64(index++, GameTime::GetGameTime());     // createTime
    stmt->setInt8(index++, 0);                            // createMode (Normal = 0)
    stmt->setUInt8(index++, 0);                           // cinematic
    stmt->setUInt32(index++, 0);                          // totaltime
    stmt->setUInt32(index++, 0);                          // leveltime
    stmt->setFloat(index++, 0.0f);                        // rest_bonus
    stmt->setUInt64(index++, GameTime::GetGameTime());    // logout_time
    stmt->setUInt8(index++, 0);                           // is_logout_resting
    stmt->setUInt32(index++, 0);                          // resettalents_cost
    stmt->setInt64(index++, 0);                           // resettalents_time
    stmt->setUInt32(index++, 0);                          // primarySpecialization
    stmt->setUInt16(index++, 0);                          // extra_flags
    stmt->setUInt32(index++, 0);                          // summonedPetNumber
    stmt->setUInt16(index++, 0);                          // at_login
    stmt->setInt64(index++, 0);                           // death_expire_time
    stmt->setString(index++, std::string());              // taxi_path
    stmt->setUInt32(index++, 0);                          // totalKills
    stmt->setUInt16(index++, 0);                          // todayKills
    stmt->setUInt16(index++, 0);                          // yesterdayKills
    stmt->setUInt32(index++, 0);                          // chosenTitle
    stmt->setUInt32(index++, -1);                         // watchedFaction (-1 = none)
    stmt->setUInt8(index++, 0);                           // drunk
    stmt->setUInt32(index++, 100);                        // health (placeholder)

    // power1-10
    for (int i = 0; i < 10; ++i)
        stmt->setUInt32(index++, 0);

    stmt->setUInt32(index++, 0);                          // latency
    stmt->setUInt8(index++, 0);                           // activeTalentGroup
    stmt->setUInt32(index++, 0);                          // lootSpecId
    stmt->setString(index++, std::string());              // exploredZones
    stmt->setString(index++, std::string());              // equipmentCache
    stmt->setString(index++, std::string());              // knownTitles
    stmt->setUInt8(index++, 0);                           // actionBars
    stmt->setUInt32(index++, 0);                          // lastLoginBuild

    trans->Append(stmt);

    // Async commit - InstanceBotPool will retry warmup via ProcessWarmingRetries()
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::CreatePlayerObject - "
        "Character record created with {} parameters", index);

    return true;
}

void BotCloneEngine::ApplyGearScaling(
    ObjectGuid botGuid,
    BotTemplate const* tmpl,
    uint32 targetLevel,
    uint32 targetGearScore)
{
    if (!tmpl)
        return;

    // Get gear set for target item level
    GearSetTemplate const* gearSet = tmpl->GetGearSetForItemLevel(targetGearScore);
    if (!gearSet || !gearSet->IsValid())
    {
        TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyGearScaling - "
            "No valid gear set found for GS {}", targetGearScore);
        return;
    }

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyGearScaling - "
        "Applying gear set for bot {}, target iLvl={}, actual GS={}",
        botGuid.ToString(), targetGearScore, gearSet->actualGearScore);

    // Insert gear items into character_inventory
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (size_t slot = 0; slot < gearSet->slots.size(); ++slot)
    {
        GearSlotTemplate const& gearSlot = gearSet->slots[slot];
        if (gearSlot.itemId == 0)
            continue;

        // Allocate item GUID
        ObjectGuid::LowType itemGuid = sObjectMgr->GetGenerator<HighGuid::Item>().Generate();

        // Insert into item_instance using CHAR_REP_ITEM_INSTANCE
        // Column order: itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, duration,
        //               charges, flags, enchantments, randomBonusListId, durability, playedTime,
        //               createTime, text, battlePetSpeciesId, battlePetBreedData, battlePetLevel,
        //               battlePetDisplayId, context, bonusListIDs, guid
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_INSTANCE);
        if (!stmt)
            continue;

        uint8 idx = 0;
        stmt->setUInt32(idx++, gearSlot.itemId);         // itemEntry
        stmt->setUInt64(idx++, botGuid.GetCounter());    // owner_guid
        stmt->setUInt64(idx++, 0);                       // creatorGuid
        stmt->setUInt64(idx++, 0);                       // giftCreatorGuid
        stmt->setUInt32(idx++, 1);                       // count
        stmt->setUInt32(idx++, 0);                       // duration
        stmt->setString(idx++, std::string());           // charges
        stmt->setUInt32(idx++, 0);                       // flags
        stmt->setString(idx++, std::string());           // enchantments
        stmt->setUInt32(idx++, 0);                       // randomBonusListId
        stmt->setUInt32(idx++, 0);                       // durability (will be set when item loads)
        stmt->setUInt32(idx++, 0);                       // playedTime
        stmt->setUInt64(idx++, GameTime::GetGameTime()); // createTime
        stmt->setString(idx++, std::string());           // text
        stmt->setUInt32(idx++, 0);                       // battlePetSpeciesId
        stmt->setUInt32(idx++, 0);                       // battlePetBreedData
        stmt->setUInt16(idx++, 0);                       // battlePetLevel
        stmt->setUInt32(idx++, 0);                       // battlePetDisplayId
        stmt->setUInt8(idx++, static_cast<uint8>(ItemContext::NONE)); // context
        stmt->setString(idx++, std::string());           // bonusListIDs
        stmt->setUInt64(idx++, itemGuid);                // guid (at end for REPLACE)

        trans->Append(stmt);

        // Insert into character_inventory using CHAR_REP_INVENTORY_ITEM
        // Column order: guid, bag, slot, item
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
        if (stmt)
        {
            stmt->setUInt64(0, botGuid.GetCounter());    // character guid
            stmt->setUInt64(1, 0);                        // bag (0 = equipped)
            stmt->setUInt8(2, static_cast<uint8>(slot)); // slot
            stmt->setUInt64(3, itemGuid);                // item guid

            trans->Append(stmt);
        }
    }

    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyGearScaling - "
        "Applied gear set to bot {}", botGuid.ToString());
}

void BotCloneEngine::ApplyTalents(ObjectGuid botGuid, BotTemplate const* tmpl)
{
    if (!tmpl)
        return;

    // Check if we have talent data
    if (tmpl->talents.talentIds.empty() && tmpl->talentBlob.empty())
    {
        TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyTalents - "
            "No talent data for bot {}", botGuid.ToString());
        return;
    }

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyTalents - "
        "Applying {} talents for bot {} spec {}",
        tmpl->talents.talentIds.size(), botGuid.ToString(), tmpl->specId);

    // Apply talents to database
    // Note: primarySpecialization is already set in CHAR_INS_CHARACTER during bot creation
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // Insert talents using CHAR_INS_CHAR_TALENT
    // Column order: guid, talentId, talentGroup
    for (uint32 talentId : tmpl->talents.talentIds)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_TALENT);
        if (!stmt)
            continue;

        stmt->setUInt64(0, botGuid.GetCounter());
        stmt->setUInt32(1, talentId);
        stmt->setUInt8(2, 0); // talentGroup

        trans->Append(stmt);
    }

    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyTalents - "
        "Applied talents to bot {}", botGuid.ToString());
}

void BotCloneEngine::ApplyActionBars(ObjectGuid botGuid, BotTemplate const* tmpl)
{
    if (!tmpl)
        return;

    // Check if we have action bar data
    if (tmpl->actionBars.buttons.empty() && tmpl->actionBarBlob.empty())
    {
        TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyActionBars - "
            "No action bar data for bot {}", botGuid.ToString());
        return;
    }

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyActionBars - "
        "Applying {} action buttons for bot {}",
        tmpl->actionBars.buttons.size(), botGuid.ToString());

    // Apply action bars to database
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto const& button : tmpl->actionBars.buttons)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_ACTION);
        if (!stmt)
            continue;

        // Calculate global button slot (bar * 12 + slot)
        uint8 globalSlot = button.actionBar * 12 + button.slot;

        stmt->setUInt64(0, botGuid.GetCounter());
        stmt->setUInt8(1, 0);           // specIndex
        stmt->setUInt8(2, globalSlot);
        stmt->setUInt32(3, button.actionId);
        stmt->setUInt8(4, static_cast<uint8>(button.actionType));

        trans->Append(stmt);
    }

    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::ApplyActionBars - "
        "Applied action bars to bot {}", botGuid.ToString());
}

bool BotCloneEngine::FastLogin(ObjectGuid botGuid)
{
    TC_LOG_DEBUG("playerbot.clone", "BotCloneEngine::FastLogin - Fast login for bot {}",
        botGuid.ToString());

    // Note: Fast login would skip the full login process and directly
    // create a Player object in memory, then add to world
    // This requires deep integration with TrinityCore's session system

    return true;
}

// ============================================================================
// INTERNAL METHODS - Async Processing
// ============================================================================

void BotCloneEngine::AsyncWorkerThread()
{
    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::AsyncWorkerThread - Worker thread started");

    while (_running.load())
    {
        ProcessAsyncQueue();

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::AsyncWorkerThread - Worker thread stopped");
}

void BotCloneEngine::ProcessAsyncQueue()
{
    // Process single clone tasks
    {
        AsyncCloneTask task;
        bool hasTask = false;

        {
            std::lock_guard<std::mutex> lock(_asyncMutex);
            if (!_asyncQueue.empty())
            {
                task = std::move(_asyncQueue.front());
                _asyncQueue.pop();
                hasTask = true;
            }
        }

        if (hasTask)
        {
            CloneResult result = ExecuteClone(
                task.tmpl,
                task.targetLevel,
                task.faction,
                task.targetGearScore);

            if (task.callback)
            {
                task.callback(result);
            }
        }
    }

    // Process batch clone tasks
    {
        AsyncBatchTask task;
        bool hasTask = false;

        {
            std::lock_guard<std::mutex> lock(_asyncMutex);
            if (!_asyncBatchQueue.empty())
            {
                task = std::move(_asyncBatchQueue.front());
                _asyncBatchQueue.pop();
                hasTask = true;
            }
        }

        if (hasTask)
        {
            std::vector<CloneResult> results = BatchClone(task.request);

            if (task.callback)
            {
                task.callback(results);
            }
        }
    }
}

// ============================================================================
// INTERNAL METHODS - Name Generation
// ============================================================================

void BotCloneEngine::LoadNamePool()
{
    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::LoadNamePool - Loading name pools");

    // Default fantasy-style names
    // These would typically be loaded from database or config file

    _maleNames = {
        "Aldric", "Borin", "Cedric", "Darian", "Eldric",
        "Falric", "Galric", "Hadric", "Ivric", "Jarric",
        "Kaldric", "Lorric", "Malric", "Norric", "Olric",
        "Perric", "Quilric", "Raldric", "Seldric", "Talric",
        "Uldric", "Valdric", "Waldric", "Xaldric", "Yaldric",
        "Aldrin", "Borin", "Corrin", "Darrin", "Eldrin",
        "Falrin", "Galrin", "Hadrin", "Ivrin", "Jarrin",
        "Kaldrin", "Lorrin", "Malrin", "Norrin", "Olrin",
        "Theron", "Gareth", "Roland", "Edmund", "Alfred",
        "Oswald", "Leofric", "Godwin", "Edgar", "Harold"
    };

    _femaleNames = {
        "Alara", "Brynn", "Cyra", "Darya", "Elara",
        "Freya", "Gwyra", "Hilda", "Ilara", "Jyra",
        "Kyra", "Lyra", "Myra", "Nyra", "Olara",
        "Petra", "Quara", "Ryra", "Syra", "Tyra",
        "Ulara", "Vyra", "Wyra", "Xyra", "Yara",
        "Aldara", "Belinda", "Cordelia", "Diana", "Elena",
        "Fiona", "Giselle", "Helena", "Iris", "Julia",
        "Katrina", "Lavinia", "Miranda", "Natalia", "Ophelia",
        "Rowena", "Sabrina", "Thalia", "Vivian", "Winifred"
    };

    // Load additional names from playerbot database if available
    QueryResult result = sPlayerbotDatabase->Query("SELECT name, gender FROM playerbot_name_pool");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            std::string name = fields[0].GetString();
            uint8 gender = fields[1].GetUInt8();

            if (gender == 0)
                _maleNames.push_back(name);
            else
                _femaleNames.push_back(name);

        } while (result->NextRow());
    }

    TC_LOG_INFO("playerbot.clone", "BotCloneEngine::LoadNamePool - Loaded {} male names, {} female names",
        _maleNames.size(), _femaleNames.size());
}

bool BotCloneEngine::IsNameAvailable(std::string const& name) const
{
    // Check if name already exists in characters table
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_NAME);
    if (!stmt)
        return false;

    stmt->setString(0, name);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    return !result;
}

} // namespace Playerbot
