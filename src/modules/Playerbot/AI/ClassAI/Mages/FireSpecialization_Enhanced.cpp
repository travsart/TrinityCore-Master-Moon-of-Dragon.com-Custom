/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FireSpecialization.h"
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

// Enhanced Fire Specialization Implementation

void FireSpecialization::OptimizeCombustionPhase(::Unit* target)
{
    if (!target || !_inCombustion.load())
        return;

    // Advanced combustion rotation optimization
    uint32 combustionTimeRemaining = _combustionEndTime - getMSTime();
    bool hasHotStreak = _hasHotStreak.load();
    bool hasHeatingUp = _hasHeatingUp.load();

    // Priority during combustion:
    // 1. Instant Pyroblast on Hot Streak
    // 2. Fire Blast to fish for crits
    // 3. Phoenix Flames for guaranteed crit
    // 4. Fireball to maintain casting
    // 5. Scorch if low on time

    if (hasHotStreak)
    {
        // Instant Pyroblast has highest priority
        CastPyroblast();
        _fireMetrics.instantPyroblasts++;
        _fireMetrics.hotStreakProcs++;
        HandleHotStreakProc();
        return;
    }

    // Use Fire Blast to fish for crits
    if (CanUseAbility(FIRE_BLAST) && GetFireBlastCharges() > 0)
    {
        // Save at least one charge if we have Heating Up
        if (!hasHeatingUp || GetFireBlastCharges() > 1)
        {
            CastFireBlast();
            OptimizeFireBlastTiming();
            return;
        }
    }

    // Use Phoenix Flames for guaranteed crit
    if (CanUseAbility(PHOENIX_FLAMES) && GetPhoenixFlamesCharges() > 0)
    {
        CastPhoenixFlames();
        return;
    }

    // Cast Fireball to build up for crits
    if (combustionTimeRemaining > FIREBALL_CAST_TIME && CanUseAbility(FIREBALL))
    {
        CastFireball();
        return;
    }

    // Use Scorch if running out of time
    if (combustionTimeRemaining < FIREBALL_CAST_TIME && CanUseAbility(SCORCH))
    {
        CastScorch();
        return;
    }

    // Update combustion efficiency metrics
    UpdateCombustionEfficiency();
}

void FireSpecialization::PrepareCombustionSetup(::Unit* target)
{
    if (!target || _inCombustion.load())
        return;

    // Prepare for optimal combustion
    bool hasIgnite = _dotTracker.HasIgnite(target->GetGUID());
    uint32 igniteStacks = _dotTracker.GetIgniteStacks(target->GetGUID());

    // Build up ignite stacks before combustion
    if (!hasIgnite || igniteStacks < OPTIMAL_IGNITE_STACKS)
    {
        StackIgniteForCombustion(target);
        return;
    }

    // Ensure we have charges for combustion
    uint32 fireBlastCharges = GetFireBlastCharges();
    uint32 phoenixCharges = GetPhoenixFlamesCharges();

    if (fireBlastCharges < 2)
    {
        // Wait for Fire Blast charges to regenerate
        TC_LOG_DEBUG("playerbot.fire", "Waiting for Fire Blast charges before combustion");
        return;
    }

    // Check if we have Hot Streak ready
    bool hasHotStreak = _hasHotStreak.load();
    if (hasHotStreak)
    {
        // Use Hot Streak before combustion to avoid waste
        CastPyroblast();
        _fireMetrics.instantPyroblasts++;
        return;
    }

    // All conditions met - ready for combustion
    _combustionPrepped = true;
    TC_LOG_DEBUG("playerbot.fire", "Combustion setup complete - ready to cast");
}

void FireSpecialization::ExecuteCombustionRotation(::Unit* target)
{
    if (!target)
        return;

    // Pre-combustion setup
    if (!_inCombustion.load() && IsOptimalCombustionTime())
    {
        if (!_combustionPrepped)
        {
            PrepareCombustionSetup(target);
            return;
        }

        // Cast Combustion
        CastCombustion();
        _combustionState.inCombustion = true;
        _combustionState.combustionStartTime = getMSTime();
        _combustionState.igniteStacksAtStart = _dotTracker.GetIgniteStacks(target->GetGUID());
        _combustionState.combustionTargets.clear();
        _combustionState.combustionTargets.push_back(target);

        _inCombustion.store(true);
        _combustionPrepped = false;
        _fireMetrics.combustionCasts++;

        TC_LOG_DEBUG("playerbot.fire", "Combustion activated with {} ignite stacks",
                     _combustionState.igniteStacksAtStart);
        return;
    }

    // During combustion
    if (_inCombustion.load())
    {
        OptimizeCombustionPhase(target);

        // Check for combustion end
        if (getMSTime() >= _combustionEndTime)
        {
            _inCombustion.store(false);
            _combustionState.Reset();
            TC_LOG_DEBUG("playerbot.fire", "Combustion phase ended");
        }
    }
}

