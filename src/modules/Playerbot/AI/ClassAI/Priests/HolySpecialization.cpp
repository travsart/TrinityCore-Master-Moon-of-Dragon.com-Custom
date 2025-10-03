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
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SharedDefines.h"
#include "ObjectAccessor.h"
#include <queue>

namespace Playerbot
{

HolySpecialization::HolySpecialization(Player* bot)
    : PriestSpecialization(bot)
    , _currentRole(PriestRole::HEALER)
    , _serendipityStacks(0)
    , _inSpiritOfRedemption(false)
    , _inChakraSerenity(false)
    , _inChakraSanctuary(false)
    , _spiritActivationTime(0)
    , _lastHealCheck(0)
    , _lastHoTCheck(0)
    , _lastAoECheck(0)
    , _lastSerendipityCheck(0)
    , _lastRotationUpdate(0)
    , _lastClusterScan(0)
    , _emergencyMode(false)
    , _emergencyStartTime(0)
{
}

void HolySpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Handle Spirit of Redemption
    if (IsInSpiritOfRedemption())
    {
        HandleSpiritOfRedemption();
        return;
    }

    // Update healing state
    UpdateHealing();

    // Emergency healing takes priority
    if (_emergencyMode)
    {
        HandleEmergencyHealing();
        return;
    }

    // Check for emergency situations
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (member && member->IsAlive() && member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        {
            _emergencyMode = true;
            _emergencyStartTime = currentTime;
            UseEmergencyHeals(member);
            return;
        }
    }

    // Normal healing rotation
    if (ShouldHeal())
    {
        ::Unit* healTarget = GetBestHealTarget();
        if (healTarget)
        {
            HealTarget(healTarget);
            return;
        }
    }

    // AoE healing opportunities
    if (ShouldUseAoEHealing())
    {
        CastOptimalAoEHeal();
        return;
    }

    // HoT maintenance
    UpdateHoTs();

    // DPS if no healing needed and in hybrid role
    if (target && _currentRole == PriestRole::HYBRID)
    {
        if (CanUseAbility(HOLY_FIRE))
        {
            CastHolyFire(target);
        }
        else if (CanUseAbility(SMITE))
        {
            CastSmite(target);
        }
    }
}

void HolySpecialization::UpdateBuffs()
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

    // Group buffs
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

void HolySpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    // Update HoT timers
    for (auto& timer : _renewTimers)
    {
        if (timer.second > diff)
            timer.second -= diff;
        else
            timer.second = 0;
    }

    // Update Holy Word cooldowns
    UpdateHolyWordCooldowns();

    // Update Serendipity
    UpdateSerendipity();

    // Check emergency mode timeout
    if (_emergencyMode && getMSTime() - _emergencyStartTime > 10000) // 10 second emergency window
    {
        _emergencyMode = false;
    }
}

bool HolySpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return true;
}

void HolySpecialization::OnCombatStart(::Unit* target)
{
    _emergencyMode = false;
    _healQueue = std::priority_queue<Playerbot::HealTarget, std::vector<Playerbot::HealTarget>, std::less<Playerbot::HealTarget>>();
}

void HolySpecialization::OnCombatEnd()
{
    _emergencyMode = false;
    _cooldowns.clear();
    _renewTimers.clear();
    _prayerOfMendingBounces.clear();
    _healQueue = std::priority_queue<Playerbot::HealTarget, std::vector<Playerbot::HealTarget>, std::less<Playerbot::HealTarget>>();
}

bool HolySpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }

    // Serendipity reduces Flash Heal and Greater Heal costs
    if ((spellId == FLASH_HEAL || spellId == GREATER_HEAL) && GetSerendipityStacks() > 0)
    {
        manaCost = manaCost * (100 - (GetSerendipityStacks() * 20)) / 100; // 20% reduction per stack
    }

    return GetMana() >= manaCost;
}

void HolySpecialization::ConsumeResource(uint32 spellId)
{
    // Mana is consumed automatically by spell casting

    // Consume Serendipity stacks
    if ((spellId == FLASH_HEAL || spellId == GREATER_HEAL) && GetSerendipityStacks() > 0)
    {
        ConsumeSerendipity();
    }
}

