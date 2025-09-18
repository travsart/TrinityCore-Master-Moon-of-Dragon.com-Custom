/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShamanAI.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "ObjectMgr.h"
#include "GameObject.h"
#include "Map.h"
#include "Group.h"
#include "Pet.h"
#include "Totem.h"
#include "WorldSession.h"

namespace Playerbot
{

// Totem spell mappings by element
const std::unordered_map<TotemType, std::vector<uint32>> ShamanAI::TOTEM_SPELLS = {
    { TotemType::FIRE, { SEARING_TOTEM, FIRE_NOVA_TOTEM, MAGMA_TOTEM, FLAMETONGUE_TOTEM, TOTEM_OF_WRATH } },
    { TotemType::EARTH, { EARTHBIND_TOTEM, STONESKIN_TOTEM, STONECLAW_TOTEM, STRENGTH_OF_EARTH_TOTEM, TREMOR_TOTEM } },
    { TotemType::WATER, { HEALING_STREAM_TOTEM, MANA_SPRING_TOTEM, POISON_CLEANSING_TOTEM, DISEASE_CLEANSING_TOTEM, FIRE_RESISTANCE_TOTEM } },
    { TotemType::AIR, { GROUNDING_TOTEM, NATURE_RESISTANCE_TOTEM, WINDFURY_TOTEM, GRACE_OF_AIR_TOTEM, WRATH_OF_AIR_TOTEM } }
};

ShamanAI::ShamanAI(Player* bot) : ClassAI(bot)
{
    _specialization = DetectSpecialization();
    _manaSpent = 0;
    _damageDealt = 0;
    _healingDone = 0;
    _totemsDeploy = 0;
    _shocksUsed = 0;

    // Initialize totem tracking
    for (auto& totem : _activeTotems)
        totem = TotemInfo();

    _lastTotemUpdate = 0;
    _totemCheckInterval = TOTEM_CHECK_INTERVAL;
    _needsTotemRefresh = false;
    _optimalTotemPosition = Position();

    // Enhancement tracking
    for (auto& imbue : _weaponImbues)
        imbue = WeaponImbue();

    _stormstrikeCharges = 0;
    _maelstromWeaponStacks = 0;
    _unleashedFuryStacks = 0;
    _lastFlametongueRefresh = 0;
    _lastWindfuryRefresh = 0;
    _dualWielding = IsDualWielding();

    // Elemental tracking
    _lightningShieldCharges = 0;
    _lavaLashStacks = 0;
    _elementalFocusStacks = 0;
    _clearcastingProcs = 0;
    _lastLightningBolt = 0;
    _lastChainLightning = 0;

    // Restoration tracking
    _earthShieldCharges = 0;
    _tidalWaveStacks = 0;
    _natureSwiftnessReady = 0;

    // Shock rotation
    _lastEarthShock = 0;
    _lastFlameShock = 0;
    _lastFrostShock = 0;
    _shockCooldown = SHOCK_COOLDOWN;
    _shockRotationIndex = 0;

    // Utility tracking
    _lastPurge = 0;
    _lastGrounding = 0;
    _lastTremorTotem = 0;
    _lastBloodlust = 0;
}

void ShamanAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    UpdateTotemManagement();
    UpdateWeaponImbues();

    switch (_specialization)
    {
        case ShamanSpec::ELEMENTAL:
            UpdateElementalRotation(target);
            break;
        case ShamanSpec::ENHANCEMENT:
            UpdateEnhancementRotation(target);
            break;
        case ShamanSpec::RESTORATION:
            UpdateRestorationRotation(target);
            break;
    }

    UpdateShockRotation(target);
}

void ShamanAI::UpdateBuffs()
{
    if (!_bot)
        return;

    UpdateShamanBuffs();
    ApplyWeaponImbues();
    DeployOptimalTotems();
}

void ShamanAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);

    // Update totem timers
    for (auto& totem : _activeTotems)
    {
        if (totem.isActive && totem.remainingTime > 0)
        {
            if (totem.remainingTime <= diff)
                totem.remainingTime = 0;
            else
                totem.remainingTime -= diff;
        }
    }

    // Update weapon imbue timers
    for (auto& imbue : _weaponImbues)
    {
        if (imbue.remainingTime > 0)
        {
            if (imbue.remainingTime <= diff)
                imbue.remainingTime = 0;
            else
                imbue.remainingTime -= diff;
        }
    }

    // Update totem check interval
    if (_lastTotemUpdate <= diff)
        _lastTotemUpdate = 0;
    else
        _lastTotemUpdate -= diff;
}

