/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "PlayerbotPacketSniffer.h"
#include "WorldSession.h"
#include "Player.h"
#include "AuraEventBus.h"
#include "SpellPackets.h"
#include "Log.h"

namespace Playerbot
{

void ParseTypedAuraUpdate(WorldSession* session, WorldPackets::Spells::AuraUpdate const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    for (auto const& auraInfo : packet.Auras)
    {
        AuraEvent event;
        event.type = auraInfo.AuraDataChanged ? AuraEventType::AURA_UPDATED : AuraEventType::AURA_APPLIED;
        event.targetGuid = packet.UnitGUID;
        event.spellId = auraInfo.SpellID;
        event.auraSlot = auraInfo.Slot;
        event.stackCount = 1;
        event.timestamp = std::chrono::steady_clock::now();

        AuraEventBus::instance()->PublishEvent(event);
    }

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received AURA_UPDATE (typed): {} auras",
        bot->GetName(), packet.Auras.size());
}

void ParseTypedSetFlatSpellModifier(WorldSession* session, WorldPackets::Spells::SetSpellModifier const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    for (auto const& modData : packet.Modifiers)
    {
        AuraEvent event;
        event.type = AuraEventType::SPELL_MODIFIER_CHANGED;
        event.targetGuid = bot->GetGUID();
        event.spellId = 0;
        event.auraSlot = static_cast<uint32>(modData.ModIndex);
        event.stackCount = 0;
        event.timestamp = std::chrono::steady_clock::now();

        AuraEventBus::instance()->PublishEvent(event);
    }

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received SET_FLAT_SPELL_MODIFIER (typed)",
        bot->GetName());
}

void ParseTypedSetPctSpellModifier(WorldSession* session, WorldPackets::Spells::SetSpellModifier const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    for (auto const& modData : packet.Modifiers)
    {
        AuraEvent event;
        event.type = AuraEventType::SPELL_MODIFIER_CHANGED;
        event.targetGuid = bot->GetGUID();
        event.spellId = 0;
        event.auraSlot = static_cast<uint32>(modData.ModIndex);
        event.stackCount = 0;
        event.timestamp = std::chrono::steady_clock::now();

        AuraEventBus::instance()->PublishEvent(event);
    }

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received SET_PCT_SPELL_MODIFIER (typed)",
        bot->GetName());
}

void ParseTypedDispelFailed(WorldSession* session, WorldPackets::Spells::DispelFailed const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    for (uint32 spellId : packet.FailedSpells)
    {
        AuraEvent event;
        event.type = AuraEventType::DISPEL_FAILED;
        event.targetGuid = packet.CasterGUID;
        event.spellId = spellId;
        event.auraSlot = 0;
        event.stackCount = 0;
        event.timestamp = std::chrono::steady_clock::now();

        AuraEventBus::instance()->PublishEvent(event);
    }

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received DISPEL_FAILED (typed): {} spells",
        bot->GetName(), packet.FailedSpells.size());
}

void RegisterAuraPacketHandlers()
{
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Spells::AuraUpdate>(&ParseTypedAuraUpdate);
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Spells::SetSpellModifier>(&ParseTypedSetFlatSpellModifier);
    // Note: SET_PCT uses same packet type as SET_FLAT, just different opcode
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Spells::DispelFailed>(&ParseTypedDispelFailed);

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Registered {} Aura packet typed handlers", 3);
}

} // namespace Playerbot
