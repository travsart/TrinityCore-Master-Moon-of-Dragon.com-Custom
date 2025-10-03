/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ProtectionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ThreatMgr.h"
#include "Unit.h"
#include "Log.h"

namespace Playerbot
{

ProtectionSpecialization::ProtectionSpecialization(Player* bot)
    : PaladinSpecialization(bot)
    , _lastConsecration(0)
    , _lastAvengersShield(0)
{
}

void ProtectionSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateThreat();
    UpdateShieldBlock();
    UpdateAvengersShield();
    UpdateConsecration();

    if (ShouldCastAvengersShield(target))
    {
        CastAvengersShield(target);
        return;
    }

    if (ShouldCastHammerOfWrath(target))
    {
        CastHammerOfWrath(target);
        return;
    }

    if (ShouldCastShieldOfRighteousness(target))
    {
        CastShieldOfRighteousness(target);
        return;
    }

    if (ShouldCastConsecration())
    {
        CastConsecration();
        return;
    }
}

void ProtectionSpecialization::UpdateBuffs()
{
    UpdateAura();
    CastHolyShield();
}

void ProtectionSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_lastConsecration > diff)
        _lastConsecration -= diff;
    else
        _lastConsecration = 0;

    if (_lastAvengersShield > diff)
        _lastAvengersShield -= diff;
    else
        _lastAvengersShield = 0;
}

bool ProtectionSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void ProtectionSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    GenerateThreat();

    if (target)
        bot->GetThreatManager().AddThreat(target, 1000.0f);
}

void ProtectionSpecialization::OnCombatEnd()
{
    _lastConsecration = 0;
    _lastAvengersShield = 0;
    _cooldowns.clear();
}

bool ProtectionSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void ProtectionSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
    if (bot->GetPower(POWER_MANA) >= manaCost)
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
}

Position ProtectionSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    float distance = 5.0f;
    float angle = target->GetAngle(bot);

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle + M_PI
    );
}

float ProtectionSpecialization::GetOptimalRange(::Unit* target)
{
    return 5.0f;
}

void ProtectionSpecialization::UpdateAura()
{
    PaladinAura optimalAura = GetOptimalAura();
    SwitchAura(optimalAura);
}

PaladinAura ProtectionSpecialization::GetOptimalAura()
{
    Player* bot = GetBot();
    if (!bot)
        return PaladinAura::DEVOTION;

    if (bot->IsInCombat())
    {
        std::vector<::Unit*> threatTargets = GetThreatTargets();
        if (threatTargets.size() > 2)
            return PaladinAura::RETRIBUTION_AURA;
        else
            return PaladinAura::DEVOTION;
    }

    return PaladinAura::DEVOTION;
}

void ProtectionSpecialization::SwitchAura(PaladinAura aura)
{
}

void ProtectionSpecialization::UpdateThreat()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    MaintainThreat();

    std::vector<::Unit*> threatTargets = GetThreatTargets();
    for (::Unit* target : threatTargets)
    {
        if (NeedsThreat(target))
            GenerateThreat();
    }
}

void ProtectionSpecialization::UpdateShieldBlock()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->IsInCombat() && bot->HasItemInSlot(EQUIPMENT_SLOT_OFFHAND))
    {
        Item* shield = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        if (shield && shield->GetTemplate()->GetInventoryType() == INVTYPE_SHIELD)
        {
            CastHolyShield();
        }
    }
}

void ProtectionSpecialization::UpdateAvengersShield()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    ::Unit* target = bot->GetSelectedUnit();
    if (target && ShouldCastAvengersShield(target))
        CastAvengersShield(target);
}

void ProtectionSpecialization::UpdateConsecration()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    if (ShouldCastConsecration())
        CastConsecration();
}

bool ProtectionSpecialization::ShouldCastAvengersShield(::Unit* target)
{
    if (!target || _lastAvengersShield > 0)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    if (!bot->HasItemInSlot(EQUIPMENT_SLOT_OFFHAND))
        return false;

    Item* shield = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!shield || shield->GetTemplate()->GetInventoryType() != INVTYPE_SHIELD)
        return false;

    return HasEnoughResource(AVENGERS_SHIELD) && bot->GetDistance(target) <= 30.0f;
}

