/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "QuestEventBus.h"
#include "Log.h"
#include <sstream>

namespace Playerbot
{

QuestEventBus* QuestEventBus::instance()
{
    static QuestEventBus instance;
    return &instance;
}

bool QuestEventBus::PublishEvent(QuestEvent const& event)
{
    if (!event.IsValid())
    {
        TC_LOG_ERROR("playerbot.quest", "QuestEventBus: Invalid event rejected");
        return false;
    }

    TC_LOG_TRACE("playerbot.quest", "QuestEventBus: {}", event.ToString());
    return true;
}

bool QuestEvent::IsValid() const
{
    return type < QuestEventType::MAX_QUEST_EVENT;
}

std::string QuestEvent::ToString() const
{
    std::ostringstream oss;

    const char* eventNames[] = {
        "GIVER_STATUS", "LIST_RECEIVED", "DETAILS_RECEIVED", "REQUEST_ITEMS",
        "OFFER_REWARD", "COMPLETED", "FAILED", "CREDIT_ADDED",
        "OBJECTIVE_COMPLETE", "UPDATE_FAILED", "CONFIRM_ACCEPT", "POI_RECEIVED"
    };

    oss << "[QuestEvent] Type: " << eventNames[static_cast<uint8>(type)]
        << ", NPC: " << npcGuid.ToString()
        << ", Quest: " << questId
        << ", Credit: " << creditEntry << " x" << creditCount;

    return oss.str();
}

} // namespace Playerbot
