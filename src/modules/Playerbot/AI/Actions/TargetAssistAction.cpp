/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "TargetAssistAction.h"
#include "BotAI.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include <algorithm>
#include <limits>

namespace Playerbot
{

TargetAssistAction::TargetAssistAction(std::string const& name)
    : CombatAction(name)
{
    // Set higher relevance for assist actions
    _relevance = 0.9f;
}

bool TargetAssistAction::IsPossible(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Can't assist if dead or incapacitated
    if (!bot->IsAlive() || bot->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING))
        return false;

    // Need to be in a group
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Check if we have valid assist targets
    Unit* target = GetBestAssistTarget(bot, group);
    return target != nullptr;
}

bool TargetAssistAction::IsUseful(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Not useful if already attacking the right target
    Unit* currentVictim = bot->GetVictim();
    if (currentVictim)
    {
        Group* group = bot->GetGroup();
        if (group)
        {
            Unit* bestTarget = GetBestAssistTarget(bot, group);
            if (currentVictim == bestTarget)
                return false; // Already assisting the right target
        }
    }

    return IsPossible(ai);
}

ActionResult TargetAssistAction::Execute(BotAI* ai, ActionContext const& context)
{
    if (!ai || !ai->GetBot())
        return ActionResult::FAILED;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group)
        return ActionResult::FAILED;

    // Get target from context or find best
    Unit* target = nullptr;
    if (context.target && context.target->IsUnit())
    {
        target = context.target->ToUnit();
        if (!IsValidAssistTarget(target, bot))
            target = nullptr;
    }

    if (!target)
        target = GetBestAssistTarget(bot, group);

    if (!target)
        return ActionResult::IMPOSSIBLE;

    // Check if we need to switch targets
    Unit* currentVictim = bot->GetVictim();
    if (currentVictim && currentVictim != target)
    {
        if (!ShouldSwitchTarget(bot, target))
            return ActionResult::FAILED;

        if (!SwitchTarget(bot, target))
            return ActionResult::FAILED;
    }
    else if (!currentVictim)
    {
        // Not in combat yet, engage the target
        if (!EngageTarget(bot, target))
            return ActionResult::FAILED;
    }

    // Move to position if needed
    if (!IsInAssistRange(bot, target))
    {
        if (!MoveToAssistPosition(bot, target))
            return ActionResult::IN_PROGRESS;
    }

    // Update statistics
    bool assistedLeader = false;
    if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
    {
        if (leader->GetVictim() == target)
            assistedLeader = true;
    }

    auto now = std::chrono::steady_clock::now();
    auto switchTime = std::chrono::milliseconds(0);

    auto it = _lastTargetSwitch.find(bot->GetGUID());
    if (it != _lastTargetSwitch.end())
    {
        switchTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
    }

    _lastTargetSwitch[bot->GetGUID()] = now;
    UpdateStatistics(assistedLeader, switchTime);

    LogAssistAction("Assisting target", bot, target);

    return ActionResult::SUCCESS;
}

Unit* TargetAssistAction::GetLeaderTarget(Player* leader) const
{
    if (!leader || !leader->IsInCombat())
        return nullptr;

    return leader->GetVictim();
}

Unit* TargetAssistAction::GetBestAssistTarget(Player* bot, Group* group) const
{
    if (!bot || !group)
        return nullptr;

    // Evaluate all potential targets
    auto targets = EvaluateTargets(bot, group);

    if (targets.empty())
        return nullptr;

    // Select the best one
    ObjectGuid bestGuid = SelectBestTarget(targets, bot);

    if (bestGuid.IsEmpty())
        return nullptr;

    return ObjectAccessor::GetUnit(*bot, bestGuid);
}

