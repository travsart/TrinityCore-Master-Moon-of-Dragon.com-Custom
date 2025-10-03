/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PriestSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "Group.h"
#include "SpellHistory.h"
#include "SharedDefines.h"
#include "Log.h"
#include "SpellAuras.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Map.h"
#include "UnitDefines.h"
#include <algorithm>

namespace Playerbot
{

PriestSpecialization::PriestSpecialization(Player* bot) : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "PriestSpecialization: Bot player is null!");
        return;
    }

    TC_LOG_DEBUG("playerbot.priest", "PriestSpecialization initialized for bot {}", _bot->GetName());
}

// Resource management methods
bool PriestSpecialization::HasEnoughMana(uint32 amount)
{
    return _bot && _bot->GetPower(POWER_MANA) >= static_cast<int32>(amount);
}

uint32 PriestSpecialization::GetMana()
{
    return _bot ? static_cast<uint32>(_bot->GetPower(POWER_MANA)) : 0;
}

uint32 PriestSpecialization::GetMaxMana()
{
    return _bot ? static_cast<uint32>(_bot->GetMaxPower(POWER_MANA)) : 0;
}

float PriestSpecialization::GetManaPercent()
{
    if (!_bot)
        return 0.0f;

    uint32 max = GetMaxMana();
    if (max == 0)
        return 0.0f;

    return static_cast<float>(GetMana()) / static_cast<float>(max) * 100.0f;
}

bool PriestSpecialization::ShouldConserveMana()
{
    return GetManaPercent() < (MANA_CONSERVATION_THRESHOLD * 100.0f);
}

// Healing abilities - Full implementation following FUNDAMENTAL RULES
void PriestSpecialization::CastHeal(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = HEAL;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check if spell is available and bot has it
    if (!_bot->HasSpell(spellId))
        return;

    // Check cooldown using modern TrinityCore API
    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    // Check mana cost using modern power cost system
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    // Check range
    if (_bot->GetDistance(target) > spellInfo->GetMaxRange())
        return;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return;

    // Cast the spell
    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Heal on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastGreaterHeal(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = GREATER_HEAL;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Greater Heal on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastFlashHeal(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = FLASH_HEAL;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Flash Heal on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastRenew(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = RENEW;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    // Don't refresh if target already has Renew
    if (target->HasAura(spellId, _bot->GetGUID()))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Renew on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastPrayerOfHealing()
{
    if (!_bot)
        return;

    uint32 spellId = PRAYER_OF_HEALING;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    _bot->CastSpell(_bot, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Prayer of Healing", _bot->GetName());
}

void PriestSpecialization::CastCircleOfHealing()
{
    if (!_bot)
        return;

    uint32 spellId = CIRCLE_OF_HEALING;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    _bot->CastSpell(_bot, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Circle of Healing", _bot->GetName());
}

// Offensive abilities - Full implementation
void PriestSpecialization::CastSmite(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = SMITE;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Smite on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastHolyFire(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = HOLY_FIRE;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Holy Fire on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastMindBlast(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = MIND_BLAST;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Mind Blast on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastShadowWordPain(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = SHADOW_WORD_PAIN;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    // Don't refresh if target already has Shadow Word: Pain
    if (target->HasAura(spellId, _bot->GetGUID()))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Shadow Word: Pain on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::CastShadowWordDeath(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = SHADOW_WORD_DEATH;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Shadow Word: Death on {}", _bot->GetName(), target->GetName());
}

// Defensive abilities
void PriestSpecialization::UseFade()
{
    if (!_bot)
        return;

    uint32 spellId = FADE;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    _bot->CastSpell(_bot, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} used Fade", _bot->GetName());
}

void PriestSpecialization::UseDispelMagic(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = DISPEL_MAGIC;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Dispel Magic on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::UseFearWard(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = FEAR_WARD;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    // Don't refresh if target already has Fear Ward
    if (target->HasAura(spellId, _bot->GetGUID()))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Fear Ward on {}", _bot->GetName(), target->GetName());
}

void PriestSpecialization::UseShieldSpell(::Unit* target)
{
    if (!_bot || !target)
        return;

    uint32 spellId = POWER_WORD_SHIELD;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot->HasSpell(spellId))
        return;

    // Don't refresh if target already has Power Word: Shield
    if (target->HasAura(spellId, _bot->GetGUID()))
        return;

    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && !HasEnoughMana(manaCost))
        return;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange() || !_bot->IsWithinLOSInMap(target))
        return;

    _bot->CastSpell(target, spellId, false);
    TC_LOG_DEBUG("playerbot.priest", "Bot {} cast Power Word: Shield on {}", _bot->GetName(), target->GetName());
}

// Utility methods
bool PriestSpecialization::IsChanneling()
{
    return _bot && _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool PriestSpecialization::IsCasting()
{
    return _bot && _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool PriestSpecialization::CanCastSpell()
{
    return _bot && !_bot->HasUnitState(UNIT_STATE_CASTING) && !_bot->IsControlledByPlayer();
}

bool PriestSpecialization::IsInDanger()
{
    if (!_bot)
        return false;

    return _bot->GetHealthPct() < 30.0f || _bot->HasAura(25771); // Forbearance or similar dangerous debuffs
}

std::vector<::Unit*> PriestSpecialization::GetGroupMembers()
{
    std::vector<::Unit*> members;

    if (!_bot)
        return members;

    Group* group = _bot->GetGroup();
    if (!group)
    {
        // If not in group, return bot as single member
        members.push_back(_bot);
        return members;
    }

    for (GroupReference const& ref : group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (member && member->IsAlive() && _bot->IsWithinDistInMap(member, OPTIMAL_HEALING_RANGE))
        {
            members.push_back(member);
        }
    }

    return members;
}

std::vector<::Unit*> PriestSpecialization::GetInjuredGroupMembers(float healthThreshold)
{
    std::vector<::Unit*> injured;
    std::vector<::Unit*> members = GetGroupMembers();

    for (::Unit* member : members)
    {
        if (member && member->IsAlive() && member->GetHealthPct() < healthThreshold)
        {
            injured.push_back(member);
        }
    }

    // Sort by health percentage (lowest first)
    std::sort(injured.begin(), injured.end(),
        [](::Unit* a, ::Unit* b) {
            return a->GetHealthPct() < b->GetHealthPct();
        });

    return injured;
}

} // namespace Playerbot