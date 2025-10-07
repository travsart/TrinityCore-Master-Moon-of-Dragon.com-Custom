/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "EventDispatcher.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>

namespace Playerbot
{
namespace Events
{

EventDispatcher::EventDispatcher(size_t initialQueueSize)
    : _enabled(true)
    , _totalEventsDispatched(0)
    , _totalEventsProcessed(0)
    , _totalProcessingTimeMs(0)
    , _droppedEvents(0)
{
    // Note: std::deque does not have reserve(), initial size hint ignored
    TC_LOG_INFO("module.playerbot", "EventDispatcher: Initialized with queue size hint {}", initialQueueSize);
}

EventDispatcher::~EventDispatcher()
{
    // Clear queue on shutdown
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _eventQueue.clear();
    }

    TC_LOG_INFO("module.playerbot", "EventDispatcher: Shutdown complete (Dispatched: {}, Processed: {}, Dropped: {})",
        _totalEventsDispatched.load(), _totalEventsProcessed.load(), _droppedEvents.load());
}

void EventDispatcher::Subscribe(StateMachine::EventType eventType, IManagerBase* manager)
{
    if (!manager)
    {
        TC_LOG_ERROR("module.playerbot", "EventDispatcher::Subscribe: Null manager pointer!");
        return;
    }

    std::lock_guard<std::mutex> lock(_subscriptionMutex);

    auto& subscribers = _subscriptions[eventType];

    // Check if already subscribed
    auto it = std::find(subscribers.begin(), subscribers.end(), manager);
    if (it != subscribers.end())
    {
        TC_LOG_WARN("module.playerbot", "EventDispatcher::Subscribe: Manager {} already subscribed to event type {}",
            manager->GetManagerId(), static_cast<uint16_t>(eventType));
        return;
    }

    subscribers.push_back(manager);

    TC_LOG_DEBUG("module.playerbot", "EventDispatcher::Subscribe: Manager {} subscribed to event type {} (total subscribers: {})",
        manager->GetManagerId(), static_cast<uint16_t>(eventType), subscribers.size());
}

void EventDispatcher::Unsubscribe(StateMachine::EventType eventType, IManagerBase* manager)
{
    if (!manager)
        return;

    std::lock_guard<std::mutex> lock(_subscriptionMutex);

    auto subIt = _subscriptions.find(eventType);
    if (subIt == _subscriptions.end())
        return;

    auto& subscribers = subIt->second;
    subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), manager), subscribers.end());

    TC_LOG_DEBUG("module.playerbot", "EventDispatcher::Unsubscribe: Manager {} unsubscribed from event type {}",
        manager->GetManagerId(), static_cast<uint16_t>(eventType));
}

void EventDispatcher::UnsubscribeAll(IManagerBase* manager)
{
    if (!manager)
        return;

    std::lock_guard<std::mutex> lock(_subscriptionMutex);

    for (auto& pair : _subscriptions)
    {
        auto& subscribers = pair.second;
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), manager), subscribers.end());
    }

    TC_LOG_DEBUG("module.playerbot", "EventDispatcher::UnsubscribeAll: Manager {} unsubscribed from all events",
        manager->GetManagerId());
}

void EventDispatcher::Dispatch(BotEvent const& event)
{
    if (!_enabled.load(std::memory_order_acquire))
        return;

    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _eventQueue.push_back(event);
    }

    _totalEventsDispatched.fetch_add(1, std::memory_order_relaxed);
}

void EventDispatcher::Dispatch(BotEvent&& event)
{
    if (!_enabled.load(std::memory_order_acquire))
        return;

    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _eventQueue.push_back(std::move(event));
    }

    _totalEventsDispatched.fetch_add(1, std::memory_order_relaxed);
}

