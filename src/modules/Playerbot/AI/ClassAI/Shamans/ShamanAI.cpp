/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShamanAI.h"
#include "ElementalSpecialization.h"
#include "EnhancementSpecialization.h"
#include "RestorationSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Group.h"
#include "Item.h"
#include "MotionMaster.h"
#include "Log.h"
#include <algorithm>
#include <chrono>

namespace Playerbot
{

// Totem spell definitions for quick reference
enum TotemSpells
{
    // Fire Totems
    SPELL_SEARING_TOTEM = 3599,
    SPELL_FIRE_NOVA_TOTEM = 1535,
    SPELL_MAGMA_TOTEM = 8190,
    SPELL_FLAMETONGUE_TOTEM = 8227,
    SPELL_TOTEM_OF_WRATH = 30706,
    SPELL_FIRE_ELEMENTAL_TOTEM = 2894,

    // Earth Totems
    SPELL_EARTHBIND_TOTEM = 2484,
    SPELL_STONESKIN_TOTEM = 8071,
    SPELL_STONECLAW_TOTEM = 5730,
    SPELL_STRENGTH_OF_EARTH_TOTEM = 8075,
    SPELL_TREMOR_TOTEM = 8143,
    SPELL_EARTH_ELEMENTAL_TOTEM = 2062,

    // Water Totems
    SPELL_HEALING_STREAM_TOTEM = 5394,
    SPELL_MANA_SPRING_TOTEM = 5675,
    SPELL_POISON_CLEANSING_TOTEM = 8166,
    SPELL_DISEASE_CLEANSING_TOTEM = 8170,
    SPELL_FIRE_RESISTANCE_TOTEM = 8184,
    SPELL_MANA_TIDE_TOTEM = 16190,

    // Air Totems
    SPELL_GROUNDING_TOTEM = 8177,
    SPELL_NATURE_RESISTANCE_TOTEM = 10595,
    SPELL_WINDFURY_TOTEM = 8512,
    SPELL_GRACE_OF_AIR_TOTEM = 8835,
    SPELL_WRATH_OF_AIR_TOTEM = 3738,
    SPELL_SENTRY_TOTEM = 6495
};

// Shock spell definitions
enum ShockSpells
{
    SPELL_EARTH_SHOCK = 8042,
    SPELL_FLAME_SHOCK = 8050,
    SPELL_FROST_SHOCK = 8056
};

// Shield spell definitions
enum ShieldSpells
{
    SPELL_LIGHTNING_SHIELD = 192106, // Updated for WoW 11.2
    SPELL_WATER_SHIELD = 52127,
    SPELL_EARTH_SHIELD = 974
};

// Weapon imbue spell definitions
enum WeaponImbues
{
    SPELL_ROCKBITER_WEAPON = 8017,
    SPELL_FLAMETONGUE_WEAPON = 8024,
    SPELL_FROSTBRAND_WEAPON = 8033,
    SPELL_WINDFURY_WEAPON = 8232,
    SPELL_EARTHLIVING_WEAPON = 51730
};

// Utility spell definitions
enum UtilitySpells
{
    SPELL_PURGE = 370,
    SPELL_HEX = 51514,
    SPELL_BLOODLUST = 2825,
    SPELL_HEROISM = 32182,
    SPELL_GHOST_WOLF = 2645,
    SPELL_ANCESTRAL_SPIRIT = 2008,
    SPELL_WATER_WALKING = 546,
    SPELL_WATER_BREATHING = 131,
    SPELL_ASTRAL_RECALL = 556
};

// Healing spell definitions
enum HealingSpells
{
    SPELL_HEALING_WAVE = 331,
    SPELL_LESSER_HEALING_WAVE = 8004,
    SPELL_CHAIN_HEAL = 1064,
    SPELL_RIPTIDE = 61295,
    SPELL_HEALING_RAIN = 73920
};

// Damage spell definitions
enum DamageSpells
{
    SPELL_LIGHTNING_BOLT = 403,
    SPELL_CHAIN_LIGHTNING = 421,
    SPELL_LAVA_BURST = 51505,
    SPELL_THUNDERSTORM = 51490,
    SPELL_EARTHQUAKE = 61882
};

// Enhancement-specific spells
enum EnhancementSpells
{
    SPELL_STORMSTRIKE = 17364,
    SPELL_LAVA_LASH = 60103,
    SPELL_SHAMANISTIC_RAGE = 30823,
    SPELL_FERAL_SPIRIT = 51533
};

// Talent IDs for specialization detection
enum ShamanTalents
{
    TALENT_ELEMENTAL_FOCUS = 16164,
    TALENT_ELEMENTAL_MASTERY = 16166,
    TALENT_LIGHTNING_OVERLOAD = 30675,
    TALENT_TOTEM_OF_WRATH_TALENT = 30706,

