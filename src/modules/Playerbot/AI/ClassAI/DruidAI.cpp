/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DruidAI.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "Group.h"
#include "WorldSession.h"

namespace Playerbot
{

DruidAI::DruidAI(Player* bot) : ClassAI(bot)
{
    _specialization = DetectSpecialization();
    _currentForm = DruidForm::HUMANOID;
    _previousForm = DruidForm::HUMANOID;
    _damageDealt = 0;
    _healingDone = 0;
    _formShifts = 0;
    _manaSpent = 0;

    // Initialize form management
    _formTransition = FormTransition();
    _lastFormShift = 0;
    _formShiftGCD = FORM_SHIFT_GCD;
    _needsFormShift = false;
    _optimalForm = GetOptimalFormForSpecialization();

    // Initialize Balance Eclipse system
    _eclipseState = EclipseState::NONE;
    _solarEnergy = 0;
    _lunarEnergy = 0;
    _lastEclipseShift = 0;
    _starfireCount = 0;
    _wrathCount = 0;
    _eclipseActive = false;

    // Initialize Feral combo system
    _comboPoints = ComboPointInfo();
    _energy = ENERGY_MAX;
    _maxEnergy = ENERGY_MAX;
    _lastEnergyRegen = 0;
    _tigersFuryReady = 0;
    _savageRoarRemaining = 0;
    _ripRemaining = 0;

    // Initialize Guardian rage system
    _rage = 0;
    _maxRage = RAGE_MAX;
    _lastRageDecay = 0;
    _thrashStacks = 0;
    _lacerateStacks = 0;
    _survivalInstinctsReady = 0;
    _frenziedRegenerationReady = 0;

    // Initialize Restoration healing
    _lastGroupScan = 0;
    _treeOfLifeRemaining = 0;
    _tranquilityReady = 0;
    _inTreeForm = false;

    // Initialize utility tracking
    _lastInnervate = 0;
    _lastBarkskin = 0;
    _lastNaturesSwiftness = 0;
    _lastEntanglingRoots = 0;
    _lastCyclone = 0;
    _lastHibernate = 0;
}

void DruidAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    UpdateFormManagement();

    switch (_specialization)
    {
        case DruidSpec::BALANCE:
            UpdateBalanceRotation(target);
            UpdateEclipseSystem();
            break;
        case DruidSpec::FERAL:
            UpdateFeralRotation(target);
            UpdateComboPointSystem();
            UpdateEnergyManagement();
            break;
        case DruidSpec::GUARDIAN:
            UpdateGuardianRotation(target);
            UpdateRageManagement();
            break;
        case DruidSpec::RESTORATION:
            UpdateRestorationRotation(target);
            UpdateHealOverTimeManagement();
            break;
    }

    UpdateDamageOverTimeManagement();
}

void DruidAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Ensure we're in the optimal form
    ShiftToOptimalForm();

    // Apply self-buffs
    if (!_bot->HasAura(MARK_OF_THE_WILD))
    {
        if (CanUseAbility(MARK_OF_THE_WILD))
            _bot->CastSpell(_bot, MARK_OF_THE_WILD, false);
    }

    // Form-specific buffs
    if (_currentForm == DruidForm::CAT && !_bot->HasAura(SAVAGE_ROAR))
    {
        if (_comboPoints.current >= 1 && CanUseAbility(SAVAGE_ROAR))
            CastSavageRoar();
    }
}

void DruidAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);

    // Update form shift cooldown
    if (_lastFormShift > 0)
    {
        if (_lastFormShift <= diff)
            _lastFormShift = 0;
        else
            _lastFormShift -= diff;
    }

    // Update energy regeneration for Feral
    if (_specialization == DruidSpec::FERAL)
    {
        _lastEnergyRegen += diff;
        if (_lastEnergyRegen >= 1000) // Regen every second
        {
            _energy = std::min(_energy + ENERGY_REGEN_RATE, _maxEnergy);
            _lastEnergyRegen = 0;
        }
    }

    // Update rage decay for Guardian
    if (_specialization == DruidSpec::GUARDIAN && _rage > 0)
    {
        _lastRageDecay += diff;
        if (_lastRageDecay >= 1000) // Decay every second
        {
            _rage = _rage >= RAGE_DECAY_RATE ? _rage - RAGE_DECAY_RATE : 0;
            _lastRageDecay = 0;
        }
    }

    // Update HoT/DoT timers
    for (auto it = _rejuvenationTimers.begin(); it != _rejuvenationTimers.end();)
    {
        if (it->second <= diff)
            it = _rejuvenationTimers.erase(it);
        else
        {
            it->second -= diff;
            ++it;
        }
    }

    for (auto it = _moonfireTimers.begin(); it != _moonfireTimers.end();)
    {
        if (it->second <= diff)
            it = _moonfireTimers.erase(it);
        else
        {
            it->second -= diff;
            ++it;
        }
    }
}

