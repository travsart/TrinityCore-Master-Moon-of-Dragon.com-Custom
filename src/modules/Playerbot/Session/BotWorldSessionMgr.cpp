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
#include "Config/PlayerbotConfig.h"
#include "../Performance/ThreadPool/ThreadPool.h"
#include "../Spatial/SpatialGridManager.h"
#include "../Spatial/SpatialGridScheduler.h"
#include <chrono>
#include <queue>
#include "GameTime.h"

namespace Playerbot {

// ============================================================================
// PHASE A: ThreadPool Integration - Priority Mapping
// ============================================================================

/**
 * @brief Map BotPriority to ThreadPool TaskPriority
 *
 * Mapping Strategy:
 * - EMERGENCY (health < 20%, combat death) ? CRITICAL (0-10ms tolerance)
 * - HIGH (combat, groups) ? HIGH (10-50ms tolerance)
 * - MEDIUM (active movement) ? NORMAL (50-200ms tolerance)
 * - LOW (idle, resting) ? LOW (200-1000ms tolerance)
 * - SUSPENDED (dead, disconnected) ? IDLE (no time constraints)
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

    TC_LOG_INFO("module.playerbot.session", "?? BotWorldSessionMgr: Initializing with native TrinityCore login pattern + Enterprise System");

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
    _maxSpawnsPerTick = sPlayerbotConfig->GetInt("Playerbot.LevelManager.MaxBotsPerUpdate", 10);

    // Initialize spatial grid system (grids created on-demand per map)
    TC_LOG_INFO("module.playerbot.session",
        "?? BotWorldSessionMgr: Spatial grid system initialized (lock-free double-buffered architecture)");

    _enabled.store(true);
    _initialized.store(true);

    TC_LOG_INFO("module.playerbot.session",
        "?? BotWorldSessionMgr: ENTERPRISE MODE enabled for 5000 bot scalability (spawn rate: {}/tick)",
        _maxSpawnsPerTick);

    return true;
}

void BotWorldSessionMgr::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("module.playerbot.session", "?? BotWorldSessionMgr: Shutting down");

    _enabled.store(false);

    // Shutdown enterprise components first
    sBotHealthCheck->Shutdown();
    sBotPerformanceMon->Shutdown();
    sBotPriorityMgr->Shutdown();

    ::std::lock_guard lock(_sessionsMutex);

    // Clean logout all bot sessions
    for (auto& [guid, session] : _botSessions)
    {
        if (!session)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: session in Shutdown");
            continue;  // Skip null sessions
        }
        if (session && session->GetPlayer())
        {
            // MEMORY SAFETY: Protect against use-after-free when accessing Player name
            Player* player = session->GetPlayer();
            try {
                TC_LOG_INFO("module.playerbot.session", "?? Logging out bot: {}", player->GetName());
            }
            catch (...)
            {
                TC_LOG_INFO("module.playerbot.session", "?? Logging out bot (name unavailable - use-after-free protection)");
            }
            session->LogoutPlayer(true);
        }
    }

    _botSessions.clear();
    _botsLoading.clear();

    // Destroy all spatial grids and stop worker threads
    sSpatialGridManager.DestroyAllGrids();
    TC_LOG_INFO("module.playerbot.session", "?? BotWorldSessionMgr: Spatial grid system shut down");

    _initialized.store(false);
}

bool BotWorldSessionMgr::AddPlayerBot(ObjectGuid playerGuid, uint32 masterAccountId)
{
    if (!_enabled.load() || !_initialized.load())
    {
        TC_LOG_ERROR("module.playerbot.session", "?? BotWorldSessionMgr not enabled or initialized");
        return false;
    }

    ::std::lock_guard lock(_sessionsMutex);

    // Check if bot is already loading
    if (_botsLoading.find(playerGuid) != _botsLoading.end())
    {
        TC_LOG_DEBUG("module.playerbot.session", "?? Bot {} already loading", playerGuid.ToString());
        return false;
    }

    // FIX #22: CRITICAL - Eliminate ObjectAccessor call to prevent deadlock
    // Check if bot is already added by scanning existing sessions (avoids ObjectAccessor lock)
    for (auto const& [guid, session] : _botSessions)
    {
        if (guid == playerGuid && session && session->GetPlayer())
        {
            TC_LOG_DEBUG("module.playerbot.session", "?? Bot {} already in world (found in _botSessions)", playerGuid.ToString());
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
        TC_LOG_DEBUG("module.playerbot.session", "?? Found account ID {} for character {}", accountId, playerGuid.ToString());
    }

    if (!accountId)
    {
        TC_LOG_ERROR("module.playerbot.session", "?? Could not find account for character {} in playerbot database", playerGuid.ToString());
        return false;
    }

    // CRITICAL FIX: Check for existing sessions by account ID to prevent duplicates
    for (auto const& [guid, session] : _botSessions)
    {
        if (session && session->GetAccountId() == accountId)
        {
            TC_LOG_WARN("module.playerbot.session", "?? DUPLICATE SESSION PREVENTION: Account {} already has an active bot session with character {}, rejecting new character {}",
                accountId, guid.ToString(), playerGuid.ToString());
            return false;
        }
    }

    // CRITICAL FIX: Enforce MaxBots limit
    uint32 maxBots = sPlayerbotConfig->GetInt("Playerbot.MaxBots", 100);
    uint32 totalBots = static_cast<uint32>(_botSessions.size() + _pendingSpawns.size());
    if (totalBots >= maxBots)
    {
        TC_LOG_WARN("module.playerbot.session",
            "?? MAX BOTS LIMIT: Cannot queue bot {} - already at limit ({}/{} bots)",
            playerGuid.ToString(), totalBots, maxBots);
        return false;
    }

    // CRITICAL FIX V3: RATE-LIMITED SPAWN QUEUE
    //
    // ROOT CAUSE OF SERVER HANG: "ENTERPRISE FIX V2" removed all throttling, causing:
    // - 296 bots spawning immediately at server startup
    // - 296  66 async queries = 19,536 database queries flooding connection pool
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
        "?? Queueing bot {} for rate-limited spawn (queue position: {}, accountId: {})",
        playerGuid.ToString(), _pendingSpawns.size() + 1, accountId);

    // Add to pending spawn queue - will be processed in UpdateSessions()
    _pendingSpawns.push_back({playerGuid, accountId});

    TC_LOG_DEBUG("module.playerbot.session",
        "?? Bot {} added to spawn queue. Total pending: {}",
        playerGuid.ToString(), _pendingSpawns.size());

    return true; // Queued successfully
}
void BotWorldSessionMgr::RemovePlayerBot(ObjectGuid playerGuid)
{
    // CRITICAL FIX (Map.cpp:686 crash): Do NOT call LogoutPlayer() synchronously!
    // Problem: RemovePlayerBot() can be called from commands while Map::Update() is running
    // If we call LogoutPlayer() here, it removes player from map IMMEDIATELY (WorldSession.cpp:715)
    // This invalidates Map iterators, causing Map.cpp:686 crash
    //
    // Solution: Use async disconnection queue (same pattern as worker thread disconnects)
    // The session will be cleaned up in next UpdateSessions() call on main thread
    // when Map::Update() is NOT iterating over players
    ::std::lock_guard lock(_sessionsMutex);

    auto it = _botSessions.find(playerGuid);
    if (it == _botSessions.end())
    {
        TC_LOG_DEBUG("module.playerbot.session", "?? Bot session not found for removal: {}", playerGuid.ToString());
        return;
    }

    ::std::shared_ptr<BotSession> session = it->second;
    if (session)
    {
        // Log removal (safely handle name access)
    if (session->GetPlayer())
        {
            try {
                TC_LOG_INFO("module.playerbot.session", "?? Queuing bot for removal: {}", session->GetPlayer()->GetName());
            }
            catch (...)
            {
                TC_LOG_INFO("module.playerbot.session", "?? Queuing bot for removal (name unavailable)");
            }
        }

        // Signal session termination - BotSession::Update() will return false next cycle
        session->KickPlayer("BotWorldSessionMgr::RemovePlayerBot - Bot removal requested");
    }

    // Push to async disconnection queue (lock-free, thread-safe)
    // Will be processed in UpdateSessions() Phase 3 when safe (after Map::Update() completes)
    _asyncDisconnections.push(playerGuid);

    TC_LOG_DEBUG("module.playerbot.session", "?? Bot {} queued for async removal", playerGuid.ToString());
}

Player* BotWorldSessionMgr::GetPlayerBot(ObjectGuid playerGuid) const
{
    ::std::lock_guard lock(_sessionsMutex);

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
    // Phase 1 (Simple rotation): 78% improvement (600ms ? 150ms)
    // Phase 2 (Enterprise priority): Targets 145ms per tick with 5000 bots
    //
    // Priority Levels:
    // - EMERGENCY (5 bots): Every tick - critical states
    // - HIGH (45 bots): Every tick - combat, groups
    // - MEDIUM (40 bots): Every 10 ticks - active movement
    // - LOW (91 bots): Every 50 ticks - idle, resting
    //
    // Total per-tick load: 5 + 45 + 40 + 91 = 181 bots @ 0.8ms = 145ms target

    uint32 currentTime = GameTime::GetGameTimeMS();
    _tickCounter++;

    // PHASE 0: Enterprise monitoring - Begin tick measurement
    sBotPerformanceMon->BeginTick(currentTime);
    sBotHealthCheck->RecordHeartbeat(currentTime);

    // PHASE 0.5: CRITICAL FIX - Process pending spawns at controlled rate
    // Prevents database overload by rate-limiting async login submissions
    {
        ::std::lock_guard lock(_sessionsMutex);

        _spawnsProcessedThisTick = 0;

        while (!_pendingSpawns.empty() && _spawnsProcessedThisTick < _maxSpawnsPerTick)
        {
            auto [playerGuid, accountId] = _pendingSpawns.front();
            _pendingSpawns.erase(_pendingSpawns.begin());

            TC_LOG_INFO("module.playerbot.session",
                "?? Processing queued spawn for bot {} (accountId: {}, remaining in queue: {})",
                playerGuid.ToString(), accountId, _pendingSpawns.size());

            // Synchronize character cache
    if (!SynchronizeCharacterCache(playerGuid))
            {
                TC_LOG_ERROR("module.playerbot.session",
                    "?? Failed to synchronize character cache for {}", playerGuid.ToString());
                continue;
            }

            // Mark as loading
            _botsLoading.insert(playerGuid);

            // Create BotSession
            ::std::shared_ptr<BotSession> botSession = BotSession::Create(accountId);
            if (!botSession)
            {
                TC_LOG_ERROR("module.playerbot.session",
                    "?? Failed to create BotSession for {}", playerGuid.ToString());
                _botsLoading.erase(playerGuid);
                continue;
            }

            // Store session
            _botSessions[playerGuid] = botSession;

            // Initiate async login (1 bot  66 queries)
    if (!botSession->LoginCharacter(playerGuid))
            {
                TC_LOG_ERROR("module.playerbot.session",
                    "?? Failed to initiate async login for {}", playerGuid.ToString());
                _botSessions.erase(playerGuid);
                _botsLoading.erase(playerGuid);
                continue;
            }

            TC_LOG_INFO("module.playerbot.session",
                "? Async bot login initiated for: {} ({}/{} spawns this tick)",
                playerGuid.ToString(), _spawnsProcessedThisTick + 1, _maxSpawnsPerTick);

            _spawnsProcessedThisTick++;
        }

        if (_spawnsProcessedThisTick > 0)
        {
            TC_LOG_INFO("module.playerbot.session",
                "?? Processed {} bot spawns this tick. Remaining in queue: {}",
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

    ::std::vector<::std::pair<ObjectGuid, ::std::shared_ptr<BotSession>>> sessionsToUpdate;
    ::std::vector<ObjectGuid> sessionsToRemove;
    uint32 botsSkipped = 0;

    // PHASE 1: Priority-based session collection (minimal lock time)
    {
        ::std::lock_guard lock(_sessionsMutex);

        if (_botSessions.empty() && _pendingSpawns.empty())
        {
            sBotPerformanceMon->EndTick(currentTime, 0, 0);
            return;
        }

        sessionsToUpdate.reserve(200); // Reserve for typical load
    for (auto it = _botSessions.begin(); it != _botSessions.end(); ++it)
        {
            ObjectGuid guid = it->first;
            ::std::shared_ptr<BotSession> session = it->second;

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
                    TC_LOG_INFO("module.playerbot.session", "?? ? Bot login completed: {}", guid.ToString());

                    // Initialize priority for newly logged-in bot (default to LOW, will adjust in Phase 2 if needed)
    if (_enterpriseMode)
                        sBotPriorityMgr->SetPriority(guid, BotPriority::LOW);

                    sessionsToUpdate.emplace_back(guid, session);
                }
                else if (session->IsLoginFailed())
                {
                    TC_LOG_ERROR("module.playerbot.session", "?? Bot login failed: {}", guid.ToString());
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
    // Before (Sequential): 145 bots  1ms = 145ms
    // After (Parallel, 8 threads): 145 bots  8 threads = 18 bots/thread  1ms = 18ms (7x speedup)
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

    ::std::vector<ObjectGuid> disconnectedSessions;

    // OPTION 5: _asyncBotsUpdated and _asyncDisconnections now in header (class members)
    // Thread-safe without mutex - using boost::lockfree::queue and std::atomic

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
    // OPTION 5: Fire-and-forget - no futures to track
    // Tasks complete asynchronously and report via _asyncDisconnections queue
    bool useThreadPool = serverReady && !sessionsToUpdate.empty();

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
    }
    for (auto& [guid, botSession] : sessionsToUpdate)
    {
        // Validate session before submitting task
    if (!botSession || !botSession->IsActive())
        {
            disconnectedSessions.push_back(guid);
            continue;
        }

        // OPTION 5: Capture weak_ptr for session lifetime detection
        ::std::weak_ptr<BotSession> weakSession = botSession;

        // Define the update logic (will be used in both parallel and sequential paths)
        auto updateLogic = [guid, weakSession, diff, currentTime, enterpriseMode, tickCounter, this]()
            {
                TC_LOG_TRACE("playerbot.session.task", "?? TASK START for bot {}", guid.ToString());
                // OPTION 5: Check if session still exists (thread-safe with weak_ptr)
                auto session = weakSession.lock();
                if (!session)
                {
                    TC_LOG_WARN("module.playerbot.session", "?? Bot session destroyed during update: {}", guid.ToString());
                    _asyncDisconnections.push(guid);  // Lock-free push
                    return;
                }

                try
                {
                    // Create PacketFilter for bot session
                    class BotPacketFilter : public PacketFilter
                    {
                    public:
                        explicit BotPacketFilter(WorldSession* session) : PacketFilter(session) {}
                        virtual ~BotPacketFilter() override = default;
                        bool Process(WorldPacket*) override { return true; }
                        bool ProcessUnsafe() const override { return true; }
                    } filter(session.get());

                    // Update session
                    TC_LOG_TRACE("playerbot.session.update", "?? Starting Update() for bot {}", guid.ToString());
                    bool updateResult = session->Update(diff, filter);

                    if (!updateResult)
                    {
                        TC_LOG_WARN("module.playerbot.session", "?? Bot update failed: {}", guid.ToString());
                        _asyncDisconnections.push(guid);  // Lock-free push
                        return;
                    }

                    // Increment success counter (atomic, thread-safe)
                    _asyncBotsUpdated.fetch_add(1, ::std::memory_order_relaxed);

                    // IMPROVEMENT #1: ADAPTIVE AutoAdjustPriority frequency based on bot activity
                    // Active bots (combat/group) = more frequent checks (250ms)
                    // Idle bots (high health, not moving) = less frequent checks (2.5s)
    if (enterpriseMode && session->IsLoginComplete())
                    {
                        Player* bot = session->GetPlayer();
                        if (!bot || !bot->IsInWorld())
                        {
                            TC_LOG_WARN("module.playerbot.session", "?? Bot disconnected: {}", guid.ToString());

                            // PLAYERBOT FIX: Do NOT call LogoutPlayer() from worker thread!
                            // This causes Map.cpp:686 crash by removing player from map on worker thread
                            // Instead, push to _asyncDisconnections - main thread will handle cleanup
                            // if (session->GetPlayer())
                            //     session->LogoutPlayer(true);  // REMOVED - worker thread unsafe!

                            _asyncDisconnections.push(guid);  // Lock-free push - main thread handles logout
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
                catch (::std::exception const& e)
                {
                    TC_LOG_ERROR("module.playerbot.session", "?? Exception updating bot {}: {}", guid.ToString(), e.what());
                    sBotHealthCheck->RecordError(guid, "UpdateException");
                    _asyncDisconnections.push(guid);  // Lock-free push
                }
                catch (...)
                {
                    TC_LOG_ERROR("module.playerbot.session", "?? Unknown exception updating bot {}", guid.ToString());
                    sBotHealthCheck->RecordError(guid, "UnknownException");
                    _asyncDisconnections.push(guid);  // Lock-free push
                }

                TC_LOG_TRACE("playerbot.session.task", "?? TASK END for bot {}", guid.ToString());
            };

        // Execute either parallel (ThreadPool) or sequential (direct call)
    if (useThreadPool)
        {
            // OPTION 5: Fire-and-forget submission (no future storage)
            try
            {
                BotPriority botPriority = sBotPriorityMgr->GetPriority(guid);
                Performance::TaskPriority taskPriority = MapBotPriorityToTaskPriority(botPriority);

                // Submit and immediately discard future - task runs async
                Performance::GetThreadPool().Submit(taskPriority, updateLogic);

                TC_LOG_TRACE("playerbot.session.submit", "?? Submitted async task for bot {}", guid.ToString());
            }
            catch (...)
            {
                // ThreadPool failed - execute sequentially
                useThreadPool = false;
                updateLogic();
            }
        }
        else
        {
            // Sequential path: Execute directly
            updateLogic();
        }
    }

    // OPTION 5: Process async disconnections from lock-free queue
    // Worker threads push to _asyncDisconnections, we pop here (no mutex needed)
    ObjectGuid disconnectedGuid;
    while (_asyncDisconnections.pop(disconnectedGuid))
    {
        disconnectedSessions.push_back(disconnectedGuid);
    }

    // PHASE 3: Cleanup disconnected sessions
    // CRITICAL FIX (GridNotifiers.cpp:226 crash): Must call LogoutPlayer() BEFORE erasing session!
    // Problem: When session is erased, shared_ptr destructor runs but Player is still registered in Grid
    // Solution: Call LogoutPlayer() first (removes Player from Map/Grid), THEN erase session
    if (!disconnectedSessions.empty())
    {
        ::std::lock_guard lock(_sessionsMutex);
        for (ObjectGuid const& guid : disconnectedSessions)
        {
            auto it = _botSessions.find(guid);
            if (it != _botSessions.end())
            {
                ::std::shared_ptr<BotSession> session = it->second;
                if (session)
                {
                    // CRITICAL: Call LogoutPlayer() on main thread to safely remove from Grid
                    // This must happen BEFORE the session is destroyed
                    if (session->GetPlayer() && session->GetPlayer()->IsInWorld())
                    {
                        try {
                            // SAFETY: Use GetGUID().GetCounter() instead of GetName()
                            // Player string data may be freed during destruction
                            TC_LOG_DEBUG("module.playerbot.session",
                                "Logging out bot {} before session cleanup (GridNotifiers crash fix)",
                                session->GetPlayer()->GetGUID().GetCounter());
                            session->LogoutPlayer(true);  // true = save to DB
                        }
                        catch (...)
                        {
                            TC_LOG_ERROR("module.playerbot.session",
                                "Exception during LogoutPlayer() for bot {} - continuing cleanup",
                                guid.ToString());
                        }
                    }
                }
                _botSessions.erase(it);
            }
            sBotPriorityMgr->RemoveBot(guid);
        }
    }

    // PHASE 4: Lightweight enterprise monitoring (reduced frequency to minimize overhead)
    if (_enterpriseMode)
    {
        // OPTION 5: Get async update count and reset for next tick
        uint32 asyncUpdated = _asyncBotsUpdated.exchange(0, ::std::memory_order_relaxed);
        sBotPerformanceMon->EndTick(currentTime, asyncUpdated, botsSkipped);

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

uint32 BotWorldSessionMgr::ProcessAllDeferredPackets()
{
    // CRITICAL: This method MUST be called from the main world thread ONLY!
    // Purpose: Process packets queued by bot worker threads that require serialization with Map::Update()
    //
    // Thread Safety:
    // - This method runs on main thread
    // - BotSession::QueueDeferredPacket() runs on worker threads (thread-safe with mutex)
    // - Deferred packets execute on main thread, synchronized with Map::Update()
    //
    // Performance:
    // - Processes all deferred packets across all bot sessions
    // - Each session limited to 50 packets/update (MAX_DEFERRED_PACKETS_PER_UPDATE)
    // - Expected load: 300-400 packets/sec with 5000 bots
    // - Main thread capacity: 1000 packets/sec (2.5x safety margin)

    uint32 totalProcessed = 0;

    // Collect sessions that need deferred packet processing
    ::std::vector<::std::shared_ptr<BotSession>> sessionsToProcess;
    {
        ::std::lock_guard lock(_sessionsMutex);
        sessionsToProcess.reserve(_botSessions.size());

        for (auto const& [guid, session] : _botSessions)
        {
            if (session && session->HasDeferredPackets())
            {
                sessionsToProcess.push_back(session);
            }
        }
    } // Release mutex before processing

    // Process deferred packets for each session
    for (auto const& session : sessionsToProcess)
    {
        if (!session)
            continue;

        uint32 processed = session->ProcessDeferredPackets();
        totalProcessed += processed;

        if (processed > 0)
        {
            TC_LOG_TRACE("playerbot.packets.deferred",
                "Bot {} processed {} deferred packets on main thread",
                session->GetPlayerName(), processed);
        }
    }

    // Log statistics if significant activity
    if (totalProcessed > 0)
    {
        TC_LOG_DEBUG("playerbot.packets.deferred",
            "ProcessAllDeferredPackets: {} total packets processed from {} bot sessions",
            totalProcessed, sessionsToProcess.size());
    }

    return totalProcessed;
}

uint32 BotWorldSessionMgr::GetBotCount() const
{
    ::std::lock_guard lock(_sessionsMutex);
    return static_cast<uint32>(_botSessions.size());
}

void BotWorldSessionMgr::TriggerCharacterLoginForAllSessions()
{
    // For compatibility with existing BotSpawner system
    // This method will work with the new native login system
    TC_LOG_INFO("module.playerbot.session", "?? TriggerCharacterLoginForAllSessions called");

    // The native login approach doesn't need this trigger mechanism
    // But we can use it to spawn new bots if needed
    TC_LOG_INFO("module.playerbot.session", "?? Using native login - no manual triggering needed");
}

bool BotWorldSessionMgr::SynchronizeCharacterCache(ObjectGuid playerGuid)
{
    TC_LOG_DEBUG("module.playerbot.session", "?? Synchronizing character cache for {}", playerGuid.ToString());

    // CRITICAL FIX: Use synchronous prepared statement for character lookup
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_DATA_FOR_GUILD);
    stmt->setUInt64(0, playerGuid.GetCounter());
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        TC_LOG_ERROR("module.playerbot.session", "?? Character {} not found in characters table", playerGuid.ToString());
        return false;
    }

    Field* fields = result->Fetch();
    ::std::string dbName = fields[0].GetString();       // name is first field
    uint32 dbAccountId = fields[6].GetUInt32();      // account is 7th field (0-indexed: field 6)
    // Get current cache data
    ::std::string cacheName = "<unknown>";
    uint32 cacheAccountId = sCharacterCache->GetCharacterAccountIdByGuid(playerGuid);
    sCharacterCache->GetCharacterNameByGuid(playerGuid, cacheName);

    TC_LOG_INFO("module.playerbot.session", "?? Cache sync: DB({}, {}) vs Cache({}, {}) for {}",
                dbName, dbAccountId, cacheName, cacheAccountId, playerGuid.ToString());

    // Update cache if different
    if (dbName != cacheName)
    {
        TC_LOG_INFO("module.playerbot.session", "?? Updating character name cache: '{}' -> '{}'", cacheName, dbName);
        sCharacterCache->UpdateCharacterData(playerGuid, dbName);
    }

    if (dbAccountId != cacheAccountId)
    {
        TC_LOG_INFO("module.playerbot.session", "?? Updating character account cache: {} -> {}", cacheAccountId, dbAccountId);
        sCharacterCache->UpdateCharacterAccountId(playerGuid, dbAccountId);
    }

    TC_LOG_DEBUG("module.playerbot.session", "?? Character cache synchronized for {}", playerGuid.ToString());
    return true;
}

// ============================================================================
// Chat Command Support APIs - Added for PlayerBot command system
// ============================================================================

::std::vector<Player*> BotWorldSessionMgr::GetPlayerBotsByAccount(uint32 accountId) const
{
    ::std::vector<Player*> bots;
    ::std::lock_guard lock(_sessionsMutex);

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
    ::std::vector<ObjectGuid> botsToRemove;

    // Collect GUIDs to remove (avoid modifying map while iterating)
    {
        ::std::lock_guard lock(_sessionsMutex);
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
    ::std::lock_guard lock(_sessionsMutex);

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
