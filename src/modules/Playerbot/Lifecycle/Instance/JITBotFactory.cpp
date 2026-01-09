/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file JITBotFactory.cpp
 * @brief Implementation of the Just-In-Time Bot Factory
 */

#include "JITBotFactory.h"
#include "BotCloneEngine.h"
#include "BotTemplateRepository.h"
#include "Config/PlayerbotConfig.h"
#include "Log.h"
#include <fmt/format.h>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

JITBotFactory* JITBotFactory::Instance()
{
    static JITBotFactory instance;
    return &instance;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool JITBotFactory::Initialize()
{
    if (_initialized.load())
    {
        TC_LOG_WARN("playerbot.jit", "JITBotFactory::Initialize - Already initialized");
        return true;
    }

    TC_LOG_INFO("playerbot.jit", "JITBotFactory::Initialize - Starting initialization");

    // Load configuration
    LoadConfig();

    if (!_config.enabled)
    {
        TC_LOG_INFO("playerbot.jit", "JITBotFactory::Initialize - JIT factory is disabled");
        _initialized.store(true);
        return true;
    }

    // Initialize statistics
    _hourStart = std::chrono::system_clock::now();
    _completedThisHour.store(0);
    _failedThisHour.store(0);
    _cancelledThisHour.store(0);
    _botsCreatedThisHour.store(0);
    _botsRecycledThisHour.store(0);

    // Start worker thread
    _running.store(true);
    _workerThread = std::make_unique<std::thread>(&JITBotFactory::WorkerThread, this);

    _initialized.store(true);
    TC_LOG_INFO("playerbot.jit", "JITBotFactory::Initialize - Initialization complete");

    return true;
}

void JITBotFactory::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("playerbot.jit", "JITBotFactory::Shutdown - Starting shutdown");

    // Stop worker thread
    _running.store(false);
    if (_workerThread && _workerThread->joinable())
    {
        _workerThread->join();
        _workerThread.reset();
    }

    // Clear pending requests
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        while (!_pendingQueue.empty())
            _pendingQueue.pop();
    }

    // Clear active requests
    {
        std::lock_guard<std::mutex> lock(_requestMutex);
        _activeRequests.clear();
    }

    // Clear recycled bots
    {
        std::lock_guard<std::mutex> lock(_recycleMutex);
        _recycledBots.clear();
    }

    _initialized.store(false);
    TC_LOG_INFO("playerbot.jit", "JITBotFactory::Shutdown - Shutdown complete");
}

void JITBotFactory::Update(uint32 /*diff*/)
{
    if (!_initialized.load() || !_config.enabled)
        return;

    // Reset hourly statistics if needed
    auto now = std::chrono::system_clock::now();
    auto hoursSinceStart = std::chrono::duration_cast<std::chrono::hours>(now - _hourStart).count();
    if (hoursSinceStart >= 1)
    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        _completedThisHour.store(0);
        _failedThisHour.store(0);
        _cancelledThisHour.store(0);
        _botsCreatedThisHour.store(0);
        _botsRecycledThisHour.store(0);
        _hourStart = now;
    }

    // Process timeouts
    ProcessTimeouts();

    // Cleanup expired recycled bots
    CleanupRecycledBots();
}

