/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GroupCombatStrategy.h"
#include "../BotAI.h"
#include "Player.h"
#include "Group.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

GroupCombatStrategy::GroupCombatStrategy()
    : Strategy("group_combat")
{
    TC_LOG_DEBUG("module.playerbot.strategy", "GroupCombatStrategy: Initialized");
}

void GroupCombatStrategy::InitializeActions()
{
    // No actions needed - ClassAI handles combat execution
    TC_LOG_DEBUG("module.playerbot.strategy", "GroupCombatStrategy: No actions (ClassAI handles combat)");
}

void GroupCombatStrategy::InitializeTriggers()
{
    // No triggers needed - relevance system handles activation
    TC_LOG_DEBUG("module.playerbot.strategy", "GroupCombatStrategy: No triggers (using relevance system)");
}

void GroupCombatStrategy::InitializeValues()
{
    // No values needed for this simple strategy
    TC_LOG_DEBUG("module.playerbot.strategy", "GroupCombatStrategy: No values");
}

void GroupCombatStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    // CRITICAL: This is called EVERY FRAME when strategy is active
    // This is where we actually check for group combat and attack!

    static uint32 lastDiagLog = 0;
    uint32 currentTime = getMSTime();
    bool shouldLog = (currentTime - lastDiagLog > 2000); // Every 2 seconds

    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot.strategy", "🔍 GroupCombat: Bot {} - inCombat={}, hasGroup={}",
                    bot->GetName(), bot->IsInCombat(), bot->GetGroup() != nullptr);
        lastDiagLog = currentTime;
    }

    // If bot already in combat, let ClassAI handle it
    if (bot->IsInCombat())
        return;

    // Check if group is in combat
    bool groupInCombat = IsGroupInCombat(ai);
    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot.strategy", "🔍 GroupCombat: Bot {} - groupInCombat={}",
                    bot->GetName(), groupInCombat);
    }

    if (!groupInCombat)
        return;

    // Group is in combat but bot isn't - ASSIST!
    Group* group = bot->GetGroup();
    if (!group)
        return;

    // Find a group member's target to attack
    // Use GetMemberSlots() for reliable GUID access
    for (auto const& slot : group->GetMemberSlots())
    {
        Player* member = ObjectAccessor::FindPlayer(slot.guid);
        if (!member || member == bot || !member->IsInCombat())
            continue;

        Unit* target = member->GetSelectedUnit();

        // If group member has an alive target they're attacking, bot should assist
        // Don't use IsValidAttackTarget() - it fails for neutral mobs
        // The player is already fighting it, so it's valid for the bot to attack
        if (target && target->IsAlive())
        {
            // Set target
            bot->SetTarget(target->GetGUID());

            // CRITICAL FIX: Move toward target BEFORE attacking
            // Attack() only works if in range, so we must chase first
            if (!bot->IsInCombat() && !bot->GetVictim())
            {
                float distance = bot->GetDistance(target);

                // Start chasing the target to get in melee range (chase to 2yd to ensure in range)
                bot->GetMotionMaster()->MoveChase(target, 2.0f);

                TC_LOG_ERROR("module.playerbot.strategy", "⚔️ GroupCombatStrategy: Bot {} CHASING {} (distance: {:.1f}yd) to assist {}",
                            bot->GetName(), target->GetName(), distance, member->GetName());

                // Also call Attack() to set combat state when in range
                bot->Attack(target, true);  // true = melee attack
            }
            break;
        }
    }
}

float GroupCombatStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // If bot already in combat, let ClassAI handle it
    if (bot->IsInCombat())
        return 0.0f;

    // If group is in combat, high relevance to assist
    if (IsGroupInCombat(ai))
    {
        // Engage group member's target
        Group* group = bot->GetGroup();
        if (group)
        {
            for (auto const& slot : group->GetMemberSlots())
            {
                Player* member = ObjectAccessor::FindPlayer(slot.guid);
                if (!member || member == bot || !member->IsInCombat())
                    continue;

                Unit* target = member->GetSelectedUnit();
                if (target && target->IsAlive())
                {
                    // Set target
                    bot->SetTarget(target->GetGUID());

                    // CRITICAL FIX: Move toward target BEFORE attacking
                    // Attack() only works if in range, so we must chase first
                    if (!bot->IsInCombat() && !bot->GetVictim())
                    {
                        float distance = bot->GetDistance(target);

                        // Start chasing the target to get in melee range (chase to 2yd to ensure in range)
                        bot->GetMotionMaster()->MoveChase(target, 2.0f);

                        TC_LOG_ERROR("module.playerbot.strategy", "⚔️ GroupCombatStrategy (Relevance): Bot {} CHASING {} (distance: {:.1f}yd) to assist {}",
                                    bot->GetName(), target->GetName(), distance, member->GetName());

                        // Also call Attack() to set combat state when in range
                        bot->Attack(target, true);  // true = melee attack
                    }
                    break;
                }
            }
        }

        return 80.0f;
    }

    return 0.0f;
}

bool GroupCombatStrategy::IsGroupInCombat(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group)
        return false;

    // DIAGNOSTIC: Log which members we're checking
    static uint32 lastLog = 0;
    uint32 now = getMSTime();
    bool shouldLog = (now - lastLog > 2000);

    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot.strategy", "🔍 Checking group members for combat (bot is {}):", bot->GetName());
        TC_LOG_ERROR("module.playerbot.strategy", "   Group GUID: {}, MemberCount: {}, Leader: {}",
                    group->GetGUID().ToString(),
                    group->GetMembersCount(),
                    group->GetLeaderGUID().ToString());
        lastLog = now;
    }

    // CRITICAL FIX: Use GetMemberSlots() instead of GetMembers()
    // GetMembers()/GetSource() only works for players in the same map
    // GetMemberSlots() gives us GUIDs which we can use with ObjectAccessor
    for (auto const& slot : group->GetMemberSlots())
    {
        ObjectGuid memberGuid = slot.guid;

        if (memberGuid.IsEmpty())
        {
            if (shouldLog)
                TC_LOG_ERROR("module.playerbot.strategy", "  - Empty member GUID");
            continue;
        }

        Player* member = ObjectAccessor::FindPlayer(memberGuid);

        if (shouldLog)
        {
            if (!member)
                TC_LOG_ERROR("module.playerbot.strategy", "  - NULL member for GUID {}", memberGuid.ToString());
            else if (member == bot)
                TC_LOG_ERROR("module.playerbot.strategy", "  - {} (is bot - SKIPPING)", member->GetName());
            else
                TC_LOG_ERROR("module.playerbot.strategy", "  - {} InCombat={}, HasTarget={}",
                            member->GetName(),
                            member->IsInCombat(),
                            member->GetSelectedUnit() != nullptr);
        }

        if (!member || member == bot)
            continue;

        if (member->IsInCombat())
            return true;
    }

    if (shouldLog)
        TC_LOG_ERROR("module.playerbot.strategy", "  ❌ No group members in combat detected");

    return false;
}

} // namespace Playerbot