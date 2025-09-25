/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LeaderFollowBehavior.h"
#include "BotAI.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "Combat/GroupCombatTrigger.h"
#include "Actions/TargetAssistAction.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

LeaderFollowBehavior::LeaderFollowBehavior()
    : Strategy("leader_follow")
{
    _priority = 200; // High priority for group behavior
}

void LeaderFollowBehavior::InitializeActions()
{
    // Create and register the assist action
    _assistAction = std::make_shared<TargetAssistAction>("group_assist");
    AddAction("group_assist", _assistAction);

    // Add movement actions
    // These would be specific movement actions for following
    // For now, using the assist action as primary
}

void LeaderFollowBehavior::InitializeTriggers()
{
    // Create and register the group combat trigger
    _combatTrigger = std::make_shared<GroupCombatTrigger>("group_combat");
    _combatTrigger->SetAction(_assistAction);
    AddTrigger(_combatTrigger);
}

void LeaderFollowBehavior::InitializeValues()
{
    // Initialize strategy-specific values
    // These would be used for decision making
}

float LeaderFollowBehavior::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // Strategy is relevant if bot is in a group
    Group* group = bot->GetGroup();
    if (!group)
        return 0.0f;

    // Check if bot is not the leader
    if (group->GetLeaderGUID() == bot->GetGUID())
        return 0.0f; // Leaders don't follow themselves

    // High relevance if in group and not leader
    float relevance = 0.8f;

    // Higher relevance if group is in combat
    if (IsGroupInCombat(group))
        relevance = 1.0f;

    return relevance;
}

StrategyRelevance LeaderFollowBehavior::CalculateRelevance(BotAI* ai) const
{
    StrategyRelevance relevance;

    if (!ai || !ai->GetBot())
        return relevance;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group || group->GetLeaderGUID() == bot->GetGUID())
        return relevance;

    // Calculate combat relevance
    if (IsGroupInCombat(group))
    {
        relevance.combatRelevance = 1.0f;
        relevance.survivalRelevance = 0.8f;
    }

    // Social relevance is always high in groups
    relevance.socialRelevance = 0.9f;

    return relevance;
}

bool LeaderFollowBehavior::IsActive(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    // Active if in a group and not the leader
    return group && group->GetLeaderGUID() != bot->GetGUID();
}

void LeaderFollowBehavior::OnActivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group)
        return;

    // Store the leader
    _currentLeader = group->GetLeaderGUID();
    _isFollowing = true;

    // Initialize components
    InitializeActions();
    InitializeTriggers();
    InitializeValues();

    LogBehaviorEvent("LeaderFollowBehavior activated", ai);

    _stats.followCommands++;
}

void LeaderFollowBehavior::OnDeactivate(BotAI* ai)
{
    if (!ai)
        return;

    _isFollowing = false;
    _currentLeader = ObjectGuid::Empty;

    StopFollowing(ai);

    LogBehaviorEvent("LeaderFollowBehavior deactivated", ai);
}

Player* LeaderFollowBehavior::GetLeader(Player* bot) const
{
    if (!bot)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    return ObjectAccessor::FindPlayer(group->GetLeaderGUID());
}

bool LeaderFollowBehavior::ShouldFollowLeader(Player* bot, Player* leader) const
{
    if (!bot || !leader)
        return false;

    // Don't follow if leader is dead
    if (!leader->IsAlive())
        return false;

    // Don't follow if bot is dead
    if (!bot->IsAlive())
        return false;

    // Don't follow if in combat (unless configured to)
    if (bot->IsInCombat() && !_combatAssistEnabled)
        return false;

    // Check distance
    float distance = bot->GetDistance(leader);
    if (distance > _maxFollowDistance)
        return false; // Too far, might have teleported

    // Check if in same zone
    if (bot->GetZoneId() != leader->GetZoneId())
        return false;

    return true;
}