Position HolySpecialization::GetOptimalPosition(::Unit* target)
{
    // Stay at optimal healing range
    float distance = OPTIMAL_HEALING_RANGE;

    if (ShouldUseAoEHealing())
    {
        // Position for optimal AoE healing
        return GetOptimalAoEHealPosition();
    }

    if (target)
    {
        return target->GetNearPosition(distance, target->GetOrientation() + M_PI);
    }

    return _bot->GetPosition();
}

float HolySpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_HEALING_RANGE;
}

void HolySpecialization::UpdateHealing()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastHealCheck < 500) // 500ms throttle for responsiveness
        return;
    _lastHealCheck = currentTime;

    // Clear old heal queue
    _healQueue = std::priority_queue<Playerbot::HealTarget, std::vector<Playerbot::HealTarget>, std::less<Playerbot::HealTarget>>();

    // Scan group members for healing needs
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        float healthPercent = member->GetHealthPct();
        if (healthPercent < 98.0f) // More generous threshold for Holy
        {
            HealPriority priority;
            if (healthPercent < 15.0f)
                priority = HealPriority::EMERGENCY;
            else if (healthPercent < 35.0f)
                priority = HealPriority::CRITICAL;
            else if (healthPercent < 65.0f)
                priority = HealPriority::MODERATE;
            else
                priority = HealPriority::MAINTENANCE;

            uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();
            Playerbot::HealTarget healTarget(member, priority, healthPercent, missingHealth);
            healTarget.hasHoTs = member->HasAura(RENEW) || member->HasAura(PRAYER_OF_MENDING);
            _healQueue.push(healTarget);
        }
    }
}

bool HolySpecialization::ShouldHeal()
{
    return !_healQueue.empty();
}

::Unit* HolySpecialization::GetBestHealTarget()
{
    if (_healQueue.empty())
        return nullptr;

    Playerbot::HealTarget bestTarget = _healQueue.top();
    return bestTarget.target;
}

void HolySpecialization::HealTarget(::Unit* target)
{
    if (!target)
        return;

    float healthPercent = target->GetHealthPct();

    // Emergency - Guardian Spirit if available
    if (healthPercent < 15.0f && ShouldCastGuardianSpirit(target))
    {
        CastGuardianSpirit(target);
        return;
    }

    // Critical health - Holy Word: Serenity
    if (healthPercent < 20.0f && ShouldCastHolyWordSerenity(target))
    {
        CastHolyWordSerenity(target);
        return;
    }

    // Fast heals for critical situations
    if (healthPercent < 30.0f)
    {
        if (GetSerendipityStacks() > 0 && CanUseAbility(FLASH_HEAL))
        {
            CastFlashHeal(target);
            return;
        }

        if (CanUseAbility(FLASH_HEAL))
        {
            CastFlashHeal(target);
            return;
        }
    }

    // Efficient healing for moderate damage
    if (healthPercent < 70.0f)
    {
        // Use Serendipity stacks
        if (GetSerendipityStacks() > 0 && CanUseAbility(GREATER_HEAL))
        {
            CastGreaterHeal(target);
            return;
        }

        if (CanUseAbility(GREATER_HEAL))
        {
            CastGreaterHeal(target);
            return;
        }

        if (CanUseAbility(HEAL))
        {
            CastHeal(target);
            return;
        }
    }

    // Maintenance healing
    if (healthPercent < 95.0f)
    {
        // Apply or refresh Renew
        if (ShouldRefreshRenew(target))
        {
            CastRenew(target);
            return;
        }

        // Prayer of Mending for mobile healing
        if (!target->HasAura(PRAYER_OF_MENDING) && CanUseAbility(PRAYER_OF_MENDING))
        {
            CastPrayerOfMending(target);
            return;
        }
    }
}

PriestRole HolySpecialization::GetCurrentRole()
{
    return _currentRole;
}

void HolySpecialization::SetRole(PriestRole role)
{
    _currentRole = role;
}

// Private methods

void HolySpecialization::UpdateHolyWordCooldowns()
{
    // Holy Word cooldowns are reduced by casting related spells
    // Implementation would track spell casts and reduce cooldowns accordingly
}

