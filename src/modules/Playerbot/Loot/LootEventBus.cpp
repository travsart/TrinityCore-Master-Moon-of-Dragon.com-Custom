/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LootEventBus.h"
#include "Log.h"
#include <sstream>

namespace Playerbot
{

LootEventBus* LootEventBus::instance()
{
    static LootEventBus instance;
    return &instance;
}

bool LootEventBus::PublishEvent(LootEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.loot", "LootEventBus: Invalid event rejected");
        return false;
    }

    TC_LOG_TRACE("playerbot.loot", "LootEventBus: {}", event.ToString());
    return true;
}

bool LootEvent::IsValid() const
{
    if (type >= LootEventType::MAX_LOOT_EVENT)
        return false;

    return true;
}

std::string LootEvent::ToString() const
{
    std::ostringstream oss;

    const char* eventNames[] = {
        "LOOT_OPENED", "LOOT_CLOSED", "ITEM_RECEIVED", "MONEY_RECEIVED",
        "LOOT_REMOVED", "SLOT_CHANGED", "ROLL_STARTED", "ROLL_CAST",
        "ROLL_WON", "ALL_PASSED", "MASTER_LOOT_LIST"
    };

    oss << "[LootEvent] Type: " << eventNames[static_cast<uint8>(type)]
        << ", Source: " << lootGuid.ToString()
        << ", Player: " << playerGuid.ToString()
        << ", Item: " << itemId << " x" << itemCount
        << ", Money: " << money
        << ", Slot: " << static_cast<uint32>(slot)
        << ", Roll: " << static_cast<uint32>(rollType);

    return oss.str();
}

} // namespace Playerbot
