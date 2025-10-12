/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotPacketSniffer.h"
#include "ResourceEventBus.h"
#include "CombatPackets.h"
#include "Player.h"
#include "WorldSession.h"
#include "Log.h"

namespace Playerbot
{

/**
 * @brief SMSG_HEALTH_UPDATE - Unit health changed
 *
 * Fired when any unit's health changes (player, bot, NPC, creature).
 * Critical for healing priority and threat assessment.
 */
void ParseTypedHealthUpdate(WorldSession* session, WorldPackets::Combat::HealthUpdate const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Get the unit whose health changed
    Unit* unit = ObjectAccessor::GetUnit(*bot, packet.Guid);
    if (!unit)
        return;

    // Create health update event
    ResourceEvent event = ResourceEvent::HealthUpdate(
        packet.Guid,
        bot->GetGUID(),
        static_cast<uint32>(packet.Health),
        unit->GetMaxHealth()
    );

    ResourceEventBus::instance()->PublishEvent(event);

    TC_LOG_TRACE("playerbot.packets", "Bot {} received HEALTH_UPDATE (typed): Unit {} health={}/{} ({}%)",
        bot->GetName(),
        packet.Guid.ToString(),
        packet.Health,
        unit->GetMaxHealth(),
        event.GetHealthPercent());
}

/**
 * @brief SMSG_POWER_UPDATE - Unit power changed (mana, rage, energy, etc.)
 *
 * Fired when any unit's power changes. Supports multiple power types.
 * Critical for resource management and spell usage decisions.
 */
void ParseTypedPowerUpdate(WorldSession* session, WorldPackets::Combat::PowerUpdate const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Get the unit whose power changed
    Unit* unit = ObjectAccessor::GetUnit(*bot, packet.Guid);
    if (!unit)
        return;

    // PowerUpdate can contain multiple power types
    for (auto const& powerInfo : packet.Powers)
    {
        // Get max power for this type
        int32 maxPower = unit->GetMaxPower(static_cast<Powers>(powerInfo.PowerType));

        // Create power update event
        ResourceEvent event = ResourceEvent::PowerUpdate(
            packet.Guid,
            bot->GetGUID(),
            powerInfo.Power,
            maxPower,
            powerInfo.PowerType
        );

        ResourceEventBus::instance()->PublishEvent(event);

        TC_LOG_TRACE("playerbot.packets", "Bot {} received POWER_UPDATE (typed): Unit {} powerType={} power={}/{} ({}%)",
            bot->GetName(),
            packet.Guid.ToString(),
            uint32(powerInfo.PowerType),
            powerInfo.Power,
            maxPower,
            event.GetPowerPercent());
    }
}

/**
 * @brief SMSG_BREAK_TARGET - Target broken/cleared
 *
 * Fired when a unit's target is forcibly cleared (e.g., stealth, vanish, fear).
 * Critical for combat state tracking and target validation.
 */
void ParseTypedBreakTarget(WorldSession* session, WorldPackets::Combat::BreakTarget const& packet)
{
    if (!session)
        return;

    Player* bot = session->GetPlayer();
    if (!bot)
        return;

    // Create break target event
    ResourceEvent event = ResourceEvent::BreakTarget(
        packet.UnitGUID,
        bot->GetGUID()
    );

    ResourceEventBus::instance()->PublishEvent(event);

    TC_LOG_DEBUG("playerbot.packets", "Bot {} received BREAK_TARGET (typed): Unit {} target broken",
        bot->GetName(),
        packet.UnitGUID.ToString());
}

/**
 * @brief Register all resource packet typed handlers
 *
 * Called during PlayerbotPacketSniffer initialization to register
 * type-safe packet interceptors for resource-related packets.
 */
void RegisterResourcePacketHandlers()
{
    // Register health tracking
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Combat::HealthUpdate>(&ParseTypedHealthUpdate);

    // Register power tracking
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Combat::PowerUpdate>(&ParseTypedPowerUpdate);

    // Register target break tracking
    PlayerbotPacketSniffer::RegisterTypedHandler<WorldPackets::Combat::BreakTarget>(&ParseTypedBreakTarget);

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Registered {} Resource packet typed handlers", 3);
}

} // namespace Playerbot
