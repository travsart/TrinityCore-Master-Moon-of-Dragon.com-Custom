/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PriestAI.h"
#include "DisciplineSpecialization_Enhanced.h"
#include "HolySpecialization_Enhanced.h"
#include "ShadowSpecialization_Enhanced.h"
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

PriestAI::PriestAI(Player* bot) : ClassAI(bot),
    _manaSpent(0), _healingDone(0), _damageDealt(0), _playersHealed(0), _damagePrevented(0),
    _lastDispel(0), _lastFearWard(0), _lastPsychicScream(0), _lastInnerFire(0)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot.priest", "Enhanced PriestAI initialized for {} with specialization {}",
                 GetBot()->GetName(), GetSpecializationName());
}

void PriestAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void PriestAI::SwitchSpecialization(PriestSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;
    _specialization.reset();

    switch (newSpec)
    {
        case PriestSpec::DISCIPLINE:
            _specialization = std::make_unique<DisciplineSpecialization>(GetBot());
            break;
        case PriestSpec::HOLY:
            _specialization = std::make_unique<HolySpecialization>(GetBot());
            break;
        case PriestSpec::SHADOW:
            _specialization = std::make_unique<ShadowSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<HolySpecialization>(GetBot());
            break;
    }

    TC_LOG_DEBUG("playerbot.priest", "Priest {} switched to {} specialization",
                 GetBot()->GetName(), _specialization->GetSpecializationName());
}

PriestSpec PriestAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return PriestSpec::HOLY;

    // Check for key Shadow talents
    if (HasTalent(15473)) // Shadowform
        return PriestSpec::SHADOW;

    // Check for key Discipline talents
    if (HasTalent(47540)) // Penance
        return PriestSpec::DISCIPLINE;

    // Default to Holy
    return PriestSpec::HOLY;
}

void PriestAI::UpdateRotation(::Unit* target)
{
    if (!_specialization)
        return;

    // Update specialization state
    UpdateSpecialization();

    // Delegate primary rotation to specialization
    _specialization->UpdateRotation(target);

    // Handle shared priest utilities
    UpdatePriestBuffs();
    UpdateDispelling();
    ManageThreat();
    OptimizeManaUsage();
}

void PriestAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();

    UpdateFortitudeBuffs();
    UpdateSpiritBuffs();
}

void PriestAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);

    // Update shared cooldowns
    _lastDispel = _lastDispel > diff ? _lastDispel - diff : 0;
    _lastFearWard = _lastFearWard > diff ? _lastFearWard - diff : 0;
    _lastPsychicScream = _lastPsychicScream > diff ? _lastPsychicScream - diff : 0;
    _lastInnerFire = _lastInnerFire > diff ? _lastInnerFire - diff : 0;
}

bool PriestAI::CanUseAbility(uint32 spellId)
{
    if (_specialization && !_specialization->CanUseAbility(spellId))
        return false;

    return ClassAI::CanUseAbility(spellId);
}

void PriestAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);

    // Reset performance metrics
    _manaSpent = 0;
    _healingDone = 0;
    _damageDealt = 0;
    _playersHealed = 0;
    _damagePrevented = 0;

    TC_LOG_DEBUG("playerbot.priest", "Priest {} entered combat with {}",
                 GetBot()->GetName(), target ? target->GetName() : "unknown target");
}

void PriestAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();

    // Log combat performance
    TC_LOG_DEBUG("playerbot.priest", "Priest {} combat ended - Healing: {}, Damage: {}, Players Healed: {}, Damage Prevented: {}",
                 GetBot()->GetName(), _healingDone, _damageDealt, _playersHealed, _damagePrevented);
}

bool PriestAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    return HasEnoughMana(GetSpellManaCost(spellId));
}

void PriestAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);

    uint32 manaCost = GetSpellManaCost(spellId);
    _manaSpent += manaCost;
}

Position PriestAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return GetBot()->GetPosition();
}

float PriestAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);

    return OPTIMAL_HEALING_RANGE;
}