bool ShamanAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    // Check mana requirements
    if (!HasEnoughResource(spellId))
        return false;

    // Check shock cooldown
    if ((spellId == EARTH_SHOCK || spellId == FLAME_SHOCK || spellId == FROST_SHOCK) && IsShockOnCooldown())
        return false;

    return true;
}

void ShamanAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
    DeployOptimalTotems();
    RefreshWeaponImbue(true);
    RefreshWeaponImbue(false);
}

void ShamanAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();
    _maelstromWeaponStacks = 0;
    _elementalFocusStacks = 0;
    _clearcastingProcs = 0;
}

bool ShamanAI::HasEnoughResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    uint32 manaCost = spellInfo->ManaCost + spellInfo->ManaCostPercentage * GetMaxMana() / 100;
    return GetMana() >= manaCost;
}

void ShamanAI::ConsumeResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    uint32 manaCost = spellInfo->ManaCost + spellInfo->ManaCostPercentage * GetMaxMana() / 100;
    _manaSpent += manaCost;
}

Position ShamanAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    float range = GetOptimalRange(target);
    Position pos = _bot->GetPosition();

    if (_specialization == ShamanSpec::ENHANCEMENT)
    {
        // Melee range for enhancement
        if (_bot->GetDistance(target) > MELEE_RANGE)
        {
            pos = target->GetPosition();
            pos.m_positionX += MELEE_RANGE * cos(target->GetOrientation());
            pos.m_positionY += MELEE_RANGE * sin(target->GetOrientation());
        }
    }
    else
    {
        // Caster range for elemental/restoration
        if (_bot->GetDistance(target) > range || _bot->GetDistance(target) < range * 0.8f)
        {
            pos = target->GetPosition();
            pos.m_positionX += range * cos(target->GetOrientation() + M_PI);
            pos.m_positionY += range * sin(target->GetOrientation() + M_PI);
        }
    }

    return pos;
}

float ShamanAI::GetOptimalRange(::Unit* target)
{
    if (_specialization == ShamanSpec::ENHANCEMENT)
        return MELEE_RANGE;

    return OPTIMAL_CASTING_RANGE;
}

void ShamanAI::UpdateElementalRotation(::Unit* target)
{
    if (!target)
        return;

    // Priority: Lava Burst on cooldown if Flame Shock is up
    if (target->HasAura(FLAME_SHOCK) && CanUseAbility(LAVA_BURST))
    {
        CastLavaBurst(target);
        return;
    }

    // Chain Lightning for multiple targets
    std::vector<::Unit*> enemies = GetChainLightningTargets(target);
    if (enemies.size() >= 3 && CanUseAbility(CHAIN_LIGHTNING))
    {
        CastChainLightning(enemies);
        return;
    }

    // Lightning Bolt as filler
    if (CanUseAbility(LIGHTNING_BOLT))
    {
        CastLightningBolt(target);
    }
}

void ShamanAI::UpdateEnhancementRotation(::Unit* target)
{
    if (!target)
        return;

    // Stormstrike on cooldown
    if (CanUseAbility(STORMSTRIKE))
    {
        CastStormstrike(target);
        return;
    }

    // Lava Lash when available
    if (CanUseAbility(LAVA_LASH))
    {
        CastLavaLash(target);
        return;
    }

    // Use Maelstrom Weapon stacks
    if (_maelstromWeaponStacks >= 5)
    {
        ConsumeMaelstromWeapon();
        return;
    }

    // Melee attacks handled by combat system
}

void ShamanAI::UpdateRestorationRotation(::Unit* target)
{
    if (!target)
        return;

    // Find healing targets first
    ::Unit* healTarget = GetBestHealTarget();
    if (healTarget)
    {
        UseRestorationAbilities();
        return;
    }

    // DPS if no healing needed
    if (CanUseAbility(LIGHTNING_BOLT))
    {
        CastLightningBolt(target);
    }
}

