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
#include "../ClassAI/ClassAI.h"  // For ClassAI positioning delegation
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

bool GroupCombatStrategy::IsActive(BotAI* ai) const
{
    // CRITICAL FIX: Only active when bot or group is actually in combat
    // This allows follow strategy to win when out of combat
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Active if bot is in combat OR group is in combat
    return bot->IsInCombat() || IsGroupInCombat(ai);
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

            // CRITICAL FIX: Delegate movement to ClassAI for proper positioning
            // ClassAI knows the bot's optimal range (melee vs ranged)

            float distance = bot->GetDistance(target);

            // CRITICAL: Ensure combat is initiated BEFORE allowing spell casts
            // bot->Attack() makes the target hostile but needs to process
            if (!bot->GetVictim() || bot->GetVictim() != target)
            {
                // Initiate combat with target
                bot->Attack(target, true);
                bot->SetInCombatWith(target);

                // CRITICAL FIX: For neutral mobs, Attack() alone doesn't make them hostile
                // We need to establish threat/aggro manually for the target to react
                if (target->IsCreature() && !target->IsInCombatWith(bot))
                {
                    target->SetInCombatWith(bot);
                    target->GetThreatManager().AddThreat(bot, 1.0f); // Minimal threat to establish aggro
                    TC_LOG_ERROR("module.playerbot.strategy", "⚔️ Establishing threat/aggro with neutral mob {}",
                                target->GetName());
                }

                TC_LOG_ERROR("module.playerbot.strategy", "⚔️ GroupCombatStrategy: Bot {} initiating combat with {} (IsInCombat={}, HasVictim={})",
                            bot->GetName(), target->GetName(), bot->IsInCombat(), bot->GetVictim() != nullptr);

                // Don't return - allow ClassAI combat updates to proceed
                // Note: OnCombatUpdate() is called from BotAI::UpdateAI() when IsInCombat() returns true
                // bot->IsInCombat() should now be true after SetInCombatWith()
            }

            // ALWAYS update movement while target is alive (even during combat)
            // This ensures bot follows moving targets
            if (target->IsAlive())
            {
                // Get optimal range from ClassAI (if available)
                float optimalRange = 5.0f; // Default to melee range
                if (ClassAI* classAI = dynamic_cast<ClassAI*>(ai))
                {
                    optimalRange = classAI->GetOptimalRange(target);
                }

                // CRITICAL FIX: Only issue MoveChase if NOT already chasing
                // Re-issuing every frame causes speed-up and blinking issues
                MotionMaster* mm = bot->GetMotionMaster();
                if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) != CHASE_MOTION_TYPE)
                {
                    mm->MoveChase(target, optimalRange);

                    TC_LOG_ERROR("module.playerbot.strategy", "⚔️ GroupCombatStrategy: Bot {} chasing {} at optimal range {:.1f}yd (current: {:.1f}yd)",
                                bot->GetName(), target->GetName(), optimalRange, distance);
                }
                else
                {
                    TC_LOG_DEBUG("module.playerbot.strategy", "⏭️ GroupCombatStrategy: Bot {} already chasing, skipping", bot->GetName());
                }
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

                    // Combat initiation is handled in UpdateBehavior
                    // Just set target here and return high relevance
                    if (!bot->IsInCombat() && !bot->GetVictim())
                    {
                        float distance = bot->GetDistance(target);

                        TC_LOG_ERROR("module.playerbot.strategy", "⚔️ GroupCombatStrategy (Relevance): Bot {} targeting {} (distance: {:.1f}yd) to assist {}",
                                    bot->GetName(), target->GetName(), distance, member->GetName());
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