bool TargetAssistAction::IsValidAssistTarget(Unit* target, Player* bot) const
{
    if (!target || !bot)
        return false;

    // Basic validation
    if (!target->IsAlive())
        return false;

    // Check if hostile
    if (!bot->IsHostileTo(target))
        return false;

    // Check if attackable
    if (!bot->CanAttack(target))
        return false;

    // Check for immunity
    if (target->HasUnitFlag(UNIT_FLAG_IMMUNE_TO_PC))
        return false;

    // Check for evade
    if (target->HasUnitState(UNIT_STATE_EVADE))
        return false;

    // Check crowd control if configured
    if (_avoidCrowdControlled && IsTargetCrowdControlled(target))
        return false;

    // Check line of sight
    if (!HasLineOfSight(bot, target))
        return false;

    // Check range
    if (!IsInAssistRange(bot, target))
        return false;

    return true;
}

bool TargetAssistAction::ShouldSwitchTarget(Player* bot, Unit* newTarget) const
{
    if (!bot || !newTarget)
        return false;

    Unit* currentTarget = bot->GetVictim();
    if (!currentTarget)
        return true; // No current target, should switch

    // Don't switch too frequently
    auto it = _lastTargetSwitch.find(bot->GetGUID());
    if (it != _lastTargetSwitch.end())
    {
        auto elapsed = std::chrono::steady_clock::now() - it->second;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < _switchDelayMs)
            return false;
    }

    // Switch if current target is about to die
    if (currentTarget->GetHealthPct() < _switchHealthThreshold)
        return true;

    // Switch if current target is no longer valid
    if (!IsValidAssistTarget(currentTarget, bot))
        return true;

    // Switch if new target has significantly higher priority
    Group* group = bot->GetGroup();
    if (group)
    {
        float currentPriority = CalculateAssistPriority(bot, currentTarget, group);
        float newPriority = CalculateAssistPriority(bot, newTarget, group);

        // Switch if new target has 50% higher priority
        if (newPriority > currentPriority * 1.5f)
            return true;
    }

    return false;
}

bool TargetAssistAction::EngageTarget(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    // Stop any current attack
    if (bot->GetVictim())
        bot->AttackStop();

    // Start attacking new target
    if (!bot->Attack(target, true))
        return false;

    // Set AI target
    if (BotAI* ai = dynamic_cast<BotAI*>(bot->GetAI()))
        ai->SetTarget(target->GetGUID());

    _stats.totalAssists++;

    return true;
}

bool TargetAssistAction::SwitchTarget(Player* bot, Unit* newTarget)
{
    if (!bot || !newTarget)
        return false;

    // Stop current attack
    bot->AttackStop();

    // Engage new target
    if (!EngageTarget(bot, newTarget))
        return false;

    _stats.targetSwitches++;

    return true;
}

float TargetAssistAction::CalculateAssistPriority(Player* bot, Unit* target, Group* group) const
{
    if (!bot || !target || !group)
        return 0.0f;

    float priority = 0.0f;

    // Count how many group members are attacking this target
    uint32 assistCount = CountAssistingMembers(target, group);
    priority += assistCount * 10.0f;

    // Leader's target gets significant bonus
    if (_prioritizeLeader)
    {
        Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
        if (leader && leader->GetVictim() == target)
            priority += LEADER_PRIORITY_BONUS;
    }

    // Low health targets get bonus
    float healthPct = target->GetHealthPct();
    if (healthPct < 30.0f)
        priority += LOW_HEALTH_BONUS;
    else if (healthPct < 50.0f)
        priority += LOW_HEALTH_BONUS * 0.5f;

    // Distance factor - closer targets preferred
    float distance = bot->GetDistance(target);
    float maxRange = GetClassAssistRange(bot);
    if (distance <= maxRange)
        priority += (1.0f - (distance / maxRange)) * 5.0f;

    // Threat factor
    float threatFactor = CalculateThreatFactor(bot, target);
    priority += threatFactor;

    // Caster priority - interrupt important casts
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        if (CanInterruptTarget(bot, target))
            priority += 15.0f;
    }

    return priority;
}

bool TargetAssistAction::IsInAssistRange(Player* bot, Unit* target) const
{
    if (!bot || !target)
        return false;

    float distance = bot->GetDistance(target);
    float maxRange = GetClassAssistRange(bot);

    return distance <= maxRange;
}