void ShamanAI::UpdateTotemManagement()
{
    uint32 now = getMSTime();
    if (now - _lastTotemUpdate < _totemCheckInterval)
        return;

    _lastTotemUpdate = now;

    // Check for expired totems
    for (auto& totem : _activeTotems)
    {
        if (totem.isActive && totem.remainingTime == 0)
        {
            totem.isActive = false;
            totem.totem = nullptr;
        }
    }

    // Deploy new totems if needed
    if (!_bot->IsInCombat())
    {
        DeployOptimalTotems();
    }
    else
    {
        RefreshExpiringTotems();
    }
}

void ShamanAI::DeployOptimalTotems()
{
    if (!_bot)
        return;

    // Deploy fire totem
    if (!IsTotemActive(TotemType::FIRE))
    {
        uint32 fireTotem = GetOptimalFireTotem();
        if (fireTotem && CanUseAbility(fireTotem))
            DeployTotem(TotemType::FIRE, fireTotem);
    }

    // Deploy earth totem
    if (!IsTotemActive(TotemType::EARTH))
    {
        uint32 earthTotem = GetOptimalEarthTotem();
        if (earthTotem && CanUseAbility(earthTotem))
            DeployTotem(TotemType::EARTH, earthTotem);
    }

    // Deploy water totem
    if (!IsTotemActive(TotemType::WATER))
    {
        uint32 waterTotem = GetOptimalWaterTotem();
        if (waterTotem && CanUseAbility(waterTotem))
            DeployTotem(TotemType::WATER, waterTotem);
    }

    // Deploy air totem
    if (!IsTotemActive(TotemType::AIR))
    {
        uint32 airTotem = GetOptimalAirTotem();
        if (airTotem && CanUseAbility(airTotem))
            DeployTotem(TotemType::AIR, airTotem);
    }
}

void ShamanAI::RefreshExpiringTotems()
{
    for (auto& totem : _activeTotems)
    {
        if (totem.isActive && totem.remainingTime <= TOTEM_REFRESH_THRESHOLD)
        {
            // Redeploy this totem
            DeployTotem(totem.type, totem.spellId);
        }
    }
}

void ShamanAI::DeployTotem(TotemType type, uint32 spellId)
{
    if (!_bot || !CanUseAbility(spellId))
        return;

    // Recall existing totem of this type
    if (IsTotemActive(type))
        RecallTotem(type);

    // Cast the totem spell
    _bot->CastSpell(_bot, spellId, false);

    // Update tracking
    TotemInfo& totem = _activeTotems[static_cast<uint8>(type)];
    totem.spellId = spellId;
    totem.type = type;
    totem.position = _bot->GetPosition();
    totem.duration = 300000; // 5 minutes default
    totem.remainingTime = totem.duration;
    totem.isActive = true;
    totem.lastPulse = getMSTime();
    totem.effectRadius = TOTEM_EFFECT_RADIUS;

    _totemsDeploy++;
    ConsumeResource(spellId);
}

void ShamanAI::RecallTotem(TotemType type)
{
    TotemInfo& totem = _activeTotems[static_cast<uint8>(type)];
    if (!totem.isActive)
        return;

    // Find and destroy the totem
    if (totem.totem)
    {
        totem.totem->ToTotem()->UnSummon();
    }

    totem.isActive = false;
    totem.totem = nullptr;
    totem.remainingTime = 0;
}

bool ShamanAI::IsTotemActive(TotemType type)
{
    const TotemInfo& totem = _activeTotems[static_cast<uint8>(type)];
    return totem.isActive && totem.remainingTime > 0;
}

uint32 ShamanAI::GetOptimalFireTotem()
{
    if (_bot->IsInCombat())
    {
        if (_specialization == ShamanSpec::ELEMENTAL)
            return TOTEM_OF_WRATH;
        else
            return SEARING_TOTEM;
    }

    return FLAMETONGUE_TOTEM;
}

uint32 ShamanAI::GetOptimalEarthTotem()
{
    if (_bot->IsInCombat())
    {
        return TREMOR_TOTEM;
    }

    return STRENGTH_OF_EARTH_TOTEM;
}

uint32 ShamanAI::GetOptimalWaterTotem()
{
    if (_specialization == ShamanSpec::RESTORATION)
        return HEALING_STREAM_TOTEM;

    return MANA_SPRING_TOTEM;
}

uint32 ShamanAI::GetOptimalAirTotem()
{
    if (_specialization == ShamanSpec::ENHANCEMENT)
        return WINDFURY_TOTEM;

    return WRATH_OF_AIR_TOTEM;
}

