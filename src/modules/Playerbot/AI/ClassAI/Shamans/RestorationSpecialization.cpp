/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RestorationSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Group.h"
#include "Log.h"

namespace Playerbot
{

RestorationSpecialization::RestorationSpecialization(Player* bot)
    : ShamanSpecialization(bot)
    , _earthShieldCharges(0)
    , _tidalWaveStacks(0)
    , _natureSwiftnessReady(0)
    , _lastNatureSwiftness(0)
    , _lastEarthShield(0)
    , _lastChainHeal(0)
    , _hasWaterShield(false)
    , _hasTidalWave(false)
    , _lastHealCheck(0)
    , _lastEarthShieldCheck(0)
    , _lastRiptideCheck(0)
    , _lastTotemCheck(0)
    , _lastGroupScan(0)
    , _emergencyMode(false)
    , _emergencyStartTime(0)
    , _totalHealingDone(0)
    , _manaSpent(0)
    , _overhealingDone(0)
{
}

void RestorationSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    UpdateHealing();
    UpdateEarthShield();
    UpdateRiptide();

    if (target && target->IsHostileTo(bot))
        UpdateShockRotation(target);
}

void RestorationSpecialization::UpdateBuffs()
{
    UseWaterShield();
    UpdateTotemManagement();
    UpdateEarthShieldManagement();
    ManageMana();
}

void RestorationSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_lastNatureSwiftness > diff)
        _lastNatureSwiftness -= diff;
    else
        _lastNatureSwiftness = 0;

    if (_lastEarthShield > diff)
        _lastEarthShield -= diff;
    else
        _lastEarthShield = 0;

    if (_lastChainHeal > diff)
        _lastChainHeal -= diff;
    else
        _lastChainHeal = 0;
}

bool RestorationSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    switch (spellId)
    {
        case NATURE_SWIFTNESS:
            return _lastNatureSwiftness == 0;
        case CHAIN_HEAL:
            return _lastChainHeal == 0;
    }

    return HasEnoughResource(spellId);
}

void RestorationSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    UseWaterShield();
    DeployOptimalTotems();
    UpdateGroupHealing();
}

void RestorationSpecialization::OnCombatEnd()
{
    _emergencyMode = false;
    _emergencyStartTime = 0;
    _tidalWaveStacks = 0;

    while (!_healQueue.empty())
        _healQueue.pop();
}

bool RestorationSpecialization::HasEnoughResource(uint32 spellId)
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

void RestorationSpecialization::ConsumeResource(uint32 spellId)
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

Position RestorationSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return Position();

    if (Group* group = bot->GetGroup())
    {
        float averageX = 0, averageY = 0, averageZ = 0;
        uint32 count = 0;

        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsInWorld())
            {
                averageX += member->GetPositionX();
                averageY += member->GetPositionY();
                averageZ += member->GetPositionZ();
                count++;
            }
        }

        if (count > 0)
            return Position(averageX / count, averageY / count, averageZ / count, 0);
    }

    return bot->GetPosition();
}

float RestorationSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_HEALING_RANGE;
}

void RestorationSpecialization::UpdateTotemManagement()
{
    uint32 now = getMSTime();
    if (now - _lastTotemCheck < 3000)
        return;
    _lastTotemCheck = now;

    DeployOptimalTotems();
    UpdateCleansingTotems();
}

void RestorationSpecialization::DeployOptimalTotems()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!IsTotemActive(TotemType::FIRE))
        DeployTotem(TotemType::FIRE, GetOptimalFireTotem());

    if (!IsTotemActive(TotemType::EARTH))
        DeployTotem(TotemType::EARTH, GetOptimalEarthTotem());

    if (!IsTotemActive(TotemType::WATER))
        DeployTotem(TotemType::WATER, GetOptimalWaterTotem());

    if (!IsTotemActive(TotemType::AIR))
        DeployTotem(TotemType::AIR, GetOptimalAirTotem());
}

uint32 RestorationSpecialization::GetOptimalFireTotem()
{
    return SEARING_TOTEM;
}

uint32 RestorationSpecialization::GetOptimalEarthTotem()
{
    return STRENGTH_OF_EARTH_TOTEM;
}