bool FireSpecialization::IsOptimalCombustionTime()
{
    // Determine if conditions are optimal for combustion
    float critChance = CalculateCritChance();

    // Don't use combustion if crit chance is too low
    if (critChance < COMBUSTION_CRIT_THRESHOLD)
        return false;

    // Check mana levels
    if (GetManaPercent() < 0.6f)
        return false;

    // Check if we have sufficient charges
    uint32 fireBlastCharges = GetFireBlastCharges();
    uint32 phoenixCharges = GetPhoenixFlamesCharges();

    if (fireBlastCharges < 2)
        return false;

    // Check if in AoE situation (combustion less effective)
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(10.0f);
    if (nearbyEnemies.size() > 5)
        return false; // Better to use AoE rotation

    return true;
}

uint32 FireSpecialization::CalculateOptimalCombustionDuration()
{
    // Calculate optimal combustion duration based on current conditions
    float critChance = CalculateCritChance();
    uint32 availableCharges = GetFireBlastCharges() + GetPhoenixFlamesCharges();

    uint32 baseDuration = COMBUSTION_DURATION;

    // Extend duration if we have high crit chance
    if (critChance > 0.85f)
        baseDuration += 2000; // +2 seconds

    // Reduce duration if we have limited charges
    if (availableCharges < 3)
        baseDuration -= 2000; // -2 seconds

    return std::max(6000u, std::min(14000u, baseDuration)); // 6-14 seconds range
}

void FireSpecialization::StackIgniteForCombustion(::Unit* target)
{
    if (!target)
        return;

    uint32 currentStacks = _dotTracker.GetIgniteStacks(target->GetGUID());

    // Use direct damage spells to stack ignite
    if (currentStacks < OPTIMAL_IGNITE_STACKS)
    {
        // Use guaranteed crit abilities first
        if (CanUseAbility(PHOENIX_FLAMES) && GetPhoenixFlamesCharges() > 0)
        {
            CastPhoenixFlames();
            return;
        }

        // Use Fire Blast for instant crit
        if (CanUseAbility(FIRE_BLAST) && GetFireBlastCharges() > 1) // Save 1 for combustion
        {
            CastFireBlast();
            return;
        }

        // Cast Fireball to build stacks
        if (CanUseAbility(FIREBALL))
        {
            CastFireball();
            return;
        }

        // Use Scorch if Fireball is not available
        if (CanUseAbility(SCORCH))
        {
            CastScorch();
        }
    }
}

void FireSpecialization::OptimizeCritFishing()
{
    // Optimize critical hit fishing for Hot Streak procs
    bool hasHeatingUp = _hasHeatingUp.load();
    bool hasHotStreak = _hasHotStreak.load();

    if (hasHotStreak)
    {
        // Use Hot Streak immediately
        HandleHotStreakProc();
        return;
    }

    if (hasHeatingUp)
    {
        // Fish for the second crit
        if (CanUseAbility(FIRE_BLAST) && GetFireBlastCharges() > 0)
        {
            CastFireBlast();
            OptimizeFireBlastTiming();
            return;
        }

        if (CanUseAbility(PHOENIX_FLAMES) && GetPhoenixFlamesCharges() > 0)
        {
            CastPhoenixFlames();
            return;
        }
    }

    // No procs active - build for Heating Up
    if (CanUseAbility(FIREBALL))
    {
        CastFireball();
    }
    else if (CanUseAbility(SCORCH))
    {
        CastScorch();
    }
}

void FireSpecialization::HandleHotStreakProc()
{
    if (!_hasHotStreak.load())
        return;

    // Use Hot Streak for instant Pyroblast
    CastPyroblast();
    _fireMetrics.instantPyroblasts++;
    _fireMetrics.hotStreakProcs++;

    // Reset Hot Streak state
    _hasHotStreak.store(false);
    _lastPyroblastTime = getMSTime();

    TC_LOG_DEBUG("playerbot.fire", "Hot Streak proc consumed - instant Pyroblast cast");
}

