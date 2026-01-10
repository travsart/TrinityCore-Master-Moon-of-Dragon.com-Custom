/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/**
 * @file JITBotFactory.h
 * @brief Just-In-Time Bot Factory for on-demand bot creation
 *
 * The JITBotFactory provides on-demand bot creation when the warm pool is
 * exhausted. It handles:
 *
 * 1. Asynchronous batch creation for large content (40v40 BGs)
 * 2. Bot recycling for efficient resource usage
 * 3. Progress tracking with callbacks
 * 4. Timeout and cancellation support
 * 5. Priority-based request processing
 *
 * Request Processing Flow:
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                        JIT BOT FACTORY                                   │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   ┌─────────────────┐                                                   │
 * │   │ Factory Request │                                                   │
 * │   │ - Instance type │                                                   │
 * │   │ - Content ID    │                                                   │
 * │   │ - Requirements  │                                                   │
 * │   │ - Callbacks     │                                                   │
 * │   └────────┬────────┘                                                   │
 * │            │                                                            │
 * │            ▼                                                            │
 * │   ┌─────────────────┐    ┌──────────────┐                              │
 * │   │ Check Recycled  │───▶│ Use Recycled │─── Fast path               │
 * │   │ Bots Available? │    │ Bots First   │                              │
 * │   └────────┬────────┘    └──────────────┘                              │
 * │            │ No recycled                                                │
 * │            ▼                                                            │
 * │   ┌─────────────────┐    ┌──────────────┐    ┌──────────────┐          │
 * │   │ Queue Request   │───▶│ Clone Engine │───▶│ Create Bots  │          │
 * │   │ (by priority)   │    │ Batch Clone  │    │ Async        │          │
 * │   └─────────────────┘    └──────────────┘    └──────────────┘          │
 * │                                   │                                     │
 * │            ┌──────────────────────┘                                     │
 * │            │                                                            │
 * │            ▼                                                            │
 * │   ┌─────────────────┐    ┌──────────────┐                              │
 * │   │ Progress Track  │───▶│ Callbacks    │                              │
 * │   │ - % Complete    │    │ - onProgress │                              │
 * │   │ - ETA           │    │ - onComplete │                              │
 * │   └─────────────────┘    │ - onFailed   │                              │
 * │                          └──────────────┘                              │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * Thread Safety:
 * - Request submission is thread-safe
 * - Worker thread processes requests sequentially
 * - Callbacks are invoked on worker thread
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "PoolSlotState.h"
#include "PoolConfiguration.h"
#include "BotCloneEngine.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Playerbot
{

// ============================================================================
// FACTORY REQUEST
// ============================================================================

/**
 * @brief Request for JIT bot creation
 *
 * Contains all information needed to create bots on-demand for an instance.
 */
struct TC_GAME_API FactoryRequest
{
    // ========================================================================
    // IDENTITY
    // ========================================================================

    uint32 requestId = 0;                   ///< Unique request ID
    InstanceType instanceType = InstanceType::Dungeon;  ///< Type of instance
    uint32 contentId = 0;                   ///< Specific content ID (dungeon/raid/bg)
    uint32 playerLevel = 80;                ///< Player level for bot matching

    // ========================================================================
    // FACTION
    // ========================================================================

    Faction playerFaction = Faction::Alliance;  ///< Requesting player's faction

    // ========================================================================
    // PVE REQUIREMENTS
    // ========================================================================

    uint32 tanksNeeded = 0;                 ///< Number of tanks required
    uint32 healersNeeded = 0;               ///< Number of healers required
    uint32 dpsNeeded = 0;                   ///< Number of DPS required

    // ========================================================================
    // PVP REQUIREMENTS (BOTH FACTIONS)
    // ========================================================================

    uint32 allianceNeeded = 0;              ///< Alliance bots needed (PvP)
    uint32 hordeNeeded = 0;                 ///< Horde bots needed (PvP)

    // ========================================================================
    // GEAR REQUIREMENTS
    // ========================================================================

    uint32 minGearScore = 0;                ///< Minimum gear score required

    // ========================================================================
    // CALLBACKS
    // ========================================================================

    /// Called when all bots are ready
    std::function<void(std::vector<ObjectGuid> const&)> onComplete;

    /// Called if request fails
    std::function<void(std::string const&)> onFailed;

    /// Called periodically with progress (0.0 to 1.0)
    std::function<void(float progress)> onProgress;

    // ========================================================================
    // TIMING
    // ========================================================================

