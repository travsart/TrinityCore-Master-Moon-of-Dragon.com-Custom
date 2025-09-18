/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarriorSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "Pet.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Log.h"
#include "PlayerbotAI.h"

namespace Playerbot
{

WarriorSpecialization::WarriorSpecialization(Player* bot) : _bot(bot)
{
}

// Resource management
bool WarriorSpecialization::HasEnoughRage(uint32 amount)
{
    if (!_bot)
        return false;

    uint32 currentRage = GetRage();
    return currentRage >= amount;
}

uint32 WarriorSpecialization::GetRage()
{
    if (!_bot)
        return 0;

    return _bot->GetPower(POWER_RAGE) / 10; // Rage is stored in 10ths
}

uint32 WarriorSpecialization::GetMaxRage()
{
    if (!_bot)
        return 0;

    return _bot->GetMaxPower(POWER_RAGE) / 10; // Rage is stored in 10ths
}

float WarriorSpecialization::GetRagePercent()
{
    uint32 maxRage = GetMaxRage();
    if (maxRage == 0)
        return 0.0f;

    return static_cast<float>(GetRage()) / static_cast<float>(maxRage) * 100.0f;
}

bool WarriorSpecialization::ShouldConserveRage()
{
    float ragePercent = GetRagePercent();
    return ragePercent < RAGE_CONSERVATION_THRESHOLD;
}

// Shared warrior abilities
void WarriorSpecialization::CastCharge(Unit* target)
{
    if (!target || !_bot)
        return;

    float distance = _bot->GetDistance2d(target);
    if (distance < MINIMUM_SAFE_RANGE || distance > CHARGE_RANGE)
        return;

    if (!_bot->IsWithinLOSInMap(target))
        return;

    if (_bot->HasSpellCooldown(CHARGE))
        return;

    if (_bot->isMoving())
        _bot->StopMoving();

    if (_bot->CastSpell(target, CHARGE, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} charged at target {}",
                    _bot->GetName(), target->GetName());
    }
}

void WarriorSpecialization::CastIntercept(Unit* target)
{
    if (!target || !_bot)
        return;

    if (!IsInStance(WarriorStance::BERSERKER))
        return;

    float distance = _bot->GetDistance2d(target);
    if (distance < MINIMUM_SAFE_RANGE || distance > CHARGE_RANGE)
        return;

    if (!_bot->IsWithinLOSInMap(target))
        return;

    if (_bot->HasSpellCooldown(INTERCEPT))
        return;

    if (!HasEnoughRage(10))
        return;

    if (_bot->CastSpell(target, INTERCEPT, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} intercepted target {}",
                    _bot->GetName(), target->GetName());
    }
}

void WarriorSpecialization::CastHeroicLeap(Unit* target)
{
    if (!target || !_bot)
        return;

    float distance = _bot->GetDistance2d(target);
    if (distance < MINIMUM_SAFE_RANGE || distance > CHARGE_RANGE)
        return;

    if (_bot->HasSpellCooldown(HEROIC_LEAP))
        return;

    Position pos = target->GetPosition();
    if (_bot->CastSpell(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), HEROIC_LEAP, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} used heroic leap towards target {}",
                    _bot->GetName(), target->GetName());
    }
}

void WarriorSpecialization::CastThunderClap()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(THUNDER_CLAP))
        return;

    if (!HasEnoughRage(20))
        return;

    // Check for nearby enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 10.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 10.0f);

    if (targets.size() >= 2) // Only use if multiple targets
    {
        if (_bot->CastSpell(_bot, THUNDER_CLAP, false))
        {
            TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} used thunder clap on {} targets",
                        _bot->GetName(), targets.size());
        }
    }
}

void WarriorSpecialization::CastShout()
{
    if (!_bot)
        return;

    // Check if we need battle shout
    if (!_bot->HasAura(BATTLE_SHOUT) && !_bot->HasSpellCooldown(BATTLE_SHOUT))
    {
        if (_bot->CastSpell(_bot, BATTLE_SHOUT, false))
        {
            TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} cast battle shout",
                        _bot->GetName());
            return;
        }
    }

    // Check if we need commanding shout for health
    if (!_bot->HasAura(COMMANDING_SHOUT) && _bot->GetHealthPct() < 80.0f &&
        !_bot->HasSpellCooldown(COMMANDING_SHOUT))
    {
        if (_bot->CastSpell(_bot, COMMANDING_SHOUT, false))
        {
            TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} cast commanding shout",
                        _bot->GetName());
        }
    }
}

void WarriorSpecialization::CastRend(Unit* target)
{
    if (!target || !_bot)
        return;

    if (target->HasAura(REND))
        return;

    if (!HasEnoughRage(10))
        return;

    if (!IsInMeleeRange(target))
        return;

    if (_bot->CastSpell(target, REND, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} applied rend to target {}",
                    _bot->GetName(), target->GetName());
    }
}