bool DruidAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (!CanCastInCurrentForm(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check form shift cooldown
    if (_lastFormShift > 0 && RequiresHumanoidForm(spellId))
        return false;

    return true;
}

void DruidAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
    ShiftToOptimalForm();

    if (_specialization == DruidSpec::GUARDIAN)
        EnterBearForm();
    else if (_specialization == DruidSpec::FERAL)
        EnterCatForm();
    else if (_specialization == DruidSpec::BALANCE)
        EnterMoonkinForm();
}

void DruidAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    _comboPoints.current = 0;
    _comboPoints.target = nullptr;
    _eclipseState = EclipseState::NONE;
    _solarEnergy = 0;
    _lunarEnergy = 0;
}

bool DruidAI::HasEnoughResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    switch (spellInfo->PowerType)
    {
        case POWER_MANA:
            return GetMana() >= spellInfo->ManaCost;
        case POWER_ENERGY:
            return HasEnoughEnergy(spellInfo->ManaCost);
        case POWER_RAGE:
            return HasEnoughRage(spellInfo->ManaCost);
        default:
            return true;
    }
}

void DruidAI::ConsumeResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    switch (spellInfo->PowerType)
    {
        case POWER_MANA:
            _manaSpent += spellInfo->ManaCost;
            break;
        case POWER_ENERGY:
            SpendEnergy(spellInfo->ManaCost);
            break;
        case POWER_RAGE:
            SpendRage(spellInfo->ManaCost);
            break;
    }
}

Position DruidAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    Position pos = _bot->GetPosition();
    float distance = _bot->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    if (distance > optimalRange || distance < optimalRange * 0.8f)
    {
        pos = target->GetPosition();
        if (_currentForm == DruidForm::CAT || _currentForm == DruidForm::BEAR)
        {
            // Melee range positioning
            pos.m_positionX += MELEE_RANGE * cos(target->GetOrientation());
            pos.m_positionY += MELEE_RANGE * sin(target->GetOrientation());
        }
        else
        {
            // Caster range positioning
            pos.m_positionX += optimalRange * cos(target->GetOrientation() + M_PI);
            pos.m_positionY += optimalRange * sin(target->GetOrientation() + M_PI);
        }
    }

    return pos;
}

float DruidAI::GetOptimalRange(::Unit* target)
{
    if (_currentForm == DruidForm::CAT || _currentForm == DruidForm::BEAR)
        return MELEE_RANGE;

    if (_specialization == DruidSpec::RESTORATION)
        return OPTIMAL_HEALING_RANGE;

    return OPTIMAL_CASTING_RANGE;
}

void DruidAI::UpdateBalanceRotation(::Unit* target)
{
    if (!target)
        return;

    // Ensure we're in Moonkin form for optimal damage
    if (_currentForm != DruidForm::MOONKIN && CanUseAbility(MOONKIN_FORM))
    {
        EnterMoonkinForm();
        return;
    }

    // Apply DoTs first
    if (!target->HasAura(MOONFIRE) && CanUseAbility(MOONFIRE))
    {
        CastMoonfire(target);
        return;
    }

    if (!target->HasAura(SUNFIRE) && CanUseAbility(SUNFIRE))
    {
        CastSunfire(target);
        return;
    }

    // Cast based on Eclipse state
    if (_eclipseState == EclipseState::SOLAR)
    {
        if (CanUseAbility(WRATH))
            CastWrath(target);
    }
    else if (_eclipseState == EclipseState::LUNAR)
    {
        if (CanUseAbility(STARFIRE))
            CastStarfire(target);
    }
    else
    {
        // Build towards eclipse
        if (ShouldCastStarfire() && CanUseAbility(STARFIRE))
            CastStarfire(target);
        else if (ShouldCastWrath() && CanUseAbility(WRATH))
            CastWrath(target);
    }

    // Use Starsurge when available
    if (CanUseAbility(STARSURGE))
        CastStarsurge(target);
}

void DruidAI::UpdateFeralRotation(::Unit* target)
{
    if (!target)
        return;

    // Ensure we're in Cat form
    if (_currentForm != DruidForm::CAT && CanUseAbility(CAT_FORM))
    {
        EnterCatForm();
        return;
    }

    // Use Tiger's Fury for energy
    if (_energy < 50 && CanUseAbility(TIGERS_FURY))
    {
        CastTigersFury();
        return;
    }

    // Maintain Savage Roar
    if (!_bot->HasAura(SAVAGE_ROAR) && _comboPoints.current >= 1 && CanUseAbility(SAVAGE_ROAR))
    {
        CastSavageRoar();
        return;
    }

    // Apply Rake if not active
    if (!target->HasAura(RAKE) && CanUseAbility(RAKE))
    {
        CastRake(target);
        return;
    }

    // Spend combo points
    if (_comboPoints.current >= 5)
    {
        if (!target->HasAura(RIP) && CanUseAbility(RIP))
            CastRip(target);
        else if (CanUseAbility(FEROCIOUS_BITE))
            CastFerociosBite(target);
        return;
    }

    // Generate combo points
    if (CanUseAbility(SHRED))
        CastShred(target);
    else if (CanUseAbility(MANGLE_CAT))
        CastMangle(target);
}