    std::chrono::system_clock::time_point createdAt;  ///< When request was created
    std::chrono::milliseconds timeout{60000};         ///< Request timeout

    // ========================================================================
    // PRIORITY
    // ========================================================================

    uint8 priority = 5;                     ///< 1 = highest, 10 = lowest

    // ========================================================================
    // METHODS
    // ========================================================================

    /**
     * @brief Get total bots needed (PvE)
     */
    uint32 GetTotalPvENeeded() const
    {
        return tanksNeeded + healersNeeded + dpsNeeded;
    }

    /**
     * @brief Get total bots needed (all types)
     */
    uint32 GetTotalNeeded() const
    {
        if (allianceNeeded > 0 || hordeNeeded > 0)
            return allianceNeeded + hordeNeeded;
        return GetTotalPvENeeded();
    }

    /**
     * @brief Check if this is a PvP request
     */
    bool IsPvP() const
    {
        return instanceType == InstanceType::Battleground ||
               instanceType == InstanceType::Arena;
    }

    /**
     * @brief Validate the request
     */
    bool IsValid() const
    {
        if (GetTotalNeeded() == 0)
            return false;
        if (GetTotalNeeded() > 100)
            return false;  // Sanity limit
        return true;
    }

    /**
     * @brief Check if request has timed out
     */
    bool HasTimedOut() const
    {
        auto now = std::chrono::system_clock::now();
        return (now - createdAt) >= timeout;
    }
};

// ============================================================================
// REQUEST STATUS
// ============================================================================

/**
 * @brief Status of a factory request
 */
enum class RequestStatus : uint8
{
    Pending,            ///< Waiting to be processed
    InProgress,         ///< Currently creating bots
    Completed,          ///< All bots created successfully
    PartiallyCompleted, ///< Some bots created (enough to proceed)
    Failed,             ///< Request failed (no bots or too few)
    Cancelled,          ///< Request was cancelled
    TimedOut            ///< Request timed out
};

/**
 * @brief Convert RequestStatus to string
 */
inline char const* RequestStatusToString(RequestStatus status)
{
    switch (status)
    {
        case RequestStatus::Pending: return "Pending";
        case RequestStatus::InProgress: return "InProgress";
        case RequestStatus::Completed: return "Completed";
        case RequestStatus::PartiallyCompleted: return "PartiallyCompleted";
        case RequestStatus::Failed: return "Failed";
        case RequestStatus::Cancelled: return "Cancelled";
        case RequestStatus::TimedOut: return "TimedOut";
        default: return "Unknown";
    }
}

// ============================================================================
// REQUEST PROGRESS
// ============================================================================

/**
 * @brief Progress tracking for a request
 */
struct RequestProgress
{
    uint32 requestId = 0;
    RequestStatus status = RequestStatus::Pending;

    uint32 totalNeeded = 0;
    uint32 created = 0;
    uint32 fromRecycled = 0;
    uint32 fromClone = 0;

    std::chrono::system_clock::time_point startTime;
    std::chrono::milliseconds elapsed{0};
    std::chrono::milliseconds estimatedRemaining{0};

    std::vector<ObjectGuid> createdBots;
    std::string errorMessage;

    /**
     * @brief Get progress percentage (0.0 to 1.0)
     */
    float GetProgress() const
    {
        if (totalNeeded == 0)
            return 1.0f;
        return static_cast<float>(created) / static_cast<float>(totalNeeded);
    }
};

// ============================================================================
// RECYCLED BOT
// ============================================================================

/**
 * @brief A recycled bot available for reuse
 */
struct RecycledBot
{
    ObjectGuid guid;                        ///< Bot GUID
    BotRole role = BotRole::DPS;            ///< Bot's role
    Faction faction = Faction::Alliance;    ///< Bot's faction
    uint32 level = 80;                      ///< Bot's level
    uint32 gearScore = 0;                   ///< Bot's gear score
    uint8 playerClass = 0;                  ///< Bot's class
    std::chrono::system_clock::time_point recycleTime;  ///< When bot was recycled

    /**
     * @brief Check if recycled bot matches requirements
     */
    bool Matches(BotRole role, Faction faction, uint32 level, uint32 minGearScore) const
    {
        if (this->role != role)
            return false;
        if (this->faction != faction)
            return false;
        if (this->level < level - 5 || this->level > level + 5)
            return false;
        if (this->gearScore < minGearScore)
            return false;
        return true;
    }
};

// ============================================================================
// JIT BOT FACTORY
// ============================================================================
// NOTE: JITFactoryConfig is defined in PoolConfiguration.h