    TALENT_DUAL_WIELD = 30798,
    TALENT_STORMSTRIKE_TALENT = 17364,
    TALENT_SHAMANISTIC_RAGE_TALENT = 30823,
    TALENT_MAELSTROM_WEAPON = 51530,

    TALENT_NATURES_SWIFTNESS = 16188,
    TALENT_MANA_TIDE_TOTEM_TALENT = 16190,
    TALENT_EARTH_SHIELD_TALENT = 974,
    TALENT_RIPTIDE_TALENT = 61295
};

// Combat constants
static const float OPTIMAL_CASTER_RANGE = 30.0f;
static const float OPTIMAL_MELEE_RANGE = 5.0f;
static const float TOTEM_PLACEMENT_RANGE = 20.0f;
static const uint32 SHOCK_GLOBAL_COOLDOWN = 1500;
static const uint32 TOTEM_UPDATE_INTERVAL = 2000;
static const uint32 SHIELD_REFRESH_TIME = 540000; // 9 minutes
static const uint32 WEAPON_IMBUE_DURATION = 1800000; // 30 minutes

ShamanAI::ShamanAI(Player* bot) : ClassAI(bot),
    _currentSpec(ShamanSpec::ELEMENTAL),
    _manaSpent(0),
    _damageDealt(0),
    _healingDone(0),
    _totemsDeploy(0),
    _shocksUsed(0)
{
    InitializeSpecialization();

    TC_LOG_DEBUG("module.playerbot.ai", "ShamanAI created for player {} with specialization {}",
                 bot ? bot->GetName() : "null",
                 _specialization ? _specialization->GetSpecializationName() : "none");
}

void ShamanAI::UpdateRotation(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    // Check if we need to switch specialization
    ShamanSpec newSpec = DetectCurrentSpecialization();
    if (newSpec != _currentSpec)
    {
        SwitchSpecialization(newSpec);
    }

    // Update shared shaman mechanics
    UpdateTotemCheck();
    UpdateShockRotation();

    // Delegate to specialization for main rotation
    DelegateToSpecialization(target);

    // Track combat metrics
    if (GetBot()->IsInCombat())
    {
        _damageDealt += CalculateDamageDealt(target);
        _healingDone += CalculateHealingDone();
        _manaSpent += CalculateManaUsage();
    }
}

void ShamanAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    // Update shields first
    UpdateShamanBuffs();

    // Check weapon imbues
    UpdateWeaponImbues();

    // Delegate additional buffs to specialization
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }

    // Water walking/breathing utility
    if (!GetBot()->IsInCombat())
    {
        UpdateUtilityBuffs();
    }
}

void ShamanAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update ability cooldowns
    for (auto& [spellId, lastUse] : _abilityUsage)
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        {
            // Update cooldown tracking
            if (lastUse > 0 && lastUse < diff)
            {
                lastUse = 0;
            }
            else if (lastUse > 0)
            {
                lastUse -= diff;
            }
        }
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

bool ShamanAI::CanUseAbility(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Check if spell is learned
    if (!GetBot()->HasSpell(spellId))
        return false;

    // Check if spell is ready
    if (!IsSpellReady(spellId))
        return false;

    // Check resource requirements
    if (!HasEnoughResource(spellId))
        return false;

    // Check specialization-specific requirements
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

void ShamanAI::OnCombatStart(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} entering combat with {}",
                 GetBot()->GetName(), target->GetName());

    // Deploy initial totems
    DeployInitialTotems(target);

    // Apply initial buffs
    ApplyCombatBuffs();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    // Initialize combat tracking
    _combatTime = 0;
    _inCombat = true;
    _currentTarget = target;
}

void ShamanAI::OnCombatEnd()
{
    if (!GetBot())
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} leaving combat. Metrics - Damage: {}, Healing: {}, Mana Used: {}, Totems: {}, Shocks: {}",
                 GetBot()->GetName(), _damageDealt, _healingDone, _manaSpent, _totemsDeploy, _shocksUsed);

    // Recall unnecessary totems
    RecallCombatTotems();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }

    // Reset combat tracking
    _inCombat = false;
    _currentTarget = nullptr;

    // Log performance metrics
    LogCombatMetrics();
}

