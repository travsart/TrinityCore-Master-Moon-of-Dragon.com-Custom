/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Combat/ThreatManager.h removed - not used in this file

#include "GroupCombatTrigger.h"
#include "../BotAI.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "../Actions/TargetAssistAction.h"
#include "../Actions/Action.h"
#include <algorithm>

namespace Playerbot
{

GroupCombatTrigger::GroupCombatTrigger(std::string const& name)
    : CombatTrigger(name)
{
    // Set higher priority for group combat triggers
    _priority = 150;
}

bool GroupCombatTrigger::Check(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // First check if bot is already in combat
    if (bot->IsInCombat())
        return false; // Already handled by normal combat

    // Check if bot is in a group
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Check if group is in combat and bot should engage
    return ShouldEngageCombat(bot, group);
}

TriggerResult GroupCombatTrigger::Evaluate(BotAI* ai) const
{
    TriggerResult result;

    if (!ai || !ai->GetBot())
        return result;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group || bot->IsInCombat())
        return result;

    // Check if we should engage
    if (!ShouldEngageCombat(bot, group))
        return result;

    // Get the best target to assist
    Unit* target = GetAssistTarget(bot, group);
    if (!target)
        return result;

    // Create the assist action
    result.triggered = true;
    result.urgency = CalculateUrgency(ai);
    result.suggestedAction = std::make_shared<TargetAssistAction>("assist_group");
    result.context.target = target;

    // Log the trigger
    LogCombatEvent("Group combat triggered", bot, target);

    return result;
}

float GroupCombatTrigger::CalculateUrgency(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group)
        return 0.0f;

    // Base urgency for group combat
    float urgency = 0.7f;

    // Increase urgency if leader is in combat
    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (leader && leader != bot && leader->IsInCombat())
    {
        urgency += 0.2f;

        // Even higher if leader is low health
        if (leader->GetHealthPct() < 50.0f)
            urgency = 0.95f;
    }

    // Check how many members are in combat
    uint32 membersInCombat = 0;
    uint32 totalMembers = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            ++totalMembers;
            if (member->IsInCombat())
                ++membersInCombat;
        }
    }

    // Increase urgency based on percentage of group in combat
    if (totalMembers > 0)
    {
        float combatRatio = static_cast<float>(membersInCombat) / totalMembers;
        urgency = std::min(1.0f, urgency + (combatRatio * 0.2f));
    }

    return urgency;
}

bool GroupCombatTrigger::IsGroupInCombat(Group* group) const
{
    if (!group)
        return false;

    // Check cache first if enabled
    if (_cachingEnabled)
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        auto it = _combatCache.find(group->GetGUID());
        if (it != _combatCache.end() && IsCacheValid(group))
        {
            return it->second.inCombat;
        }
    }

    // Update cache and check combat state
    bool inCombat = false;
    uint32 membersInCombat = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (member->IsInCombat())
            {
                inCombat = true;
                ++membersInCombat;
            }
        }
    }

    // Update cache
    if (_cachingEnabled)
    {
        const_cast<GroupCombatTrigger*>(this)->UpdateGroupCombatState(group, inCombat);
    }

    return inCombat;
}

bool GroupCombatTrigger::ShouldEngageCombat(Player* bot, Group* group) const
{
    if (!bot || !group || bot->IsInCombat())
        return false;

    // Check if group is in combat
    if (!IsGroupInCombat(group))
        return false;

    // Check engagement delay
    if (_engagementDelayMs > 0)
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        auto it = _combatCache.find(group->GetGUID());
        if (it != _combatCache.end())
        {
            auto elapsed = std::chrono::steady_clock::now() - it->second.combatStartTime;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < _engagementDelayMs)
                return false; // Still in delay period
        }
    }

    // Check if we have a valid target
    Unit* target = GetAssistTarget(bot, group);
    if (!target)
        return false;

    // Check if target is in range
    if (!IsInEngagementRange(bot, target))
        return false;

    // Check if target is valid for this bot
    if (!IsValidGroupTarget(bot, target))
        return false;

    return true;
}