bool LeaderFollowBehavior::CalculateFollowPosition(Player* bot, Player* leader, float& x, float& y, float& z) const
{
    if (!bot || !leader)
        return false;

    // Get follow distance based on role
    float followDistance = GetFollowDistance(bot);

    // Calculate base angle
    float angle = leader->GetOrientation();

    // Get formation position if in group
    Group* group = bot->GetGroup();
    if (group && _formationEnabled)
    {
        // Find bot's position in group
        uint32 index = 0;
        bool found = false;

        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (itr->GetSource() == bot)
            {
                found = true;
                break;
            }
            if (itr->GetSource() != leader)
                index++;
        }

        if (found)
        {
            return GetFormationPosition(bot, group, index, x, y, z);
        }
    }

    // Simple follow position behind leader
    angle = GetRolePositionOffset(bot, angle, followDistance);

    x = leader->GetPositionX() - std::cos(angle) * followDistance;
    y = leader->GetPositionY() - std::sin(angle) * followDistance;
    z = leader->GetPositionZ();

    // Adjust Z for terrain
    bot->UpdateGroundPositionZ(x, y, z);

    return true;
}

float LeaderFollowBehavior::GetFollowDistance(Player* bot) const
{
    if (!bot)
        return _minFollowDistance;

    // Determine distance based on class/role
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return TANK_FOLLOW_DISTANCE;

        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_PALADIN:
            // Check if healer spec (simplified)
            return HEALER_FOLLOW_DISTANCE;

        default:
            return DPS_FOLLOW_DISTANCE;
    }
}

bool LeaderFollowBehavior::GetFormationPosition(Player* bot, Group* group, uint32 index, float& x, float& y, float& z) const
{
    if (!bot || !group)
        return false;

    Player* leader = GetLeader(bot);
    if (!leader)
        return false;

    float spacing = _inCombatFormation ? COMBAT_FORMATION_SPACING : FORMATION_SPACING;
    float baseAngle = leader->GetOrientation();

    switch (_formationType)
    {
        case FormationType::SINGLE_FILE:
        {
            // Line formation behind leader
            float distance = (index + 1) * spacing;
            x = leader->GetPositionX() - std::cos(baseAngle) * distance;
            y = leader->GetPositionY() - std::sin(baseAngle) * distance;
            z = leader->GetPositionZ();
            break;
        }

        case FormationType::SPREAD:
        {
            // V formation
            float side = (index % 2 == 0) ? 1.0f : -1.0f;
            float row = (index / 2) + 1;
            float distance = row * spacing;
            float offset = row * spacing * 0.5f * side;

            float angle = baseAngle + (side * M_PI / 6); // 30 degree spread
            x = leader->GetPositionX() - std::cos(baseAngle) * distance + std::cos(baseAngle + M_PI/2) * offset;
            y = leader->GetPositionY() - std::sin(baseAngle) * distance + std::sin(baseAngle + M_PI/2) * offset;
            z = leader->GetPositionZ();
            break;
        }

        case FormationType::TIGHT:
        {
            // Close square formation
            uint32 perRow = 3;
            uint32 row = index / perRow;
            uint32 col = index % perRow;

            float distance = (row + 1) * spacing;
            float offset = (col - 1) * spacing; // -1, 0, 1

            x = leader->GetPositionX() - std::cos(baseAngle) * distance + std::cos(baseAngle + M_PI/2) * offset;
            y = leader->GetPositionY() - std::sin(baseAngle) * distance + std::sin(baseAngle + M_PI/2) * offset;
            z = leader->GetPositionZ();
            break;
        }

        case FormationType::COMBAT_SPREAD:
        {
            // Spread around target if in combat
            Unit* target = GetGroupTarget(group);
            if (target)
            {
                float angle = (2 * M_PI * index) / std::max(1u, group->GetMembersCount() - 1);
                float distance = GetFollowDistance(bot);

                x = target->GetPositionX() + std::cos(angle) * distance;
                y = target->GetPositionY() + std::sin(angle) * distance;
                z = target->GetPositionZ();
            }
            else
            {
                // Fall back to spread formation
                return GetFormationPosition(bot, group, index, x, y, z);
            }
            break;
        }

        case FormationType::DEFENSIVE:
        {
            // Circle around leader
            float angle = (2 * M_PI * index) / std::max(1u, group->GetMembersCount() - 1);
            float distance = spacing * 2;

            x = leader->GetPositionX() + std::cos(angle) * distance;
            y = leader->GetPositionY() + std::sin(angle) * distance;
            z = leader->GetPositionZ();
            break;
        }
    }

    // Adjust Z for terrain
    bot->UpdateGroundPositionZ(x, y, z);

    // Validate position is safe
    if (!IsPositionSafe(bot, x, y, z))
    {
        // Fall back to simple follow
        return CalculateFollowPosition(bot, leader, x, y, z);
    }

    return true;
}