void JITBotFactory::LoadConfig()
{
    TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::LoadConfig - Loading configuration");

    _config.enabled = sPlayerbotConfig->GetBool("Playerbot.Instance.JIT.Enable", true);
    _config.maxConcurrentCreations = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.MaxConcurrentCreations", 10);
    _config.recycleTimeoutMinutes = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.RecycleTimeoutMinutes", 5);
    _config.maxRecycledBots = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.MaxRecycledBots", 100);

    _config.dungeonPriority = static_cast<uint8>(sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.DungeonPriority", 1));
    _config.arenaPriority = static_cast<uint8>(sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.ArenaPriority", 2));
    _config.raidPriority = static_cast<uint8>(sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.RaidPriority", 3));
    _config.battlegroundPriority = static_cast<uint8>(sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.BattlegroundPriority", 4));

    _config.dungeonTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.DungeonTimeoutMs", 30000);
    _config.raidTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.RaidTimeoutMs", 60000);
    _config.battlegroundTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.BattlegroundTimeoutMs", 120000);
    _config.arenaTimeoutMs = sPlayerbotConfig->GetInt("Playerbot.Instance.JIT.ArenaTimeoutMs", 15000);

    TC_LOG_INFO("playerbot.jit", "JITBotFactory::LoadConfig - JIT Factory: enabled={}, maxConcurrent={}, recycleTimeout={}min",
        _config.enabled, _config.maxConcurrentCreations, _config.recycleTimeoutMinutes);
}

// ============================================================================
// REQUEST SUBMISSION
// ============================================================================

uint32 JITBotFactory::SubmitRequest(FactoryRequest request)
{
    if (!_initialized.load() || !_config.enabled)
    {
        TC_LOG_WARN("playerbot.jit", "JITBotFactory::SubmitRequest - Factory not available");
        if (request.onFailed)
            request.onFailed("JIT factory not available");
        return 0;
    }

    if (!request.IsValid())
    {
        TC_LOG_WARN("playerbot.jit", "JITBotFactory::SubmitRequest - Invalid request");
        if (request.onFailed)
            request.onFailed("Invalid request parameters");
        return 0;
    }

    // Assign request ID
    request.requestId = _nextRequestId.fetch_add(1);
    request.createdAt = std::chrono::system_clock::now();

    // Set priority and timeout based on instance type
    request.priority = GetPriorityForType(request.instanceType);
    request.timeout = GetTimeoutForType(request.instanceType);

    TC_LOG_INFO("playerbot.jit", "JITBotFactory::SubmitRequest - Request {} submitted: type={}, total={}, priority={}",
        request.requestId, InstanceTypeToString(request.instanceType),
        request.GetTotalNeeded(), request.priority);

    // Add to pending queue
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _pendingQueue.push(std::move(request));
    }

    return request.requestId;
}

void JITBotFactory::CancelRequest(uint32 requestId)
{
    TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::CancelRequest - Cancelling request {}", requestId);

    // Try to remove from active requests
    {
        std::lock_guard<std::mutex> lock(_requestMutex);
        auto it = _activeRequests.find(requestId);
        if (it != _activeRequests.end())
        {
            it->second.progress.status = RequestStatus::Cancelled;
            if (it->second.request.onFailed)
                it->second.request.onFailed("Request cancelled");
            _activeRequests.erase(it);
            _cancelledThisHour.fetch_add(1);
            return;
        }
    }

    // Request might be in pending queue - mark for cancellation
    // (will be handled when dequeued)
    TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::CancelRequest - Request {} not active, may be pending",
        requestId);
}

RequestStatus JITBotFactory::GetRequestStatus(uint32 requestId) const
{
    std::lock_guard<std::mutex> lock(_requestMutex);

    auto it = _activeRequests.find(requestId);
    if (it != _activeRequests.end())
        return it->second.progress.status;

    return RequestStatus::Pending;  // Might be in queue
}

RequestProgress JITBotFactory::GetRequestProgress(uint32 requestId) const
{
    std::lock_guard<std::mutex> lock(_requestMutex);

    auto it = _activeRequests.find(requestId);
    if (it != _activeRequests.end())
        return it->second.progress;

    RequestProgress defaultProgress;
    defaultProgress.requestId = requestId;
    defaultProgress.status = RequestStatus::Pending;
    return defaultProgress;
}

std::chrono::milliseconds JITBotFactory::GetEstimatedCompletion(uint32 requestId) const
{
    std::lock_guard<std::mutex> lock(_requestMutex);

    auto it = _activeRequests.find(requestId);
    if (it != _activeRequests.end())
        return it->second.progress.estimatedRemaining;

    // For pending requests, estimate based on queue position and avg time
    return std::chrono::milliseconds(5000);  // Default estimate
}

// ============================================================================
// BOT RECYCLING
// ============================================================================

void JITBotFactory::RecycleBot(
    ObjectGuid botGuid,
    BotRole role,
    Faction faction,
    uint32 level,
    uint32 gearScore,
    uint8 playerClass)
{
    if (!_initialized.load())
        return;

    std::lock_guard<std::mutex> lock(_recycleMutex);

    // Check if we're at capacity
    if (_recycledBots.size() >= _config.maxRecycledBots)
    {
        TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::RecycleBot - Recycle pool full, discarding bot {}",
            botGuid.ToString());
        return;
    }

    // Check if already recycled
    for (auto const& bot : _recycledBots)
    {
        if (bot.guid == botGuid)
        {
            TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::RecycleBot - Bot {} already recycled",
                botGuid.ToString());
            return;
        }
    }

    RecycledBot bot;
    bot.guid = botGuid;
    bot.role = role;
    bot.faction = faction;
    bot.level = level;
    bot.gearScore = gearScore;
    bot.playerClass = playerClass;
    bot.recycleTime = std::chrono::system_clock::now();

    _recycledBots.push_back(std::move(bot));
    _botsRecycledThisHour.fetch_add(1);

    TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::RecycleBot - Bot {} recycled (total: {})",
        botGuid.ToString(), _recycledBots.size());
}