void HolySpecialization::UpdateSerendipity()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastSerendipityCheck < 1000) // 1 second throttle
        return;
    _lastSerendipityCheck = currentTime;

    UpdateSerendipityStacks();
}

void HolySpecialization::UpdateSpiritOfRedemption()
{
    _inSpiritOfRedemption = _bot->HasAura(SPIRIT_OF_REDEMPTION);
}

void HolySpecialization::UpdateEmpoweredRenew()
{
    // Check for Empowered Renew procs
}

bool HolySpecialization::ShouldCastHolyWordSerenity(::Unit* target)
{
    return target && target->GetHealthPct() < 25.0f && CanUseAbility(HOLY_WORD_SERENITY);
}

bool HolySpecialization::ShouldCastHolyWordSanctify()
{
    return ShouldUseAoEHealing() && CanUseAbility(HOLY_WORD_SANCTIFY);
}

bool HolySpecialization::ShouldCastDivineHymn()
{
    std::vector<::Unit*> injured = GetInjuredGroupMembers(50.0f);
    return injured.size() >= 3 && CanUseAbility(DIVINE_HYMN);
}

bool HolySpecialization::ShouldCastGuardianSpirit(::Unit* target)
{
    return target && target->GetHealthPct() < 15.0f && CanUseAbility(GUARDIAN_SPIRIT);
}

bool HolySpecialization::ShouldCastEmpoweredRenew(::Unit* target)
{
    return target && _bot->HasAura(EMPOWERED_RENEW) && !target->HasAura(RENEW);
}

void HolySpecialization::CastHolyWordSerenity(::Unit* target)
{
    if (target && CanUseAbility(HOLY_WORD_SERENITY))
    {
        _bot->CastSpell(target, HOLY_WORD_SERENITY, false);
        _cooldowns[HOLY_WORD_SERENITY] = 60000; // 60 second base cooldown
    }
}

void HolySpecialization::CastHolyWordSanctify()
{
    if (CanUseAbility(HOLY_WORD_SANCTIFY))
    {
        Position aoeTarget = GetOptimalAoEHealPosition();
        _bot->CastSpell(aoeTarget, HOLY_WORD_SANCTIFY, false);
        _cooldowns[HOLY_WORD_SANCTIFY] = 60000; // 60 second base cooldown
    }
}

void HolySpecialization::CastHolyWordChastise(::Unit* target)
{
    if (target && CanUseAbility(HOLY_WORD_CHASTISE))
    {
        _bot->CastSpell(target, HOLY_WORD_CHASTISE, false);
        _cooldowns[HOLY_WORD_CHASTISE] = 45000; // 45 second cooldown
    }
}

void HolySpecialization::CastDivineHymn()
{
    if (CanUseAbility(DIVINE_HYMN))
    {
        _bot->CastSpell(_bot, DIVINE_HYMN, false);
        _cooldowns[DIVINE_HYMN] = 300000; // 5 minute cooldown
    }
}

void HolySpecialization::CastGuardianSpirit(::Unit* target)
{
    if (target && CanUseAbility(GUARDIAN_SPIRIT))
    {
        _bot->CastSpell(target, GUARDIAN_SPIRIT, false);
        _cooldowns[GUARDIAN_SPIRIT] = 180000; // 3 minute cooldown
    }
}

void HolySpecialization::CastGreaterHeal(::Unit* target)
{
    if (target && CanUseAbility(GREATER_HEAL))
    {
        _bot->CastSpell(target, GREATER_HEAL, false);
        // Build Serendipity stack
        if (_serendipityStacks < MAX_SERENDIPITY_STACKS)
            _serendipityStacks++;
    }
}

void HolySpecialization::CastFlashHeal(::Unit* target)
{
    if (target && CanUseAbility(FLASH_HEAL))
    {
        _bot->CastSpell(target, FLASH_HEAL, false);
        // Build Serendipity stack
        if (_serendipityStacks < MAX_SERENDIPITY_STACKS)
            _serendipityStacks++;
    }
}

void HolySpecialization::CastHeal(::Unit* target)
{
    if (target && CanUseAbility(HEAL))
    {
        _bot->CastSpell(target, HEAL, false);
        // Build Serendipity stack
        if (_serendipityStacks < MAX_SERENDIPITY_STACKS)
            _serendipityStacks++;
    }
}

