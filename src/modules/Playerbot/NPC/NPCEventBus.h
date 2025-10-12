/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_NPC_EVENT_BUS_H
#define PLAYERBOT_NPC_EVENT_BUS_H

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

enum class NPCEventType : uint8
{
    GOSSIP_MENU_RECEIVED = 0,
    GOSSIP_COMPLETE,
    VENDOR_LIST_RECEIVED,
    TRAINER_LIST_RECEIVED,
    TRAINER_SERVICE_RESULT,
    BANK_OPENED,
    SPIRIT_HEALER_CONFIRM,
    PETITION_LIST_RECEIVED,
    MAX_NPC_EVENT
};

struct NPCEvent
{
    NPCEventType type;
    ObjectGuid playerGuid;
    ObjectGuid npcGuid;
    uint32 menuId;
    uint32 textId;
    uint32 vendorEntry;
    uint32 trainerEntry;
    uint32 trainerService;
    uint32 petitionEntry;
    std::vector<uint32> gossipOptions;
    std::vector<uint32> availableItems;  // For vendor/trainer
    std::chrono::steady_clock::time_point timestamp;

    static NPCEvent GossipMenuReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 menuId, uint32 textId, std::vector<uint32> options);
    static NPCEvent GossipComplete(ObjectGuid playerGuid, ObjectGuid npcGuid);
    static NPCEvent VendorListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 vendorEntry, std::vector<uint32> items);
    static NPCEvent TrainerListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 trainerEntry, std::vector<uint32> spells);
    static NPCEvent TrainerServiceResult(ObjectGuid playerGuid, uint32 trainerService);
    static NPCEvent BankOpened(ObjectGuid playerGuid, ObjectGuid npcGuid);
    static NPCEvent SpiritHealerConfirm(ObjectGuid playerGuid, ObjectGuid npcGuid);
    static NPCEvent PetitionListReceived(ObjectGuid playerGuid, ObjectGuid npcGuid, uint32 petitionEntry);

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API NPCEventBus
{
public:
    static NPCEventBus* instance();
    bool PublishEvent(NPCEvent const& event);

    using EventHandler = std::function<void(NPCEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<NPCEventType> const& types);
    void SubscribeAll(BotAI* subscriber);
    void Unsubscribe(BotAI* subscriber);

    uint32 SubscribeCallback(EventHandler handler, std::vector<NPCEventType> const& types);
    void UnsubscribeCallback(uint32 subscriptionId);

    uint64 GetTotalEventsPublished() const { return _totalEventsPublished; }
    uint64 GetEventCount(NPCEventType type) const;

private:
    NPCEventBus() = default;
    void DeliverEvent(NPCEvent const& event);

    std::unordered_map<NPCEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<NPCEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    std::unordered_map<NPCEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    mutable std::mutex _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_NPC_EVENT_BUS_H
