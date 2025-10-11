/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_COOLDOWN_EVENT_BUS_H
#define PLAYERBOT_COOLDOWN_EVENT_BUS_H

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>

namespace Playerbot
{

enum class CooldownEventType : uint8
{
    SPELL_COOLDOWN_START = 0,
    SPELL_COOLDOWN_CLEAR,
    SPELL_COOLDOWN_MODIFY,
    SPELL_COOLDOWNS_CLEAR_ALL,
    ITEM_COOLDOWN_START,
    CATEGORY_COOLDOWN_START,
    MAX_COOLDOWN_EVENT
};

struct CooldownEvent
{
    CooldownEventType type;
    ObjectGuid casterGuid;
    uint32 spellId;
    uint32 itemId;
    uint32 category;
    uint32 cooldownMs;
    int32 modRateMs;
    std::chrono::steady_clock::time_point timestamp;

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API CooldownEventBus
{
public:
    static CooldownEventBus* instance();
    bool PublishEvent(CooldownEvent const& event);
private:
    CooldownEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_COOLDOWN_EVENT_BUS_H