void ShamanAI::UpdateWeaponImbues()
{
    uint32 now = getMSTime();

    // Check main hand weapon imbue
    if (_weaponImbues[0].remainingTime == 0 || now - _lastFlametongueRefresh > WEAPON_IMBUE_CHECK_INTERVAL)
    {
        RefreshWeaponImbue(true);
        _lastFlametongueRefresh = now;
    }

    // Check off hand weapon imbue if dual wielding
    if (_dualWielding && (_weaponImbues[1].remainingTime == 0 || now - _lastWindfuryRefresh > WEAPON_IMBUE_CHECK_INTERVAL))
    {
        RefreshWeaponImbue(false);
        _lastWindfuryRefresh = now;
    }
}

void ShamanAI::ApplyWeaponImbues()
{
    if (_specialization != ShamanSpec::ENHANCEMENT)
        return;

    if (!HasWeaponImbue(true))
        CastFlametongueWeapon();

    if (_dualWielding && !HasWeaponImbue(false))
        CastWindfuryWeapon();
}

void ShamanAI::RefreshWeaponImbue(bool mainHand)
{
    if (_specialization != ShamanSpec::ENHANCEMENT)
        return;

    if (mainHand)
    {
        if (CanUseAbility(FLAMETONGUE_WEAPON))
            CastFlametongueWeapon();
    }
    else if (_dualWielding)
    {
        if (CanUseAbility(WINDFURY_WEAPON))
            CastWindfuryWeapon();
    }
}

bool ShamanAI::HasWeaponImbue(bool mainHand)
{
    size_t index = mainHand ? 0 : 1;
    return _weaponImbues[index].remainingTime > 0;
}

void ShamanAI::UpdateShockRotation(::Unit* target)
{
    if (!target || IsShockOnCooldown())
        return;

    uint32 shockSpell = GetNextShockSpell(target);
    if (shockSpell && CanUseAbility(shockSpell))
    {
        switch (shockSpell)
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

        _shocksUsed++;
    }
}

uint32 ShamanAI::GetNextShockSpell(::Unit* target)
{
    if (!target)
        return 0;

    // Flame Shock if not applied or about to expire
    if (!target->HasAura(FLAME_SHOCK) || target->GetRemainingTimeOnAura(FLAME_SHOCK) < 3000)
        return FLAME_SHOCK;

    // Earth Shock for damage/interrupt
    if (_specialization == ShamanSpec::ELEMENTAL)
        return EARTH_SHOCK;

    // Frost Shock for slowing
    if (_specialization == ShamanSpec::ENHANCEMENT && target->GetDistance(_bot) > MELEE_RANGE)
        return FROST_SHOCK;

    return EARTH_SHOCK;
}

bool ShamanAI::IsShockOnCooldown()
{
    uint32 now = getMSTime();
    return (now - _lastEarthShock < _shockCooldown) ||
           (now - _lastFlameShock < _shockCooldown) ||
           (now - _lastFrostShock < _shockCooldown);
}

void ShamanAI::UpdateShamanBuffs()
{
    // Lightning Shield for elemental/enhancement
    if (_specialization != ShamanSpec::RESTORATION && !_bot->HasAura(LIGHTNING_SHIELD))
        CastLightningShield();

    // Water Shield for restoration
    if (_specialization == ShamanSpec::RESTORATION && !_bot->HasAura(WATER_SHIELD))
        CastWaterShield();
}

void ShamanAI::UseElementalAbilities(::Unit* target)
{
    if (!target)
        return;

    if (CanUseAbility(LAVA_BURST))
        CastLavaBurst(target);
    else if (CanUseAbility(LIGHTNING_BOLT))
        CastLightningBolt(target);
}

void ShamanAI::UseEnhancementAbilities(::Unit* target)
{
    if (!target)
        return;

    if (CanUseAbility(STORMSTRIKE))
        CastStormstrike(target);
    else if (CanUseAbility(LAVA_LASH))
        CastLavaLash(target);
}

void ShamanAI::UseRestorationAbilities()
{
    ::Unit* healTarget = GetBestHealTarget();
    if (!healTarget)
        return;

    float healthPercent = healTarget->GetHealthPct();

    if (healthPercent < 30.0f && CanUseAbility(LESSER_HEALING_WAVE))
        CastLesserHealingWave(healTarget);
    else if (healthPercent < 60.0f && CanUseAbility(HEALING_WAVE))
        CastHealingWave(healTarget);
    else if (CanUseAbility(RIPTIDE))
        CastRiptide(healTarget);
}

