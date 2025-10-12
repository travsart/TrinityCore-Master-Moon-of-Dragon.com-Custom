/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "SocialEventBus.h"
#include "BotAI.h"
#include "Log.h"
#include <sstream>
#include <algorithm>

namespace Playerbot
{

// Factory methods
SocialEvent SocialEvent::MessageChat(ObjectGuid senderGuid, ObjectGuid targetGuid,
    std::string senderName, std::string message, uint32 chatType, uint32 language,
    std::string channelName, uint32 achievementId)
{
    SocialEvent event;
    event.type = SocialEventType::MESSAGE_CHAT;
    event.senderGuid = senderGuid;
    event.targetGuid = targetGuid;
    event.senderName = std::move(senderName);
    event.message = std::move(message);
    event.chatType = chatType;
    event.language = language;
    event.channelName = std::move(channelName);
    event.achievementId = achievementId;
    event.emoteId = 0;
    event.guildId = 0;
    event.tradeStatus = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

SocialEvent SocialEvent::EmoteReceived(ObjectGuid senderGuid, ObjectGuid targetGuid, uint32 emoteId)
{
    SocialEvent event;
    event.type = SocialEventType::EMOTE_RECEIVED;
    event.senderGuid = senderGuid;
    event.targetGuid = targetGuid;
    event.emoteId = emoteId;
    event.chatType = 0;
    event.language = 0;
    event.achievementId = 0;
    event.guildId = 0;
    event.tradeStatus = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

SocialEvent SocialEvent::TextEmoteReceived(ObjectGuid senderGuid, ObjectGuid targetGuid, uint32 emoteId)
{
    SocialEvent event;
    event.type = SocialEventType::TEXT_EMOTE_RECEIVED;
    event.senderGuid = senderGuid;
    event.targetGuid = targetGuid;
    event.emoteId = emoteId;
    event.chatType = 0;
    event.language = 0;
    event.achievementId = 0;
    event.guildId = 0;
    event.tradeStatus = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

SocialEvent SocialEvent::GuildInviteReceived(ObjectGuid inviterGuid, ObjectGuid botGuid,
    std::string inviterName, uint32 guildId)
{
    SocialEvent event;
    event.type = SocialEventType::GUILD_INVITE_RECEIVED;
    event.senderGuid = inviterGuid;
    event.targetGuid = botGuid;
    event.senderName = std::move(inviterName);
    event.guildId = guildId;
    event.chatType = 0;
    event.language = 0;
    event.emoteId = 0;
    event.achievementId = 0;
    event.tradeStatus = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

SocialEvent SocialEvent::GuildEventReceived(ObjectGuid botGuid, uint32 guildId, std::string message)
{
    SocialEvent event;
    event.type = SocialEventType::GUILD_EVENT_RECEIVED;
    event.targetGuid = botGuid;
    event.guildId = guildId;
    event.message = std::move(message);
    event.chatType = 0;
    event.language = 0;
    event.emoteId = 0;
    event.achievementId = 0;
    event.tradeStatus = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

SocialEvent SocialEvent::TradeStatusChanged(ObjectGuid traderGuid, ObjectGuid botGuid, uint8 tradeStatus)
{
    SocialEvent event;
    event.type = SocialEventType::TRADE_STATUS_CHANGED;
    event.senderGuid = traderGuid;
    event.targetGuid = botGuid;
    event.tradeStatus = tradeStatus;
    event.chatType = 0;
    event.language = 0;
    event.emoteId = 0;
    event.guildId = 0;
    event.achievementId = 0;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool SocialEvent::IsValid() const
{
    if (type >= SocialEventType::MAX_SOCIAL_EVENT)
        return false;

    switch (type)
    {
        case SocialEventType::MESSAGE_CHAT:
            return !senderGuid.IsEmpty() && !message.empty();
        case SocialEventType::EMOTE_RECEIVED:
        case SocialEventType::TEXT_EMOTE_RECEIVED:
            return !senderGuid.IsEmpty() && emoteId > 0;
        case SocialEventType::GUILD_INVITE_RECEIVED:
            return !senderGuid.IsEmpty() && !targetGuid.IsEmpty();
        case SocialEventType::GUILD_EVENT_RECEIVED:
            return !targetGuid.IsEmpty() && guildId > 0;
        case SocialEventType::TRADE_STATUS_CHANGED:
            return !targetGuid.IsEmpty();
        default:
            return false;
    }
}

std::string SocialEvent::ToString() const
{
    std::ostringstream oss;
    oss << "SocialEvent[";

    switch (type)
    {
        case SocialEventType::MESSAGE_CHAT:
            oss << "MESSAGE_CHAT, sender=" << senderName << ", type=" << chatType
                << ", msg=" << message.substr(0, 50);
            if (!channelName.empty())
                oss << ", channel=" << channelName;
            break;
        case SocialEventType::EMOTE_RECEIVED:
            oss << "EMOTE_RECEIVED, sender=" << senderGuid.ToString() << ", emote=" << emoteId;
            break;
        case SocialEventType::TEXT_EMOTE_RECEIVED:
            oss << "TEXT_EMOTE_RECEIVED, sender=" << senderGuid.ToString() << ", emote=" << emoteId;
            break;
        case SocialEventType::GUILD_INVITE_RECEIVED:
            oss << "GUILD_INVITE_RECEIVED, inviter=" << senderName << ", guild=" << guildId;
            break;
        case SocialEventType::GUILD_EVENT_RECEIVED:
            oss << "GUILD_EVENT_RECEIVED, guild=" << guildId << ", msg=" << message;
            break;
        case SocialEventType::TRADE_STATUS_CHANGED:
            oss << "TRADE_STATUS_CHANGED, trader=" << senderGuid.ToString() << ", status=" << uint32(tradeStatus);
            break;
        default:
            oss << "UNKNOWN";
    }

    oss << "]";
    return oss.str();
}

// SocialEventBus implementation
SocialEventBus* SocialEventBus::instance()
{
    static SocialEventBus instance;
    return &instance;
}

bool SocialEventBus::PublishEvent(SocialEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.events", "SocialEventBus: Invalid event rejected: {}", event.ToString());
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

    TC_LOG_TRACE("playerbot.events", "SocialEventBus: Published event: {}", event.ToString());
    return true;
}

void SocialEventBus::Subscribe(BotAI* subscriber, std::vector<SocialEventType> const& types)
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
            TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Subscriber {} registered for type {}",
                static_cast<void*>(subscriber), uint32(type));
        }
    }
}

void SocialEventBus::SubscribeAll(BotAI* subscriber)
{
    if (!subscriber)
        return;

    std::lock_guard<std::mutex> lock(_subscriberMutex);

    if (std::find(_globalSubscribers.begin(), _globalSubscribers.end(), subscriber) == _globalSubscribers.end())
    {
        _globalSubscribers.push_back(subscriber);
        TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Subscriber {} registered for ALL events",
            static_cast<void*>(subscriber));
    }
}

void SocialEventBus::Unsubscribe(BotAI* subscriber)
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

    TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Subscriber {} unregistered", static_cast<void*>(subscriber));
}

uint32 SocialEventBus::SubscribeCallback(EventHandler handler, std::vector<SocialEventType> const& types)
{
    if (!handler)
        return 0;

    std::lock_guard<std::mutex> lock(_subscriberMutex);

    CallbackSubscription sub;
    sub.id = _nextCallbackId++;
    sub.handler = std::move(handler);
    sub.types = types;

    _callbackSubscriptions.push_back(std::move(sub));

    TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Callback {} registered for {} types",
        sub.id, types.size());