void PriestAI::UpdateSpecialization()
{
    PriestSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        SwitchSpecialization(detectedSpec);
        AdaptToGroupRole();
    }
}

void PriestAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void PriestAI::UpdatePriestBuffs()
{
    uint32 currentTime = getMSTime();

    // Inner Fire
    if (currentTime - _lastInnerFire > INNER_FIRE_DURATION)
        CastInnerFire();
}

void PriestAI::CastInnerFire()
{
    if (CanUseAbility(INNER_FIRE))
    {
        GetBot()->CastSpell(GetBot(), INNER_FIRE, false);
        _lastInnerFire = getMSTime();
        TC_LOG_DEBUG("playerbot.priest", "Priest {} cast Inner Fire", GetBot()->GetName());
    }
}

void PriestAI::UpdateFortitudeBuffs()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (member && !member->HasAura(POWER_WORD_FORTITUDE) && !member->HasAura(PRAYER_OF_FORTITUDE))
        {
            if (groupMembers.size() >= 3 && CanUseAbility(PRAYER_OF_FORTITUDE))
                GetBot()->CastSpell(GetBot(), PRAYER_OF_FORTITUDE, false);
            else if (CanUseAbility(POWER_WORD_FORTITUDE))
                GetBot()->CastSpell(member, POWER_WORD_FORTITUDE, false);
            break;
        }
    }
}

void PriestAI::UpdateSpiritBuffs()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (member && member->GetPowerType() == POWER_MANA &&
            !member->HasAura(DIVINE_SPIRIT) && !member->HasAura(PRAYER_OF_SPIRIT))
        {
            if (groupMembers.size() >= 3 && CanUseAbility(PRAYER_OF_SPIRIT))
                GetBot()->CastSpell(GetBot(), PRAYER_OF_SPIRIT, false);
            else if (CanUseAbility(DIVINE_SPIRIT))
                GetBot()->CastSpell(member, DIVINE_SPIRIT, false);
            break;
        }
    }
}

bool PriestAI::HasEnoughMana(uint32 amount)
{
    return GetMana() >= amount;
}

uint32 PriestAI::GetMana()
{
    return GetBot()->GetPower(POWER_MANA);
}

uint32 PriestAI::GetMaxMana()
{
    return GetBot()->GetMaxPower(POWER_MANA);
}

float PriestAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float)GetMana() / maxMana * 100.0f : 0.0f;
}

void PriestAI::OptimizeManaUsage()
{
    float manaPercent = GetManaPercent();

    // Use mana regeneration abilities when low
    if (manaPercent < MANA_EMERGENCY_THRESHOLD * 100.0f)
    {
        if (CanUseAbility(HYMN_OF_HOPE) && ShouldConserveMana())
            CastHymnOfHope();
    }
}

bool PriestAI::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD * 100.0f;
}

void PriestAI::UseManaRegeneration()
{
    if (CanUseAbility(HYMN_OF_HOPE))
        CastHymnOfHope();
}

void PriestAI::CastHymnOfHope()
{
    if (CanUseAbility(HYMN_OF_HOPE))
    {
        GetBot()->CastSpell(GetBot(), HYMN_OF_HOPE, false);
        TC_LOG_DEBUG("playerbot.priest", "Priest {} cast Hymn of Hope", GetBot()->GetName());
    }
}

void PriestAI::UseDefensiveAbilities()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    float healthPercent = (float)bot->GetHealth() / bot->GetMaxHealth() * 100.0f;

    // Emergency abilities
    if (healthPercent < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
    {
        if (CanUseAbility(FADE))
            CastFade();

        if (CanUseAbility(DESPERATE_PRAYER))
            GetBot()->CastSpell(GetBot(), DESPERATE_PRAYER, false);
    }

    // Defensive abilities when threatened
    if (IsInDanger())
    {
        if (CanUseAbility(PSYCHIC_SCREAM) && getMSTime() - _lastPsychicScream > PSYCHIC_SCREAM_COOLDOWN)
            CastPsychicScream();

        if (CanUseAbility(FEAR_WARD) && getMSTime() - _lastFearWard > FEAR_WARD_DURATION)
            CastFearWard();
    }
}

