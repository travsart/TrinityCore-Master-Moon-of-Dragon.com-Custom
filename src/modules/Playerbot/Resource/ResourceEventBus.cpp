/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ResourceEventBus.h"
#include "BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// Factory methods
ResourceEvent ResourceEvent::HealthUpdate(ObjectGuid unitGuid, ObjectGuid playerGuid, uint32 health, uint32 maxHealth)
{
    ResourceEvent event;
    event.type = ResourceEventType::HEALTH_UPDATE;
    event.unitGuid = unitGuid;
    event.playerGuid = playerGuid;
    event.health = health;
    event.maxHealth = maxHealth;
    event.power = 0;
    event.maxPower = 0;
    event.powerType = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ResourceEvent ResourceEvent::PowerUpdate(ObjectGuid unitGuid, ObjectGuid playerGuid, int32 power, int32 maxPower, uint8 powerType)
{
    ResourceEvent event;
    event.type = ResourceEventType::POWER_UPDATE;
    event.unitGuid = unitGuid;
    event.playerGuid = playerGuid;
    event.health = 0;
    event.maxHealth = 0;
    event.power = power;
    event.maxPower = maxPower;
    event.powerType = powerType;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ResourceEvent ResourceEvent::BreakTarget(ObjectGuid unitGuid, ObjectGuid playerGuid)
{
    ResourceEvent event;
    event.type = ResourceEventType::BREAK_TARGET;
    event.unitGuid = unitGuid;
    event.playerGuid = playerGuid;
    event.health = 0;
    event.maxHealth = 0;
    event.power = 0;
    event.maxPower = 0;
    event.powerType = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool ResourceEvent::IsValid() const
{
    if (type >= ResourceEventType::MAX_RESOURCE_EVENT)
        return false;

    if (unitGuid.IsEmpty())
        return false;

    switch (type)
    {
        case ResourceEventType::HEALTH_UPDATE:
            return maxHealth > 0;
        case ResourceEventType::POWER_UPDATE:
            return true; // Power can be 0 or negative for some types
        case ResourceEventType::BREAK_TARGET:
            return true;
        default:
            return false;
    }
}

std::string ResourceEvent::ToString() const
{
    std::ostringstream oss;
    oss << "ResourceEvent[";

    switch (type)
    {
        case ResourceEventType::HEALTH_UPDATE:
            oss << "HEALTH_UPDATE, " << health << "/" << maxHealth << " (" << GetHealthPercent() << "%)";
            break;
        case ResourceEventType::POWER_UPDATE:
            oss << "POWER_UPDATE, " << power << "/" << maxPower << " (" << GetPowerPercent() << "%), type=" << uint32(powerType);
            break;
        case ResourceEventType::BREAK_TARGET:
            oss << "BREAK_TARGET";
            break;
        default:
            oss << "UNKNOWN";
    }

    oss << ", unit=" << unitGuid.ToString() << ", player=" << playerGuid.ToString() << "]";
    return oss.str();
}

// ResourceEventBus implementation
ResourceEventBus* ResourceEventBus::instance()
{
    static ResourceEventBus instance;
    return &instance;
}

bool ResourceEventBus::PublishEvent(ResourceEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.events", "ResourceEventBus: Invalid event rejected: {}", event.ToString());
        return false;
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(_subscriberMutex);
        ++_eventCounts[event.type];
        ++_totalEventsPublished;
    }

    // Deliver to subscribers
    DeliverEvent(event);

    TC_LOG_TRACE("playerbot.events", "ResourceEventBus: Published event: {}", event.ToString());
    return true;
}

void ResourceEventBus::Subscribe(BotAI* subscriber, std::vector<ResourceEventType> const& types)
{
    if (!subscriber)
        return;

    std::lock_guard<std::mutex> lock(_subscriberMutex);

    for (auto type : types)
    {
        auto& typeSubscribers = _subscribers[type];
        if (std::find(typeSubscribers.begin(), typeSubscribers.end(), subscriber) == typeSubscribers.end())
        {
            typeSubscribers.push_back(subscriber);
            TC_LOG_DEBUG("playerbot.events", "ResourceEventBus: Subscriber {} registered for type {}",
                static_cast<void*>(subscriber), uint32(type));
        }
    }
}

void ResourceEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard<std::mutex> lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot.events", "ResourceEventBus: Subscriber {} registered for ALL events",
            static_cast<void*>(subscriber));
    }
}

void ResourceEventBus::Unsubscribe(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard<std::mutex> lock(_subscriberMutex);

    // Remove from type-specific subscriptions
    for (auto& [type, subscribers] : _subscribers)
    {
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), subscriber), subscribers.end());
    }

    // Remove from global subscriptions
    _globalSubscribers.erase(std::remove(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber),
        _globalSubscribers.end());

    TC_LOG_DEBUG("playerbot.events", "ResourceEventBus: Subscriber {} unregistered", static_cast<void*>(subscriber));
}

uint32 ResourceEventBus::SubscribeCallback(EventHandler handler, std::vector<ResourceEventType> const& types)
{
    if (!handler)
        return 0;

    std::lock_guard<std::mutex> lock(_subscriberMutex);

    CallbackSubscription sub;
    sub.id = _nextCallbackId++;
    sub.handler = std::move(handler);
    sub.types = types;

    _callbackSubscriptions.push_back(std::move(sub));

    TC_LOG_DEBUG("playerbot.events", "ResourceEventBus: Callback {} registered for {} types",
        sub.id, types.size());

    return sub.id;
}

void ResourceEventBus::UnsubscribeCallback(uint32 subscriptionId)
{
    std::lock_guard<std::mutex> lock(_subscriberMutex);

    _callbackSubscriptions.erase(
        std::remove_if(_callbackSubscriptions.begin(), _callbackSubscriptions.end(),
            [subscriptionId](CallbackSubscription const& sub) { return sub.id == subscriptionId; }),
        _callbackSubscriptions.end());

    TC_LOG_DEBUG("playerbot.events", "ResourceEventBus: Callback {} unregistered", subscriptionId);
}

uint64 ResourceEventBus::GetEventCount(ResourceEventType type) const
{
    std::lock_guard<std::mutex> lock(_subscriberMutex);
    auto it = _eventCounts.find(type);
    return it != _eventCounts.end() ? it->second : 0;
}

void ResourceEventBus::DeliverEvent(ResourceEvent const& event)
{
    std::lock_guard<std::mutex> lock(_subscriberMutex);

    // Deliver to type-specific subscribers
    auto it = _subscribers.find(event.type);
    if (it != _subscribers.end())
    {
        for (auto subscriber : it->second)
        {
            if (subscriber)
                subscriber->OnResourceEvent(event);
        }
    }

    // Deliver to global subscribers
    for (auto subscriber : _globalSubscribers)
    {
        if (subscriber)
            subscriber->OnResourceEvent(event);
    }

    // Deliver to callback subscriptions
    for (auto const& sub : _callbackSubscriptions)
    {
        if (sub.types.empty() || std::find(sub.types.begin(), sub.types.end(), event.type) != sub.types.end())
        {
            sub.handler(event);
        }
    }
}

} // namespace Playerbot
