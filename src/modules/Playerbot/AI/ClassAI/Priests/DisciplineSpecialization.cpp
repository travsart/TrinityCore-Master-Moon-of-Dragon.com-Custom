/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DisciplineSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectAccessor.h"
#include "GameTime.h"

namespace Playerbot
{

DisciplineSpecialization::DisciplineSpecialization(Player* bot)
    : PriestSpecialization(bot)
    , _currentRole(PriestRole::HEALER)
    , _atonementMode(false)
    , _lastInnerFocus(0)
    , _lastPainSuppression(0)
    , _lastGuardianSpirit(0)
    , _lastHealCheck(0)
    , _lastShieldCheck(0)
    , _lastAtonementCheck(0)
    , _lastRotationUpdate(0)
    , _lastAtonementScan(0)
{
}

void DisciplineSpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Update healing state
    UpdateHealing();

    // Emergency abilities first
    UseEmergencyAbilities();

    // Prioritize healing if in healer role
    if (_currentRole == PriestRole::HEALER || _currentRole == PriestRole::HYBRID)
    {
        if (ShouldHeal())
        {
            ::Unit* healTarget = GetBestHealTarget();
            if (healTarget)
            {
                HealTarget(healTarget);
                return;
            }
        }
    }

    // Shield management
    UpdateShields();

    // Atonement healing through damage
    if (ShouldUseAtonement() && target)
    {
        PerformAtonementHealing(target);
        return;
    }

    // DPS rotation if in DPS role or no healing needed
    if (target && (_currentRole == PriestRole::DPS || _currentRole == PriestRole::HYBRID))
    {
        // Penance for damage
        if (ShouldCastPenance(target))
        {
            CastPenance(target);
            return;
        }

        // Mind Blast
        if (CanUseAbility(MIND_BLAST))
        {
            CastMindBlast(target);
            return;
        }

        // Holy Fire
        if (CanUseAbility(HOLY_FIRE))
        {
            CastHolyFire(target);
            return;
        }

        // Smite
        if (CanUseAbility(SMITE))
        {
            CastSmite(target);
            return;
        }
    }
}

void DisciplineSpecialization::UpdateBuffs()
{
    // Power Word: Fortitude
    if (!_bot->HasAura(POWER_WORD_FORTITUDE))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(POWER_WORD_FORTITUDE, DIFFICULTY_NONE))
        {
            _bot->CastSpell(_bot, POWER_WORD_FORTITUDE, false);
        }
    }

    // Divine Spirit
    if (!_bot->HasAura(DIVINE_SPIRIT))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(DIVINE_SPIRIT, DIFFICULTY_NONE))
        {
            _bot->CastSpell(_bot, DIVINE_SPIRIT, false);
        }
    }

    // Inner Fire
    if (!_bot->HasAura(INNER_FIRE))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(INNER_FIRE, DIFFICULTY_NONE))
        {
            _bot->CastSpell(_bot, INNER_FIRE, false);
        }
    }

    // Check for group buffs
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        // Ensure everyone has Power Word: Fortitude
        if (!member->HasAura(POWER_WORD_FORTITUDE))
        {
            _bot->CastSpell(member, POWER_WORD_FORTITUDE, false);
            break; // One cast per update
        }
    }
}

void DisciplineSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    // Update shield timers
    RemoveExpiredShields(diff);

    // Update weakened soul timers
    for (auto& timer : _weakenedSoulTimers)
    {
        if (timer.second > diff)
            timer.second -= diff;
        else
            timer.second = 0;
    }

    // Update grace stacks
    for (auto& grace : _graceStacks)
    {
        if (grace.second > diff)
            grace.second -= diff;
        else
            grace.second = 0;
    }
}

bool DisciplineSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return true;
}

void DisciplineSpecialization::OnCombatStart(::Unit* target)
{
    _atonementMode = false;
    _healQueue = {};
    _atonementTargets.clear();
}

