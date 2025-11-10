/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MovementArbiter.h"
#include "Player.h"
#include "MotionMaster.h"
#include "Log.h"
#include "Timer.h"
#include "ObjectAccessor.h"
#include "../Spatial/SpatialGridQueryHelpers.h"
#include <sstream>
#include <chrono>
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// MovementArbiterStatistics
// ============================================================================

void MovementArbiterStatistics::Reset()
{
    totalRequests.store(0, std::memory_order_relaxed);
    executedRequests.store(0, std::memory_order_relaxed);
    duplicateRequests.store(0, std::memory_order_relaxed);
    lowPriorityFiltered.store(0, std::memory_order_relaxed);
    interruptedRequests.store(0, std::memory_order_relaxed);

    criticalRequests.store(0, std::memory_order_relaxed);
    veryHighRequests.store(0, std::memory_order_relaxed);
    highRequests.store(0, std::memory_order_relaxed);
    mediumRequests.store(0, std::memory_order_relaxed);
    lowRequests.store(0, std::memory_order_relaxed);
    minimalRequests.store(0, std::memory_order_relaxed);

    totalArbitrationTimeUs.store(0, std::memory_order_relaxed);
    maxArbitrationTimeUs.store(0, std::memory_order_relaxed);

    currentQueueSize.store(0, std::memory_order_relaxed);
    maxQueueSize.store(0, std::memory_order_relaxed);
}

std::string MovementArbiterStatistics::ToString() const
{
    std::ostringstream oss;

    uint64 total = totalRequests.load(std::memory_order_relaxed);
    uint64 executed = executedRequests.load(std::memory_order_relaxed);
    uint64 duplicates = duplicateRequests.load(std::memory_order_relaxed);
    uint64 filtered = lowPriorityFiltered.load(std::memory_order_relaxed);

    oss << "MovementArbiterStatistics {\n";
    oss << "  Total Requests: " << total << "\n";
    oss << "  Executed: " << executed << " (" << (GetAcceptanceRate() * 100.0) << "%)\n";
    oss << "  Duplicates: " << duplicates << " (" << (GetDuplicateRate() * 100.0) << "%)\n";
    oss << "  Filtered: " << filtered << "\n";
    oss << "  Interrupted: " << interruptedRequests.load(std::memory_order_relaxed) << "\n\n";

    oss << "  Priority Distribution:\n";
    oss << "    CRITICAL: " << criticalRequests.load(std::memory_order_relaxed) << "\n";
    oss << "    VERY_HIGH: " << veryHighRequests.load(std::memory_order_relaxed) << "\n";
    oss << "    HIGH: " << highRequests.load(std::memory_order_relaxed) << "\n";
    oss << "    MEDIUM: " << mediumRequests.load(std::memory_order_relaxed) << "\n";
    oss << "    LOW: " << lowRequests.load(std::memory_order_relaxed) << "\n";
    oss << "    MINIMAL: " << minimalRequests.load(std::memory_order_relaxed) << "\n\n";

    oss << "  Performance:\n";
    oss << "    Avg Arbitration: " << GetAverageArbitrationTimeUs() << " us\n";
    oss << "    Max Arbitration: " << maxArbitrationTimeUs.load(std::memory_order_relaxed) << " us\n";
    oss << "    Current Queue: " << currentQueueSize.load(std::memory_order_relaxed) << "\n";
    oss << "    Max Queue: " << maxQueueSize.load(std::memory_order_relaxed) << "\n";

    oss << "}";
    return oss.str();
}

// ============================================================================
// MovementArbiter - Construction/Destruction
// ============================================================================

MovementArbiter::MovementArbiter(Player* bot)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
            return nullptr;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
            return;
        }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
        return nullptr;
    }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
                return;
            }
    : _bot(bot)
    , _lastUpdateTime(GameTime::GetGameTimeMS())
{
    if (!_bot)
        throw std::invalid_argument("MovementArbiter: bot cannot be null");

    TC_LOG_DEBUG("playerbot.movement.arbiter",
        "MovementArbiter: Created for bot {} (GUID: {})",
        _bot->GetName(), _bot->GetGUID().ToString());
}

MovementArbiter::~MovementArbiter()
{
    // Log final statistics
    if (_diagnosticLogging)
    {
        TC_LOG_INFO("playerbot.movement.arbiter",
            "MovementArbiter: Destroying arbiter for bot {} (GUID: {})\n{}",
            _bot->GetName(), _bot->GetGUID().ToString(),
            _statistics.ToString());
    }

    // Clear pending requests
    std::lock_guard<std::mutex> lock(_queueMutex);
    _pendingRequests.clear();
}