void HolySpecialization::CastRenew(::Unit* target)
{
    if (target && CanUseAbility(RENEW))
    {
        _bot->CastSpell(target, RENEW, false);
        _renewTimers[target->GetGUID().GetCounter()] = getMSTime() + RENEW_DURATION;
    }
}

void HolySpecialization::CastPrayerOfHealing()
{
    if (CanUseAbility(PRAYER_OF_HEALING))
    {
        _bot->CastSpell(_bot, PRAYER_OF_HEALING, false);
    }
}

void HolySpecialization::CastCircleOfHealing()
{
    if (CanUseAbility(CIRCLE_OF_HEALING))
    {
        _bot->CastSpell(_bot, CIRCLE_OF_HEALING, false);
        _cooldowns[CIRCLE_OF_HEALING] = 10000; // 10 second cooldown
    }
}

void HolySpecialization::CastPrayerOfMending(::Unit* target)
{
    if (target && CanUseAbility(PRAYER_OF_MENDING))
    {
        _bot->CastSpell(target, PRAYER_OF_MENDING, false);
        _prayerOfMendingBounces[target->GetGUID().GetCounter()] = PRAYER_OF_MENDING_BOUNCES;
    }
}

void HolySpecialization::UpdateSerendipityStacks()
{
    // Serendipity stacks are built by casting healing spells
    // and tracked in the cast methods
}

uint32 HolySpecialization::GetSerendipityStacks()
{
    return _serendipityStacks;
}

bool HolySpecialization::ShouldUseSerendipity()
{
    return _serendipityStacks > 0;
}

void HolySpecialization::ConsumeSerendipity()
{
    _serendipityStacks = 0;
}

void HolySpecialization::UpdateHoTs()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastHoTCheck < 2000) // 2 second throttle
        return;
    _lastHoTCheck = currentTime;

    ManageRenews();
    OptimizeHoTCoverage();
}

void HolySpecialization::ManageRenews()
{
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        if (ShouldRefreshRenew(member))
        {
            CastRenew(member);
            break; // One Renew per update
        }
    }
}

bool HolySpecialization::ShouldRefreshRenew(::Unit* target)
{
    if (!target || target->GetHealthPct() > 95.0f)
        return false;

    uint32 timeRemaining = GetRenewTimeRemaining(target);
    return timeRemaining < RENEW_REFRESH_THRESHOLD;
}

uint32 HolySpecialization::GetRenewTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _renewTimers.find(target->GetGUID().GetCounter());
    if (it != _renewTimers.end() && it->second > getMSTime())
    {
        return it->second - getMSTime();
    }
    return 0;
}

void HolySpecialization::OptimizeHoTCoverage()
{
    // Ensure all injured members have appropriate HoTs
    std::vector<::Unit*> injured = GetInjuredGroupMembers(85.0f);
    for (::Unit* member : injured)
    {
        if (!member->HasAura(RENEW) && GetRenewTimeRemaining(member) == 0)
        {
            CastRenew(member);
            break;
        }
    }
}

void HolySpecialization::UpdateAoEHealing()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastAoECheck < 1000) // 1 second throttle
        return;
    _lastAoECheck = currentTime;

    _clusteredMembers.clear();
    std::vector<::Unit*> clusteredUnits = GetClusteredInjuredMembers();
    for (::Unit* unit : clusteredUnits)
    {
        if (unit)
            _clusteredMembers.push_back(unit->GetGUID().GetCounter());
    }
}

bool HolySpecialization::ShouldUseAoEHealing()
{
    UpdateAoEHealing();
    return _clusteredMembers.size() >= AOE_HEAL_THRESHOLD;
}

void HolySpecialization::CastOptimalAoEHeal()
{
    // Priority: Holy Word: Sanctify > Circle of Healing > Prayer of Healing
    if (ShouldCastHolyWordSanctify())
    {
        CastHolyWordSanctify();
    }
    else if (CanUseAbility(CIRCLE_OF_HEALING))
    {
        CastCircleOfHealing();
    }
    else if (CanUseAbility(PRAYER_OF_HEALING))
    {
        CastPrayerOfHealing();
    }
}

