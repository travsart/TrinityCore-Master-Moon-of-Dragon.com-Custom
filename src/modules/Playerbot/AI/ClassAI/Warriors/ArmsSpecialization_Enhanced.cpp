/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArmsSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include <algorithm>
#include <numeric>

namespace Playerbot
{

// Enhanced Arms Specialization Implementation

void ArmsSpecialization::OptimizeColossusSmashTiming(::Unit* target)
{
    if (!target)
        return;

    bool hasColossusSmash = _debuffTracker.HasColossusSmashDebuff(target->GetGUID());
    uint32 timeRemaining = GetColossusSmashTimeRemaining(target);

    // Apply Colossus Smash if not present or about to expire
    if (!hasColossusSmash || timeRemaining < 2000) // 2 seconds remaining
    {
        if (ShouldCastColossusSmash(target) && HasEnoughRage(20))
        {
            CastColossusSmash(target);
            _armsMetrics.colossusSmashUptime++;
            _debuffTracker.UpdateColossusSmash(target->GetGUID(), COLOSSUS_SMASH_DURATION);

            TC_LOG_DEBUG("playerbot.arms", "Colossus Smash applied to {} for debuff window",
                         target->GetName());
        }
    }
    else if (hasColossusSmash && timeRemaining > 3000)
    {
        // We have CS debuff active - optimize damage during window
        OptimizeDamageInColossusWindow(target);
    }
}

void ArmsSpecialization::OptimizeDamageInColossusWindow(::Unit* target)
{
    if (!target)
        return;

    // Priority during Colossus Smash window:
    // 1. Mortal Strike if available
    // 2. Execute if in execute phase
    // 3. Overpower if proc is up
    // 4. Rend if not applied

    if (ShouldCastMortalStrike(target) && HasEnoughRage(MORTAL_STRIKE_RAGE_COST))
    {
        CastMortalStrike(target);
        return;
    }

    if (IsInExecutePhase(target) && ShouldCastExecute(target))
    {
        OptimizeExecuteRageSpending(target);
        return;
    }

    if (_overpowerReady.load() && ShouldCastOverpower(target) && HasEnoughRage(OVERPOWER_RAGE_COST))
    {
        CastOverpower(target);
        HandleOverpowerProc();
        return;
    }

    if (!_debuffTracker.HasRend(target->GetGUID()) && HasEnoughRage(REND_RAGE_COST))
    {
        ManageRendDebuff(target);
    }
}

void ArmsSpecialization::ManageMortalStrikeDebuff(::Unit* target)
{
    if (!target)
        return;

    ObjectGuid targetGuid = target->GetGUID();
    bool hasMortalStrike = _debuffTracker.HasMortalStrike(targetGuid);

    // Apply or refresh Mortal Strike debuff
    if (!hasMortalStrike)
    {
        if (ShouldCastMortalStrike(target) && HasEnoughRage(MORTAL_STRIKE_RAGE_COST))
        {
            CastMortalStrike(target);
            _armsMetrics.totalMortalStrikes++;
            _debuffTracker.UpdateMortalStrike(targetGuid, MORTAL_STRIKE_DURATION);

            TC_LOG_DEBUG("playerbot.arms", "Mortal Strike applied to {} for healing reduction",
                         target->GetName());
        }
    }
    else
    {
        // Check if debuff is about to expire
        uint32 timeRemaining = GetMortalStrikeTimeRemaining(target);
        if (timeRemaining < 3000 && HasEnoughRage(MORTAL_STRIKE_RAGE_COST)) // 3 seconds
        {
            CastMortalStrike(target);
            _armsMetrics.totalMortalStrikes++;
            _debuffTracker.UpdateMortalStrike(targetGuid, MORTAL_STRIKE_DURATION);
        }
    }
}

void ArmsSpecialization::HandleOverpowerProc()
{
    if (!_overpowerReady.load())
        return;

    _armsMetrics.overpowerProcs++;
    _overpowerReady.store(false);

    TC_LOG_DEBUG("playerbot.arms", "Overpower proc consumed - critical hit opportunity used");

    // Track overpower efficiency
    uint32 totalProcs = _armsMetrics.overpowerProcs.load();
    if (totalProcs > 0)
    {
        // Calculate proc utilization rate
        float utilizationRate = static_cast<float>(totalProcs) / (totalProcs + 1);
        TC_LOG_DEBUG("playerbot.arms", "Overpower utilization rate: {:.1f}%", utilizationRate * 100.0f);
    }
}

void ArmsSpecialization::OptimizeExecutePhaseRotation(::Unit* target)
{
    if (!target || !IsInExecutePhase(target))
        return;

    if (!_inExecutePhase.load())
    {
        // Entering execute phase
        _inExecutePhase.store(true);
        _executePhaseStartTime = getMSTime();
        PrepareForExecutePhase(target);
        TC_LOG_DEBUG("playerbot.arms", "Entering execute phase for {}", target->GetName());
    }

    // Execute phase priority:
    // 1. Execute with optimal rage spending
    // 2. Mortal Strike if Execute is not available
    // 3. Sudden Death procs

    if (_suddenDeathProc.load())
    {
        HandleSuddenDeathProc(target);
        return;
    }

    if (ShouldCastExecute(target))
    {
        OptimizeExecuteRageSpending(target);
        return;
    }

    // Use Mortal Strike as filler if Execute is on cooldown
    if (ShouldCastMortalStrike(target) && HasEnoughRage(MORTAL_STRIKE_RAGE_COST))
    {
        CastMortalStrike(target);
    }
}

void ArmsSpecialization::OptimizeExecuteRageSpending(::Unit* target)
{
    if (!target)
        return;

    uint32 currentRage = GetRage();

    // Calculate optimal rage spending for Execute
    uint32 executeRage = std::min(currentRage, EXECUTE_MAX_RAGE_COST);
    executeRage = std::max(executeRage, EXECUTE_RAGE_COST); // Minimum cost

    if (executeRage >= EXECUTE_RAGE_COST)
    {
        CastExecute(target);
        ConsumeResource(executeRage);
        _executeAttempts++;

        // Check if target died from Execute
        if (!target->IsAlive())
        {
            _successfulExecutes++;
            _armsMetrics.executeKills++;
            TC_LOG_DEBUG("playerbot.arms", "Execute kill confirmed on {}", target->GetName());
        }

        // Track execute efficiency
        float efficiency = CalculateExecuteEfficiency();
        _armsMetrics.executePhaseEfficiency = (efficiency + _armsMetrics.executePhaseEfficiency.load()) / 2.0f;
    }
}

float ArmsSpecialization::CalculateExecuteEfficiency()
{
    if (_executeAttempts == 0)
        return 0.0f;

    float killRate = static_cast<float>(_successfulExecutes) / _executeAttempts;
    return killRate;
}

void ArmsSpecialization::PrepareForExecutePhase(::Unit* target)
{
    if (!target)
        return;

    // Save rage for execute phase
    uint32 currentRage = GetRage();
    if (currentRage < 40) // Want at least 40 rage for execute
    {
        // Avoid spending rage on other abilities
        TC_LOG_DEBUG("playerbot.arms", "Conserving rage for execute phase (current: {})", currentRage);
        return;
    }

    // Ensure debuffs are applied before execute phase
    if (!_debuffTracker.HasMortalStrike(target->GetGUID()))
    {
        ManageMortalStrikeDebuff(target);
    }

    if (!_debuffTracker.HasRend(target->GetGUID()))
    {
        ManageRendDebuff(target);
    }
}

bool ArmsSpecialization::ShouldSaveRageForExecute(::Unit* target)
{
    if (!target)
        return false;

    float healthPct = target->GetHealthPct();

    // Start saving rage when target is near execute threshold
    if (healthPct <= EXECUTE_OPTIMAL_THRESHOLD)
    {
        uint32 currentRage = GetRage();
        return currentRage < 40; // Save if we don't have enough for good execute
    }

    return false;
}

void ArmsSpecialization::HandleExecutePhaseTransition(::Unit* target)
{
    if (!target)
        return;

    float healthPct = target->GetHealthPct();
    bool wasInExecutePhase = _inExecutePhase.load();

    if (healthPct <= EXECUTE_HEALTH_THRESHOLD && !wasInExecutePhase)
    {
        // Entering execute phase
        _inExecutePhase.store(true);
        _executePhaseStartTime = getMSTime();
        PrepareForExecutePhase(target);
    }
    else if (healthPct > EXECUTE_HEALTH_THRESHOLD && wasInExecutePhase)
    {
        // Exiting execute phase (target healed above threshold)
        _inExecutePhase.store(false);
        TC_LOG_DEBUG("playerbot.arms", "Exiting execute phase - target healed above threshold");
    }
}

void ArmsSpecialization::ManageRendDebuff(::Unit* target)
{
    if (!target)
        return;

    ObjectGuid targetGuid = target->GetGUID();
    bool hasRend = _debuffTracker.HasRend(targetGuid);

    if (!hasRend && HasEnoughRage(REND_RAGE_COST))
    {
        CastRend(target);
        _debuffTracker.UpdateRend(targetGuid, REND_DURATION);
        _lastRendApplication = getMSTime();

        TC_LOG_DEBUG("playerbot.arms", "Rend applied to {} for DoT damage", target->GetName());
    }
    else if (hasRend)
    {
        // Check if Rend is about to expire
        uint32 timeRemaining = GetRendTimeRemaining(target);
        if (timeRemaining < 5000 && HasEnoughRage(REND_RAGE_COST)) // 5 seconds
        {
            CastRend(target);
            _debuffTracker.UpdateRend(targetGuid, REND_DURATION);
            _lastRendApplication = getMSTime();
        }
    }
}

void ArmsSpecialization::HandleSuddenDeathProc(::Unit* target)
{
    if (!target || !_suddenDeathProc.load())
        return;

    // Sudden Death allows Execute regardless of target health
    if (HasEnoughRage(EXECUTE_RAGE_COST))
    {
        CastExecute(target);
        _suddenDeathProc.store(false);
        _armsMetrics.suddenDeathProcs++;

        TC_LOG_DEBUG("playerbot.arms", "Sudden Death proc used for Execute on {}", target->GetName());
    }
}

void ArmsSpecialization::OptimizeWeaponSwapping()
{
    // Optimize weapon choice based on situation
    // This would be more complex in a real implementation

    bool hasTwoHandedWeapon = HasTwoHandedWeapon();

    if (!hasTwoHandedWeapon)
    {
        TC_LOG_DEBUG("playerbot.arms", "Warning: Arms warrior should use two-handed weapon for optimal damage");
        return;
    }

    UpdateWeaponSpecialization();
}

void ArmsSpecialization::UpdateWeaponSpecialization()
{
    // Update weapon mastery and specialization bonuses
    float damageBonus = CalculateWeaponDamageBonus();
    _armsMetrics.weaponDamageEfficiency = damageBonus;

    // Check for weapon mastery procs
    if (getMSTime() - _lastWeaponCheck > WEAPON_MASTERY_WINDOW)
    {
        HandleWeaponMasteryProcs();
        _lastWeaponCheck = getMSTime();
    }
}

float ArmsSpecialization::CalculateWeaponDamageBonus()
{
    float bonus = 1.0f;

    // Two-handed weapon specialization
    if (HasTwoHandedWeapon())
    {
        bonus *= TWO_HANDED_DAMAGE_BONUS;
    }

    // Weapon mastery talents (simplified)
    if (HasTalent(12163)) // Weapon Mastery
        bonus *= 1.05f;

    // Critical strike bonuses
    float critChance = CalculateCriticalStrikeChance();
    if (critChance >= CRITICAL_STRIKE_THRESHOLD)
        bonus *= 1.1f;

    return bonus;
}

float ArmsSpecialization::CalculateCriticalStrikeChance()
{
    // Calculate current critical strike chance
    float baseCrit = 0.05f; // 5% base

    // Add gear crit rating (simplified)
    float gearCrit = _bot->GetFloatValue(PLAYER_CRIT_PERCENTAGE) / 100.0f;

    // Add stance bonuses
    WarriorStance currentStance = GetCurrentStance();
    float stanceCrit = 0.0f;

    if (currentStance == WarriorStance::BERSERKER)
        stanceCrit = 0.03f; // 3% crit in Berserker

    return std::min(1.0f, baseCrit + gearCrit + stanceCrit);
}

void ArmsSpecialization::HandleWeaponMasteryProcs()
{
    // Handle weapon mastery talent procs
    // This would check for specific weapon proc effects

    uint32 weaponMasterySpell = 12295; // Tactical Mastery
    if (_bot->HasAura(weaponMasterySpell))
    {
        TC_LOG_DEBUG("playerbot.arms", "Weapon mastery proc active - optimizing rotation");

        // Use high-damage abilities during proc
        ::Unit* target = _bot->GetTarget();
        if (target)
        {
            if (ShouldCastMortalStrike(target))
            {
                CastMortalStrike(target);
            }
            else if (ShouldCastColossusSmash(target))
            {
                CastColossusSmash(target);
            }
        }
    }
}

void ArmsSpecialization::OptimizeCriticalStrikes()
{
    // Optimize for critical strike opportunities
    float critChance = CalculateCriticalStrikeChance();

    if (critChance >= CRITICAL_STRIKE_THRESHOLD)
    {
        // High crit chance - prioritize high-damage abilities
        ::Unit* target = _bot->GetTarget();
        if (target)
        {
            if (IsInExecutePhase(target))
            {
                OptimizeExecuteRageSpending(target);
            }
            else if (ShouldCastMortalStrike(target))
            {
                CastMortalStrike(target);
            }
        }
    }
}

void ArmsSpecialization::MonitorExecuteOpportunities(::Unit* target)
{
    if (!target)
        return;

    float healthPct = target->GetHealthPct();

    // Track execute opportunities for optimization
    if (healthPct <= EXECUTE_HEALTH_THRESHOLD)
    {
        _executeTimings.push(getMSTime());

        // Limit queue size for memory efficiency
        if (_executeTimings.size() > 10)
            _executeTimings.pop();

        HandleExecutePhaseTransition(target);
    }

    // Monitor for sudden death procs
    if (_bot->HasAura(SUDDEN_DEATH))
    {
        _suddenDeathProc.store(true);
        TC_LOG_DEBUG("playerbot.arms", "Sudden Death proc detected");
    }
    else if (_suddenDeathProc.load())
    {
        _suddenDeathProc.store(false);
    }

    // Check for overpower opportunities
    if (_bot->HasAura(OVERPOWER)) // Assuming overpower proc has an aura
    {
        _overpowerReady.store(true);
    }
}

uint32 ArmsSpecialization::GetColossusSmashTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    ObjectGuid targetGuid = target->GetGUID();
    auto it = _debuffTracker.colossusSmashExpireTimes.find(targetGuid);

