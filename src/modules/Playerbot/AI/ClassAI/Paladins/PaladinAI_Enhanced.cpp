/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PaladinAI.h"
#include "HolySpecialization_Enhanced.h"
#include "ProtectionSpecialization_Enhanced.h"
#include "RetributionSpecialization_Enhanced.h"
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

PaladinAI::PaladinAI(Player* bot) : ClassAI(bot),
    _manaSpent(0), _healingDone(0), _damageDealt(0),
    _lastBlessings(0), _lastAura(0)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot.paladin", "Enhanced PaladinAI initialized for {} with specialization {}",
                 GetBot()->GetName(), GetSpecializationName());
}

void PaladinAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void PaladinAI::SwitchSpecialization(PaladinSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;
    _specialization.reset();

    switch (newSpec)
    {
        case PaladinSpec::HOLY:
            _specialization = std::make_unique<HolyPaladinSpecialization>(GetBot());
            break;
        case PaladinSpec::PROTECTION:
            _specialization = std::make_unique<ProtectionPaladinSpecialization>(GetBot());
            break;
        case PaladinSpec::RETRIBUTION:
            _specialization = std::make_unique<RetributionPaladinSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<RetributionPaladinSpecialization>(GetBot());
            break;
    }

    TC_LOG_DEBUG("playerbot.paladin", "Paladin {} switched to {} specialization",
                 GetBot()->GetName(), _specialization->GetSpecializationName());
}

PaladinSpec PaladinAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return PaladinSpec::RETRIBUTION;

    // Check for key Protection talents
    if (HasTalent(31850)) // Ardent Defender
        return PaladinSpec::PROTECTION;

    // Check for key Holy talents
    if (HasTalent(20473)) // Holy Shock
        return PaladinSpec::HOLY;

    // Default to Retribution
    return PaladinSpec::RETRIBUTION;
}

void PaladinAI::UpdateRotation(::Unit* target)
{
    if (!_specialization)
        return;

    // Update specialization state
    UpdateSpecialization();

    // Delegate primary rotation to specialization
    _specialization->UpdateRotation(target);

    // Handle shared paladin utilities
    UpdatePaladinBuffs();
    UpdateAuras();
    ManageSharedAbilities();
}

void PaladinAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();

    CastBlessings();
}

void PaladinAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);

    // Update shared cooldowns
    _lastBlessings = _lastBlessings > diff ? _lastBlessings - diff : 0;
    _lastAura = _lastAura > diff ? _lastAura - diff : 0;

    // Update ability usage tracking
    for (auto& [spellId, lastUsed] : _abilityUsage)
        _abilityUsage[spellId] = lastUsed > diff ? lastUsed - diff : 0;
}

bool PaladinAI::CanUseAbility(uint32 spellId)
{
    if (_specialization && !_specialization->CanUseAbility(spellId))
        return false;

    return ClassAI::CanUseAbility(spellId);
}

void PaladinAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);

    // Reset performance metrics
    _manaSpent = 0;
    _healingDone = 0;
    _damageDealt = 0;

    TC_LOG_DEBUG("playerbot.paladin", "Paladin {} entered combat with {}",
                 GetBot()->GetName(), target ? target->GetName() : "unknown target");
}

void PaladinAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();

    // Log combat performance
    TC_LOG_DEBUG("playerbot.paladin", "Paladin {} combat ended - Healing: {}, Damage: {}",
                 GetBot()->GetName(), _healingDone, _damageDealt);
}

bool PaladinAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    return HasEnoughMana(GetSpellManaCost(spellId));
}

void PaladinAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);

    uint32 manaCost = GetSpellManaCost(spellId);
    _manaSpent += manaCost;
    _abilityUsage[spellId] = getMSTime();
}

Position PaladinAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return GetBot()->GetPosition();
}

float PaladinAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);

    // Default to melee range for paladin
    return OPTIMAL_MELEE_RANGE;
}

void PaladinAI::UpdateSpecialization()
{
    PaladinSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        SwitchSpecialization(detectedSpec);
    }
}

void PaladinAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void PaladinAI::UpdatePaladinBuffs()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    uint32 currentTime = getMSTime();

    // Maintain appropriate seal
    if (!HasActiveSeal())
        CastOptimalSeal();

    // Maintain aura if specialization supports it
    if (_specialization)
        _specialization->UpdateAura();
}

void PaladinAI::CastBlessings()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBlessings < BLESSING_REFRESH_INTERVAL)
        return;

    std::vector<::Unit*> groupMembers = GetGroupMembers();

    // Prioritize group blessings if we have enough members
    if (groupMembers.size() >= 3)
    {
        CastGroupBlessings();
    }
    else
    {
        CastIndividualBlessings(groupMembers);
    }

    _lastBlessings = currentTime;
}