::Unit* ShamanAI::GetBestHealTarget()
{
    ::Unit* lowestTarget = nullptr;
    float lowestHealth = 100.0f;

    // Check self first
    if (_bot->GetHealthPct() < 70.0f)
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
                if (player->GetHealthPct() < lowestHealth && player->GetDistance(_bot) <= OPTIMAL_CASTING_RANGE)
                {
                    lowestTarget = player;
                    lowestHealth = player->GetHealthPct();
                }
            }
        }
    }

    return lowestTarget;
}

std::vector<::Unit*> ShamanAI::GetChainLightningTargets(::Unit* primary)
{
    std::vector<::Unit*> targets;
    targets.push_back(primary);

    // Find nearby enemies for chain lightning
    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(_bot, _bot, 15.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, check);
    Cell::VisitAllObjects(_bot, searcher, 15.0f);

    for (Unit* enemy : nearbyEnemies)
    {
        if (enemy != primary && targets.size() < 3)
            targets.push_back(enemy);
    }

    return targets;
}

uint32 ShamanAI::GetMana()
{
    return _bot ? _bot->GetPower(POWER_MANA) : 0;
}

uint32 ShamanAI::GetMaxMana()
{
    return _bot ? _bot->GetMaxPower(POWER_MANA) : 0;
}

float ShamanAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float)GetMana() / maxMana : 0.0f;
}

bool ShamanAI::HasEnoughMana(uint32 amount)
{
    return GetMana() >= amount;
}

ShamanSpec ShamanAI::DetectSpecialization()
{
    if (!_bot)
        return ShamanSpec::ELEMENTAL;

    // Simple detection based on weapon and spells
    if (_bot->HasSpell(STORMSTRIKE) || _bot->HasSpell(LAVA_LASH))
        return ShamanSpec::ENHANCEMENT;

    if (_bot->HasSpell(CHAIN_HEAL) || _bot->HasSpell(RIPTIDE))
        return ShamanSpec::RESTORATION;

    return ShamanSpec::ELEMENTAL;
}

bool ShamanAI::IsDualWielding()
{
    if (!_bot)
        return false;

    Item* mainHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    return mainHand && offHand && offHand->GetTemplate()->SubClass != ITEM_SUBCLASS_ARMOR_SHIELD;
}

// Combat ability implementations
void ShamanAI::CastLightningBolt(::Unit* target)
{
    if (!target || !CanUseAbility(LIGHTNING_BOLT))
        return;

    _bot->CastSpell(target, LIGHTNING_BOLT, false);
    _lastLightningBolt = getMSTime();
    ConsumeResource(LIGHTNING_BOLT);
}

void ShamanAI::CastChainLightning(const std::vector<::Unit*>& enemies)
{
    if (enemies.empty() || !CanUseAbility(CHAIN_LIGHTNING))
        return;

    _bot->CastSpell(enemies[0], CHAIN_LIGHTNING, false);
    _lastChainLightning = getMSTime();
    ConsumeResource(CHAIN_LIGHTNING);
}

void ShamanAI::CastLavaBurst(::Unit* target)
{
    if (!target || !CanUseAbility(LAVA_BURST))
        return;

    _bot->CastSpell(target, LAVA_BURST, false);
    ConsumeResource(LAVA_BURST);
}

void ShamanAI::CastStormstrike(::Unit* target)
{
    if (!target || !CanUseAbility(STORMSTRIKE))
        return;

    _bot->CastSpell(target, STORMSTRIKE, false);
    ConsumeResource(STORMSTRIKE);
}

void ShamanAI::CastLavaLash(::Unit* target)
{
    if (!target || !CanUseAbility(LAVA_LASH))
        return;

    _bot->CastSpell(target, LAVA_LASH, false);
    ConsumeResource(LAVA_LASH);
}

void ShamanAI::CastEarthShock(::Unit* target)
{
    if (!target || !CanUseAbility(EARTH_SHOCK))
        return;

    _bot->CastSpell(target, EARTH_SHOCK, false);
    _lastEarthShock = getMSTime();
    ConsumeResource(EARTH_SHOCK);
}

