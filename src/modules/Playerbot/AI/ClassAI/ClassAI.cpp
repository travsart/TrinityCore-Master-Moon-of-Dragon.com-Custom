/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ClassAI.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Object.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include <chrono>

namespace Playerbot
{

ClassAI::ClassAI(Player* bot) : BotAI(bot), _currentTarget(nullptr), _inCombat(false),
    _combatTime(0), _lastTargetSwitch(0), _lastUpdate(0)
{
    // Initialize component managers
    _actionQueue = std::make_unique<ActionPriorityQueue>();
    _cooldownManager = std::make_unique<CooldownManager>();
    _resourceManager = std::make_unique<ResourceManager>(bot);
}

void ClassAI::UpdateAI(uint32 diff)
{
    // Performance optimization - limit update frequency
    _lastUpdate += diff;
    if (_lastUpdate < UPDATE_INTERVAL_MS)
        return;

    uint32 actualDiff = _lastUpdate;
    _lastUpdate = 0;

    // Update component managers
    _cooldownManager->Update(actualDiff);
    UpdateCombatState(actualDiff);

    // Update targeting
    UpdateTargeting();

    // Class-specific updates
    UpdateCooldowns(actualDiff);

    if (_currentTarget)
    {
        UpdateRotation(_currentTarget);
        UpdateMovement();
    }
    else
    {
        UpdateBuffs();
    }

    // Call base class update
    BotAI::UpdateAI(actualDiff);
}

void ClassAI::OnCombatStart(::Unit* target)
{
    _inCombat = true;
    _combatTime = 0;
    _currentTarget = target;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} entering combat with {}",
                 GetBot()->GetName(), target ? target->GetName() : "unknown");
}

void ClassAI::OnCombatEnd()
{
    _inCombat = false;
    _combatTime = 0;
    _currentTarget = nullptr;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} leaving combat", GetBot()->GetName());
}

void ClassAI::OnTargetChanged(::Unit* newTarget)
{
    _currentTarget = newTarget;
    _lastTargetSwitch = _combatTime;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} switching target to {}",
                 GetBot()->GetName(), newTarget ? newTarget->GetName() : "none");
}

bool ClassAI::IsSpellReady(uint32 spellId)
{
    if (!spellId)
        return false;

    return _cooldownManager->IsReady(spellId) && _cooldownManager->IsGCDReady();
}

bool ClassAI::IsInRange(::Unit* target, uint32 spellId)
{
    if (!target || !spellId)
        return false;

    float range = GetSpellRange(spellId);
    if (range <= 0.0f)
        return true; // No range restriction

    return GetBot()->GetDistance(target) <= range;
}

bool ClassAI::HasLineOfSight(::Unit* target)
{
    if (!target)
        return false;

    return GetBot()->IsWithinLOSInMap(target);
}

bool ClassAI::IsSpellUsable(uint32 spellId)
{
    if (!spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    return GetBot()->CanUseSpell(spellInfo) == SPELL_CAST_OK;
}

float ClassAI::GetSpellRange(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0.0f;

    return spellInfo->GetMaxRange();
}

uint32 ClassAI::GetSpellCooldown(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    return spellInfo->RecoveryTime;
}

bool ClassAI::CastSpell(::Unit* target, uint32 spellId)
{
    if (!target || !spellId)
        return false;

    if (!IsSpellReady(spellId) || !IsSpellUsable(spellId))
        return false;

    if (!IsInRange(target, spellId) || !HasLineOfSight(target))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check resource requirements
    if (!HasEnoughResource(spellId))
        return false;

    // Cast the spell
    SpellCastTargets targets;
    targets.SetUnitTarget(target);

    Spell* spell = new Spell(GetBot(), spellInfo, TRIGGERED_NONE);
    SpellCastResult result = spell->prepare(targets);

    if (result == SPELL_CAST_OK)
    {
        // Start cooldowns
        _cooldownManager->StartCooldown(spellId, GetSpellCooldown(spellId));
        _cooldownManager->TriggerGCD();

        // Consume resources
        ConsumeResource(spellId);

        TC_LOG_DEBUG("playerbot.classai", "Bot {} cast spell {} on {}",
                     GetBot()->GetName(), spellId, target->GetName());
        return true;
    }
    else
    {
        delete spell;
        return false;
    }
}

bool ClassAI::CastSpell(uint32 spellId)
{
    return CastSpell(GetBot(), spellId);
}

::Unit* ClassAI::GetBestAttackTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    // Priority: Current target if valid
    if (::Unit* currentTarget = bot->GetSelectedUnit())
    {
        if (currentTarget->IsAlive() && bot->IsValidAttackTarget(currentTarget))
            return currentTarget;
    }

    // Find nearest hostile target
    return GetNearestEnemy(30.0f);
}