void DisciplineSpecialization::OnCombatEnd()
{
    _atonementMode = false;
    _cooldowns.clear();
    _shieldTimers.clear();
    _weakenedSoulTimers.clear();
    _graceStacks.clear();
    _atonementTargets.clear();
    _healQueue = {};
}

bool DisciplineSpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }
    return GetMana() >= manaCost;
}

void DisciplineSpecialization::ConsumeResource(uint32 spellId)
{
    // Mana is consumed automatically by spell casting
}

Position DisciplineSpecialization::GetOptimalPosition(::Unit* target)
{
    if (_currentRole == PriestRole::HEALER)
    {
        // Stay at max healing range, behind cover if possible
        float distance = OPTIMAL_HEALING_RANGE;
        if (target)
        {
            Position pos = target->GetNearPosition(distance, target->GetOrientation() + M_PI);
            return pos;
        }
    }
    else
    {
        // DPS positioning
        float distance = OPTIMAL_DPS_RANGE;
        if (target)
        {
            Position pos = target->GetNearPosition(distance, target->GetOrientation() + M_PI);
            return pos;
        }
    }

    return _bot->GetPosition();
}

float DisciplineSpecialization::GetOptimalRange(::Unit* target)
{
    return _currentRole == PriestRole::HEALER ? OPTIMAL_HEALING_RANGE : OPTIMAL_DPS_RANGE;
}

void DisciplineSpecialization::UpdateHealing()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastHealCheck < 1000) // 1 second throttle
        return;
    _lastHealCheck = currentTime;

    // Clear old heal queue
    _healQueue = {};

    // Scan group members for healing needs
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        float healthPercent = member->GetHealthPct();
        if (healthPercent < 95.0f)
        {
            HealPriority priority;
            if (healthPercent < 20.0f)
                priority = HealPriority::EMERGENCY;
            else if (healthPercent < 40.0f)
                priority = HealPriority::CRITICAL;
            else if (healthPercent < 70.0f)
                priority = HealPriority::MODERATE;
            else
                priority = HealPriority::MAINTENANCE;

            uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();
            ::Playerbot::HealTarget healTarget(member, priority, healthPercent, missingHealth);
            healTarget.hasHoTs = member->HasAura(RENEW) || member->HasAura(PRAYER_OF_MENDING);
            _healQueue.push(healTarget);
        }
    }
}

bool DisciplineSpecialization::ShouldHeal()
{
    return !_healQueue.empty() && (_currentRole == PriestRole::HEALER || _currentRole == PriestRole::HYBRID);
}

::Unit* DisciplineSpecialization::GetBestHealTarget()
{
    if (_healQueue.empty())
        return nullptr;

    ::Playerbot::HealTarget bestTarget = _healQueue.top();
    return bestTarget.target;
}

void DisciplineSpecialization::HealTarget(::Unit* target)
{
    if (!target)
        return;

    float healthPercent = target->GetHealthPct();

    // Emergency healing
    if (healthPercent < 20.0f)
    {
        // Pain Suppression for damage reduction
        if (ShouldUsePainSuppression(target))
        {
            CastPainSuppression(target);
            return;
        }

        // Guardian Spirit for death prevention
        if (ShouldUseGuardianSpirit(target))
        {
            CastGuardianSpirit(target);
            return;
        }

        // Flash Heal for speed
        if (CanUseAbility(FLASH_HEAL))
        {
            CastFlashHeal(target);
            return;
        }

        // Penance for healing
        if (ShouldCastPenance(target))
        {
            CastPenance(target);
            return;
        }
    }

    // Shield if not weakened soul
    if (ShouldCastShield(target))
    {
        CastPowerWordShield(target);
        return;
    }

    // Grace stacking with Greater Heal
    if (ShouldStackGrace(target) && CanUseAbility(GREATER_HEAL))
    {
        CastGreaterHeal(target);
        return;
    }

    // Prayer of Mending for mobile healing
    if (!target->HasAura(PRAYER_OF_MENDING) && CanUseAbility(PRAYER_OF_MENDING))
    {
        CastPrayerOfMending(target);
        return;
    }

    // Standard healing based on health level
    if (healthPercent < 70.0f)
    {
        if (CanUseAbility(GREATER_HEAL))
        {
            CastGreaterHeal(target);
        }
        else if (CanUseAbility(HEAL))
        {
            CastHeal(target);
        }
    }
    else if (healthPercent < 90.0f)
    {
        // Renew for efficient topping off
        if (!target->HasAura(RENEW) && CanUseAbility(RENEW))
        {
            CastRenew(target);
        }
    }
}

