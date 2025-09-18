/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MonkAI.h"
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

MonkAI::MonkAI(Player* bot) : ClassAI(bot)
{
    _specialization = DetectSpecialization();
    _damageDealt = 0;
    _healingDone = 0;
    _damageMitigated = 0;
    _chiGenerated = 0;
    _energySpent = 0;

    // Initialize resource tracking
    _chi = ChiInfo();
    _energy = 100;
    _maxEnergy = 100;
    _mana = 0;
    _maxMana = 0;
    _lastEnergyRegen = 0;
    _lastChiGeneration = 0;

    // Initialize Brewmaster systems
    _stagger = StaggerInfo();
    _lastStaggerClear = 0;
    _staggerCheckInterval = STAGGER_CHECK_INTERVAL;
    _needsStaggerManagement = false;
    _ironskinBrewCharges = BREW_CHARGES_MAX;
    _purifyingBrewCharges = BREW_CHARGES_MAX;

    // Initialize Mistweaver systems
    _lastHealingScan = 0;
    _fistweavingMode = false;
    _soothinMistChanneling = 0;

    // Initialize Windwalker systems
    _comboPower = 0;
    _tigerPalmStacks = 0;
    _lastComboSpender = 0;
    _markOfTheCraneStacks = 0;
    _stormEarthAndFireActive = false;
    _touchOfDeathReady = 0;

    // Initialize utility tracking
    _lastExpelHarm = 0;
    _lastFortifyingBrew = 0;
    _lastZenMeditation = 0;
    _lastTranscendence = 0;
    _lastRoll = 0;
    _lastTeleport = 0;
    _lastParalysis = 0;
    _lastSpearHandStrike = 0;
    _inTranscendence = false;
    _transcendencePosition = Position();
}

void MonkAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    UpdateChiManagement();
    UpdateEnergyManagement();

    switch (_specialization)
    {
        case MonkSpec::BREWMASTER:
            UpdateBrewmasterRotation(target);
            UpdateStaggerManagement();
            break;
        case MonkSpec::MISTWEAVER:
            UpdateMistweaverRotation(target);
            UpdateMistweaverHealing();
            break;
        case MonkSpec::WINDWALKER:
            UpdateWindwalkerRotation(target);
            UpdateComboSystem();
            break;
    }
}

void MonkAI::UpdateBuffs()
{
    if (!_bot)
        return;

    UpdateMonkBuffs();
    RefreshSelfBuffs();

    if (_specialization == MonkSpec::BREWMASTER)
        ManageActiveDefensives();
}

void MonkAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);

    // Update energy regeneration
    _lastEnergyRegen += diff;
    if (_lastEnergyRegen >= 1000) // Regen every second
    {
        _energy = std::min(_energy + ENERGY_REGEN_RATE / 10, _maxEnergy);
        _lastEnergyRegen = 0;
    }

    // Update chi generation
    _lastChiGeneration += diff;
    if (_lastChiGeneration >= CHI_GENERATION_INTERVAL)
    {
        if (_chi.isRegenerating && _chi.current < _chi.maximum)
        {
            GenerateChi(1);
        }
        _lastChiGeneration = 0;
    }

    // Update stagger timer
    if (_stagger.remainingTime > 0)
    {
        if (_stagger.remainingTime <= diff)
            _stagger.remainingTime = 0;
        else
            _stagger.remainingTime -= diff;
    }

    // Update brew charges
    if (_ironskinBrewCharges < BREW_CHARGES_MAX || _purifyingBrewCharges < BREW_CHARGES_MAX)
    {
        // Recharge brews every 20 seconds
        static uint32 lastBrewRecharge = 0;
        lastBrewRecharge += diff;
        if (lastBrewRecharge >= 20000)
        {
            if (_ironskinBrewCharges < BREW_CHARGES_MAX)
                _ironskinBrewCharges++;
            if (_purifyingBrewCharges < BREW_CHARGES_MAX)
                _purifyingBrewCharges++;
            lastBrewRecharge = 0;
        }
    }
}

bool MonkAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check chi requirements
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (spellInfo && spellInfo->PowerType == POWER_CHI)
    {
        return HasChi(spellInfo->ManaCost);
    }

    return true;
}

void MonkAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    if (_specialization == MonkSpec::BREWMASTER)
    {
        UseBrewmasterDefensives();
    }
    else if (_specialization == MonkSpec::MISTWEAVER)
    {
        _fistweavingMode = ShouldSwitchToFistweaving();
    }
}

void MonkAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    _comboPower = 0;
    _tigerPalmStacks = 0;
    _markOfTheCraneStacks = 0;
    _stormEarthAndFireActive = false;
    _fistweavingMode = false;
}

bool MonkAI::HasEnoughResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    switch (spellInfo->PowerType)
    {
        case POWER_CHI:
            return HasChi(spellInfo->ManaCost);
        case POWER_ENERGY:
            return HasEnergy(spellInfo->ManaCost);
        case POWER_MANA:
            return _mana >= spellInfo->ManaCost;
        default:
            return true;
    }
}

void MonkAI::ConsumeResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    switch (spellInfo->PowerType)
    {
        case POWER_CHI:
            SpendChi(spellInfo->ManaCost);
            break;
        case POWER_ENERGY:
            SpendEnergy(spellInfo->ManaCost);
            break;
        case POWER_MANA:
            _mana = _mana >= spellInfo->ManaCost ? _mana - spellInfo->ManaCost : 0;
            break;
    }
}

Position MonkAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    Position pos = _bot->GetPosition();
    float distance = _bot->GetDistance(target);

    if (_specialization == MonkSpec::MISTWEAVER && !_fistweavingMode)
    {
        // Healing range for pure healing
        if (distance > OPTIMAL_HEAL_RANGE || distance < OPTIMAL_HEAL_RANGE * 0.8f)
        {
            pos = target->GetPosition();
            pos.m_positionX += OPTIMAL_HEAL_RANGE * 0.9f * cos(target->GetOrientation() + M_PI);
            pos.m_positionY += OPTIMAL_HEAL_RANGE * 0.9f * sin(target->GetOrientation() + M_PI);
        }
    }
    else
    {
        // Melee range for Brewmaster, Windwalker, and Fistweaving
        if (distance > MELEE_RANGE)
        {
            pos = target->GetPosition();
            pos.m_positionX += MELEE_RANGE * cos(target->GetOrientation());
            pos.m_positionY += MELEE_RANGE * sin(target->GetOrientation());
        }
    }

    return pos;
}

float MonkAI::GetOptimalRange(::Unit* target)
{
    if (_specialization == MonkSpec::MISTWEAVER && !_fistweavingMode)
        return OPTIMAL_HEAL_RANGE;

    return MELEE_RANGE;
}

void MonkAI::UpdateBrewmasterRotation(::Unit* target)
{
    if (!target)
        return;

    // Maintain aggro with Keg Smash
    if (CanUseAbility(KEG_SMASH))
    {
        CastKegSmash(target);
        return;
    }

    // Tiger Palm for chi generation
    if (_chi.current < 2 && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        return;
    }

    // Breath of Fire for AoE
    if (GetAoETargets().size() >= 3 && CanUseAbility(BREATH_OF_FIRE))
    {
        CastBreathOfFire();
        return;
    }

    // Blackout Kick for damage
    if (HasChi(1) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
    }
}

void MonkAI::UpdateMistweaverRotation(::Unit* target)
{
    if (!target)
        return;

    if (_fistweavingMode)
    {
        ExecuteFistweaving(target);
    }
    else
    {
        ExecuteHealingRotation();
    }

    // Toggle between fistweaving and pure healing based on group health
    if (getMSTime() - _lastHealingScan > 2000)
    {
        ToggleFistweavingMode();
        _lastHealingScan = getMSTime();
    }
}

void MonkAI::UpdateWindwalkerRotation(::Unit* target)
{
    if (!target)
        return;

    // Touch of Death on low health targets
    if (ShouldUseTouchOfDeath(target) && CanUseAbility(TOUCH_OF_DEATH))
    {
        CastTouchOfDeath(target);
        return;
    }

    // Rising Sun Kick on cooldown
    if (CanUseAbility(RISING_SUN_KICK))
    {
        CastRisingSunKick(target);
        return;
    }

    // Fists of Fury for AoE or high damage
    if (HasChi(3) && CanUseAbility(FISTS_OF_FURY))
    {
        CastFistsOfFury(target);
        return;
    }

    // Tiger Palm for chi and debuff
    if (_chi.current < 2 && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        return;
    }

    // Blackout Kick for combo points
    if (HasChi(1) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
    }
}

