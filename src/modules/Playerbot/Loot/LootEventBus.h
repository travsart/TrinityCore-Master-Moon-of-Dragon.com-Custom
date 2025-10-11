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
#include <mutex>
#include <functional>

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

struct LootEvent
{
    LootEventType type;
    ObjectGuid lootGuid;        // Source of loot (creature, chest, etc.)
    ObjectGuid playerGuid;      // Player involved
    uint32 itemId;
    uint32 itemCount;
    uint32 money;
    uint8 slot;
    uint8 rollType;             // Need/Greed/Pass/Disenchant
    std::chrono::steady_clock::time_point timestamp;

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API LootEventBus
{
public:
    static LootEventBus* instance();
    bool PublishEvent(LootEvent const& event);

    // Subscription management
    using EventHandler = std::function<void(LootEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<LootEventType> const& types);
    void SubscribeAll(BotAI* subscriber);
    void Unsubscribe(BotAI* subscriber);

    // Direct callback subscriptions (for systems without BotAI)
    uint32 SubscribeCallback(EventHandler handler, std::vector<LootEventType> const& types);
    void UnsubscribeCallback(uint32 subscriptionId);

private:
    LootEventBus() = default;

    void DeliverEvent(LootEvent const& event);

    std::unordered_map<LootEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<LootEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    mutable std::mutex _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_LOOT_EVENT_BUS_H