uint32 EventDispatcher::ProcessQueue(uint32 maxEvents)
{
    if (maxEvents == 0)
        return 0;

    uint32 startTime = getMSTime();
    uint32 eventsProcessed = 0;

    // Dequeue events into local buffer
    std::vector<BotEvent> events;
    {
        std::lock_guard<std::mutex> lock(_queueMutex);

        size_t processCount = std::min<size_t>(maxEvents, _eventQueue.size());
        if (processCount == 0)
            return 0;

        events.reserve(processCount);

        auto it = _eventQueue.begin();
        auto end = it + processCount;
        events.assign(it, end);
        _eventQueue.erase(it, end);
    }

    // Process events (no lock held)
    for (auto const& event : events)
    {
        RouteEvent(event);
        ++eventsProcessed;
    }

    _totalEventsProcessed.fetch_add(eventsProcessed, std::memory_order_relaxed);

    uint32 processingTime = getMSTimeDiff(startTime, getMSTime());
    _totalProcessingTimeMs.fetch_add(processingTime, std::memory_order_relaxed);

    return eventsProcessed;
}

void EventDispatcher::RouteEvent(BotEvent const& event)
{
    std::lock_guard<std::mutex> lock(_subscriptionMutex);

    auto it = _subscriptions.find(event.type);
    if (it == _subscriptions.end())
        return;

    auto const& subscribers = it->second;
    for (auto* manager : subscribers)
    {
        if (manager && manager->IsActive())
        {
            try
            {
                manager->OnEvent(event);
            }
            catch (std::exception const& ex)
            {
                TC_LOG_ERROR("module.playerbot", "EventDispatcher::RouteEvent: Exception in manager {} handling event {}: {}",
                    manager->GetManagerId(), event.eventId, ex.what());
            }
            catch (...)
            {
                TC_LOG_ERROR("module.playerbot", "EventDispatcher::RouteEvent: Unknown exception in manager {} handling event {}",
                    manager->GetManagerId(), event.eventId);
            }
        }
    }
}

size_t EventDispatcher::GetQueueSize() const
{
    std::lock_guard<std::mutex> lock(_queueMutex);
    return _eventQueue.size();
}

size_t EventDispatcher::GetSubscriberCount(StateMachine::EventType eventType) const
{
    std::lock_guard<std::mutex> lock(_subscriptionMutex);

    auto it = _subscriptions.find(eventType);
    return (it != _subscriptions.end()) ? it->second.size() : 0;
}

void EventDispatcher::ClearQueue()
{
    std::lock_guard<std::mutex> lock(_queueMutex);
    _eventQueue.clear();

    TC_LOG_INFO("module.playerbot", "EventDispatcher::ClearQueue: Event queue cleared");
}

void EventDispatcher::SetEnabled(bool enabled)
{
    _enabled.store(enabled, std::memory_order_release);

    TC_LOG_INFO("module.playerbot", "EventDispatcher: Event dispatching {}",
        enabled ? "enabled" : "disabled");
}

bool EventDispatcher::IsEnabled() const
{
    return _enabled.load(std::memory_order_acquire);
}

EventDispatcher::PerformanceMetrics EventDispatcher::GetMetrics() const
{
    PerformanceMetrics metrics;

    metrics.totalEventsDispatched = _totalEventsDispatched.load(std::memory_order_relaxed);
    metrics.totalEventsProcessed = _totalEventsProcessed.load(std::memory_order_relaxed);
    metrics.totalProcessingTimeMs = _totalProcessingTimeMs.load(std::memory_order_relaxed);
    metrics.currentQueueSize = static_cast<uint32>(GetQueueSize());
    metrics.droppedEvents = _droppedEvents.load(std::memory_order_relaxed);

    if (metrics.totalEventsProcessed > 0)
        metrics.averageProcessingTimeMs = static_cast<float>(metrics.totalProcessingTimeMs) / metrics.totalEventsProcessed;
    else
        metrics.averageProcessingTimeMs = 0.0f;

    return metrics;
}

void EventDispatcher::ResetMetrics()
{
    _totalEventsDispatched.store(0, std::memory_order_relaxed);
    _totalEventsProcessed.store(0, std::memory_order_relaxed);
    _totalProcessingTimeMs.store(0, std::memory_order_relaxed);
    _droppedEvents.store(0, std::memory_order_relaxed);

    TC_LOG_INFO("module.playerbot", "EventDispatcher: Performance metrics reset");
}

} // namespace Events
} // namespace Playerbot
