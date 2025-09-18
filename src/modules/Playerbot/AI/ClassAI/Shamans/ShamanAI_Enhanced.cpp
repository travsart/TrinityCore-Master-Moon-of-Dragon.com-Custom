/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShamanAI.h"
#include "ElementalSpecialization_Enhanced.h"
#include "EnhancementSpecialization_Enhanced.h"
#include "RestorationSpecialization_Enhanced.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Group.h"
#include "Log.h"
#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace Playerbot
{

ShamanAI::ShamanAI(Player* bot) : ClassAI(bot),
    _manaSpent(0), _damageDealt(0), _healingDone(0), _totemsDeploy(0), _shocksUsed(0)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot.shaman", "Enhanced ShamanAI initialized for {} with specialization {}",
                 GetBot()->GetName(), GetSpecializationName());
}

void ShamanAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
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
            break;
        case ShamanSpec::ENHANCEMENT:
            _specialization = std::make_unique<EnhancementSpecialization>(GetBot());
            break;
        case ShamanSpec::RESTORATION:
            _specialization = std::make_unique<RestorationSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<ElementalSpecialization>(GetBot());
            break;
    }

    TC_LOG_DEBUG("playerbot.shaman", "Shaman {} switched to {} specialization",
                 GetBot()->GetName(), _specialization->GetSpecializationName());
}

ShamanSpec ShamanAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return ShamanSpec::ELEMENTAL;

    // Check for key Restoration talents
    if (HasTalent(16188)) // Nature's Swiftness
        return ShamanSpec::RESTORATION;

    // Check for key Enhancement talents
    if (HasTalent(17364)) // Stormstrike
        return ShamanSpec::ENHANCEMENT;

    // Default to Elemental
    return ShamanSpec::ELEMENTAL;
}

void ShamanAI::UpdateRotation(::Unit* target)
{
    if (!_specialization)
        return;

    // Update specialization
    UpdateSpecialization();

    // Delegate primary rotation to specialization
    _specialization->UpdateRotation(target);

    // Handle shared shaman utilities
    UpdateShamanBuffs();
    UpdateTotemCheck();
    UpdateShockRotation();
    ManageSharedAbilities();
}

void ShamanAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();

    UpdateShields();
    UpdateWeaponImbues();
}

void ShamanAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);

    // Update ability usage tracking
    for (auto& [spellId, lastUsed] : _abilityUsage)
        _abilityUsage[spellId] = lastUsed > diff ? lastUsed - diff : 0;
}

bool ShamanAI::CanUseAbility(uint32 spellId)
{
    if (_specialization && !_specialization->CanUseAbility(spellId))
        return false;

    return ClassAI::CanUseAbility(spellId);
}

void ShamanAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);

    // Reset performance metrics
    _manaSpent = 0;
    _damageDealt = 0;
    _healingDone = 0;
    _totemsDeploy = 0;
    _shocksUsed = 0;

    TC_LOG_DEBUG("playerbot.shaman", "Shaman {} entered combat with {}",
                 GetBot()->GetName(), target ? target->GetName() : "unknown target");
}

void ShamanAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();

    // Log combat performance
    TC_LOG_DEBUG("playerbot.shaman", "Shaman {} combat ended - Damage: {}, Healing: {}, Totems: {}, Shocks: {}",
                 GetBot()->GetName(), _damageDealt, _healingDone, _totemsDeploy, _shocksUsed);
}

bool ShamanAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    return HasEnoughMana(GetSpellManaCost(spellId));
}

void ShamanAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);

    uint32 manaCost = GetSpellManaCost(spellId);
    _manaSpent += manaCost;
    _abilityUsage[spellId] = getMSTime();
}

Position ShamanAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return GetBot()->GetPosition();
}

float ShamanAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);

    // Default range based on spec
    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
            return 30.0f; // Ranged caster
        case ShamanSpec::ENHANCEMENT:
            return 5.0f;  // Melee
        case ShamanSpec::RESTORATION:
            return 40.0f; // Ranged healer
        default:
            return 30.0f;
    }
}

void ShamanAI::UpdateSpecialization()
{
    ShamanSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        SwitchSpecialization(detectedSpec);
    }
}

void ShamanAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void ShamanAI::UpdateShamanBuffs()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    // Maintain shield appropriate to spec
    UpdateShields();

    // Maintain weapon imbues for Enhancement
    if (_currentSpec == ShamanSpec::ENHANCEMENT)
        UpdateWeaponImbues();
}

void ShamanAI::UpdateShields()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 optimalShield = GetOptimalShieldForSpecialization();
    if (!HasActiveShield() && optimalShield && CanUseAbility(optimalShield))
    {
        bot->CastSpell(bot, optimalShield, false);
        TC_LOG_DEBUG("playerbot.shaman", "Shaman {} cast shield spell {}", bot->GetName(), optimalShield);
    }
}

