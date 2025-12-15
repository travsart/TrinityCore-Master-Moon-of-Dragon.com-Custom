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
#include "Unit.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "ThreatManager.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "../../Group/GroupRoleEnums.h"  // For IsPlayerHealer

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
    uint32 currentTime = GameTime::GetGameTimeMS();
    bool shouldLog = (currentTime - lastDiagLog > 2000); // Every 2 seconds
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();
    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot.strategy", " GroupCombat: Bot {} - inCombat={}, hasGroup={}",
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
        TC_LOG_ERROR("module.playerbot.strategy", " GroupCombat: Bot {} - groupInCombat={}",
                    bot->GetName(), groupInCombat);
    }

    if (!groupInCombat)
        return;

    // Group is in combat but bot isn't - ASSIST!
    // Use FindGroupCombatTarget() to properly detect attackers
    Unit* target = FindGroupCombatTarget(ai);

    // HEALER FIX: Healers should enter combat state but NOT attack enemies
    // They need to be in combat mode for their healing rotation to trigger,
    // but they should focus on healing group members, not attacking.
    bool isHealer = IsPlayerHealer(bot);

    if (isHealer)
    {
        // HEALER: Enter combat state but DON'T attack
        // Just set combat flag so OnCombatUpdate() triggers healing rotation
        if (!bot->IsInCombat() && target)
        {
            // Put healer in combat with the enemy (for combat state tracking)
            // but don't call Attack() - healers don't auto-attack
            bot->SetInCombatWith(target);

            TC_LOG_ERROR("module.playerbot.strategy",
                "💚 GroupCombatStrategy: HEALER {} entering combat state (NOT attacking) - will heal group",
                bot->GetName());
        }

        // Healers should position at healing range (30yd from group center, not enemies)
        // Find a group member to stay near (preferably the tank)
        Group* group = bot->GetGroup();
        if (group)
        {
            Player* tankToFollow = nullptr;
            for (auto const& slot : group->GetMemberSlots())
            {
                Player* member = ObjectAccessor::FindPlayer(slot.guid);
                if (!member || member == bot || !member->IsAlive())
                    continue;

                if (IsPlayerTank(member))
                {
                    tankToFollow = member;
                    break;
                }
            }

            // If no tank found, follow the group leader or any member in combat
            if (!tankToFollow)
            {
                for (auto const& slot : group->GetMemberSlots())
                {
                    Player* member = ObjectAccessor::FindPlayer(slot.guid);
                    if (member && member != bot && member->IsAlive() && member->IsInCombat())
                    {
                        tankToFollow = member;
                        break;
                    }
                }
            }

            if (tankToFollow)
            {
                float healerRange = 25.0f; // Stay at healing range
                float currentDist = bot->GetExactDist(tankToFollow);

                // Only move if too far from group (>30yd) or too close (<15yd from combat)
                if (currentDist > 30.0f)
                {
                    MotionMaster* mm = bot->GetMotionMaster();
                    if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) != FOLLOW_MOTION_TYPE)
                    {
                        mm->MoveFollow(tankToFollow, healerRange, 0);
                        TC_LOG_DEBUG("module.playerbot.strategy",
                            "💚 Healer {} following {} at {:.1f}yd (current: {:.1f}yd)",
                            bot->GetName(), tankToFollow->GetName(), healerRange, currentDist);
                    }
                }
            }
        }
        return; // Healers don't proceed to attack logic
    }

    // DPS/TANK: Standard combat initiation
    if (target && target->IsAlive())
    {
        // Set target
        bot->SetTarget(target->GetGUID());

        float distance = ::std::sqrt(bot->GetExactDistSq(target));

        // Initiate combat if not already fighting this target
        if (!bot->GetVictim() || bot->GetVictim() != target)
        {
            // For neutral mobs, make THEM attack US first
            if (Creature* targetCreature = target->ToCreature())
            {
                if (targetCreature->CanHaveThreatList())
                {
                    targetCreature->GetThreatManager().AddThreat(bot, 1.0f);
                    TC_LOG_ERROR("module.playerbot.strategy",
                        "⚔️ THREAT ADDED: Bot {} added threat to {} (Entry: {})",
                        bot->GetName(), targetCreature->GetName(), targetCreature->GetEntry());
                }

                if (CreatureAI* creatureAI = targetCreature->AI())
                {
                    creatureAI->AttackStart(bot);
                    TC_LOG_ERROR("module.playerbot.strategy",
                        "⚔️ CREATURE ENGAGED: {} AttackStart() on bot {}",
                        targetCreature->GetName(), bot->GetName());
                }
            }

            // Initiate combat
            bot->Attack(target, true);
            bot->SetInCombatWith(target);
            target->SetInCombatWith(bot);

            TC_LOG_ERROR("module.playerbot.strategy",
                "⚔️ GroupCombatStrategy: Bot {} initiating combat with {} (IsInCombat={}, HasVictim={})",
                bot->GetName(), target->GetName(), bot->IsInCombat(), bot->GetVictim() != nullptr);
        }

        // Update movement to chase target
        if (target->IsAlive())
        {
            float optimalRange = 5.0f; // Default melee
            if (ClassAI* classAI = dynamic_cast<ClassAI*>(ai))
            {
                optimalRange = classAI->GetOptimalRange(target);
            }

            MotionMaster* mm = bot->GetMotionMaster();
            if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) != CHASE_MOTION_TYPE)
            {
                mm->MoveChase(target, optimalRange);
                TC_LOG_ERROR("module.playerbot.strategy",
                    "⚔️ Bot {} chasing {} at {:.1f}yd (current: {:.1f}yd)",
                    bot->GetName(), target->GetName(), optimalRange, distance);
            }
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
        // Use FindGroupCombatTarget to properly detect attackers
        Unit* target = FindGroupCombatTarget(ai);
        if (target && target->IsAlive())
        {
            // Set target for combat initiation (handled in UpdateBehavior)
            bot->SetTarget(target->GetGUID());

            if (!bot->IsInCombat() && !bot->GetVictim())
            {
                float distance = ::std::sqrt(bot->GetExactDistSq(target));
                TC_LOG_ERROR("module.playerbot.strategy",
                    "⚔️ GroupCombatStrategy (Relevance): Bot {} targeting {} (distance: {:.1f}yd)",
                    bot->GetName(), target->GetName(), distance);
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
    uint32 now = GameTime::GetGameTimeMS();
    bool shouldLog = (now - lastLog > 2000);

    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot.strategy", " Checking group members for combat (bot is {}):", bot->GetName());
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
        TC_LOG_ERROR("module.playerbot.strategy", "   No group members in combat detected");

    return false;
}

Unit* GroupCombatStrategy::FindGroupCombatTarget(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* bestTarget = nullptr;
    float bestPriority = 0.0f;

    // CRITICAL FIX: Iterate through all group members and find:
    // 1. Enemies attacking group members (highest priority)
    // 2. What group members are attacking (GetVictim)
    // 3. What group members have selected (GetSelectedUnit)
    for (auto const& slot : group->GetMemberSlots())
    {
        Player* member = ObjectAccessor::FindPlayer(slot.guid);
        if (!member || member == bot || !member->IsInCombat())
            continue;

        // PRIORITY 1: Find enemies attacking this group member (highest priority)
        // Check the member's threat manager for creatures threatening them
        if (member->IsInCombat())
        {
            // Get nearby hostile creatures that might be attacking the member
            std::list<Unit*> nearbyHostiles;
            Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(member, member, 40.0f);
            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(member, nearbyHostiles, check);
            Cell::VisitAllObjects(member, searcher, 40.0f);

            for (Unit* hostile : nearbyHostiles)
            {
                if (!hostile || !hostile->IsAlive())
                    continue;

                // Check if this hostile has threat on our group member
                if (Creature* creature = hostile->ToCreature())
                {
                    if (creature->CanHaveThreatList())
                    {
                        ThreatManager& threatMgr = creature->GetThreatManager();
                        // Check if group member is in this creature's threat list
                        if (threatMgr.GetThreat(member, true) > 0)
                        {
                            // This enemy IS attacking our group member!
                            float priority = 100.0f + threatMgr.GetThreat(member, true);

                            // Higher priority if attacking tank/healer
                            // Priority boost for protecting tanks and healers
                            if (priority > bestPriority)
                            {
                                bestTarget = hostile;
                                bestPriority = priority;

                                TC_LOG_DEBUG("module.playerbot.strategy",
                                    "🎯 FindGroupCombatTarget: Found {} attacking {} (threat: {:.1f})",
                                    creature->GetName(), member->GetName(), threatMgr.GetThreat(member, true));
                            }
                        }
                    }
                }
            }
        }

        // PRIORITY 2: What the group member is currently fighting (GetVictim)
        if (Unit* memberVictim = member->GetVictim())
        {
            if (memberVictim->IsAlive() && bestPriority < 50.0f)
            {
                bestTarget = memberVictim;
                bestPriority = 50.0f;

                TC_LOG_DEBUG("module.playerbot.strategy",
                    "🎯 FindGroupCombatTarget: {} is fighting {} (GetVictim)",
                    member->GetName(), memberVictim->GetName());
            }
        }

        // PRIORITY 3: What the group member has selected (lowest priority)
        if (Unit* selectedTarget = member->GetSelectedUnit())
        {
            if (selectedTarget->IsAlive() && bestPriority < 10.0f)
            {
                // Verify it's actually hostile
                if (bot->IsValidAttackTarget(selectedTarget) ||
                    (selectedTarget->IsInCombat() && !bot->IsFriendlyTo(selectedTarget)))
                {
                    bestTarget = selectedTarget;
                    bestPriority = 10.0f;

                    TC_LOG_DEBUG("module.playerbot.strategy",
                        "🎯 FindGroupCombatTarget: {} has {} selected (GetSelectedUnit)",
                        member->GetName(), selectedTarget->GetName());
                }
            }
        }
    }

    if (bestTarget)
    {
        TC_LOG_ERROR("module.playerbot.strategy",
            "🎯 GroupCombatStrategy: Bot {} found target {} (priority: {:.1f})",
            bot->GetName(), bestTarget->GetName(), bestPriority);
    }

    return bestTarget;
}

} // namespace Playerbot