void PriestAI::CastPsychicScream()
{
    if (CanUseAbility(PSYCHIC_SCREAM))
    {
        GetBot()->CastSpell(GetBot(), PSYCHIC_SCREAM, false);
        _lastPsychicScream = getMSTime();
        TC_LOG_DEBUG("playerbot.priest", "Priest {} cast Psychic Scream", GetBot()->GetName());
    }
}

void PriestAI::CastFade()
{
    if (CanUseAbility(FADE))
    {
        GetBot()->CastSpell(GetBot(), FADE, false);
        TC_LOG_DEBUG("playerbot.priest", "Priest {} cast Fade", GetBot()->GetName());
    }
}

void PriestAI::CastDispelMagic()
{
    ::Unit* target = GetBestDispelTarget();
    if (target && CanUseAbility(DISPEL_MAGIC) && getMSTime() - _lastDispel > DISPEL_COOLDOWN)
    {
        GetBot()->CastSpell(target, DISPEL_MAGIC, false);
        _lastDispel = getMSTime();
        TC_LOG_DEBUG("playerbot.priest", "Priest {} dispelled {}", GetBot()->GetName(), target->GetName());
    }
}

void PriestAI::CastFearWard()
{
    ::Unit* target = GetBestFearWardTarget();
    if (target && CanUseAbility(FEAR_WARD))
    {
        GetBot()->CastSpell(target, FEAR_WARD, false);
        _lastFearWard = getMSTime();
        TC_LOG_DEBUG("playerbot.priest", "Priest {} cast Fear Ward on {}", GetBot()->GetName(), target->GetName());
    }
}

void PriestAI::UseCrowdControl(::Unit* target)
{
    if (!target)
        return;

    // Mind Control for critical targets
    if (target->IsPlayer() && CanUseAbility(MIND_CONTROL))
        CastMindControl(target);

    // Shackle Undead
    if (target->GetCreatureType() == CREATURE_TYPE_UNDEAD && CanUseAbility(SHACKLE_UNDEAD))
        CastShackleUndead(target);

    // Silence casters
    if (target->HasUnitState(UNIT_STATE_CASTING) && CanUseAbility(SILENCE))
        CastSilence(target);
}

void PriestAI::CastMindControl(::Unit* target)
{
    if (CanUseAbility(MIND_CONTROL))
    {
        GetBot()->CastSpell(target, MIND_CONTROL, false);
        _mindControlTargets[target->GetGUID()] = getMSTime();
        TC_LOG_DEBUG("playerbot.priest", "Priest {} mind controlled {}", GetBot()->GetName(), target->GetName());
    }
}

void PriestAI::CastShackleUndead(::Unit* target)
{
    if (CanUseAbility(SHACKLE_UNDEAD))
    {
        GetBot()->CastSpell(target, SHACKLE_UNDEAD, false);
        TC_LOG_DEBUG("playerbot.priest", "Priest {} shackled undead {}", GetBot()->GetName(), target->GetName());
    }
}

void PriestAI::CastSilence(::Unit* target)
{
    if (CanUseAbility(SILENCE))
    {
        GetBot()->CastSpell(target, SILENCE, false);
        TC_LOG_DEBUG("playerbot.priest", "Priest {} silenced {}", GetBot()->GetName(), target->GetName());
    }
}

void PriestAI::UpdateDispelling()
{
    if (getMSTime() - _lastDispel < DISPEL_COOLDOWN)
        return;

    ::Unit* dispelTarget = GetBestDispelTarget();
    if (dispelTarget)
        CastDispelMagic();
}

void PriestAI::CheckForDebuffs()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (!member)
            continue;

        // Check for dispellable debuffs
        if (HasDispellableDebuff(member))
        {
            if (CanUseAbility(DISPEL_MAGIC))
                GetBot()->CastSpell(member, DISPEL_MAGIC, false);
            break;
        }

        // Check for diseases
        if (HasDiseaseDebuff(member))
        {
            if (CanUseAbility(ABOLISH_DISEASE))
                GetBot()->CastSpell(member, ABOLISH_DISEASE, false);
            else if (CanUseAbility(CURE_DISEASE))
                GetBot()->CastSpell(member, CURE_DISEASE, false);
            break;
        }
    }
}