    if (it != _debuffTracker.colossusSmashExpireTimes.end())
    {
        uint32 expireTime = it->second;
        uint32 currentTime = getMSTime();
        return expireTime > currentTime ? expireTime - currentTime : 0;
    }

    return 0;
}

uint32 ArmsSpecialization::GetMortalStrikeTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    ObjectGuid targetGuid = target->GetGUID();
    auto it = _debuffTracker.mortalStrikeExpireTimes.find(targetGuid);

    if (it != _debuffTracker.mortalStrikeExpireTimes.end())
    {
        uint32 expireTime = it->second;
        uint32 currentTime = getMSTime();
        return expireTime > currentTime ? expireTime - currentTime : 0;
    }

    return 0;
}

uint32 ArmsSpecialization::GetRendTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    ObjectGuid targetGuid = target->GetGUID();
    auto it = _debuffTracker.rendExpireTimes.find(targetGuid);

    if (it != _debuffTracker.rendExpireTimes.end())
    {
        uint32 expireTime = it->second;
        uint32 currentTime = getMSTime();
        return expireTime > currentTime ? expireTime - currentTime : 0;
    }

    return 0;
}

void ArmsSpecialization::OptimizeRageManagement()
{
    uint32 currentRage = GetRage();
    float ragePercent = static_cast<float>(currentRage) / GetMaxRage();

    // Rage conservation during execute preparation
    ::Unit* target = _bot->GetTarget();
    if (target && ShouldSaveRageForExecute(target))
    {
        TC_LOG_DEBUG("playerbot.arms", "Conserving rage for upcoming execute phase");
        return;
    }

    // Rage dump prevention
    if (ragePercent > 0.9f) // 90% rage
    {
        // Use rage efficiently
        if (target)
        {
            if (ShouldCastMortalStrike(target))
            {
                CastMortalStrike(target);
            }
            else if (ShouldCastOverpower(target) && _overpowerReady.load())
            {
                CastOverpower(target);
                HandleOverpowerProc();
            }
            else if (HasEnoughRage(REND_RAGE_COST) && !_debuffTracker.HasRend(target->GetGUID()))
            {
                ManageRendDebuff(target);
            }
        }
    }
}

