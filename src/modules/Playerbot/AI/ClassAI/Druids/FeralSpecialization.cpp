/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FeralSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

FeralSpecialization::FeralSpecialization(Player* bot)
    : DruidSpecialization(bot)
    , _comboPoints{}
    , _lastComboPointGenerated(0)
    , _lastComboPointSpent(0)
    , _energy(ENERGY_MAX)
    , _maxEnergy(ENERGY_MAX)
    , _lastEnergyRegen(0)
    , _energyRegenRate(ENERGY_REGEN_RATE)
    , _tigersFuryReady(0)
    , _savageRoarRemaining(0)
    , _lastTigersFury(0)
    , _lastSavageRoar(0)
    , _totalMeleeDamage(0)
    , _comboPointsGenerated(0)
    , _comboPointsSpent(0)
    , _energySpent(0)
{
    _currentForm = DruidForm::HUMANOID;
}

void FeralSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateComboPointSystem();
    UpdateEnergyManagement();
    UpdateFormManagement();
    UpdateDotHotManagement();

    // Ensure we're in Cat form for optimal DPS
    if (!IsInForm(DruidForm::CAT) && ShouldUseCatForm())
    {
        EnterCatForm();
        return;
    }

    // Use Tiger's Fury for energy if needed
    if (ShouldCastTigersFury())
    {
        CastTigersFury();
        return;
    }

    // Maintain Savage Roar buff
    if (ShouldCastSavageRoar())
    {
        CastSavageRoar();
        return;
    }

    // Apply DoTs if needed
    if (ShouldCastRake(target) && !target->HasAura(RAKE))
    {
        CastRake(target);
        return;
    }

    // Use combo point finishers at 4-5 combo points
    if (_comboPoints.count >= 4)
    {
        if (ShouldCastRip(target) && !target->HasAura(RIP))
        {
            CastRip(target);
            return;
        }

        if (ShouldCastFerociosBite(target))
        {
            CastFerociosBite(target);
            return;
        }
    }

    // Build combo points with generators
    if (bot->isInBack(target) && ShouldCastShred(target))
    {
        CastShred(target);
    }
    else if (ShouldCastMangle(target))
    {
        CastMangle(target);
    }
}

void FeralSpecialization::UpdateBuffs()
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

void FeralSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_tigersFuryReady > diff)
        _tigersFuryReady -= diff;
    else
        _tigersFuryReady = 0;

    if (_lastTigersFury > diff)
        _lastTigersFury -= diff;
    else
        _lastTigersFury = 0;

    if (_lastSavageRoar > diff)
        _lastSavageRoar -= diff;
    else
        _lastSavageRoar = 0;

    if (_lastFormShift > diff)
        _lastFormShift -= diff;
    else
        _lastFormShift = 0;

    if (_savageRoarRemaining > diff)
        _savageRoarRemaining -= diff;
    else
        _savageRoarRemaining = 0;
}

bool FeralSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    if (!CanCastInCurrentForm(spellId))
        return false;

    return HasEnoughResource(spellId);
}

void FeralSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Enter Cat form for combat
    if (ShouldUseCatForm())
        EnterCatForm();

    // Reset combo points for new combat
    _comboPoints.count = 0;
    _comboPoints.target = target ? target->GetGUID() : ObjectGuid::Empty;
}

void FeralSpecialization::OnCombatEnd()
{
    _comboPoints.count = 0;
    _comboPoints.target = ObjectGuid::Empty;
    _energy = ENERGY_MAX;
    _savageRoarRemaining = 0;
    _cooldowns.clear();
    _rakeTimers.clear();
    _ripTimers.clear();
}

bool FeralSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;

    // Check energy cost for feral abilities
    switch (spellId)
    {
        case SHRED:
            return _energy >= 42;
        case MANGLE_CAT:
            return _energy >= 35;
        case RAKE:
            return _energy >= 35;
        case RIP:
            return _energy >= 30 && _comboPoints.count > 0;
        case FEROCIOUS_BITE:
            return _energy >= 35 && _comboPoints.count > 0;
        case SAVAGE_ROAR:
            return _energy >= 25 && _comboPoints.count > 0;
        case TIGERS_FURY:
            return _tigersFuryReady == 0;
        default:
            auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
            return bot->GetPower(POWER_MANA) >= manaCost;
    }
}

void FeralSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    switch (spellId)
    {
        case SHRED:
            SpendEnergy(42);
            break;
        case MANGLE_CAT:
            SpendEnergy(35);
            break;
        case RAKE:
            SpendEnergy(35);
            break;
        case RIP:
            SpendEnergy(30);
            SpendComboPoints(bot->GetTarget());
            break;
        case FEROCIOUS_BITE:
            SpendEnergy(35);
            SpendComboPoints(bot->GetTarget());
            break;
        case SAVAGE_ROAR:
            SpendEnergy(25);
            SpendComboPoints(bot->GetTarget());
            break;
        case TIGERS_FURY:
            _tigersFuryReady = TIGERS_FURY_COOLDOWN;
            break;
        default:
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
            if (spellInfo)
            {
                auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
                if (bot->GetPower(POWER_MANA) >= manaCost)
                    bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
            }
            break;
    }
}

