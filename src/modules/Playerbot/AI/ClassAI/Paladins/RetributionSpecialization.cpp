/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RetributionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Item.h"
#include "Log.h"

namespace Playerbot
{

RetributionSpecialization::RetributionSpecialization(Player* bot)
    : PaladinSpecialization(bot)
    , _holyPower(0)
    , _hasArtOfWar(false)
{
}

void RetributionSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateSealTwisting();
    UpdateArtOfWar();
    UpdateDivineStorm();
    OptimizeTwoHandedWeapon();

    if (ShouldCastTemplarsVerdict(target) && _holyPower >= 3)
    {
        CastTemplarsVerdict(target);
        return;
    }

    if (ShouldCastDivineStorm() && _holyPower >= 3)
    {
        CastDivineStorm();
        return;
    }

    if (ShouldCastCrusaderStrike(target))
    {
        CastCrusaderStrike(target);
        return;
    }

    if (ShouldCastExorcism(target) && _hasArtOfWar)
    {
        CastExorcism(target);
        return;
    }

    if (ShouldCastHammerOfWrath(target))
    {
        CastHammerOfWrath(target);
        return;
    }
}

void RetributionSpecialization::UpdateBuffs()
{
    UpdateAura();
    OptimizeTwoHandedWeapon();
}

void RetributionSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
}

bool RetributionSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void RetributionSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    OptimizeTwoHandedWeapon();

    if (!bot->HasAura(SEAL_OF_COMMAND))
    {
        bot->CastSpell(bot, SEAL_OF_COMMAND, false);
    }
}

void RetributionSpecialization::OnCombatEnd()
{
    _holyPower = 0;
    _hasArtOfWar = false;
    _cooldowns.clear();
}

bool RetributionSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    switch (spellId)
    {
        case TEMPLARS_VERDICT:
        case DIVINE_STORM:
            return _holyPower >= 3;
        default:
            break;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void RetributionSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    switch (spellId)
    {
        case TEMPLARS_VERDICT:
        case DIVINE_STORM:
            _holyPower = 0;
            return;
        case CRUSADER_STRIKE:
            if (_holyPower < 3)
                _holyPower++;
            break;
        default:
            break;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
    if (bot->GetPower(POWER_MANA) >= manaCost)
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
}

Position RetributionSpecialization::GetOptimalPosition(::Unit* target)
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

float RetributionSpecialization::GetOptimalRange(::Unit* target)
{
    return 5.0f;
}

void RetributionSpecialization::UpdateAura()
{
    PaladinAura optimalAura = GetOptimalAura();
    SwitchAura(optimalAura);
}

PaladinAura RetributionSpecialization::GetOptimalAura()
{
    Player* bot = GetBot();
    if (!bot)
        return PaladinAura::RETRIBUTION_AURA;

    if (bot->IsInCombat())
    {
        auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), 30.0f);
        uint32 enemyCount = 0;
        for (auto unit : units)
        {
            if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
                enemyCount++;
        }

        if (enemyCount > 2)
            return PaladinAura::RETRIBUTION_AURA;
        else
            return PaladinAura::SANCTITY;
    }

    return PaladinAura::RETRIBUTION_AURA;
}

void RetributionSpecialization::SwitchAura(PaladinAura aura)
{
}

void RetributionSpecialization::UpdateSealTwisting()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasAura(SEAL_OF_COMMAND))
    {
        bot->CastSpell(bot, SEAL_OF_COMMAND, false);
    }
}

void RetributionSpecialization::UpdateArtOfWar()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    _hasArtOfWar = bot->HasAura(ART_OF_WAR);
}

void RetributionSpecialization::UpdateDivineStorm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (ShouldCastDivineStorm())
    {
        CastDivineStorm();
    }
}

bool RetributionSpecialization::ShouldCastCrusaderStrike(::Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    return HasEnoughResource(CRUSADER_STRIKE) &&
           bot->GetDistance(target) <= 5.0f &&
           _holyPower < 3;
}

bool RetributionSpecialization::ShouldCastDivineStorm()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), 8.0f);
    uint32 enemyCount = 0;
    for (auto unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
            enemyCount++;
    }

    return HasEnoughResource(DIVINE_STORM) && enemyCount >= 2;
}

bool RetributionSpecialization::ShouldCastTemplarsVerdict(::Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    return HasEnoughResource(TEMPLARS_VERDICT) &&
           bot->GetDistance(target) <= 5.0f;
}

bool RetributionSpecialization::ShouldCastExorcism(::Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    bool isUndead = target->GetCreatureType() == CREATURE_TYPE_UNDEAD;
    bool isDemon = target->GetCreatureType() == CREATURE_TYPE_DEMON;

    return HasEnoughResource(EXORCISM) &&
           bot->GetDistance(target) <= 30.0f &&
           (isUndead || isDemon || _hasArtOfWar);
}

void RetributionSpecialization::OptimizeTwoHandedWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!HasTwoHandedWeapon())
    {
        TC_LOG_DEBUG("playerbot", "Retribution paladin should use two-handed weapon for optimal DPS");
    }
}

bool RetributionSpecialization::HasTwoHandedWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainHand)
        return false;

    uint32 subClass = mainHand->GetTemplate()->GetSubClass();
    return (subClass == ITEM_SUBCLASS_WEAPON_SWORD2 ||
            subClass == ITEM_SUBCLASS_WEAPON_AXE2 ||
            subClass == ITEM_SUBCLASS_WEAPON_MACE2 ||
            subClass == ITEM_SUBCLASS_WEAPON_POLEARM);
}

void RetributionSpecialization::CastCrusaderStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(CRUSADER_STRIKE))
    {
        bot->CastSpell(target, CRUSADER_STRIKE, false);
        ConsumeResource(CRUSADER_STRIKE);
        _cooldowns[CRUSADER_STRIKE] = 6000;
    }
}

void RetributionSpecialization::CastTemplarsVerdict(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(TEMPLARS_VERDICT))
    {
        bot->CastSpell(target, TEMPLARS_VERDICT, false);
        ConsumeResource(TEMPLARS_VERDICT);
        _cooldowns[TEMPLARS_VERDICT] = 1500;
    }
}

void RetributionSpecialization::CastDivineStorm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(DIVINE_STORM))
    {
        bot->CastSpell(bot, DIVINE_STORM, false);
        ConsumeResource(DIVINE_STORM);
        _cooldowns[DIVINE_STORM] = 1500;
    }
}

void RetributionSpecialization::CastExorcism(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(EXORCISM))
    {
        bot->CastSpell(target, EXORCISM, false);
        ConsumeResource(EXORCISM);
        _cooldowns[EXORCISM] = 15000;
        _hasArtOfWar = false;
    }
}

void RetributionSpecialization::CastHammerOfWrath(::Unit* target)
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

bool RetributionSpecialization::ShouldCastHammerOfWrath(::Unit* target)
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

} // namespace Playerbot