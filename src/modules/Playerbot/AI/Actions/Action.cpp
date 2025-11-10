/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Combat/ThreatManager.h removed - not used in this file
#include "Action.h"
#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "Object.h"
#include "Item.h"
#include "Group.h"
#include "MotionMaster.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "SpellHistory.h"
#include "Log.h"
#include "Chat.h"
#include "GridNotifiers.h"
#include "../../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "CellImpl.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

// Action implementation
Action::Action(std::string const& name) : _name(name)
{
    _lastExecution = std::chrono::steady_clock::now();
}

bool Action::IsOnCooldown() const
{
    if (GetCooldown() <= 0.0f)
        return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastExecution);
    return elapsed.count() < GetCooldown();
}

void Action::AddPrerequisite(std::shared_ptr<Action> action)
{
    if (action)
        _prerequisites.push_back(action);
}

float Action::GetSuccessRate() const
{
    uint32 executions = _executionCount.load();
    if (executions == 0)
        return 0.0f;

    uint32 successes = _successCount.load();
    return static_cast<float>(successes) / static_cast<float>(executions);
}

// Helper methods
bool Action::CanCast(BotAI* ai, uint32 spellId, ::Unit* target) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method HasSpell");
            return nullptr;
        }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPower");
    return nullptr;
}
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method HasSpell");
        return nullptr;
    }
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check if bot can cast this spell
    if (!bot->HasSpell(spellId))
        return false;

    // Check mana/energy requirements
    std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPower");
            return nullptr;
        }
    for (SpellPowerCost const& cost : costs)
    {
        if (bot->GetPower(cost.Power) < cost.Amount)
            return false;
    }

    // Check cooldown using TrinityCore's SpellHistory API
    if (bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check if target is valid (if spell requires target)
    if (spellInfo->IsTargetingArea() && !target)
        return false;

    // Check range if target specified
    if (target)
    {
        float range = spellInfo->GetMaxRange();
        float rangeSq = range * range;
        if (bot->GetExactDistSq(target) > rangeSq)
            return false;
    }

    return true;
}

bool Action::DoCast(BotAI* ai, uint32 spellId, ::Unit* target)
{
    if (!CanCast(ai, spellId, target))
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method CastSpell");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method CastSpell");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method CastSpell");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method CastSpell");
            return;
        }
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Cast the spell
    if (target)
        bot->CastSpell(target, spellId, false);
    else
        bot->CastSpell(bot, spellId, false);

    return true;
}

bool Action::DoMove(BotAI* ai, float x, float y, float z)
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    bot->GetMotionMaster()->MovePoint(0, x, y, z);
    return true;
}

bool Action::DoSay(BotAI* ai, std::string const& text)
{
    if (!ai || text.empty())
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    bot->Say(text, LANG_UNIVERSAL);
    return true;
}

bool Action::DoEmote(BotAI* ai, uint32 emoteId)
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    bot->HandleEmoteCommand(static_cast<Emote>(emoteId));
    return true;
}

bool Action::UseItem(BotAI* ai, uint32 itemId, ::Unit* target)
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    Item* item = bot->GetItemByEntry(itemId);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMap");
        return;
    }
    if (!item)
        return false;

    // Use the item with proper SpellCastTargets
    SpellCastTargets targets;
    if (target)
        targets.SetUnitTarget(target);
    else
        targets.SetUnitTarget(bot);

    bot->CastItemUseSpell(item, targets, ObjectGuid::Empty, nullptr);
    return true;
}

::Unit* Action::GetNearestEnemy(BotAI* ai, float range) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPosition");
    return;
}
    if (!bot)
        return nullptr;

    ::Unit* nearestEnemy = nullptr;
    float nearestDistanceSq = range * range; // Use squared distance for comparison

    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = bot->GetMap();
    if (!unit)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMap");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPosition");
            return nullptr;
        }
    if (!map)
        return nullptr;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return nullptr;
    }

    // Query nearby creature GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        bot->GetPosition(), range);

    // Resolve GUIDs to Unit pointers and find nearest enemy
    for (ObjectGuid guid : nearbyGuids)
    {
        /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ ::Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        if (!unit)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
            return;
        }
        if (!unit || !unit->IsAlive())
            continue;
if (!member)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsAlive");
    return;
}

        // Check if hostile
        if (!bot->IsHostileTo(unit))
            continue;

        float distanceSq = bot->GetExactDistSq(unit);
        if (distanceSq < nearestDistanceSq)
        {
            nearestDistanceSq = distanceSq;
            nearestEnemy = unit;
        }
    }

    return nearestEnemy;
}