bool ShamanAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA && GetBot()->GetPower(POWER_MANA) < int32(cost.Amount))
            return false;
    }

    // Delegate additional checks to specialization
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    return true;
}

void ShamanAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Track mana consumption
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
            _manaSpent += cost.Amount;
    }

    // Track ability usage
    _abilityUsage[spellId] = getMSTime();

    // Track specific spell categories
    if (IsShockSpell(spellId))
    {
        _shocksUsed++;
    }
    else if (IsTotemSpell(spellId))
    {
        _totemsDeploy++;
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

Position ShamanAI::GetOptimalPosition(::Unit* target)
{
    if (!GetBot() || !target)
        return Position();

    // Delegate to specialization for position preference
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    // Default positioning based on spec
    float optimalRange = GetOptimalRange(target);
    float angle = GetBot()->GetAbsoluteAngle(target);
    float x = target->GetPositionX() - optimalRange * std::cos(angle);
    float y = target->GetPositionY() - optimalRange * std::sin(angle);
    float z = target->GetPositionZ();

    return Position(x, y, z);
}

float ShamanAI::GetOptimalRange(::Unit* target)
{
    if (!GetBot() || !target)
        return OPTIMAL_CASTER_RANGE;

    // Delegate to specialization for range preference
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    // Default range based on spec
    switch (_currentSpec)
    {
        case ShamanSpec::ENHANCEMENT:
            return OPTIMAL_MELEE_RANGE;
        case ShamanSpec::ELEMENTAL:
        case ShamanSpec::RESTORATION:
        default:
            return OPTIMAL_CASTER_RANGE;
    }
}

void ShamanAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

ShamanSpec ShamanAI::DetectCurrentSpecialization()
{
    if (!GetBot())
        return ShamanSpec::ELEMENTAL;

    // Check for key Restoration talents
    if (GetBot()->HasSpell(TALENT_EARTH_SHIELD_TALENT) ||
        GetBot()->HasSpell(TALENT_RIPTIDE_TALENT) ||
        GetBot()->HasSpell(TALENT_MANA_TIDE_TOTEM_TALENT))
    {
        return ShamanSpec::RESTORATION;
    }

    // Check for key Enhancement talents
    if (GetBot()->HasSpell(TALENT_STORMSTRIKE_TALENT) ||
        GetBot()->HasSpell(TALENT_DUAL_WIELD) ||
        GetBot()->HasSpell(TALENT_MAELSTROM_WEAPON))
    {
        return ShamanSpec::ENHANCEMENT;
    }

    // Check for key Elemental talents
    if (GetBot()->HasSpell(TALENT_ELEMENTAL_MASTERY) ||
        GetBot()->HasSpell(TALENT_LIGHTNING_OVERLOAD) ||
        GetBot()->HasSpell(TALENT_TOTEM_OF_WRATH_TALENT))
    {
        return ShamanSpec::ELEMENTAL;
    }

    // Default to Elemental if no clear specialization
    return ShamanSpec::ELEMENTAL;
}

void ShamanAI::SwitchSpecialization(ShamanSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;
    _specialization.reset();

    switch (newSpec)
    {
        case ShamanSpec::ELEMENTAL:
            _specialization = std::make_unique<ElementalSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} switching to Elemental specialization", GetBot()->GetName());
            break;
        case ShamanSpec::ENHANCEMENT:
            _specialization = std::make_unique<EnhancementSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} switching to Enhancement specialization", GetBot()->GetName());
            break;
        case ShamanSpec::RESTORATION:
            _specialization = std::make_unique<RestorationSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} switching to Restoration specialization", GetBot()->GetName());
            break;
    }
}

void ShamanAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization || !target)
        return;

    _specialization->UpdateRotation(target);
}

