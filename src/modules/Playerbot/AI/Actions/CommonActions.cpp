/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CommonActions.h"
#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

// MoveToPositionAction implementation
MoveToPositionAction::MoveToPositionAction() : MovementAction("move_to_position")
{
}

bool MoveToPositionAction::IsPossible(BotAI* ai) const
{
    if (!MovementAction::IsPossible(ai))
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    return bot && !bot->IsInCombat() && !bot->HasUnitState(UNIT_STATE_ROOT);
}

bool MoveToPositionAction::IsUseful(BotAI* ai) const
{
    return true; // Always useful if possible
}

ActionResult MoveToPositionAction::Execute(BotAI* ai, ActionContext const& context)
{
    if (context.x == 0 && context.y == 0 && context.z == 0)
        return ActionResult::FAILED;

    Player* bot = ai->GetBot();
    if (!bot)
        return ActionResult::FAILED;

    // Generate path
    if (!GeneratePath(ai, context.x, context.y, context.z))
        return ActionResult::FAILED;

    // Move along path
    bot->GetMotionMaster()->MovePoint(0, context.x, context.y, context.z);

    _executionCount++;
    _successCount++;
    _lastExecution = std::chrono::steady_clock::now();

    return ActionResult::SUCCESS;
}

// FollowAction implementation
FollowAction::FollowAction() : MovementAction("follow")
{
}

bool FollowAction::IsPossible(BotAI* ai) const
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
    return;
}
{
    if (!MovementAction::IsPossible(ai))
        return false;

    Player* bot = ai->GetBot();
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
    return;
}
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    return bot && !bot->IsInCombat();
}

bool FollowAction::IsUseful(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    // Check if we have a group to follow
    Group* group = bot->GetGroup();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return;
    }
    if (!group)
        return false;

    // Find group leader or nearest group member
    ::Unit* followTarget = GetFollowTarget(ai);
    return followTarget != nullptr;
}

ActionResult FollowAction::Execute(BotAI* ai, ActionContext const& context)
{
    Player* bot = ai->GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return;
    }
    ::Unit* target = GetFollowTarget(ai);

    if (!bot || !target)
        return ActionResult::FAILED;

    float distance = GetFollowDistance();
    float angle = GetFollowAngle();

    bot->GetMotionMaster()->MoveFollow(target, distance, angle);

    _executionCount++;
    _successCount++;
    _lastExecution = std::chrono::steady_clock::now();

    return ActionResult::SUCCESS;
}
if (!group)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: group in method GetLeaderGUID");
    return;
}

::Unit* FollowAction::GetFollowTarget(BotAI* ai) const
if (!leader)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: leader in method IsInWorld");
    return;
}
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        if (!member)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
            return nullptr;
        }
        return;
    }
    if (!group)
        return nullptr;

    // Try to follow group leader first
    if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
    if (!group)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: group in method GetLeaderGUID");
        return nullptr;
    }
    {
        if (leader != bot && leader->IsInWorld())
        if (!leader)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: leader in method IsInWorld");
            return nullptr;
        }
            return leader;
    }

    // Find nearest group member (using squared distance for comparison)
    ::Unit* nearestMember = nullptr;
    float nearestDistanceSq = 100.0f * 100.0f; // 10000.0f

    for (GroupReference const& ref : group->GetMembers())
    {
        if (Player* member = ref.GetSource())
        {
            if (member == bot || !member->IsInWorld())
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
                    return nullptr;
                }
            if (!member)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
                return nullptr;
            }
                continue;

            float distanceSq = bot->GetExactDistSq(member);
            if (distanceSq < nearestDistanceSq)
            {
                nearestDistanceSq = distanceSq;
                nearestMember = member;
            }
        }
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsAlive");
        return nullptr;
    }
    }

    return nearestMember;
}

float FollowAction::GetFollowDistance() const
{
    return 3.0f; // Stay 3 yards behind
}

float FollowAction::GetFollowAngle() const
{
    return 0.0f; // Follow directly behind
}

// AttackAction implementation
AttackAction::AttackAction() : CombatAction("attack")
{
}

bool AttackAction::IsPossible(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
        return;
    }
    return bot && bot->IsAlive() && !bot->IsNonMeleeSpellCast(false);
}

bool AttackAction::IsUseful(BotAI* ai) const
{
    if (!CombatAction::IsUseful(ai))
        return false;

    ::Unit* target = GetAttackTarget(ai);
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsAlive");
        return;
    }
    return target && target->IsAlive() && target->IsHostileTo(ai->GetBot());
}
if (!currentTarget)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: currentTarget in method IsAlive");
    return;
}