// ============================================================================
// MovementArbiter - Request Submission (Thread-Safe)
// ============================================================================

bool MovementArbiter::RequestMovement(MovementRequest const& request)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Increment total requests counter
    _statistics.totalRequests.fetch_add(1, std::memory_order_relaxed);

    // Update priority distribution statistics
    UpdatePriorityStatistics(request.GetPriority());

    // Fast Path: Duplicate detection (lock-free)
    if (_config.enableDeduplication && IsDuplicate(request))
    {
        _statistics.duplicateRequests.fetch_add(1, std::memory_order_relaxed);

        if (_diagnosticLogging)
            LogRequest(request, "DUPLICATE");

        return false;
    }

    // Fast Path: Priority filtering
    // If we have a current request and new request has lower priority, filter it
    if (_config.enablePriorityFiltering)
    {
        std::lock_guard<std::mutex> currentLock(_currentRequestMutex);
        if (_currentRequest.has_value())
        {
            // Only accept if new request has higher priority
            if (static_cast<uint8>(request.GetPriority()) <
                static_cast<uint8>(_currentRequest->GetPriority()))
            {
                _statistics.lowPriorityFiltered.fetch_add(1, std::memory_order_relaxed);

                if (_diagnosticLogging)
                    LogRequest(request, "FILTERED_LOW_PRIORITY");

                return false;
            }
        }
    }

    // Slow Path: Queue insertion (mutex required)
    {
        std::lock_guard<std::mutex> queueLock(_queueMutex);

        // Add to pending queue
        _pendingRequests.push_back(request);

        // Update queue size statistics
        uint32 queueSize = static_cast<uint32>(_pendingRequests.size());
        _statistics.currentQueueSize.store(queueSize, std::memory_order_relaxed);

if (!bot)

{

    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");

    return;

}

        uint32 maxQueue = _statistics.maxQueueSize.load(std::memory_order_relaxed);
        if (queueSize > maxQueue)
            _statistics.maxQueueSize.store(queueSize, std::memory_order_relaxed);

        // Warning if queue is getting large
        if (queueSize > _config.maxQueueSize)
        {
            TC_LOG_WARN("playerbot.movement.arbiter",
                "MovementArbiter: Large queue size ({}) for bot {} - possible performance issue",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                queueSize, _bot->GetName());
        }
    }

    // Update deduplication cache
    if (_config.enableDeduplication)
    {
        std::lock_guard<std::mutex> dedupLock(_deduplicationMutex);
        uint64 hash = request.GetSpatialTemporalHash();
        _recentRequests[hash] = GameTime::GetGameTimeMS();
    }

    // Calculate arbitration time
    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    _statistics.totalArbitrationTimeUs.fetch_add(durationUs, std::memory_order_relaxed);

    uint32 maxTime = _statistics.maxArbitrationTimeUs.load(std::memory_order_relaxed);
    if (static_cast<uint32>(durationUs) > maxTime)
        _statistics.maxArbitrationTimeUs.store(static_cast<uint32>(durationUs), std::memory_order_relaxed);

    if (_diagnosticLogging)
        LogRequest(request, "ACCEPTED");

    return true;
}

// ============================================================================
// MovementArbiter - Update (World Thread Only)
// ============================================================================