void PriestAI::UpdatePriestPositioning()
{
    if (_specialization && _specialization->GetCurrentRole() == PriestRole::HEALER)
        MaintainHealingPosition();
    else
        MaintainDPSPosition();
}

bool PriestAI::IsAtOptimalHealingRange(::Unit* target)
{
    if (!target)
        return false;

    float distance = GetBot()->GetDistance(target);
    return distance <= OPTIMAL_HEALING_RANGE && distance >= SAFE_HEALING_RANGE;
}

void PriestAI::MaintainHealingPosition()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    if (groupMembers.empty())
        return;

    // Find center position of group
    float centerX = 0, centerY = 0, centerZ = 0;
    uint32 memberCount = 0;

    for (::Unit* member : groupMembers)
    {
        if (member && member->IsAlive())
        {
            Position pos = member->GetPosition();
            centerX += pos.GetPositionX();
            centerY += pos.GetPositionY();
            centerZ += pos.GetPositionZ();
            ++memberCount;
        }
    }

    if (memberCount > 0)
    {
        centerX /= memberCount;
        centerY /= memberCount;
        centerZ /= memberCount;

        Position centerPos(centerX, centerY, centerZ, 0);
        float distance = GetBot()->GetDistance(centerPos);

        // Move towards center if too far
        if (distance > OPTIMAL_HEALING_RANGE * 0.8f)
        {
            GetBot()->GetMotionMaster()->MovePoint(0, centerPos);
        }
    }
}

void PriestAI::MaintainDPSPosition()
{
    ::Unit* target = GetBot()->GetSelectedUnit();
    if (!target)
        return;

    float distance = GetBot()->GetDistance(target);
    if (distance > OPTIMAL_DPS_RANGE)
    {
        // Move closer for DPS
        Position targetPos = target->GetPosition();
        Position movePos = GetBot()->GetNearPoint(target, OPTIMAL_DPS_RANGE * 0.8f, 0);
        GetBot()->GetMotionMaster()->MovePoint(0, movePos);
    }
    else if (distance < MINIMUM_SAFE_RANGE)
    {
        // Move back if too close
        Position safePos = GetBot()->GetNearPoint(GetBot(), MINIMUM_SAFE_RANGE, M_PI);
        GetBot()->GetMotionMaster()->MovePoint(0, safePos);
    }
}

bool PriestAI::IsInDanger()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check health threshold
    float healthPercent = (float)bot->GetHealth() / bot->GetMaxHealth();
    if (healthPercent < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Check threat levels
    if (HasTooMuchThreat())
        return true;

    // Check for dangerous debuffs
    if (HasDangerousDebuff(bot))
        return true;

    return false;
}

void PriestAI::FindSafePosition()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Find position away from enemies
    ::Unit* nearestEnemy = GetNearestEnemy();
    if (nearestEnemy)
    {
        float angle = bot->GetAngle(nearestEnemy) + M_PI; // Opposite direction
        Position safePos = bot->GetNearPoint(bot, 20.0f, angle);
        bot->GetMotionMaster()->MovePoint(0, safePos);
    }
}

::Unit* PriestAI::GetBestHealTarget()
{
    if (_specialization)
        return _specialization->GetBestHealTarget();

    return GetLowestHealthAlly();
}

::Unit* PriestAI::GetBestDispelTarget()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (member && HasDispellableDebuff(member))
            return member;
    }

    return nullptr;
}

::Unit* PriestAI::GetBestFearWardTarget()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    // Prioritize tanks and melee DPS
    for (::Unit* member : groupMembers)
    {
        if (member && !member->HasAura(FEAR_WARD) &&
            (member->HasRole(ROLE_TANK) || member->HasRole(ROLE_MELEE_DPS)))
            return member;
    }

    // Fallback to any group member
    for (::Unit* member : groupMembers)
    {
        if (member && !member->HasAura(FEAR_WARD))
            return member;
    }

    return nullptr;
}

