/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EnhancementSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Item.h"
#include "Log.h"

namespace Playerbot
{

EnhancementSpecialization::EnhancementSpecialization(Player* bot)
    : ShamanSpecialization(bot)
    , _stormstrikeCharges(0)
    , _maelstromWeaponStacks(0)
    , _unleashedFuryStacks(0)
    , _lastFlametongueRefresh(0)
    , _lastWindfuryRefresh(0)
    , _lastStormstrike(0)
    , _lastLavaLash(0)
    , _lastShamanisticRage(0)
    , _lastFeralSpirit(0)
    , _dualWielding(false)
    , _hasShamanisticRage(false)
    , _hasFeralSpirit(false)
    , _totalMeleeDamage(0)
    , _instantSpellsCast(0)
    , _weaponImbueProcs(0)
{
    _dualWielding = IsDualWielding();
}

void EnhancementSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateWeaponImbues();
    UpdateMaelstromWeapon();
    UpdateStormstrike();

    if (ShouldConsumeMaelstromWeapon() && _maelstromWeaponStacks >= 5)
    {
        if (ShouldCastChainLightning())
            CastInstantChainLightning();
        else if (ShouldCastLightningBolt(target))
            CastInstantLightningBolt(target);
        return;
    }

    if (ShouldCastStormstrike(target))
    {
        CastStormstrike(target);
        return;
    }

    if (ShouldCastLavaLash(target))
    {
        CastLavaLash(target);
        return;
    }

    UpdateShockRotation(target);

    if (ShouldUseShamanisticRage())
    {
        CastShamanisticRage();
        return;
    }

    if (ShouldCastFeralSpirit())
    {
        CastFeralSpirit();
        return;
    }
}

void EnhancementSpecialization::UpdateBuffs()
{
    UpdateWeaponImbues();
    UpdateTotemManagement();
    OptimizeDualWield();
}

void EnhancementSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    for (auto& imbue : _weaponImbues)
        if (imbue.remainingTime > diff)
            imbue.remainingTime -= diff;
        else
            imbue.remainingTime = 0;

    if (_lastStormstrike > diff)
        _lastStormstrike -= diff;
    else
        _lastStormstrike = 0;

    if (_lastLavaLash > diff)
        _lastLavaLash -= diff;
    else
        _lastLavaLash = 0;

    if (_lastShamanisticRage > diff)
        _lastShamanisticRage -= diff;
    else
        _lastShamanisticRage = 0;

    if (_lastFeralSpirit > diff)
        _lastFeralSpirit -= diff;
    else
        _lastFeralSpirit = 0;
}

bool EnhancementSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    switch (spellId)
    {
        case STORMSTRIKE:
            return _lastStormstrike == 0;
        case LAVA_LASH:
            return _lastLavaLash == 0;
        case SHAMANISTIC_RAGE:
            return _lastShamanisticRage == 0;
        case FERAL_SPIRIT:
            return _lastFeralSpirit == 0;
    }

    return HasEnoughResource(spellId);
}

void EnhancementSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    ApplyWeaponImbues();
    DeployOptimalTotems();
    _maelstromWeaponStacks = 0;
}

void EnhancementSpecialization::OnCombatEnd()
{
    _maelstromWeaponStacks = 0;
    _stormstrikeCharges = 0;
    _hasShamanisticRage = false;
    _hasFeralSpirit = false;
    _cooldowns.clear();
}