void ShamanAI::CastFlameShock(::Unit* target)
{
    if (!target || !CanUseAbility(FLAME_SHOCK))
        return;

    _bot->CastSpell(target, FLAME_SHOCK, false);
    _lastFlameShock = getMSTime();
    ConsumeResource(FLAME_SHOCK);
}

void ShamanAI::CastFrostShock(::Unit* target)
{
    if (!target || !CanUseAbility(FROST_SHOCK))
        return;

    _bot->CastSpell(target, FROST_SHOCK, false);
    _lastFrostShock = getMSTime();
    ConsumeResource(FROST_SHOCK);
}

void ShamanAI::CastHealingWave(::Unit* target)
{
    if (!target || !CanUseAbility(HEALING_WAVE))
        return;

    _bot->CastSpell(target, HEALING_WAVE, false);
    ConsumeResource(HEALING_WAVE);
}

void ShamanAI::CastLesserHealingWave(::Unit* target)
{
    if (!target || !CanUseAbility(LESSER_HEALING_WAVE))
        return;

    _bot->CastSpell(target, LESSER_HEALING_WAVE, false);
    ConsumeResource(LESSER_HEALING_WAVE);
}

void ShamanAI::CastChainHeal(::Unit* target)
{
    if (!target || !CanUseAbility(CHAIN_HEAL))
        return;

    _bot->CastSpell(target, CHAIN_HEAL, false);
    ConsumeResource(CHAIN_HEAL);
}

void ShamanAI::CastRiptide(::Unit* target)
{
    if (!target || !CanUseAbility(RIPTIDE))
        return;

    _bot->CastSpell(target, RIPTIDE, false);
    ConsumeResource(RIPTIDE);
}

void ShamanAI::CastLightningShield()
{
    if (!CanUseAbility(LIGHTNING_SHIELD))
        return;

    _bot->CastSpell(_bot, LIGHTNING_SHIELD, false);
    ConsumeResource(LIGHTNING_SHIELD);
}

void ShamanAI::CastWaterShield()
{
    if (!CanUseAbility(WATER_SHIELD))
        return;

    _bot->CastSpell(_bot, WATER_SHIELD, false);
    ConsumeResource(WATER_SHIELD);
}

void ShamanAI::CastFlametongueWeapon()
{
    if (!CanUseAbility(FLAMETONGUE_WEAPON))
        return;

    _bot->CastSpell(_bot, FLAMETONGUE_WEAPON, false);
    _weaponImbues[0] = WeaponImbue(FLAMETONGUE_WEAPON, 3600000, 0, true); // 1 hour
    ConsumeResource(FLAMETONGUE_WEAPON);
}

void ShamanAI::CastWindfuryWeapon()
{
    if (!CanUseAbility(WINDFURY_WEAPON))
        return;

    _bot->CastSpell(_bot, WINDFURY_WEAPON, false);
    _weaponImbues[1] = WeaponImbue(WINDFURY_WEAPON, 3600000, 0, false); // 1 hour
    ConsumeResource(WINDFURY_WEAPON);
}

void ShamanAI::ConsumeMaelstromWeapon()
{
    if (_maelstromWeaponStacks == 0)
        return;

    // Use instant Lightning Bolt with Maelstrom Weapon
    if (CanUseAbility(LIGHTNING_BOLT))
    {
        CastLightningBolt(GetTarget());
        _maelstromWeaponStacks = 0;
    }
}

void ShamanAI::RecordDamageDealt(uint32 damage, ::Unit* target)
{
    _damageDealt += damage;
}

void ShamanAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
}

// Utility class implementations
uint32 ShamanSpellCalculator::CalculateLightningBoltDamage(Player* caster, ::Unit* target)
{
    return 1000; // Placeholder
}

uint32 ShamanSpellCalculator::CalculateChainLightningDamage(Player* caster, ::Unit* target, uint32 jumpNumber)
{
    return 800; // Placeholder
}

uint32 ShamanSpellCalculator::CalculateShockDamage(uint32 shockSpell, Player* caster, ::Unit* target)
{
    return 600; // Placeholder
}

uint32 ShamanSpellCalculator::CalculateHealingWaveDamage(Player* caster, ::Unit* target)
{
    return 1200; // Placeholder
}

uint32 ShamanSpellCalculator::CalculateChainHealAmount(Player* caster, ::Unit* target, uint32 jumpNumber)
{
    return 1000; // Placeholder
}

