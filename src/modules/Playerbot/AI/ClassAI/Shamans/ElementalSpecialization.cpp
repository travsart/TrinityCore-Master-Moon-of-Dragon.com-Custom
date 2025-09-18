/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ElementalSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

ElementalSpecialization::ElementalSpecialization(Player* bot)
    : ShamanSpecialization(bot)
    , _lightningShieldCharges(0)
    , _elementalFocusStacks(0)
    , _clearcastingProcs(0)
    , _lastLightningBolt(0)
    , _lastChainLightning(0)
    , _lastLavaBurst(0)
    , _lastElementalBlast(0)
    , _lastThunderstorm(0)
    , _primaryTarget(nullptr)
    , _totalDamageDealt(0)
    , _manaSpent(0)
    , _spellsCast(0)
{
}

void ElementalSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateElementalFocus();
    UpdateLavaBurst();
    UpdateLightningShield();
    UpdateShockRotation(target);

    std::vector<::Unit*> chainTargets = GetChainLightningTargets(target);
    if (chainTargets.size() >= CHAIN_LIGHTNING_MIN_TARGETS && ShouldCastChainLightning(chainTargets))
    {
        CastChainLightning(chainTargets);
        return;
    }

    if (ShouldCastLavaBurst(target))
    {
        CastLavaBurst(target);
        return;
    }

    if (ShouldCastElementalBlast(target))
    {
        CastElementalBlast(target);
        return;
    }

    if (ShouldCastLightningBolt(target))
    {
        CastLightningBolt(target);
        return;
    }

    if (ShouldCastThunderstorm())
    {
        CastThunderstorm();
        return;
    }
}

void ElementalSpecialization::UpdateBuffs()
{
    UpdateLightningShield();
    UpdateTotemManagement();
    ManageElementalFocus();
}

void ElementalSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_lastLightningBolt > diff)
        _lastLightningBolt -= diff;
    else
        _lastLightningBolt = 0;

    if (_lastChainLightning > diff)
        _lastChainLightning -= diff;
    else
        _lastChainLightning = 0;

    if (_lastLavaBurst > diff)
        _lastLavaBurst -= diff;
    else
        _lastLavaBurst = 0;

    if (_lastElementalBlast > diff)
        _lastElementalBlast -= diff;
    else
        _lastElementalBlast = 0;

    if (_lastThunderstorm > diff)
        _lastThunderstorm -= diff;
    else
        _lastThunderstorm = 0;
}

bool ElementalSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void ElementalSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    RefreshLightningShield();
    DeployOptimalTotems();
    _primaryTarget = target;
}

void ElementalSpecialization::OnCombatEnd()
{
    _primaryTarget = nullptr;
    _chainTargets.clear();
    _elementalFocusStacks = 0;
    _clearcastingProcs = 0;
    _cooldowns.clear();
}

bool ElementalSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    if (HasClearcasting())
        return true;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return true;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void ElementalSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasClearcasting())
    {
        _clearcastingProcs--;
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    if (bot->GetPower(POWER_MANA) >= manaCost)
    {
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
        _manaSpent += manaCost;
    }

    _spellsCast++;
}

Position ElementalSpecialization::GetOptimalPosition(::Unit* target)
{
    return GetSafeCastingPosition(target);
}

float ElementalSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

void ElementalSpecialization::UpdateTotemManagement()
{
    uint32 now = getMSTime();
    if (now - _lastTotemUpdate < 3000)
        return;
    _lastTotemUpdate = now;

    DeployOptimalTotems();
}

void ElementalSpecialization::DeployOptimalTotems()
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

uint32 ElementalSpecialization::GetOptimalFireTotem()
{
    Player* bot = GetBot();
    if (!bot)
        return SEARING_TOTEM;

    if (bot->IsInCombat())
    {
        auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), 30.0f);
        uint32 enemyCount = 0;
        for (auto unit : units)
        {
            if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
                enemyCount++;
        }

        if (enemyCount > 3)
            return MAGMA_TOTEM;
        else if (bot->getLevel() >= 50)
            return TOTEM_OF_WRATH;
        else
            return SEARING_TOTEM;
    }

    return FLAMETONGUE_TOTEM;
}

