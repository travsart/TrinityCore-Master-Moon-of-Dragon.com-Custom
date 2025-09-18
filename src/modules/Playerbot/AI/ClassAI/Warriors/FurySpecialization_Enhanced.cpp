/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FurySpecialization.h"
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
#include <cmath>

namespace Playerbot
{

// Enhanced Fury Specialization Implementation

void FurySpecialization::OptimizeFuryRotation(::Unit* target)
{
    if (!target)
        return;

    // Update all fury mechanics
    UpdateEnrage();
    HandleRampageMechanics(target);
    OptimizeDualWieldAttackSpeed();
    HandleFuryProcs();

    // Priority system for Fury rotation:
    // 1. Rampage at 4+ stacks during Enrage
    // 2. Bloodthirst for Enrage triggers and healing
    // 3. Raging Blow during Enrage
    // 4. Execute in execute phase
    // 5. Whirlwind for multiple targets
    // 6. Furious Slash as filler

    bool isEnraged = _isEnraged.load();
    uint32 rampageStacks = _rampageTracker.GetStackCount();

    // Execute phase has highest priority
    if (IsInExecutePhase(target))
    {
        OptimizeExecutePhaseFury(target);
        return;
    }

    // Rampage at max stacks during Enrage
    if (isEnraged && _rampageTracker.HasMaxStacks() && ShouldCastRampage(target))
    {
        ExecuteOptimalRampage(target);
        return;
    }

    // Bloodthirst for Enrage generation and sustain
    if (ShouldCastBloodthirst(target) && HasEnoughRage(BLOODTHIRST_RAGE_COST))
    {
        ExecuteOptimalBloodthirst(target);
        return;
    }

    // Raging Blow during Enrage window
    if (isEnraged && CanUseAbility(RAGING_BLOW) && HasEnoughRage(RAGING_BLOW_RAGE_COST))
    {
        CastRagingBlow(target);
        RecordRagingBlowUsage(target);
        return;
    }

    // Multi-target Whirlwind
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(8.0f);
    if (nearbyEnemies.size() >= 3 && ShouldCastWhirlwind() && HasEnoughRage(WHIRLWIND_RAGE_COST))
    {
        CastWhirlwind();
        _furyMetrics.whirlwindHits += nearbyEnemies.size();
        return;
    }

    // Rampage without Enrage (if at max stacks)
    if (_rampageTracker.HasMaxStacks() && ShouldCastRampage(target))
    {
        ExecuteOptimalRampage(target);
        return;
    }

    // Furious Slash as filler
    if (CanUseAbility(FURIOUS_SLASH) && HasEnoughRage(10))
    {
        CastFuriousSlash(target);
        return;
    }

    // Rage management
    ManageRageEfficiencyFury();
}

void FurySpecialization::ExecuteOptimalRampage(::Unit* target)
{
    if (!target || !_rampageTracker.HasMaxStacks())
        return;

    bool isEnraged = _isEnraged.load();

    // Calculate optimal rampage timing
    if (isEnraged || ShouldExecuteRampageWithoutEnrage(target))
    {
        CastRampage(target);
        _rampageTracker.ConsumeStacks();
        _furyMetrics.rampageExecutions++;
        _lastRampage = getMSTime();

        // Rampage extends Enrage duration
        if (isEnraged)
        {
            ExtendEnrageDuration();
        }

        TC_LOG_DEBUG("playerbot.fury", "Optimal Rampage executed on {} (Enraged: {})",
                     target->GetName(), isEnraged ? "Yes" : "No");
    }
}

bool FurySpecialization::ShouldExecuteRampageWithoutEnrage(::Unit* target)
{
    if (!target)
        return false;

    // Execute Rampage without Enrage if:
    // 1. About to cap on stacks
    // 2. Target is about to die
    // 3. Need to maintain uptime

    uint32 timeSinceLastStack = getMSTime() - _rampageTracker.stackBuildTimes.back();

    // Cap prevention - use if we've had max stacks for too long
    if (timeSinceLastStack > 8000) // 8 seconds
        return true;

    // Target low health
    if (target->GetHealthPct() < 35.0f)
        return true;

    // Long time since last Rampage
    if (getMSTime() - _rampageTracker.lastRampageTime > 15000) // 15 seconds
        return true;

    return false;
}

void FurySpecialization::ExecuteOptimalBloodthirst(::Unit* target)
{
    if (!target)
        return;

    CastBloodthirst(target);
    _lastBloodthirst = getMSTime();

    // Track if Bloodthirst crits for Enrage
    bool willCrit = WillBloodthirstCrit(target);
    if (willCrit)
    {
        _furyMetrics.bloodthirstCrits++;
        _bloodthirstCritReady = true;

        // Prepare for Enrage proc
        if (!_isEnraged.load())
        {
            PrepareForEnrageProc();
        }
    }

    // Add Rampage stack
    _rampageTracker.AddStack();
    _rampageStacks = _rampageTracker.GetStackCount();

    TC_LOG_DEBUG("playerbot.fury", "Bloodthirst cast on {} (Stacks: {}, Crit Ready: {})",
                 target->GetName(), _rampageStacks.load(), willCrit);
}

bool FurySpecialization::WillBloodthirstCrit(::Unit* target)
{
    if (!target)
        return false;

    // Calculate Bloodthirst crit chance
    float baseCrit = CalculateCriticalStrikeChance();

    // Bloodthirst has higher crit chance
    float bloodthirstCrit = baseCrit + 0.1f; // +10% base bonus

    // Enrage increases crit chance
    if (_isEnraged.load())
        bloodthirstCrit += 0.1f; // +10% during Enrage

    // Random determination (simplified)
    return bloodthirstCrit > 0.6f; // Simplified crit check
}

float FurySpecialization::CalculateCriticalStrikeChance()
{
    float baseCrit = 0.05f; // 5% base

    // Add gear crit rating
    float gearCrit = _bot->GetFloatValue(PLAYER_CRIT_PERCENTAGE) / 100.0f;

    // Berserker Stance bonus
    WarriorStance currentStance = GetCurrentStance();
    float stanceCrit = 0.0f;
    if (currentStance == WarriorStance::BERSERKER)
        stanceCrit = 0.03f; // 3% crit bonus

    // Rampage crit bonus
    uint32 rampageStacks = _rampageStacks.load();
    float rampageCrit = rampageStacks * RAMPAGE_CRIT_BONUS;

    return std::min(1.0f, baseCrit + gearCrit + stanceCrit + rampageCrit);
}

void FurySpecialization::PrepareForEnrageProc()
{
    // Prepare rotation for incoming Enrage proc
    TC_LOG_DEBUG("playerbot.fury", "Preparing for Enrage proc - optimizing ability queue");

    // Save rage for Enrage abilities
    if (GetRage() < 60)
    {
        // Conservative rage spending
        return;
    }

    // Position for optimal Enrage usage
    ::Unit* target = _bot->GetTarget();
    if (target && !IsInMeleeRange(target))
    {
        // Move to melee range for Enrage window
        OptimizePositionForEnrage(target);
    }
}

void FurySpecialization::OptimizePositionForEnrage(::Unit* target)
{
    if (!target)
        return;

    float distance = _bot->GetDistance(target);

    // Ensure we're in melee range for Enrage abilities
    if (distance > OPTIMAL_MELEE_RANGE)
    {
        // Use movement abilities if needed
        if (distance >= CHARGE_MIN_RANGE && distance <= CHARGE_MAX_RANGE)
        {
            if (CanUseAbility(CHARGE))
            {
                CastCharge(target);
                TC_LOG_DEBUG("playerbot.fury", "Charging to optimize position for Enrage");
            }
        }
        else if (CanUseAbility(HEROIC_LEAP))
        {
            CastHeroicLeap(target);
            TC_LOG_DEBUG("playerbot.fury", "Heroic Leap to optimize position for Enrage");
        }
    }
}

void FurySpecialization::HandleRampageMechanics(::Unit* target)
{
    if (!target)
        return;

    uint32 currentStacks = _rampageTracker.GetStackCount();
    _rampageStacks.store(currentStacks);

    // Rampage stack management
    if (currentStacks >= RAMPAGE_STACK_REQUIREMENT)
    {
        // Decision logic for Rampage usage
        bool shouldUseRampage = ShouldCastRampage(target);
        bool isEnraged = _isEnraged.load();

        if (shouldUseRampage)
        {
            if (isEnraged || ShouldExecuteRampageWithoutEnrage(target))
            {
                ExecuteOptimalRampage(target);
            }
        }
    }
    else
    {
        // Build stacks with Bloodthirst and other abilities
        OptimizeRampageStacks(target);
    }

    // Track rampage efficiency
    UpdateRampageEfficiencyMetrics();
}

void FurySpecialization::OptimizeRampageStacks(::Unit* target)
{
    if (!target)
        return;

    // Prioritize abilities that build Rampage stacks
    if (ShouldCastBloodthirst(target) && HasEnoughRage(BLOODTHIRST_RAGE_COST))
    {
        ExecuteOptimalBloodthirst(target);
        return;
    }

    if (CanUseAbility(RAGING_BLOW) && HasEnoughRage(RAGING_BLOW_RAGE_COST))
    {
        CastRagingBlow(target);
        _rampageTracker.AddStack();
        return;
    }

    // Furious Slash also builds stacks
    if (CanUseAbility(FURIOUS_SLASH) && HasEnoughRage(10))
    {
        CastFuriousSlash(target);
        _rampageTracker.AddStack();
    }
}

void FurySpecialization::UpdateRampageEfficiencyMetrics()
{
    uint32 totalExecutions = _furyMetrics.rampageExecutions.load();
    uint32 totalStacks = _rampageTracker.totalStacks;

    if (totalStacks > 0)
    {
        float efficiency = static_cast<float>(totalExecutions * 4) / totalStacks; // 4 stacks per rampage
        TC_LOG_DEBUG("playerbot.fury", "Rampage efficiency: {:.2f} (Executions: {}, Stacks: {})",
                     efficiency, totalExecutions, totalStacks);
    }
}

void FurySpecialization::ManageEnrageUptime()
{
    bool isEnraged = _isEnraged.load();
    uint32 currentTime = getMSTime();

    if (isEnraged)
    {
        // Track enrage time
        _furyMetrics.totalEnrageTime += 100; // Approximate per update

        // Check if Enrage is about to expire
        uint32 timeRemaining = GetEnrageTimeRemaining();
        if (timeRemaining < 2000) // 2 seconds remaining
        {
            // Try to extend Enrage
            ::Unit* target = _bot->GetTarget();
            if (target && _rampageTracker.HasMaxStacks())
            {
                ExecuteOptimalRampage(target); // Rampage extends Enrage
            }
        }
    }
    else
    {
        // Not enraged - try to trigger Enrage
        OptimizeEnrageTiming();
    }

    // Update enrage uptime metrics
    UpdateEnrageUptimeMetrics();
}

void FurySpecialization::OptimizeEnrageTiming()
{
    ::Unit* target = _bot->GetTarget();
    if (!target)
        return;

    // Trigger Enrage through:
    // 1. Bloodthirst critical hits
    // 2. Taking damage
    // 3. Berserker Rage ability

    // Bloodthirst for Enrage
    if (ShouldCastBloodthirst(target) && HasEnoughRage(BLOODTHIRST_RAGE_COST))
    {
        ExecuteOptimalBloodthirst(target);
        return;
    }

    // Berserker Rage for guaranteed Enrage
    if (ShouldUseBerserkerRage() && CanUseAbility(BERSERKER_RAGE))
    {
        CastBerserkerRage();
        TriggerEnrage();
        _lastBerserkerRage = getMSTime();
        TC_LOG_DEBUG("playerbot.fury", "Berserker Rage used to trigger Enrage");
    }
}

void FurySpecialization::TriggerEnrage()
{
    if (_isEnraged.load())
        return; // Already enraged

    _isEnraged.store(true);
    _enrageEndTime = getMSTime() + ENRAGE_DURATION;
    _lastEnrageTrigger = getMSTime();
    _enrageCount++;

    TC_LOG_DEBUG("playerbot.fury", "Enrage triggered - entering enhanced damage phase");

    // Optimize for Enrage window
    ExecuteEnragePhase(_bot->GetTarget());
}

void FurySpecialization::ExecuteEnragePhase(::Unit* target)
{
    if (!target || !_isEnraged.load())
        return;

    // Priority during Enrage:
    // 1. Rampage if at 4+ stacks
    // 2. Raging Blow for high damage
    // 3. Continue building stacks
    // 4. Extend Enrage duration

    if (_rampageTracker.HasMaxStacks())
    {
        ExecuteOptimalRampage(target); // Also extends Enrage
        return;
    }

    if (CanUseAbility(RAGING_BLOW) && HasEnoughRage(RAGING_BLOW_RAGE_COST))
    {
        CastRagingBlow(target);
        _rampageTracker.AddStack();
        return;
    }

    if (ShouldCastBloodthirst(target) && HasEnoughRage(BLOODTHIRST_RAGE_COST))
    {
        ExecuteOptimalBloodthirst(target);
        return;
    }

    // Use Execute if available during Enrage
    if (IsInExecutePhase(target) && ShouldCastExecute(target))
    {
        OptimizeExecutePhaseFury(target);
    }
}

void FurySpecialization::ExtendEnrageDuration()
{
    if (!_isEnraged.load())
        return;

    // Rampage extends Enrage duration
    uint32 extension = 4000; // 4 seconds extension
    _enrageEndTime += extension;

    // Cap at maximum duration
    uint32 maxDuration = getMSTime() + ENRAGE_EXTENDED_DURATION;
    if (_enrageEndTime > maxDuration)
        _enrageEndTime = maxDuration;

    TC_LOG_DEBUG("playerbot.fury", "Enrage duration extended by {}ms", extension);
}

void FurySpecialization::HandleEnrageProcs()
{
    // Handle various ways Enrage can proc
    uint32 currentTime = getMSTime();

    // Check if Enrage expired
    if (_isEnraged.load() && currentTime >= _enrageEndTime)
    {
        _isEnraged.store(false);
        TC_LOG_DEBUG("playerbot.fury", "Enrage expired - returning to normal rotation");
    }

    // Check for Bloodthirst crit procs
    if (_bloodthirstCritReady)
    {
        TriggerEnrage();
        _bloodthirstCritReady = false;
    }

    // Auto-attack crits can also trigger Enrage (simplified)
    if (ShouldCheckAutoAttackEnrage())
    {
        float critChance = CalculateCriticalStrikeChance();
        if (critChance > 0.7f) // High crit chance
        {
            TriggerEnrage();
        }
    }
}

bool FurySpecialization::ShouldCheckAutoAttackEnrage()
{
    // Check auto-attack Enrage procs periodically
    static uint32 lastCheck = 0;
    uint32 currentTime = getMSTime();

    if (currentTime - lastCheck > 2000) // Check every 2 seconds
    {
        lastCheck = currentTime;
        return true;
    }

    return false;
}

void FurySpecialization::OptimizeDualWieldAttackSpeed()
{
    if (!HasDualWieldWeapons())
        return;

    // Calculate current dual wield efficiency
    float efficiency = CalculateDualWieldEfficiency();
    _furyMetrics.dualWieldEfficiency.store(efficiency);

    // Optimize attack speed through Flurry
    if (_flurryProc.load() && !_bot->HasAura(FLURRY))
    {
        CastFlurry();
        _flurryProc.store(false);
        TC_LOG_DEBUG("playerbot.fury", "Flurry activated for increased attack speed");
    }

    // Handle dual wield penalties
    HandleDualWieldPenalties();

    // Maximize attack speed bonuses
    MaximizeAttackSpeed();
}

float FurySpecialization::CalculateDualWieldEfficiency()
{
    if (!HasDualWieldWeapons())
        return 0.0f;

    float baseEfficiency = 1.0f;

    // Apply dual wield penalty
    baseEfficiency *= (1.0f - DUAL_WIELD_PENALTY);

    // Add speed bonuses
    baseEfficiency *= (1.0f + DUAL_WIELD_SPEED_BONUS);

    // Flurry bonus
    if (_bot->HasAura(FLURRY))
    {
        uint32 flurryStacks = _flurryStacks.load();
        baseEfficiency *= (1.0f + (flurryStacks * 0.1f)); // 10% per stack
    }

    // Enrage attack speed bonus
    if (_isEnraged.load())
        baseEfficiency *= 1.25f; // 25% attack speed bonus

    return baseEfficiency;
}

void FurySpecialization::HandleDualWieldPenalties()
{
    // Mitigate dual wield hit penalties through talents and abilities

    // Weapon Mastery reduces miss chance
    if (HasTalent(12163)) // Weapon Mastery
    {
        // Already accounted for in hit calculations
        TC_LOG_DEBUG("playerbot.fury", "Weapon Mastery talent mitigating dual wield penalty");
    }

    // Precision talent
    if (HasTalent(12459)) // Precision
    {
        // Reduces miss chance further
        TC_LOG_DEBUG("playerbot.fury", "Precision talent active for hit rating");
    }
}

void FurySpecialization::MaximizeAttackSpeed()
{
    float currentSpeed = CalculateAttackSpeed();
    _furyMetrics.attackSpeedBonus.store(currentSpeed);

    // Use Flurry for attack speed
    if (HasFlurryProc() && !_bot->HasAura(FLURRY))
    {
        CastFlurry();
    }

    // Maintain Enrage for attack speed bonus
    if (!_isEnraged.load())
    {
        OptimizeEnrageTiming();
    }
}

float FurySpecialization::CalculateAttackSpeed()
{
    float baseSpeed = 1.0f;

    // Dual wield speed bonus
    if (HasDualWieldWeapons())
        baseSpeed *= (1.0f + DUAL_WIELD_SPEED_BONUS);

    // Flurry stacks
    if (_bot->HasAura(FLURRY))
    {
        uint32 stacks = _flurryStacks.load();
        baseSpeed *= (1.0f + (stacks * 0.1f)); // 10% per stack
    }

    // Enrage speed bonus
    if (_isEnraged.load())
        baseSpeed *= 1.25f; // 25% speed increase

    // Berserker Stance bonus
    WarriorStance currentStance = GetCurrentStance();
    if (currentStance == WarriorStance::BERSERKER)
        baseSpeed *= 1.1f; // 10% attack speed

    return baseSpeed;
}

void FurySpecialization::OptimizeExecutePhaseFury(::Unit* target)
{
    if (!target || !IsInExecutePhase(target))
        return;

    // Fury execute phase priority:
    // 1. Execute with Enrage active
    // 2. Rampage to extend Enrage
    // 3. Bloodthirst to maintain Enrage
    // 4. Execute without Enrage

    bool isEnraged = _isEnraged.load();

    if (isEnraged && ShouldCastExecute(target))
    {
        CastExecute(target);
        RecordExecuteUsage(target, true);
        return;
    }

    if (_rampageTracker.HasMaxStacks())
    {
        ExecuteOptimalRampage(target); // Extends Enrage
        return;
    }

    if (ShouldCastBloodthirst(target) && HasEnoughRage(BLOODTHIRST_RAGE_COST))
    {
        ExecuteOptimalBloodthirst(target); // Can trigger Enrage
        return;
    }

    if (ShouldCastExecute(target))
    {
        CastExecute(target);
        RecordExecuteUsage(target, false);
    }
}

void FurySpecialization::RecordExecuteUsage(::Unit* target, bool duringEnrage)
{
    TC_LOG_DEBUG("playerbot.fury", "Execute used on {} (During Enrage: {})",
                 target ? target->GetName() : "Unknown", duringEnrage);

    // Track execute efficiency based on Enrage state
    if (duringEnrage)
    {
        // Execute during Enrage is optimal
        float efficiency = _furyMetrics.dualWieldEfficiency.load();
        _furyMetrics.dualWieldEfficiency.store(efficiency + 0.1f);
    }
}

void FurySpecialization::RecordRagingBlowUsage(::Unit* target)
{
    TC_LOG_DEBUG("playerbot.fury", "Raging Blow used on {}", target ? target->GetName() : "Unknown");

    // Add Rampage stack
    _rampageTracker.AddStack();
    _rampageStacks.store(_rampageTracker.GetStackCount());
}

void FurySpecialization::ManageRageEfficiencyFury()
{
    uint32 currentRage = GetRage();
    float ragePercent = static_cast<float>(currentRage) / GetMaxRage();

    // Fury-specific rage management
    if (ragePercent > RAGE_DUMP_THRESHOLD / 100.0f)
    {
        // Rage dump with priority abilities
        ::Unit* target = _bot->GetTarget();
        if (target)
        {
            if (_rampageTracker.HasMaxStacks())
            {
                ExecuteOptimalRampage(target);
            }
            else if (ShouldCastBloodthirst(target))
            {
                ExecuteOptimalBloodthirst(target);
            }
            else if (CanUseAbility(RAGING_BLOW))
            {
                CastRagingBlow(target);
            }
        }
    }
    else if (ragePercent < 0.3f) // 30% rage
    {
        // Rage conservation mode
        ConserveRageForPriorities();
    }
}

void FurySpecialization::ConserveRageForPriorities()
{
    // Save rage for high-priority abilities
    ::Unit* target = _bot->GetTarget();
    if (!target)
        return;

    // Priority 1: Bloodthirst for Enrage
    if (!_isEnraged.load() && ShouldCastBloodthirst(target))
    {
        if (HasEnoughRage(BLOODTHIRST_RAGE_COST))
        {
            ExecuteOptimalBloodthirst(target);
        }
        return;
    }

    // Priority 2: Rampage at max stacks
    if (_rampageTracker.HasMaxStacks())
    {
        if (HasEnoughRage(RAMPAGE_RAGE_COST))
        {
            ExecuteOptimalRampage(target);
        }
        return;
    }

    // Priority 3: Execute in execute phase
    if (IsInExecutePhase(target) && ShouldCastExecute(target))
    {
        if (HasEnoughRage(20)) // Base execute cost
        {
            CastExecute(target);
        }
    }
}

void FurySpecialization::UpdateEnrageUptimeMetrics()
{
    auto currentTime = std::chrono::steady_clock::now();
    auto combatDuration = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - _furyMetrics.combatStartTime);