void DruidAI::UpdateGuardianRotation(::Unit* target)
{
    if (!target)
        return;

    // Ensure we're in Bear form
    if (_currentForm != DruidForm::BEAR && CanUseAbility(BEAR_FORM))
    {
        EnterBearForm();
        return;
    }

    // Use defensive cooldowns if low health
    if (_bot->GetHealthPct() < 30.0f)
    {
        if (CanUseAbility(SURVIVAL_INSTINCTS))
            CastSurvivalInstincts();
        else if (CanUseAbility(FRENZIED_REGENERATION))
            CastFrenziedRegeneration();
    }

    // AoE abilities for multiple targets
    std::vector<::Unit*> enemies = GetAoETargets();
    if (enemies.size() >= 3)
    {
        if (CanUseAbility(THRASH))
            CastThrash();
        else if (CanUseAbility(SWIPE))
            CastSwipe();
        return;
    }

    // Single target rotation
    if (!target->HasAura(LACERATE) && CanUseAbility(LACERATE))
        CastLacerate(target);
    else if (CanUseAbility(MANGLE_BEAR))
        _bot->CastSpell(target, MANGLE_BEAR, false);
    else if (CanUseAbility(MAUL))
        CastMaul(target);
}

void DruidAI::UpdateRestorationRotation(::Unit* target)
{
    // Prioritize healing over damage
    UseRestorationAbilities();

    // DPS if no healing needed
    if (!GetBestHealTarget() && target)
    {
        if (!target->HasAura(MOONFIRE) && CanUseAbility(MOONFIRE))
            CastMoonfire(target);
        else if (CanUseAbility(WRATH))
            CastWrath(target);
    }
}

void DruidAI::UpdateFormManagement()
{
    uint32 now = getMSTime();
    if (now - _lastFormShift < _formShiftGCD)
        return;

    DruidForm optimalForm = GetOptimalFormForSituation();
    if (optimalForm != _currentForm && CanShiftToForm(optimalForm))
    {
        ShiftToForm(optimalForm);
    }
}

void DruidAI::ShiftToOptimalForm()
{
    DruidForm optimal = GetOptimalFormForSituation();
    if (optimal != _currentForm)
        ShiftToForm(optimal);
}

void DruidAI::ShiftToForm(DruidForm form)
{
    if (!CanShiftToForm(form))
        return;

    uint32 spellId = GetShapeshiftSpellId(form);
    if (spellId && CanUseAbility(spellId))
    {
        _bot->CastSpell(_bot, spellId, false);
        _previousForm = _currentForm;
        _currentForm = form;
        _lastFormShift = getMSTime();
        _formShifts++;
    }
}

bool DruidAI::CanShiftToForm(DruidForm form)
{
    if (_currentForm == form)
        return false;

    uint32 now = getMSTime();
    if (now - _lastFormShift < _formShiftGCD)
        return false;

    return true;
}

DruidForm DruidAI::GetOptimalFormForSituation()
{
    if (!_bot->IsInCombat())
    {
        // Travel forms out of combat
        if (_bot->GetMap() && _bot->GetMap()->IsOutdoor())
            return DruidForm::TRAVEL;
        return DruidForm::HUMANOID;
    }

    // Combat forms
    switch (_specialization)
    {
        case DruidSpec::BALANCE:
            return DruidForm::MOONKIN;
        case DruidSpec::FERAL:
            return DruidForm::CAT;
        case DruidSpec::GUARDIAN:
            return DruidForm::BEAR;
        case DruidSpec::RESTORATION:
            // Stay in humanoid for healing, or tree form if available
            return _inTreeForm ? DruidForm::TREE_OF_LIFE : DruidForm::HUMANOID;
    }

    return DruidForm::HUMANOID;
}

DruidForm DruidAI::GetOptimalFormForSpecialization()
{
    switch (_specialization)
    {
        case DruidSpec::BALANCE:
            return DruidForm::MOONKIN;
        case DruidSpec::FERAL:
            return DruidForm::CAT;
        case DruidSpec::GUARDIAN:
            return DruidForm::BEAR;
        case DruidSpec::RESTORATION:
            return DruidForm::HUMANOID;
    }
    return DruidForm::HUMANOID;
}

bool DruidAI::CanCastInCurrentForm(uint32 spellId)
{
    // Most spells require humanoid form
    if (RequiresHumanoidForm(spellId))
        return _currentForm == DruidForm::HUMANOID || _currentForm == DruidForm::TREE_OF_LIFE;

    // Form-specific spells
    switch (spellId)
    {
        case SHRED:
        case RAKE:
        case RIP:
        case FEROCIOUS_BITE:
        case SAVAGE_ROAR:
        case TIGERS_FURY:
            return _currentForm == DruidForm::CAT;

        case MAUL:
        case MANGLE_BEAR:
        case THRASH:
        case SWIPE:
        case LACERATE:
        case FRENZIED_REGENERATION:
        case SURVIVAL_INSTINCTS:
            return _currentForm == DruidForm::BEAR;

        case STARFIRE:
        case WRATH:
        case STARSURGE:
            return _currentForm == DruidForm::MOONKIN || _currentForm == DruidForm::HUMANOID;

        default:
            return true;
    }
}

