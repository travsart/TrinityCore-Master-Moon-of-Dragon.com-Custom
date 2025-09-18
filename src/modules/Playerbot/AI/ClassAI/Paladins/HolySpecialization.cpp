/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HolySpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Group.h"
#include "Log.h"

namespace Playerbot
{

HolySpecialization::HolySpecialization(Player* bot)
    : PaladinSpecialization(bot)
    , _currentAura(PaladinAura::DEVOTION)
    , _holyPower(0)
    , _lastDivineFavor(0)
    , _lastLayOnHands(0)
    , _hasDivineIllumination(false)
    , _hasInfusionOfLight(false)
    , _lastHealCheck(0)
    , _lastBeaconCheck(0)
    , _lastAuraCheck(0)
    , _lastRotationUpdate(0)
    , _lastGroupScan(0)
    , _emergencyMode(false)
    , _emergencyStartTime(0)
{
}

void HolySpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    uint32 now = getMSTime();
    if (now - _lastRotationUpdate < 100)
        return;
    _lastRotationUpdate = now;

    UpdateHealing();

    if (target->IsHostileTo(bot))
    {
        UpdateJudgementForMana();

        if (CanHolyShockDamage())
            CastHolyShock(target);
    }
}

void HolySpecialization::UpdateBuffs()
{
    UpdateBeaconOfLight();
    UpdateAura();
    ManageDivineFavor();
}

void HolySpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
}

bool HolySpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void HolySpecialization::OnCombatStart(::Unit* target)
{
    _emergencyMode = false;
    UpdateGroupHealing();
}

void HolySpecialization::OnCombatEnd()
{
    _emergencyMode = false;
    _emergencyStartTime = 0;

    while (!_healQueue.empty())
        _healQueue.pop();
}

bool HolySpecialization::HasEnoughResource(uint32 spellId)
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

void HolySpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    if (bot->GetPower(POWER_MANA) >= manaCost)
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
}

Position HolySpecialization::GetOptimalPosition(::Unit* target)
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

float HolySpecialization::GetOptimalRange(::Unit* target)
{
    return 25.0f;
}

void HolySpecialization::UpdateAura()
{
    uint32 now = getMSTime();
    if (now - _lastAuraCheck < 5000)
        return;
    _lastAuraCheck = now;

    PaladinAura optimalAura = GetOptimalAura();
    if (_currentAura != optimalAura)
        SwitchAura(optimalAura);
}

PaladinAura HolySpecialization::GetOptimalAura()
{
    Player* bot = GetBot();
    if (!bot)
        return PaladinAura::DEVOTION;

    if (bot->IsInCombat())
    {
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member->GetHealthPct() < 50.0f)
                    return PaladinAura::DEVOTION;
            }
        }
        return PaladinAura::CONCENTRATION;
    }

    return PaladinAura::DEVOTION;
}

void HolySpecialization::SwitchAura(PaladinAura aura)
{
    _currentAura = aura;
}

void HolySpecialization::UpdateHealing()
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

void HolySpecialization::UpdateGroupHealing()
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
                bot->GetDistance(member) <= 40.0f)
            {
                _groupMembers.push_back(member);
            }
        }
    }
}

::Unit* HolySpecialization::GetBestHealTarget()
{
    if (_healQueue.empty())
        return nullptr;

    PaladinHealTarget target = _healQueue.top();
    _healQueue.pop();

    if (target.target && target.target->IsAlive() &&
        target.target->GetHealthPct() < 95.0f)
        return target.target;

    return nullptr;
}

void HolySpecialization::HealTarget(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    float healthPct = target->GetHealthPct();
    uint32 missingHealth = target->GetMaxHealth() - target->GetHealth();

    if (healthPct < EMERGENCY_HEALTH_THRESHOLD)
    {
        if (ShouldCastLayOnHands(target))
        {
            bot->CastSpell(target, 633, false);
            _cooldowns[633] = LAY_ON_HANDS_COOLDOWN;
            _lastLayOnHands = getMSTime();
            return;
        }

        if (ShouldCastHolyShock(target) && CanHolyShockHeal())
        {
            CastHolyShock(target);
            return;
        }

        if (ShouldUseDivineFavor())
        {
            bot->CastSpell(bot, DIVINE_FAVOR, false);
            _cooldowns[DIVINE_FAVOR] = DIVINE_FAVOR_COOLDOWN;
            _lastDivineFavor = getMSTime();
        }

        if (ShouldCastFlashOfLight(target))
        {
            bot->CastSpell(target, 19750, false);
            ConsumeResource(19750);
        }
    }
    else if (healthPct < FLASH_OF_LIGHT_THRESHOLD)
    {
        if (ShouldCastHolyShock(target) && CanHolyShockHeal())
        {
            CastHolyShock(target);
            return;
        }

        if (ShouldCastFlashOfLight(target))
        {
            bot->CastSpell(target, 19750, false);
            ConsumeResource(19750);
        }
    }
    else if (healthPct < HOLY_LIGHT_THRESHOLD)
    {
        if (ShouldCastHolyLight(target))
        {
            bot->CastSpell(target, 635, false);
            ConsumeResource(635);
        }
    }
}