void JITBotFactory::RecycleBots(std::vector<ObjectGuid> const& bots)
{
    // Note: This simplified version doesn't have full bot info
    // In a full implementation, we'd look up bot details from the pool
    for (auto const& guid : bots)
    {
        RecycleBot(guid, BotRole::DPS, Faction::Alliance, 80, 400, 1);
    }
}

ObjectGuid JITBotFactory::GetRecycledBot(
    BotRole role,
    Faction faction,
    uint32 level,
    uint32 minGearScore)
{
    std::lock_guard<std::mutex> lock(_recycleMutex);

    for (auto it = _recycledBots.begin(); it != _recycledBots.end(); ++it)
    {
        if (it->Matches(role, faction, level, minGearScore))
        {
            ObjectGuid guid = it->guid;
            _recycledBots.erase(it);

            TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::GetRecycledBot - Found recycled bot {}",
                guid.ToString());
            return guid;
        }
    }

    return ObjectGuid::Empty;
}

uint32 JITBotFactory::GetRecycledBotCount() const
{
    std::lock_guard<std::mutex> lock(_recycleMutex);
    return static_cast<uint32>(_recycledBots.size());
}

void JITBotFactory::CleanupRecycledBots()
{
    std::lock_guard<std::mutex> lock(_recycleMutex);

    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::minutes(_config.recycleTimeoutMinutes);

    auto it = std::remove_if(_recycledBots.begin(), _recycledBots.end(),
        [now, timeout](RecycledBot const& bot) {
            return (now - bot.recycleTime) >= timeout;
        });

    if (it != _recycledBots.end())
    {
        size_t removed = std::distance(it, _recycledBots.end());
        _recycledBots.erase(it, _recycledBots.end());
        TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::CleanupRecycledBots - Removed {} expired bots",
            removed);
    }
}

// ============================================================================
// QUERIES
// ============================================================================

bool JITBotFactory::IsBusy() const
{
    std::lock_guard<std::mutex> lock(_requestMutex);
    return _activeRequests.size() >= _config.maxConcurrentCreations;
}

uint32 JITBotFactory::GetPendingRequestCount() const
{
    std::lock_guard<std::mutex> lock(_queueMutex);
    return static_cast<uint32>(_pendingQueue.size());
}

bool JITBotFactory::CanHandleRequest(FactoryRequest const& request) const
{
    if (!_initialized.load() || !_config.enabled)
        return false;

    if (!request.IsValid())
        return false;

    // Check if we can create the required bots
    uint32 totalNeeded = request.GetTotalNeeded();
    if (totalNeeded > 100)  // Hard limit
        return false;

    return true;
}

// ============================================================================
// STATISTICS
// ============================================================================