::Unit* ClassAI::GetBestHealTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    // Priority: Self if critically low
    if (bot->GetHealthPct() < 30.0f)
        return bot;

    // Find ally with lowest health
    ::Unit* ally = GetLowestHealthAlly(40.0f);
    if (ally && ally->GetHealthPct() < 80.0f)
        return ally;

    // Heal self if below threshold
    if (bot->GetHealthPct() < 80.0f)
        return bot;

    return nullptr;
}

::Unit* ClassAI::GetNearestEnemy(float maxRange)
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    ::Unit* nearestEnemy = nullptr;
    float nearestDistance = maxRange;

    // Use Map API to find nearby players and creatures
    Map* map = bot->GetMap();
    if (map)
    {
        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator iter = players.begin(); iter != players.end(); ++iter)
        {
            Player* player = iter->GetSource();
            if (!player || player == bot || !player->IsInWorld())
                continue;

            if (bot->IsValidAttackTarget(player))
            {
                float distance = bot->GetDistance(player);
                if (distance <= maxRange && distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearestEnemy = player;
                }
            }
        }
    }

    return nearestEnemy;
}

::Unit* ClassAI::GetLowestHealthAlly(float maxRange)
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    ::Unit* lowestHealthAlly = nullptr;
    float lowestHealthPct = 100.0f;

    // Check group members first
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member == bot || !member->IsAlive())
                    continue;

                if (bot->GetDistance(member) > maxRange)
                    continue;

                float healthPct = member->GetHealthPct();
                if (healthPct < lowestHealthPct)
                {
                    lowestHealthPct = healthPct;
                    lowestHealthAlly = member;
                }
            }
        }
    }

    // If no group or no injured group members, consider self
    if (!lowestHealthAlly && bot->GetHealthPct() < lowestHealthPct)
    {
        lowestHealthAlly = bot;
    }

    return lowestHealthAlly;
}

bool ClassAI::HasAura(uint32 spellId, ::Unit* target)
{
    if (!target)
        target = GetBot();

    return target->HasAura(spellId);
}

uint32 ClassAI::GetAuraStacks(uint32 spellId, ::Unit* target)
{
    if (!target)
        target = GetBot();

    if (Aura* aura = target->GetAura(spellId))
        return aura->GetStackAmount();

    return 0;
}

uint32 ClassAI::GetAuraRemainingTime(uint32 spellId, ::Unit* target)
{
    if (!target)
        target = GetBot();

    if (Aura* aura = target->GetAura(spellId))
        return aura->GetDuration();

    return 0;
}

bool ClassAI::IsMoving()
{
    return GetBot()->IsMoving();
}

bool ClassAI::IsInMeleeRange(::Unit* target)
{
    if (!target)
        return false;

    return GetBot()->GetDistance(target) <= ATTACK_DISTANCE;
}

bool ClassAI::ShouldMoveToTarget(::Unit* target)
{
    if (!target)
        return false;

    float distance = GetBot()->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    // Move if too far or too close
    return (distance > optimalRange + 1.0f) || (distance < optimalRange - 1.0f);
}

void ClassAI::MoveToTarget(::Unit* target, float distance)
{
    if (!target)
        return;

    if (distance == 0.0f)
        distance = GetOptimalRange(target);

    Position pos = GetOptimalPosition(target);
    GetBot()->GetMotionMaster()->MovePoint(0, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
}

void ClassAI::StopMovement()
{
    GetBot()->GetMotionMaster()->Clear();
    GetBot()->StopMoving();
}

void ClassAI::UpdateTargeting()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if current target is still valid
    if (_currentTarget)
    {
        if (!_currentTarget->IsAlive() || !bot->IsValidAttackTarget(_currentTarget))
        {
            OnTargetChanged(nullptr);
        }
    }

    // Find new target if needed
    if (!_currentTarget && bot->IsInCombat())
    {
        ::Unit* newTarget = GetBestAttackTarget();
        if (newTarget)
        {
            OnTargetChanged(newTarget);
            bot->SetTarget(newTarget->GetGUID());
        }
    }
}

void ClassAI::UpdateMovement()
{
    if (!_currentTarget)
        return;

    if (ShouldMoveToTarget(_currentTarget))
    {
        MoveToTarget(_currentTarget);
    }
}

void ClassAI::UpdateCombatState(uint32 diff)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    bool wasInCombat = _inCombat;
    _inCombat = bot->IsInCombat();

    if (_inCombat)
    {
        _combatTime += diff;

        if (!wasInCombat)
        {
            OnCombatStart(_currentTarget);
        }
    }
    else if (wasInCombat)
    {
        OnCombatEnd();
    }
}

void ClassAI::RecordPerformanceMetric(const std::string& metric, uint32 value)
{
    // TODO: Implement performance metrics recording
    // This could integrate with the lifecycle manager for monitoring
}

} // namespace Playerbot