void MonkAI::UpdateChiManagement()
{
    // Natural chi regeneration is handled in UpdateCooldowns

    // Optimize chi usage based on specialization
    if (_specialization == MonkSpec::WINDWALKER)
    {
        // Windwalkers should maintain 2-3 chi for combos
        if (_chi.current > 3)
        {
            // Spend excess chi
            SpendCombo();
        }
    }
    else if (_specialization == MonkSpec::BREWMASTER)
    {
        // Brewmasters should conserve chi for defensive abilities
        if (_chi.current < 2 && _bot->IsInCombat())
        {
            // Generate chi with Tiger Palm
            if (CanUseAbility(TIGER_PALM))
            {
                ::Unit* target = GetTarget();
                if (target)
                    CastTigerPalm(target);
            }
        }
    }
}

void MonkAI::UpdateEnergyManagement()
{
    // Energy regeneration is automatic, managed in UpdateCooldowns

    // Ensure we don't waste energy by capping
    if (_energy >= _maxEnergy * 0.9f)
    {
        // Use energy abilities to avoid waste
        if (CanUseAbility(TIGER_PALM))
        {
            ::Unit* target = GetTarget();
            if (target)
                CastTigerPalm(target);
        }
    }
}

void MonkAI::UpdateStaggerManagement()
{
    if (_specialization != MonkSpec::BREWMASTER)
        return;

    uint32 now = getMSTime();
    if (now - _lastStaggerClear < _staggerCheckInterval)
        return;

    _lastStaggerClear = now;

    // Check stagger level and clear if necessary
    if (ShouldUsePurifyingBrew())
    {
        CastPurifyingBrew();
    }

    // Use Ironskin Brew proactively
    if (ShouldUseIronskinBrew())
    {
        CastIronskinBrew();
    }
}

void MonkAI::GenerateChi(uint32 amount)
{
    _chi.current = std::min(_chi.current + amount, _chi.maximum);
    _chiGenerated += amount;
}

void MonkAI::SpendChi(uint32 amount)
{
    _chi.current = _chi.current >= amount ? _chi.current - amount : 0;
}

bool MonkAI::HasChi(uint32 required)
{
    return _chi.current >= required;
}

uint32 MonkAI::GetChi()
{
    return _chi.current;
}

uint32 MonkAI::GetMaxChi()
{
    return _chi.maximum;
}

bool MonkAI::HasEnergy(uint32 required)
{
    return _energy >= required;
}

void MonkAI::SpendEnergy(uint32 amount)
{
    _energy = _energy >= amount ? _energy - amount : 0;
    _energySpent += amount;
}

uint32 MonkAI::GetEnergy()
{
    return _energy;
}

uint32 MonkAI::GetMaxEnergy()
{
    return _maxEnergy;
}

float MonkAI::GetEnergyPercent()
{
    return _maxEnergy > 0 ? (float)_energy / _maxEnergy : 0.0f;
}

bool MonkAI::ShouldUsePurifyingBrew()
{
    if (_purifyingBrewCharges == 0)
        return false;

    // Use on heavy stagger
    if (_stagger.isHeavy)
        return true;

    // Use on moderate stagger if low on health
    if (_stagger.isModerate && _bot->GetHealthPct() < 60.0f)
        return true;

    return false;
}

bool MonkAI::ShouldUseIronskinBrew()
{
    if (_ironskinBrewCharges == 0)
        return false;

    // Use if not active and in combat
    if (_bot->IsInCombat() && !_bot->HasAura(IRONSKIN_BREW))
        return true;

    // Refresh if about to expire
    if (_bot->HasAura(IRONSKIN_BREW) && _bot->GetRemainingTimeOnAura(IRONSKIN_BREW) < 3000)
        return true;

    return false;
}

void MonkAI::UpdateMistweaverHealing()
{
    if (_specialization != MonkSpec::MISTWEAVER)
        return;

    ScanForHealTargets();

    if (!_healingTargets.empty())
    {
        if (_fistweavingMode)
        {
            ::Unit* target = GetBestFistweavingTarget();
            if (target)
                ExecuteFistweaving(target);
        }
        else
        {
            ExecuteHealingRotation();
        }
    }
}

