/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "InterruptManager.h"
#include "Player.h"
#include "Unit.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "Log.h"
#include "Creature.h"
#include "Map.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include "SharedDefines.h"

namespace Playerbot
{

InterruptManager::InterruptManager(Player* bot) : _bot(bot)
{
    // Initialize high priority spells that should be interrupted
    _highPrioritySpells.insert(2139);   // Counterspell
    _highPrioritySpells.insert(118);    // Polymorph
    _highPrioritySpells.insert(5782);   // Fear
    _highPrioritySpells.insert(8122);   // Psychic Scream
    _highPrioritySpells.insert(5484);   // Howl of Terror
    _highPrioritySpells.insert(6770);   // Sap
    _highPrioritySpells.insert(2094);   // Blind
    _highPrioritySpells.insert(20066);  // Repentance
    _highPrioritySpells.insert(9484);   // Shackle Undead
    _highPrioritySpells.insert(339);    // Entangling Roots
    _highPrioritySpells.insert(2637);   // Hibernate
    _highPrioritySpells.insert(1499);   // Freezing Trap
    _highPrioritySpells.insert(19386);  // Wyvern Sting
}

::Unit* InterruptManager::GetHighestPriorityTarget()
{
    if (!_bot)
        return nullptr;

    ::Unit* highestPriorityTarget = nullptr;
    float highestPriority = 0.0f;

    // Check all units in range using TrinityCore API
    std::list<::Unit*> targets;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, 40.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, targets, check);
    Cell::VisitAllObjects(_bot, searcher, 40.0f);

    for (::Unit* target : targets)
    {
        if (!target || !target->IsNonMeleeSpellCast(false))
            continue;

        float priority = CalculateInterruptPriority(target);
        if (priority > highestPriority)
        {
            highestPriority = priority;
            highestPriorityTarget = target;
        }
    }

    return highestPriorityTarget;
}

bool InterruptManager::ShouldInterrupt(::Unit* target)
{
    if (!target || !_bot)
        return false;

    // Check if target is casting
    if (!target->IsNonMeleeSpellCast(false))
        return false;

    // Check if we recently tried to interrupt this target
    auto it = _lastInterruptAttempt.find(target->GetGUID());
    if (it != _lastInterruptAttempt.end())
    {
        uint32 timeSinceLastAttempt = getMSTime() - it->second;
        if (timeSinceLastAttempt < 3000) // 3 second cooldown
            return false;
    }

    // Check if the spell being cast is worth interrupting
    if (target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        uint32 spellId = target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id;
        return IsHighPrioritySpell(spellId) || CalculateInterruptPriority(target) > 50.0f;
    }

    return false;
}

void InterruptManager::RecordInterruptAttempt(::Unit* target, bool success)
{
    if (!target)
        return;

    _lastInterruptAttempt[target->GetGUID()] = getMSTime();

    // Log interrupt attempt for analysis
    if (success)
    {
        TC_LOG_DEBUG("playerbot.interrupt", "Successfully interrupted {} casting spell",
                     target->GetName());
    }
    else
    {
        TC_LOG_DEBUG("playerbot.interrupt", "Failed to interrupt {} - target may be immune",
                     target->GetName());
    }
}

float InterruptManager::CalculateInterruptPriority(::Unit* target)
{
    if (!target || !_bot)
        return 0.0f;

    float priority = 0.0f;

    // Check if target is casting
    if (!target->IsNonMeleeSpellCast(false))
        return 0.0f;

    // Base priority on target type
    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        priority += 75.0f; // Players are high priority
    }
    else if (target->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creature = target->ToCreature();
        if (creature)
        {
            if (creature->GetCreatureClassification() == CreatureClassifications::Elite ||
                creature->GetCreatureClassification() == CreatureClassifications::RareElite)
                priority += 60.0f; // Elite creatures
            else
                priority += 30.0f; // Normal creatures
        }
    }

    // Check spell being cast
    if (target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        uint32 spellId = target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id;

        if (IsHighPrioritySpell(spellId))
            priority += 50.0f;

        // Check spell school for additional priority
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, target->GetMap()->GetDifficultyID());
        if (spellInfo)
        {
            // Prioritize healing spells
            if (spellInfo->HasEffect(SPELL_EFFECT_HEAL) ||
                spellInfo->HasEffect(SPELL_EFFECT_HEAL_PCT))
            {
                priority += 40.0f;
            }

            // Prioritize crowd control
            if (spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA))
            {
                priority += 30.0f;
            }

            // Long cast time spells are higher priority
            if (spellInfo->CalcCastTime() > 3000)
                priority += 20.0f;
        }
    }

    // Distance factor (closer targets are higher priority)
    float distance = _bot->GetDistance(target);
    if (distance < 10.0f)
        priority += 15.0f;
    else if (distance < 20.0f)
        priority += 10.0f;
    else if (distance < 30.0f)
        priority += 5.0f;

    // Health factor (low health targets are lower priority unless they're healing)
    float healthPct = target->GetHealthPct();
    if (healthPct < 20.0f)
        priority -= 10.0f;

    return priority;
}

bool InterruptManager::IsHighPrioritySpell(uint32 spellId)
{
    return _highPrioritySpells.find(spellId) != _highPrioritySpells.end();
}

} // namespace Playerbot