void MovementArbiter::Update(uint32 diff)
{
    // Update deduplication cache (remove expired entries)
    if (_config.enableDeduplication)
    {
        uint32 currentTime = GameTime::GetGameTimeMS();
        UpdateDeduplicationCache(currentTime);
    }

    // Check if we have pending requests
    std::unique_lock<std::mutex> queueLock(_queueMutex);
    if (_pendingRequests.empty())
        return;

    // Find highest-priority request
    // Sort by priority (descending) - highest priority first
    std::sort(_pendingRequests.begin(), _pendingRequests.end(),
        [](MovementRequest const& a, MovementRequest const& b)
        {
            return static_cast<uint8>(a.GetPriority()) > static_cast<uint8>(b.GetPriority());
        });

    // Get the winning request (highest priority)
    MovementRequest winningRequest = _pendingRequests.front();
    _pendingRequests.pop_front();

    // Update current queue size
    _statistics.currentQueueSize.store(static_cast<uint32>(_pendingRequests.size()), std::memory_order_relaxed);

    // Check if we should interrupt current movement
    bool shouldInterrupt = false;
    {
        std::lock_guard<std::mutex> currentLock(_currentRequestMutex);

        if (_currentRequest.has_value())
        {
            // Interrupt if:
            // 1. New request has higher priority
            // 2. Current request allows interruption
            if (static_cast<uint8>(winningRequest.GetPriority()) >
                static_cast<uint8>(_currentRequest->GetPriority()))
            {
                if (_currentRequest->CanBeInterrupted())
                {
                    shouldInterrupt = true;
                    _statistics.interruptedRequests.fetch_add(1, std::memory_order_relaxed);

                    if (_diagnosticLogging)
                    {
                        TC_LOG_DEBUG("playerbot.movement.arbiter",
                            "MovementArbiter: Interrupting current request (priority {}) with higher priority request (priority {})",
                            static_cast<int>(_currentRequest->GetPriority()),
                            static_cast<int>(winningRequest.GetPriority()));
                    }
                }
                else
                {
                    // Current request cannot be interrupted - skip this update
                    if (_diagnosticLogging)
                    {
                        TC_LOG_DEBUG("playerbot.movement.arbiter",
                            "MovementArbiter: Cannot interrupt current request (priority {}) - request marked as non-interruptible",
                            static_cast<int>(_currentRequest->GetPriority()));
                    }

                    // Put winning request back in queue for next update
                    _pendingRequests.push_front(winningRequest);
                    return;
                }
            }
            else
            {
                // New request has lower or equal priority - don't interrupt
                if (_diagnosticLogging)
                {
                    TC_LOG_DEBUG("playerbot.movement.arbiter",
                        "MovementArbiter: Not interrupting current request (priority {}) with lower/equal priority request (priority {})",
                        static_cast<int>(_currentRequest->GetPriority()),
                        static_cast<int>(winningRequest.GetPriority()));
                }

                // Filter out lower priority request
                _statistics.lowPriorityFiltered.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }

        // Update current request
        _currentRequest = winningRequest;
    }

    queueLock.unlock();

    // Execute the winning request
    ExecuteMovementRequest(winningRequest);

    // Update statistics
    _statistics.executedRequests.fetch_add(1, std::memory_order_relaxed);

    if (_diagnosticLogging)
        LogRequest(winningRequest, "EXECUTED");
}

// ============================================================================
// MovementArbiter - Movement Execution
// ============================================================================

void MovementArbiter::ExecuteMovementRequest(MovementRequest const& request)
{
    if (!_bot)
        return;

    MotionMaster* motionMaster = _bot->GetMotionMaster();
    if (!motionMaster)
        return;

    // CRITICAL FIX: Clear MotionMaster before executing ANY movement request
    // This prevents movement spam/cancellation that causes "teleporting" behavior
    // Reference: mod-playerbot MovementActions.cpp:244-248
    //
    // Why this is safe and necessary:
    // 1. MovementArbiter already handles interruption logic (line 244-298)
    // 2. We only reach here if we've decided to execute this movement
    // 3. Clearing ensures clean slate for new movement generator
    // 4. Prevents stacking of conflicting movement commands
    // 5. Matches mod-playerbot's proven approach
    motionMaster->Clear();

    // Map PlayerBot priority to TrinityCore priority
    TrinityCorePriority tcPriority = MovementPriorityMapper::Map(request.GetPriority());

    // Execute based on request type
    switch (request.GetType())
    {
        case MovementRequestType::POINT:
        {
            auto const& params = request.GetPointParams();

            // TrinityCore MovePoint API: uses MovementWalkRunSpeedSelectionMode, not MovementGeneratorMode
            motionMaster->MovePoint(
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                0,  // Movement ID
                params.targetPos,
                params.generatePath,
                params.finalOrient,
                params.speed,
                MovementWalkRunSpeedSelectionMode::Default,
                params.closeEnoughDistance);

            if (_diagnosticLogging)
            {
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    "MovementArbiter: Executing POINT movement to ({:.2f}, {:.2f}, {:.2f}) for bot {} - Priority: {} ({})",
                    params.targetPos.GetPositionX(),
                    params.targetPos.GetPositionY(),
                    params.targetPos.GetPositionZ(),
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName(),
                    MovementPriorityMapper::GetPriorityName(request.GetPriority()),
                    tcPriority.ToString());
            }
            break;
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        }

        case MovementRequestType::CHASE:
        {
            auto const& params = request.GetChaseParams();

            // PHASE 5D: Thread-safe spatial grid validation before ObjectAccessor
            // Units can be Creatures or Players - check existence first
            Unit* target = nullptr;

            if (SpatialGridQueryHelpers::EntityExists(_bot, params.targetGuid))
            {
                // Validated via spatial grid - safe to call ObjectAccessor on World Thread
                target = ObjectAccessor::GetUnit(*_bot, params.targetGuid);
                if (!target)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
                    return nullptr;
                }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
            }

            if (!target)
            {
                TC_LOG_WARN("playerbot.movement.arbiter",
                    "MovementArbiter: CHASE target not found (GUID: {}) for bot {}",
                    params.targetGuid.ToString(), _bot->GetName());
                return;
            }

            motionMaster->MoveChase(
                target,
                params.range.value_or(0.0f),
                params.angle);

            if (_diagnosticLogging)
            {
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    "MovementArbiter: Executing CHASE movement (target: {}) for bot {} - Priority: {} ({})",
                    if (!target)
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return nullptr;
                    }
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
                        return;
                    }
                    target->GetName(),
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName(),
                    MovementPriorityMapper::GetPriorityName(request.GetPriority()),
                    tcPriority.ToString());
            }
            break;
        }

        case MovementRequestType::FOLLOW:
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        {
            auto const& params = request.GetFollowParams();

            // PHASE 5D: Thread-safe spatial grid validation before ObjectAccessor
            // Units can be Creatures or Players - check existence first
            Unit* target = nullptr;

            if (SpatialGridQueryHelpers::EntityExists(_bot, params.targetGuid))
            {
                // Validated via spatial grid - safe to call ObjectAccessor on World Thread
                target = ObjectAccessor::GetUnit(*_bot, params.targetGuid);
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
            }

            if (!target)
            {
                TC_LOG_WARN("playerbot.movement.arbiter",
                    "MovementArbiter: FOLLOW target not found (GUID: {}) for bot {}",
                    params.targetGuid.ToString(), _bot->GetName());
                return;
            }

            motionMaster->MoveFollow(
                target,
                params.distance,
                params.angle,
                params.duration,
                false,  // ignoreTargetWalk
                tcPriority.slot);

            if (_diagnosticLogging)
            {
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return nullptr;
                    }
                    "MovementArbiter: Executing FOLLOW movement (target: {}, distance: {:.2f}) for bot {} - Priority: {} ({})",
                    target->GetName(),
                    params.distance,
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName(),
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    MovementPriorityMapper::GetPriorityName(request.GetPriority()),
                    tcPriority.ToString());
            }
            break;
        }

        case MovementRequestType::IDLE:
        {
            motionMaster->MoveIdle();

            if (_diagnosticLogging)
            {
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    "MovementArbiter: Executing IDLE movement for bot {}",
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName());
            }
            break;
        }

        case MovementRequestType::JUMP:
        {
            auto const& params = request.GetJumpParams();

            motionMaster->MoveJump(
                params.targetPos,
                params.speedXY,
                params.speedZ,
                params.eventId);

            if (_diagnosticLogging)
            {
                TC_LOG_DEBUG("playerbot.movement.arbiter",
                    "MovementArbiter: Executing JUMP movement to ({:.2f}, {:.2f}, {:.2f}) for bot {}",
                    params.targetPos.GetPositionX(),
                    params.targetPos.GetPositionY(),
                    params.targetPos.GetPositionZ(),
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName());
            }
            break;
        }

        default:
            TC_LOG_ERROR("playerbot.movement.arbiter",
                "MovementArbiter: Unknown movement type {} for bot {}",
                static_cast<int>(request.GetType()),
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return 0;
                    }
                    return;
                }
                _bot->GetName());
            break;
    }
}