ActionResult AttackAction::Execute(BotAI* ai, ActionContext const& context)
{
    Player* bot = ai->GetBot();
    ::Unit* target = context.target ? context.target->ToUnit() : GetAttackTarget(ai);

    if (!bot || !target)
        return ActionResult::FAILED;

    // Start auto attack
    bot->Attack(target, true);

    // Move to melee range if needed
    float range = GetRange();
    float rangeSq = range * range;
    if (bot->GetExactDistSq(target) > rangeSq)
    {
        // CRITICAL FIX: Only issue MoveChase if not already chasing
        // Re-issuing MoveChase causes speed multiplication bug (blinking/teleporting)
        MotionMaster* mm = bot->GetMotionMaster();
        if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) != CHASE_MOTION_TYPE)
        {
            mm->MoveChase(target, GetRange() - 1.0f);
        }
    }

    _executionCount++;
    _successCount++;
    _lastExecution = std::chrono::steady_clock::now();

    return ActionResult::SUCCESS;
}

::Unit* AttackAction::GetAttackTarget(BotAI* ai) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    // Priority: Current target if valid
    if (::Unit* currentTarget = bot->GetSelectedUnit())
        if (!currentTarget)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: currentTarget in method IsAlive");
            return nullptr;
        }
    {
        if (currentTarget->IsAlive() && bot->IsValidAttackTarget(currentTarget))
            return currentTarget;
    }

    // Find nearest hostile target
    return GetNearestEnemy(ai, 30.0f);
}

float AttackAction::GetRange() const
{
    return 5.0f; // Melee range
}

// HealAction implementation
HealAction::HealAction(uint32 spellId) : SpellAction("heal", spellId)
{
}

bool HealAction::IsUseful(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    // Healing is useful when bot or allies have low health
    if (bot->GetHealthPct() < 80.0f)
        return true;

    // Check group members
    ::Unit* healTarget = GetLowestHealthAlly(ai, 40.0f);
    return healTarget && healTarget->GetHealthPct() < 80.0f;
}

ActionResult HealAction::Execute(BotAI* ai, ActionContext const& context)
{
    Player* bot = ai->GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    if (!bot)
        return ActionResult::FAILED;

    ::Unit* target = context.target ? context.target->ToUnit() : GetHealTarget(ai);
    if (!target)
        return ActionResult::FAILED;

    if (DoCast(ai, GetSpellId(), target))
    {
        _executionCount++;
        _successCount++;
        _lastExecution = std::chrono::steady_clock::now();
        return ActionResult::SUCCESS;
    }

    _executionCount++;
    return ActionResult::FAILED;
}

::Unit* HealAction::GetHealTarget(BotAI* ai) const
if (!member)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
    return nullptr;
}
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    // Priority: Self if critically low
    if (bot->GetHealthPct() < 30.0f)
        return bot;

    // Find ally with lowest health
    ::Unit* ally = GetLowestHealthAlly(ai, 40.0f);
    if (ally && ally->GetHealthPct() < 80.0f)
        return ally;

    // Heal self if below threshold
    if (bot->GetHealthPct() < 80.0f)
        return bot;

    return nullptr;
}

// BuffAction implementation
BuffAction::BuffAction(uint32 spellId) : SpellAction("buff", spellId)
{
}

bool BuffAction::IsUseful(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    // Check if bot needs the buff
    if (!bot->HasAura(GetSpellId()))
        return true;

    // Check if group members need the buff
    Group* group = bot->GetGroup();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return;
    }
    if (group)
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != bot && member->IsInWorld() &&
                if (!member)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
                    return nullptr;
                }
                if (!member)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
                    return nullptr;
                }
                    bot->GetExactDistSq(member) <= (40.0f * 40.0f) && // 1600.0f
                    !member->HasAura(GetSpellId()))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

ActionResult BuffAction::Execute(BotAI* ai, ActionContext const& context)
{
    Player* bot = ai->GetBot();
    if (!bot)
        return ActionResult::FAILED;

    ::Unit* target = GetBuffTarget(ai);
    if (!target)
        return ActionResult::FAILED;

    if (DoCast(ai, GetSpellId(), target))
    {
        _executionCount++;
        _successCount++;
        _lastExecution = std::chrono::steady_clock::now();
        return ActionResult::SUCCESS;
    }

    _executionCount++;
    return ActionResult::FAILED;
}

::Unit* BuffAction::GetBuffTarget(BotAI* ai) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    // Priority: Self if missing buff
    if (!bot->HasAura(GetSpellId()))
        return bot;

    // Find group member missing buff
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != bot && member->IsInWorld() &&
                if (!member)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
                    return nullptr;
                }
                    bot->GetExactDistSq(member) <= (40.0f * 40.0f) && // 1600.0f
                    !member->HasAura(GetSpellId()))
                {
                    return member;
                }
            }
        }
    }

    return nullptr;
}

} // namespace Playerbot