void PaladinAI::CastGroupBlessings()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Greater Blessing of Might for physical DPS
    if (HasPhysicalDPSInGroup() && CanUseAbility(GREATER_BLESSING_OF_MIGHT))
        bot->CastSpell(bot, GREATER_BLESSING_OF_MIGHT, false);

    // Greater Blessing of Wisdom for casters
    if (HasCastersInGroup() && CanUseAbility(GREATER_BLESSING_OF_WISDOM))
        bot->CastSpell(bot, GREATER_BLESSING_OF_WISDOM, false);

    // Greater Blessing of Kings if available
    if (CanUseAbility(GREATER_BLESSING_OF_KINGS))
        bot->CastSpell(bot, GREATER_BLESSING_OF_KINGS, false);
}

void PaladinAI::CastIndividualBlessings(const std::vector<::Unit*>& groupMembers)
{
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        // Check what blessing the member needs
        if (!HasOptimalBlessing(member))
        {
            CastOptimalBlessing(member);
            break; // Only cast one blessing per update
        }
    }
}

void PaladinAI::UpdateAuras()
{
    if (!_specialization)
        return;

    PaladinAura currentAura = GetCurrentAura();
    PaladinAura optimalAura = _specialization->GetOptimalAura();

    if (currentAura != optimalAura)
        _specialization->SwitchAura(optimalAura);
}

void PaladinAI::ManageSharedAbilities()
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

void PaladinAI::HandleEmergencyAbilities()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    float healthPercent = (float)bot->GetHealth() / bot->GetMaxHealth() * 100.0f;

    // Emergency self-healing
    if (healthPercent < EMERGENCY_HEALTH_THRESHOLD)
    {
        // Divine Shield if in extreme danger
        if (healthPercent < 15.0f && CanUseAbility(DIVINE_SHIELD))
        {
            bot->CastSpell(bot, DIVINE_SHIELD, false);
            TC_LOG_DEBUG("playerbot.paladin", "Paladin {} used Divine Shield at {}% health",
                        bot->GetName(), healthPercent);
        }
        // Lay on Hands if available
        else if (healthPercent < 25.0f && CanUseAbility(LAY_ON_HANDS))
        {
            bot->CastSpell(bot, LAY_ON_HANDS, false);
            TC_LOG_DEBUG("playerbot.paladin", "Paladin {} used Lay on Hands at {}% health",
                        bot->GetName(), healthPercent);
        }
        // Divine Protection for damage reduction
        else if (CanUseAbility(DIVINE_PROTECTION))
        {
            bot->CastSpell(bot, DIVINE_PROTECTION, false);
        }
    }
}

void PaladinAI::HandleUtilityAbilities()
{
    // Handle group member emergencies
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive() || member == GetBot())
            continue;

        float memberHealthPercent = (float)member->GetHealth() / member->GetMaxHealth() * 100.0f;

        // Emergency healing for group members
        if (memberHealthPercent < EMERGENCY_HEALTH_THRESHOLD)
        {
            if (CanUseAbility(BLESSING_OF_PROTECTION))
            {
                GetBot()->CastSpell(member, BLESSING_OF_PROTECTION, false);
                TC_LOG_DEBUG("playerbot.paladin", "Paladin {} cast Blessing of Protection on {}",
                            GetBot()->GetName(), member->GetName());
                break;
            }
        }

        // Freedom for movement impaired allies
        if (IsMovementImpaired(member) && CanUseAbility(BLESSING_OF_FREEDOM))
        {
            GetBot()->CastSpell(member, BLESSING_OF_FREEDOM, false);
            TC_LOG_DEBUG("playerbot.paladin", "Paladin {} cast Blessing of Freedom on {}",
                        GetBot()->GetName(), member->GetName());
            break;
        }
    }
}

void PaladinAI::HandleCleansing()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        // Cleanse diseases, poisons, and magic effects
        if (HasCleansableDebuff(member) && CanUseAbility(CLEANSE))
        {
            GetBot()->CastSpell(member, CLEANSE, false);
            TC_LOG_DEBUG("playerbot.paladin", "Paladin {} cleansed {}",
                        GetBot()->GetName(), member->GetName());
            break;
        }
    }
}

bool PaladinAI::HasActiveSeal()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check for any active seal
    return bot->HasAura(SEAL_OF_RIGHTEOUSNESS) ||
           bot->HasAura(SEAL_OF_LIGHT) ||
           bot->HasAura(SEAL_OF_WISDOM) ||
           bot->HasAura(SEAL_OF_JUSTICE) ||
           bot->HasAura(SEAL_OF_THE_CRUSADER);
}

void PaladinAI::CastOptimalSeal()
{
    if (!_specialization)
        return;

    // Let specialization determine optimal seal
    uint32 optimalSeal = GetOptimalSealForSpecialization();
    if (optimalSeal && CanUseAbility(optimalSeal))
        GetBot()->CastSpell(GetBot(), optimalSeal, false);
}

uint32 PaladinAI::GetOptimalSealForSpecialization()
{
    switch (_currentSpec)
    {
        case PaladinSpec::HOLY:
            return SEAL_OF_LIGHT; // For healing through judgements
        case PaladinSpec::PROTECTION:
            return SEAL_OF_WISDOM; // For mana sustainability
        case PaladinSpec::RETRIBUTION:
            return SEAL_OF_RIGHTEOUSNESS; // For damage
        default:
            return SEAL_OF_RIGHTEOUSNESS;
    }
}