// ============================================================================
// MovementArbiter - Deduplication
// ============================================================================

bool MovementArbiter::IsDuplicate(MovementRequest const& request) const
{
    std::lock_guard<std::mutex> lock(_deduplicationMutex);

    uint64 hash = request.GetSpatialTemporalHash();
    auto it = _recentRequests.find(hash);

    if (it == _recentRequests.end())
        return false;

if (!bot)

{

    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");

    return nullptr;

}

    // Check if within deduplication window
    uint32 currentTime = GameTime::GetGameTimeMS();
    uint32 lastRequestTime = it->second;

    if (currentTime - lastRequestTime < _config.minTimeBetweenDuplicatesMs)
        return true;

    return false;
}

void MovementArbiter::UpdateDeduplicationCache(uint32 currentTime)
{
    std::lock_guard<std::mutex> lock(_deduplicationMutex);

    // Remove entries older than deduplication window
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    auto it = _recentRequests.begin();
    while (it != _recentRequests.end())
    {
        if (currentTime - it->second > _config.deduplicationWindowMs)
            it = _recentRequests.erase(it);
        else
            ++it;
    }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
}

// ============================================================================
// MovementArbiter - Clear/Stop
// ============================================================================

void MovementArbiter::ClearPendingRequests()
{
    std::lock_guard<std::mutex> lock(_queueMutex);
    _pendingRequests.clear();
    _statistics.currentQueueSize.store(0, std::memory_order_relaxed);

    if (_diagnosticLogging)
    {
        TC_LOG_DEBUG("playerbot.movement.arbiter",
            "MovementArbiter: Cleared pending requests for bot {}",
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            _bot->GetName());
    }
}