Position FeralSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Prefer behind target for Shred
    float distance = MELEE_RANGE * 0.8f;
    float angle = target->GetOrientation() + M_PI;

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float FeralSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

void FeralSpecialization::UpdateFormManagement()
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

DruidForm FeralSpecialization::GetOptimalFormForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return DruidForm::HUMANOID;

    if (bot->IsInCombat())
        return DruidForm::CAT;
    else
        return DruidForm::HUMANOID;
}

bool FeralSpecialization::ShouldShiftToForm(DruidForm form)
{
    return _currentForm != form && _lastFormShift == 0;
}

void FeralSpecialization::ShiftToForm(DruidForm form)
{
    CastShapeshift(form);
    _previousForm = _currentForm;
    _currentForm = form;
    _lastFormShift = 1500; // GCD
}

void FeralSpecialization::UpdateDotHotManagement()
{
    uint32 now = getMSTime();

    // Clean up expired DoT timers
    for (auto it = _rakeTimers.begin(); it != _rakeTimers.end();)
    {
        if (now - it->second > RAKE_DURATION)
            it = _rakeTimers.erase(it);
        else
            ++it;
    }

    for (auto it = _ripTimers.begin(); it != _ripTimers.end();)
    {
        if (now - it->second > RIP_DURATION)
            it = _ripTimers.erase(it);
        else
            ++it;
    }
}

bool FeralSpecialization::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    switch (spellId)
    {
        case RAKE:
            return !target->HasAura(RAKE) && HasEnoughResource(RAKE);
        case RIP:
            return !target->HasAura(RIP) && HasEnoughResource(RIP) && _comboPoints.count >= 4;
        default:
            return false;
    }
}

bool FeralSpecialization::ShouldApplyHoT(::Unit* target, uint32 spellId)
{
    // Feral doesn't typically use HoTs
    return false;
}

void FeralSpecialization::UpdateComboPointSystem()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Update combo point count from player
    ::Unit* target = bot->GetTarget();
    if (target)
    {
        _comboPoints.target = target->GetGUID();
        _comboPoints.count = bot->GetComboPoints();
    }
    else
    {
        _comboPoints.count = 0;
        _comboPoints.target = ObjectGuid::Empty;
    }
}

void FeralSpecialization::UpdateEnergyManagement()
{
    uint32 now = getMSTime();
    if (_lastEnergyRegen == 0)
        _lastEnergyRegen = now;

    uint32 timeDiff = now - _lastEnergyRegen;
    if (timeDiff >= 1000) // Regenerate every second
    {
        uint32 energyToAdd = (timeDiff / 1000) * _energyRegenRate;
        _energy = std::min(_energy + energyToAdd, _maxEnergy);
        _lastEnergyRegen = now;
    }
}

void FeralSpecialization::UpdateFeralBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check Savage Roar duration
    if (bot->HasAura(SAVAGE_ROAR))
    {
        // Update remaining time based on aura
        if (Aura* aura = bot->GetAura(SAVAGE_ROAR))
            _savageRoarRemaining = aura->GetDuration();
    }
    else
    {
        _savageRoarRemaining = 0;
    }
}

bool FeralSpecialization::ShouldCastShred(::Unit* target)
{
    return target && HasEnoughResource(SHRED) &&
           bot->IsWithinMeleeRange(target) && bot->isInBack(target);
}

bool FeralSpecialization::ShouldCastMangle(::Unit* target)
{
    return target && HasEnoughResource(MANGLE_CAT) &&
           bot->IsWithinMeleeRange(target);
}

bool FeralSpecialization::ShouldCastRake(::Unit* target)
{
    return target && !target->HasAura(RAKE) &&
           HasEnoughResource(RAKE) && bot->IsWithinMeleeRange(target);
}

bool FeralSpecialization::ShouldCastRip(::Unit* target)
{
    return target && !target->HasAura(RIP) &&
           HasEnoughResource(RIP) && _comboPoints.count >= 4 &&
           bot->IsWithinMeleeRange(target);
}

bool FeralSpecialization::ShouldCastFerociosBite(::Unit* target)
{
    return target && HasEnoughResource(FEROCIOUS_BITE) &&
           _comboPoints.count >= 4 && bot->IsWithinMeleeRange(target) &&
           target->GetHealthPct() < 25.0f; // Execute range
}

bool FeralSpecialization::ShouldCastSavageRoar()
{
    return _savageRoarRemaining < 5000 && // Less than 5 seconds remaining
           _comboPoints.count >= 1 && HasEnoughResource(SAVAGE_ROAR);
}