void ShamanAI::UpdateShamanBuffs()
{
    if (!GetBot())
        return;

    // Lightning Shield for Elemental/Enhancement
    if (_currentSpec != ShamanSpec::RESTORATION)
    {
        if (!HasAura(SPELL_LIGHTNING_SHIELD, GetBot()))
        {
            if (CastSpell(SPELL_LIGHTNING_SHIELD))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Lightning Shield", GetBot()->GetName());
            }
        }
    }
    // Water Shield for Restoration
    else
    {
        if (!HasAura(SPELL_WATER_SHIELD, GetBot()))
        {
            if (CastSpell(SPELL_WATER_SHIELD))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Water Shield", GetBot()->GetName());
            }
        }
    }

    // Earth Shield on tank in group
    if (_currentSpec == ShamanSpec::RESTORATION && GetBot()->HasSpell(SPELL_EARTH_SHIELD))
    {
        if (Group* group = GetBot()->GetGroup())
        {
            Player* tank = FindGroupTank(group);
            if (tank && !HasAura(SPELL_EARTH_SHIELD, tank))
            {
                if (CastSpell(tank, SPELL_EARTH_SHIELD))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Earth Shield on tank {}",
                                 GetBot()->GetName(), tank->GetName());
                }
            }
        }
    }
}

void ShamanAI::UpdateTotemCheck()
{
    if (!GetBot())
        return;

    static uint32 lastTotemCheck = 0;
    uint32 currentTime = getMSTime();

    if (currentTime - lastTotemCheck < TOTEM_UPDATE_INTERVAL)
        return;

    lastTotemCheck = currentTime;

    // Check if totems need refreshing
    if (_specialization)
    {
        _specialization->UpdateTotemManagement();
    }
}

void ShamanAI::UpdateShockRotation()
{
    if (!GetBot() || !_currentTarget)
        return;

    // Delegate shock rotation to specialization
    if (_specialization)
    {
        _specialization->UpdateShockRotation(_currentTarget);
    }
}

// Private helper methods
void ShamanAI::UpdateWeaponImbues()
{
    if (!GetBot())
        return;

    // Check main hand weapon imbue
    if (!HasWeaponImbue(true))
    {
        uint32 imbueSpell = GetOptimalWeaponImbue(true);
        if (imbueSpell && CastSpell(imbueSpell))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} applying weapon imbue {} to main hand",
                         GetBot()->GetName(), imbueSpell);
        }
    }

    // Check off-hand weapon imbue for Enhancement
    if (_currentSpec == ShamanSpec::ENHANCEMENT && !HasWeaponImbue(false))
    {
        uint32 imbueSpell = GetOptimalWeaponImbue(false);
        if (imbueSpell && CastSpell(imbueSpell))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} applying weapon imbue {} to off-hand",
                         GetBot()->GetName(), imbueSpell);
        }
    }
}

void ShamanAI::UpdateUtilityBuffs()
{
    if (!GetBot())
        return;

    // Water walking when near water
    if (NearWater() && !HasAura(SPELL_WATER_WALKING, GetBot()))
    {
        CastSpell(SPELL_WATER_WALKING);
    }

    // Ghost Wolf for movement speed when traveling
    if (GetBot()->isMoving() && !GetBot()->IsInCombat() && !HasAura(SPELL_GHOST_WOLF, GetBot()))
    {
        // Use Ghost Wolf for long-distance travel
        CastSpell(SPELL_GHOST_WOLF);
    }
}

void ShamanAI::DeployInitialTotems(::Unit* target)
{
    if (!GetBot() || !target || !_specialization)
        return;

    _specialization->DeployOptimalTotems();
}

void ShamanAI::RecallCombatTotems()
{
    // Totems automatically despawn, but we can track their removal
    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} combat ended, totems will expire naturally",
                 GetBot()->GetName());
}

void ShamanAI::ApplyCombatBuffs()
{
    if (!GetBot())
        return;

    // Bloodlust/Heroism in boss fights or on request
    if (ShouldUseBloodlust())
    {
        uint32 spellId = GetBot()->GetTeamId() == TEAM_ALLIANCE ? SPELL_HEROISM : SPELL_BLOODLUST;
        if (CastSpell(spellId))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Bloodlust/Heroism", GetBot()->GetName());
        }
    }
}

void ShamanAI::LogCombatMetrics()
{
    TC_LOG_DEBUG("module.playerbot.ai",
                 "Shaman {} combat metrics - Duration: {}s, Damage: {}, Healing: {}, Mana: {}, Totems: {}, Shocks: {}",
                 GetBot()->GetName(), _combatTime / 1000, _damageDealt, _healingDone,
                 _manaSpent, _totemsDeploy, _shocksUsed);

    // Reset metrics for next combat
    _damageDealt = 0;
    _healingDone = 0;
    _manaSpent = 0;
    _totemsDeploy = 0;
    _shocksUsed = 0;
}