std::vector<::Unit*> HolySpecialization::GetClusteredInjuredMembers()
{
    std::vector<::Unit*> clustered;
    std::vector<::Unit*> injured = GetInjuredGroupMembers(80.0f);

    // Find clusters of injured members
    for (::Unit* member : injured)
    {
        if (!member)
            continue;

        uint32 nearbyCount = 0;
        for (::Unit* other : injured)
        {
            if (!other || other == member)
                continue;

            if (member->GetDistance(other) <= CLUSTER_DISTANCE)
                nearbyCount++;
        }

        if (nearbyCount >= 2) // At least 2 others nearby
        {
            clustered.push_back(member);
        }
    }

    return clustered;
}

Position HolySpecialization::GetOptimalAoEHealPosition()
{
    if (_clusteredMembers.empty())
        return _bot->GetPosition();

    // Calculate center of clustered members
    float totalX = 0, totalY = 0, totalZ = 0;
    uint32 count = 0;

    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member)
            continue;

        for (uint64 guid : _clusteredMembers)
        {
            if (member->GetGUID().GetCounter() == guid)
            {
                totalX += member->GetPositionX();
                totalY += member->GetPositionY();
                totalZ += member->GetPositionZ();
                count++;
                break;
            }
        }
    }

    if (count > 0)
    {
        Position center;
        center.Relocate(totalX / count, totalY / count, totalZ / count);
        return center;
    }

    return _bot->GetPosition();
}

void HolySpecialization::HandleSpiritOfRedemption()
{
    if (!_inSpiritOfRedemption)
        return;

    // Maximize healing output during Spirit of Redemption
    MaximizeSpiritHealing();
}

bool HolySpecialization::IsInSpiritOfRedemption()
{
    return _bot->HasAura(SPIRIT_OF_REDEMPTION);
}

void HolySpecialization::MaximizeSpiritHealing()
{
    // Cast as many heals as possible during spirit form
    ::Unit* criticalTarget = nullptr;
    float lowestHealth = 100.0f;

    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        if (member->GetHealthPct() < lowestHealth)
        {
            lowestHealth = member->GetHealthPct();
            criticalTarget = member;
        }
    }

    if (criticalTarget)
    {
        if (CanUseAbility(GREATER_HEAL))
            CastGreaterHeal(criticalTarget);
        else if (CanUseAbility(FLASH_HEAL))
            CastFlashHeal(criticalTarget);
    }
}

void HolySpecialization::HandleEmergencyHealing()
{
    std::vector<::Unit*> criticalMembers;
    std::vector<::Unit*> groupMembers = GetGroupMembers();

    for (::Unit* member : groupMembers)
    {
        if (member && member->IsAlive() && member->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        {
            criticalMembers.push_back(member);
        }
    }

    if (criticalMembers.empty())
    {
        _emergencyMode = false;
        return;
    }

    // Sort by health percentage
    std::sort(criticalMembers.begin(), criticalMembers.end(),
        [](::Unit* a, ::Unit* b) { return a->GetHealthPct() < b->GetHealthPct(); });

    // Heal the most critical target
    UseEmergencyHeals(criticalMembers[0]);

    // Use emergency cooldowns if multiple critical
    if (criticalMembers.size() > 1)
    {
        ActivateEmergencyCooldowns();
    }
}

void HolySpecialization::UseEmergencyHeals(::Unit* target)
{
    if (!target)
        return;

    // Guardian Spirit for death prevention
    if (ShouldCastGuardianSpirit(target))
    {
        CastGuardianSpirit(target);
        return;
    }

    // Holy Word: Serenity for instant big heal
    if (ShouldCastHolyWordSerenity(target))
    {
        CastHolyWordSerenity(target);
        return;
    }

    // Flash Heal for speed
    if (CanUseAbility(FLASH_HEAL))
    {
        CastFlashHeal(target);
        return;
    }

    // Any available heal
    if (CanUseAbility(GREATER_HEAL))
        CastGreaterHeal(target);
    else if (CanUseAbility(HEAL))
        CastHeal(target);
}

void HolySpecialization::ActivateEmergencyCooldowns()
{
    // Divine Hymn for group emergency
    if (ShouldCastDivineHymn())
    {
        CastDivineHymn();
    }
}

} // namespace Playerbot