uint32 ShamanAI::GetOptimalShieldForSpecialization()
{
    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
            return LIGHTNING_SHIELD; // For damage and mana efficiency
        case ShamanSpec::ENHANCEMENT:
            return LIGHTNING_SHIELD; // For proc effects and damage
        case ShamanSpec::RESTORATION:
            return WATER_SHIELD;     // For mana regeneration
        default:
            return LIGHTNING_SHIELD;
    }
}

bool ShamanAI::HasActiveShield()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->HasAura(LIGHTNING_SHIELD) ||
           bot->HasAura(WATER_SHIELD) ||
           bot->HasAura(EARTH_SHIELD);
}

void ShamanAI::UpdateWeaponImbues()
{
    if (_currentSpec != ShamanSpec::ENHANCEMENT)
        return;

    Player* bot = GetBot();
    if (!bot)
        return;

    // Main hand imbue
    if (!HasMainHandImbue() && CanUseAbility(WINDFURY_WEAPON))
    {
        bot->CastSpell(bot, WINDFURY_WEAPON, false);
        TC_LOG_DEBUG("playerbot.shaman", "Shaman {} applied Windfury to main hand", bot->GetName());
    }

    // Off hand imbue
    if (HasOffHandWeapon() && !HasOffHandImbue() && CanUseAbility(FLAMETONGUE_WEAPON))
    {
        bot->CastSpell(bot, FLAMETONGUE_WEAPON, false);
        TC_LOG_DEBUG("playerbot.shaman", "Shaman {} applied Flametongue to off hand", bot->GetName());
    }
}

bool ShamanAI::HasMainHandImbue()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check for weapon imbue auras
    return bot->HasAura(WINDFURY_WEAPON) ||
           bot->HasAura(FLAMETONGUE_WEAPON) ||
           bot->HasAura(FROSTBRAND_WEAPON) ||
           bot->HasAura(EARTHLIVING_WEAPON) ||
           bot->HasAura(ROCKBITER_WEAPON);
}

bool ShamanAI::HasOffHandImbue()
{
    // Similar check for off-hand weapon imbues
    return HasMainHandImbue(); // Simplified for now
}

bool ShamanAI::HasOffHandWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    Item* offHandItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    return offHandItem && offHandItem->GetTemplate()->Class == ITEM_CLASS_WEAPON;
}

void ShamanAI::UpdateTotemCheck()
{
    if (!_specialization)
        return;

    // Let specialization handle totem management
    _specialization->UpdateTotemManagement();
}

void ShamanAI::UpdateShockRotation()
{
    // Shock management is handled by individual specializations
    // This is a placeholder for shared shock cooldown tracking
}

void ShamanAI::ManageSharedAbilities()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    // Handle emergency abilities
    HandleEmergencyAbilities();

    // Handle utility abilities
    HandleUtilityAbilities();

    // Handle cleansing
    HandleCleansing();
}

void ShamanAI::HandleEmergencyAbilities()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    float healthPercent = (float)bot->GetHealth() / bot->GetMaxHealth() * 100.0f;

    // Emergency self-healing for non-Restoration shamans
    if (_currentSpec != ShamanSpec::RESTORATION && healthPercent < EMERGENCY_HEALTH_THRESHOLD)
    {
        // Use Healing Wave for emergency self-healing
        if (CanUseAbility(HEALING_WAVE) && HasEnoughMana(400))
        {
            bot->CastSpell(bot, HEALING_WAVE, false);
            TC_LOG_DEBUG("playerbot.shaman", "Shaman {} used emergency Healing Wave at {}% health",
                        bot->GetName(), healthPercent);
        }
    }

    // Ghost Wolf for escape
    if (IsInDanger() && CanUseAbility(GHOST_WOLF))
    {
        bot->CastSpell(bot, GHOST_WOLF, false);
        TC_LOG_DEBUG("playerbot.shaman", "Shaman {} activated Ghost Wolf for escape", bot->GetName());
    }
}

void ShamanAI::HandleUtilityAbilities()
{
    // Handle group utility
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    // Bloodlust/Heroism usage
    if (ShouldUseBloodlust() && CanUseAbility(BLOODLUST))
    {
        GetBot()->CastSpell(GetBot(), BLOODLUST, false);
        TC_LOG_DEBUG("playerbot.shaman", "Shaman {} cast Bloodlust/Heroism", GetBot()->GetName());
    }

    // Purge enemy buffs
    ::Unit* purgeTarget = GetBestPurgeTarget();
    if (purgeTarget && CanUseAbility(PURGE))
    {
        GetBot()->CastSpell(purgeTarget, PURGE, false);
        TC_LOG_DEBUG("playerbot.shaman", "Shaman {} purged {}", GetBot()->GetName(), purgeTarget->GetName());
    }
}