bool EnhancementSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    switch (spellId)
    {
        case LIGHTNING_BOLT_INSTANT:
        case CHAIN_LIGHTNING_INSTANT:
            return _maelstromWeaponStacks >= 5;
        case STORMSTRIKE:
        case LAVA_LASH:
            return true; // Melee abilities don't cost mana
        default:
            break;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return true;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void EnhancementSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    switch (spellId)
    {
        case LIGHTNING_BOLT_INSTANT:
        case CHAIN_LIGHTNING_INSTANT:
            _maelstromWeaponStacks = 0;
            _instantSpellsCast++;
            return;
        case STORMSTRIKE:
        case LAVA_LASH:
            return; // Melee abilities don't cost resources
        default:
            break;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    uint32 manaCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    if (bot->GetPower(POWER_MANA) >= manaCost)
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
}

Position EnhancementSpecialization::GetOptimalPosition(::Unit* target)
{
    return GetOptimalMeleePosition(target);
}

float EnhancementSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

void EnhancementSpecialization::UpdateTotemManagement()
{
    uint32 now = getMSTime();
    if (now - _lastTotemUpdate < 3000)
        return;
    _lastTotemUpdate = now;

    DeployOptimalTotems();
}

void EnhancementSpecialization::DeployOptimalTotems()
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

uint32 EnhancementSpecialization::GetOptimalFireTotem()
{
    Player* bot = GetBot();
    if (!bot)
        return SEARING_TOTEM;

    if (bot->IsInCombat())
        return SEARING_TOTEM;

    return FLAMETONGUE_TOTEM;
}

uint32 EnhancementSpecialization::GetOptimalEarthTotem()
{
    return STRENGTH_OF_EARTH_TOTEM;
}

uint32 EnhancementSpecialization::GetOptimalWaterTotem()
{
    Player* bot = GetBot();
    if (!bot)
        return MANA_SPRING_TOTEM;

    if (bot->GetPowerPct(POWER_MANA) < 50.0f)
        return MANA_SPRING_TOTEM;

    return HEALING_STREAM_TOTEM;
}

uint32 EnhancementSpecialization::GetOptimalAirTotem()
{
    return WINDFURY_TOTEM;
}

void EnhancementSpecialization::UpdateShockRotation(::Unit* target)
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

uint32 EnhancementSpecialization::GetNextShockSpell(::Unit* target)
{
    if (!target)
        return 0;

    if (!target->HasAura(FLAME_SHOCK))
        return FLAME_SHOCK;

    return EARTH_SHOCK;
}

void EnhancementSpecialization::UpdateWeaponImbues()
{
    uint32 now = getMSTime();
    if (now - _lastFlametongueRefresh > WEAPON_IMBUE_CHECK_INTERVAL)
    {
        _lastFlametongueRefresh = now;
        if (!HasWeaponImbue(true))
            RefreshWeaponImbue(true);
    }

    if (_dualWielding && now - _lastWindfuryRefresh > WEAPON_IMBUE_CHECK_INTERVAL)
    {
        _lastWindfuryRefresh = now;
        if (!HasWeaponImbue(false))
            RefreshWeaponImbue(false);
    }
}

void EnhancementSpecialization::UpdateMaelstromWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (Aura* aura = bot->GetAura(MAELSTROM_WEAPON))
        _maelstromWeaponStacks = aura->GetCharges();
    else
        _maelstromWeaponStacks = 0;
}

void EnhancementSpecialization::UpdateStormstrike()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (Aura* aura = bot->GetAura(STORMSTRIKE))
        _stormstrikeCharges = aura->GetCharges();
    else
        _stormstrikeCharges = 0;
}

bool EnhancementSpecialization::ShouldCastStormstrike(::Unit* target)
{
    return target && IsInMeleeRange(target) && CanUseAbility(STORMSTRIKE);
}

bool EnhancementSpecialization::ShouldCastLavaLash(::Unit* target)
{
    return target && IsInMeleeRange(target) && CanUseAbility(LAVA_LASH) &&
           HasWeaponImbue(false);
}

bool EnhancementSpecialization::ShouldCastLightningBolt(::Unit* target)
{
    return target && _maelstromWeaponStacks >= 5;
}

bool EnhancementSpecialization::ShouldCastChainLightning()
{
    Player* bot = GetBot();
    if (!bot || _maelstromWeaponStacks < 5)
        return false;

    auto units = bot->GetMap()->GetUnitsInRange(bot->GetPosition(), 25.0f);
    uint32 enemyCount = 0;
    for (auto unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
            enemyCount++;
    }

    return enemyCount >= 3;
}

bool EnhancementSpecialization::ShouldUseShamanisticRage()
{
    Player* bot = GetBot();
    return bot && bot->GetPowerPct(POWER_MANA) < 30.0f &&
           CanUseAbility(SHAMANISTIC_RAGE);
}

bool EnhancementSpecialization::ShouldCastFeralSpirit()
{
    Player* bot = GetBot();
    return bot && bot->IsInCombat() && CanUseAbility(FERAL_SPIRIT);
}

void EnhancementSpecialization::ApplyWeaponImbues()
{
    if (!HasWeaponImbue(true))
        CastFlametongueWeapon();

    if (_dualWielding && !HasWeaponImbue(false))
        CastWindfuryWeapon();
}

void EnhancementSpecialization::CastFlametongueWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(FLAMETONGUE_WEAPON))
    {
        bot->CastSpell(bot, FLAMETONGUE_WEAPON, false);
        ConsumeResource(FLAMETONGUE_WEAPON);
        _weaponImbues[0] = WeaponImbue(FLAMETONGUE_WEAPON, FLAMETONGUE_DURATION, 0, true);
    }
}