void FireSpecialization::HandleHeatingUpProc()
{
    if (!_hasHeatingUp.load())
        return;

    _fireMetrics.heatingUpProcs++;

    // Immediately try to convert to Hot Streak
    if (CanUseAbility(FIRE_BLAST) && GetFireBlastCharges() > 0)
    {
        CastFireBlast();
        TC_LOG_DEBUG("playerbot.fire", "Converting Heating Up to Hot Streak with Fire Blast");
    }
    else if (CanUseAbility(PHOENIX_FLAMES) && GetPhoenixFlamesCharges() > 0)
    {
        CastPhoenixFlames();
        TC_LOG_DEBUG("playerbot.fire", "Converting Heating Up to Hot Streak with Phoenix Flames");
    }
}

void FireSpecialization::ChainPyroblasts(::Unit* target)
{
    if (!target)
        return;

    // Chain multiple Pyroblasts during Hot Streak windows
    static uint32 lastPyroblastChain = 0;
    uint32 currentTime = getMSTime();

    if (currentTime - lastPyroblastChain < 2000) // 2 second cooldown between chains
        return;

    if (_hasHotStreak.load())
    {
        CastPyroblast();
        lastPyroblastChain = currentTime;

        // Try to immediately get another Hot Streak
        if (CanUseAbility(FIRE_BLAST) && GetFireBlastCharges() > 0)
        {
            // Delay Fire Blast slightly to ensure Pyroblast hits first
            // In a real implementation, this would be handled by a spell queue
            CastFireBlast();
        }
    }
}

void FireSpecialization::OptimizeFireBlastTiming()
{
    // Optimize Fire Blast usage for maximum efficiency
    uint32 charges = GetFireBlastCharges();
    bool hasHeatingUp = _hasHeatingUp.load();
    bool inCombustion = _inCombustion.load();

    // Don't waste charges
    if (charges == 0)
        return;

    // High priority uses
    if (hasHeatingUp && !_hasHotStreak.load())
    {
        // Convert Heating Up to Hot Streak
        CastFireBlast();
        TC_LOG_DEBUG("playerbot.fire", "Fire Blast used to convert Heating Up");
        return;
    }

    if (inCombustion && charges > 0)
    {
        // Use for crit fishing during combustion
        CastFireBlast();
        TC_LOG_DEBUG("playerbot.fire", "Fire Blast used during combustion");
        return;
    }

    // Conservative use - only if we have multiple charges
    if (charges >= 2)
    {
        CastFireBlast();
        TC_LOG_DEBUG("playerbot.fire", "Fire Blast used with {} charges available", charges);
    }
}

void FireSpecialization::OptimizeIgniteStacking(::Unit* target)
{
    if (!target)
        return;

    ObjectGuid targetGuid = target->GetGUID();
    uint32 currentStacks = _dotTracker.GetIgniteStacks(targetGuid);
    bool hasIgnite = _dotTracker.HasIgnite(targetGuid);

    // Apply or refresh ignite
    if (!hasIgnite)
    {
        ApplyIgnite(target);
        TC_LOG_DEBUG("playerbot.fire", "Applying initial ignite to {}", target->GetName());
    }
    else if (currentStacks < MAX_IGNITE_STACKS)
    {
        // Stack ignite higher
        if (CanUseAbility(FIREBALL))
        {
            CastFireball();
        }
        else if (CanUseAbility(FIRE_BLAST) && GetFireBlastCharges() > 1)
        {
            CastFireBlast();
        }
    }

    // Snapshot ignite for combustion
    if (currentStacks >= OPTIMAL_IGNITE_STACKS && !_inCombustion.load())
    {
        HandleIgniteSnapshot(target);
    }
}

void FireSpecialization::HandleIgniteSnapshot(::Unit* target)
{
    if (!target)
        return;

    // Take a snapshot of current ignite for combustion calculations
    ObjectGuid targetGuid = target->GetGUID();
    uint32 igniteStacks = _dotTracker.GetIgniteStacks(targetGuid);

    if (igniteStacks >= OPTIMAL_IGNITE_STACKS)
    {
        // Store snapshot data for combustion optimization
        _combustionState.igniteStacksAtStart = igniteStacks;

        TC_LOG_DEBUG("playerbot.fire", "Ignite snapshot taken: {} stacks on {}",
                     igniteStacks, target->GetName());
    }
}

