/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BalanceSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

BalanceSpecialization::BalanceSpecialization(Player* bot)
    : DruidSpecialization(bot)
    , _eclipseState(EclipseState::NONE)
    , _solarEnergy(0)
    , _lunarEnergy(0)
    , _lastEclipseShift(0)
    , _starfireCount(0)
    , _wrathCount(0)
    , _eclipseActive(false)
    , _totalDamageDealt(0)
    , _manaSpent(0)
    , _eclipseProcs(0)
{
    _currentForm = DruidForm::HUMANOID;
}

void BalanceSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateEclipseSystem();
    UpdateFormManagement();
    UpdateDotHotManagement();

    // Ensure we're in Moonkin form for optimal DPS
    if (!IsInForm(DruidForm::MOONKIN) && ShouldUseMoonkinForm())
    {
        EnterMoonkinForm();
        return;
    }

    // Priority rotation based on Eclipse state
    if (ShouldCastStarsurge(target))
    {
        CastStarsurge(target);
        return;
    }

    if (ShouldCastMoonfire(target) && !target->HasAura(MOONFIRE))
    {
        CastMoonfire(target);
        return;
    }

    // Eclipse-based spell selection
    switch (_eclipseState)
    {
        case EclipseState::SOLAR:
            if (ShouldCastWrath(target))
                CastWrath(target);
            break;
        case EclipseState::LUNAR:
            if (ShouldCastStarfire(target))
                CastStarfire(target);
            break;
        case EclipseState::NONE:
            // Build toward eclipse
            if (_solarEnergy < _lunarEnergy && ShouldCastWrath(target))
                CastWrath(target);
            else if (ShouldCastStarfire(target))
                CastStarfire(target);
            break;
    }
}

void BalanceSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Mark of the Wild
    if (!bot->HasAura(MARK_OF_THE_WILD) && bot->HasSpell(MARK_OF_THE_WILD))
    {
        bot->CastSpell(bot, MARK_OF_THE_WILD, false);
    }

    // Maintain Thorns
    if (!bot->HasAura(THORNS) && bot->HasSpell(THORNS))
    {
        bot->CastSpell(bot, THORNS, false);
    }

    UpdateFormManagement();
}

void BalanceSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_lastEclipseShift > diff)
        _lastEclipseShift -= diff;
    else
        _lastEclipseShift = 0;

    if (_lastFormShift > diff)
        _lastFormShift -= diff;
    else
        _lastFormShift = 0;
}

bool BalanceSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    if (!CanCastInCurrentForm(spellId))
        return false;

    return HasEnoughResource(spellId);
}

void BalanceSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Enter Moonkin form for combat
    if (ShouldUseMoonkinForm())
        EnterMoonkinForm();

    // Reset eclipse state for new combat
    _eclipseState = EclipseState::NONE;
    _solarEnergy = 0;
    _lunarEnergy = 0;
}

void BalanceSpecialization::OnCombatEnd()
{
    _eclipseState = EclipseState::NONE;
    _solarEnergy = 0;
    _lunarEnergy = 0;
    _starfireCount = 0;
    _wrathCount = 0;
    _eclipseActive = false;
    _cooldowns.clear();
}

bool BalanceSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return true;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void BalanceSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    if (bot->GetPower(POWER_MANA) >= manaCost)
    {
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
        _manaSpent += manaCost;
    }
}

Position BalanceSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    float distance = OPTIMAL_CASTING_RANGE * 0.8f;
    float angle = target->GetAngle(bot) + M_PI;

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float BalanceSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

void BalanceSpecialization::UpdateFormManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    DruidForm optimalForm = GetOptimalFormForSituation();
    if (_currentForm != optimalForm && ShouldShiftToForm(optimalForm))
    {
        ShiftToForm(optimalForm);
    }
}

DruidForm BalanceSpecialization::GetOptimalFormForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return DruidForm::HUMANOID;

    if (bot->IsInCombat())
        return DruidForm::MOONKIN;
    else
        return DruidForm::HUMANOID;
}

bool BalanceSpecialization::ShouldShiftToForm(DruidForm form)
{
    return _currentForm != form && _lastFormShift == 0;
}

void BalanceSpecialization::ShiftToForm(DruidForm form)
{
    CastShapeshift(form);
    _previousForm = _currentForm;
    _currentForm = form;
    _lastFormShift = 1500; // GCD
}