    if (combatDuration.count() > 0)
    {
        uint32 totalEnrageTime = _furyMetrics.totalEnrageTime.load();
        float uptime = static_cast<float>(totalEnrageTime) / (combatDuration.count() * 1000);
        _furyMetrics.averageEnrageUptime.store(uptime);

        TC_LOG_DEBUG("playerbot.fury", "Enrage uptime: {:.1f}% over {} seconds",
                     uptime * 100.0f, combatDuration.count());
    }
}

void FurySpecialization::UpdateFuryMetrics()
{
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - _furyMetrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 5) // Update every 5 seconds
    {
        // Update all metrics
        UpdateEnrageUptimeMetrics();

        float dualWieldEff = CalculateDualWieldEfficiency();
        _furyMetrics.dualWieldEfficiency.store(dualWieldEff);

        float attackSpeed = CalculateAttackSpeed();
        _furyMetrics.attackSpeedBonus.store(attackSpeed);

        _furyMetrics.lastUpdate = currentTime;

        TC_LOG_DEBUG("playerbot.fury", "Fury metrics updated - Enrage: {:.1f}%, DW Efficiency: {:.2f}, Speed: {:.2f}",
                     _furyMetrics.averageEnrageUptime.load() * 100.0f, dualWieldEff, attackSpeed);
    }
}