/**
 * @brief Just-In-Time factory for creating bots on demand
 *
 * Handles overflow bot creation when the warm pool is exhausted.
 * Supports large content like 40v40 battlegrounds.
 */
class TC_GAME_API JITBotFactory
{
public:
    /**
     * @brief Get singleton instance
     */
    static JITBotFactory* Instance();

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the factory
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update (process queue)
     * @param diff Time since last update
     */
    void Update(uint32 diff);

    /**
     * @brief Load configuration
     */
    void LoadConfig();

    // ========================================================================
    // REQUEST SUBMISSION
    // ========================================================================

    /**
     * @brief Submit a request for bot creation
     * @param request The factory request
     * @return Request ID for tracking
     */
    uint32 SubmitRequest(FactoryRequest request);

    /**
     * @brief Cancel a pending request
     * @param requestId Request to cancel
     */
    void CancelRequest(uint32 requestId);

    /**
     * @brief Get request status
     * @param requestId Request to check
     * @return Current status
     */
    RequestStatus GetRequestStatus(uint32 requestId) const;

    /**
     * @brief Get request progress
     * @param requestId Request to check
     * @return Progress info
     */
    RequestProgress GetRequestProgress(uint32 requestId) const;

    /**
     * @brief Get estimated completion time
     * @param requestId Request to check
     * @return Estimated time until completion
     */
    std::chrono::milliseconds GetEstimatedCompletion(uint32 requestId) const;

    // ========================================================================
    // BOT RECYCLING
    // ========================================================================

    /**
     * @brief Recycle a bot for potential reuse
     * @param botGuid Bot to recycle
     * @param role Bot's role
     * @param faction Bot's faction
     * @param level Bot's level
     * @param gearScore Bot's gear score
     * @param playerClass Bot's class
     */
    void RecycleBot(
        ObjectGuid botGuid,
        BotRole role,
        Faction faction,
        uint32 level,
        uint32 gearScore,
        uint8 playerClass);

    /**
     * @brief Recycle multiple bots from an instance
     * @param bots Vector of bot GUIDs
     */
    void RecycleBots(std::vector<ObjectGuid> const& bots);

    /**
     * @brief Get a recycled bot if available
     * @param role Required role
     * @param faction Required faction
     * @param level Required level
     * @param minGearScore Minimum gear score
     * @return Bot GUID or empty if none available
     */
    ObjectGuid GetRecycledBot(
        BotRole role,
        Faction faction,
        uint32 level,
        uint32 minGearScore);

    /**
     * @brief Get count of recycled bots available
     */
    uint32 GetRecycledBotCount() const;

    /**
     * @brief Clear expired recycled bots
     */
    void CleanupRecycledBots();

    // ========================================================================
    // QUERIES
    // ========================================================================

    /**
     * @brief Check if factory is busy
     */
    bool IsBusy() const;

    /**
     * @brief Get number of pending requests
     */
    uint32 GetPendingRequestCount() const;

    /**
     * @brief Check if factory can handle a request
     * @param request Request to check
     * @return true if can be handled
     */
    bool CanHandleRequest(FactoryRequest const& request) const;

    /**
     * @brief Get account ID for a bot created by this factory
     * @param botGuid Bot GUID to look up
     * @return Account ID or 0 if not found
     *
     * Since database commits are async, we store the account ID from CloneResult
     * so callbacks can log in bots without waiting for database to commit.
     */
    uint32 GetAccountForBot(ObjectGuid botGuid) const;

    /**
     * @brief Store account ID mapping for a created bot
     * @param botGuid Bot GUID
     * @param accountId Account ID
     */
    void StoreAccountForBot(ObjectGuid botGuid, uint32 accountId);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    struct FactoryStatistics
    {
        uint32 pendingRequests = 0;
        uint32 inProgressRequests = 0;
        uint32 completedThisHour = 0;
        uint32 failedThisHour = 0;
        uint32 cancelledThisHour = 0;
        uint32 recycledBotsAvailable = 0;
        uint32 botsCreatedThisHour = 0;
        uint32 botsRecycledThisHour = 0;
        std::chrono::milliseconds avgCreationTime{0};
        std::chrono::milliseconds avgRequestTime{0};
    };

    /**
     * @brief Get current statistics
     */
    FactoryStatistics GetStatistics() const;

    /**
     * @brief Print statistics to log
     */
    void PrintStatistics() const;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Get current configuration
     */
    JITFactoryConfig const& GetConfig() const { return _config; }