bool TargetAssistAction::GetAssistPosition(Player* bot, Unit* target, float& x, float& y, float& z) const
{
    if (!bot || !target)
        return false;

    // Get appropriate range for class
    float optimalRange = GetClassAssistRange(bot) * 0.8f; // Stay within 80% of max range

    // Calculate position based on class role
    float angle = target->GetAngle(bot);

    // Melee classes should spread around the target
    if (optimalRange <= MELEE_ASSIST_RANGE)
    {
        // Find a less crowded side
        float bestAngle = angle;
        uint32 minCount = std::numeric_limits<uint32>::max();

        for (float testAngle = 0; testAngle < 2 * M_PI; testAngle += M_PI / 4)
        {
            uint32 count = 0;
            // Count players at this angle
            if (Group* group = bot->GetGroup())
            {
                for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    if (Player* member = itr->GetSource())
                    {
                        if (member == bot || !member->IsInCombat())
                            continue;

                        float memberAngle = target->GetAngle(member);
                        if (std::abs(memberAngle - testAngle) < M_PI / 8)
                            count++;
                    }
                }
            }

            if (count < minCount)
            {
                minCount = count;
                bestAngle = testAngle;
            }
        }

        angle = bestAngle;
    }

    // Calculate position
    x = target->GetPositionX() + std::cos(angle) * optimalRange;
    y = target->GetPositionY() + std::sin(angle) * optimalRange;
    z = target->GetPositionZ();

    // Adjust Z for terrain
    bot->UpdateGroundPositionZ(x, y, z);

    return true;
}

bool TargetAssistAction::MoveToAssistPosition(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    float x, y, z;
    if (!GetAssistPosition(bot, target, x, y, z))
        return false;

    // Move to position
    if (BotAI* ai = dynamic_cast<BotAI*>(bot->GetAI()))
    {
        ai->MoveTo(x, y, z);
        return true;
    }

    return false;
}

bool TargetAssistAction::IsTargetUnderAttack(Unit* target, Group* group) const
{
    if (!target || !group)
        return false;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
        {
            if (member->GetVictim() == target)
                return true;
        }
    }

    return false;
}

uint32 TargetAssistAction::CountAssistingMembers(Unit* target, Group* group) const
{
    if (!target || !group)
        return 0;

    uint32 count = 0;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
        {
            if (member->IsInCombat() && member->GetVictim() == target)
                count++;
        }
    }

    return count;
}

bool TargetAssistAction::IsTargetCrowdControlled(Unit* target) const
{
    if (!target)
        return false;

    // Check for common CC states
    if (target->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING))
        return true;

    // Check for specific CC auras (polymorph, hex, etc.)
    // This would need to be expanded with specific spell IDs

    return false;
}

float TargetAssistAction::GetClassAssistRange(Player* bot) const
{
    if (!bot)
        return _maxAssistRange;

    // Determine range based on class
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_ROGUE:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return std::min(MELEE_ASSIST_RANGE, _maxAssistRange);

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_EVOKER:
            return std::min(RANGED_ASSIST_RANGE, _maxAssistRange);

        default:
            return _maxAssistRange;
    }
}

bool TargetAssistAction::ShouldClassAssist(Player* bot) const
{
    if (!bot)
        return false;

    // All DPS and tank classes should assist
    // Healers might not always assist depending on situation

    // This is a simplified check - in reality would check spec/role
    return true;
}

std::unordered_map<ObjectGuid, TargetAssistAction::TargetInfo> TargetAssistAction::EvaluateTargets(Player* bot, Group* group) const
{
    std::unordered_map<ObjectGuid, TargetInfo> targets;

    if (!bot || !group)
        return targets;

    // Collect all targets being attacked by group members
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
        {
            if (!member->IsInCombat())
                continue;

            if (Unit* victim = member->GetVictim())
            {
                if (!IsValidAssistTarget(victim, bot))
                    continue;

                auto& info = targets[victim->GetGUID()];
                info.guid = victim->GetGUID();
                info.assistingCount++;
                info.distance = bot->GetDistance(victim);
                info.healthPct = victim->GetHealthPct();

                if (member->GetGUID() == group->GetLeaderGUID())
                    info.isLeaderTarget = true;

                if (info.firstSeen == std::chrono::steady_clock::time_point{})
                    info.firstSeen = std::chrono::steady_clock::now();
            }
        }
    }

    // Calculate priorities
    for (auto& [guid, info] : targets)
    {
        if (Unit* target = ObjectAccessor::GetUnit(*bot, guid))
            info.priority = CalculateAssistPriority(bot, target, group);
    }

    return targets;
}