bool FeralSpecialization::ShouldCastTigersFury()
{
    return _energy < 30 && _tigersFuryReady == 0; // Low energy and off cooldown
}

void FeralSpecialization::GenerateComboPoint(::Unit* target)
{
    if (!target || _comboPoints.count >= COMBO_POINTS_MAX)
        return;

    if (_comboPoints.target != target->GetGUID())
    {
        _comboPoints.count = 0;
        _comboPoints.target = target->GetGUID();
    }

    _comboPoints.count++;
    _comboPointsGenerated++;
    _lastComboPointGenerated = getMSTime();
}

void FeralSpecialization::SpendComboPoints(::Unit* target)
{
    if (!target || _comboPoints.count == 0)
        return;

    _comboPointsSpent += _comboPoints.count;
    _comboPoints.count = 0;
    _lastComboPointSpent = getMSTime();
}

bool FeralSpecialization::ShouldSpendComboPoints()
{
    return _comboPoints.count >= 4;
}

void FeralSpecialization::OptimizeComboPointUsage()
{
    // Spend combo points on high-value finishers
    if (_comboPoints.count >= 4)
    {
        ::Unit* target = GetBot()->GetTarget();
        if (target)
        {
            if (!target->HasAura(RIP) && ShouldCastRip(target))
                return; // RIP takes priority

            if (target->GetHealthPct() < 25.0f && ShouldCastFerociosBite(target))
                return; // Execute with Ferocious Bite
        }
    }
}

bool FeralSpecialization::HasEnoughEnergy(uint32 required)
{
    return _energy >= required;
}

void FeralSpecialization::SpendEnergy(uint32 amount)
{
    if (_energy >= amount)
    {
        _energy -= amount;
        _energySpent += amount;
    }
}

uint32 FeralSpecialization::GetEnergy()
{
    return _energy;
}

float FeralSpecialization::GetEnergyPercent()
{
    return static_cast<float>(_energy) / static_cast<float>(_maxEnergy);
}

void FeralSpecialization::CastShred(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(SHRED))
    {
        bot->CastSpell(target, SHRED, false);
        ConsumeResource(SHRED);
        GenerateComboPoint(target);
    }
}

void FeralSpecialization::CastMangle(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(MANGLE_CAT))
    {
        bot->CastSpell(target, MANGLE_CAT, false);
        ConsumeResource(MANGLE_CAT);
        GenerateComboPoint(target);
    }
}

void FeralSpecialization::CastRake(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(RAKE))
    {
        bot->CastSpell(target, RAKE, false);
        ConsumeResource(RAKE);
        ApplyDoT(target, RAKE);
        _rakeTimers[target->GetGUID()] = getMSTime();
        GenerateComboPoint(target);
    }
}

void FeralSpecialization::CastRip(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(RIP))
    {
        bot->CastSpell(target, RIP, false);
        ConsumeResource(RIP);
        ApplyDoT(target, RIP);
        _ripTimers[target->GetGUID()] = getMSTime();
    }
}

void FeralSpecialization::CastFerociosBite(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(FEROCIOUS_BITE))
    {
        bot->CastSpell(target, FEROCIOUS_BITE, false);
        ConsumeResource(FEROCIOUS_BITE);
    }
}

void FeralSpecialization::CastSavageRoar()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SAVAGE_ROAR))
    {
        bot->CastSpell(bot, SAVAGE_ROAR, false);
        ConsumeResource(SAVAGE_ROAR);
        _savageRoarRemaining = SAVAGE_ROAR_DURATION;
        _lastSavageRoar = getMSTime();
    }
}

void FeralSpecialization::CastTigersFury()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(TIGERS_FURY))
    {
        bot->CastSpell(bot, TIGERS_FURY, false);
        ConsumeResource(TIGERS_FURY);
        _energy = std::min(_energy + 60, _maxEnergy); // Tiger's Fury gives energy
        _lastTigersFury = getMSTime();
    }
}

void FeralSpecialization::EnterCatForm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(CAT_FORM) && !IsInForm(DruidForm::CAT))
    {
        ShiftToForm(DruidForm::CAT);
    }
}

bool FeralSpecialization::ShouldUseCatForm()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(CAT_FORM) && bot->IsInCombat();
}

void FeralSpecialization::ManageStealth()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (ShouldUseStealth() && !bot->HasAura(PROWL))
    {
        CastProwl();
    }
}

void FeralSpecialization::CastProwl()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(PROWL) && IsInForm(DruidForm::CAT) && !bot->IsInCombat())
    {
        bot->CastSpell(bot, PROWL, false);
    }
}

bool FeralSpecialization::ShouldUseStealth()
{
    Player* bot = GetBot();
    return bot && IsInForm(DruidForm::CAT) && !bot->IsInCombat() &&
           bot->GetTarget() && bot->GetTarget()->IsHostileTo(bot);
}

} // namespace Playerbot