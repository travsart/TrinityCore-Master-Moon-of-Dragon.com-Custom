/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GuardianSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

GuardianSpecialization::GuardianSpecialization(Player* bot)
    : DruidSpecialization(bot)
    , _rage(0)
    , _maxRage(RAGE_MAX)
    , _lastRageDecay(0)
    , _rageDecayRate(RAGE_DECAY_RATE)
    , _thrashStacks(0)
    , _lacerateStacks(0)
    , _lastThreatUpdate(0)
    , _survivalInstinctsReady(0)
    , _frenziedRegenerationReady(0)
    , _lastSurvivalInstincts(0)
    , _lastFrenziedRegeneration(0)
    , _totalThreatGenerated(0)
    , _rageSpent(0)
    , _damageAbsorbed(0)
{
    _currentForm = DruidForm::HUMANOID;
}

void GuardianSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateRageManagement();
    UpdateThreatManagement();
    UpdateFormManagement();
    UpdateDotHotManagement();
    UpdateDefensiveCooldowns();

    // Ensure we're in Bear form for tanking
    if (!IsInForm(DruidForm::BEAR) && ShouldUseBearForm())
    {
        EnterBearForm();
        return;
    }

    // Emergency defensive abilities
    if (bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100)
    {
        ManageEmergencyAbilities();
        return;
    }

    // Use defensive cooldowns if needed
    if (bot->GetHealthPct() < 50.0f)
    {
        UseDefensiveCooldowns();
    }

    // Threat generation priority
    std::vector<::Unit*> threatTargets = GetThreatTargets();
    if (threatTargets.size() > 1)
    {
        // Multi-target: Use Thrash and Swipe
        if (ShouldCastThrash())
        {
            CastThrash();
            return;
        }

        if (ShouldCastSwipe())
        {
            CastSwipe();
            return;
        }
    }

    // Single target rotation
    if (ShouldCastMangle(target))
    {
        CastMangle(target);
        return;
    }

    if (ShouldCastLacerate(target))
    {
        CastLacerate(target);
        return;
    }

    if (ShouldCastMaul(target))
    {
        CastMaul(target);
        return;
    }
}

void GuardianSpecialization::UpdateBuffs()
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

void GuardianSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_survivalInstinctsReady > diff)
        _survivalInstinctsReady -= diff;
    else
        _survivalInstinctsReady = 0;

    if (_frenziedRegenerationReady > diff)
        _frenziedRegenerationReady -= diff;
    else
        _frenziedRegenerationReady = 0;

    if (_lastSurvivalInstincts > diff)
        _lastSurvivalInstincts -= diff;
    else
        _lastSurvivalInstincts = 0;

    if (_lastFrenziedRegeneration > diff)
        _lastFrenziedRegeneration -= diff;
    else
        _lastFrenziedRegeneration = 0;

    if (_lastFormShift > diff)
        _lastFormShift -= diff;
    else
        _lastFormShift = 0;

    if (_lastThreatUpdate > diff)
        _lastThreatUpdate -= diff;
    else
        _lastThreatUpdate = 0;
}

bool GuardianSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    if (!CanCastInCurrentForm(spellId))
        return false;

    return HasEnoughResource(spellId);
}

void GuardianSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Enter Bear form for combat
    if (ShouldUseBearForm())
        EnterBearForm();

    // Reset threat tracking
    _threatTargets.clear();
    _thrashStacks = 0;
    _lacerateStacks = 0;
}

void GuardianSpecialization::OnCombatEnd()
{
    _rage = 0;
    _threatTargets.clear();
    _thrashStacks = 0;
    _lacerateStacks = 0;
    _cooldowns.clear();
    _lacerateTimers.clear();
    _thrashTimers.clear();
}

bool GuardianSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;

    // Check rage cost for Guardian abilities
    switch (spellId)
    {
        case MAUL:
            return _rage >= 30;
        case MANGLE_BEAR:
            return _rage >= 15;
        case THRASH:
            return _rage >= 15;
        case SWIPE:
            return _rage >= 20;
        case LACERATE:
            return _rage >= 15;
        case FRENZIED_REGENERATION:
            return _frenziedRegenerationReady == 0;
        case SURVIVAL_INSTINCTS:
            return _survivalInstinctsReady == 0;
        default:
            auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
            return bot->GetPower(POWER_MANA) >= manaCost;
    }
}

void GuardianSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    switch (spellId)
    {
        case MAUL:
            SpendRage(30);
            break;
        case MANGLE_BEAR:
            SpendRage(15);
            break;
        case THRASH:
            SpendRage(15);
            break;
        case SWIPE:
            SpendRage(20);
            break;
        case LACERATE:
            SpendRage(15);
            break;
        case FRENZIED_REGENERATION:
            _frenziedRegenerationReady = FRENZIED_REGENERATION_COOLDOWN;
            break;
        case SURVIVAL_INSTINCTS:
            _survivalInstinctsReady = SURVIVAL_INSTINCTS_COOLDOWN;
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

Position GuardianSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Tank stays in front of target
    float distance = MELEE_RANGE * 0.8f;
    float angle = target->GetAngle(bot);

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        target->GetAngle(bot) + M_PI
    );
}

float GuardianSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

void GuardianSpecialization::UpdateFormManagement()
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

DruidForm GuardianSpecialization::GetOptimalFormForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return DruidForm::HUMANOID;

    if (bot->IsInCombat())
        return DruidForm::BEAR;
    else
        return DruidForm::HUMANOID;
}

bool GuardianSpecialization::ShouldShiftToForm(DruidForm form)
{
    return _currentForm != form && _lastFormShift == 0;
}

void GuardianSpecialization::ShiftToForm(DruidForm form)
{
    CastShapeshift(form);
    _previousForm = _currentForm;
    _currentForm = form;
    _lastFormShift = 1500; // GCD
}

void GuardianSpecialization::UpdateDotHotManagement()
{
    uint32 now = getMSTime();

    // Clean up expired DoT timers
    for (auto it = _lacerateTimers.begin(); it != _lacerateTimers.end();)
    {
        if (now - it->second > LACERATE_DURATION)
            it = _lacerateTimers.erase(it);
        else
            ++it;
    }

    for (auto it = _thrashTimers.begin(); it != _thrashTimers.end();)
    {
        if (now - it->second > THRASH_DURATION)
            it = _thrashTimers.erase(it);
        else
            ++it;
    }
}

bool GuardianSpecialization::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    switch (spellId)
    {
        case LACERATE:
            return HasEnoughResource(LACERATE);
        case THRASH:
            return HasEnoughResource(THRASH);
        default:
            return false;
    }
}

bool GuardianSpecialization::ShouldApplyHoT(::Unit* target, uint32 spellId)
{
    // Guardian doesn't typically use HoTs
    return false;
}

void GuardianSpecialization::UpdateRageManagement()
{
    uint32 now = getMSTime();
    if (_lastRageDecay == 0)
        _lastRageDecay = now;

    // Rage decays over time when not in combat
    Player* bot = GetBot();
    if (bot && !bot->IsInCombat())
    {
        uint32 timeDiff = now - _lastRageDecay;
        if (timeDiff >= 1000) // Decay every second
        {
            uint32 rageToLose = (timeDiff / 1000) * _rageDecayRate;
            if (_rage > rageToLose)
                _rage -= rageToLose;
            else
                _rage = 0;
            _lastRageDecay = now;
        }
    }
}

void GuardianSpecialization::UpdateThreatManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastThreatUpdate < 1000) // Update every second
        return;

    _threatTargets.clear();

    // Find all hostile targets in threat range
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            // Check for enemies targeting group members
            for (auto& threatRef : member->getHostileRefManager())
            {
                if (::Unit* enemy = threatRef.GetSource()->GetOwner())
                {
                    if (enemy->IsWithinDistInMap(bot, 30.0f))
                        _threatTargets.push_back(enemy);
                }
            }
        }
    }

    _lastThreatUpdate = now;
}

void GuardianSpecialization::UpdateDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if we need emergency healing
    if (bot->GetHealthPct() < 30.0f && ShouldCastFrenziedRegeneration())
    {
        CastFrenziedRegeneration();
    }

    // Check if we need damage reduction
    if (bot->GetHealthPct() < 40.0f && ShouldCastSurvivalInstincts())
    {
        CastSurvivalInstincts();
    }
}

bool GuardianSpecialization::ShouldCastMaul(::Unit* target)
{
    return target && HasEnoughResource(MAUL) &&
           bot->IsWithinMeleeRange(target) && _rage >= 50; // Only when rage is high
}

bool GuardianSpecialization::ShouldCastMangle(::Unit* target)
{
    return target && HasEnoughResource(MANGLE_BEAR) &&
           bot->IsWithinMeleeRange(target);
}

bool GuardianSpecialization::ShouldCastThrash()
{
    return HasEnoughResource(THRASH) && _threatTargets.size() > 1;
}

bool GuardianSpecialization::ShouldCastSwipe()
{
    return HasEnoughResource(SWIPE) && _threatTargets.size() > 1;
}

bool GuardianSpecialization::ShouldCastLacerate(::Unit* target)
{
    return target && HasEnoughResource(LACERATE) &&
           bot->IsWithinMeleeRange(target);
}

bool GuardianSpecialization::ShouldCastFrenziedRegeneration()
{
    Player* bot = GetBot();
    return bot && bot->GetHealthPct() < 50.0f && _frenziedRegenerationReady == 0;
}