void MonkAI::ScanForHealTargets()
{
    // Clear old targets
    while (!_healingTargets.empty())
        _healingTargets.pop();

    // Check self
    if (_bot->GetHealthPct() < 90.0f)
    {
        MistweaverTarget selfTarget(_bot, _bot->GetHealthPct(), _bot->GetMaxHealth() - _bot->GetHealth());
        selfTarget.priority = _bot->GetHealthPct() < 50.0f ? 1 : 2;
        _healingTargets.push(selfTarget);
    }

    // Check group members
    if (Group* group = _bot->GetGroup())
    {
        for (auto const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(*_bot, member.guid))
            {
                if (player->GetHealthPct() < 90.0f && player->GetDistance(_bot) <= OPTIMAL_HEAL_RANGE)
                {
                    MistweaverTarget target(player, player->GetHealthPct(), player->GetMaxHealth() - player->GetHealth());
                    target.priority = player->GetHealthPct() < 30.0f ? 1 : (player->GetHealthPct() < 60.0f ? 2 : 3);
                    _healingTargets.push(target);
                }
            }
        }
    }
}

void MonkAI::ExecuteFistweaving(::Unit* target)
{
    if (!target)
        return;

    // Use chi-generating abilities for healing through Teachings of the Monastery
    if (CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
    }
    else if (HasChi(1) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
    }
}

void MonkAI::ExecuteHealingRotation()
{
    if (_healingTargets.empty())
        return;

    MistweaverTarget healTarget = _healingTargets.top();
    _healingTargets.pop();

    if (!healTarget.target)
        return;

    float healthPercent = healTarget.healthPercent;

    // Emergency healing
    if (healthPercent < 30.0f && CanUseAbility(VIVIFY))
    {
        CastVivify(healTarget.target);
    }
    // Moderate healing
    else if (healthPercent < 70.0f && CanUseAbility(ENVELOPING_MIST))
    {
        CastEnvelopingMist(healTarget.target);
    }
    // Maintenance healing
    else if (CanUseAbility(RENEWING_MIST))
    {
        CastRenewingMist(healTarget.target);
    }
}

::Unit* MonkAI::GetBestHealTarget()
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
                if (player->GetHealthPct() < lowestHealth && player->GetDistance(_bot) <= OPTIMAL_HEAL_RANGE)
                {
                    lowestTarget = player;
                    lowestHealth = player->GetHealthPct();
                }
            }
        }
    }

    return lowestTarget;
}

::Unit* MonkAI::GetBestFistweavingTarget()
{
    // Find the best enemy target for fistweaving
    ::Unit* target = GetTarget();
    if (target && IsInMeleeRange(target))
        return target;

    return nullptr;
}

void MonkAI::ToggleFistweavingMode()
{
    // Calculate group average health
    float totalHealth = 0.0f;
    uint32 memberCount = 0;

    if (Group* group = _bot->GetGroup())
    {
        for (auto const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(*_bot, member.guid))
            {
                totalHealth += player->GetHealthPct();
                memberCount++;
            }
        }
    }
    else
    {
        totalHealth = _bot->GetHealthPct();
        memberCount = 1;
    }

    float avgHealth = memberCount > 0 ? totalHealth / memberCount : 100.0f;

    // Switch to fistweaving if group health is high
    if (avgHealth > FISTWEAVING_HEAL_THRESHOLD && !_fistweavingMode)
    {
        _fistweavingMode = true;
    }
    // Switch to direct healing if group health is low
    else if (avgHealth < FISTWEAVING_HEAL_THRESHOLD * 0.8f && _fistweavingMode)
    {
        _fistweavingMode = false;
    }
}

bool MonkAI::ShouldSwitchToFistweaving()
{
    return GetBestHealTarget() == nullptr ||
           (_bot->GetGroup() && GetBestHealTarget()->GetHealthPct() > FISTWEAVING_HEAL_THRESHOLD);
}

void MonkAI::UpdateComboSystem()
{
    if (_specialization != MonkSpec::WINDWALKER)
        return;

    // Build combo power with generators
    if (_comboPower < 5 && _chi.current < 2)
    {
        BuildCombo();
    }
    // Spend combo power with finishers
    else if (_comboPower >= 3)
    {
        SpendCombo();
    }
}

void MonkAI::BuildCombo()
{
    ::Unit* target = GetTarget();
    if (!target)
        return;

    if (CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        _comboPower++;
    }
}

void MonkAI::SpendCombo()
{
    ::Unit* target = GetTarget();
    if (!target)
        return;

    if (HasChi(2) && CanUseAbility(RISING_SUN_KICK))
    {
        CastRisingSunKick(target);
        _comboPower = 0;
        _lastComboSpender = getMSTime();
    }
}