Unit* GroupCombatTrigger::GetGroupTarget(Group* group) const
{
    if (!group)
        return nullptr;

    std::unordered_map<ObjectGuid, uint32> targetCounts;
    Unit* leaderTarget = nullptr;

    // Count targets being attacked by group members
    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (!member->IsInCombat())
                continue;

            if (Unit* victim = member->GetVictim())
            {
                targetCounts[victim->GetGUID()]++;

                // Track leader's target specifically
                if (member->GetGUID() == group->GetLeaderGUID())
                    leaderTarget = victim;
            }
        }
    }

    // Prioritize leader's target if configured
    if (_prioritizeLeader && leaderTarget)
        return leaderTarget;

    // Find most attacked target
    ObjectGuid bestTargetGuid;
    uint32 maxCount = 0;

    for (auto const& [guid, count] : targetCounts)
    {
        if (count > maxCount)
        {
            maxCount = count;
            bestTargetGuid = guid;
        }
    }

    if (!bestTargetGuid.IsEmpty())
    {
        if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
            return ObjectAccessor::GetUnit(*leader, bestTargetGuid);
    }

    return nullptr;
}

Unit* GroupCombatTrigger::GetLeaderTarget(Group* group) const
{
    if (!group)
        return nullptr;

    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (!leader || !leader->IsInCombat())
        return nullptr;

    return leader->GetVictim();
}

Unit* GroupCombatTrigger::GetAssistTarget(Player* bot, Group* group) const
{
    if (!bot || !group)
        return nullptr;

    // First try leader's target
    Unit* leaderTarget = GetLeaderTarget(group);
    if (leaderTarget && IsValidGroupTarget(bot, leaderTarget) && IsInEngagementRange(bot, leaderTarget))
        return leaderTarget;

    // Then try group's primary target
    Unit* groupTarget = GetGroupTarget(group);
    if (groupTarget && IsValidGroupTarget(bot, groupTarget) && IsInEngagementRange(bot, groupTarget))
        return groupTarget;

    // Find nearest valid target being attacked by group
    Unit* nearestTarget = nullptr;
    float nearestDistance = _maxEngagementRange;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (member == bot || !member->IsInCombat())
                continue;

            if (Unit* victim = member->GetVictim())
            {
                if (!IsValidGroupTarget(bot, victim))
                    continue;

                float distance = bot->GetDistance(victim);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearestTarget = victim;
                }
            }
        }
    }

    return nearestTarget;
}

void GroupCombatTrigger::UpdateGroupCombatState(Group* group, bool inCombat)
{
    if (!group)
        return;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    auto& info = _combatCache[group->GetGUID()];

    // Track combat state changes
    if (!info.inCombat && inCombat)
    {
        // Group entering combat
        info.combatStartTime = std::chrono::steady_clock::now();
        const_cast<CombatStats&>(_stats).totalEngagements++;
    }

    info.inCombat = inCombat;
    info.lastUpdateTime = std::chrono::steady_clock::now();

    // Update member targets
    info.memberTargets.clear();
    info.membersInCombat = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (member->IsInCombat())
            {
                info.membersInCombat++;
                if (Unit* victim = member->GetVictim())
                    info.memberTargets[member->GetGUID()] = victim->GetGUID();
            }
        }
    }
}

uint32 GroupCombatTrigger::GetCombatDuration(Group* group) const
{
    if (!group)
        return 0;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    auto it = _combatCache.find(group->GetGUID());
    if (it == _combatCache.end() || !it->second.inCombat)
        return 0;

    auto elapsed = std::chrono::steady_clock::now() - it->second.combatStartTime;
    return static_cast<uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

bool GroupCombatTrigger::IsInEngagementRange(Player* bot, Unit* target) const
{
    if (!bot || !target)
        return false;

    float distance = bot->GetDistance(target);

    // Check maximum range
    if (distance > _maxEngagementRange)
        return false;

    // For melee classes, ensure they can reach the target
    if (bot->GetClass() == CLASS_WARRIOR || bot->GetClass() == CLASS_ROGUE ||
        bot->GetClass() == CLASS_DEATH_KNIGHT || bot->GetClass() == CLASS_DEMON_HUNTER ||
        bot->GetClass() == CLASS_PALADIN || bot->GetClass() == CLASS_MONK)
    {
        // Allow slightly more range for melee to account for movement
        return distance <= std::max(10.0f, MIN_ENGAGEMENT_RANGE * 2);
    }

    // Ranged classes can engage from further
    return distance <= _maxEngagementRange;
}

bool GroupCombatTrigger::IsValidGroupTarget(Player* bot, Unit* target) const
{
    if (!bot || !target)
        return false;

    // Basic validation
    if (!target->IsAlive() || target->HasUnitState(UNIT_STATE_EVADE))
        return false;

    // Check if target is hostile
    if (!bot->IsHostileTo(target))
        return false;

    // Check if target is attackable (use Unit's CanAttack instead)
    if (!bot->IsValidAttackTarget(target))
        return false;

    // Check line of sight
    if (!bot->IsWithinLOSInMap(target))
        return false;

    // Check if target is immune to damage
    if (target->HasUnitFlag(UNIT_FLAG_IMMUNE_TO_PC))
        return false;

    return true;
}

bool GroupCombatTrigger::IsTargetEngaged(Group* group, Unit* target) const
{
    if (!group || !target)
        return false;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (member->IsInCombat() && member->GetVictim() == target)
                return true;
        }
    }

    return false;
}