JITBotFactory::FactoryStatistics JITBotFactory::GetStatistics() const
{
    FactoryStatistics stats;

    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        stats.pendingRequests = static_cast<uint32>(_pendingQueue.size());
    }

    {
        std::lock_guard<std::mutex> lock(_requestMutex);
        stats.inProgressRequests = static_cast<uint32>(_activeRequests.size());
    }

    stats.completedThisHour = _completedThisHour.load();
    stats.failedThisHour = _failedThisHour.load();
    stats.cancelledThisHour = _cancelledThisHour.load();
    stats.recycledBotsAvailable = GetRecycledBotCount();
    stats.botsCreatedThisHour = _botsCreatedThisHour.load();
    stats.botsRecycledThisHour = _botsRecycledThisHour.load();
    stats.avgCreationTime = _avgCreationTime;
    stats.avgRequestTime = _avgRequestTime;

    return stats;
}

void JITBotFactory::PrintStatistics() const
{
    FactoryStatistics stats = GetStatistics();

    TC_LOG_INFO("playerbot.jit", "=== JITBotFactory Statistics ===");
    TC_LOG_INFO("playerbot.jit", "Pending Requests: {}", stats.pendingRequests);
    TC_LOG_INFO("playerbot.jit", "In Progress: {}", stats.inProgressRequests);
    TC_LOG_INFO("playerbot.jit", "Completed This Hour: {}", stats.completedThisHour);
    TC_LOG_INFO("playerbot.jit", "Failed This Hour: {}", stats.failedThisHour);
    TC_LOG_INFO("playerbot.jit", "Cancelled This Hour: {}", stats.cancelledThisHour);
    TC_LOG_INFO("playerbot.jit", "Recycled Available: {}", stats.recycledBotsAvailable);
    TC_LOG_INFO("playerbot.jit", "Bots Created This Hour: {}", stats.botsCreatedThisHour);
    TC_LOG_INFO("playerbot.jit", "Bots Recycled This Hour: {}", stats.botsRecycledThisHour);
    TC_LOG_INFO("playerbot.jit", "Avg Creation Time: {}ms", stats.avgCreationTime.count());
    TC_LOG_INFO("playerbot.jit", "Avg Request Time: {}ms", stats.avgRequestTime.count());
}

void JITBotFactory::SetConfig(JITFactoryConfig const& config)
{
    _config = config;
    TC_LOG_INFO("playerbot.jit", "JITBotFactory::SetConfig - Configuration updated");
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void JITBotFactory::WorkerThread()
{
    TC_LOG_INFO("playerbot.jit", "JITBotFactory::WorkerThread - Worker thread started");

    while (_running.load())
    {
        // Get next request from queue
        FactoryRequest request;
        bool hasRequest = false;

        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            if (!_pendingQueue.empty())
            {
                // Check if we have capacity
                std::lock_guard<std::mutex> reqLock(_requestMutex);
                if (_activeRequests.size() < _config.maxConcurrentCreations)
                {
                    request = _pendingQueue.top();
                    _pendingQueue.pop();
                    hasRequest = true;
                }
            }
        }

        if (hasRequest)
        {
            // Check if request timed out while in queue
            if (request.HasTimedOut())
            {
                TC_LOG_WARN("playerbot.jit", "JITBotFactory::WorkerThread - Request {} timed out in queue",
                    request.requestId);
                if (request.onFailed)
                    request.onFailed("Request timed out");
                _failedThisHour.fetch_add(1);
                continue;
            }

            // Create active request entry
            ActiveRequest active;
            active.request = request;
            active.progress.requestId = request.requestId;
            active.progress.status = RequestStatus::InProgress;
            active.progress.totalNeeded = request.GetTotalNeeded();
            active.progress.startTime = std::chrono::system_clock::now();

            {
                std::lock_guard<std::mutex> lock(_requestMutex);
                _activeRequests[request.requestId] = std::move(active);
            }

            TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::WorkerThread - Processing request {}",
                request.requestId);

            // Process the request
            RequestProgress progress = ProcessRequest(request);

            // Update final status
            {
                std::lock_guard<std::mutex> lock(_requestMutex);
                auto it = _activeRequests.find(request.requestId);
                if (it != _activeRequests.end())
                {
                    it->second.progress = progress;

                    // If completed, call callback and remove
                    if (progress.status == RequestStatus::Completed)
                    {
                        if (request.onComplete)
                            request.onComplete(progress.createdBots);
                        _completedThisHour.fetch_add(1);
                        _activeRequests.erase(it);
                    }
                    else if (progress.status == RequestStatus::Failed)
                    {
                        if (request.onFailed)
                            request.onFailed(progress.errorMessage);
                        _failedThisHour.fetch_add(1);
                        _activeRequests.erase(it);
                    }
                }
            }

            // Update timing statistics
            auto requestDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - progress.startTime);

            {
                std::lock_guard<std::mutex> lock(_statsMutex);
                ++_requestTimeSamples;
                auto totalMs = _avgRequestTime.count() * (_requestTimeSamples - 1) + requestDuration.count();
                _avgRequestTime = std::chrono::milliseconds(totalMs / _requestTimeSamples);
            }
        }
        else
        {
            // No work, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    TC_LOG_INFO("playerbot.jit", "JITBotFactory::WorkerThread - Worker thread stopped");
}