void FireSpecialization::ManageLivingBombSpread(const std::vector<::Unit*>& targets)
{
    if (targets.empty())
        return;

    // Intelligent Living Bomb spreading
    uint32 targetsWithBomb = 0;
    ::Unit* spreadTarget = nullptr;

    for (::Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        if (_dotTracker.HasLivingBomb(target->GetGUID()))
        {
            targetsWithBomb++;
        }
        else if (!spreadTarget)
        {
            spreadTarget = target;
        }
    }

    // Apply Living Bomb to new targets
    if (spreadTarget && targetsWithBomb < 3 && CanUseAbility(LIVING_BOMB))
    {
        CastLivingBomb(spreadTarget);
        TC_LOG_DEBUG("playerbot.fire", "Spreading Living Bomb to {}", spreadTarget->GetName());
    }

    // Refresh expiring Living Bombs
    for (::Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        ObjectGuid targetGuid = target->GetGUID();
        if (_dotTracker.HasLivingBomb(targetGuid))
        {
            uint32 timeRemaining = GetLivingBombTimeRemaining(targetGuid);
            if (timeRemaining < 3000 && CanUseAbility(LIVING_BOMB)) // 3 seconds remaining
            {
                CastLivingBomb(target);
                TC_LOG_DEBUG("playerbot.fire", "Refreshing Living Bomb on {}", target->GetName());
                break; // One refresh per update
            }
        }
    }
}

void FireSpecialization::HandleAoERotation(const std::vector<::Unit*>& targets)
{
    if (targets.size() < AOE_THRESHOLD)
        return;

    // Optimize AoE rotation for multiple targets
    Position optimalPosition = CalculateOptimalFlamestrikePosition(targets);

    // Use Flamestrike for grouped enemies
    if (targets.size() >= 3 && CanUseAbility(FLAMESTRIKE))
    {
        OptimizeFlamestrikePlacement(targets);
        return;
    }

    // Use Dragon's Breath for close enemies
    if (targets.size() >= 2)
    {
        HandleDragonBreathTiming(targets);
    }

    // Manage AoE ignites
    ManageAoEIgnites(targets);

    // Spread Living Bombs
    SpreadLivingBombs(targets);

    // Use Blast Wave if available
    if (targets.size() >= 4 && CanUseAbility(BLAST_WAVE))
    {
        CastBlastWave();
        TC_LOG_DEBUG("playerbot.fire", "Using Blast Wave for {} targets", targets.size());
    }

    // Use Meteor for large groups
    if (targets.size() >= 5 && CanUseAbility(METEOR))
    {
        CastMeteor();
        TC_LOG_DEBUG("playerbot.fire", "Using Meteor for {} targets", targets.size());
    }
}

Position FireSpecialization::CalculateOptimalFlamestrikePosition(const std::vector<::Unit*>& targets)
{
    if (targets.empty())
        return Position();

    // Calculate the center point of all targets
    float totalX = 0.0f, totalY = 0.0f, totalZ = 0.0f;
    uint32 validTargets = 0;

    for (::Unit* target : targets)
    {
        if (target && target->IsAlive())
        {
            totalX += target->GetPositionX();
            totalY += target->GetPositionY();
            totalZ += target->GetPositionZ();
            validTargets++;
        }
    }

    if (validTargets == 0)
        return Position();

    Position centerPos(totalX / validTargets, totalY / validTargets, totalZ / validTargets);

    // Validate the position is within range
    if (_bot->GetDistance(centerPos) <= SCORCH_RANGE)
    {
        return centerPos;
    }

    return Position(); // Invalid position
}

void FireSpecialization::OptimizeFlamestrikePlacement(const std::vector<::Unit*>& targets)
{
    Position optimalPos = CalculateOptimalFlamestrikePosition(targets);

    if (optimalPos.IsPositionValid() && CanUseAbility(FLAMESTRIKE))
    {
        _bot->CastSpell(optimalPos.GetPositionX(), optimalPos.GetPositionY(),
                       optimalPos.GetPositionZ(), FLAMESTRIKE, false);

        TC_LOG_DEBUG("playerbot.fire", "Flamestrike cast at optimal position for {} targets",
                     targets.size());
    }
}