void EnhancementSpecialization::CastWindfuryWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(WINDFURY_WEAPON))
    {
        bot->CastSpell(bot, WINDFURY_WEAPON, false);
        ConsumeResource(WINDFURY_WEAPON);
        _weaponImbues[1] = WeaponImbue(WINDFURY_WEAPON, WINDFURY_DURATION, 0, false);
    }
}

void EnhancementSpecialization::CastFrostbrandWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(FROSTBRAND_WEAPON))
    {
        bot->CastSpell(bot, FROSTBRAND_WEAPON, false);
        ConsumeResource(FROSTBRAND_WEAPON);
        _weaponImbues[0] = WeaponImbue(FROSTBRAND_WEAPON, FROSTBRAND_DURATION, 0, true);
    }
}

void EnhancementSpecialization::RefreshWeaponImbue(bool mainHand)
{
    if (mainHand)
        CastFlametongueWeapon();
    else if (_dualWielding)
        CastWindfuryWeapon();
}

bool EnhancementSpecialization::HasWeaponImbue(bool mainHand)
{
    size_t index = mainHand ? 0 : 1;
    return _weaponImbues[index].remainingTime > 0;
}

uint32 EnhancementSpecialization::GetWeaponImbueRemainingTime(bool mainHand)
{
    size_t index = mainHand ? 0 : 1;
    return _weaponImbues[index].remainingTime;
}

void EnhancementSpecialization::ConsumeMaelstromWeapon()
{
    _maelstromWeaponStacks = 0;
}

bool EnhancementSpecialization::ShouldConsumeMaelstromWeapon()
{
    return _maelstromWeaponStacks >= 5;
}

uint32 EnhancementSpecialization::GetMaelstromWeaponStacks()
{
    return _maelstromWeaponStacks;
}

void EnhancementSpecialization::CastInstantLightningBolt(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(LIGHTNING_BOLT_INSTANT))
    {
        bot->CastSpell(target, LIGHTNING_BOLT_INSTANT, false);
        ConsumeResource(LIGHTNING_BOLT_INSTANT);
    }
}

void EnhancementSpecialization::CastInstantChainLightning()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    ::Unit* target = bot->GetSelectedUnit();
    if (!target)
        return;

    if (HasEnoughResource(CHAIN_LIGHTNING_INSTANT))
    {
        bot->CastSpell(target, CHAIN_LIGHTNING_INSTANT, false);
        ConsumeResource(CHAIN_LIGHTNING_INSTANT);
    }
}

void EnhancementSpecialization::CastStormstrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(STORMSTRIKE))
    {
        bot->CastSpell(target, STORMSTRIKE, false);
        ConsumeResource(STORMSTRIKE);
        _lastStormstrike = STORMSTRIKE_COOLDOWN;
    }
}

void EnhancementSpecialization::CastLavaLash(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(LAVA_LASH))
    {
        bot->CastSpell(target, LAVA_LASH, false);
        ConsumeResource(LAVA_LASH);
        _lastLavaLash = LAVA_LASH_COOLDOWN;
    }
}

void EnhancementSpecialization::CastShamanisticRage()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SHAMANISTIC_RAGE))
    {
        bot->CastSpell(bot, SHAMANISTIC_RAGE, false);
        ConsumeResource(SHAMANISTIC_RAGE);
        _lastShamanisticRage = SHAMANISTIC_RAGE_COOLDOWN;
        _hasShamanisticRage = true;
    }
}

void EnhancementSpecialization::CastFeralSpirit()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(FERAL_SPIRIT))
    {
        bot->CastSpell(bot, FERAL_SPIRIT, false);
        ConsumeResource(FERAL_SPIRIT);
        _lastFeralSpirit = FERAL_SPIRIT_COOLDOWN;
        _hasFeralSpirit = true;
    }
}

bool EnhancementSpecialization::IsDualWielding()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    return mainHand && offHand &&
           offHand->GetTemplate()->GetInventoryType() != INVTYPE_SHIELD;
}

void EnhancementSpecialization::OptimizeDualWield()
{
    _dualWielding = IsDualWielding();
}

bool EnhancementSpecialization::IsInMeleeRange(::Unit* target)
{
    Player* bot = GetBot();
    return bot && target && bot->GetDistance(target) <= MELEE_RANGE;
}

Position EnhancementSpecialization::GetOptimalMeleePosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    float distance = MELEE_RANGE * 0.8f;
    float angle = target->GetAngle(bot);

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle + M_PI
    );
}

} // namespace Playerbot