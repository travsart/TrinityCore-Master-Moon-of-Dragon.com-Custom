/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_UTILITY_CONTEXT_BUILDER_H
#define TRINITYCORE_UTILITY_CONTEXT_BUILDER_H

#include "UtilitySystem.h"
#include "AI/BotAI.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"

namespace Playerbot
{

class Blackboard;

/**
 * @brief Builds utility context from game world state
 * Extracts relevant information from bot, group, and world for decision-making
 */
class TC_GAME_API UtilityContextBuilder
{
public:
    /**
     * @brief Build context from current game state
     * @param ai Bot AI instance
     * @param blackboard Shared blackboard (can be nullptr)
     * @return Populated utility context
     */
    static UtilityContext Build(BotAI* ai, Blackboard* blackboard = nullptr)
    {
        UtilityContext context;
        context.bot = ai;
        context.blackboard = blackboard;

        if (!ai)
            return context;

        Player* bot = ai->GetBot();
        if (!bot)
            return context;

        // Bot state
        context.healthPercent = bot->GetHealthPct() / 100.0f;

        // Mana percentage (handle power types correctly)
        if (bot->GetMaxPower(POWER_MANA) > 0)
            context.manaPercent = bot->GetPower(POWER_MANA) / static_cast<float>(bot->GetMaxPower(POWER_MANA));
        else
            context.manaPercent = 1.0f; // Non-mana classes always at "full" mana

        context.inCombat = bot->IsInCombat();
        context.hasAggro = HasAggro(bot);

        // Group state
        Group* group = bot->GetGroup();
        context.inGroup = (group != nullptr);
        context.groupSize = group ? group->GetMembersCount() : 1;
        context.lowestAllyHealthPercent = GetLowestAllyHealth(bot, group);
        context.enemiesInRange = CountEnemiesInRange(bot, 40.0f);

        // Role detection
        context.role = DetectRole(bot);

        // Timing
        context.timeSinceCombatStart = GetTimeSinceCombatStart(ai);
        context.lastDecisionTime = GameTime::GetGameTimeMS();

        return context;
    }

private:
    /**
     * @brief Check if bot currently has aggro
     */
    static bool HasAggro(Player* bot)
    {
        if (!bot || !bot->IsInCombat())
            return false;

        // Check if bot has any attackers
        Unit* victim = bot->GetVictim();
        if (victim && victim->GetVictim() == bot)
            return true;

        // Check threat table if available
        // TODO: Integrate with ThreatManager when available
        return false;
    }

    /**
     * @brief Get lowest health percentage among group members
     */
    static float GetLowestAllyHealth(Player* bot, Group* group)
    {
        if (!group)
            return bot ? (bot->GetHealthPct() / 100.0f) : 1.0f;

        float lowest = 1.0f;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || member->isDead())
                continue;

            float healthPct = member->GetHealthPct() / 100.0f;
            if (healthPct < lowest)
                lowest = healthPct;
        }

        return lowest;
    }

    /**
     * @brief Count enemies within specified range
     */
    static uint32 CountEnemiesInRange(Player* bot, float range)
    {
        if (!bot)
            return 0;

        uint32 count = 0;

        // Use Trinity's SearcherInRange to find nearby hostile units
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, range);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
        Cell::VisitAllObjects(bot, searcher, range);

        return static_cast<uint32>(targets.size());
    }

    /**
     * @brief Detect bot's role based on class, spec, and talents
     */
    static UtilityContext::Role DetectRole(Player* bot)
    {
        if (!bot)
            return UtilityContext::Role::DPS;

        uint8 classId = bot->getClass();
        uint8 spec = bot->GetPrimaryTalentTree(bot->GetActiveSpec());

        // Tank specs
        if (classId == CLASS_WARRIOR && spec == 2) // Protection
            return UtilityContext::Role::TANK;
        if (classId == CLASS_PALADIN && spec == 1) // Protection
            return UtilityContext::Role::TANK;
        if (classId == CLASS_DEATH_KNIGHT && spec == 0) // Blood
            return UtilityContext::Role::TANK;
        if (classId == CLASS_DRUID && spec == 1) // Feral (Bear form)
            return UtilityContext::Role::TANK;

        // Healer specs
        if (classId == CLASS_PRIEST && (spec == 1 || spec == 2)) // Discipline or Holy
            return UtilityContext::Role::HEALER;
        if (classId == CLASS_PALADIN && spec == 0) // Holy
            return UtilityContext::Role::HEALER;
        if (classId == CLASS_SHAMAN && spec == 2) // Restoration
            return UtilityContext::Role::HEALER;
        if (classId == CLASS_DRUID && spec == 2) // Restoration
            return UtilityContext::Role::HEALER;

        // Support (some specs can be hybrid)
        if (classId == CLASS_SHAMAN && spec == 1) // Enhancement (can off-heal)
            return UtilityContext::Role::SUPPORT;
        if (classId == CLASS_DRUID && spec == 1) // Feral (can off-tank/DPS)
            return UtilityContext::Role::SUPPORT;

        // Default: DPS
        return UtilityContext::Role::DPS;
    }

    /**
     * @brief Get time since combat started (in milliseconds)
     */
    static uint32 GetTimeSinceCombatStart(BotAI* ai)
    {
        if (!ai)
            return 0;

        Player* bot = ai->GetBot();
        if (!bot || !bot->IsInCombat())
            return 0;

        // TODO: Integrate with CombatStateManager to track combat start time
        // For now, return 0 as placeholder
        return 0;
    }
};

} // namespace Playerbot

#endif // TRINITYCORE_UTILITY_CONTEXT_BUILDER_H
