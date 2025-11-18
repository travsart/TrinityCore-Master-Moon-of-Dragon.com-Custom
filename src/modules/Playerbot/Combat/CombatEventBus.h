/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_COMBAT_EVENT_BUS_H
#define PLAYERBOT_COMBAT_EVENT_BUS_H

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "CombatEvents.h"
#include "Core/DI/Interfaces/ICombatEventBus.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>
#include <atomic>

// Forward declarations
class Player;
class Unit;
class Spell;

namespace Playerbot
{

class BotAI;

/**
 * @class CombatEventBus
 * @brief Central event distribution system for all combat-related events
 *
 * Performance Targets:
 * - Event publishing: <5 microseconds
 * - Event processing: <500 microseconds per event
 * - Batch processing: 100 events in <5ms
 */
class TC_GAME_API CombatEventBus final : public ICombatEventBus
{
public:
    static CombatEventBus* instance();

    // Event publishing
    bool PublishEvent(CombatEvent const& event) override;

    // Subscription
    bool Subscribe(BotAI* subscriber, std::vector<CombatEventType> const& types) override;
    bool SubscribeAll(BotAI* subscriber) override;
    void Unsubscribe(BotAI* subscriber) override;

    // Event processing
    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 100) override;
    uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff) override;
    void ClearUnitEvents(ObjectGuid unitGuid) override;

    // Statistics
    struct Statistics
    {
        std::atomic<uint64_t> totalEventsPublished{0};
        std::atomic<uint64_t> totalEventsProcessed{0};
        std::atomic<uint64_t> totalEventsDropped{0};
        std::atomic<uint64_t> totalDeliveries{0};
        std::atomic<uint64_t> averageProcessingTimeUs{0};
        std::atomic<uint32_t> peakQueueSize{0};
        std::chrono::steady_clock::time_point startTime;

        void Reset();
        std::string ToString() const;
    };

    Statistics const& GetStatistics() const { return _stats; }
    void ResetStatistics() { _stats.Reset(); }

    // Configuration
    void SetMaxQueueSize(uint32 size) override { _maxQueueSize = size; }
    void SetEventTTL(uint32 ttlMs) override { _eventTTLMs = ttlMs; }
    void SetBatchSize(uint32 size) override { _batchSize = size; }

    uint32 GetMaxQueueSize() const override { return _maxQueueSize; }
    uint32 GetEventTTL() const override { return _eventTTLMs; }
    uint32 GetBatchSize() const override { return _batchSize; }

    // Debugging
    void DumpSubscribers() const override;
    void DumpEventQueue() const override;
    std::vector<CombatEvent> GetQueueSnapshot() const override;

private:
    CombatEventBus();
    ~CombatEventBus();

    CombatEventBus(CombatEventBus const&) = delete;
    CombatEventBus& operator=(CombatEventBus const&) = delete;

    bool DeliverEvent(BotAI* subscriber, CombatEvent const& event);
    bool ValidateEvent(CombatEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(CombatEvent const& event, std::string const& action) const;

private:
    std::priority_queue<CombatEvent> _eventQueue;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TARGET_SELECTOR> _queueMutex;

    std::unordered_map<CombatEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;
    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::TARGET_SELECTOR> _subscriberMutex;

    std::atomic<uint32_t> _maxQueueSize{10000};
    std::atomic<uint32_t> _eventTTLMs{5000};  // Combat events expire faster (5s vs 30s)
    std::atomic<uint32_t> _batchSize{100};

    Statistics _stats;

    uint32 _cleanupTimer{0};
    uint32 _metricsUpdateTimer{0};

    static constexpr uint32 CLEANUP_INTERVAL = 2000;        // 2 seconds (faster than group)
    static constexpr uint32 METRICS_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 100;
};

} // namespace Playerbot

#endif // PLAYERBOT_COMBAT_EVENT_BUS_H
