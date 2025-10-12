/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_RESOURCE_EVENT_BUS_H
#define PLAYERBOT_RESOURCE_EVENT_BUS_H

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Playerbot
{

class BotAI;

enum class ResourceEventType : uint8
{
    HEALTH_UPDATE = 0,
    POWER_UPDATE,
    BREAK_TARGET,
    MAX_RESOURCE_EVENT
};

struct ResourceEvent
{
    ResourceEventType type;
    ObjectGuid unitGuid;        // Unit whose resources changed
    ObjectGuid playerGuid;      // Player (bot) observing the change
    uint32 health;
    uint32 maxHealth;
    int32 power;                // Can be negative for some power types
    int32 maxPower;
    uint8 powerType;            // Powers enum (Mana, Rage, Energy, etc.)
    std::chrono::steady_clock::time_point timestamp;

    // Factory methods for type-safe event creation
    static ResourceEvent HealthUpdate(ObjectGuid unitGuid, ObjectGuid playerGuid, uint32 health, uint32 maxHealth);
    static ResourceEvent PowerUpdate(ObjectGuid unitGuid, ObjectGuid playerGuid, int32 power, int32 maxPower, uint8 powerType);
    static ResourceEvent BreakTarget(ObjectGuid unitGuid, ObjectGuid playerGuid);

    bool IsValid() const;
    std::string ToString() const;

    // Helper methods
    float GetHealthPercent() const { return maxHealth > 0 ? (health * 100.0f) / maxHealth : 0.0f; }
    float GetPowerPercent() const { return maxPower > 0 ? (power * 100.0f) / maxPower : 0.0f; }
    bool IsLowHealth(float threshold = 30.0f) const { return GetHealthPercent() < threshold; }
    bool IsLowPower(float threshold = 20.0f) const { return GetPowerPercent() < threshold; }
};

class TC_GAME_API ResourceEventBus
{
public:
    static ResourceEventBus* instance();

    bool PublishEvent(ResourceEvent const& event);

    // Subscription management
    using EventHandler = std::function<void(ResourceEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<ResourceEventType> const& types);
    void SubscribeAll(BotAI* subscriber);
    void Unsubscribe(BotAI* subscriber);

    // Direct callback subscriptions (for systems without BotAI)
    uint32 SubscribeCallback(EventHandler handler, std::vector<ResourceEventType> const& types);
    void UnsubscribeCallback(uint32 subscriptionId);

    // Statistics
    uint64 GetTotalEventsPublished() const { return _totalEventsPublished; }
    uint64 GetEventCount(ResourceEventType type) const;

private:
    ResourceEventBus() = default;

    void DeliverEvent(ResourceEvent const& event);

    std::unordered_map<ResourceEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<ResourceEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    std::unordered_map<ResourceEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    mutable std::mutex _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_RESOURCE_EVENT_BUS_H