bool ProtectionSpecialization::ShouldCastHammerOfWrath(::Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    return HasEnoughResource(HAMMER_OF_WRATH) &&
           bot->GetDistance(target) <= 30.0f &&
           target->GetHealthPct() <= 20.0f;
}

bool ProtectionSpecialization::ShouldCastShieldOfRighteousness(::Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    if (!bot->HasItemInSlot(EQUIPMENT_SLOT_OFFHAND))
        return false;

    Item* shield = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!shield || shield->GetTemplate()->GetInventoryType() != INVTYPE_SHIELD)
        return false;

    return HasEnoughResource(SHIELD_OF_RIGHTEOUSNESS) &&
           bot->GetDistance(target) <= 5.0f;
}

bool ProtectionSpecialization::ShouldCastConsecration()
{
    if (_lastConsecration > 0)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    std::vector<::Unit*> nearbyEnemies;
    auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), 8.0f);
    for (auto unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
            nearbyEnemies.push_back(unit);
    }

    return HasEnoughResource(CONSECRATION) && nearbyEnemies.size() >= 2;
}

void ProtectionSpecialization::GenerateThreat()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    ::Unit* target = bot->GetSelectedUnit();
    if (target && target->IsHostileTo(bot))
    {
        bot->GetThreatManager().AddThreat(target, 500.0f);
    }
}

void ProtectionSpecialization::MaintainThreat()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    std::vector<::Unit*> threatTargets = GetThreatTargets();
    for (::Unit* target : threatTargets)
    {
        if (NeedsThreat(target))
        {
            bot->GetThreatManager().AddThreat(target, 200.0f);
        }
    }
}

std::vector<::Unit*> ProtectionSpecialization::GetThreatTargets()
{
    std::vector<::Unit*> targets;
    Player* bot = GetBot();
    if (!bot)
        return targets;

    auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), 30.0f);
    for (auto unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive() && unit->IsInCombat())
            targets.push_back(unit);
    }

    return targets;
}

bool ProtectionSpecialization::NeedsThreat(::Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    float myThreat = target->GetThreatManager().GetThreat(bot);
    float maxThreat = 0.0f;

    auto const& threatList = target->GetThreatManager().GetThreatList();
    for (auto const& ref : threatList)
    {
        if (ref->GetThreat() > maxThreat)
            maxThreat = ref->GetThreat();
    }

    return myThreat < maxThreat * 1.1f;
}

void ProtectionSpecialization::CastAvengersShield(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(AVENGERS_SHIELD))
    {
        bot->CastSpell(target, AVENGERS_SHIELD, false);
        ConsumeResource(AVENGERS_SHIELD);
        _lastAvengersShield = 30000;
    }
}

void ProtectionSpecialization::CastShieldOfRighteousness(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(SHIELD_OF_RIGHTEOUSNESS))
    {
        bot->CastSpell(target, SHIELD_OF_RIGHTEOUSNESS, false);
        ConsumeResource(SHIELD_OF_RIGHTEOUSNESS);
        _cooldowns[SHIELD_OF_RIGHTEOUSNESS] = 6000;
    }
}

void ProtectionSpecialization::CastHolyShield()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasAura(HOLY_SHIELD) && HasEnoughResource(HOLY_SHIELD))
    {
        bot->CastSpell(bot, HOLY_SHIELD, false);
        ConsumeResource(HOLY_SHIELD);
        _cooldowns[HOLY_SHIELD] = 10000;
    }
}

void ProtectionSpecialization::CastConsecration()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(CONSECRATION))
    {
        bot->CastSpell(bot, CONSECRATION, false);
        ConsumeResource(CONSECRATION);
        _lastConsecration = 20000;
    }
}

void ProtectionSpecialization::CastHammerOfWrath(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(HAMMER_OF_WRATH))
    {
        bot->CastSpell(target, HAMMER_OF_WRATH, false);
        ConsumeResource(HAMMER_OF_WRATH);
        _cooldowns[HAMMER_OF_WRATH] = 6000;
    }
}

} // namespace Playerbot