void DruidAI::UpdateEclipseSystem()
{
    if (_specialization != DruidSpec::BALANCE)
        return;

    // Update eclipse energy based on spell casts
    uint32 now = getMSTime();
    if (now - _lastEclipseShift > 1000) // Update every second
    {
        // Eclipse energy naturally decays towards neutral
        if (_solarEnergy > 0)
            _solarEnergy = std::max(0u, _solarEnergy - 1);
        if (_lunarEnergy > 0)
            _lunarEnergy = std::max(0u, _lunarEnergy - 1);

        // Determine eclipse state
        if (_solarEnergy >= ECLIPSE_ENERGY_MAX)
            _eclipseState = EclipseState::SOLAR;
        else if (_lunarEnergy >= ECLIPSE_ENERGY_MAX)
            _eclipseState = EclipseState::LUNAR;
        else
            _eclipseState = EclipseState::NONE;

        _lastEclipseShift = now;
    }
}

bool DruidAI::ShouldCastStarfire()
{
    // Cast Starfire to build Lunar energy or during Solar eclipse
    return _eclipseState == EclipseState::SOLAR || _lunarEnergy < _solarEnergy;
}

bool DruidAI::ShouldCastWrath()
{
    // Cast Wrath to build Solar energy or during Lunar eclipse
    return _eclipseState == EclipseState::LUNAR || _solarEnergy < _lunarEnergy;
}

void DruidAI::UpdateComboPointSystem()
{
    if (_specialization != DruidSpec::FERAL)
        return;

    // Combo points are tied to specific targets
    ::Unit* currentTarget = GetTarget();
    if (currentTarget != _comboPoints.target)
    {
        _comboPoints.current = 0;
        _comboPoints.target = currentTarget;
    }
}

void DruidAI::GenerateComboPoint(::Unit* target)
{
    if (target == _comboPoints.target)
    {
        _comboPoints.AddComboPoint();
        _comboPoints.lastGenerated = getMSTime();
    }
}

bool DruidAI::ShouldSpendComboPoints()
{
    return _comboPoints.current >= 5 ||
           (_comboPoints.current >= 3 && _energy < 30);
}

bool DruidAI::HasEnoughEnergy(uint32 required)
{
    return _energy >= required;
}

void DruidAI::SpendEnergy(uint32 amount)
{
    _energy = _energy >= amount ? _energy - amount : 0;
}

uint32 DruidAI::GetEnergy()
{
    return _energy;
}

float DruidAI::GetEnergyPercent()
{
    return _maxEnergy > 0 ? (float)_energy / _maxEnergy : 0.0f;
}

bool DruidAI::HasEnoughRage(uint32 required)
{
    return _rage >= required;
}

void DruidAI::GenerateRage(uint32 amount)
{
    _rage = std::min(_rage + amount, _maxRage);
}

void DruidAI::SpendRage(uint32 amount)
{
    _rage = _rage >= amount ? _rage - amount : 0;
}

uint32 DruidAI::GetRage()
{
    return _rage;
}

float DruidAI::GetRagePercent()
{
    return _maxRage > 0 ? (float)_rage / _maxRage : 0.0f;
}

void DruidAI::UpdateHealOverTimeManagement()
{
    if (_specialization != DruidSpec::RESTORATION)
        return;

    RefreshExpiringHoTs();
}

void DruidAI::RefreshExpiringHoTs()
{
    for (auto it = _rejuvenationTimers.begin(); it != _rejuvenationTimers.end(); ++it)
    {
        if (it->second <= HOT_REFRESH_THRESHOLD)
        {
            if (Player* player = ObjectAccessor::GetPlayer(*_bot, it->first))
            {
                if (CanUseAbility(REJUVENATION))
                    CastRejuvenation(player);
            }
        }
    }
}

bool DruidAI::ShouldApplyHoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    return !target->HasAura(spellId) || target->GetRemainingTimeOnAura(spellId) < HOT_REFRESH_THRESHOLD;
}

void DruidAI::UpdateDamageOverTimeManagement()
{
    RefreshExpiringDoTs();
}

void DruidAI::RefreshExpiringDoTs()
{
    for (auto it = _moonfireTimers.begin(); it != _moonfireTimers.end(); ++it)
    {
        if (it->second <= DOT_REFRESH_THRESHOLD)
        {
            if (Unit* unit = ObjectAccessor::GetUnit(*_bot, it->first))
            {
                if (CanUseAbility(MOONFIRE))
                    CastMoonfire(unit);
            }
        }
    }
}

bool DruidAI::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    return !target->HasAura(spellId) || target->GetRemainingTimeOnAura(spellId) < DOT_REFRESH_THRESHOLD;
}

