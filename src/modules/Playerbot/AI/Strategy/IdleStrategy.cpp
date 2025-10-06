/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "IdleStrategy.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Game/QuestManager.h"
#include "Social/TradeManager.h"
#include "Professions/GatheringManager.h"

namespace Playerbot
{

IdleStrategy::IdleStrategy() : Strategy("idle")
{
    SetPriority(50); // Lower priority than group strategies
}

void IdleStrategy::InitializeActions()
{
    // TODO: Add idle actions (wander, emote, interact)
    // For now, this is a placeholder
}

void IdleStrategy::InitializeTriggers()
{
    // TODO: Add idle triggers (boredom, curiosity, etc.)
    // For now, this is a placeholder
}

void IdleStrategy::InitializeValues()
{
    // TODO: Add idle values (preferred activities, personality traits)
    // For now, this is a placeholder
}

void IdleStrategy::OnActivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot", "Idle strategy activated for bot {}", ai->GetBot()->GetName());
    SetActive(true);
}

void IdleStrategy::OnDeactivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot", "Idle strategy deactivated for bot {}", ai->GetBot()->GetName());
    SetActive(false);
}

bool IdleStrategy::IsActive(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot", "⚠️ IdleStrategy::IsActive() - ai or bot is null!");
        return false;
    }

    bool active = _active;
    bool hasGroup = (ai->GetBot()->GetGroup() != nullptr);
    bool result = active && !hasGroup;

    static uint32 debugCounter = 0;
    if (++debugCounter % 50 == 0)
    {
        TC_LOG_ERROR("module.playerbot", "🔎 IdleStrategy::IsActive() for bot {} - _active={}, hasGroup={}, result={}",
                    ai->GetBot()->GetName(), active, hasGroup, result);
    }

    // Active when not in a group and explicitly activated
    return result;
}

void IdleStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // CRITICAL DEBUG: Confirm this method is being called
    static uint32 callCounter = 0;
    if (++callCounter % 50 == 0)
    {
        TC_LOG_ERROR("module.playerbot", "🔥 IdleStrategy::UpdateBehavior() CALLED for bot {} - callCounter={}",
                    bot->GetName(), callCounter);
    }

    // IDLE BEHAVIOR COORDINATOR
    // The managers are now updated via UpdateManagers() in BotAI
    // They inherit from BehaviorManager and handle their own throttling
    // No need to call them here anymore

    // Check current quest activity for logging
    uint32 activeQuests = bot->getQuestStatusMap().size();
    if (activeQuests > 0)
    {
        static uint32 questActivityCounter = 0;
        if (++questActivityCounter % 50 == 0)
        {
            TC_LOG_INFO("module.playerbot", "🎯 IdleStrategy: Bot {} actively questing ({} active quests)",
                       bot->GetName(), activeQuests);
        }
    }

    // Fallback - Simple wandering behavior
    // If no other system is active, bot does basic exploration
    uint32 currentTime = getMSTime();
    if (currentTime - _lastWanderTime > _wanderInterval)
    {
        // TODO: Implement proper wandering with pathfinding
        // For now, just log that the bot is truly idle
        static uint32 idleCounter = 0;
        if (++idleCounter % 100 == 0)
        {
            TC_LOG_DEBUG("module.playerbot", "💤 IdleStrategy: Bot {} is idle (no quests, wandering), counter={}",
                        bot->GetName(), idleCounter);
        }

        _lastWanderTime = currentTime;
    }
}

} // namespace Playerbot