void BalanceSpecialization::UpdateDotHotManagement()
{
    // Update DoT timers for Balance spells
    uint32 now = getMSTime();

    // Clean up expired timers
    for (auto it = _moonfireTimers.begin(); it != _moonfireTimers.end();)
    {
        if (now - it->second > 18000) // 18 second duration
            it = _moonfireTimers.erase(it);
        else
            ++it;
    }
}

bool BalanceSpecialization::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    switch (spellId)
    {
        case MOONFIRE:
            return !target->HasAura(MOONFIRE) && HasEnoughResource(MOONFIRE);
        default:
            return false;
    }
}

bool BalanceSpecialization::ShouldApplyHoT(::Unit* target, uint32 spellId)
{
    // Balance doesn't typically use HoTs
    return false;
}

void BalanceSpecialization::UpdateEclipseSystem()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check for eclipse procs
    if (bot->HasAura(ECLIPSE_SOLAR))
    {
        _eclipseState = EclipseState::SOLAR;
        _eclipseActive = true;
    }
    else if (bot->HasAura(ECLIPSE_LUNAR))
    {
        _eclipseState = EclipseState::LUNAR;
        _eclipseActive = true;
    }
    else
    {
        _eclipseState = EclipseState::NONE;
        _eclipseActive = false;
    }
}

bool BalanceSpecialization::ShouldCastStarfire(::Unit* target)
{
    return target && HasEnoughResource(STARFIRE) &&
           bot->GetDistance(target) <= OPTIMAL_CASTING_RANGE;
}

bool BalanceSpecialization::ShouldCastWrath(::Unit* target)
{
    return target && HasEnoughResource(WRATH) &&
           bot->GetDistance(target) <= OPTIMAL_CASTING_RANGE;
}

bool BalanceSpecialization::ShouldCastStarsurge(::Unit* target)
{
    return target && HasEnoughResource(STARSURGE) &&
           CanUseAbility(STARSURGE) &&
           bot->GetDistance(target) <= OPTIMAL_CASTING_RANGE;
}

bool BalanceSpecialization::ShouldCastMoonfire(::Unit* target)
{
    return target && !target->HasAura(MOONFIRE) &&
           HasEnoughResource(MOONFIRE) &&
           bot->GetDistance(target) <= OPTIMAL_CASTING_RANGE;
}

void BalanceSpecialization::CastStarfire(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(STARFIRE))
    {
        bot->CastSpell(target, STARFIRE, false);
        ConsumeResource(STARFIRE);
        _starfireCount++;
        _lunarEnergy = std::min(_lunarEnergy + 20, ECLIPSE_ENERGY_MAX);
    }
}

void BalanceSpecialization::CastWrath(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(WRATH))
    {
        bot->CastSpell(target, WRATH, false);
        ConsumeResource(WRATH);
        _wrathCount++;
        _solarEnergy = std::min(_solarEnergy + 15, ECLIPSE_ENERGY_MAX);
    }
}

void BalanceSpecialization::CastStarsurge(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(STARSURGE))
    {
        bot->CastSpell(target, STARSURGE, false);
        ConsumeResource(STARSURGE);
        _cooldowns[STARSURGE] = STARSURGE_COOLDOWN;
    }
}

void BalanceSpecialization::CastMoonfire(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(MOONFIRE))
    {
        bot->CastSpell(target, MOONFIRE, false);
        ConsumeResource(MOONFIRE);
        ApplyDoT(target, MOONFIRE);
        _moonfireTimers[target->GetGUID()] = getMSTime();
    }
}

void BalanceSpecialization::CastForceOfNature()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(FORCE_OF_NATURE) && CanUseAbility(FORCE_OF_NATURE))
    {
        bot->CastSpell(bot, FORCE_OF_NATURE, false);
        ConsumeResource(FORCE_OF_NATURE);
        _cooldowns[FORCE_OF_NATURE] = FORCE_OF_NATURE_COOLDOWN;
    }
}

void BalanceSpecialization::EnterMoonkinForm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(MOONKIN_FORM) && !IsInForm(DruidForm::MOONKIN))
    {
        ShiftToForm(DruidForm::MOONKIN);
    }
}

bool BalanceSpecialization::ShouldUseMoonkinForm()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(MOONKIN_FORM) && bot->IsInCombat();
}

} // namespace Playerbot