::Unit* DruidAI::GetBestHealTarget()
{
    ::Unit* lowestTarget = nullptr;
    float lowestHealth = 100.0f;

    // Check self
    if (_bot->GetHealthPct() < lowestHealth)
    {
        lowestTarget = _bot;
        lowestHealth = _bot->GetHealthPct();
    }

    // Check group members
    if (Group* group = _bot->GetGroup())
    {
        for (auto const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(*_bot, member.guid))
            {
                if (player->GetHealthPct() < lowestHealth && player->GetDistance(_bot) <= OPTIMAL_HEALING_RANGE)
                {
                    lowestTarget = player;
                    lowestHealth = player->GetHealthPct();
                }
            }
        }
    }

    return lowestTarget;
}

std::vector<::Unit*> DruidAI::GetAoETargets()
{
    std::vector<::Unit*> targets;

    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(_bot, _bot, 8.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, check);
    Cell::VisitAllObjects(_bot, searcher, 8.0f);

    for (Unit* enemy : nearbyEnemies)
    {
        targets.push_back(enemy);
    }

    return targets;
}

uint32 DruidAI::GetMana()
{
    return _bot ? _bot->GetPower(POWER_MANA) : 0;
}

uint32 DruidAI::GetMaxMana()
{
    return _bot ? _bot->GetMaxPower(POWER_MANA) : 0;
}

float DruidAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float)GetMana() / maxMana : 0.0f;
}

bool DruidAI::HasEnoughMana(uint32 amount)
{
    return GetMana() >= amount;
}

DruidSpec DruidAI::DetectSpecialization()
{
    if (!_bot)
        return DruidSpec::BALANCE;

    // Simple detection based on known spells
    if (_bot->HasSpell(MANGLE_BEAR) || _bot->HasSpell(SURVIVAL_INSTINCTS))
        return DruidSpec::GUARDIAN;

    if (_bot->HasSpell(SHRED) || _bot->HasSpell(SAVAGE_ROAR))
        return DruidSpec::FERAL;

    if (_bot->HasSpell(LIFEBLOOM) || _bot->HasSpell(SWIFTMEND))
        return DruidSpec::RESTORATION;

    return DruidSpec::BALANCE;
}

uint32 DruidAI::GetShapeshiftSpellId(DruidForm form)
{
    switch (form)
    {
        case DruidForm::BEAR: return BEAR_FORM;
        case DruidForm::CAT: return CAT_FORM;
        case DruidForm::AQUATIC: return AQUATIC_FORM;
        case DruidForm::TRAVEL: return TRAVEL_FORM;
        case DruidForm::MOONKIN: return MOONKIN_FORM;
        case DruidForm::TREE_OF_LIFE: return TREE_OF_LIFE;
        case DruidForm::FLIGHT: return FLIGHT_FORM;
        default: return 0;
    }
}

bool DruidAI::RequiresHumanoidForm(uint32 spellId)
{
    // Most healing and utility spells require humanoid form
    switch (spellId)
    {
        case HEALING_TOUCH:
        case REGROWTH:
        case REJUVENATION:
        case LIFEBLOOM:
        case SWIFTMEND:
        case TRANQUILITY:
        case INNERVATE:
        case ENTANGLING_ROOTS:
        case CYCLONE:
        case HIBERNATE:
        case REMOVE_CURSE:
            return true;
        default:
            return false;
    }
}

// Combat ability implementations
void DruidAI::CastStarfire(::Unit* target)
{
    if (!target || !CanUseAbility(STARFIRE))
        return;

    _bot->CastSpell(target, STARFIRE, false);
    _lunarEnergy = std::min(_lunarEnergy + 20, ECLIPSE_ENERGY_MAX);
    _starfireCount++;
    ConsumeResource(STARFIRE);
}

void DruidAI::CastWrath(::Unit* target)
{
    if (!target || !CanUseAbility(WRATH))
        return;

    _bot->CastSpell(target, WRATH, false);
    _solarEnergy = std::min(_solarEnergy + 15, ECLIPSE_ENERGY_MAX);
    _wrathCount++;
    ConsumeResource(WRATH);
}

void DruidAI::CastMoonfire(::Unit* target)
{
    if (!target || !CanUseAbility(MOONFIRE))
        return;

    _bot->CastSpell(target, MOONFIRE, false);
    _moonfireTimers[target->GetGUID()] = 18000; // 18 seconds
    ConsumeResource(MOONFIRE);
}

void DruidAI::CastSunfire(::Unit* target)
{
    if (!target || !CanUseAbility(SUNFIRE))
        return;

    _bot->CastSpell(target, SUNFIRE, false);
    _sunfireTimers[target->GetGUID()] = 18000; // 18 seconds
    ConsumeResource(SUNFIRE);
}

void DruidAI::CastStarsurge(::Unit* target)
{
    if (!target || !CanUseAbility(STARSURGE))
        return;

    _bot->CastSpell(target, STARSURGE, false);
    ConsumeResource(STARSURGE);
}

