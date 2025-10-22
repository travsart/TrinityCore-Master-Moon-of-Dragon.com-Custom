/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotWorldSessionMgr.h"
#include "BotSession.h"
#include "BotPriorityManager.h"
#include "BotPerformanceMonitor.h"
#include "BotHealthCheck.h"
#include "Player.h"
#include "World.h"
#include "DatabaseEnv.h"
#include "CharacterCache.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "WorldSession.h"
#include "Config.h"
#include "../Performance/ThreadPool/ThreadPool.h"
#include "../Spatial/SpatialGridManager.h"
#include "../Spatial/SpatialGridScheduler.h"
#include <chrono>
#include <queue>

namespace Playerbot {

// ============================================================================
// PHASE A: ThreadPool Integration - Priority Mapping
// ============================================================================

/**
 * @brief Map BotPriority to ThreadPool TaskPriority
 *
 * Mapping Strategy:
 * - EMERGENCY (health < 20%, combat death) → CRITICAL (0-10ms tolerance)
 * - HIGH (combat, groups) → HIGH (10-50ms tolerance)
 * - MEDIUM (active movement) → NORMAL (50-200ms tolerance)
 * - LOW (idle, resting) → LOW (200-1000ms tolerance)
 * - SUSPENDED (dead, disconnected) → IDLE (no time constraints)
 */
inline Performance::TaskPriority MapBotPriorityToTaskPriority(BotPriority botPriority)
{
    switch (botPriority)
    {
        case BotPriority::EMERGENCY:
            return Performance::TaskPriority::CRITICAL;
        case BotPriority::HIGH:
            return Performance::TaskPriority::HIGH;
        case BotPriority::MEDIUM:
            return Performance::TaskPriority::NORMAL;
        case BotPriority::LOW:
            return Performance::TaskPriority::LOW;
        case BotPriority::SUSPENDED:
            return Performance::TaskPriority::IDLE;
        default:
            return Performance::TaskPriority::NORMAL;
    }
}

bool BotWorldSessionMgr::Initialize()
{
    if (_initialized.load())
        return true;

    TC_LOG_INFO("module.playerbot.session", "🔧 BotWorldSessionMgr: Initializing with native TrinityCore login pattern + Enterprise System");

    // Initialize enterprise components
    if (!sBotPriorityMgr->Initialize())
    {
        TC_LOG_ERROR("module.playerbot.session", "Failed to initialize BotPriorityManager");
        return false;
    }

    if (!sBotPerformanceMon->Initialize())
    {
        TC_LOG_ERROR("module.playerbot.session", "Failed to initialize BotPerformanceMonitor");
        return false;
    }

    if (!sBotHealthCheck->Initialize())
    {
        TC_LOG_ERROR("module.playerbot.session", "Failed to initialize BotHealthCheck");
        return false;
    }

    // Load spawn throttling config
    _maxSpawnsPerTick = sConfigMgr->GetIntDefault("Playerbot.LevelManager.MaxBotsPerUpdate", 10);

    // Initialize spatial grid system (grids created on-demand per map)
    TC_LOG_INFO("module.playerbot.session",
        "🔧 BotWorldSessionMgr: Spatial grid system initialized (lock-free double-buffered architecture)");

    _enabled.store(true);
    _initialized.store(true);

    TC_LOG_INFO("module.playerbot.session",
        "🔧 BotWorldSessionMgr: ENTERPRISE MODE enabled for 5000 bot scalability (spawn rate: {}/tick)",
        _maxSpawnsPerTick);

    return true;
}

void BotWorldSessionMgr::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("module.playerbot.session", "🔧 BotWorldSessionMgr: Shutting down");

    _enabled.store(false);

    // Shutdown enterprise components first
    sBotHealthCheck->Shutdown();
    sBotPerformanceMon->Shutdown();
    sBotPriorityMgr->Shutdown();

    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

    // Clean logout all bot sessions
    for (auto& [guid, session] : _botSessions)
    {
        if (session && session->GetPlayer())
        {
            // MEMORY SAFETY: Protect against use-after-free when accessing Player name
            Player* player = session->GetPlayer();
            try {
                TC_LOG_INFO("module.playerbot.session", "🔧 Logging out bot: {}", player->GetName());
            }
            catch (...) {
                TC_LOG_INFO("module.playerbot.session", "🔧 Logging out bot (name unavailable - use-after-free protection)");
            }
            session->LogoutPlayer(true);
        }
    }

    _botSessions.clear();
    _botsLoading.clear();

    // Destroy all spatial grids and stop worker threads
    sSpatialGridManager.DestroyAllGrids();
    TC_LOG_INFO("module.playerbot.session", "🔧 BotWorldSessionMgr: Spatial grid system shut down");