uint32 ElementalSpecialization::GetOptimalEarthTotem()
{
    Player* bot = GetBot();
    if (!bot)
        return STRENGTH_OF_EARTH_TOTEM;

    if (bot->IsInCombat())
    {
        if (bot->GetHealthPct() < 50.0f)
            return STONECLAW_TOTEM;
        else
            return STRENGTH_OF_EARTH_TOTEM;
    }

    return STRENGTH_OF_EARTH_TOTEM;
}

uint32 ElementalSpecialization::GetOptimalWaterTotem()
{
    Player* bot = GetBot();
    if (!bot)
        return MANA_SPRING_TOTEM;

    if (bot->GetPowerPct(POWER_MANA) < 50.0f)
        return MANA_SPRING_TOTEM;

    return HEALING_STREAM_TOTEM;
}

uint32 ElementalSpecialization::GetOptimalAirTotem()
{
    return WRATH_OF_AIR_TOTEM;
}

void ElementalSpecialization::UpdateShockRotation(::Unit* target)
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

uint32 ElementalSpecialization::GetNextShockSpell(::Unit* target)
{
    if (!target)
        return 0;

    if (!target->HasAura(FLAME_SHOCK))
        return FLAME_SHOCK;

    if (target->GetHealthPct() > 50.0f)
        return EARTH_SHOCK;

    return FROST_SHOCK;
}

void ElementalSpecialization::UpdateElementalFocus()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasAura(16246))
        _elementalFocusStacks = std::min(_elementalFocusStacks + 1, ELEMENTAL_FOCUS_MAX_STACKS);
}

void ElementalSpecialization::UpdateLavaBurst()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (CanUseAbility(LAVA_BURST) && _primaryTarget)
    {
        if (_primaryTarget->HasAura(FLAME_SHOCK))
        {
            CastLavaBurst(_primaryTarget);
        }
    }
}

void ElementalSpecialization::UpdateLightningShield()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasAura(LIGHTNING_SHIELD) || GetLightningShieldCharges() < 1)
        RefreshLightningShield();
}

bool ElementalSpecialization::ShouldCastLightningBolt(::Unit* target)
{
    return target && HasEnoughResource(LIGHTNING_BOLT) &&
           GetBot()->GetDistance(target) <= OPTIMAL_CASTING_RANGE;
}

bool ElementalSpecialization::ShouldCastChainLightning(const std::vector<::Unit*>& enemies)
{
    return enemies.size() >= CHAIN_LIGHTNING_MIN_TARGETS &&
           HasEnoughResource(CHAIN_LIGHTNING);
}

bool ElementalSpecialization::ShouldCastLavaBurst(::Unit* target)
{
    return target && target->HasAura(FLAME_SHOCK) &&
           HasEnoughResource(LAVA_BURST) &&
           CanUseAbility(LAVA_BURST);
}

bool ElementalSpecialization::ShouldCastElementalBlast(::Unit* target)
{
    return target && HasEnoughResource(ELEMENTAL_BLAST) &&
           CanUseAbility(ELEMENTAL_BLAST);
}

bool ElementalSpecialization::ShouldCastThunderstorm()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), THUNDERSTORM_RANGE);
    uint32 enemyCount = 0;
    for (auto unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
            enemyCount++;
    }

    return enemyCount >= 3 && HasEnoughResource(THUNDERSTORM) &&
           CanUseAbility(THUNDERSTORM);
}

void ElementalSpecialization::CastLightningBolt(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(LIGHTNING_BOLT))
    {
        bot->CastSpell(target, LIGHTNING_BOLT, false);
        ConsumeResource(LIGHTNING_BOLT);
        _lastLightningBolt = 2500;
    }
}