uint32 RestorationSpecialization::GetOptimalWaterTotem()
{
    Player* bot = GetBot();
    if (!bot)
        return HEALING_STREAM_TOTEM;

    if (bot->GetPowerPct(POWER_MANA) < 50.0f)
        return MANA_SPRING_TOTEM;

    return HEALING_STREAM_TOTEM;
}

uint32 RestorationSpecialization::GetOptimalAirTotem()
{
    return WRATH_OF_AIR_TOTEM;
}

void RestorationSpecialization::UpdateShockRotation(::Unit* target)
{
    if (!target || IsShockOnCooldown())
        return;

    uint32 nextShock = GetNextShockSpell(target);
    switch (nextShock)
    {
        case EARTH_SHOCK:
            CastEarthShock(target);
            break;
        case FLAME_SHOCK:
            CastFlameShock(target);
            break;
        case FROST_SHOCK:
            CastFrostShock(target);
            break;
    }
}

uint32 RestorationSpecialization::GetNextShockSpell(::Unit* target)
{
    if (!target)
        return 0;

    if (!target->HasAura(FLAME_SHOCK))
        return FLAME_SHOCK;

    return EARTH_SHOCK;
}

void RestorationSpecialization::UpdateHealing()
{
    uint32 now = getMSTime();
    if (now - _lastHealCheck < 500)
        return;
    _lastHealCheck = now;

    UpdateGroupHealing();
    PerformTriage();

    if (IsEmergencyHealing())
    {
        HandleEmergencyHealing();
        return;
    }

    ::Unit* healTarget = GetBestHealTarget();
    if (healTarget)
        HealTarget(healTarget);
}

void RestorationSpecialization::UpdateGroupHealing()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastGroupScan < 2000)
        return;
    _lastGroupScan = now;

    _groupMembers.clear();
    _groupMembers.push_back(bot);

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member != bot && member->IsInWorld() &&
                bot->GetDistance(member) <= OPTIMAL_HEALING_RANGE)
            {
                _groupMembers.push_back(member);
            }
        }
    }
}

::Unit* RestorationSpecialization::GetBestHealTarget()
{
    if (_healQueue.empty())
        return nullptr;

    ShamanHealTarget target = _healQueue.top();
    _healQueue.pop();

    if (target.target && target.target->IsAlive() &&
        target.target->GetHealthPct() < 95.0f)
        return target.target;

    return nullptr;
}

void RestorationSpecialization::HealTarget(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    float healthPct = target->GetHealthPct();

    if (healthPct < EMERGENCY_HEALTH_THRESHOLD)
    {
        if (ShouldUseNatureSwiftness())
        {
            UseNatureSwiftness();
            CastInstantHealingWave(target);
            return;
        }

        if (ShouldCastLesserHealingWave(target))
        {
            bot->CastSpell(target, LESSER_HEALING_WAVE, false);
            ConsumeResource(LESSER_HEALING_WAVE);
            TriggerTidalWave();
        }
    }
    else if (healthPct < LESSER_HEALING_WAVE_THRESHOLD)
    {
        if (ShouldCastRiptide(target) && !HasRiptide(target))
        {
            CastRiptide(target);
            return;
        }

        if (ShouldUseChainHeal())
        {
            CastChainHeal(target);
            return;
        }

        if (ShouldCastLesserHealingWave(target))
        {
            bot->CastSpell(target, LESSER_HEALING_WAVE, false);
            ConsumeResource(LESSER_HEALING_WAVE);
            TriggerTidalWave();
        }
    }
    else if (healthPct < HEALING_WAVE_THRESHOLD)
    {
        if (ShouldCastHealingWave(target))
        {
            bot->CastSpell(target, HEALING_WAVE, false);
            ConsumeResource(HEALING_WAVE);
            TriggerTidalWave();
        }
    }
}