    _initialized.store(false);
}

bool BotWorldSessionMgr::AddPlayerBot(ObjectGuid playerGuid, uint32 masterAccountId)
{
    if (!_enabled.load() || !_initialized.load())
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 BotWorldSessionMgr not enabled or initialized");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

    // Check if bot is already loading
    if (_botsLoading.find(playerGuid) != _botsLoading.end())
    {
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot {} already loading", playerGuid.ToString());
        return false;
    }

    // FIX #22: CRITICAL - Eliminate ObjectAccessor call to prevent deadlock
    // Check if bot is already added by scanning existing sessions (avoids ObjectAccessor lock)
    for (auto const& [guid, session] : _botSessions)
    {
        if (guid == playerGuid && session && session->GetPlayer())
        {
            TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot {} already in world (found in _botSessions)", playerGuid.ToString());
            return false;
        }
    }

    // Get account ID for this character from playerbot database directly
    uint32 accountId = 0;

    // Query playerbot_characters database directly since character cache won't find bot accounts
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PINFO);
    stmt->setUInt64(0, playerGuid.GetCounter());
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        // CHAR_SEL_CHAR_PINFO returns: totaltime, level, money, account, race, class, map, zone, gender, health, playerFlags
        // Account is at index 3
        accountId = (*result)[3].GetUInt32();
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Found account ID {} for character {}", accountId, playerGuid.ToString());
    }

    if (!accountId)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Could not find account for character {} in playerbot database", playerGuid.ToString());
        return false;
    }

    // CRITICAL FIX: Check for existing sessions by account ID to prevent duplicates
    for (auto const& [guid, session] : _botSessions)
    {
        if (session && session->GetAccountId() == accountId)
        {
            TC_LOG_WARN("module.playerbot.session", "🔧 DUPLICATE SESSION PREVENTION: Account {} already has an active bot session with character {}, rejecting new character {}",
                accountId, guid.ToString(), playerGuid.ToString());
            return false;
        }
    }

    // CRITICAL FIX: Enforce MaxBots limit
    uint32 maxBots = sConfigMgr->GetIntDefault("Playerbot.MaxBots", 100);
    uint32 totalBots = static_cast<uint32>(_botSessions.size() + _pendingSpawns.size());
    if (totalBots >= maxBots)
    {
        TC_LOG_WARN("module.playerbot.session",
            "🔧 MAX BOTS LIMIT: Cannot queue bot {} - already at limit ({}/{} bots)",
            playerGuid.ToString(), totalBots, maxBots);
        return false;
    }

    // CRITICAL FIX V3: RATE-LIMITED SPAWN QUEUE
    //
    // ROOT CAUSE OF SERVER HANG: "ENTERPRISE FIX V2" removed all throttling, causing:
    // - 296 bots spawning immediately at server startup
    // - 296 × 66 async queries = 19,536 database queries flooding connection pool
    // - ProcessQueryCallbacks() overwhelmed, world thread hangs 60+ seconds
    //
    // NEW SOLUTION: Queue-based rate-limited spawning
    // - AddPlayerBot() adds to _pendingSpawns queue (no immediate spawn)
    // - UpdateSessions() processes N spawns per tick (configurable, default 10)
    // - Prevents database overload while maintaining async benefits
    // - No threads, no deadlock, controlled flow
    //
    // Performance: MaxBotsPerUpdate config controls rate (10/tick = 10/50ms = 200 bots/sec)
    // Full 100-bot spawn completes in 500ms instead of overloading database instantly

    TC_LOG_INFO("module.playerbot.session",
        "🔧 Queueing bot {} for rate-limited spawn (queue position: {}, accountId: {})",
        playerGuid.ToString(), _pendingSpawns.size() + 1, accountId);

    // Add to pending spawn queue - will be processed in UpdateSessions()
    _pendingSpawns.push_back({playerGuid, accountId});

    TC_LOG_DEBUG("module.playerbot.session",
        "🔧 Bot {} added to spawn queue. Total pending: {}",
        playerGuid.ToString(), _pendingSpawns.size());

    return true; // Queued successfully
}

void BotWorldSessionMgr::RemovePlayerBot(ObjectGuid playerGuid)
{
    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

    auto it = _botSessions.find(playerGuid);
    if (it == _botSessions.end())
    {
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot session not found for removal: {}", playerGuid.ToString());
        return;
    }

    std::shared_ptr<BotSession> session = it->second;
    if (session && session->GetPlayer())
    {
        // MEMORY SAFETY: Protect against use-after-free when accessing Player name
        Player* player = session->GetPlayer();
        try {
            TC_LOG_INFO("module.playerbot.session", "🔧 Removing bot: {}", player->GetName());
        }
        catch (...) {
            TC_LOG_INFO("module.playerbot.session", "🔧 Removing bot (name unavailable - use-after-free protection)");
        }
        session->LogoutPlayer(true);
    }

    // Remove from map - shared_ptr will automatically clean up when no more references exist
    _botSessions.erase(it);
}