::Unit* PriestAI::GetLowestHealthAlly()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    ::Unit* lowestHealthTarget = nullptr;
    float lowestHealthPercent = 100.0f;

    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        float healthPercent = (float)member->GetHealth() / member->GetMaxHealth() * 100.0f;
        if (healthPercent < lowestHealthPercent && healthPercent < 95.0f)
        {
            lowestHealthPercent = healthPercent;
            lowestHealthTarget = member;
        }
    }

    return lowestHealthTarget;
}

::Unit* PriestAI::GetHighestPriorityPatient()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    ::Unit* highestPriorityTarget = nullptr;
    HealPriority highestPriority = HealPriority::FULL;

    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        float healthPercent = (float)member->GetHealth() / member->GetMaxHealth() * 100.0f;
        HealPriority priority = GetHealPriority(healthPercent);

        if (priority < highestPriority)
        {
            highestPriority = priority;
            highestPriorityTarget = member;
        }
    }

    return highestPriorityTarget;
}

void PriestAI::AdaptToGroupRole()
{
    if (!_specialization)
        return;

    Group* group = GetBot()->GetGroup();
    if (!group)
    {
        // Solo play - adapt based on spec
        if (_currentSpec == PriestSpec::SHADOW)
            _specialization->SetRole(PriestRole::DPS);
        else
            _specialization->SetRole(PriestRole::HYBRID);
        return;
    }

    // In group - check for other healers
    bool hasOtherHealer = false;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member != GetBot() && member->HasRole(ROLE_HEALER))
        {
            hasOtherHealer = true;
            break;
        }
    }

    // Set role based on spec and group composition
    if (_currentSpec == PriestSpec::SHADOW)
        _specialization->SetRole(hasOtherHealer ? PriestRole::DPS : PriestRole::HYBRID);
    else
        _specialization->SetRole(PriestRole::HEALER);
}

void PriestAI::ManageThreat()
{
    if (HasTooMuchThreat())
        ReduceThreat();
}

bool PriestAI::HasTooMuchThreat()
{
    // Implementation would check actual threat levels
    return false; // Placeholder
}

void PriestAI::ReduceThreat()
{
    if (CanUseAbility(FADE))
        CastFade();
}

void PriestAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
    if (target && target != GetBot())
        ++_playersHealed;
}

void PriestAI::RecordDamageDone(uint32 amount, ::Unit* target)
{
    _damageDealt += amount;
}

const char* PriestAI::GetSpecializationName() const
{
    if (_specialization)
        return _specialization->GetSpecializationName();
    return "Unknown";
}

HealPriority PriestAI::GetHealPriority(float healthPercent)
{
    if (healthPercent < 25.0f)
        return HealPriority::EMERGENCY;
    else if (healthPercent < 50.0f)
        return HealPriority::CRITICAL;
    else if (healthPercent < 75.0f)
        return HealPriority::MODERATE;
    else if (healthPercent < 90.0f)
        return HealPriority::MAINTENANCE;
    else
        return HealPriority::FULL;
}

std::vector<::Unit*> PriestAI::GetGroupMembers()
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

bool PriestAI::HasDispellableDebuff(::Unit* target)
{
    // Implementation would check for magic debuffs that can be dispelled
    return false; // Placeholder
}

bool PriestAI::HasDiseaseDebuff(::Unit* target)
{
    // Implementation would check for disease debuffs
    return false; // Placeholder
}

bool PriestAI::HasDangerousDebuff(::Unit* target)
{
    // Implementation would check for dangerous debuffs like fear, charm, etc.
    return false; // Placeholder
}

::Unit* PriestAI::GetNearestEnemy()
{
    // Implementation would find the nearest hostile unit
    return nullptr; // Placeholder
}

uint32 PriestAI::GetSpellManaCost(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    return spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
}

bool PriestAI::HasTalent(uint32 talentId)
{
    // Implementation would check if bot has specific talent
    return false; // Placeholder
}

} // namespace Playerbot