bool LeaderFollowBehavior::IsGroupInCombat(Group* group) const
{
    if (!group)
        return false;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
        {
            if (member->IsInCombat())
                return true;
        }
    }

    return false;
}

Unit* LeaderFollowBehavior::GetGroupTarget(Group* group) const
{
    if (!group || !_combatTrigger)
        return nullptr;

    return _combatTrigger->GetGroupTarget(group);
}

bool LeaderFollowBehavior::ShouldEngageCombat(Player* bot) const
{
    if (!bot || !_combatAssistEnabled)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Check if group is in combat
    if (!IsGroupInCombat(group))
        return false;

    // Check if we have a valid target
    Unit* target = GetGroupTarget(group);
    if (!target || !target->IsAlive())
        return false;

    // Check if target is in range
    float distance = bot->GetDistance(target);
    if (distance > _maxFollowDistance)
        return false;

    return true;
}

bool LeaderFollowBehavior::EngageCombat(BotAI* ai, Unit* target)
{
    if (!ai || !target || !_assistAction)
        return false;

    // Set combat formation
    _inCombatFormation = true;
    SetFormationType(FormationType::COMBAT_SPREAD);

    // Execute assist action
    ActionContext context;
    context.target = target;

    ActionResult result = _assistAction->Execute(ai, context);

    if (result == ActionResult::SUCCESS)
    {
        _stats.combatEngagements++;
        _stats.targetAssists++;
        return true;
    }

    return false;
}

void LeaderFollowBehavior::UpdateMovement(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot() || !_isFollowing)
        return;

    // Check update interval
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastMovementUpdate);

    if (elapsed.count() < _updateIntervalMs)
        return;

    _lastMovementUpdate = now;

    Player* bot = ai->GetBot();
    Player* leader = GetLeader(bot);

    if (!leader || !ShouldFollowLeader(bot, leader))
    {
        StopFollowing(ai);
        return;
    }

    // Check if in combat
    if (_combatAssistEnabled && ShouldEngageCombat(bot))
    {
        Unit* target = GetGroupTarget(bot->GetGroup());
        if (target)
        {
            EngageCombat(ai, target);
            return;
        }
    }

    // Calculate follow position
    float x, y, z;
    if (!CalculateFollowPosition(bot, leader, x, y, z))
        return;

    // Check if movement needed
    if (NeedsMovement(bot, x, y, z))
    {
        ai->MoveTo(x, y, z);
        _stats.formationAdjustments++;
    }
}

bool LeaderFollowBehavior::IsInPosition(Player* bot) const
{
    if (!bot || !_isFollowing)
        return true;

    Player* leader = GetLeader(bot);
    if (!leader)
        return true;

    float x, y, z;
    if (!CalculateFollowPosition(bot, leader, x, y, z))
        return true;

    float distance = bot->GetDistance(x, y, z);
    return distance <= POSITION_TOLERANCE;
}

