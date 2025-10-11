/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "AuraEventBus.h"
#include "Log.h"
#include <sstream>

namespace Playerbot
{

AuraEventBus* AuraEventBus::instance()
{
    static AuraEventBus instance;
    return &instance;
}

bool AuraEventBus::PublishEvent(AuraEvent const& event)
{
    if (!event.IsValid())
        return false;

    TC_LOG_TRACE("playerbot.aura", "AuraEvent: {}", event.ToString());
    return true;
}

bool AuraEvent::IsValid() const
{
    return type < AuraEventType::MAX_AURA_EVENT;
}

std::string AuraEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[AuraEvent] Type: " << static_cast<uint32>(type)
        << ", Spell: " << spellId
        << ", Stacks: " << static_cast<uint32>(stackCount);
    return oss.str();
}

} // namespace Playerbot