ObjectGuid TargetAssistAction::SelectBestTarget(std::unordered_map<ObjectGuid, TargetInfo> const& targets, Player* bot) const
{
    if (targets.empty() || !bot)
        return ObjectGuid::Empty;

    ObjectGuid bestGuid;
    float bestPriority = -1.0f;

    for (auto const& [guid, info] : targets)
    {
        // Prioritize leader's target if configured
        if (_prioritizeLeader && info.isLeaderTarget)
            return guid;

        if (info.priority > bestPriority)
        {
            bestPriority = info.priority;
            bestGuid = guid;
        }
    }

    return bestGuid;
}

float TargetAssistAction::CalculateThreatFactor(Player* bot, Unit* target) const
{
    if (!bot || !target)
        return 0.0f;

    // Simple threat calculation
    // In real implementation, would use ThreatManager

    float factor = 0.0f;

    // Check if target is attacking the bot
    if (target->GetVictim() == bot)
        factor += 20.0f;

    // Check if target is attacking a healer
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* member = itr->GetSource())
            {
                // Simple healer check - would need proper role detection
                if (member->getClass() == CLASS_PRIEST || member->getClass() == CLASS_PALADIN ||
                    member->getClass() == CLASS_DRUID || member->getClass() == CLASS_SHAMAN)
                {
                    if (target->GetVictim() == member)
                        factor += 15.0f;
                }
            }
        }
    }

    return factor;
}

bool TargetAssistAction::CanInterruptTarget(Player* bot, Unit* target) const
{
    if (!bot || !target)
        return false;

    // Check if target is casting
    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return false;

    // Check if bot has interrupt abilities available
    // This would need class-specific spell checks

    return false; // Simplified for now
}

void TargetAssistAction::LogAssistAction(std::string const& action, Player* bot, Unit* target) const
{
    if (!bot)
        return;

    if (target)
    {
        TC_LOG_DEBUG("playerbot", "TargetAssist: {} - Bot: {} ({}), Target: {} ({})",
                     action,
                     bot->GetName(),
                     bot->GetGUID().ToString(),
                     target->GetName(),
                     target->GetGUID().ToString());
    }
    else
    {
        TC_LOG_DEBUG("playerbot", "TargetAssist: {} - Bot: {} ({})",
                     action,
                     bot->GetName(),
                     bot->GetGUID().ToString());
    }
}

void TargetAssistAction::UpdateStatistics(bool assistedLeader, std::chrono::milliseconds switchTime)
{
    _stats.totalAssists++;

    if (assistedLeader)
        _stats.leaderAssists++;

    // Update average switch time
    if (_stats.targetSwitches > 0)
    {
        auto total = _stats.averageSwitchTime.count() * (_stats.targetSwitches - 1);
        _stats.averageSwitchTime = std::chrono::milliseconds((total + switchTime.count()) / _stats.targetSwitches);
    }
    else
    {
        _stats.averageSwitchTime = switchTime;
    }

    _stats.lastAssist = std::chrono::steady_clock::now();
}

bool TargetAssistAction::HasLineOfSight(Player* bot, Unit* target) const
{
    if (!bot || !target)
        return false;

    return bot->IsWithinLOSInMap(target);
}

bool TargetAssistAction::RequiresFacing(Player* bot) const
{
    if (!bot)
        return true;

    // Most classes need to face their target
    // Only exception might be some instant cast abilities
    return true;
}

} // namespace Playerbot