void DruidAI::EnterMoonkinForm()
{
    if (!CanUseAbility(MOONKIN_FORM))
        return;

    ShiftToForm(DruidForm::MOONKIN);
}

void DruidAI::CastShred(::Unit* target)
{
    if (!target || !CanUseAbility(SHRED))
        return;

    _bot->CastSpell(target, SHRED, false);
    GenerateComboPoint(target);
    ConsumeResource(SHRED);
}

void DruidAI::CastMangle(::Unit* target)
{
    if (!target || !CanUseAbility(MANGLE_CAT))
        return;

    _bot->CastSpell(target, MANGLE_CAT, false);
    GenerateComboPoint(target);
    ConsumeResource(MANGLE_CAT);
}

void DruidAI::CastRake(::Unit* target)
{
    if (!target || !CanUseAbility(RAKE))
        return;

    _bot->CastSpell(target, RAKE, false);
    GenerateComboPoint(target);
    ConsumeResource(RAKE);
}

void DruidAI::CastRip(::Unit* target)
{
    if (!target || !CanUseAbility(RIP))
        return;

    _bot->CastSpell(target, RIP, false);
    _comboPoints.SpendComboPoints();
    _ripRemaining = 22000; // 22 seconds
    ConsumeResource(RIP);
}

void DruidAI::CastFerociosBite(::Unit* target)
{
    if (!target || !CanUseAbility(FEROCIOUS_BITE))
        return;

    _bot->CastSpell(target, FEROCIOUS_BITE, false);
    _comboPoints.SpendComboPoints();
    ConsumeResource(FEROCIOUS_BITE);
}

void DruidAI::CastSavageRoar()
{
    if (!CanUseAbility(SAVAGE_ROAR))
        return;

    _bot->CastSpell(_bot, SAVAGE_ROAR, false);
    _comboPoints.SpendComboPoints();
    _savageRoarRemaining = 42000; // 42 seconds
    ConsumeResource(SAVAGE_ROAR);
}

void DruidAI::CastTigersFury()
{
    if (!CanUseAbility(TIGERS_FURY))
        return;

    _bot->CastSpell(_bot, TIGERS_FURY, false);
    _energy = std::min(_energy + 60, _maxEnergy);
    ConsumeResource(TIGERS_FURY);
}

void DruidAI::EnterCatForm()
{
    if (!CanUseAbility(CAT_FORM))
        return;

    ShiftToForm(DruidForm::CAT);
}

void DruidAI::CastMaul(::Unit* target)
{
    if (!target || !CanUseAbility(MAUL))
        return;

    _bot->CastSpell(target, MAUL, false);
    ConsumeResource(MAUL);
}

void DruidAI::CastThrash()
{
    if (!CanUseAbility(THRASH))
        return;

    _bot->CastSpell(_bot, THRASH, false);
    _thrashStacks = std::min(_thrashStacks + 1, 3u);
    ConsumeResource(THRASH);
}

void DruidAI::CastSwipe()
{
    if (!CanUseAbility(SWIPE))
        return;

    _bot->CastSpell(_bot, SWIPE, false);
    ConsumeResource(SWIPE);
}

void DruidAI::CastLacerate(::Unit* target)
{
    if (!target || !CanUseAbility(LACERATE))
        return;

    _bot->CastSpell(target, LACERATE, false);
    _lacerateStacks = std::min(_lacerateStacks + 1, 5u);
    ConsumeResource(LACERATE);
}

void DruidAI::CastFrenziedRegeneration()
{
    if (!CanUseAbility(FRENZIED_REGENERATION))
        return;

    _bot->CastSpell(_bot, FRENZIED_REGENERATION, false);
    ConsumeResource(FRENZIED_REGENERATION);
}

void DruidAI::CastSurvivalInstincts()
{
    if (!CanUseAbility(SURVIVAL_INSTINCTS))
        return;

    _bot->CastSpell(_bot, SURVIVAL_INSTINCTS, false);
    ConsumeResource(SURVIVAL_INSTINCTS);
}

void DruidAI::EnterBearForm()
{
    if (!CanUseAbility(BEAR_FORM))
        return;

    ShiftToForm(DruidForm::BEAR);
}

void DruidAI::UseRestorationAbilities()
{
    ::Unit* healTarget = GetBestHealTarget();
    if (!healTarget)
        return;

    float healthPercent = healTarget->GetHealthPct();

    // Emergency healing
    if (healthPercent < 30.0f)
    {
        if (CanUseAbility(HEALING_TOUCH))
            CastHealingTouch(healTarget);
        else if (CanUseAbility(REGROWTH))
            CastRegrowth(healTarget);
    }
    // Moderate healing
    else if (healthPercent < 70.0f)
    {
        if (ShouldApplyHoT(healTarget, REJUVENATION) && CanUseAbility(REJUVENATION))
            CastRejuvenation(healTarget);
        else if (CanUseAbility(REGROWTH))
            CastRegrowth(healTarget);
    }
    // Maintenance healing
    else if (healthPercent < 90.0f)
    {
        if (ShouldApplyHoT(healTarget, REJUVENATION) && CanUseAbility(REJUVENATION))
            CastRejuvenation(healTarget);
    }
}