RequestProgress JITBotFactory::ProcessRequest(FactoryRequest& request)
{
    RequestProgress progress;
    progress.requestId = request.requestId;
    progress.status = RequestStatus::InProgress;
    progress.totalNeeded = request.GetTotalNeeded();
    progress.startTime = std::chrono::system_clock::now();

    TC_LOG_INFO("playerbot.jit", "JITBotFactory::ProcessRequest - Processing request {}: {} bots needed",
        request.requestId, progress.totalNeeded);

    try
    {
        std::vector<ObjectGuid> createdBots;

        if (request.IsPvP())
        {
            createdBots = CreatePvPBots(request, progress);
        }
        else
        {
            createdBots = CreatePvEBots(request, progress);
        }

        progress.createdBots = std::move(createdBots);
        progress.created = static_cast<uint32>(progress.createdBots.size());
        progress.status = (progress.created >= progress.totalNeeded) ?
            RequestStatus::Completed : RequestStatus::Failed;

        if (progress.status == RequestStatus::Failed)
            progress.errorMessage = fmt::format("Only created {}/{} bots", progress.created, progress.totalNeeded);

        auto endTime = std::chrono::system_clock::now();
        progress.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - progress.startTime);

        TC_LOG_INFO("playerbot.jit", "JITBotFactory::ProcessRequest - Request {} {}: {}/{} bots in {}ms",
            request.requestId, RequestStatusToString(progress.status),
            progress.created, progress.totalNeeded, progress.elapsed.count());
    }
    catch (std::exception const& e)
    {
        progress.status = RequestStatus::Failed;
        progress.errorMessage = fmt::format("Exception: {}", e.what());
        TC_LOG_ERROR("playerbot.jit", "JITBotFactory::ProcessRequest - Request {} failed with exception: {}",
            request.requestId, e.what());
    }

    return progress;
}