void ArmsSpecialization::UpdateArmsMetrics()
{
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - _armsMetrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 5) // Update every 5 seconds
    {
        // Calculate execute phase efficiency
        if (_executeAttempts > 0)
        {
            float efficiency = CalculateExecuteEfficiency();
            _armsMetrics.executePhaseEfficiency = efficiency;
        }

        // Update weapon damage efficiency
        float weaponBonus = CalculateWeaponDamageBonus();
        _armsMetrics.weaponDamageEfficiency = weaponBonus;

        _armsMetrics.lastUpdate = currentTime;

        TC_LOG_DEBUG("playerbot.arms", "Arms metrics updated - Execute efficiency: {:.2f}, Weapon bonus: {:.2f}",
                     _armsMetrics.executePhaseEfficiency.load(), weaponBonus);
    }
}

void ArmsSpecialization::CleanupExpiredDebuffs()
{
    uint32 currentTime = getMSTime();

    // Clean up expired Mortal Strike debuffs
    auto msIt = _debuffTracker.mortalStrikeExpireTimes.begin();
    while (msIt != _debuffTracker.mortalStrikeExpireTimes.end())
    {
        if (msIt->second <= currentTime)
        {
            msIt = _debuffTracker.mortalStrikeExpireTimes.erase(msIt);
        }
        else
        {
            ++msIt;
        }
    }

    // Clean up expired Rend debuffs
    auto rendIt = _debuffTracker.rendExpireTimes.begin();
    while (rendIt != _debuffTracker.rendExpireTimes.end())
    {
        if (rendIt->second <= currentTime)
        {
            rendIt = _debuffTracker.rendExpireTimes.erase(rendIt);
        }
        else
        {
            ++rendIt;
        }
    }

    // Clean up expired Colossus Smash debuffs
    auto csIt = _debuffTracker.colossusSmashExpireTimes.begin();
    while (csIt != _debuffTracker.colossusSmashExpireTimes.end())
    {
        if (csIt->second <= currentTime)
        {
            csIt = _debuffTracker.colossusSmashExpireTimes.erase(csIt);
        }
        else
        {
            ++csIt;
        }
    }
}