// Helper method implementations

bool FurySpecialization::HasDualWieldWeapons()
{
    Item* mainHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    return mainHand && offHand &&
           offHand->GetTemplate()->GetInventoryType() != INVTYPE_SHIELD;
}

bool FurySpecialization::HasFlurryProc()
{
    return _flurryProc.load();
}

void FurySpecialization::CastFlurry()
{
    if (!CanUseAbility(FLURRY))
        return;

    _bot->CastSpell(_bot, FLURRY, false);
    _flurryStacks.store(MAX_FLURRY_STACKS);
    _flurryProc.store(false);

    TC_LOG_DEBUG("playerbot.fury", "Flurry activated with {} stacks", MAX_FLURRY_STACKS);
}

uint32 FurySpecialization::GetEnrageTimeRemaining()
{
    if (!_isEnraged.load())
        return 0;

    uint32 currentTime = getMSTime();
    return _enrageEndTime > currentTime ? _enrageEndTime - currentTime : 0;
}

bool FurySpecialization::HasTalent(uint32 talentId)
{
    // Simplified talent check - in real implementation would check talent trees
    return true; // Assume all Fury talents are available
}

void FurySpecialization::HandleFuryProcs()
{
    // Handle various Fury-specific procs and mechanics

    // Flurry procs from critical hits
    if (ShouldCheckFlurryProc())
    {
        _flurryProc.store(true);
    }

    // Enrage procs
    HandleEnrageProcs();

    // Update proc windows
    UpdateProcWindows();
}