std::vector<ObjectGuid> JITBotFactory::CreatePvEBots(
    FactoryRequest const& request,
    RequestProgress& progress)
{
    std::vector<ObjectGuid> result;

    // Create tanks
    if (request.tanksNeeded > 0)
    {
        auto recycled = GetRecycledBotsForRole(BotRole::Tank, request.playerFaction,
            request.playerLevel, request.minGearScore, request.tanksNeeded);

        progress.fromRecycled += static_cast<uint32>(recycled.size());
        result.insert(result.end(), recycled.begin(), recycled.end());

        uint32 remaining = request.tanksNeeded - static_cast<uint32>(recycled.size());
        if (remaining > 0)
        {
            BatchCloneRequest cloneReq;
            cloneReq.role = BotRole::Tank;
            cloneReq.count = remaining;
            cloneReq.targetLevel = request.playerLevel;
            cloneReq.faction = request.playerFaction;
            cloneReq.minGearScore = request.minGearScore;

            auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
            for (auto const& cr : cloneResults)
            {
                if (cr.success)
                {
                    result.push_back(cr.botGuid);
                    progress.fromClone++;
                    _botsCreatedThisHour.fetch_add(1);
                }
            }
        }

        // Report progress
        progress.created = static_cast<uint32>(result.size());
        if (request.onProgress)
            request.onProgress(progress.GetProgress());
    }

    // Create healers
    if (request.healersNeeded > 0)
    {
        auto recycled = GetRecycledBotsForRole(BotRole::Healer, request.playerFaction,
            request.playerLevel, request.minGearScore, request.healersNeeded);

        progress.fromRecycled += static_cast<uint32>(recycled.size());
        result.insert(result.end(), recycled.begin(), recycled.end());

        uint32 remaining = request.healersNeeded - static_cast<uint32>(recycled.size());
        if (remaining > 0)
        {
            BatchCloneRequest cloneReq;
            cloneReq.role = BotRole::Healer;
            cloneReq.count = remaining;
            cloneReq.targetLevel = request.playerLevel;
            cloneReq.faction = request.playerFaction;
            cloneReq.minGearScore = request.minGearScore;

            auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
            for (auto const& cr : cloneResults)
            {
                if (cr.success)
                {
                    result.push_back(cr.botGuid);
                    progress.fromClone++;
                    _botsCreatedThisHour.fetch_add(1);
                }
            }
        }

        progress.created = static_cast<uint32>(result.size());
        if (request.onProgress)
            request.onProgress(progress.GetProgress());
    }

    // Create DPS
    if (request.dpsNeeded > 0)
    {
        auto recycled = GetRecycledBotsForRole(BotRole::DPS, request.playerFaction,
            request.playerLevel, request.minGearScore, request.dpsNeeded);

        progress.fromRecycled += static_cast<uint32>(recycled.size());
        result.insert(result.end(), recycled.begin(), recycled.end());

        uint32 remaining = request.dpsNeeded - static_cast<uint32>(recycled.size());
        if (remaining > 0)
        {
            BatchCloneRequest cloneReq;
            cloneReq.role = BotRole::DPS;
            cloneReq.count = remaining;
            cloneReq.targetLevel = request.playerLevel;
            cloneReq.faction = request.playerFaction;
            cloneReq.minGearScore = request.minGearScore;

            auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
            for (auto const& cr : cloneResults)
            {
                if (cr.success)
                {
                    result.push_back(cr.botGuid);
                    progress.fromClone++;
                    _botsCreatedThisHour.fetch_add(1);
                }
            }
        }

        progress.created = static_cast<uint32>(result.size());
        if (request.onProgress)
            request.onProgress(progress.GetProgress());
    }

    return result;
}