// Stance management
void WarriorSpecialization::EnterBattleStance()
{
    if (!_bot || IsInStance(WarriorStance::BATTLE))
        return;

    if (_bot->CastSpell(_bot, BATTLE_STANCE, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} entered battle stance",
                    _bot->GetName());
    }
}

void WarriorSpecialization::EnterDefensiveStance()
{
    if (!_bot || IsInStance(WarriorStance::DEFENSIVE))
        return;

    if (_bot->CastSpell(_bot, DEFENSIVE_STANCE, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} entered defensive stance",
                    _bot->GetName());
    }
}

void WarriorSpecialization::EnterBerserkerStance()
{
    if (!_bot || IsInStance(WarriorStance::BERSERKER))
        return;

    if (_bot->CastSpell(_bot, BERSERKER_STANCE, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} entered berserker stance",
                    _bot->GetName());
    }
}

WarriorStance WarriorSpecialization::GetCurrentStance()
{
    if (!_bot)
        return WarriorStance::NONE;

    if (_bot->HasAura(BATTLE_STANCE))
        return WarriorStance::BATTLE;
    else if (_bot->HasAura(DEFENSIVE_STANCE))
        return WarriorStance::DEFENSIVE;
    else if (_bot->HasAura(BERSERKER_STANCE))
        return WarriorStance::BERSERKER;

    return WarriorStance::NONE;
}

bool WarriorSpecialization::IsInStance(WarriorStance stance)
{
    return GetCurrentStance() == stance;
}

void WarriorSpecialization::SwitchStance(WarriorStance stance)
{
    if (IsInStance(stance))
        return;

    switch (stance)
    {
        case WarriorStance::BATTLE:
            EnterBattleStance();
            break;
        case WarriorStance::DEFENSIVE:
            EnterDefensiveStance();
            break;
        case WarriorStance::BERSERKER:
            EnterBerserkerStance();
            break;
        default:
            break;
    }
}

// Shared defensive abilities
void WarriorSpecialization::UseShieldWall()
{
    if (!_bot)
        return;

    if (_bot->GetHealthPct() > 30.0f)
        return;

    if (_bot->HasSpellCooldown(SHIELD_WALL))
        return;

    if (!IsInStance(WarriorStance::DEFENSIVE))
        return;

    if (_bot->CastSpell(_bot, SHIELD_WALL, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} used shield wall",
                    _bot->GetName());
    }
}

void WarriorSpecialization::UseLastStand()
{
    if (!_bot)
        return;

    if (_bot->GetHealthPct() > 20.0f)
        return;

    if (_bot->HasSpellCooldown(LAST_STAND))
        return;

    if (_bot->CastSpell(_bot, LAST_STAND, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} used last stand",
                    _bot->GetName());
    }
}

void WarriorSpecialization::UseEnragedRegeneration()
{
    if (!_bot)
        return;

    if (_bot->GetHealthPct() > 40.0f)
        return;

    if (_bot->HasSpellCooldown(ENRAGED_REGENERATION))
        return;

    if (!HasEnoughRage(15))
        return;

    if (_bot->CastSpell(_bot, ENRAGED_REGENERATION, false))
    {
        TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} used enraged regeneration",
                    _bot->GetName());
    }
}

void WarriorSpecialization::UseSpellReflection()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(SPELL_REFLECTION))
        return;

    // Check if nearby enemies are casting spells
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 20.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 20.0f);

    bool shouldReflect = false;
    for (Unit* target : targets)
    {
        if (target->HasUnitState(UNIT_STATE_CASTING))
        {
            shouldReflect = true;
            break;
        }
    }

    if (shouldReflect)
    {
        if (_bot->CastSpell(_bot, SPELL_REFLECTION, false))
        {
            TC_LOG_DEBUG("playerbots", "WarriorSpecialization: Bot {} used spell reflection",
                        _bot->GetName());
        }
    }
}

// Shared utility
bool WarriorSpecialization::IsChanneling()
{
    if (!_bot)
        return false;

    return _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool WarriorSpecialization::IsCasting()
{
    if (!_bot)
        return false;

    return _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool WarriorSpecialization::CanUseAbility()
{
    if (!_bot)
        return false;

    return !_bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING);
}

bool WarriorSpecialization::IsInDanger()
{
    if (!_bot)
        return false;

    // Check health threshold
    if (_bot->GetHealthPct() < 30.0f)
        return true;

    // Check for multiple attackers
    std::list<Unit*> attackers;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 15.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, attackers, u_check);
    Cell::VisitAllObjects(_bot, searcher, 15.0f);

    if (attackers.size() >= 3)
        return true;

    // Check for strong enemies
    for (Unit* attacker : attackers)
    {
        if (attacker->GetLevel() > _bot->GetLevel() + 2)
            return true;
    }

    return false;
}

bool WarriorSpecialization::IsInMeleeRange(Unit* target)
{
    if (!target || !_bot)
        return false;

    float distance = _bot->GetDistance2d(target);
    return distance <= OPTIMAL_MELEE_RANGE;
}

} // namespace Playerbot