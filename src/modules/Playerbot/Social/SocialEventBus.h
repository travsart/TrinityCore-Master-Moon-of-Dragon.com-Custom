/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_SOCIAL_EVENT_BUS_H
#define PLAYERBOT_SOCIAL_EVENT_BUS_H

#include "Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Playerbot
{

class BotAI;

enum class SocialEventType : uint8
{
    MESSAGE_CHAT = 0,           // SMSG_CHAT / SMSG_MESSAGECHAT
    EMOTE_RECEIVED,             // SMSG_EMOTE
    TEXT_EMOTE_RECEIVED,        // SMSG_TEXT_EMOTE
    GUILD_INVITE_RECEIVED,      // SMSG_GUILD_INVITE
    GUILD_EVENT_RECEIVED,       // SMSG_GUILD_EVENT (multiple types)
    TRADE_STATUS_CHANGED,       // SMSG_TRADE_STATUS
    MAX_SOCIAL_EVENT
};

struct SocialEvent
{
    SocialEventType type;
    ObjectGuid senderGuid;      // Who sent the message/emote/invite
    ObjectGuid targetGuid;      // Who received it (the bot)
    std::string senderName;
    std::string targetName;
    std::string message;        // Chat message or channel name
    std::string channelName;    // For channel chat
    uint32 chatType;            // ChatMsg enum value
    uint32 language;            // Language enum value
    uint32 emoteId;             // Emote/text emote ID
    uint32 guildId;
    uint32 achievementId;       // For achievement links in chat
    uint8 tradeStatus;          // Trade status enum
    std::chrono::steady_clock::time_point timestamp;

    // Factory methods for type-safe event creation
    static SocialEvent MessageChat(ObjectGuid senderGuid, ObjectGuid targetGuid,
        std::string senderName, std::string message, uint32 chatType, uint32 language,
        std::string channelName = "", uint32 achievementId = 0);

    static SocialEvent EmoteReceived(ObjectGuid senderGuid, ObjectGuid targetGuid, uint32 emoteId);

    static SocialEvent TextEmoteReceived(ObjectGuid senderGuid, ObjectGuid targetGuid, uint32 emoteId);

    static SocialEvent GuildInviteReceived(ObjectGuid inviterGuid, ObjectGuid botGuid,
        std::string inviterName, uint32 guildId);

    static SocialEvent GuildEventReceived(ObjectGuid botGuid, uint32 guildId, std::string message);

    static SocialEvent TradeStatusChanged(ObjectGuid traderGuid, ObjectGuid botGuid, uint8 tradeStatus);

    bool IsValid() const;
    std::string ToString() const;

    // Helper methods
    bool IsWhisper() const { return chatType == CHAT_MSG_WHISPER; }
    bool IsPartyChat() const { return chatType == CHAT_MSG_PARTY || chatType == CHAT_MSG_RAID; }
    bool IsGuildChat() const { return chatType == CHAT_MSG_GUILD || chatType == CHAT_MSG_OFFICER; }
    bool IsChannelChat() const { return chatType == CHAT_MSG_CHANNEL; }
    bool IsSayOrYell() const { return chatType == CHAT_MSG_SAY || chatType == CHAT_MSG_YELL; }
};

class TC_GAME_API SocialEventBus
{
public:
    static SocialEventBus* instance();

    bool PublishEvent(SocialEvent const& event);

    // Subscription management
    using EventHandler = std::function<void(SocialEvent const&)>;

    void Subscribe(BotAI* subscriber, std::vector<SocialEventType> const& types);
    void SubscribeAll(BotAI* subscriber);
    void Unsubscribe(BotAI* subscriber);

    // Direct callback subscriptions (for systems without BotAI)
    uint32 SubscribeCallback(EventHandler handler, std::vector<SocialEventType> const& types);
    void UnsubscribeCallback(uint32 subscriptionId);

    // Statistics
    uint64 GetTotalEventsPublished() const { return _totalEventsPublished; }
    uint64 GetEventCount(SocialEventType type) const;

private:
    SocialEventBus() = default;

    void DeliverEvent(SocialEvent const& event);

    std::unordered_map<SocialEventType, std::vector<BotAI*>> _subscribers;
    std::vector<BotAI*> _globalSubscribers;

    struct CallbackSubscription
    {
        uint32 id;
        EventHandler handler;
        std::vector<SocialEventType> types;
    };
    std::vector<CallbackSubscription> _callbackSubscriptions;
    uint32 _nextCallbackId = 1;

    std::unordered_map<SocialEventType, uint64> _eventCounts;
    uint64 _totalEventsPublished = 0;

    mutable std::mutex _subscriberMutex;
};

} // namespace Playerbot

#endif // PLAYERBOT_SOCIAL_EVENT_BUS_H
