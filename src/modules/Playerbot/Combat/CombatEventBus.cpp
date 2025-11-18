/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CombatEventBus.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"

namespace Playerbot
{

CombatEventBus::CombatEventBus()
{
    _stats.startTime = std::chrono::steady_clock::now();
    TC_LOG_INFO("module.playerbot.combat", "CombatEventBus initialized");
}

CombatEventBus::~CombatEventBus()
{
    TC_LOG_INFO("module.playerbot.combat", "CombatEventBus shutting down - Stats: {}", _stats.ToString());
}

CombatEventBus* CombatEventBus::instance()
{
    static CombatEventBus instance;
    return &instance;
}

bool CombatEventBus::PublishEvent(CombatEvent const& event)
{
    if (!ValidateEvent(event))
    {
        _stats.totalEventsDropped++;
        return false;
    }

    {
        std::lock_guard lock(_queueMutex);
        if (_eventQueue.size() >= _maxQueueSize)
        {
            _stats.totalEventsDropped++;
            return false;
        }
        _eventQueue.push(event);
    }

    _stats.totalEventsPublished++;
    LogEvent(event, "Published");
    return true;
}

bool CombatEventBus::Subscribe(BotAI* subscriber, std::vector<CombatEventType> const& types)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);
    for (CombatEventType type : types)
        _subscribers[type].push_back(subscriber);
    return true;
}

bool CombatEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return false;

    std::lock_guard lock(_subscriberMutex);
    _globalSubscribers.push_back(subscriber);
    return true;
}

void CombatEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard lock(_subscriberMutex);
    for (auto& [type, subscriberList] : _subscribers)
    {
        subscriberList.erase(
            std::remove(subscriberList.begin(), subscriberList.end(), subscriber),
            subscriberList.end()
        );
    }
    _globalSubscribers.erase(
        std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber),
        _globalSubscribers.end()
    );
}

uint32 CombatEventBus::ProcessEvents(uint32 diff, uint32 maxEvents)
{
    _cleanupTimer += diff;
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredEvents();
        _cleanupTimer = 0;
    }

    uint32 processedCount = 0;
    std::vector<CombatEvent> eventsToProcess;

    {
        std::lock_guard lock(_queueMutex);
        while (!_eventQueue.empty() && (maxEvents == 0 || processedCount < maxEvents))
        {
            CombatEvent event = _eventQueue.top();
            _eventQueue.pop();

            if (event.IsExpired())
            {
                _stats.totalEventsDropped++;
                continue;
            }

            eventsToProcess.push_back(event);
            processedCount++;
        }
    }

    for (CombatEvent const& event : eventsToProcess)
    {
        std::vector<BotAI*> subscribers;
        std::vector<BotAI*> globalSubs;

        {
            std::lock_guard lock(_subscriberMutex);
            auto it = _subscribers.find(event.type);
            if (it != _subscribers.end())
                subscribers = it->second;
            globalSubs = _globalSubscribers;
        }

        for (BotAI* subscriber : subscribers)
        {
            if (DeliverEvent(subscriber, event))
                _stats.totalDeliveries++;
        }

        for (BotAI* subscriber : globalSubs)
        {
            if (DeliverEvent(subscriber, event))
                _stats.totalDeliveries++;
        }

        _stats.totalEventsProcessed++;
    }

    return processedCount;
}

uint32 CombatEventBus::ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff)
{
    // Simplified - just process all events
    return ProcessEvents(diff, 0);
}

void CombatEventBus::ClearUnitEvents(ObjectGuid unitGuid)
{
    std::lock_guard lock(_queueMutex);
    std::vector<CombatEvent> remainingEvents;

    while (!_eventQueue.empty())
    {
        CombatEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (event.casterGuid != unitGuid && event.targetGuid != unitGuid)
            remainingEvents.push_back(event);
        else
            _stats.totalEventsDropped++;
    }

    for (CombatEvent const& event : remainingEvents)
        _eventQueue.push(event);
}

bool CombatEventBus::DeliverEvent(BotAI* subscriber, CombatEvent const& event)
{
    if (!subscriber)
        return false;

    try
    {
        // Phase 4: Call virtual event handler on BotAI
        subscriber->OnCombatEvent(event);
        TC_LOG_TRACE("module.playerbot.combat", "CombatEventBus: Delivered event to subscriber");
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool CombatEventBus::ValidateEvent(CombatEvent const& event) const
{
    return event.IsValid() && !event.IsExpired();
}

uint32 CombatEventBus::CleanupExpiredEvents()
{
    std::lock_guard lock(_queueMutex);
    uint32 cleanedCount = 0;
    std::vector<CombatEvent> validEvents;

    while (!_eventQueue.empty())
    {
        CombatEvent event = _eventQueue.top();
        _eventQueue.pop();

        if (!event.IsExpired())
            validEvents.push_back(event);
        else
            cleanedCount++;
    }

    for (CombatEvent const& event : validEvents)
        _eventQueue.push(event);

    return cleanedCount;
}

void CombatEventBus::UpdateMetrics(std::chrono::microseconds processingTime)
{
    uint64_t currentAvg = _stats.averageProcessingTimeUs.load();
    uint64_t newTime = processingTime.count();
    uint64_t newAvg = (currentAvg * 9 + newTime) / 10;
    _stats.averageProcessingTimeUs.store(newAvg);
}

void CombatEventBus::LogEvent(CombatEvent const& event, std::string const& action) const
{
    TC_LOG_TRACE("module.playerbot.combat", "CombatEventBus: {} event - {}", action, event.ToString());
}

void CombatEventBus::DumpSubscribers() const
{
    std::lock_guard lock(_subscriberMutex);
    TC_LOG_INFO("module.playerbot.combat", "=== CombatEventBus Subscribers Dump ===");
    TC_LOG_INFO("module.playerbot.combat", "Global subscribers: {}", _globalSubscribers.size());
}

void CombatEventBus::DumpEventQueue() const
{
    std::lock_guard lock(_queueMutex);
    TC_LOG_INFO("module.playerbot.combat", "=== CombatEventBus Queue Dump ===");
    TC_LOG_INFO("module.playerbot.combat", "Queue size: {}", _eventQueue.size());
}

std::vector<CombatEvent> CombatEventBus::GetQueueSnapshot() const
{
    std::lock_guard lock(_queueMutex);
    std::vector<CombatEvent> snapshot;
    std::priority_queue<CombatEvent> tempQueue = _eventQueue;

    while (!tempQueue.empty())
    {
        snapshot.push_back(tempQueue.top());
        tempQueue.pop();
    }

    return snapshot;
}

void CombatEventBus::Statistics::Reset()
{
    totalEventsPublished.store(0);
    totalEventsProcessed.store(0);
    totalEventsDropped.store(0);
    totalDeliveries.store(0);
    averageProcessingTimeUs.store(0);
    peakQueueSize.store(0);
    startTime = std::chrono::steady_clock::now();
}

std::string CombatEventBus::Statistics::ToString() const
{
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);

    std::ostringstream oss;
    oss << "Published: " << totalEventsPublished.load()
        << ", Processed: " << totalEventsProcessed.load()
        << ", Dropped: " << totalEventsDropped.load()
        << ", Deliveries: " << totalDeliveries.load()
        << ", Avg Processing: " << averageProcessingTimeUs.load() << "μs"
        << ", Peak Queue: " << peakQueueSize.load()
        << ", Uptime: " << uptime.count() << "s";
    return oss.str();
}

} // namespace Playerbot
