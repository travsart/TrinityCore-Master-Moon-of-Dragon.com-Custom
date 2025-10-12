/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_LOOT_EVENT_BUS_H
#define PLAYERBOT_LOOT_EVENT_BUS_H

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>

namespace Playerbot
{

class BotAI;

enum class LootEventType : uint8
{
    LOOT_WINDOW_OPENED = 0,
    LOOT_WINDOW_CLOSED,
    LOOT_ITEM_RECEIVED,
    LOOT_MONEY_RECEIVED,
    LOOT_REMOVED,
    LOOT_SLOT_CHANGED,
    LOOT_ROLL_STARTED,
    LOOT_ROLL_CAST,
    LOOT_ROLL_WON,
    LOOT_ALL_PASSED,
    MASTER_LOOT_LIST,
    MAX_LOOT_EVENT
};

enum class LootEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

enum class LootType : uint8
{
    CORPSE = 0,
    PICKPOCKETING,
    FISHING,
    DISENCHANTING,
    SKINNING,
    PROSPECTING,
    MILLING,
    ITEM,
    MAIL,
    INSIGNIA
};

struct LootEvent
{
    LootEventType type;
    LootEventPriority priority;
    ObjectGuid looterGuid;
    ObjectGuid itemGuid;
    uint32 itemEntry;
    uint32 itemCount;
    LootType lootType;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;

    // Helper constructors
    static LootEvent ItemLooted(ObjectGuid looter, ObjectGuid item, uint32 entry, uint32 count, LootType type);
    static LootEvent LootRollStarted(ObjectGuid item, uint32 entry);
    static LootEvent LootRollWon(ObjectGuid winner, ObjectGuid item, uint32 entry);

    // Priority comparison for priority queue
    bool operator<(LootEvent const& other) const
    {
        return priority > other.priority;
    }
};

class TC_GAME_API LootEventBus
{
public:
    static LootEventBus* instance();

    // Event publishing
    bool PublishEvent(LootEvent const& event);

    // Subscription management
    bool Subscribe(BotAI* subscriber, std::vector<LootEventType> const& types);
    bool SubscribeAll(BotAI* subscriber);
    void Unsubscribe(BotAI* subscriber);

    // Event processing
    uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 0);
    uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff);
    void ClearUnitEvents(ObjectGuid unitGuid);

    // Status queries
    uint32 GetPendingEventCount() const;
    uint32 GetSubscriberCount() const;

    // Diagnostics
    void DumpSubscribers() const;
    void DumpEventQueue() const;
    std::vector<LootEvent> GetQueueSnapshot() const;

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

private:
    LootEventBus();
    ~LootEventBus();

    // Event delivery
    bool DeliverEvent(BotAI* subscriber, LootEvent const& event);
    bool ValidateEvent(LootEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(LootEvent const& event, std::string const& action) const;

    // Event queue
    std::priority_queue<LootEvent> _eventQueue;
    mutable std::mutex _queueMutex;

    // Subscriber management
    std::unordered_map<LootEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;
    mutable std::mutex _subscriberMutex;

    // Configuration
    static constexpr uint32 MAX_QUEUE_SIZE = 10000;
    static constexpr uint32 CLEANUP_INTERVAL = 30000;
    static constexpr uint32 MAX_SUBSCRIBERS_PER_EVENT = 5000;

    // Timers
    uint32 _cleanupTimer = 0;
    uint32 _metricsUpdateTimer = 0;

    // Statistics
    Statistics _stats;
    uint32 _maxQueueSize = MAX_QUEUE_SIZE;
};

} // namespace Playerbot

#endif // PLAYERBOT_LOOT_EVENT_BUS_H