PriestRole DisciplineSpecialization::GetCurrentRole()
{
    return _currentRole;
}

void DisciplineSpecialization::SetRole(PriestRole role)
{
    _currentRole = role;
}

// Private methods

void DisciplineSpecialization::UpdateShields()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastShieldCheck < 2000) // 2 second throttle
        return;
    _lastShieldCheck = currentTime;

    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        if (ShouldCastShield(member))
        {
            CastPowerWordShield(member);
            break; // One shield per update
        }
    }
}

void DisciplineSpecialization::UpdateAtonement()
{
    if (!ShouldUseAtonement())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastAtonementCheck < 1000) // 1 second throttle
        return;
    _lastAtonementCheck = currentTime;

    UpdateAtonementHealing();
}

void DisciplineSpecialization::UpdateGrace()
{
    UpdateGraceStacks();
}

bool DisciplineSpecialization::ShouldCastShield(::Unit* target)
{
    if (!target || !CanUseAbility(POWER_WORD_SHIELD))
        return false;

    // Don't shield if weakened soul
    if (HasWeakenedSoul(target))
        return false;

    // Don't shield if already shielded
    if (target->HasAura(POWER_WORD_SHIELD))
        return false;

    // Shield low health targets
    if (target->GetHealthPct() < SHIELD_HEALTH_THRESHOLD)
        return true;

    // Shield before incoming damage
    if (target->IsInCombat() && target->GetHealthPct() < 95.0f)
        return true;

    return false;
}

bool DisciplineSpecialization::ShouldCastPenance(::Unit* target)
{
    if (!target || !CanUseAbility(PENANCE))
        return false;

    // Use for healing critical targets
    if (_currentRole == PriestRole::HEALER && target->GetHealthPct() < 40.0f)
        return true;

    // Use for damage on enemies
    if (_currentRole != PriestRole::HEALER && target->IsHostileTo(_bot))
        return true;

    return false;
}

bool DisciplineSpecialization::ShouldUseInnerFocus()
{
    uint32 currentTime = getMSTime();
    return currentTime - _lastInnerFocus > 180000 && // 3 minute cooldown
           GetManaPercent() < 50.0f &&
           CanUseAbility(INNER_FOCUS);
}

bool DisciplineSpecialization::ShouldUsePainSuppression(::Unit* target)
{
    if (!target || !CanUseAbility(PAIN_SUPPRESSION))
        return false;

    uint32 currentTime = getMSTime();
    return target->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD &&
           currentTime - _lastPainSuppression > 180000; // 3 minute cooldown
}

bool DisciplineSpecialization::ShouldUseGuardianSpirit(::Unit* target)
{
    if (!target || !CanUseAbility(GUARDIAN_SPIRIT))
        return false;

    uint32 currentTime = getMSTime();
    return target->GetHealthPct() < 15.0f &&
           currentTime - _lastGuardianSpirit > 180000; // 3 minute cooldown
}

void DisciplineSpecialization::CastPowerWordShield(::Unit* target)
{
    if (target && CanUseAbility(POWER_WORD_SHIELD))
    {
        _bot->CastSpell(target, POWER_WORD_SHIELD, false);
        _shieldTimers[target->GetGUID().GetCounter()] = getMSTime() + SHIELD_DURATION;
        _weakenedSoulTimers[target->GetGUID().GetCounter()] = getMSTime() + WEAKENED_SOUL_DURATION;
    }
}