void MovementArbiter::StopMovement()
{
    ClearPendingRequests();

    {
        std::lock_guard<std::mutex> lock(_currentRequestMutex);
        _currentRequest.reset();
    }

    if (_bot && _bot->GetMotionMaster())
        _bot->GetMotionMaster()->Clear();

    if (_diagnosticLogging)
    {
        TC_LOG_DEBUG("playerbot.movement.arbiter",
            "MovementArbiter: Stopped all movement for bot {}",
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            }
            _bot->GetName());
    }
}

// ============================================================================
// MovementArbiter - Statistics & Diagnostics
// ============================================================================

void MovementArbiter::ResetStatistics()
{
    _statistics.Reset();

    if (_diagnosticLogging)
    {
        TC_LOG_DEBUG("playerbot.movement.arbiter",
            "MovementArbiter: Reset statistics for bot {}",
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            _bot->GetName());
    }
}

std::string MovementArbiter::GetDiagnosticString() const
{
    std::ostringstream oss;

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    oss << "MovementArbiter Diagnostics for " << _bot->GetName() << ":\n";
    oss << "  Pending Requests: " << GetPendingRequestCount() << "\n";

    {
        std::lock_guard<std::mutex> lock(_currentRequestMutex);
        if (_currentRequest.has_value())
        {
            oss << "  Current Request: " << _currentRequest->ToString() << "\n";
        }
        else
        {
            oss << "  Current Request: None\n";
        }
    }

    oss << "\n" << _statistics.ToString();

    return oss.str();
}

void MovementArbiter::LogStatistics() const
{
    TC_LOG_INFO("playerbot.movement.arbiter",
        "{}",
        _statistics.ToString());
}

// ============================================================================
// MovementArbiter - Configuration
// ============================================================================

MovementArbiterConfig MovementArbiter::GetConfig() const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    return _config;
}

void MovementArbiter::SetConfig(MovementArbiterConfig const& config)
{
    std::lock_guard<std::mutex> lock(_configMutex);
    _config = config;
    _diagnosticLogging.store(config.enableDiagnosticLogging, std::memory_order_relaxed);

    TC_LOG_DEBUG("playerbot.movement.arbiter",
        "MovementArbiter: Updated configuration for bot {}",
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        _bot->GetName());
}

void MovementArbiter::SetDiagnosticLogging(bool enable)
{
    _diagnosticLogging.store(enable, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _config.enableDiagnosticLogging = enable;
    }
}

// ============================================================================
// MovementArbiter - Query Current State
// ============================================================================

Optional<MovementRequest> MovementArbiter::GetCurrentRequest() const
{
    std::lock_guard<std::mutex> lock(_currentRequestMutex);
    return _currentRequest;
}

uint32 MovementArbiter::GetPendingRequestCount() const
{
    return _statistics.currentQueueSize.load(std::memory_order_relaxed);
}

// ============================================================================
// MovementArbiter - Internal Helpers
// ============================================================================

void MovementArbiter::LogRequest(MovementRequest const& request, std::string const& action) const
{
    TC_LOG_DEBUG("playerbot.movement.arbiter",
        "MovementArbiter: {} - {}", action, request.ToString());
}

void MovementArbiter::UpdatePriorityStatistics(PlayerBotMovementPriority priority)
{
    uint8 value = static_cast<uint8>(priority);

    if (value >= 240)
        _statistics.criticalRequests.fetch_add(1, std::memory_order_relaxed);
    else if (value >= 200)
        _statistics.veryHighRequests.fetch_add(1, std::memory_order_relaxed);
    else if (value >= 150)
        _statistics.highRequests.fetch_add(1, std::memory_order_relaxed);
    else if (value >= 100)
        _statistics.mediumRequests.fetch_add(1, std::memory_order_relaxed);
    else if (value >= 50)
        _statistics.lowRequests.fetch_add(1, std::memory_order_relaxed);
    else
        _statistics.minimalRequests.fetch_add(1, std::memory_order_relaxed);
}

} // namespace Playerbot