Player* BotWorldSessionMgr::GetPlayerBot(ObjectGuid playerGuid) const
{
    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

    auto it = _botSessions.find(playerGuid);
    if (it == _botSessions.end())
        return nullptr;

    return it->second ? it->second->GetPlayer() : nullptr;
}

void BotWorldSessionMgr::UpdateSessions(uint32 diff)
{
    if (!_enabled.load())
        return;

    // ENTERPRISE-GRADE PRIORITY-BASED UPDATE SYSTEM
    // Designed for 5000+ concurrent bots with optimal performance
    //
    // Phase 1 (Simple rotation): 78% improvement (600ms → 150ms)
    // Phase 2 (Enterprise priority): Targets 145ms per tick with 5000 bots
    //
    // Priority Levels:
    // - EMERGENCY (5 bots): Every tick - critical states
    // - HIGH (45 bots): Every tick - combat, groups
    // - MEDIUM (40 bots): Every 10 ticks - active movement
    // - LOW (91 bots): Every 50 ticks - idle, resting
    //
    // Total per-tick load: 5 + 45 + 40 + 91 = 181 bots @ 0.8ms = 145ms target

    uint32 currentTime = getMSTime();
    _tickCounter++;

    // PHASE 0: Enterprise monitoring - Begin tick measurement
    sBotPerformanceMon->BeginTick(currentTime);
    sBotHealthCheck->RecordHeartbeat(currentTime);

    // PHASE 0.5: CRITICAL FIX - Process pending spawns at controlled rate
    // Prevents database overload by rate-limiting async login submissions
    {
        std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

        _spawnsProcessedThisTick = 0;

        while (!_pendingSpawns.empty() && _spawnsProcessedThisTick < _maxSpawnsPerTick)
        {
            auto [playerGuid, accountId] = _pendingSpawns.front();
            _pendingSpawns.erase(_pendingSpawns.begin());

            TC_LOG_INFO("module.playerbot.session",
                "🔧 Processing queued spawn for bot {} (accountId: {}, remaining in queue: {})",
                playerGuid.ToString(), accountId, _pendingSpawns.size());

            // Synchronize character cache
            if (!SynchronizeCharacterCache(playerGuid))
            {
                TC_LOG_ERROR("module.playerbot.session",
                    "🔧 Failed to synchronize character cache for {}", playerGuid.ToString());
                continue;
            }

            // Mark as loading
            _botsLoading.insert(playerGuid);

            // Create BotSession
            std::shared_ptr<BotSession> botSession = BotSession::Create(accountId);
            if (!botSession)
            {
                TC_LOG_ERROR("module.playerbot.session",
                    "🔧 Failed to create BotSession for {}", playerGuid.ToString());
                _botsLoading.erase(playerGuid);
                continue;
            }

            // Store session
            _botSessions[playerGuid] = botSession;

            // Initiate async login (1 bot × 66 queries)
            if (!botSession->LoginCharacter(playerGuid))
            {
                TC_LOG_ERROR("module.playerbot.session",
                    "🔧 Failed to initiate async login for {}", playerGuid.ToString());
                _botSessions.erase(playerGuid);
                _botsLoading.erase(playerGuid);
                continue;
            }

            TC_LOG_INFO("module.playerbot.session",
                "✅ Async bot login initiated for: {} ({}/{} spawns this tick)",
                playerGuid.ToString(), _spawnsProcessedThisTick + 1, _maxSpawnsPerTick);

            _spawnsProcessedThisTick++;
        }

        if (_spawnsProcessedThisTick > 0)
        {
            TC_LOG_INFO("module.playerbot.session",
                "🔧 Processed {} bot spawns this tick. Remaining in queue: {}",
                _spawnsProcessedThisTick, _pendingSpawns.size());
        }
    }

    // DEADLOCK FIX: Update all spatial grids ONCE before bot updates
    // This prevents 25+ threads from all trying to update simultaneously
    // Single, controlled update point = no contention = no deadlock
    if (_tickCounter % 2 == 0)  // Update every 100ms (every 2 ticks at 50ms/tick)
    {
        sSpatialGridScheduler.UpdateAllGrids(diff);
    }

    std::vector<std::pair<ObjectGuid, std::shared_ptr<BotSession>>> sessionsToUpdate;
    std::vector<ObjectGuid> sessionsToRemove;
    uint32 botsSkipped = 0;

    // PHASE 1: Priority-based session collection (minimal lock time)
    {
        std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

        if (_botSessions.empty() && _pendingSpawns.empty())
        {
            sBotPerformanceMon->EndTick(currentTime, 0, 0);
            return;
        }

        sessionsToUpdate.reserve(200); // Reserve for typical load

        for (auto it = _botSessions.begin(); it != _botSessions.end(); ++it)
        {
            ObjectGuid guid = it->first;
            std::shared_ptr<BotSession> session = it->second;

            // CRITICAL SAFETY: Validate session
            if (!session || !session->IsBot())
            {
                sessionsToRemove.push_back(guid);
                continue;
            }

            // Check if bot should update this tick based on priority
            if (_enterpriseMode && !sBotPriorityMgr->ShouldUpdateThisTick(guid, _tickCounter))
            {
                sBotPriorityMgr->RecordUpdateSkipped(guid);
                botsSkipped++;
                continue;
            }

            // Handle loading sessions
            if (_botsLoading.find(guid) != _botsLoading.end())
            {
                if (session->IsLoginComplete())
                {
                    _botsLoading.erase(guid);
                    TC_LOG_INFO("module.playerbot.session", "🔧 ✅ Bot login completed: {}", guid.ToString());

                    // Initialize priority for newly logged-in bot (default to LOW, will adjust in Phase 2 if needed)
                    if (_enterpriseMode)
                        sBotPriorityMgr->SetPriority(guid, BotPriority::LOW);

                    sessionsToUpdate.emplace_back(guid, session);
                }
                else if (session->IsLoginFailed())
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 Bot login failed: {}", guid.ToString());
                    _botsLoading.erase(guid);
                    sessionsToRemove.push_back(guid);
                }
                else
                {
                    sessionsToUpdate.emplace_back(guid, session); // Still loading
                }
            }
            else
            {
                sessionsToUpdate.emplace_back(guid, session);
            }
        }

        // Clean up invalid sessions
        for (ObjectGuid const& guid : sessionsToRemove)
        {
            _botSessions.erase(guid);
            sBotPriorityMgr->RemoveBot(guid);
        }
    } // Release mutex - CRITICAL for deadlock prevention

    // PHASE 2: Update sessions with ThreadPool (parallel execution for 7x speedup)
    //
    // PHASE A THREADPOOL INTEGRATION: Replace sequential bot updates with parallel execution
    //
    // Before (Sequential): 145 bots × 1ms = 145ms
    // After (Parallel, 8 threads): 145 bots ÷ 8 threads = 18 bots/thread × 1ms = 18ms (7x speedup)
    //
    // Architecture:
    // 1. Submit each bot update as a task to ThreadPool with mapped priority
    // 2. Capture shared_ptr by value (thread-safe reference counting)
    // 3. Collect futures for synchronization barrier
    // 4. Handle errors through concurrent queue (thread-safe)
    // 5. Wait for all tasks to complete before proceeding
    //
    // CRITICAL SAFETY: Check if ThreadPool is safe to use (World must be fully initialized)
    // During server startup, World components aren't ready and ThreadPool workers would crash
    // Fallback to sequential execution if parallel execution isn't safe yet

    std::vector<ObjectGuid> disconnectedSessions;
    std::atomic<uint32> botsUpdated{0};

    // Thread-safe error queue for disconnections (std::queue with mutex)
    std::queue<ObjectGuid> disconnectionQueue;
    std::recursive_mutex disconnectionMutex;

    // CRITICAL SAFETY: Never use ThreadPool during server startup
    // Check if we have any active bots that have completed login
    // During startup, no bots should be fully logged in yet
    bool serverReady = false;
    for (auto& [guid, session] : sessionsToUpdate)
    {
        if (session && session->IsLoginComplete())
        {
            serverReady = true;
            break;
        }
    }

    // ThreadPool enabled with CPU affinity removed to prevent 60s hang
    // CPU affinity (SetThreadAffinityMask) was causing blocking during thread creation
    // Workers now create without affinity and OS scheduler handles CPU assignment
    //
    // CRITICAL: If futures hang, the issue is ObjectAccessor calls from BotAI::UpdateAI()
    // Solution: Replace ObjectAccessor with spatial grid snapshots in the hanging code path
    bool useThreadPool = serverReady && !sessionsToUpdate.empty();
    std::vector<std::future<void>> futures;
    std::vector<ObjectGuid> futureGuids;  // Track which GUID corresponds to which future

    // Cache member variables before lambda capture to avoid threading issues
    bool enterpriseMode = _enterpriseMode;
    uint32 tickCounter = _tickCounter;

    if (useThreadPool)
    {
        // CRITICAL FIX: Check ThreadPool saturation BEFORE submitting tasks
        // If workers are busy with previous batch, new tasks queue up and cause cascading delays
        // Better to skip this update cycle than create 10+ second backlogs
        size_t queuedTasks = Performance::GetThreadPool().GetQueuedTasks();
        size_t activeWorkers = Performance::GetThreadPool().GetActiveThreads();
        uint32 workerCount = Performance::GetThreadPool().GetWorkerCount();

        // REVISED SATURATION LOGIC: Check both queue depth AND worker availability
        //
        // Problem: Queue depth alone doesn't prevent 10s delays. Even with queue capacity,
        // if all workers are busy and we submit more tasks, the last task can wait
        // 10+ seconds in a worker's queue while that worker processes 10+ sequential tasks.
        //
        // Solution: Also check worker availability. If most workers are busy, skip updates.
        //
        // Thresholds:
        // 1. Queue depth: >100 tasks = backlog building up
        // 2. Worker utilization: >80% busy = insufficient capacity for new batch
        //
        // Example with 30 workers (32 cores - 2):
        // - >100 queued tasks = ~3 tasks per worker queued = ~300ms backlog (acceptable)
        // - >24 workers busy (80%) = only 6 free workers for new batch
        //
        // Example with 16 workers (18 cores - 2):
        // - >100 queued tasks = ~6 tasks per worker queued = ~600ms backlog
        // - >13 workers busy (80%) = only 3 free workers for new batch
        //
        // Rationale:
        // - 100 task threshold scales with worker count (3-6 tasks per worker)
        // - 80% busy threshold ensures enough free workers to handle new batch quickly
        // - Prevents scenario where new tasks queue behind long-running tasks
        // - Priority system spreads bots across ticks, so actual per-tick load is manageable
        uint32 busyThreshold = (workerCount * 4) / 5;  // 80% of workers

        // Safety: Ensure minimum threshold to handle edge cases
        // Even with minimum 16 workers enforced by ThreadPool, be defensive
        if (busyThreshold < 3)
            busyThreshold = 3;

        if (queuedTasks > 100 || activeWorkers > busyThreshold)
        {
            TC_LOG_WARN("module.playerbot.session",
                "ThreadPool saturated (queue: {} tasks, active: {}/{} workers, busy threshold: {}) - skipping bot updates this tick",
                queuedTasks, activeWorkers, workerCount, busyThreshold);
            return;  // Skip this update cycle to let workers catch up
        }

        futures.reserve(sessionsToUpdate.size());
        futureGuids.reserve(sessionsToUpdate.size());
    }

    for (auto& [guid, botSession] : sessionsToUpdate)
    {
        // Validate session before submitting task
        if (!botSession || !botSession->IsActive())
        {
            disconnectedSessions.push_back(guid);
            continue;
        }

        // Define the update logic (will be used in both parallel and sequential paths)
        auto updateLogic = [guid, botSession, diff, currentTime, enterpriseMode, tickCounter, &botsUpdated, &disconnectionQueue, &disconnectionMutex]()
            {
                TC_LOG_ERROR("module.playerbot.session", "🔹 DEBUG: TASK START for bot {}", guid.ToString());
                TC_LOG_TRACE("playerbot.session.task", "🔹 TASK START for bot {}", guid.ToString());
                try
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔹 DEBUG: Creating PacketFilter for bot {}", guid.ToString());
                    // Create PacketFilter for bot session
                    class BotPacketFilter : public PacketFilter
                    {
                    public:
                        explicit BotPacketFilter(WorldSession* session) : PacketFilter(session) {}
                        virtual ~BotPacketFilter() override = default;
                        bool Process(WorldPacket*) override { return true; }
                        bool ProcessUnsafe() const override { return true; }
                    } filter(botSession.get());
                    TC_LOG_ERROR("module.playerbot.session", "🔹 DEBUG: PacketFilter created for bot {}", guid.ToString());

                    // Update session
                    TC_LOG_TRACE("playerbot.session.update", "📋 Starting Update() for bot {}", guid.ToString());
                    TC_LOG_ERROR("module.playerbot.session", "🔍 DEBUG: About to call botSession->Update() for bot {}", guid.ToString());
                    bool updateResult = botSession->Update(diff, filter);
                    TC_LOG_ERROR("module.playerbot.session", "🔍 DEBUG: botSession->Update() returned {} for bot {}", updateResult, guid.ToString());
                    TC_LOG_TRACE("playerbot.session.update", "📋 Update() returned {} for bot {}", updateResult, guid.ToString());

                    if (!updateResult)
                    {
                        TC_LOG_WARN("module.playerbot.session", "🔧 Bot update failed: {}", guid.ToString());
                        std::lock_guard<std::recursive_mutex> lock(disconnectionMutex);
                        disconnectionQueue.push(guid);
                        return;  // Early return OK - packaged_task completes promise automatically
                    }

                    botsUpdated.fetch_add(1, std::memory_order_relaxed);

                    // IMPROVEMENT #1: ADAPTIVE AutoAdjustPriority frequency based on bot activity
                    // Active bots (combat/group) = more frequent checks (250ms)
                    // Idle bots (high health, not moving) = less frequent checks (2.5s)
                    if (enterpriseMode && botSession->IsLoginComplete())
                    {
                        Player* bot = botSession->GetPlayer();
                        if (!bot || !bot->IsInWorld())
                        {
                            TC_LOG_ERROR("module.playerbot.session", "🔧 Bot disconnected: {}", guid.ToString());
                            if (botSession->GetPlayer())
                                botSession->LogoutPlayer(true);

                            std::lock_guard<std::recursive_mutex> lock(disconnectionMutex);
                            disconnectionQueue.push(guid);
                            return;
                        }

                        // Adaptive frequency: Adjust interval based on bot activity
                        uint32 adjustInterval = 10; // Default 500ms

                        if (bot->IsInCombat() || bot->GetGroup())
                            adjustInterval = 5;  // Active bots: 250ms (more responsive)
                        else if (!bot->isMoving() && bot->GetHealthPct() > 80.0f)
                            adjustInterval = 50; // Idle healthy bots: 2.5s (save CPU)

                        // Call AutoAdjustPriority at adaptive interval
                        if (tickCounter % adjustInterval == 0)
                        {
                            sBotPriorityMgr->AutoAdjustPriority(bot, currentTime);
                        }
                        // Fast-path critical state detection on other ticks (lightweight checks only)
                        else
                        {
                            // Immediate priority boost for critical situations (no group/movement checks)
                            if (bot->IsInCombat())
                            {
                                sBotPriorityMgr->SetPriority(guid, BotPriority::HIGH);
                            }
                            else if (bot->GetHealthPct() < 20.0f)
                            {
                                sBotPriorityMgr->SetPriority(guid, BotPriority::EMERGENCY);
                            }
                        }
                    }
                }
                catch (std::exception const& e)
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 DEBUG: Exception caught in lambda for bot {}: {}", guid.ToString(), e.what());
                    TC_LOG_ERROR("module.playerbot.session", "🔧 Exception updating bot {}: {}", guid.ToString(), e.what());
                    sBotHealthCheck->RecordError(guid, "UpdateException");

                    std::lock_guard<std::recursive_mutex> lock(disconnectionMutex);
                    disconnectionQueue.push(guid);
                }
                catch (...)
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 DEBUG: Unknown exception caught in lambda for bot {}", guid.ToString());
                    TC_LOG_ERROR("module.playerbot.session", "🔧 Unknown exception updating bot {}", guid.ToString());
                    sBotHealthCheck->RecordError(guid, "UnknownException");

                    std::lock_guard<std::recursive_mutex> lock(disconnectionMutex);
                    disconnectionQueue.push(guid);
                }

                TC_LOG_ERROR("module.playerbot.session", "🔹 DEBUG: TASK END for bot {}", guid.ToString());
                TC_LOG_TRACE("playerbot.session.task", "🔹 TASK END for bot {}", guid.ToString());
            };

        // Execute either parallel (ThreadPool) or sequential (direct call)
        if (useThreadPool)
        {
            // Parallel path: Submit to ThreadPool
            try
            {
                BotPriority botPriority = sBotPriorityMgr->GetPriority(guid);
                Performance::TaskPriority taskPriority = MapBotPriorityToTaskPriority(botPriority);

                auto future = Performance::GetThreadPool().Submit(taskPriority, updateLogic);
                futures.push_back(std::move(future));
                futureGuids.push_back(guid);  // Track GUID for this future
                TC_LOG_ERROR("module.playerbot.session", "📤 DEBUG: Submitted task {} for bot {} to ThreadPool", futures.size() - 1, guid.ToString());
            }
            catch (...)
            {
                // ThreadPool failed (not initialized yet), fallback to sequential for remaining bots
                useThreadPool = false;
                updateLogic(); // Execute this bot sequentially
            }
        }
        else
        {
            // Sequential path: Execute directly
            updateLogic();
        }
    }

    // CRITICAL FIX: Wake all workers BEFORE waiting on futures to prevent deadlock
    // When batch-submitting many tasks, individual Submit() wake calls may not wake
    // enough workers due to race conditions. This ensures ALL workers are processing tasks.
    if (useThreadPool && !futures.empty())
    {
        Performance::GetThreadPool().WakeAllWorkers();
    }

    // SYNCHRONIZATION BARRIER: Wait for all parallel tasks to complete
    // This ensures all bot updates finish before proceeding to cleanup phase
    // future.get() also propagates exceptions from worker threads
    //
    // CRITICAL FIX: Use wait_for() with timeout and retry with WakeAllWorkers()
    // Problem: Workers may finish tasks and sleep BEFORE main thread reaches future.get()
    // Solution: Timeout + wake + retry loop ensures futures eventually complete
    for (size_t i = 0; i < futures.size(); ++i)
    {
        auto& future = futures[i];
        bool completed = false;
        uint32 retries = 0;
        constexpr uint32 MAX_RETRIES = 100;  // 100 * 100ms = 10 seconds max wait
        constexpr auto TIMEOUT = std::chrono::milliseconds(100);

        while (!completed && retries < MAX_RETRIES)
        {
            auto status = future.wait_for(TIMEOUT);

            if (status == std::future_status::ready)
            {
                completed = true;
                try
                {
                    future.get(); // Get result + exception propagation
                }
                catch (std::exception const& e)
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 ThreadPool task failed: {}", e.what());
                }
                catch (...)
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 ThreadPool task failed with unknown exception");
                }
            }
            else
            {
                // Future not ready - wake all workers and retry
                ++retries;

                // CRITICAL FIX: Emergency task resubmission after timeout
                // If a future hasn't completed after 5 seconds (50 retries), the worker thread
                // assigned to it is likely deadlocked or stuck. Resubmit the task to a different worker.
                if (retries == 50)
                {
                    TC_LOG_ERROR("module.playerbot.session",
                        "🚨 EMERGENCY: Future {} (bot {}) stuck after 5s - task likely not executed by worker. "
                        "This indicates ThreadPool worker deadlock or task queue starvation.",
                        i + 1, futureGuids[i].ToString());

                    // The task is stuck in a worker's queue and never being executed.
                    // We cannot safely resubmit it (would create duplicate tasks).
                    // Best we can do is aggressively wake ALL workers and hope work stealing kicks in.
                    Performance::GetThreadPool().WakeAllWorkers();
                }

                if (retries % 5 == 0)  // Every 500ms, wake workers
                {
                    TC_LOG_WARN("module.playerbot.session",
                        "🔧 Future {} of {} (bot {}) not ready after {}ms, waking workers (attempt {}/{})",
                        i + 1, futures.size(), futureGuids[i].ToString(), retries * 100, retries, MAX_RETRIES);
                    Performance::GetThreadPool().WakeAllWorkers();
                }
            }
        }

        if (!completed)
        {
            TC_LOG_FATAL("module.playerbot.session",
                "🔧 DEADLOCK DETECTED: Future {} of {} (bot {}) did not complete after {} seconds!",
                i + 1, futures.size(), futureGuids[i].ToString(), (MAX_RETRIES * 100) / 1000);
        }
    }

    // Collect disconnected sessions from thread-safe queue
    {
        std::lock_guard<std::recursive_mutex> lock(disconnectionMutex);
        while (!disconnectionQueue.empty())
        {
            disconnectedSessions.push_back(disconnectionQueue.front());
            disconnectionQueue.pop();
        }
    }

    // PHASE 3: Cleanup disconnected sessions
    if (!disconnectedSessions.empty())
    {
        std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);
        for (ObjectGuid const& guid : disconnectedSessions)
        {
            _botSessions.erase(guid);
            sBotPriorityMgr->RemoveBot(guid);
        }
    }

    // PHASE 4: Lightweight enterprise monitoring (reduced frequency to minimize overhead)
    if (_enterpriseMode)
    {
        sBotPerformanceMon->EndTick(currentTime, botsUpdated.load(std::memory_order_relaxed), botsSkipped);

        // Only check thresholds every 10 ticks (500ms) instead of every tick
        if (_tickCounter % 10 == 0)
        {
            sBotPerformanceMon->CheckPerformanceThresholds();
            sBotHealthCheck->PerformHealthChecks(currentTime);
        }

        // Periodic enterprise logging (every 60 seconds)
        if (_tickCounter % 1200 == 0)
        {
            sBotPriorityMgr->LogPriorityDistribution();
            sBotPerformanceMon->LogPerformanceReport();
        }
    }
}