bool MonkAI::ShouldUseTouchOfDeath(::Unit* target)
{
    if (!target)
        return false;

    // Use on targets below 10% health
    return target->GetHealthPct() < 10.0f;
}

MonkSpec MonkAI::DetectSpecialization()
{
    if (!_bot)
        return MonkSpec::WINDWALKER;

    // Detect based on known spells
    if (_bot->HasSpell(KEG_SMASH) || _bot->HasSpell(IRONSKIN_BREW))
        return MonkSpec::BREWMASTER;

    if (_bot->HasSpell(VIVIFY) || _bot->HasSpell(ENVELOPING_MIST))
        return MonkSpec::MISTWEAVER;

    return MonkSpec::WINDWALKER;
}

bool MonkAI::IsInMeleeRange(::Unit* target)
{
    return target && _bot->GetDistance(target) <= MELEE_RANGE;
}

std::vector<::Unit*> MonkAI::GetAoETargets()
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

void MonkAI::UpdateMonkBuffs()
{
    // Legacy of the White Tiger for stats
    if (!_bot->HasAura(LEGACY_OF_THE_WHITE_TIGER) && CanUseAbility(LEGACY_OF_THE_WHITE_TIGER))
    {
        CastLegacyOfTheWhiteTiger();
    }
}

void MonkAI::RefreshSelfBuffs()
{
    // Maintain self-buffs based on specialization
    if (_specialization == MonkSpec::BREWMASTER && !_bot->HasAura(IRONSKIN_BREW))
    {
        if (CanUseAbility(IRONSKIN_BREW))
            CastIronskinBrew();
    }
}

void MonkAI::UseBrewmasterDefensives()
{
    if (_bot->GetHealthPct() < 50.0f && CanUseAbility(FORTIFYING_BREW))
    {
        CastFortifyingBrew();
    }
}

void MonkAI::ManageActiveDefensives()
{
    // Maintain defensive abilities for Brewmaster
    if (_bot->IsInCombat() && !_bot->HasAura(IRONSKIN_BREW))
    {
        if (CanUseAbility(IRONSKIN_BREW))
            CastIronskinBrew();
    }
}

// Combat ability implementations
void MonkAI::CastTigerPalm(::Unit* target)
{
    if (!target || !CanUseAbility(TIGER_PALM))
        return;

    _bot->CastSpell(target, TIGER_PALM, false);
    GenerateChi(1);
    ConsumeResource(TIGER_PALM);
}

void MonkAI::CastBlackoutKick(::Unit* target)
{
    if (!target || !CanUseAbility(BLACKOUT_KICK))
        return;

    _bot->CastSpell(target, BLACKOUT_KICK, false);
    ConsumeResource(BLACKOUT_KICK);
}

void MonkAI::CastRisingSunKick(::Unit* target)
{
    if (!target || !CanUseAbility(RISING_SUN_KICK))
        return;

    _bot->CastSpell(target, RISING_SUN_KICK, false);
    ConsumeResource(RISING_SUN_KICK);
}

void MonkAI::CastFistsOfFury(::Unit* target)
{
    if (!target || !CanUseAbility(FISTS_OF_FURY))
        return;

    _bot->CastSpell(target, FISTS_OF_FURY, false);
    ConsumeResource(FISTS_OF_FURY);
}

void MonkAI::CastKegSmash(::Unit* target)
{
    if (!target || !CanUseAbility(KEG_SMASH))
        return;

    _bot->CastSpell(target, KEG_SMASH, false);
    GenerateChi(1);
    ConsumeResource(KEG_SMASH);
}

void MonkAI::CastBreathOfFire()
{
    if (!CanUseAbility(BREATH_OF_FIRE))
        return;

    _bot->CastSpell(_bot, BREATH_OF_FIRE, false);
    ConsumeResource(BREATH_OF_FIRE);
}

void MonkAI::CastTouchOfDeath(::Unit* target)
{
    if (!target || !CanUseAbility(TOUCH_OF_DEATH))
        return;

    _bot->CastSpell(target, TOUCH_OF_DEATH, false);
    ConsumeResource(TOUCH_OF_DEATH);
}

void MonkAI::CastVivify(::Unit* target)
{
    if (!target || !CanUseAbility(VIVIFY))
        return;

    _bot->CastSpell(target, VIVIFY, false);
    ConsumeResource(VIVIFY);
}