void ShamanAI::HandleCleansing()
{
    if (_currentSpec != ShamanSpec::RESTORATION)
        return;

    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        // Cleanse poisons
        if (HasPoisonDebuff(member) && CanUseAbility(CURE_POISON))
        {
            GetBot()->CastSpell(member, CURE_POISON, false);
            TC_LOG_DEBUG("playerbot.shaman", "Shaman {} cured poison on {}",
                        GetBot()->GetName(), member->GetName());
            break;
        }

        // Cleanse diseases
        if (HasDiseaseDebuff(member) && CanUseAbility(CURE_DISEASE))
        {
            GetBot()->CastSpell(member, CURE_DISEASE, false);
            TC_LOG_DEBUG("playerbot.shaman", "Shaman {} cured disease on {}",
                        GetBot()->GetName(), member->GetName());
            break;
        }
    }
}

bool ShamanAI::ShouldUseBloodlust()
{
    // Use Bloodlust in challenging encounters or when group health is low
    Group* group = GetBot()->GetGroup();
    if (!group)
        return false;

    uint32 membersInCombat = 0;
    uint32 membersLowHealth = 0;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member->IsInMap(GetBot()))
        {
            if (member->IsInCombat())
                membersInCombat++;

            float healthPercent = (float)member->GetHealth() / member->GetMaxHealth() * 100.0f;
            if (healthPercent < 50.0f)
                membersLowHealth++;
        }
    }

    // Use Bloodlust if multiple members are in combat and some are low on health
    return membersInCombat >= 3 && membersLowHealth >= 2;
}

::Unit* ShamanAI::GetBestPurgeTarget()
{
    // Find enemy with beneficial magic effects to purge
    std::vector<::Unit*> enemies = GetNearbyEnemies(40.0f);

    for (::Unit* enemy : enemies)
    {
        if (enemy && HasPurgeableBuffs(enemy))
            return enemy;
    }

    return nullptr;
}

bool ShamanAI::IsInDanger()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check health threshold
    float healthPercent = (float)bot->GetHealth() / bot->GetMaxHealth();
    if (healthPercent < EMERGENCY_HEALTH_THRESHOLD / 100.0f)
        return true;

    // Check for dangerous debuffs
    if (HasDangerousDebuff(bot))
        return true;

    return false;
}

const char* ShamanAI::GetSpecializationName() const
{
    if (_specialization)
        return _specialization->GetSpecializationName();
    return "Unknown";
}

std::vector<::Unit*> ShamanAI::GetGroupMembers()
{
    std::vector<::Unit*> members;
    Player* bot = GetBot();

    Group* group = bot->GetGroup();
    if (!group)
    {
        members.push_back(bot);
        return members;
    }

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member->IsInMap(bot))
            members.push_back(member);
    }

    return members;
}

std::vector<::Unit*> ShamanAI::GetNearbyEnemies(float range)
{
    std::vector<::Unit*> enemies;
    Player* bot = GetBot();

    if (!bot)
        return enemies;

    // Implementation would find hostile units within range
    // Placeholder for now
    return enemies;
}

bool ShamanAI::HasEnoughMana(uint32 amount)
{
    return GetBot()->GetPower(POWER_MANA) >= amount;
}

uint32 ShamanAI::GetMana()
{
    return GetBot()->GetPower(POWER_MANA);
}

uint32 ShamanAI::GetMaxMana()
{
    return GetBot()->GetMaxPower(POWER_MANA);
}

float ShamanAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float)GetMana() / maxMana * 100.0f : 0.0f;
}

uint32 ShamanAI::GetSpellManaCost(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    return spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
}

bool ShamanAI::HasTalent(uint32 talentId)
{
    // Implementation would check if bot has specific talent
    return false; // Placeholder
}

bool ShamanAI::HasPoisonDebuff(::Unit* target)
{
    if (!target)
        return false;

    // Check for poison debuff types
    return target->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE_PERCENT) &&
           target->HasAuraWithMechanic(1 << MECHANIC_POISON);
}

bool ShamanAI::HasDiseaseDebuff(::Unit* target)
{
    if (!target)
        return false;

    // Check for disease debuff types
    return target->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) &&
           target->HasAuraWithMechanic(1 << MECHANIC_DISEASE);
}

bool ShamanAI::HasDangerousDebuff(::Unit* target)
{
    if (!target)
        return false;

    // Check for dangerous debuffs like fear, charm, etc.
    return target->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           target->HasAuraType(SPELL_AURA_MOD_CHARM) ||
           target->HasAuraType(SPELL_AURA_MOD_STUN);
}

bool ShamanAI::HasPurgeableBuffs(::Unit* target)
{
    if (!target)
        return false;

    // Check for beneficial magic effects that can be purged
    return target->HasAuraType(SPELL_AURA_MOD_DAMAGE_DONE) ||
           target->HasAuraType(SPELL_AURA_MOD_DAMAGE_TAKEN) ||
           target->HasAuraType(SPELL_AURA_HASTE_SPELLS);
}

void ShamanAI::RecordDamageDone(uint32 amount)
{
    _damageDealt += amount;
}

void ShamanAI::RecordHealingDone(uint32 amount)
{
    _healingDone += amount;
}

void ShamanAI::RecordTotemDeployed()
{
    _totemsDeploy++;
}

void ShamanAI::RecordShockUsed()
{
    _shocksUsed++;
}

} // namespace Playerbot