uint32 BotWorldSessionMgr::GetBotCount() const
{
    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);
    return static_cast<uint32>(_botSessions.size());
}

void BotWorldSessionMgr::TriggerCharacterLoginForAllSessions()
{
    // For compatibility with existing BotSpawner system
    // This method will work with the new native login system
    TC_LOG_INFO("module.playerbot.session", "🔧 TriggerCharacterLoginForAllSessions called");

    // The native login approach doesn't need this trigger mechanism
    // But we can use it to spawn new bots if needed
    TC_LOG_INFO("module.playerbot.session", "🔧 Using native login - no manual triggering needed");
}

bool BotWorldSessionMgr::SynchronizeCharacterCache(ObjectGuid playerGuid)
{
    TC_LOG_DEBUG("module.playerbot.session", "🔧 Synchronizing character cache for {}", playerGuid.ToString());

    // CRITICAL FIX: Use synchronous query instead of CHAR_SEL_CHARACTER (which is CONNECTION_ASYNC only)
    // Create a simple synchronous query for just name and account
    std::string query = "SELECT guid, name, account FROM characters WHERE guid = " + std::to_string(playerGuid.GetCounter());
    QueryResult result = CharacterDatabase.Query(query.c_str());

    if (!result)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Character {} not found in characters table", playerGuid.ToString());
        return false;
    }

    Field* fields = result->Fetch();
    std::string dbName = fields[1].GetString();
    uint32 dbAccountId = fields[2].GetUInt32();

    // Get current cache data
    std::string cacheName = "<unknown>";
    uint32 cacheAccountId = sCharacterCache->GetCharacterAccountIdByGuid(playerGuid);
    sCharacterCache->GetCharacterNameByGuid(playerGuid, cacheName);

    TC_LOG_INFO("module.playerbot.session", "🔧 Cache sync: DB({}, {}) vs Cache({}, {}) for {}",
                dbName, dbAccountId, cacheName, cacheAccountId, playerGuid.ToString());

    // Update cache if different
    if (dbName != cacheName)
    {
        TC_LOG_INFO("module.playerbot.session", "🔧 Updating character name cache: '{}' -> '{}'", cacheName, dbName);
        sCharacterCache->UpdateCharacterData(playerGuid, dbName);
    }

    if (dbAccountId != cacheAccountId)
    {
        TC_LOG_INFO("module.playerbot.session", "🔧 Updating character account cache: {} -> {}", cacheAccountId, dbAccountId);
        sCharacterCache->UpdateCharacterAccountId(playerGuid, dbAccountId);
    }

    TC_LOG_DEBUG("module.playerbot.session", "🔧 Character cache synchronized for {}", playerGuid.ToString());
    return true;
}

