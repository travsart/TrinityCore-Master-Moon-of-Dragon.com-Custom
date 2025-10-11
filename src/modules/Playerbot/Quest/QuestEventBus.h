/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_QUEST_EVENT_BUS_H
#define PLAYERBOT_QUEST_EVENT_BUS_H

#include "Define.h"
#include "ObjectGuid.h"
#include <chrono>
#include <string>

namespace Playerbot
{

enum class QuestEventType : uint8
{
    QUEST_GIVER_STATUS = 0,
    QUEST_LIST_RECEIVED,
    QUEST_DETAILS_RECEIVED,
    QUEST_REQUEST_ITEMS,
    QUEST_OFFER_REWARD,
    QUEST_COMPLETED,
    QUEST_FAILED,
    QUEST_CREDIT_ADDED,
    QUEST_OBJECTIVE_COMPLETE,
    QUEST_UPDATE_FAILED,
    QUEST_CONFIRM_ACCEPT,
    QUEST_POI_RECEIVED,
    MAX_QUEST_EVENT
};

struct QuestEvent
{
    QuestEventType type;
    ObjectGuid npcGuid;
    ObjectGuid playerGuid;
    uint32 questId;
    uint32 creditEntry;
    uint16 creditCount;
    std::chrono::steady_clock::time_point timestamp;

    bool IsValid() const;
    std::string ToString() const;
};

class TC_GAME_API QuestEventBus
{
public:
    static QuestEventBus* instance();
    bool PublishEvent(QuestEvent const& event);

private:
    QuestEventBus() = default;
};

} // namespace Playerbot

#endif // PLAYERBOT_QUEST_EVENT_BUS_H