    return sub.id;
}

void SocialEventBus::UnsubscribeCallback(uint32 subscriptionId)
{
    std::lock_guard<std::mutex> lock(_subscriberMutex);

    _callbackSubscriptions.erase(
        std::remove_if(_callbackSubscriptions.begin(), _callbackSubscriptions.end(),
            [subscriptionId](CallbackSubscription const& sub) { return sub.id == subscriptionId; }),
        _callbackSubscriptions.end());

    TC_LOG_DEBUG("playerbot.events", "SocialEventBus: Callback {} unregistered", subscriptionId);
}

uint64 SocialEventBus::GetEventCount(SocialEventType type) const
{
    std::lock_guard<std::mutex> lock(_subscriberMutex);
    auto it = _eventCounts.find(type);
    return it != _eventCounts.end() ? it->second : 0;
}

void SocialEventBus::DeliverEvent(SocialEvent const& event)
{
    std::lock_guard<std::mutex> lock(_subscriberMutex);

    // Deliver to type-specific subscribers
    auto it = _subscribers.find(event.type);
    if (it != _subscribers.end())
    {
        for (auto subscriber : it->second)
        {
            if (subscriber)
                subscriber->OnSocialEvent(event);
        }
    }

    // Deliver to global subscribers
    for (auto subscriber : _globalSubscribers)
    {
        if (subscriber)
            subscriber->OnSocialEvent(event);
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