void DisciplineSpecialization::CastPainSuppression(::Unit* target)
{
    if (target && CanUseAbility(PAIN_SUPPRESSION))
    {
        _bot->CastSpell(target, PAIN_SUPPRESSION, false);
        _lastPainSuppression = getMSTime();
        _cooldowns[PAIN_SUPPRESSION] = 180000; // 3 minute cooldown
    }
}

void DisciplineSpecialization::CastGuardianSpirit(::Unit* target)
{
    if (target && CanUseAbility(GUARDIAN_SPIRIT))
    {
        _bot->CastSpell(target, GUARDIAN_SPIRIT, false);
        _lastGuardianSpirit = getMSTime();
        _cooldowns[GUARDIAN_SPIRIT] = 180000; // 3 minute cooldown
    }
}

void DisciplineSpecialization::CastInnerFocus()
{
    if (CanUseAbility(INNER_FOCUS))
    {
        _bot->CastSpell(_bot, INNER_FOCUS, false);
        _lastInnerFocus = getMSTime();
        _cooldowns[INNER_FOCUS] = 180000; // 3 minute cooldown
    }
}

bool DisciplineSpecialization::HasWeakenedSoul(::Unit* target)
{
    if (!target)
        return false;

    auto it = _weakenedSoulTimers.find(target->GetGUID().GetCounter());
    return it != _weakenedSoulTimers.end() && it->second > getMSTime();
}

uint32 DisciplineSpecialization::GetShieldTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _shieldTimers.find(target->GetGUID().GetCounter());
    if (it != _shieldTimers.end() && it->second > getMSTime())
    {
        return it->second - getMSTime();
    }
    return 0;
}

void DisciplineSpecialization::CastPenance(::Unit* target)
{
    if (target && CanUseAbility(PENANCE))
    {
        _bot->CastSpell(target, PENANCE, false);
        _cooldowns[PENANCE] = 8000; // 8 second cooldown
    }
}

void DisciplineSpecialization::CastGreaterHeal(::Unit* target)
{
    if (target && CanUseAbility(GREATER_HEAL))
    {
        _bot->CastSpell(target, GREATER_HEAL, false);
        // Apply Grace stack
        uint64 guid = target->GetGUID().GetCounter();
        _graceStacks[guid] = getMSTime() + GRACE_DURATION;
    }
}

void DisciplineSpecialization::CastFlashHeal(::Unit* target)
{
    if (target && CanUseAbility(FLASH_HEAL))
    {
        _bot->CastSpell(target, FLASH_HEAL, false);
    }
}

void DisciplineSpecialization::CastBindingHeal(::Unit* target)
{
    if (target && CanUseAbility(BINDING_HEAL))
    {
        _bot->CastSpell(target, BINDING_HEAL, false);
    }
}

void DisciplineSpecialization::CastPrayerOfMending(::Unit* target)
{
    if (target && CanUseAbility(PRAYER_OF_MENDING))
    {
        _bot->CastSpell(target, PRAYER_OF_MENDING, false);
    }
}

void DisciplineSpecialization::UpdateAtonementHealing()
{
    // Implementation for Atonement healing through damage
    // Requires tracking damage dealt and converting to healing
}

bool DisciplineSpecialization::ShouldUseAtonement()
{
    return _atonementMode && GetManaPercent() > ATONEMENT_MANA_THRESHOLD;
}

void DisciplineSpecialization::PerformAtonementHealing(::Unit* target)
{
    if (!target)
        return;

    // Cast damage spells for Atonement healing
    if (CanUseAbility(SMITE))
    {
        CastSmite(target);
    }
    else if (CanUseAbility(HOLY_FIRE))
    {
        CastHolyFire(target);
    }
}

