/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_QUEST_EVENT_BUS_H
#define PLAYERBOT_QUEST_EVENT_BUS_H

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

enum class QuestEventType : uint8
{
    QUEST_GIVER_STATUS = 0,
    QUEST_LIST_RECEIVED,
    QUEST_DETAILS_RECEIVED,
    QUEST_REQUEST_ITEMS,
    QUEST_OFFER_REWARD,
    QUEST_COMPLETED,
    QUEST_FAILED,
    QUEST_CREDIT_ADDED,
    QUEST_OBJECTIVE_COMPLETE,
    QUEST_UPDATE_FAILED,
    QUEST_CONFIRM_ACCEPT,
    QUEST_POI_RECEIVED,
    MAX_QUEST_EVENT
};

enum class QuestEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

enum class QuestState : uint8
{
    NONE = 0,
    COMPLETE,
    UNAVAILABLE,
    INCOMPLETE,
    AVAILABLE,
    FAILED
};

struct QuestEvent
{
    QuestEventType type;
    QuestEventPriority priority;
    ObjectGuid playerGuid;
    uint32 questId;
    uint32 objectiveId;
    int32 objectiveCount;
    QuestState state;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point expiryTime;

    bool IsValid() const;
    bool IsExpired() const;
    std::string ToString() const;

    // Helper constructors
    static QuestEvent QuestAccepted(ObjectGuid player, uint32 questId);
    static QuestEvent QuestCompleted(ObjectGuid player, uint32 questId);
    static QuestEvent ObjectiveProgress(ObjectGuid player, uint32 questId, uint32 objId, int32 count);

    // Priority comparison for priority queue
    bool operator<(QuestEvent const& other) const
    {
        return priority > other.priority;
    }
};

class TC_GAME_API QuestEventBus
{
public:
    static QuestEventBus* instance();

    // Event publishing
    bool PublishEvent(QuestEvent const& event);

    // Subscription management
    bool Subscribe(BotAI* subscriber, std::vector<QuestEventType> const& types);
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
    std::vector<QuestEvent> GetQueueSnapshot() const;

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
    QuestEventBus();
    ~QuestEventBus();

    // Event delivery
    bool DeliverEvent(BotAI* subscriber, QuestEvent const& event);
    bool ValidateEvent(QuestEvent const& event) const;
    uint32 CleanupExpiredEvents();
    void UpdateMetrics(std::chrono::microseconds processingTime);
    void LogEvent(QuestEvent const& event, std::string const& action) const;

    // Event queue
    std::priority_queue<QuestEvent> _eventQueue;
    mutable std::mutex _queueMutex;

    // Subscriber management
    std::unordered_map<QuestEventType, std::vector<BotAI*>> _subscribers;
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

#endif // PLAYERBOT_QUEST_EVENT_BUS_H