void LeaderFollowBehavior::StopFollowing(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    _isFollowing = false;
    _inCombatFormation = false;

    ai->StopMovement();
}

void LeaderFollowBehavior::UpdateFormation(BotAI* ai)
{
    if (!ai || !ai->GetBot() || !_formationEnabled)
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastFormationCheck);

    if (elapsed.count() < FORMATION_UPDATE_INTERVAL)
        return;

    _lastFormationCheck = now;

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();

    if (!group)
        return;

    // Adjust formation based on combat state
    if (IsGroupInCombat(group))
    {
        if (!_inCombatFormation)
        {
            _inCombatFormation = true;
            SetFormationType(FormationType::COMBAT_SPREAD);
            _stats.formationAdjustments++;
        }
    }
    else
    {
        if (_inCombatFormation)
        {
            _inCombatFormation = false;
            SetFormationType(FormationType::SPREAD);
            _stats.formationAdjustments++;
        }
    }
}

bool LeaderFollowBehavior::NeedsMovement(Player* bot, float targetX, float targetY, float targetZ) const
{
    if (!bot)
        return false;

    float distance = bot->GetDistance(targetX, targetY, targetZ);

    // Need to move if outside tolerance
    if (distance > POSITION_TOLERANCE)
        return true;

    // Also check if too close (for ranged classes)
    if (bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_MAGE ||
        bot->getClass() == CLASS_WARLOCK || bot->getClass() == CLASS_PRIEST)
    {
        if (distance < _minFollowDistance * 0.8f)
            return true;
    }

    return false;
}

float LeaderFollowBehavior::GetRolePositionOffset(Player* bot, float baseAngle, float& distance) const
{
    if (!bot)
        return baseAngle;

    // Adjust angle based on role
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
        case CLASS_DEMON_HUNTER:
            // Tanks in front
            distance = TANK_FOLLOW_DISTANCE;
            return baseAngle;

        case CLASS_ROGUE:
            // Rogues behind target
            distance = DPS_FOLLOW_DISTANCE * 0.7f;
            return baseAngle + M_PI;

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            // Ranged DPS spread out
            distance = DPS_FOLLOW_DISTANCE * 1.5f;
            return baseAngle + (M_PI / 4);

        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_PALADIN:
            // Healers stay back
            distance = HEALER_FOLLOW_DISTANCE;
            return baseAngle - (M_PI / 6);

        default:
            distance = DPS_FOLLOW_DISTANCE;
            return baseAngle;
    }
}

bool LeaderFollowBehavior::IsPositionSafe(Player* bot, float x, float y, float z) const
{
    if (!bot)
        return false;

    // Check if position is in line of sight
    if (!bot->IsWithinLOS(x, y, z))
        return false;

    // Check if position is not in hazard (would need environmental checks)
    // This is simplified for now

    return true;
}

void LeaderFollowBehavior::LogBehaviorEvent(std::string const& event, BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    TC_LOG_DEBUG("playerbot", "LeaderFollowBehavior: {} - Bot: {} ({})",
                 event,
                 bot->GetName(),
                 bot->GetGUID().ToString());
}

void LeaderFollowBehavior::UpdateStatistics(std::string const& eventType, std::chrono::milliseconds responseTime)
{
    if (eventType == "follow")
        _stats.followCommands++;
    else if (eventType == "combat")
        _stats.combatEngagements++;
    else if (eventType == "formation")
        _stats.formationAdjustments++;
    else if (eventType == "assist")
        _stats.targetAssists++;

    // Update average response time
    if (_stats.followCommands > 0)
    {
        auto total = _stats.averageResponseTime.count() * (_stats.followCommands - 1);
        _stats.averageResponseTime = std::chrono::milliseconds((total + responseTime.count()) / _stats.followCommands);
    }

    _stats.lastUpdate = std::chrono::steady_clock::now();
}

} // namespace Playerbot