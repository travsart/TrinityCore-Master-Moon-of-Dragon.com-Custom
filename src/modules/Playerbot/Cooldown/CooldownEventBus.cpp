/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "CooldownEventBus.h"
#include "Log.h"
#include <sstream>

namespace Playerbot
{

CooldownEventBus* CooldownEventBus::instance()
{
    static CooldownEventBus instance;
    return &instance;
}

bool CooldownEventBus::PublishEvent(CooldownEvent const& event)
{
    if (!event.IsValid())
        return false;

    TC_LOG_TRACE("playerbot.cooldown", "CooldownEvent: {}", event.ToString());
    return true;
}

bool CooldownEvent::IsValid() const
{
    return type < CooldownEventType::MAX_COOLDOWN_EVENT;
}

std::string CooldownEvent::ToString() const
{
    std::ostringstream oss;
    oss << "[CooldownEvent] Type: " << static_cast<uint32>(type)
        << ", Spell: " << spellId
        << ", Duration: " << cooldownMs << "ms";
    return oss.str();
}

} // namespace Playerbot
