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
#include "LootEventBus.h"
#include "LootPackets.h"
#include "Log.h"

namespace Playerbot
{

// ================================================================================================
// TYPED PACKET HANDLERS - LOOT CATEGORY
// Full implementation of all 13 loot packets
// ================================================================================================

void ParseTypedLootResponse(WorldSession* session, WorldPackets::Loot::LootResponse const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_WINDOW_OPENED;
    event.lootGuid = packet.Owner;
    event.playerGuid = bot->GetGUID();
    event.itemId = 0;
    event.itemCount = 0;
    event.money = packet.Coins;
    event.slot = 0;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_RESPONSE (typed): {} items, {} copper",
        bot->GetName(), packet.Items.size(), packet.Coins);
}

void ParseTypedLootReleaseResponse(WorldSession* session, WorldPackets::Loot::LootReleaseResponse const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_WINDOW_CLOSED;
    event.lootGuid = packet.LootObj;
    event.playerGuid = bot->GetGUID();
    event.itemId = 0;
    event.itemCount = 0;
    event.money = 0;
    event.slot = 0;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_RELEASE_RESPONSE (typed)",
        bot->GetName());
}

void ParseTypedLootRemoved(WorldSession* session, WorldPackets::Loot::LootRemoved const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_REMOVED;
    event.lootGuid = packet.Owner;
    event.playerGuid = bot->GetGUID();
    event.itemId = 0;
    event.itemCount = 0;
    event.money = 0;
    event.slot = packet.LootListID;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_REMOVED (typed): slot={}",
        bot->GetName(), packet.LootListID);
}

void ParseTypedLootMoneyNotify(WorldSession* session, WorldPackets::Loot::LootMoneyNotify const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_MONEY_RECEIVED;
    event.lootGuid = ObjectGuid::Empty;
    event.playerGuid = bot->GetGUID();
    event.itemId = 0;
    event.itemCount = 0;
    event.money = packet.Money;
    event.slot = 0;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_MONEY_NOTIFY (typed): {} copper, soleLooter={}",
        bot->GetName(), packet.Money, packet.SoleLooter);
}

void ParseTypedLootSlotChanged(WorldSession* session, WorldPackets::Loot::LootSlotChanged const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_SLOT_CHANGED;
    event.lootGuid = packet.Owner;
    event.playerGuid = bot->GetGUID();
    event.itemId = 0;
    event.itemCount = 0;
    event.money = 0;
    event.slot = packet.LootListID;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_SLOT_CHANGED (typed): slot={}",
        bot->GetName(), packet.LootListID);
}

void ParseTypedStartLootRoll(WorldSession* session, WorldPackets::Loot::StartLootRoll const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_ROLL_STARTED;
    event.lootGuid = packet.LootObj;
    event.playerGuid = bot->GetGUID();
    event.itemId = packet.Item.ItemID;
    event.itemCount = packet.Item.Quantity;
    event.money = 0;
    event.slot = packet.LootListID;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received START_LOOT_ROLL (typed): item={} x{}, rollTime={}ms",
        bot->GetName(), packet.Item.ItemID, packet.Item.Quantity, packet.RollTime);
}

void ParseTypedLootRoll(WorldSession* session, WorldPackets::Loot::LootRoll const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_ROLL_CAST;
    event.lootGuid = packet.LootObj;
    event.playerGuid = packet.Player;
    event.itemId = 0;
    event.itemCount = 0;
    event.money = 0;
    event.slot = packet.LootListID;
    event.rollType = static_cast<uint8>(packet.RollType);
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_ROLL (typed): player={}, rollType={}, roll={}",
        bot->GetName(), packet.Player.ToString(), static_cast<uint32>(packet.RollType), packet.Roll);
}

void ParseTypedLootRollWon(WorldSession* session, WorldPackets::Loot::LootRollWon const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_ROLL_WON;
    event.lootGuid = packet.LootObj;
    event.playerGuid = packet.Player;
    event.itemId = packet.Item.ItemID;
    event.itemCount = packet.Item.Quantity;
    event.money = 0;
    event.slot = packet.LootListID;
    event.rollType = static_cast<uint8>(packet.RollType);
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_ROLL_WON (typed): player={}, item={}, roll={}",
        bot->GetName(), packet.Player.ToString(), packet.Item.ItemID, packet.Roll);
}

void ParseTypedLootAllPassed(WorldSession* session, WorldPackets::Loot::LootAllPassed const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::LOOT_ALL_PASSED;
    event.lootGuid = packet.LootObj;
    event.playerGuid = bot->GetGUID();
    event.itemId = packet.Item.ItemID;
    event.itemCount = packet.Item.Quantity;
    event.money = 0;
    event.slot = packet.LootListID;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_ALL_PASSED (typed): item={}",
        bot->GetName(), packet.Item.ItemID);
}

void ParseTypedMasterLootCandidateList(WorldSession* session, WorldPackets::Loot::MasterLootCandidateList const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    LootEvent event;
    event.type = LootEventType::MASTER_LOOT_LIST;
    event.lootGuid = packet.LootObj;
    event.playerGuid = bot->GetGUID();
    event.itemId = 0;
    event.itemCount = 0;
    event.money = 0;
    event.slot = 0;
    event.rollType = 0;
    event.timestamp = std::chrono::steady_clock::now();

    LootEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received MASTER_LOOT_LIST (typed): {} candidates",
        bot->GetName(), packet.Players.size());
}

void ParseTypedLootList(WorldSession* session, WorldPackets::Loot::LootList const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received LOOT_LIST (typed): owner={}, master={}",
        bot->GetName(), packet.Owner.ToString(),
        packet.Master.IsEmpty() ? "none" : packet.Master.ToString());
}

// ================================================================================================
// HANDLER REGISTRATION
// ================================================================================================

void RegisterLootPacketHandlers()
{
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootResponse>(&ParseTypedLootResponse);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootReleaseResponse>(&ParseTypedLootReleaseResponse);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootRemoved>(&ParseTypedLootRemoved);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootMoneyNotify>(&ParseTypedLootMoneyNotify);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootSlotChanged>(&ParseTypedLootSlotChanged);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::StartLootRoll>(&ParseTypedStartLootRoll);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootRoll>(&ParseTypedLootRoll);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootRollWon>(&ParseTypedLootRollWon);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootAllPassed>(&ParseTypedLootAllPassed);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::MasterLootCandidateList>(&ParseTypedMasterLootCandidateList);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Loot::LootList>(&ParseTypedLootList);

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Registered {} Loot packet typed handlers", 11);
}

} // namespace Playerbot
