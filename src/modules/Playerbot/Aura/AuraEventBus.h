/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef PLAYERBOT_AURA_EVENT_BUS_H
#define PLAYERBOT_AURA_EVENT_BUS_H

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>

namespace Playerbot
{

enum class AuraEventType : uint8
{
    AURA_APPLIED = 0,
    AURA_REMOVED,
    AURA_UPDATED,
    DISPEL_FAILED,
    SPELL_MODIFIER_CHANGED,
    MAX_AURA_EVENT
};

struct AuraEvent
{
    AuraEventType type;
    ObjectGuid targetGuid;
    uint32 spellId;
    uint32 auraSlot;
    uint8 stackCount;
    std::chrono::steady_clock::time_point timestamp;

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API AuraEventBus
{
public:
    static AuraEventBus* instance();
    bool PublishEvent(AuraEvent const& event);
private:
    AuraEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_AURA_EVENT_BUS_H