// Enhanced rotation integration
void ArmsSpecialization::ExecuteEnhancedRotation(::Unit* target)
{
    if (!target)
        return;

    // Update all tracking systems
    MonitorExecuteOpportunities(target);
    CleanupExpiredDebuffs();
    UpdateArmsMetrics();

    // Optimize for current phase
    if (IsInExecutePhase(target))
    {
        OptimizeExecutePhaseRotation(target);
    }
    else
    {
        // Normal rotation
        OptimizeColossusSmashTiming(target);
        ManageMortalStrikeDebuff(target);
        ManageRendDebuff(target);

        // Handle procs
        if (_overpowerReady.load())
        {
            HandleOverpowerProc();
        }

        if (_suddenDeathProc.load())
        {
            HandleSuddenDeathProc(target);
        }
    }

    // Optimize rage usage
    OptimizeRageManagement();

    // Weapon optimization
    OptimizeWeaponSwapping();

    // Critical strike optimization
    OptimizeCriticalStrikes();
}

// Helper method implementations
bool ArmsSpecialization::HasTalent(uint32 talentId)
{
    // Simplified talent check - in real implementation would check talent trees
    return true; // Assume all Arms talents are available
}

bool ArmsSpecialization::HasTwoHandedWeapon()
{
    Item* mainHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainHand)
        return false;

    uint32 itemSubclass = mainHand->GetTemplate()->GetSubClass();

    // Check for two-handed weapon types
    return (itemSubclass == ITEM_SUBCLASS_WEAPON_SWORD2 ||
            itemSubclass == ITEM_SUBCLASS_WEAPON_AXE2 ||
            itemSubclass == ITEM_SUBCLASS_WEAPON_MACE2 ||
            itemSubclass == ITEM_SUBCLASS_WEAPON_POLEARM ||
            itemSubclass == ITEM_SUBCLASS_WEAPON_STAFF);
}

// Enhanced debuff tracker methods
void ArmsSpecialization::DebuffTracker::UpdateColossusSmash(ObjectGuid guid, uint32 duration)
{
    colossusSmashExpireTimes[guid] = getMSTime() + duration;
}

bool ArmsSpecialization::DebuffTracker::HasColossusSmashDebuff(ObjectGuid guid) const
{
    auto it = colossusSmashExpireTimes.find(guid);
    return it != colossusSmashExpireTimes.end() && it->second > getMSTime();
}

} // namespace Playerbot