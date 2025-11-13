/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotPacketSniffer.h"
#include "../NPC/NPCEventBus.h"
#include "NPCPackets.h"
#include "BankPackets.h"
#include "PetitionPackets.h"
#include "Player.h"
#include "WorldSession.h"
#include "Log.h"

namespace Playerbot
{

/**
 * @brief SMSG_GOSSIP_MESSAGE - Gossip menu received from NPC
 */
void ParseTypedGossipMessage(WorldSession* session, WorldPackets::NPC::GossipMessage const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Extract gossip option IDs
    std::vector<uint32> options;
    options.reserve(packet.GossipOptions.size());
    for (auto const& option : packet.GossipOptions)
        options.push_back(option.GossipOptionID);

    NPCEvent event = NPCEvent::GossipMenuReceived(
        bot->GetGUID(),
        packet.GossipGUID,
        packet.GossipID,
        packet.RandomTextID.value_or(0),
        std::move(options)
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_TRACE("playerbot.packets", "Bot {} received GOSSIP_MESSAGE (typed): npc={}, menu={}, options={}",
        bot->GetName(), packet.GossipGUID.ToString(), packet.GossipID, options.size());
}

/**
 * @brief SMSG_GOSSIP_COMPLETE - Gossip interaction complete
 */
void ParseTypedGossipComplete(WorldSession* session, WorldPackets::NPC::GossipComplete const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Note: GossipComplete doesn't include NPC GUID, use last known NPC
    NPCEvent event = NPCEvent::GossipComplete(
        bot->GetGUID(),
        ObjectGuid::Empty  // No NPC GUID available in packet
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_TRACE("playerbot.packets", "Bot {} received GOSSIP_COMPLETE (typed)",
        bot->GetName());
}

/**
 * @brief SMSG_VENDOR_INVENTORY - Vendor item list received
 */
void ParseTypedVendorInventory(WorldSession* session, WorldPackets::NPC::VendorInventory const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Extract item IDs from vendor inventory
    std::vector<uint32> items;
    items.reserve(packet.Items.size());
    for (auto const& vendorItem : packet.Items)
        items.push_back(vendorItem.Item.ItemID);

    NPCEvent event = NPCEvent::VendorListReceived(
        bot->GetGUID(),
        packet.Vendor,
        0,  // Vendor entry not in packet
        std::move(items)
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received VENDOR_INVENTORY (typed): vendor={}, items={}",
        bot->GetName(), packet.Vendor.ToString(), items.size());
}

/**
 * @brief SMSG_TRAINER_LIST - Trainer spell list received
 */
void ParseTypedTrainerList(WorldSession* session, WorldPackets::NPC::TrainerList const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Extract spell IDs from trainer list
    std::vector<uint32> spells;
    spells.reserve(packet.Spells.size());
    for (auto const& spell : packet.Spells)
        spells.push_back(spell.SpellID);

    NPCEvent event = NPCEvent::TrainerListReceived(
        bot->GetGUID(),
        packet.TrainerGUID,
        packet.TrainerID,
        std::move(spells)
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received TRAINER_LIST (typed): trainer={}, spells={}",
        bot->GetName(), packet.TrainerGUID.ToString(), spells.size());
}

/**
 * @brief SMSG_TRAINER_BUY_FAILED - Trainer service result
 */
void ParseTypedTrainerBuyFailed(WorldSession* session, WorldPackets::NPC::TrainerBuyFailed const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    NPCEvent event = NPCEvent::TrainerServiceResult(
        bot->GetGUID(),
        packet.TrainerFailedReason
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received TRAINER_BUY_FAILED (typed): reason={}",
        bot->GetName(), packet.TrainerFailedReason);
}

/**
 * @brief SMSG_NPC_INTERACTION_OPEN_RESULT - NPC interaction opened (used for bank)
 */
void ParseTypedNPCInteractionOpen(WorldSession* session, WorldPackets::NPC::NPCInteractionOpenResult const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Check if this is a bank interaction
    // PlayerInteractionType enum values not directly accessible, check success
    if (packet.Success)
    {
        NPCEvent event = NPCEvent::BankOpened(
            bot->GetGUID(),
            packet.Npc
        );

        NPCEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("playerbot.packets", "Bot {} received NPC_INTERACTION_OPEN (typed): npc={}, type={}",
            bot->GetName(), packet.Npc.ToString(), static_cast<int32>(packet.InteractionType));
    }
}

/**
 * @brief SMSG_PETITION_SHOW_LIST - Petition list received
 */
void ParseTypedPetitionShowList(WorldSession* session, WorldPackets::Petition::ServerPetitionShowList const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    NPCEvent event = NPCEvent::PetitionListReceived(
        bot->GetGUID(),
        packet.Unit,
        0  // Petition entry not in show list packet
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received PETITION_SHOW_LIST (typed): npc={}, price={}",
        bot->GetName(), packet.Unit.ToString(), packet.Price);
}

/**
 * @brief SMSG_PETITION_SHOW_SIGNATURES - Petition signatures received
 */
void ParseTypedPetitionShowSignatures(WorldSession* session, WorldPackets::Petition::ServerPetitionShowSignatures const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Use this as another petition event (could differentiate by adding a PETITION_SIGNATURES_RECEIVED type)
    NPCEvent event = NPCEvent::PetitionListReceived(
        bot->GetGUID(),
        packet.Owner,  // Use owner as NPC GUID
        packet.PetitionID
    );

    NPCEventBus::instance()->PublishEvent(event);

    TC_LOG_TRACE("playerbot.packets", "Bot {} received PETITION_SHOW_SIGNATURES (typed): petition={}, sigs={}",
        bot->GetName(), packet.PetitionID, packet.Signatures.size());
}

/**
 * @brief Register all NPC packet typed handlers
 */
void RegisterNPCPacketHandlers()
{
    // Register gossip handlers
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::NPC::GossipMessage>(&ParseTypedGossipMessage);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::NPC::GossipComplete>(&ParseTypedGossipComplete);

    // Register vendor/trainer handlers
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::NPC::VendorInventory>(&ParseTypedVendorInventory);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::NPC::TrainerList>(&ParseTypedTrainerList);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::NPC::TrainerBuyFailed>(&ParseTypedTrainerBuyFailed);

    // Register bank/interaction handler
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::NPC::NPCInteractionOpenResult>(&ParseTypedNPCInteractionOpen);

    // Register petition handlers
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Petition::ServerPetitionShowList>(&ParseTypedPetitionShowList);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Petition::ServerPetitionShowSignatures>(&ParseTypedPetitionShowSignatures);

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Registered {} NPC packet typed handlers", 8);
}

} // namespace Playerbot