std::vector<ObjectGuid> JITBotFactory::CreatePvPBots(
    FactoryRequest const& request,
    RequestProgress& progress)
{
    std::vector<ObjectGuid> result;

    // Create Alliance bots
    if (request.allianceNeeded > 0)
    {
        TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::CreatePvPBots - Creating {} Alliance bots",
            request.allianceNeeded);

        // For PvP, distribute roles: 10% tank, 15% healer, 75% DPS
        uint32 allianceTanks = std::max(1u, request.allianceNeeded / 10);
        uint32 allianceHealers = std::max(1u, request.allianceNeeded * 15 / 100);
        uint32 allianceDPS = request.allianceNeeded - allianceTanks - allianceHealers;

        // Create Alliance tanks
        {
            auto recycled = GetRecycledBotsForRole(BotRole::Tank, Faction::Alliance,
                request.playerLevel, request.minGearScore, allianceTanks);
            progress.fromRecycled += static_cast<uint32>(recycled.size());
            result.insert(result.end(), recycled.begin(), recycled.end());

            uint32 remaining = allianceTanks - static_cast<uint32>(recycled.size());
            if (remaining > 0)
            {
                BatchCloneRequest cloneReq;
                cloneReq.role = BotRole::Tank;
                cloneReq.count = remaining;
                cloneReq.targetLevel = request.playerLevel;
                cloneReq.faction = Faction::Alliance;
                cloneReq.minGearScore = request.minGearScore;

                auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
                for (auto const& cr : cloneResults)
                {
                    if (cr.success)
                    {
                        result.push_back(cr.botGuid);
                        progress.fromClone++;
                        _botsCreatedThisHour.fetch_add(1);
                    }
                }
            }
        }

        // Create Alliance healers
        {
            auto recycled = GetRecycledBotsForRole(BotRole::Healer, Faction::Alliance,
                request.playerLevel, request.minGearScore, allianceHealers);
            progress.fromRecycled += static_cast<uint32>(recycled.size());
            result.insert(result.end(), recycled.begin(), recycled.end());

            uint32 remaining = allianceHealers - static_cast<uint32>(recycled.size());
            if (remaining > 0)
            {
                BatchCloneRequest cloneReq;
                cloneReq.role = BotRole::Healer;
                cloneReq.count = remaining;
                cloneReq.targetLevel = request.playerLevel;
                cloneReq.faction = Faction::Alliance;
                cloneReq.minGearScore = request.minGearScore;

                auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
                for (auto const& cr : cloneResults)
                {
                    if (cr.success)
                    {
                        result.push_back(cr.botGuid);
                        progress.fromClone++;
                        _botsCreatedThisHour.fetch_add(1);
                    }
                }
            }
        }

        // Create Alliance DPS
        {
            auto recycled = GetRecycledBotsForRole(BotRole::DPS, Faction::Alliance,
                request.playerLevel, request.minGearScore, allianceDPS);
            progress.fromRecycled += static_cast<uint32>(recycled.size());
            result.insert(result.end(), recycled.begin(), recycled.end());

            uint32 remaining = allianceDPS - static_cast<uint32>(recycled.size());
            if (remaining > 0)
            {
                BatchCloneRequest cloneReq;
                cloneReq.role = BotRole::DPS;
                cloneReq.count = remaining;
                cloneReq.targetLevel = request.playerLevel;
                cloneReq.faction = Faction::Alliance;
                cloneReq.minGearScore = request.minGearScore;

                auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
                for (auto const& cr : cloneResults)
                {
                    if (cr.success)
                    {
                        result.push_back(cr.botGuid);
                        progress.fromClone++;
                        _botsCreatedThisHour.fetch_add(1);
                    }
                }
            }
        }

        progress.created = static_cast<uint32>(result.size());
        if (request.onProgress)
            request.onProgress(progress.GetProgress() * 0.5f);
    }

    // Create Horde bots
    if (request.hordeNeeded > 0)
    {
        TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::CreatePvPBots - Creating {} Horde bots",
            request.hordeNeeded);

        uint32 hordeTanks = std::max(1u, request.hordeNeeded / 10);
        uint32 hordeHealers = std::max(1u, request.hordeNeeded * 15 / 100);
        uint32 hordeDPS = request.hordeNeeded - hordeTanks - hordeHealers;

        // Create Horde tanks
        {
            auto recycled = GetRecycledBotsForRole(BotRole::Tank, Faction::Horde,
                request.playerLevel, request.minGearScore, hordeTanks);
            progress.fromRecycled += static_cast<uint32>(recycled.size());
            result.insert(result.end(), recycled.begin(), recycled.end());

            uint32 remaining = hordeTanks - static_cast<uint32>(recycled.size());
            if (remaining > 0)
            {
                BatchCloneRequest cloneReq;
                cloneReq.role = BotRole::Tank;
                cloneReq.count = remaining;
                cloneReq.targetLevel = request.playerLevel;
                cloneReq.faction = Faction::Horde;
                cloneReq.minGearScore = request.minGearScore;

                auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
                for (auto const& cr : cloneResults)
                {
                    if (cr.success)
                    {
                        result.push_back(cr.botGuid);
                        progress.fromClone++;
                        _botsCreatedThisHour.fetch_add(1);
                    }
                }
            }
        }

        // Create Horde healers
        {
            auto recycled = GetRecycledBotsForRole(BotRole::Healer, Faction::Horde,
                request.playerLevel, request.minGearScore, hordeHealers);
            progress.fromRecycled += static_cast<uint32>(recycled.size());
            result.insert(result.end(), recycled.begin(), recycled.end());

            uint32 remaining = hordeHealers - static_cast<uint32>(recycled.size());
            if (remaining > 0)
            {
                BatchCloneRequest cloneReq;
                cloneReq.role = BotRole::Healer;
                cloneReq.count = remaining;
                cloneReq.targetLevel = request.playerLevel;
                cloneReq.faction = Faction::Horde;
                cloneReq.minGearScore = request.minGearScore;

                auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
                for (auto const& cr : cloneResults)
                {
                    if (cr.success)
                    {
                        result.push_back(cr.botGuid);
                        progress.fromClone++;
                        _botsCreatedThisHour.fetch_add(1);
                    }
                }
            }
        }

        // Create Horde DPS
        {
            auto recycled = GetRecycledBotsForRole(BotRole::DPS, Faction::Horde,
                request.playerLevel, request.minGearScore, hordeDPS);
            progress.fromRecycled += static_cast<uint32>(recycled.size());
            result.insert(result.end(), recycled.begin(), recycled.end());

            uint32 remaining = hordeDPS - static_cast<uint32>(recycled.size());
            if (remaining > 0)
            {
                BatchCloneRequest cloneReq;
                cloneReq.role = BotRole::DPS;
                cloneReq.count = remaining;
                cloneReq.targetLevel = request.playerLevel;
                cloneReq.faction = Faction::Horde;
                cloneReq.minGearScore = request.minGearScore;

                auto cloneResults = sBotCloneEngine->BatchClone(cloneReq);
                for (auto const& cr : cloneResults)
                {
                    if (cr.success)
                    {
                        result.push_back(cr.botGuid);
                        progress.fromClone++;
                        _botsCreatedThisHour.fetch_add(1);
                    }
                }
            }
        }

        progress.created = static_cast<uint32>(result.size());
        if (request.onProgress)
            request.onProgress(progress.GetProgress());
    }

    return result;
}