::Unit* DisciplineSpecialization::GetBestAtonementTarget()
{
    return _bot->GetVictim();
}

void DisciplineSpecialization::UpdateGraceStacks()
{
    // Grace stacks are updated when casting Greater Heal
    // Remove expired stacks
    uint32 currentTime = getMSTime();
    for (auto it = _graceStacks.begin(); it != _graceStacks.end();)
    {
        if (it->second <= currentTime)
            it = _graceStacks.erase(it);
        else
            ++it;
    }
}

uint32 DisciplineSpecialization::GetGraceStacks(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _graceStacks.find(target->GetGUID().GetCounter());
    return it != _graceStacks.end() && it->second > getMSTime() ? 1 : 0;
}

bool DisciplineSpecialization::ShouldStackGrace(::Unit* target)
{
    return target && GetGraceStacks(target) == 0 && target->GetHealthPct() < 80.0f;
}

void DisciplineSpecialization::UseEmergencyAbilities()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        if (member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        {
            if (ShouldUsePainSuppression(member))
            {
                CastPainSuppression(member);
                return;
            }
            if (ShouldUseGuardianSpirit(member))
            {
                CastGuardianSpirit(member);
                return;
            }
        }
    }
}

void DisciplineSpecialization::UsePainSuppression()
{
    ::Unit* target = GetBestHealTarget();
    if (target && ShouldUsePainSuppression(target))
    {
        CastPainSuppression(target);
    }
}

void DisciplineSpecialization::UseGuardianSpirit()
{
    ::Unit* target = GetBestHealTarget();
    if (target && ShouldUseGuardianSpirit(target))
    {
        CastGuardianSpirit(target);
    }
}

void DisciplineSpecialization::UseInnerFocus()
{
    if (ShouldUseInnerFocus())
    {
        CastInnerFocus();
    }
}

void DisciplineSpecialization::TrackShields()
{
    // Shield tracking is handled in CastPowerWordShield
}

void DisciplineSpecialization::RemoveExpiredShields(uint32 diff)
{
    for (auto& timer : _shieldTimers)
    {
        if (timer.second > diff)
            timer.second -= diff;
        else
            timer.second = 0;
    }
}

void DisciplineSpecialization::CastMindBlast(::Unit* target)
{
    if (!target || !CanUseAbility(MIND_BLAST))
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(MIND_BLAST, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (GetMana() < manaCost)
        return;

    // Check range
    float distance = _bot->GetDistance(target);
    if (distance > spellInfo->GetMaxRange())
        return;

    // Cast spell
    if (_bot->CastSpell(target, MIND_BLAST, false) == SPELL_CAST_OK)
    {
        _cooldowns[MIND_BLAST] = 8000; // 8 second cooldown
    }
}

void DisciplineSpecialization::CastHolyFire(::Unit* target)
{
    if (!target || !CanUseAbility(HOLY_FIRE))
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(HOLY_FIRE, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (GetMana() < manaCost)
        return;

    // Check range
    float distance = _bot->GetDistance(target);
    if (distance > spellInfo->GetMaxRange())
        return;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return;

    // Cast spell
    if (_bot->CastSpell(target, HOLY_FIRE, false) == SPELL_CAST_OK)
    {
        _cooldowns[HOLY_FIRE] = 10000; // 10 second cooldown
    }
}

void DisciplineSpecialization::CastSmite(::Unit* target)
{
    if (!target || !CanUseAbility(SMITE))
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SMITE, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (GetMana() < manaCost)
        return;

    // Check range
    float distance = _bot->GetDistance(target);
    if (distance > spellInfo->GetMaxRange())
        return;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return;

    // Cast spell
    if (_bot->CastSpell(target, SMITE, false) == SPELL_CAST_OK)
    {
        // Smite has no cooldown in classic, only cast time
    }
}

} // namespace Playerbot