    /**
     * @brief Set configuration
     */
    void SetConfig(JITFactoryConfig const& config);

private:
    JITBotFactory() = default;
    ~JITBotFactory() = default;
    JITBotFactory(JITBotFactory const&) = delete;
    JITBotFactory& operator=(JITBotFactory const&) = delete;

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    /**
     * @brief Worker thread function
     */
    void WorkerThread();

    /**
     * @brief Process a single request
     * @param request Request to process
     * @return Progress after processing
     */
    RequestProgress ProcessRequest(FactoryRequest& request);

    /**
     * @brief Create bots for PvE request
     */
    std::vector<ObjectGuid> CreatePvEBots(
        FactoryRequest const& request,
        RequestProgress& progress);

    /**
     * @brief Create bots for PvP request (both factions)
     */
    std::vector<ObjectGuid> CreatePvPBots(
        FactoryRequest const& request,
        RequestProgress& progress);

    /**
     * @brief Clone bots with retry logic
     * @param baseReq Base clone request parameters
     * @param maxRetries Maximum retry attempts
     * @param progress Progress tracker to update
     * @return Vector of successfully created bot GUIDs
     */
    std::vector<ObjectGuid> BatchCloneWithRetry(
        BatchCloneRequest const& baseReq,
        uint32 maxRetries,
        RequestProgress& progress);

    /**
     * @brief Try to get recycled bots for role
     */
    std::vector<ObjectGuid> GetRecycledBotsForRole(
        BotRole role,
        Faction faction,
        uint32 level,
        uint32 minGearScore,
        uint32 count);

    /**
     * @brief Get priority for instance type
     */
    uint8 GetPriorityForType(InstanceType type) const;

    /**
     * @brief Get timeout for instance type
     */
    std::chrono::milliseconds GetTimeoutForType(InstanceType type) const;

    /**
     * @brief Process request timeouts
     */
    void ProcessTimeouts();

    // ========================================================================
    // DATA MEMBERS - Request Management
    // ========================================================================

    /// Active request being processed
    struct ActiveRequest
    {
        FactoryRequest request;
        RequestProgress progress;
    };

    std::unordered_map<uint32, ActiveRequest> _activeRequests;
    mutable std::mutex _requestMutex;

    /// Priority queue comparator (lower priority number = higher priority)
    struct RequestComparator
    {
        bool operator()(FactoryRequest const& a, FactoryRequest const& b) const
        {
            return a.priority > b.priority;
        }
    };

    std::priority_queue<FactoryRequest, std::vector<FactoryRequest>, RequestComparator> _pendingQueue;
    mutable std::mutex _queueMutex;

    std::atomic<uint32> _nextRequestId{1};

    // ========================================================================
    // DATA MEMBERS - Recycled Bots
    // ========================================================================

    std::vector<RecycledBot> _recycledBots;
    mutable std::mutex _recycleMutex;

    // ========================================================================
    // DATA MEMBERS - Bot Account Mapping
    // ========================================================================

    /// Map of bot GUID → account ID for bots created by this factory
    /// Needed because database commits are async, so we can't query account
    /// immediately after creation. Stored here for callback use.
    std::unordered_map<ObjectGuid, uint32> _botAccountMap;
    mutable std::mutex _accountMapMutex;

    // ========================================================================
    // DATA MEMBERS - Worker Thread
    // ========================================================================

    std::unique_ptr<std::thread> _workerThread;
    std::atomic<bool> _running{false};

    // ========================================================================
    // DATA MEMBERS - Configuration
    // ========================================================================

    JITFactoryConfig _config;

    // ========================================================================
    // DATA MEMBERS - Statistics
    // ========================================================================

    std::atomic<uint32> _completedThisHour{0};
    std::atomic<uint32> _failedThisHour{0};
    std::atomic<uint32> _cancelledThisHour{0};
    std::atomic<uint32> _botsCreatedThisHour{0};
    std::atomic<uint32> _botsRecycledThisHour{0};
    std::chrono::milliseconds _avgCreationTime{0};
    std::chrono::milliseconds _avgRequestTime{0};
    uint32 _creationTimeSamples = 0;
    uint32 _requestTimeSamples = 0;
    mutable std::mutex _statsMutex;

    std::chrono::system_clock::time_point _hourStart;

    // ========================================================================
    // DATA MEMBERS - State
    // ========================================================================

    std::atomic<bool> _initialized{false};
};

} // namespace Playerbot

#define sJITBotFactory Playerbot::JITBotFactory::Instance()