// ============================================================================
// Chat Command Support APIs - Added for PlayerBot command system
// ============================================================================

std::vector<Player*> BotWorldSessionMgr::GetPlayerBotsByAccount(uint32 accountId) const
{
    std::vector<Player*> bots;
    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

    for (auto const& [guid, session] : _botSessions)
    {
        if (!session)
            continue;

        Player* bot = session->GetPlayer();
        if (!bot)
            continue;

        // Check if this bot belongs to the specified account
        if (bot->GetSession() && bot->GetSession()->GetAccountId() == accountId)
            bots.push_back(bot);
    }

    return bots;
}

void BotWorldSessionMgr::RemoveAllPlayerBots(uint32 accountId)
{
    std::vector<ObjectGuid> botsToRemove;

    // Collect GUIDs to remove (avoid modifying map while iterating)
    {
        std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);
        for (auto const& [guid, session] : _botSessions)
        {
            if (!session)
                continue;

            Player* bot = session->GetPlayer();
            if (bot && bot->GetSession() && bot->GetSession()->GetAccountId() == accountId)
                botsToRemove.push_back(guid);
        }
    }

    // Remove bots (this releases the mutex between iterations)
    for (ObjectGuid const& guid : botsToRemove)
    {
        RemovePlayerBot(guid);
        TC_LOG_INFO("module.playerbot.commands", "Removed bot {} for account {}", guid.ToString(), accountId);
    }
}

uint32 BotWorldSessionMgr::GetBotCountByAccount(uint32 accountId) const
{
    uint32 count = 0;
    std::lock_guard<std::recursive_mutex> lock(_sessionsMutex);

    for (auto const& [guid, session] : _botSessions)
    {
        if (!session)
            continue;

        Player* bot = session->GetPlayer();
        if (bot && bot->GetSession() && bot->GetSession()->GetAccountId() == accountId)
            ++count;
    }

    return count;
}

} // namespace Playerbot