void ElementalSpecialization::CastChainLightning(const std::vector<::Unit*>& enemies)
{
    Player* bot = GetBot();
    if (!bot || enemies.empty())
        return;

    if (HasEnoughResource(CHAIN_LIGHTNING))
    {
        bot->CastSpell(enemies[0], CHAIN_LIGHTNING, false);
        ConsumeResource(CHAIN_LIGHTNING);
        _lastChainLightning = 2500;
    }
}

void ElementalSpecialization::CastLavaBurst(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(LAVA_BURST))
    {
        bot->CastSpell(target, LAVA_BURST, false);
        ConsumeResource(LAVA_BURST);
        _lastLavaBurst = 2000;
        _cooldowns[LAVA_BURST] = 8000;
    }
}

void ElementalSpecialization::CastElementalBlast(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(ELEMENTAL_BLAST))
    {
        bot->CastSpell(target, ELEMENTAL_BLAST, false);
        ConsumeResource(ELEMENTAL_BLAST);
        _lastElementalBlast = 2000;
        _cooldowns[ELEMENTAL_BLAST] = 12000;
    }
}

void ElementalSpecialization::CastThunderstorm()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(THUNDERSTORM))
    {
        bot->CastSpell(bot, THUNDERSTORM, false);
        ConsumeResource(THUNDERSTORM);
        _lastThunderstorm = 1500;
        _cooldowns[THUNDERSTORM] = 45000;
    }
}

::Unit* ElementalSpecialization::GetBestElementalTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    return bot->GetSelectedUnit();
}

std::vector<::Unit*> ElementalSpecialization::GetChainLightningTargets(::Unit* primary)
{
    std::vector<::Unit*> targets;
    Player* bot = GetBot();
    if (!bot || !primary)
        return targets;

    targets.push_back(primary);

    auto units = bot->GetMap()->GetUnitsInRange(primary->GetPosition(), CHAIN_LIGHTNING_RANGE);
    for (auto unit : units)
    {
        if (unit && unit != primary && unit->IsHostileTo(bot) && unit->IsAlive())
        {
            targets.push_back(unit);
            if (targets.size() >= 5)
                break;
        }
    }

    return targets;
}

::Unit* ElementalSpecialization::GetBestShockTarget()
{
    return GetBestElementalTarget();
}

void ElementalSpecialization::ManageMana()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetPowerPct(POWER_MANA) < MANA_CONSERVATION_THRESHOLD)
    {
        UseManaSpringTotem();
    }
}

void ElementalSpecialization::UseManaSpringTotem()
{
    if (!IsTotemActive(TotemType::WATER))
        DeployTotem(TotemType::WATER, MANA_SPRING_TOTEM);
}

bool ElementalSpecialization::ShouldConserveMana()
{
    Player* bot = GetBot();
    return bot && bot->GetPowerPct(POWER_MANA) < MANA_CONSERVATION_THRESHOLD;
}

void ElementalSpecialization::ManageElementalFocus()
{
    if (_elementalFocusStacks > 0 && ShouldConserveMana())
    {
        TriggerClearcastingProc();
    }
}

void ElementalSpecialization::TriggerClearcastingProc()
{
    _clearcastingProcs++;
    _elementalFocusStacks = 0;
}

bool ElementalSpecialization::HasClearcasting()
{
    return _clearcastingProcs > 0;
}

void ElementalSpecialization::RefreshLightningShield()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(LIGHTNING_SHIELD))
    {
        bot->CastSpell(bot, LIGHTNING_SHIELD, false);
        ConsumeResource(LIGHTNING_SHIELD);
        _lightningShieldCharges = LIGHTNING_SHIELD_MAX_CHARGES;
    }
}

uint32 ElementalSpecialization::GetLightningShieldCharges()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    if (Aura* aura = bot->GetAura(LIGHTNING_SHIELD))
        return aura->GetCharges();

    return 0;
}

Position ElementalSpecialization::GetSafeCastingPosition(::Unit* target)
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

} // namespace Playerbot