void DruidAI::CastHealingTouch(::Unit* target)
{
    if (!target || !CanUseAbility(HEALING_TOUCH))
        return;

    _bot->CastSpell(target, HEALING_TOUCH, false);
    ConsumeResource(HEALING_TOUCH);
}

void DruidAI::CastRegrowth(::Unit* target)
{
    if (!target || !CanUseAbility(REGROWTH))
        return;

    _bot->CastSpell(target, REGROWTH, false);
    _regrowthTimers[target->GetGUID()] = 21000; // 21 seconds
    ConsumeResource(REGROWTH);
}

void DruidAI::CastRejuvenation(::Unit* target)
{
    if (!target || !CanUseAbility(REJUVENATION))
        return;

    _bot->CastSpell(target, REJUVENATION, false);
    _rejuvenationTimers[target->GetGUID()] = 15000; // 15 seconds
    ConsumeResource(REJUVENATION);
}

void DruidAI::CastLifebloom(::Unit* target)
{
    if (!target || !CanUseAbility(LIFEBLOOM))
        return;

    _bot->CastSpell(target, LIFEBLOOM, false);
    _lifebloomTimers[target->GetGUID()] = 10000; // 10 seconds
    ConsumeResource(LIFEBLOOM);
}

void DruidAI::CastSwiftmend(::Unit* target)
{
    if (!target || !CanUseAbility(SWIFTMEND))
        return;

    _bot->CastSpell(target, SWIFTMEND, false);
    ConsumeResource(SWIFTMEND);
}

void DruidAI::RecordDamageDealt(uint32 damage, ::Unit* target)
{
    _damageDealt += damage;
}

void DruidAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
}

// Utility class implementations
uint32 DruidCalculator::CalculateStarfireDamage(Player* caster, ::Unit* target)
{
    return 1500; // Placeholder
}

uint32 DruidCalculator::CalculateWrathDamage(Player* caster, ::Unit* target)
{
    return 1000; // Placeholder
}

uint32 DruidCalculator::CalculateShredDamage(Player* caster, ::Unit* target)
{
    return 1200; // Placeholder
}

uint32 DruidCalculator::CalculateHealingTouchAmount(Player* caster, ::Unit* target)
{
    return 2000; // Placeholder
}

uint32 DruidCalculator::CalculateRegrowthAmount(Player* caster, ::Unit* target)
{
    return 1800; // Placeholder
}

uint32 DruidCalculator::CalculateRejuvenationTick(Player* caster, ::Unit* target)
{
    return 300; // Placeholder
}

EclipseState DruidCalculator::CalculateNextEclipseState(uint32 solarEnergy, uint32 lunarEnergy)
{
    if (solarEnergy >= 100)
        return EclipseState::SOLAR;
    if (lunarEnergy >= 100)
        return EclipseState::LUNAR;
    return EclipseState::NONE;
}

uint32 DruidCalculator::CalculateEclipseDamageBonus(EclipseState state, uint32 spellId)
{
    return 25; // 25% bonus placeholder
}

DruidForm DruidCalculator::GetOptimalFormForSituation(DruidSpec spec, bool inCombat, bool needsHealing)
{
    if (needsHealing)
        return DruidForm::HUMANOID;

    if (!inCombat)
        return DruidForm::TRAVEL;

    switch (spec)
    {
        case DruidSpec::BALANCE: return DruidForm::MOONKIN;
        case DruidSpec::FERAL: return DruidForm::CAT;
        case DruidSpec::GUARDIAN: return DruidForm::BEAR;
        case DruidSpec::RESTORATION: return DruidForm::HUMANOID;
    }

    return DruidForm::HUMANOID;
}

bool DruidCalculator::ShouldShiftToForm(DruidForm current, DruidForm desired, Player* caster)
{
    return current != desired;
}

uint32 DruidCalculator::CalculateFormShiftCost(DruidForm fromForm, DruidForm toForm)
{
    return 0; // No cost in modern WoW
}

float DruidCalculator::CalculateHoTEfficiency(uint32 spellId, Player* caster, ::Unit* target)
{
    return 1.0f; // Placeholder
}

float DruidCalculator::CalculateDoTEfficiency(uint32 spellId, Player* caster, ::Unit* target)
{
    return 1.0f; // Placeholder
}

bool DruidCalculator::ShouldRefreshHoT(uint32 spellId, ::Unit* target, uint32 remainingTime)
{
    return remainingTime < 6000; // Refresh with 6 seconds left
}

void DruidCalculator::CacheDruidData()
{
    // Cache implementation
}

// DruidFormManager implementation
DruidFormManager::DruidFormManager(DruidAI* owner) : _owner(owner), _currentForm(DruidForm::HUMANOID),
                                                     _previousForm(DruidForm::HUMANOID), _requestedForm(DruidForm::HUMANOID),
                                                     _isShifting(false), _lastShift(0), _shiftCooldown(1500)
{
}