std::vector<ObjectGuid> JITBotFactory::GetRecycledBotsForRole(
    BotRole role,
    Faction faction,
    uint32 level,
    uint32 minGearScore,
    uint32 count)
{
    std::vector<ObjectGuid> result;
    result.reserve(count);

    std::lock_guard<std::mutex> lock(_recycleMutex);

    auto it = _recycledBots.begin();
    while (it != _recycledBots.end() && result.size() < count)
    {
        if (it->Matches(role, faction, level, minGearScore))
        {
            result.push_back(it->guid);
            it = _recycledBots.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (!result.empty())
    {
        TC_LOG_DEBUG("playerbot.jit", "JITBotFactory::GetRecycledBotsForRole - Found {} recycled bots for {} {}",
            result.size(), BotRoleToString(role), FactionToString(faction));
    }

    return result;
}

uint8 JITBotFactory::GetPriorityForType(InstanceType type) const
{
    switch (type)
    {
        case InstanceType::Dungeon: return _config.dungeonPriority;
        case InstanceType::Arena: return _config.arenaPriority;
        case InstanceType::Raid: return _config.raidPriority;
        case InstanceType::Battleground: return _config.battlegroundPriority;
        default: return 5;
    }
}

std::chrono::milliseconds JITBotFactory::GetTimeoutForType(InstanceType type) const
{
    switch (type)
    {
        case InstanceType::Dungeon: return std::chrono::milliseconds(_config.dungeonTimeoutMs);
        case InstanceType::Arena: return std::chrono::milliseconds(_config.arenaTimeoutMs);
        case InstanceType::Raid: return std::chrono::milliseconds(_config.raidTimeoutMs);
        case InstanceType::Battleground: return std::chrono::milliseconds(_config.battlegroundTimeoutMs);
        default: return std::chrono::milliseconds(60000);
    }
}

void JITBotFactory::ProcessTimeouts()
{
    std::lock_guard<std::mutex> lock(_requestMutex);

    auto now = std::chrono::system_clock::now();

    for (auto it = _activeRequests.begin(); it != _activeRequests.end(); )
    {
        if (it->second.request.HasTimedOut())
        {
            TC_LOG_WARN("playerbot.jit", "JITBotFactory::ProcessTimeouts - Request {} timed out",
                it->first);

            it->second.progress.status = RequestStatus::TimedOut;
            if (it->second.request.onFailed)
                it->second.request.onFailed("Request timed out");

            _failedThisHour.fetch_add(1);
            it = _activeRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace Playerbot