void MonkAI::CastEnvelopingMist(::Unit* target)
{
    if (!target || !CanUseAbility(ENVELOPING_MIST))
        return;

    _bot->CastSpell(target, ENVELOPING_MIST, false);
    ConsumeResource(ENVELOPING_MIST);
}

void MonkAI::CastRenewingMist(::Unit* target)
{
    if (!target || !CanUseAbility(RENEWING_MIST))
        return;

    _bot->CastSpell(target, RENEWING_MIST, false);
    ConsumeResource(RENEWING_MIST);
}

void MonkAI::CastIronskinBrew()
{
    if (!CanUseAbility(IRONSKIN_BREW) || _ironskinBrewCharges == 0)
        return;

    _bot->CastSpell(_bot, IRONSKIN_BREW, false);
    _ironskinBrewCharges--;
    ConsumeResource(IRONSKIN_BREW);
}

void MonkAI::CastPurifyingBrew()
{
    if (!CanUseAbility(PURIFYING_BREW) || _purifyingBrewCharges == 0)
        return;

    _bot->CastSpell(_bot, PURIFYING_BREW, false);
    _purifyingBrewCharges--;
    _stagger.totalDamage = 0;
    _stagger.tickDamage = 0;
    _stagger.remainingTime = 0;
    ConsumeResource(PURIFYING_BREW);
}

void MonkAI::CastFortifyingBrew()
{
    if (!CanUseAbility(FORTIFYING_BREW))
        return;

    _bot->CastSpell(_bot, FORTIFYING_BREW, false);
    ConsumeResource(FORTIFYING_BREW);
}

void MonkAI::CastLegacyOfTheWhiteTiger()
{
    if (!CanUseAbility(LEGACY_OF_THE_WHITE_TIGER))
        return;

    _bot->CastSpell(_bot, LEGACY_OF_THE_WHITE_TIGER, false);
    ConsumeResource(LEGACY_OF_THE_WHITE_TIGER);
}

void MonkAI::RecordDamageDealt(uint32 damage, ::Unit* target)
{
    _damageDealt += damage;
}

void MonkAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
}

void MonkAI::RecordDamageMitigated(uint32 amount)
{
    _damageMitigated += amount;
}

// Utility class implementations
uint32 MonkCalculator::CalculateTigerPalmDamage(Player* caster, ::Unit* target)
{
    return 800; // Placeholder
}

uint32 MonkCalculator::CalculateBlackoutKickDamage(Player* caster, ::Unit* target)
{
    return 1200; // Placeholder
}

uint32 MonkCalculator::CalculateRisingSunKickDamage(Player* caster, ::Unit* target)
{
    return 1500; // Placeholder
}

uint32 MonkCalculator::CalculateVivifyHealing(Player* caster, ::Unit* target)
{
    return 2000; // Placeholder
}

uint32 MonkCalculator::CalculateEnvelopingMistHealing(Player* caster, ::Unit* target)
{
    return 1800; // Placeholder
}

uint32 MonkCalculator::CalculateStaggerDamage(Player* monk, uint32 incomingDamage)
{
    return incomingDamage / 2; // 50% staggered
}

uint32 MonkCalculator::GetStaggerTickDamage(Player* monk)
{
    return 500; // Placeholder
}

bool MonkCalculator::ShouldPurifyStagger(Player* monk, uint32 staggerAmount)
{
    return staggerAmount > 1000; // Placeholder
}

float MonkCalculator::CalculateChiEfficiency(uint32 spellId, Player* caster)
{
    return 1.0f; // Placeholder
}

uint32 MonkCalculator::GetOptimalChiSpender(Player* caster, ::Unit* target)
{
    return MonkAI::BLACKOUT_KICK; // Placeholder
}

float MonkCalculator::CalculateFistweavingEfficiency(Player* caster, const std::vector<::Unit*>& healTargets)
{
    return 1.0f; // Placeholder
}

bool MonkCalculator::ShouldFistweave(Player* caster, const std::vector<::Unit*>& allies)
{
    return true; // Placeholder
}

void MonkCalculator::CacheMonkData()
{
    // Cache implementation
}

// BrewManager implementation
BrewManager::BrewManager(MonkAI* owner) : _owner(owner), _ironskinCharges(3), _purifyingCharges(3),
                                          _maxCharges(3), _rechargeTime(20000), _lastRecharge(0)
{
}