void DruidFormManager::Update(uint32 diff)
{
    if (_lastShift > 0)
    {
        _lastShift = _lastShift > diff ? _lastShift - diff : 0;
    }

    if (_requestedForm != _currentForm && !_isShifting && _lastShift == 0)
    {
        if (CanShiftToForm(_requestedForm))
        {
            ForceFormShift(_requestedForm);
        }
    }
}

void DruidFormManager::RequestFormShift(DruidForm targetForm)
{
    _requestedForm = targetForm;
}

bool DruidFormManager::CanShiftToForm(DruidForm form) const
{
    return _lastShift == 0 && _currentForm != form;
}

void DruidFormManager::ForceFormShift(DruidForm form)
{
    _previousForm = _currentForm;
    _currentForm = form;
    _isShifting = true;
    _lastShift = _shiftCooldown;
}

DruidForm DruidFormManager::GetCurrentForm() const
{
    return _currentForm;
}

DruidForm DruidFormManager::GetPreviousForm() const
{
    return _previousForm;
}

bool DruidFormManager::IsShifting() const
{
    return _isShifting;
}

uint32 DruidFormManager::GetFormShiftCooldown() const
{
    return _lastShift;
}

DruidForm DruidFormManager::GetOptimalForm() const
{
    return DruidForm::HUMANOID; // Placeholder
}

void DruidFormManager::OptimizeFormForSituation()
{
    // Optimize form selection
}

void DruidFormManager::AdaptToSituation(bool inCombat, bool needsHealing, bool needsTravel)
{
    if (needsHealing)
        RequestFormShift(DruidForm::HUMANOID);
    else if (needsTravel && !inCombat)
        RequestFormShift(DruidForm::TRAVEL);
    else if (inCombat)
        RequestFormShift(GetCombatForm());
}

DruidForm DruidFormManager::GetCombatForm() const
{
    return DruidForm::CAT; // Placeholder
}

DruidForm DruidFormManager::GetHealingForm() const
{
    return DruidForm::HUMANOID;
}

DruidForm DruidFormManager::GetTravelForm() const
{
    return DruidForm::TRAVEL;
}

// EclipseController implementation
EclipseController::EclipseController(DruidAI* owner) : _owner(owner), _currentState(EclipseState::NONE),
                                                       _solarEnergy(0), _lunarEnergy(0), _lastEclipseUpdate(0)
{
}

void EclipseController::Update(uint32 diff)
{
    _lastEclipseUpdate += diff;
    if (_lastEclipseUpdate >= 1000) // Update every second
    {
        UpdateEclipseEnergy();
        _lastEclipseUpdate = 0;
    }
}

void EclipseController::CastEclipseSpell(::Unit* target)
{
    if (ShouldCastStarfire())
    {
        // Cast Starfire to build Lunar energy
        _lunarEnergy = std::min(_lunarEnergy + 20, 100u);
    }
    else if (ShouldCastWrath())
    {
        // Cast Wrath to build Solar energy
        _solarEnergy = std::min(_solarEnergy + 15, 100u);
    }
}

EclipseState EclipseController::GetCurrentState() const
{
    return _currentState;
}

uint32 EclipseController::GetSolarEnergy() const
{
    return _solarEnergy;
}

uint32 EclipseController::GetLunarEnergy() const
{
    return _lunarEnergy;
}

bool EclipseController::ShouldCastStarfire() const
{
    return _currentState == EclipseState::SOLAR || _lunarEnergy < _solarEnergy;
}

bool EclipseController::ShouldCastWrath() const
{
    return _currentState == EclipseState::LUNAR || _solarEnergy < _lunarEnergy;
}

void EclipseController::OptimizeEclipseRotation(::Unit* target)
{
    CastEclipseSpell(target);
}

void EclipseController::UpdateEclipseEnergy()
{
    // Natural energy decay
    if (_solarEnergy > 0)
        _solarEnergy = std::max(0u, _solarEnergy - 1);
    if (_lunarEnergy > 0)
        _lunarEnergy = std::max(0u, _lunarEnergy - 1);

    // Update eclipse state
    if (_solarEnergy >= 100)
        _currentState = EclipseState::SOLAR;
    else if (_lunarEnergy >= 100)
        _currentState = EclipseState::LUNAR;
    else
        _currentState = EclipseState::NONE;
}

void EclipseController::AdvanceToNextEclipse()
{
    // Eclipse advancement logic
}

uint32 EclipseController::CalculateSpellEclipseValue(uint32 spellId)
{
    switch (spellId)
    {
        case DruidAI::STARFIRE: return 20;
        case DruidAI::WRATH: return 15;
        default: return 0;
    }
}

// Cache static variables
std::unordered_map<uint32, uint32> DruidCalculator::_damageCache;
std::unordered_map<uint32, uint32> DruidCalculator::_healingCache;
std::unordered_map<DruidForm, uint32> DruidCalculator::_formEfficiencyCache;
std::mutex DruidCalculator::_cacheMutex;

} // namespace Playerbot