bool ShamanAI::IsShockSpell(uint32 spellId) const
{
    return spellId == SPELL_EARTH_SHOCK ||
           spellId == SPELL_FLAME_SHOCK ||
           spellId == SPELL_FROST_SHOCK;
}

bool ShamanAI::IsTotemSpell(uint32 spellId) const
{
    // Check if spell is a totem spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check spell effects for totem summoning
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->GetEffect(SpellEffIndex(i)).Effect == SPELL_EFFECT_SUMMON)
        {
            // Additional check for totem-specific summons could be added here
            return true;
        }
    }

    // Check against known totem spell IDs
    return (spellId >= SPELL_SEARING_TOTEM && spellId <= SPELL_SENTRY_TOTEM) ||
           spellId == SPELL_FIRE_ELEMENTAL_TOTEM ||
           spellId == SPELL_EARTH_ELEMENTAL_TOTEM ||
           spellId == SPELL_MANA_TIDE_TOTEM;
}

uint32 ShamanAI::GetOptimalWeaponImbue(bool mainHand) const
{
    if (!GetBot())
        return 0;

    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
            return SPELL_FLAMETONGUE_WEAPON;
        case ShamanSpec::ENHANCEMENT:
            return mainHand ? SPELL_WINDFURY_WEAPON : SPELL_FLAMETONGUE_WEAPON;
        case ShamanSpec::RESTORATION:
            return SPELL_EARTHLIVING_WEAPON;
        default:
            return SPELL_ROCKBITER_WEAPON;
    }
}

bool ShamanAI::HasWeaponImbue(bool mainHand) const
{
    if (!GetBot())
        return false;

    // Check for active weapon enchantment
    Item* weapon = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, mainHand ? EQUIPMENT_SLOT_MAINHAND : EQUIPMENT_SLOT_OFFHAND);
    if (!weapon)
        return false;

    // Check if weapon has temporary enchantment (imbue)
    return weapon->GetEnchantmentId(EnchantmentSlot(TEMP_ENCHANTMENT_SLOT)) != 0;
}

bool ShamanAI::NearWater() const
{
    if (!GetBot())
        return false;

    // Check if bot is near water
    return GetBot()->GetMap()->IsInWater(GetBot()->GetPhaseShift(), GetBot()->GetPositionX(), GetBot()->GetPositionY(), GetBot()->GetPositionZ());
}

bool ShamanAI::ShouldUseBloodlust() const
{
    if (!GetBot())
        return false;

    // Check if already has exhaustion debuff
    if (GetBot()->HasAura(57723) || GetBot()->HasAura(57724)) // Exhaustion/Sated
        return false;

    // Use in boss fights or when health is critical
    if (_currentTarget && _currentTarget->GetHealthPct() < 30.0f && _currentTarget->GetMaxHealth() > 100000)
        return true;

    // Use when multiple group members are low
    if (Group* group = GetBot()->GetGroup())
    {
        uint32 lowHealthCount = 0;
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                if (member->GetHealthPct() < 40.0f)
                    lowHealthCount++;
            }
        }
        return lowHealthCount >= 3;
    }

    return false;
}

Player* ShamanAI::FindGroupTank(Group* group) const
{
    if (!group)
        return nullptr;

    Player* tank = nullptr;
    uint32 highestHealth = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            // Simple tank detection - highest health or warrior/paladin/death knight
            if (member->GetClass() == CLASS_WARRIOR ||
                member->GetClass() == CLASS_PALADIN ||
                member->GetClass() == CLASS_DEATH_KNIGHT)
            {
                if (member->GetMaxHealth() > highestHealth)
                {
                    highestHealth = member->GetMaxHealth();
                    tank = member;
                }
            }
        }
    }

    return tank;
}

uint32 ShamanAI::CalculateDamageDealt(::Unit* target) const
{
    // Simplified damage calculation for metrics
    // In a real implementation, this would track actual damage events
    return 100;
}

uint32 ShamanAI::CalculateHealingDone() const
{
    // Simplified healing calculation for metrics
    // In a real implementation, this would track actual healing events
    return _currentSpec == ShamanSpec::RESTORATION ? 200 : 0;
}

uint32 ShamanAI::CalculateManaUsage() const
{
    // Simplified mana usage calculation
    // In a real implementation, this would track actual mana consumption
    return 50;
}

} // namespace Playerbot