void GroupCombatTrigger::SetUpdateInterval(uint32 intervalMs)
{
    _updateIntervalMs = std::clamp(intervalMs, MIN_UPDATE_INTERVAL, MAX_UPDATE_INTERVAL);
}

bool GroupCombatTrigger::UpdateCombatCache(Group* group) const
{
    if (!group)
        return false;

    bool inCombat = false;
    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (member->IsInCombat())
            {
                inCombat = true;
                break;
            }
        }
    }

    const_cast<GroupCombatTrigger*>(this)->UpdateGroupCombatState(group, inCombat);
    return true;
}

bool GroupCombatTrigger::IsCacheValid(Group* group) const
{
    if (!group || !_cachingEnabled)
        return false;

    auto it = _combatCache.find(group->GetGUID());
    if (it == _combatCache.end())
        return false;

    auto elapsed = std::chrono::steady_clock::now() - it->second.lastUpdateTime;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < _updateIntervalMs;
}

float GroupCombatTrigger::CalculateTargetPriority(Group* group, Unit* target) const
{
    if (!group || !target)
        return 0.0f;

    float priority = 0.0f;

    // Base priority from how many members are attacking
    uint32 attackerCount = CountMembersOnTarget(group, target);
    priority += attackerCount * 10.0f;

    // Leader's target gets bonus priority
    if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
    {
        if (leader->GetVictim() == target)
            priority += 20.0f;
    }

    // Lower health targets get slightly higher priority
    float healthPct = target->GetHealthPct();
    if (healthPct < 30.0f)
        priority += 5.0f;
    else if (healthPct < 50.0f)
        priority += 2.0f;

    return priority;
}

uint32 GroupCombatTrigger::CountMembersOnTarget(Group* group, Unit* target) const
{
    if (!group || !target)
        return 0;

    uint32 count = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            if (member->IsInCombat() && member->GetVictim() == target)
                ++count;
        }
    }

    return count;
}

void GroupCombatTrigger::LogCombatEvent(std::string const& event, Player* bot, Unit* target) const
{
    if (!bot)
        return;

    if (target)
    {
        TC_LOG_DEBUG("playerbot", "GroupCombat: {} - Bot: {} ({}), Target: {} ({})",
                     event,
                     bot->GetName(),
                     bot->GetGUID().ToString(),
                     target->GetName(),
                     target->GetGUID().ToString());
    }
    else
    {
        TC_LOG_DEBUG("playerbot", "GroupCombat: {} - Bot: {} ({})",
                     event,
                     bot->GetName(),
                     bot->GetGUID().ToString());
    }
}

void GroupCombatTrigger::UpdateStatistics(bool assistingLeader, std::chrono::milliseconds engagementTime)
{
    _stats.groupCombatTriggers++;

    if (assistingLeader)
        _stats.leaderAssists++;

    // Update average engagement time
    if (_stats.totalEngagements > 0)
    {
        auto total = _stats.averageEngagementTime.count() * (_stats.totalEngagements - 1);
        _stats.averageEngagementTime = std::chrono::milliseconds((total + engagementTime.count()) / _stats.totalEngagements);
    }
    else
    {
        _stats.averageEngagementTime = engagementTime;
    }

    _stats.lastEngagement = std::chrono::steady_clock::now();
}

} // namespace Playerbot