PaladinAura PaladinAI::GetCurrentAura()
{
    Player* bot = GetBot();
    if (!bot)
        return PaladinAura::NONE;

    if (bot->HasAura(DEVOTION_AURA))
        return PaladinAura::DEVOTION;
    else if (bot->HasAura(RETRIBUTION_AURA))
        return PaladinAura::RETRIBUTION_AURA;
    else if (bot->HasAura(CONCENTRATION_AURA))
        return PaladinAura::CONCENTRATION;
    else if (bot->HasAura(SHADOW_RESISTANCE_AURA))
        return PaladinAura::SHADOW_RESISTANCE;
    else if (bot->HasAura(FROST_RESISTANCE_AURA))
        return PaladinAura::FROST_RESISTANCE;
    else if (bot->HasAura(FIRE_RESISTANCE_AURA))
        return PaladinAura::FIRE_RESISTANCE;
    else
        return PaladinAura::NONE;
}

bool PaladinAI::HasPhysicalDPSInGroup()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (member && member->HasRole(ROLE_MELEE_DPS))
            return true;
    }

    return false;
}

bool PaladinAI::HasCastersInGroup()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (member && member->GetPowerType() == POWER_MANA && member->HasRole(ROLE_RANGED_DPS))
            return true;
    }

    return false;
}

bool PaladinAI::HasOptimalBlessing(::Unit* target)
{
    if (!target)
        return true;

    // Check if target has any blessing
    return target->HasAura(BLESSING_OF_MIGHT) ||
           target->HasAura(BLESSING_OF_WISDOM) ||
           target->HasAura(BLESSING_OF_KINGS) ||
           target->HasAura(GREATER_BLESSING_OF_MIGHT) ||
           target->HasAura(GREATER_BLESSING_OF_WISDOM) ||
           target->HasAura(GREATER_BLESSING_OF_KINGS);
}

void PaladinAI::CastOptimalBlessing(::Unit* target)
{
    if (!target)
        return;

    // Blessing of Kings if available (generally best)
    if (CanUseAbility(BLESSING_OF_KINGS))
    {
        GetBot()->CastSpell(target, BLESSING_OF_KINGS, false);
    }
    // Blessing of Might for physical DPS
    else if (target->HasRole(ROLE_MELEE_DPS) && CanUseAbility(BLESSING_OF_MIGHT))
    {
        GetBot()->CastSpell(target, BLESSING_OF_MIGHT, false);
    }
    // Blessing of Wisdom for casters
    else if (target->GetPowerType() == POWER_MANA && CanUseAbility(BLESSING_OF_WISDOM))
    {
        GetBot()->CastSpell(target, BLESSING_OF_WISDOM, false);
    }
    // Default to Might
    else if (CanUseAbility(BLESSING_OF_MIGHT))
    {
        GetBot()->CastSpell(target, BLESSING_OF_MIGHT, false);
    }
}

bool PaladinAI::IsMovementImpaired(::Unit* target)
{
    if (!target)
        return false;

    // Check for movement impairing effects
    return target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED) ||
           target->HasAuraType(SPELL_AURA_MOD_ROOT) ||
           target->HasAuraType(SPELL_AURA_MOD_STUN);
}

bool PaladinAI::HasCleansableDebuff(::Unit* target)
{
    if (!target)
        return false;

    // Check for cleansable debuff types
    return target->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) || // Disease/Poison DoTs
           target->HasAuraType(SPELL_AURA_MOD_FEAR) ||         // Fear
           target->HasAuraType(SPELL_AURA_MOD_CHARM) ||        // Charm
           target->HasAuraType(SPELL_AURA_TRANSFORM);          // Polymorph/etc
}

const char* PaladinAI::GetSpecializationName() const
{
    if (_specialization)
        return _specialization->GetSpecializationName();
    return "Unknown";
}

std::vector<::Unit*> PaladinAI::GetGroupMembers()
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

bool PaladinAI::HasEnoughMana(uint32 amount)
{
    return GetBot()->GetPower(POWER_MANA) >= amount;
}

uint32 PaladinAI::GetMana()
{
    return GetBot()->GetPower(POWER_MANA);
}

uint32 PaladinAI::GetMaxMana()
{
    return GetBot()->GetMaxPower(POWER_MANA);
}

float PaladinAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float)GetMana() / maxMana * 100.0f : 0.0f;
}

uint32 PaladinAI::GetSpellManaCost(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    return spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
}

bool PaladinAI::HasTalent(uint32 talentId)
{
    // Implementation would check if bot has specific talent
    return false; // Placeholder
}

void PaladinAI::RecordHealingDone(uint32 amount)
{
    _healingDone += amount;
}

void PaladinAI::RecordDamageDone(uint32 amount)
{
    _damageDealt += amount;
}

} // namespace Playerbot