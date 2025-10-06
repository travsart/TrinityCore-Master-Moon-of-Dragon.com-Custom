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
#include "Economy/AuctionManager.h"

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

    // ========================================================================
    // PHASE 2.5: OBSERVER PATTERN IMPLEMENTATION
    // ========================================================================
    // IdleStrategy observes manager states via atomic queries (<0.001ms each)
    // Managers self-throttle (1s-10s intervals) via BotAI::UpdateManagers()
    // This achieves <0.1ms UpdateBehavior() performance target
    // ========================================================================

    // Query manager states atomically (lock-free, <0.001ms per query)
    bool isQuesting = ai->GetQuestManager() && ai->GetQuestManager()->IsQuestingActive();
    bool isGathering = ai->GetGatheringManager() && ai->GetGatheringManager()->IsGathering();
    bool isTrading = ai->GetTradeManager() && ai->GetTradeManager()->IsTradingActive();
    bool hasAuctions = ai->GetAuctionManager() && ai->GetAuctionManager()->HasActiveAuctions();

    // Determine current bot activity state
    bool isBusy = isQuesting || isGathering || isTrading || hasAuctions;

    // Periodic activity logging (every 5 seconds)
    static uint32 activityLogTimer = 0;
    activityLogTimer += diff;
    if (activityLogTimer >= 5000)
    {
        TC_LOG_DEBUG("module.playerbot",
            "IdleStrategy: Bot {} - Questing:{} Gathering:{} Trading:{} Auctions:{} Busy:{}",
            bot->GetName(), isQuesting, isGathering, isTrading, hasAuctions, isBusy);
        activityLogTimer = 0;
    }

    // If bot is busy with any manager activity, skip wandering
    if (isBusy)
        return;

    // ========================================================================
    // FALLBACK: SIMPLE WANDERING BEHAVIOR
    // ========================================================================
    // If no manager is active, bot does basic idle exploration
    // This is the lowest-priority activity
    // ========================================================================

    uint32 currentTime = getMSTime();
    if (currentTime - _lastWanderTime > _wanderInterval)
    {
        // TODO: Implement proper wandering with pathfinding
        // For now, just log that the bot is truly idle
        TC_LOG_TRACE("module.playerbot",
            "IdleStrategy: Bot {} is idle (no active managers), considering wandering",
            bot->GetName());

        _lastWanderTime = currentTime;
    }
}

} // namespace Playerbot