bool FurySpecialization::ShouldCheckFlurryProc()
{
    // Check for Flurry procs periodically
    static uint32 lastFlurryCheck = 0;
    uint32 currentTime = getMSTime();

    if (currentTime - lastFlurryCheck > 1000) // Check every second
    {
        lastFlurryCheck = currentTime;

        // Simplified proc chance based on crit rate
        float critChance = CalculateCriticalStrikeChance();
        return critChance > 0.6f;
    }

    return false;
}

void FurySpecialization::UpdateProcWindows()
{
    // Update various proc windows and timers
    uint32 currentTime = getMSTime();

    // Flurry proc window
    if (_flurryProc.load() && currentTime - _lastBloodthirst > FURY_PROC_WINDOW)
    {
        // Flurry proc expired
        _flurryProc.store(false);
    }

    // Enrage management
    ManageEnrageCooldowns();
}

void FurySpecialization::ManageEnrageCooldowns()
{
    // Manage Enrage-related cooldowns and mechanics

    // Berserker Rage cooldown management
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBerserkerRage > 30000) // 30 second cooldown
    {
        // Berserker Rage is available - consider using for Enrage
        if (!_isEnraged.load() && ShouldUseBerserkerRage())
        {
            OptimizeEnrageTiming();
        }
    }
}

// Enhanced rotation integration
void FurySpecialization::ExecuteEnhancedFuryRotation(::Unit* target)
{
    if (!target)
        return;

    // Update all fury systems
    OptimizeFuryRotation(target);
    UpdateFuryMetrics();
    HandleFuryProcs();
    ManageEnrageUptime();
    OptimizeDualWieldAttackSpeed();

    // Execute the optimized rotation
    if (IsInExecutePhase(target))
    {
        OptimizeExecutePhaseFury(target);
    }
    else
    {
        // Normal rotation based on priorities
        if (_isEnraged.load())
        {
            ExecuteEnragePhase(target);
        }
        else
        {
            // Build toward Enrage and Rampage
            OptimizeRampageStacks(target);
            OptimizeEnrageTiming();
        }
    }
}

} // namespace Playerbot