void RestorationSpecialization::PerformTriage()
{
    while (!_healQueue.empty())
        _healQueue.pop();

    for (::Unit* member : _groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        float healthPct = member->GetHealthPct();
        if (healthPct >= 95.0f)
            continue;

        ShamanHealPriority priority = ShamanHealPriority::FULL;
        uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();

        if (healthPct < 20.0f)
            priority = ShamanHealPriority::EMERGENCY;
        else if (healthPct < 40.0f)
            priority = ShamanHealPriority::CRITICAL;
        else if (healthPct < 70.0f)
            priority = ShamanHealPriority::MODERATE;
        else if (healthPct < 90.0f)
            priority = ShamanHealPriority::MAINTENANCE;

        _healQueue.emplace(member, priority, healthPct, missingHealth);
    }
}

bool RestorationSpecialization::ShouldCastHealingWave(::Unit* target)
{
    return target && target->GetHealthPct() < HEALING_WAVE_THRESHOLD &&
           HasEnoughResource(HEALING_WAVE);
}

bool RestorationSpecialization::ShouldCastLesserHealingWave(::Unit* target)
{
    return target && target->GetHealthPct() < LESSER_HEALING_WAVE_THRESHOLD &&
           HasEnoughResource(LESSER_HEALING_WAVE);
}

bool RestorationSpecialization::ShouldCastChainHeal(::Unit* target)
{
    return target && ShouldUseChainHeal() && CanUseAbility(CHAIN_HEAL);
}

bool RestorationSpecialization::ShouldCastRiptide(::Unit* target)
{
    return target && !HasRiptide(target) && HasEnoughResource(RIPTIDE);
}

bool RestorationSpecialization::ShouldUseNatureSwiftness()
{
    return CanUseAbility(NATURE_SWIFTNESS) && _lastNatureSwiftness == 0;
}

void RestorationSpecialization::UpdateEarthShieldManagement()
{
    uint32 now = getMSTime();
    if (now - _lastEarthShieldCheck < 10000)
        return;
    _lastEarthShieldCheck = now;

    ::Unit* shieldTarget = GetBestEarthShieldTarget();
    if (shieldTarget && !HasEarthShield(shieldTarget))
        CastEarthShield(shieldTarget);
}

void RestorationSpecialization::CastEarthShield(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(EARTH_SHIELD))
    {
        bot->CastSpell(target, EARTH_SHIELD, false);
        ConsumeResource(EARTH_SHIELD);
        _earthShieldTargets[target->GetGUID()] = getMSTime();
        _lastEarthShield = 2000;
    }
}

::Unit* RestorationSpecialization::GetBestEarthShieldTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member->IsInWorld() &&
                bot->GetDistance(member) <= OPTIMAL_HEALING_RANGE)
            {
                if (member->HasAura(1459) || member->HasAura(9116))
                    return member;
            }
        }
    }

    return bot;
}

bool RestorationSpecialization::HasEarthShield(::Unit* target)
{
    if (!target)
        return false;

    auto it = _earthShieldTargets.find(target->GetGUID());
    if (it != _earthShieldTargets.end())
    {
        uint32 elapsed = getMSTime() - it->second;
        if (elapsed < EARTH_SHIELD_DURATION)
            return true;
        else
            _earthShieldTargets.erase(it);
    }

    return target->HasAura(EARTH_SHIELD);
}

void RestorationSpecialization::CastRiptide(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(RIPTIDE))
    {
        bot->CastSpell(target, RIPTIDE, false);
        ConsumeResource(RIPTIDE);
        _riptideTimers[target->GetGUID()] = getMSTime();
    }
}

bool RestorationSpecialization::HasRiptide(::Unit* target)
{
    if (!target)
        return false;

    auto it = _riptideTimers.find(target->GetGUID());
    if (it != _riptideTimers.end())
    {
        uint32 elapsed = getMSTime() - it->second;
        if (elapsed < RIPTIDE_DURATION)
            return true;
        else
            _riptideTimers.erase(it);
    }

    return target->HasAura(RIPTIDE);
}

void RestorationSpecialization::CastChainHeal(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(CHAIN_HEAL))
    {
        bot->CastSpell(target, CHAIN_HEAL, false);
        ConsumeResource(CHAIN_HEAL);
        _lastChainHeal = CHAIN_HEAL_COOLDOWN;
        TriggerTidalWave();
    }
}

