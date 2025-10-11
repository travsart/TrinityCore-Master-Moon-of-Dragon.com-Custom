/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotPacketSniffer.h"
#include "WorldSession.h"
#include "Player.h"
#include "QuestEventBus.h"
#include "QuestPackets.h"
#include "Log.h"

namespace Playerbot
{

// ================================================================================================
// TYPED PACKET HANDLERS - QUEST CATEGORY
// Full implementation of all 14 quest packets
// ================================================================================================

void ParseTypedQuestGiverStatus(WorldSession* session, WorldPackets::Quest::QuestGiverStatus const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_GIVER_STATUS;
    event.npcGuid = packet.QuestGiver.Guid;
    event.playerGuid = bot->GetGUID();
    event.questId = 0;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_STATUS (typed): npc={}, status={}",
        bot->GetName(), packet.QuestGiver.Guid.ToString(), static_cast<uint32>(packet.StatusFlags));
}

void ParseTypedQuestGiverQuestListMessage(WorldSession* session, WorldPackets::Quest::QuestGiverQuestListMessage const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_LIST_RECEIVED;
    event.npcGuid = packet.QuestGiverGUID;
    event.playerGuid = bot->GetGUID();
    event.questId = 0;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_QUEST_LIST (typed): {} quests available",
        bot->GetName(), packet.QuestDataContainers.size());
}

void ParseTypedQuestGiverQuestDetails(WorldSession* session, WorldPackets::Quest::QuestGiverQuestDetails const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_DETAILS_RECEIVED;
    event.npcGuid = packet.QuestGiverGUID;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_QUEST_DETAILS (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestGiverRequestItems(WorldSession* session, WorldPackets::Quest::QuestGiverRequestItems const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_REQUEST_ITEMS;
    event.npcGuid = packet.QuestGiverGUID;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_REQUEST_ITEMS (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestGiverOfferRewardMessage(WorldSession* session, WorldPackets::Quest::QuestGiverOfferRewardMessage const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_OFFER_REWARD;
    event.npcGuid = packet.QuestGiverGUID;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_OFFER_REWARD (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestGiverQuestComplete(WorldSession* session, WorldPackets::Quest::QuestGiverQuestComplete const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_COMPLETED;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_QUEST_COMPLETE (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestGiverQuestFailed(WorldSession* session, WorldPackets::Quest::QuestGiverQuestFailed const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_FAILED;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_GIVER_QUEST_FAILED (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestUpdateAddCreditSimple(WorldSession* session, WorldPackets::Quest::QuestUpdateAddCreditSimple const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_CREDIT_ADDED;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = packet.ObjectID;
    event.creditCount = 1;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_UPDATE_ADD_CREDIT_SIMPLE (typed): quest={}, credit={}",
        bot->GetName(), packet.QuestID, packet.ObjectID);
}

void ParseTypedQuestUpdateAddCredit(WorldSession* session, WorldPackets::Quest::QuestUpdateAddCredit const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_CREDIT_ADDED;
    event.npcGuid = packet.VictimGUID;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = packet.ObjectID;
    event.creditCount = packet.Count;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_UPDATE_ADD_CREDIT (typed): quest={}, credit={}, count={}",
        bot->GetName(), packet.QuestID, packet.ObjectID, packet.Count);
}

void ParseTypedQuestUpdateComplete(WorldSession* session, WorldPackets::Quest::QuestUpdateComplete const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_OBJECTIVE_COMPLETE;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_UPDATE_COMPLETE (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestUpdateFailed(WorldSession* session, WorldPackets::Quest::QuestUpdateFailed const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_UPDATE_FAILED;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_UPDATE_FAILED (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestUpdateFailedTimer(WorldSession* session, WorldPackets::Quest::QuestUpdateFailedTimer const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_UPDATE_FAILED;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_UPDATE_FAILED_TIMER (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestConfirmAccept(WorldSession* session, WorldPackets::Quest::QuestConfirmAccept const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_CONFIRM_ACCEPT;
    event.npcGuid = packet.InitiatedBy;
    event.playerGuid = bot->GetGUID();
    event.questId = packet.QuestID;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_CONFIRM_ACCEPT (typed): quest={}",
        bot->GetName(), packet.QuestID);
}

void ParseTypedQuestPOIQueryResponse(WorldSession* session, WorldPackets::Quest::QuestPOIQueryResponse const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    QuestEvent event;
    event.type = QuestEventType::QUEST_POI_RECEIVED;
    event.npcGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.questId = 0;
    event.creditEntry = 0;
    event.creditCount = 0;
    event.timestamp = std::chrono::steady_clock::now();

    QuestEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received QUEST_POI_QUERY_RESPONSE (typed): {} POIs",
        bot->GetName(), packet.QuestPOIData.size());
}

// ================================================================================================
// HANDLER REGISTRATION
// ================================================================================================

void RegisterQuestPacketHandlers()
{
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverStatus>(&ParseTypedQuestGiverStatus);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverQuestListMessage>(&ParseTypedQuestGiverQuestListMessage);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverQuestDetails>(&ParseTypedQuestGiverQuestDetails);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverRequestItems>(&ParseTypedQuestGiverRequestItems);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverOfferRewardMessage>(&ParseTypedQuestGiverOfferRewardMessage);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverQuestComplete>(&ParseTypedQuestGiverQuestComplete);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestGiverQuestFailed>(&ParseTypedQuestGiverQuestFailed);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestUpdateAddCreditSimple>(&ParseTypedQuestUpdateAddCreditSimple);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestUpdateAddCredit>(&ParseTypedQuestUpdateAddCredit);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestUpdateComplete>(&ParseTypedQuestUpdateComplete);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestUpdateFailed>(&ParseTypedQuestUpdateFailed);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestUpdateFailedTimer>(&ParseTypedQuestUpdateFailedTimer);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestConfirmAccept>(&ParseTypedQuestConfirmAccept);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Quest::QuestPOIQueryResponse>(&ParseTypedQuestPOIQueryResponse);

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Registered {} Quest packet typed handlers", 14);
}

} // namespace Playerbot