float ShamanSpellCalculator::CalculateTotemEffectiveness(uint32 totemSpell, const std::vector<::Unit*>& affectedUnits)
{
    return 1.0f; // Placeholder
}

Position ShamanSpellCalculator::GetOptimalTotemPosition(TotemType type, const std::vector<::Unit*>& allies)
{
    return Position(); // Placeholder
}

bool ShamanSpellCalculator::ShouldReplaceTotem(uint32 currentTotem, uint32 newTotem, Player* caster)
{
    return newTotem != currentTotem; // Placeholder
}

uint32 ShamanSpellCalculator::CalculateStormstrikeDamage(Player* caster, ::Unit* target)
{
    return 1500; // Placeholder
}

uint32 ShamanSpellCalculator::CalculateLavaLashDamage(Player* caster, ::Unit* target)
{
    return 1200; // Placeholder
}

float ShamanSpellCalculator::CalculateWeaponImbueEffectiveness(uint32 imbueSpell, Player* caster)
{
    return 1.0f; // Placeholder
}

float ShamanSpellCalculator::CalculateSpellManaEfficiency(uint32 spellId, Player* caster)
{
    return 1.0f; // Placeholder
}

uint32 ShamanSpellCalculator::GetOptimalSpellForMana(Player* caster, ::Unit* target, uint32 availableMana)
{
    return 0; // Placeholder
}

void ShamanSpellCalculator::CacheShamanData()
{
    // Cache implementation
}

// TotemController implementation
TotemController::TotemController(ShamanAI* owner) : _owner(owner), _lastUpdate(0)
{
    for (auto& totem : _totems)
        totem = TotemInfo();
}

void TotemController::Update(uint32 diff)
{
    _lastUpdate += diff;

    if (_lastUpdate >= 1000) // Update every second
    {
        UpdateTotemStates();
        CheckTotemEffectiveness();
        _lastUpdate = 0;
    }
}

void TotemController::DeployTotem(TotemType type, uint32 spellId, const Position& position)
{
    if (!_owner)
        return;

    TotemInfo& totem = _totems[static_cast<uint8>(type)];
    totem.spellId = spellId;
    totem.type = type;
    totem.position = position;
    totem.isActive = true;
    totem.duration = 300000; // 5 minutes
    totem.remainingTime = totem.duration;
}

void TotemController::RecallTotem(TotemType type)
{
    TotemInfo& totem = _totems[static_cast<uint8>(type)];
    totem.isActive = false;
    totem.remainingTime = 0;
}

void TotemController::RecallAllTotems()
{
    for (auto& totem : _totems)
    {
        if (totem.isActive)
        {
            totem.isActive = false;
            totem.remainingTime = 0;
        }
    }
}

bool TotemController::IsTotemActive(TotemType type) const
{
    const TotemInfo& totem = _totems[static_cast<uint8>(type)];
    return totem.isActive && totem.remainingTime > 0;
}

uint32 TotemController::GetTotemRemainingTime(TotemType type) const
{
    const TotemInfo& totem = _totems[static_cast<uint8>(type)];
    return totem.remainingTime;
}

Position TotemController::GetTotemPosition(TotemType type) const
{
    const TotemInfo& totem = _totems[static_cast<uint8>(type)];
    return totem.position;
}

void TotemController::UpdateTotemStates()
{
    // Update totem states and timers
}

void TotemController::CheckTotemEffectiveness()
{
    // Analyze totem performance
}

void TotemController::OptimizeTotemPlacement()
{
    // Find optimal positions for totems
}

bool TotemController::ShouldReplaceTotem(TotemType type, uint32 newSpellId)
{
    const TotemInfo& totem = _totems[static_cast<uint8>(type)];
    return !totem.isActive || totem.spellId != newSpellId;
}

void TotemController::SetTotemStrategy(const std::vector<uint32>& totemPriorities)
{
    _deploymentPriorities = totemPriorities;
}

void TotemController::AdaptToSituation(bool inCombat, bool groupHealing, bool needsUtility)
{
    // Adapt totem strategy based on situation
}

// Cache static variables
std::unordered_map<uint32, uint32> ShamanSpellCalculator::_spellDamageCache;
std::unordered_map<TotemType, Position> ShamanSpellCalculator::_totemPositionCache;
std::mutex ShamanSpellCalculator::_cacheMutex;

} // namespace Playerbot