::Unit* Action::GetLowestHealthAlly(BotAI* ai, float range) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    ::Unit* lowestHealthAlly = nullptr;
    float lowestHealthPct = 100.0f;

    // Check group members first
    if (Group* group = bot->GetGroup())
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMap");
            return nullptr;
        }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (!player)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method IsInWorld");
                    return nullptr;
                }
                if (member == bot || !member->IsAlive())
                if (!member)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsAlive");
                    return nullptr;
                }
                    continue;

                float rangeSq = range * range;
                if (bot->GetExactDistSq(member) > rangeSq)
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

    return lowestHealthAlly;
}

Player* Action::GetNearestPlayer(BotAI* ai, float range) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    Player* nearestPlayer = nullptr;
    float nearestDistanceSq = range * range; // Use squared distance for comparison

    // Use Map API to find nearby players
    Map* map = bot->GetMap();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMap");
        return;
    }
    if (map)
    {
        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator iter = players.begin(); iter != players.end(); ++iter)
        {
            Player* player = iter->GetSource();
            if (!player || player == bot || !player->IsInWorld())
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPositionZ");
                    return nullptr;
                }
            if (!player)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method IsInWorld");
                return;
            }
                continue;

            float distanceSq = bot->GetExactDistSq(player);
            float rangeSq = range * range;
            if (distanceSq <= rangeSq && distanceSq < nearestDistanceSq)
            {
                nearestDistanceSq = distanceSq;
                nearestPlayer = player;
            }
        }
    }

    return nearestPlayer;
}

// MovementAction implementation
bool MovementAction::IsPossible(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    // Can't move if rooted or stunned
    if (bot->HasUnitState(UNIT_STATE_ROOT) || bot->HasUnitState(UNIT_STATE_STUNNED))
        return false;

    return true;
}

ActionResult MovementAction::Execute(BotAI* ai, ActionContext const& context)
{
    if (!IsPossible(ai))
        return ActionResult::IMPOSSIBLE;

    // Default movement implementation - derived classes should override
    return ActionResult::SUCCESS;
}

bool MovementAction::GeneratePath(BotAI* ai, float x, float y, float z)
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPositionZ");
        return;
    }
    if (!bot)
        return false;

    // Basic pathfinding - just move directly for now
    // In a full implementation, this would use proper pathfinding
    _path.clear();
    _path.push_back(G3D::Vector3(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()));
    _path.push_back(G3D::Vector3(x, y, z));

    return true;
}

// CombatAction implementation
bool CombatAction::IsUseful(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    // Combat actions are useful when in combat or when there are enemies nearby
    return bot->IsInCombat() || GetNearestEnemy(ai, 30.0f) != nullptr;
}

// SpellAction implementation
bool SpellAction::IsPossible(BotAI* ai) const
{
    return CanCast(ai, _spellId);
}

bool SpellAction::IsUseful(BotAI* ai) const
{
    // Check base combat utility
    if (!CombatAction::IsUseful(ai))
        return false;

    // Additional spell-specific checks could go here
    return true;
}

ActionResult SpellAction::Execute(BotAI* ai, ActionContext const& context)
{
    if (!IsPossible(ai))
        return ActionResult::IMPOSSIBLE;

    ::Unit* target = context.target ? context.target->ToUnit() : nullptr;

    if (DoCast(ai, _spellId, target))
    {
        _executionCount++;
        _successCount++;
        _lastExecution = std::chrono::steady_clock::now();
        return ActionResult::SUCCESS;
    }

    _executionCount++;
    return ActionResult::FAILED;
}

// ActionFactory implementation
ActionFactory* ActionFactory::instance()
{
    static ActionFactory instance;
    return &instance;
}

void ActionFactory::RegisterAction(std::string const& name,
                                 std::function<std::shared_ptr<Action>()> creator)
{
    _creators[name] = creator;
}

std::shared_ptr<Action> ActionFactory::CreateAction(std::string const& name)
{
    auto it = _creators.find(name);
    if (it != _creators.end())
        return it->second();

    return nullptr;
}

std::vector<std::shared_ptr<Action>> ActionFactory::CreateClassActions(uint8 classId, uint8 spec)
{
    std::vector<std::shared_ptr<Action>> actions;

    // This will be expanded with class-specific action creation
    // For now, return empty vector

    return actions;
}

std::vector<std::shared_ptr<Action>> ActionFactory::CreateCombatActions(uint8 classId)
{
    std::vector<std::shared_ptr<Action>> actions;

    // This will be expanded with combat action creation
    // For now, return empty vector

    return actions;
}

std::vector<std::shared_ptr<Action>> ActionFactory::CreateMovementActions()
{
    std::vector<std::shared_ptr<Action>> actions;

    // This will be expanded with movement action creation
    // For now, return empty vector

    return actions;
}

std::vector<std::string> ActionFactory::GetAvailableActions() const
{
    std::vector<std::string> actions;
    actions.reserve(_creators.size());

    for (auto const& pair : _creators)
        actions.push_back(pair.first);

    return actions;
}

bool ActionFactory::HasAction(std::string const& name) const
{
    return _creators.find(name) != _creators.end();
}

} // namespace Playerbot