bool RestorationSpecialization::ShouldUseChainHeal()
{
    uint32 injuredCount = 0;
    for (::Unit* member : _groupMembers)
    {
        if (member && member->IsAlive() && member->GetHealthPct() < 80.0f)
            injuredCount++;
    }

    return injuredCount >= CHAIN_HEAL_MIN_TARGETS;
}

void RestorationSpecialization::TriggerTidalWave()
{
    _tidalWaveStacks = std::min(_tidalWaveStacks + 1, MAX_TIDAL_WAVE_STACKS);
    _hasTidalWave = true;
}

bool RestorationSpecialization::HasTidalWave()
{
    return _hasTidalWave && _tidalWaveStacks > 0;
}

void RestorationSpecialization::UseNatureSwiftness()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(NATURE_SWIFTNESS))
    {
        bot->CastSpell(bot, NATURE_SWIFTNESS, false);
        ConsumeResource(NATURE_SWIFTNESS);
        _lastNatureSwiftness = NATURE_SWIFTNESS_COOLDOWN;
        _natureSwiftnessReady = getMSTime() + 15000;
    }
}

void RestorationSpecialization::CastInstantHealingWave(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(HEALING_WAVE))
    {
        bot->CastSpell(target, HEALING_WAVE, false);
        ConsumeResource(HEALING_WAVE);
        _natureSwiftnessReady = 0;
    }
}

bool RestorationSpecialization::IsEmergencyHealing()
{
    for (::Unit* member : _groupMembers)
    {
        if (member && member->IsAlive() &&
            member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        {
            if (!_emergencyMode)
            {
                _emergencyMode = true;
                _emergencyStartTime = getMSTime();
            }
            return true;
        }
    }

    if (_emergencyMode && getMSTime() - _emergencyStartTime > 10000)
        _emergencyMode = false;

    return false;
}

void RestorationSpecialization::HandleEmergencyHealing()
{
    for (::Unit* member : _groupMembers)
    {
        if (member && member->IsAlive() &&
            member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        {
            UseEmergencyHeals(member);
            return;
        }
    }
}

void RestorationSpecialization::UseEmergencyHeals(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (ShouldUseNatureSwiftness())
    {
        UseNatureSwiftness();
        CastInstantHealingWave(target);
        return;
    }

    if (ShouldCastLesserHealingWave(target))
    {
        bot->CastSpell(target, LESSER_HEALING_WAVE, false);
        ConsumeResource(LESSER_HEALING_WAVE);
    }
}

void RestorationSpecialization::UseWaterShield()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasAura(WATER_SHIELD) && HasEnoughResource(WATER_SHIELD))
    {
        bot->CastSpell(bot, WATER_SHIELD, false);
        ConsumeResource(WATER_SHIELD);
        _hasWaterShield = true;
    }
}

void RestorationSpecialization::ManageMana()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetPowerPct(POWER_MANA) < MANA_CONSERVATION_THRESHOLD)
    {
        UseManaSpringTotem();
    }
}

void RestorationSpecialization::UseManaSpringTotem()
{
    if (!IsTotemActive(TotemType::WATER))
        DeployTotem(TotemType::WATER, MANA_SPRING_TOTEM);
}

void RestorationSpecialization::UpdateCleansingTotems()
{
    if (ShouldUsePoisonCleansing())
        DeployTotem(TotemType::WATER, POISON_CLEANSING_TOTEM);
    else if (ShouldUseDiseaseCleansing())
        DeployTotem(TotemType::WATER, DISEASE_CLEANSING_TOTEM);
}

bool RestorationSpecialization::ShouldUsePoisonCleansing()
{
    for (::Unit* member : _groupMembers)
    {
        if (member && member->HasAuraType(SPELL_AURA_POISON))
            return true;
    }
    return false;
}

bool RestorationSpecialization::ShouldUseDiseaseCleansing()
{
    for (::Unit* member : _groupMembers)
    {
        if (member && member->HasAuraType(SPELL_AURA_DISEASE))
            return true;
    }
    return false;
}

} // namespace Playerbot