bool GuardianSpecialization::ShouldCastSurvivalInstincts()
{
    Player* bot = GetBot();
    return bot && bot->GetHealthPct() < 40.0f && _survivalInstinctsReady == 0;
}

void GuardianSpecialization::GenerateRage(uint32 amount)
{
    _rage = std::min(_rage + amount, _maxRage);
}

void GuardianSpecialization::SpendRage(uint32 amount)
{
    if (_rage >= amount)
    {
        _rage -= amount;
        _rageSpent += amount;
    }
}

bool GuardianSpecialization::HasEnoughRage(uint32 required)
{
    return _rage >= required;
}

uint32 GuardianSpecialization::GetRage()
{
    return _rage;
}

float GuardianSpecialization::GetRagePercent()
{
    return static_cast<float>(_rage) / static_cast<float>(_maxRage);
}

void GuardianSpecialization::BuildThreat(::Unit* target)
{
    if (!target)
        return;

    // Use threat-generating abilities
    if (ShouldCastMangle(target))
        CastMangle(target);
    else if (ShouldCastLacerate(target))
        CastLacerate(target);
}

void GuardianSpecialization::MaintainThreat()
{
    // Check threat on all targets and use AoE abilities if needed
    if (_threatTargets.size() > 1)
    {
        if (ShouldCastThrash())
            CastThrash();
        else if (ShouldCastSwipe())
            CastSwipe();
    }
}

std::vector<::Unit*> GuardianSpecialization::GetThreatTargets()
{
    return _threatTargets;
}

bool GuardianSpecialization::NeedsThreat(::Unit* target)
{
    if (!target)
        return false;

    // Check if target is attacking someone other than the tank
    return target->GetTarget() != GetBot();
}

void GuardianSpecialization::CastMaul(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(MAUL))
    {
        bot->CastSpell(target, MAUL, false);
        ConsumeResource(MAUL);
        BuildThreat(target);
    }
}

void GuardianSpecialization::CastMangle(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(MANGLE_BEAR))
    {
        bot->CastSpell(target, MANGLE_BEAR, false);
        ConsumeResource(MANGLE_BEAR);
        GenerateRage(5); // Mangle generates rage
        BuildThreat(target);
    }
}

void GuardianSpecialization::CastThrash()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(THRASH))
    {
        bot->CastSpell(bot, THRASH, false);
        ConsumeResource(THRASH);
        _thrashStacks++;
        _thrashTimers[bot->GetGUID()] = getMSTime();

        // Generate threat on all nearby enemies
        for (::Unit* target : _threatTargets)
            BuildThreat(target);
    }
}

void GuardianSpecialization::CastSwipe()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SWIPE))
    {
        bot->CastSpell(bot, SWIPE, false);
        ConsumeResource(SWIPE);

        // Generate threat on all nearby enemies
        for (::Unit* target : _threatTargets)
            BuildThreat(target);
    }
}

void GuardianSpecialization::CastLacerate(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(LACERATE))
    {
        bot->CastSpell(target, LACERATE, false);
        ConsumeResource(LACERATE);
        ApplyDoT(target, LACERATE);
        _lacerateTimers[target->GetGUID()] = getMSTime();
        _lacerateStacks++;
        BuildThreat(target);
    }
}

void GuardianSpecialization::CastFrenziedRegeneration()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(FRENZIED_REGENERATION))
    {
        bot->CastSpell(bot, FRENZIED_REGENERATION, false);
        ConsumeResource(FRENZIED_REGENERATION);
        _lastFrenziedRegeneration = getMSTime();
    }
}

void GuardianSpecialization::CastSurvivalInstincts()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SURVIVAL_INSTINCTS))
    {
        bot->CastSpell(bot, SURVIVAL_INSTINCTS, false);
        ConsumeResource(SURVIVAL_INSTINCTS);
        _lastSurvivalInstincts = getMSTime();
    }
}

void GuardianSpecialization::EnterBearForm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(BEAR_FORM) && !IsInForm(DruidForm::BEAR))
    {
        ShiftToForm(DruidForm::BEAR);
    }
}

bool GuardianSpecialization::ShouldUseBearForm()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(BEAR_FORM) && bot->IsInCombat();
}

void GuardianSpecialization::UseDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetHealthPct() < 30.0f && ShouldCastFrenziedRegeneration())
        CastFrenziedRegeneration();

    if (bot->GetHealthPct() < 40.0f && ShouldCastSurvivalInstincts())
        CastSurvivalInstincts();
}

void GuardianSpecialization::ManageEmergencyAbilities()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Priority: Survival Instincts for damage reduction
    if (ShouldCastSurvivalInstincts())
    {
        CastSurvivalInstincts();
        return;
    }

    // Secondary: Frenzied Regeneration for healing
    if (ShouldCastFrenziedRegeneration())
    {
        CastFrenziedRegeneration();
        return;
    }
}

} // namespace Playerbot