void HolySpecialization::PerformTriage()
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

        PaladinHealPriority priority = PaladinHealPriority::FULL;
        uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();

        if (healthPct < 20.0f)
            priority = PaladinHealPriority::EMERGENCY;
        else if (healthPct < 40.0f)
            priority = PaladinHealPriority::CRITICAL;
        else if (healthPct < 70.0f)
            priority = PaladinHealPriority::MODERATE;
        else if (healthPct < 90.0f)
            priority = PaladinHealPriority::MAINTENANCE;

        _healQueue.emplace(member, priority, healthPct, missingHealth);
    }
}

bool HolySpecialization::ShouldCastHolyLight(::Unit* target)
{
    return target && target->GetHealthPct() < HOLY_LIGHT_THRESHOLD &&
           HasEnoughResource(635);
}

bool HolySpecialization::ShouldCastFlashOfLight(::Unit* target)
{
    return target && target->GetHealthPct() < FLASH_OF_LIGHT_THRESHOLD &&
           HasEnoughResource(19750);
}

bool HolySpecialization::ShouldCastHolyShock(::Unit* target)
{
    return target && CanUseAbility(HOLY_SHOCK);
}

bool HolySpecialization::ShouldCastLayOnHands(::Unit* target)
{
    return target && target->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD &&
           CanUseAbility(633) &&
           getMSTime() - _lastLayOnHands > LAY_ON_HANDS_COOLDOWN;
}

bool HolySpecialization::ShouldUseDivineFavor()
{
    return CanUseAbility(DIVINE_FAVOR) &&
           getMSTime() - _lastDivineFavor > DIVINE_FAVOR_COOLDOWN;
}

void HolySpecialization::UpdateBeaconOfLight()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastBeaconCheck < 10000)
        return;
    _lastBeaconCheck = now;

    ::Unit* beaconTarget = GetBestBeaconTarget();
    if (beaconTarget && !HasBeaconOfLight(beaconTarget))
        CastBeaconOfLight(beaconTarget);
}

void HolySpecialization::CastBeaconOfLight(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(BEACON_OF_LIGHT))
    {
        bot->CastSpell(target, BEACON_OF_LIGHT, false);
        ConsumeResource(BEACON_OF_LIGHT);
        _beaconTargets[target->GetGUID().GetRawValue()] = getMSTime();
    }
}

::Unit* HolySpecialization::GetBestBeaconTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member != bot && member->IsInWorld() &&
                bot->GetDistance(member) <= 40.0f)
            {
                if (member->HasAura(1459) || member->HasAura(9116))
                    return member;
            }
        }
    }

    return bot;
}

bool HolySpecialization::HasBeaconOfLight(::Unit* target)
{
    if (!target)
        return false;

    auto it = _beaconTargets.find(target->GetGUID().GetRawValue());
    if (it != _beaconTargets.end())
    {
        uint32 elapsed = getMSTime() - it->second;
        if (elapsed < BEACON_OF_LIGHT_DURATION)
            return true;
        else
            _beaconTargets.erase(it);
    }

    return target->HasAura(BEACON_OF_LIGHT);
}

void HolySpecialization::CastHolyShock(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(HOLY_SHOCK))
    {
        bot->CastSpell(target, HOLY_SHOCK, false);
        ConsumeResource(HOLY_SHOCK);
        _cooldowns[HOLY_SHOCK] = HOLY_SHOCK_COOLDOWN;
    }
}

bool HolySpecialization::CanHolyShockHeal()
{
    return true;
}

bool HolySpecialization::CanHolyShockDamage()
{
    return true;
}

void HolySpecialization::ManageDivineFavor()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    _hasDivineIllumination = bot->HasAura(DIVINE_ILLUMINATION);
    _hasInfusionOfLight = bot->HasAura(INFUSION_OF_LIGHT);
}

bool HolySpecialization::IsEmergencyHealing()
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

void HolySpecialization::HandleEmergencyHealing()
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

void HolySpecialization::UseEmergencyHeals(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (ShouldCastLayOnHands(target))
    {
        bot->CastSpell(target, 633, false);
        _cooldowns[633] = LAY_ON_HANDS_COOLDOWN;
        _lastLayOnHands = getMSTime();
        return;
    }

    if (ShouldUseDivineFavor())
    {
        bot->CastSpell(bot, DIVINE_FAVOR, false);
        _cooldowns[DIVINE_FAVOR] = DIVINE_FAVOR_COOLDOWN;
        _lastDivineFavor = getMSTime();
    }

    if (ShouldCastHolyShock(target))
    {
        CastHolyShock(target);
        return;
    }

    if (ShouldCastFlashOfLight(target))
    {
        bot->CastSpell(target, 19750, false);
        ConsumeResource(19750);
    }
}

void HolySpecialization::UpdateJudgementForMana()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetPowerPct(POWER_MANA) < 50.0f && ShouldJudgeForMana())
    {
        ::Unit* target = bot->GetSelectedUnit();
        if (target && target->IsHostileTo(bot))
            CastJudgementOfWisdom(target);
    }
}

void HolySpecialization::CastJudgementOfWisdom(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(JUDGEMENT_OF_WISDOM))
    {
        bot->CastSpell(target, JUDGEMENT_OF_WISDOM, false);
        ConsumeResource(JUDGEMENT_OF_WISDOM);
    }
}

bool HolySpecialization::ShouldJudgeForMana()
{
    Player* bot = GetBot();
    return bot && bot->GetPowerPct(POWER_MANA) < 50.0f;
}

} // namespace Playerbot