void FireSpecialization::HandleDragonBreathTiming(const std::vector<::Unit*>& targets)
{
    // Check if targets are in cone range
    uint32 targetsInRange = 0;
    for (::Unit* target : targets)
    {
        if (target && target->IsAlive() && _bot->GetDistance(target) <= 12.0f)
        {
            targetsInRange++;
        }
    }

    if (targetsInRange >= 2 && CanUseAbility(DRAGONS_BREATH))
    {
        CastDragonsBreath();
        TC_LOG_DEBUG("playerbot.fire", "Dragon's Breath used on {} targets", targetsInRange);
    }
}

void FireSpecialization::ManageAoEIgnites(const std::vector<::Unit*>& targets)
{
    // Manage ignite spreading in AoE situations
    uint32 targetsWithIgnite = 0;

    for (::Unit* target : targets)
    {
        if (target && target->IsAlive() && _dotTracker.HasIgnite(target->GetGUID()))
        {
            targetsWithIgnite++;
        }
    }

    // If most targets have ignite, use AoE spells to spread/maintain
    if (targetsWithIgnite >= targets.size() / 2)
    {
        if (CanUseAbility(FLAMESTRIKE))
        {
            OptimizeFlamestrikePlacement(targets);
        }
    }
    else
    {
        // Focus on single target to build strong ignite
        ::Unit* primaryTarget = GetHighestPriorityTarget(targets);
        if (primaryTarget)
        {
            OptimizeIgniteStacking(primaryTarget);
        }
    }
}

void FireSpecialization::SpreadLivingBombs(const std::vector<::Unit*>& targets)
{
    // Spread Living Bombs across multiple targets
    ManageLivingBombSpread(targets);
}

::Unit* FireSpecialization::GetHighestPriorityTarget(const std::vector<::Unit*>& targets)
{
    if (targets.empty())
        return nullptr;

    ::Unit* bestTarget = nullptr;
    float bestPriority = 0.0f;

    for (::Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        float priority = CalculateTargetPriority(target);
        if (priority > bestPriority)
        {
            bestPriority = priority;
            bestTarget = target;
        }
    }

    return bestTarget;
}

float FireSpecialization::CalculateTargetPriority(::Unit* target)
{
    if (!target)
        return 0.0f;

    float priority = 1.0f;

    // Higher priority for lower health
    float healthPct = target->GetHealthPct();
    if (healthPct < 30.0f)
        priority += 2.0f;
    else if (healthPct < 60.0f)
        priority += 1.0f;

    // Higher priority for targets without ignite
    if (!_dotTracker.HasIgnite(target->GetGUID()))
        priority += 1.5f;

    // Higher priority for closer targets
    float distance = _bot->GetDistance(target);
    if (distance < 15.0f)
        priority += 1.0f;

    // Higher priority for casters
    if (target->HasUnitState(UNIT_STATE_CASTING))
        priority += 1.5f;

    return priority;
}

float FireSpecialization::CalculateFireDamageBonus(::Unit* target)
{
    if (!target)
        return 1.0f;

    float bonus = 1.0f;

    // Bonus damage against targets with ignite
    if (_dotTracker.HasIgnite(target->GetGUID()))
    {
        uint32 stacks = _dotTracker.GetIgniteStacks(target->GetGUID());
        bonus += stacks * 0.1f; // 10% per stack
    }

    // Bonus during combustion
    if (_inCombustion.load())
        bonus += 0.25f; // 25% combustion bonus

    // Critical Mass bonus
    if (HasCriticalMass())
        bonus += 0.15f; // 15% Critical Mass bonus

    return bonus;
}

void FireSpecialization::UpdateIgniteTracking()
{
    // Update ignite tracking and cleanup expired entries
    uint32 currentTime = getMSTime();

    auto it = _dotTracker.igniteExpireTimes.begin();
    while (it != _dotTracker.igniteExpireTimes.end())
    {
        if (it->second <= currentTime)
        {
            // Ignite expired
            ObjectGuid expiredGuid = it->first;
            _dotTracker.igniteStacks.erase(expiredGuid);
            it = _dotTracker.igniteExpireTimes.erase(it);

            TC_LOG_DEBUG("playerbot.fire", "Ignite expired on target {}", expiredGuid.ToString());
        }
        else
        {
            ++it;
        }
    }

    // Update Living Bomb tracking
    auto bombIt = _dotTracker.livingBombExpireTimes.begin();
    while (bombIt != _dotTracker.livingBombExpireTimes.end())
    {
        if (bombIt->second <= currentTime)
        {
            ObjectGuid expiredGuid = bombIt->first;
            bombIt = _dotTracker.livingBombExpireTimes.erase(bombIt);

            TC_LOG_DEBUG("playerbot.fire", "Living Bomb expired on target {}", expiredGuid.ToString());
        }
        else
        {
            ++bombIt;
        }
    }

    // Update ignite uptime metrics
    float totalUptime = 0.0f;
    uint32 totalTargets = _dotTracker.igniteExpireTimes.size();

    if (totalTargets > 0)
    {
        totalUptime = static_cast<float>(totalTargets) / 10.0f; // Simplified calculation
        _fireMetrics.igniteUptime = (totalUptime + _fireMetrics.igniteUptime.load()) / 2.0f;
    }
}