void BrewManager::Update(uint32 diff)
{
    _lastRecharge += diff;
    if (_lastRecharge >= _rechargeTime)
    {
        RechargeBrews();
        _lastRecharge = 0;
    }

    UpdateStagger(diff);
}

void BrewManager::RechargeBrews()
{
    if (_ironskinCharges < _maxCharges)
        _ironskinCharges++;
    if (_purifyingCharges < _maxCharges)
        _purifyingCharges++;
}

void BrewManager::UseIronskinBrew()
{
    if (_ironskinCharges > 0)
        _ironskinCharges--;
}

void BrewManager::UsePurifyingBrew()
{
    if (_purifyingCharges > 0)
        _purifyingCharges--;
}

bool BrewManager::ShouldUseIronskinBrew() const
{
    return _ironskinCharges > 0;
}

bool BrewManager::ShouldUsePurifyingBrew() const
{
    return _purifyingCharges > 0 && IsStaggerHeavy();
}

uint32 BrewManager::GetBrewCharges() const
{
    return std::min(_ironskinCharges, _purifyingCharges);
}

bool BrewManager::HasBrewCharges() const
{
    return _ironskinCharges > 0 || _purifyingCharges > 0;
}

void BrewManager::UpdateStagger(uint32 diff)
{
    if (_currentStagger.remainingTime > 0)
    {
        _currentStagger.remainingTime = _currentStagger.remainingTime > diff ?
            _currentStagger.remainingTime - diff : 0;

        _currentStagger.UpdateStaggerLevel();
    }
}

bool BrewManager::IsStaggerHeavy() const
{
    return _currentStagger.isHeavy;
}

bool BrewManager::IsStaggerModerate() const
{
    return _currentStagger.isModerate;
}

uint32 BrewManager::GetStaggerTickDamage() const
{
    return _currentStagger.tickDamage;
}

// FistweavingController implementation
FistweavingController::FistweavingController(MonkAI* owner) : _owner(owner), _fistweavingActive(false),
                                                              _lastToggle(0), _fistweavingHealing(0),
                                                              _directHealing(0), _evaluationPeriod(30000)
{
}

void FistweavingController::Update(uint32 diff)
{
    static uint32 evaluationTimer = 0;
    evaluationTimer += diff;

    if (evaluationTimer >= _evaluationPeriod)
    {
        EvaluateFistweavingEffectiveness();
        evaluationTimer = 0;
    }
}

void FistweavingController::ToggleFistweaving()
{
    _fistweavingActive = !_fistweavingActive;
    _lastToggle = getMSTime();
}

bool FistweavingController::ShouldFistweave() const
{
    return _fistweavingActive && GetFistweavingTarget() != nullptr;
}

bool FistweavingController::IsFistweaving() const
{
    return _fistweavingActive;
}

::Unit* FistweavingController::GetFistweavingTarget() const
{
    return nullptr; // Placeholder
}

::Unit* FistweavingController::GetHealingTarget() const
{
    return nullptr; // Placeholder
}

void FistweavingController::RecordFistweavingHealing(uint32 amount)
{
    _fistweavingHealing += amount;
}

void FistweavingController::RecordDirectHealing(uint32 amount)
{
    _directHealing += amount;
}

float FistweavingController::GetFistweavingEfficiency() const
{
    if (_directHealing == 0)
        return 1.0f;

    return (float)_fistweavingHealing / _directHealing;
}

void FistweavingController::EvaluateFistweavingEffectiveness()
{
    float efficiency = GetFistweavingEfficiency();

    if (efficiency > 1.2f && !_fistweavingActive)
    {
        ToggleFistweaving();
    }
    else if (efficiency < 0.8f && _fistweavingActive)
    {
        ToggleFistweaving();
    }

    // Reset counters
    _fistweavingHealing = 0;
    _directHealing = 0;
}

bool FistweavingController::ShouldSwitchToDirectHealing() const
{
    return GetFistweavingEfficiency() < 0.8f;
}

bool FistweavingController::ShouldSwitchToFistweaving() const
{
    return GetFistweavingEfficiency() > 1.2f;
}

// Cache static variables
std::unordered_map<uint32, uint32> MonkCalculator::_damageCache;
std::unordered_map<uint32, uint32> MonkCalculator::_healingCache;
std::mutex MonkCalculator::_cacheMutex;

} // namespace Playerbot