void FireSpecialization::UpdateCombustionEfficiency()
{
    if (!_inCombustion.load())
        return;

    uint32 combustionDuration = getMSTime() - _combustionState.combustionStartTime;
    float efficiency = 1.0f;

    // Calculate efficiency based on damage dealt during combustion
    if (combustionDuration > 0)
    {
        float damagePerSecond = static_cast<float>(_combustionState.damageDealtDuringCombustion) / (combustionDuration / 1000.0f);

        // Compare against expected DPS (simplified)
        float expectedDPS = 1000.0f; // Base expected DPS
        efficiency = std::min(2.0f, damagePerSecond / expectedDPS);
    }

    _fireMetrics.combustionEfficiency = efficiency;
}

float FireSpecialization::CalculateCritChance()
{
    // Calculate current critical hit chance
    // This would normally come from character stats
    float baseCrit = 0.05f; // 5% base

    // Add gear crit rating (simplified)
    float gearCrit = _bot->GetFloatValue(PLAYER_CRIT_PERCENTAGE) / 100.0f;

    // Add buffs and talents
    float buffCrit = 0.0f;
    if (HasCriticalMass())
        buffCrit += 0.15f; // 15% from Critical Mass

    return std::min(1.0f, baseCrit + gearCrit + buffCrit);
}

uint32 FireSpecialization::GetFireBlastCharges()
{
    // Get current Fire Blast charges
    // This would normally check the spell charge system
    return FIRE_BLAST_CHARGES; // Simplified - return max charges
}

uint32 FireSpecialization::GetPhoenixFlamesCharges()
{
    // Get current Phoenix Flames charges
    return PHOENIX_FLAMES_CHARGES; // Simplified - return max charges
}

uint32 FireSpecialization::GetLivingBombTimeRemaining(ObjectGuid targetGuid)
{
    auto it = _dotTracker.livingBombExpireTimes.find(targetGuid);
    if (it != _dotTracker.livingBombExpireTimes.end())
    {
        uint32 expireTime = it->second;
        uint32 currentTime = getMSTime();
        return expireTime > currentTime ? expireTime - currentTime : 0;
    }
    return 0;
}

void FireSpecialization::HandleCombustionEmergency()
{
    // Handle emergency situations during combustion
    if (!_inCombustion.load())
        return;

    // Health emergency
    if (_bot->GetHealthPct() < 20.0f)
    {
        // Use defensive abilities
        if (CanUseAbility(MIRROR_IMAGE))
        {
            CastMirrorImage();
            TC_LOG_DEBUG("playerbot.fire", "Emergency Mirror Image during combustion");
        }

        // Consider early combustion exit if critically low
        if (_bot->GetHealthPct() < 10.0f)
        {
            _inCombustion.store(false);
            _combustionState.Reset();
            TC_LOG_DEBUG("playerbot.fire", "Emergency combustion termination - critical health");
        }
    }

    // Mana emergency
    if (GetManaPercent() < 0.2f)
    {
        // Use mana-efficient spells only
        if (CanUseAbility(FIRE_BLAST))
        {
            CastFireBlast();
        }
        else if (CanUseAbility(PHOENIX_FLAMES))
        {
            CastPhoenixFlames();
        }
    }

    // Target lost
    if (_combustionState.combustionTargets.empty() ||
        !_combustionState.combustionTargets[0] ||
        !_combustionState.combustionTargets[0]->IsAlive())
    {
        TC_LOG_DEBUG("playerbot.fire", "Combustion target lost - seeking new target");

        // Try to find new target
        std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(30.0f);
        if (!nearbyEnemies.empty())
        {
            _combustionState.combustionTargets.clear();
            _combustionState.combustionTargets.push_back(nearbyEnemies[0]);
        }
        else
        {
            // No targets available - end combustion
            _inCombustion.store(false);
            _combustionState.Reset();
        }
    }
}

} // namespace Playerbot