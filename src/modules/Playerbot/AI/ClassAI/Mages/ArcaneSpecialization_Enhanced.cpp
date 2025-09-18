/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArcaneSpecialization.h"
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

// Enhanced method implementations for ArcaneSpecialization

void ArcaneSpecialization::OptimizeBurnPhaseRotation(::Unit* target)
{
    if (!target || !_inBurnPhase.load())
        return;

    uint32 currentCharges = GetArcaneCharges();
    bool hasClearcasting = HasClearcastingProc();
    bool hasArcanePower = HasArcanePower();

    // Optimal burn phase priority:
    // 1. Arcane Power + Presence of Mind if available
    // 2. Arcane Orb if charges < 4 and high efficiency
    // 3. Arcane Blast to 4 charges
    // 4. Arcane Missiles on Clearcasting
    // 5. Arcane Barrage to reset and repeat

    if (!hasArcanePower && CanUseAbility(ARCANE_POWER))
    {
        CastArcanePower();
        _metrics.burnPhaseEfficiency += 0.2f;
        return;
    }

    if (!HasPresenceOfMind() && CanUseAbility(PRESENCE_OF_MIND))
    {
        CastPresenceOfMind();
        return;
    }

    // Use Arcane Orb for efficiency when building charges
    if (currentCharges < 3 && CanUseAbility(ARCANE_ORB))
    {
        float orbEfficiency = CalculateArcaneOrbEfficiency(target);
        if (orbEfficiency > ARCANE_ORB_EFFICIENCY)
        {
            CastArcaneOrb();
            UpdateArcaneOrb();
            return;
        }
    }

    // Priority casting based on charges and procs
    if (hasClearcasting && currentCharges >= 2)
    {
        CastArcaneMissiles();
        _metrics.totalArcaneBlasts++;
        return;
    }

    if (currentCharges < ARCANE_BLAST_MAX_STACKS)
    {
        if (!ShouldDelayArcaneBlast())
        {
            CastArcaneBlast();
            _arcaneBlastStacks++;
            _metrics.totalArcaneBlasts++;

            if (currentCharges == 4)
                _metrics.fourStackBlasts++;
        }
        return;
    }

    // At 4 charges, decide whether to continue or barrage
    if (currentCharges == ARCANE_BLAST_MAX_STACKS)
    {
        if (ShouldExtendBurnPhase() && GetManaPercent() > 0.4f)
        {
            if (hasClearcasting)
            {
                CastArcaneMissiles();
            }
            else
            {
                CastArcaneBlast(); // Maintain 4 charges
                _metrics.fourStackBlasts++;
            }
        }
        else
        {
            CastArcaneBarrage();
            _arcaneBlastStacks = 0; // Reset charges
        }
    }
}

void ArcaneSpecialization::OptimizeConservePhaseRotation(::Unit* target)
{
    if (!target || _inBurnPhase.load())
        return;

    uint32 currentCharges = GetArcaneCharges();
    bool hasClearcasting = HasClearcastingProc();

    // Conservative rotation optimization:
    // 1. Use mana gems if very low on mana
    // 2. Cast Arcane Blast on Clearcasting to 2 charges
    // 3. Cast Arcane Missiles to spend Clearcasting
    // 4. Cast Arcane Barrage at 2+ charges
    // 5. Maintain Arcane Intellect and Mana Shield

    if (GetManaPercent() < MANA_GEM_THRESHOLD && ShouldUseManaGem())
    {
        UseManaGem();
        HandleManaRegeneration();
        return;
    }

    // Use Clearcasting efficiently
    if (hasClearcasting)
    {
        if (currentCharges < 2)
        {
            CastArcaneBlast();
            _arcaneBlastStacks++;
            return;
        }
        else
        {
            CastArcaneMissiles();
            OptimizeArcaneMissilesTiming();
            return;
        }
    }

    // Build to 2 charges, then barrage
    if (currentCharges < 2 && HasEnoughMana(200)) // Conservative mana check
    {
        CastArcaneBlast();
        _arcaneBlastStacks++;
    }
    else if (currentCharges >= 2)
    {
        CastArcaneBarrage();
        _arcaneBlastStacks = 0;
    }

    // Update efficiency metrics
    float currentEfficiency = CalculateManaEfficiency();
    _metrics.manaEfficiency = (currentEfficiency + _metrics.manaEfficiency.load()) / 2.0f;
}

void ArcaneSpecialization::CalculateOptimalBurnDuration()
{
    float currentMana = GetManaPercent();
    uint32 estimatedBurnTime = 0;

    if (currentMana >= 0.9f)
    {
        estimatedBurnTime = MAX_BURN_DURATION;
    }
    else if (currentMana >= 0.8f)
    {
        estimatedBurnTime = OPTIMAL_BURN_DURATION;
    }
    else if (currentMana >= 0.6f)
    {
        estimatedBurnTime = static_cast<uint32>(OPTIMAL_BURN_DURATION * 0.7f);
    }
    else
    {
        // Not enough mana for effective burn phase
        estimatedBurnTime = 0;
    }

    _burnPhaseDuration = estimatedBurnTime;
    TC_LOG_DEBUG("playerbot.arcane", "Calculated optimal burn duration: {}ms for {:.1f}% mana",
                 estimatedBurnTime, currentMana * 100.0f);
}

bool ArcaneSpecialization::ShouldExtendBurnPhase()
{
    uint32 currentTime = getMSTime();
    uint32 burnDuration = currentTime - _burnPhaseStartTime;

    // Don't extend if we've been burning too long
    if (burnDuration > MAX_BURN_DURATION)
        return false;

    // Don't extend if mana is critically low
    if (GetManaPercent() < BURN_EXIT_THRESHOLD)
        return false;

    // Extend if we have good mana and high DPS potential
    bool hasGoodMana = GetManaPercent() > 0.4f;
    bool hasHighCharges = GetArcaneCharges() == 4;
    bool hasCooldowns = HasArcanePower() || HasPresenceOfMind();

    return hasGoodMana && (hasHighCharges || hasCooldowns);
}

bool ArcaneSpecialization::ShouldTransitionToBurn()
{
    float currentMana = GetManaPercent();
    uint32 conserveDuration = getMSTime() - _conservePhaseStartTime;

    // Minimum conserve time
    if (conserveDuration < MIN_CONSERVE_DURATION)
        return false;

    // Mana threshold check
    if (currentMana < BURN_ENTRY_THRESHOLD)
        return false;

    // Check if major cooldowns are available for optimal burn
    bool hasArcanePower = CanUseAbility(ARCANE_POWER);
    bool hasPresenceOfMind = CanUseAbility(PRESENCE_OF_MIND);

    // Prefer to burn when cooldowns are available
    if (hasArcanePower || hasPresenceOfMind)
        return true;

    // Emergency burn if mana is nearly full
    return currentMana >= 0.95f;
}

void ArcaneSpecialization::HandleBurnPhaseEmergency()
{
    // Emergency situations during burn phase
    if (!_inBurnPhase.load())
        return;

    float manaPct = GetManaPercent();
    uint32 burnDuration = getMSTime() - _burnPhaseStartTime;

    // Critical mana shortage
    if (manaPct < 0.1f)
    {
        // Immediately use mana gem
        if (ShouldUseManaGem())
        {
            UseManaGem();
        }

        // Switch to conserve phase
        EnterConservePhase();
        TC_LOG_DEBUG("playerbot.arcane", "Emergency exit from burn phase - critical mana");
        return;
    }

    // Burn phase too long
    if (burnDuration > MAX_BURN_DURATION)
    {
        EnterConservePhase();
        TC_LOG_DEBUG("playerbot.arcane", "Emergency exit from burn phase - maximum duration reached");
        return;
    }

    // Health emergency during burn
    if (_bot->GetHealthPct() < 20.0f)
    {
        // Use defensive abilities
        if (!_bot->HasAura(MANA_SHIELD))
            CastManaShield();

        // Consider early exit if very low health
        if (_bot->GetHealthPct() < 10.0f)
        {
            EnterConservePhase();
            TC_LOG_DEBUG("playerbot.arcane", "Emergency exit from burn phase - critical health");
        }
    }
}

void ArcaneSpecialization::HandleManaRegeneration()
{
    float manaPct = GetManaPercent();

    // Use Evocation if available and mana is very low
    uint32 evocationSpell = 12051;
    if (manaPct < 0.2f && CanUseAbility(evocationSpell))
    {
        _bot->CastSpell(_bot, evocationSpell, false);
        TC_LOG_DEBUG("playerbot.arcane", "Using Evocation for mana regeneration");
        return;
    }

    // Use mana gems strategically
    if (manaPct < MANA_GEM_THRESHOLD && ShouldUseManaGem())
    {
        UseManaGem();
        _metrics.manaEfficiency += 0.1f;
    }

    // Maintain Arcane Intellect for mana pool
    if (!_bot->HasAura(ARCANE_INTELLECT))
    {
        HandleArcaneIntellectBuff();
    }
}

void ArcaneSpecialization::ManageArcaneChargeOptimization()
{
    uint32 currentCharges = GetArcaneCharges();
    uint32 currentTime = getMSTime();

    // Track average charges for optimization
    float currentAvg = _metrics.averageCharges.load();
    float newAvg = (currentAvg * 0.9f) + (currentCharges * 0.1f);
    _metrics.averageCharges.store(newAvg);

    // Detect wasted charges (charges expiring)
    static uint32 lastChargeCheck = 0;
    static uint32 lastChargeCount = 0;

    if (currentTime - lastChargeCheck > 1000) // Check every second
    {
        if (lastChargeCount > currentCharges && currentCharges == 0)
        {
            _metrics.wastedCharges++;
            TC_LOG_DEBUG("playerbot.arcane", "Detected wasted Arcane Charges");
        }

        lastChargeCheck = currentTime;
        lastChargeCount = currentCharges;
    }

    // Optimize charge timing
    if (currentCharges > 0)
    {
        uint32 chargeTimeRemaining = GetArcaneChargeTimeRemaining();
        if (chargeTimeRemaining < 3000 && chargeTimeRemaining > 0) // 3 seconds warning
        {
            // Try to spend charges before they expire
            if (_inConservePhase.load() && CanUseAbility(ARCANE_BARRAGE))
            {
                CastArcaneBarrage();
                TC_LOG_DEBUG("playerbot.arcane", "Using Arcane Barrage to prevent charge waste");
            }
        }
    }
}

uint32 ArcaneSpecialization::GetArcaneChargeTimeRemaining()
{
    Aura* chargeAura = _bot->GetAura(ARCANE_CHARGES);
    if (!chargeAura)
        return 0;

    uint32 duration = chargeAura->GetDuration();
    return duration;
}

bool ArcaneSpecialization::ShouldDelayArcaneBlast()
{
    // Delay Arcane Blast in specific situations
    uint32 currentCharges = GetArcaneCharges();

    // Don't delay if we have no charges
    if (currentCharges == 0)
        return false;

    // Delay if Clearcasting is about to proc (heuristic)
    if (currentCharges >= 2 && GetManaPercent() > 0.8f)
    {
        // Check if we haven't had clearcasting recently
        static uint32 lastClearcastingTime = 0;
        uint32 currentTime = getMSTime();

        if (currentTime - lastClearcastingTime > 15000) // 15 seconds
        {
            return true; // Delay for potential proc
        }
    }

    // Delay if we're near max charges and want to use Arcane Orb
    if (currentCharges == 3 && CanUseAbility(ARCANE_ORB))
    {
        return true;
    }

    return false;
}

void ArcaneSpecialization::UpdateArcaneOrb()
{
    // Track Arcane Orb usage and effectiveness
    static uint32 orbsCast = 0;
    static uint32 orbsHit = 0;

    orbsCast++;

    // Simplified hit detection (would need proper spell hit tracking)
    if (orbsCast % 10 == 0) // Assume 90% hit rate
    {
        orbsHit += 9;
        float hitRate = static_cast<float>(orbsHit) / orbsCast;
        TC_LOG_DEBUG("playerbot.arcane", "Arcane Orb hit rate: {:.1f}%", hitRate * 100.0f);
    }
}

float ArcaneSpecialization::CalculateArcaneOrbEfficiency(::Unit* target)
{
    if (!target)
        return 0.0f;

    // Calculate orb efficiency based on target distance and movement
    float distance = _bot->GetDistance(target);
    float efficiency = 1.0f;

    // Orb is more efficient at medium range
    if (distance > 10.0f && distance < 25.0f)
        efficiency += 0.3f;

    // Less efficient against moving targets
    if (target->HasUnitState(UNIT_STATE_MOVING))
        efficiency -= 0.2f;

    // More efficient if target is slowed
    if (target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
        efficiency += 0.2f;

    return efficiency;
}

void ArcaneSpecialization::OptimizeArcaneMissilesTiming()
{
    // Optimize Arcane Missiles usage for maximum efficiency
    bool hasClearcasting = HasClearcastingProc();

    if (!hasClearcasting)
    {
        TC_LOG_DEBUG("playerbot.arcane", "Warning: Casting Arcane Missiles without Clearcasting");
        return;
    }

    // Track missile timing for optimization
    static uint32 lastMissileTime = 0;
    uint32 currentTime = getMSTime();

    if (currentTime - lastMissileTime < 3000) // 3 second minimum between missiles
    {
        TC_LOG_DEBUG("playerbot.arcane", "Delaying Arcane Missiles for optimal timing");
        return;
    }

    lastMissileTime = currentTime;

    // Use missiles immediately if at 4 charges
    if (GetArcaneCharges() == 4)
    {
        CastArcaneMissiles();
        return;
    }

    // Use missiles to prevent clearcasting waste
    uint32 clearcastingTimeRemaining = GetClearcastingTimeRemaining();
    if (clearcastingTimeRemaining < 5000) // 5 seconds remaining
    {
        CastArcaneMissiles();
    }
}

uint32 ArcaneSpecialization::GetClearcastingTimeRemaining()
{
    Aura* clearcastingAura = _bot->GetAura(CLEARCASTING);
    if (!clearcastingAura)
        return 0;

    return clearcastingAura->GetDuration();
}

void ArcaneSpecialization::HandleArcaneIntellectBuff()
{
    // Enhanced Arcane Intellect management
    if (_bot->HasAura(ARCANE_INTELLECT))
        return;

    // Cast on self
    if (CanUseAbility(ARCANE_INTELLECT))
    {
        _bot->CastSpell(_bot, ARCANE_INTELLECT, false);
        TC_LOG_DEBUG("playerbot.arcane", "Casting Arcane Intellect on self");
    }

    // Cast on group members if in group
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive() && !member->HasAura(ARCANE_INTELLECT))
            {
                if (_bot->GetDistance(member) <= 30.0f)
                {
                    _bot->CastSpell(member, ARCANE_INTELLECT, false);
                    TC_LOG_DEBUG("playerbot.arcane", "Casting Arcane Intellect on {}", member->GetName());
                    break; // One cast per update to avoid spam
                }
            }
        }
    }
}

void ArcaneSpecialization::HandleManaAdeptProc()
{
    // Handle Mana Adept talent proc (if available)
    uint32 manaAdeptSpell = 92506; // Mana Adept proc

    if (_bot->HasAura(manaAdeptSpell))
    {
        // Prioritize high-damage spells during proc
        if (CanUseAbility(ARCANE_BLAST))
        {
            CastArcaneBlast();
            TC_LOG_DEBUG("playerbot.arcane", "Using Arcane Blast with Mana Adept proc");
        }
    }
}

void ArcaneSpecialization::UpdateTimeWarpEffects()
{
    // Handle Time Warp and similar haste effects
    uint32 timeWarpSpell = 80353;

    if (_bot->HasAura(timeWarpSpell))
    {
        // Optimize rotation during Time Warp
        if (_inConservePhase.load() && GetManaPercent() > 0.6f)
        {
            // Switch to burn phase during Time Warp
            EnterBurnPhase();
            TC_LOG_DEBUG("playerbot.arcane", "Entering burn phase during Time Warp");
        }

        // Use cooldowns during Time Warp
        if (!HasArcanePower() && CanUseAbility(ARCANE_POWER))
        {
            CastArcanePower();
        }
    }
}

void ArcaneSpecialization::OptimizeCooldownUsage()
{
    // Advanced cooldown optimization
    bool inBurnPhase = _inBurnPhase.load();
    float manaPct = GetManaPercent();

    // Arcane Power optimization
    if (CanUseAbility(ARCANE_POWER))
    {
        if (inBurnPhase && manaPct > 0.6f)
        {
            CastArcanePower();
        }
        else if (!inBurnPhase && ShouldHoldCooldownsForBurn())
        {
            // Hold for next burn phase
            return;
        }
    }

    // Presence of Mind optimization
    if (CanUseAbility(PRESENCE_OF_MIND))
    {
        if (inBurnPhase || (GetArcaneCharges() == 4 && manaPct > 0.4f))
        {
            CastPresenceOfMind();
        }
    }

    // Mirror Image for survivability
    uint32 mirrorImageSpell = 55342;
    if (_bot->GetHealthPct() < 30.0f && CanUseAbility(mirrorImageSpell))
    {
        _bot->CastSpell(_bot, mirrorImageSpell, false);
        TC_LOG_DEBUG("playerbot.arcane", "Using Mirror Image for survivability");
    }
}

bool ArcaneSpecialization::ShouldHoldCooldownsForBurn()
{
    // Determine if we should save cooldowns for the next burn phase
    float manaPct = GetManaPercent();
    uint32 conserveDuration = getMSTime() - _conservePhaseStartTime;

    // Don't hold if mana is very low and we need help
    if (manaPct < 0.3f)
        return false;

    // Don't hold if we've been conserving for too long
    if (conserveDuration > 45000) // 45 seconds
        return false;

    // Hold if we're close to burn phase threshold
    return manaPct > 0.7f;
}

void ArcaneSpecialization::HandleCooldownSynergy()
{
    // Handle synergy between different cooldowns
    bool hasArcanePower = HasArcanePower();
    bool hasPresenceOfMind = HasPresenceOfMind();
    bool hasTimeWarp = _bot->HasAura(80353); // Time Warp

    // Optimal synergy: Arcane Power + Presence of Mind + Time Warp
    if (hasArcanePower && hasPresenceOfMind && hasTimeWarp)
    {
        // Maximum burn phase
        if (!_inBurnPhase.load())
        {
            EnterBurnPhase();
        }

        // Use highest damage spells
        if (GetArcaneCharges() < 4)
        {
            CastArcaneBlast();
        }
        else
        {
            if (HasClearcastingProc())
                CastArcaneMissiles();
            else
                CastArcaneBlast(); // Maintain 4 charges
        }
    }
}

float ArcaneSpecialization::CalculateManaEfficiency()
{
    // Calculate current mana efficiency
    uint32 totalManaSpent = _metrics.totalManaSpent.load();
    uint32 totalDamage = _metrics.totalDamage.load();

    if (totalManaSpent == 0)
        return 0.0f;

    float efficiency = static_cast<float>(totalDamage) / totalManaSpent;
    return efficiency;
}

void ArcaneSpecialization::UpdatePerformanceMetrics()
{
    // Update all performance metrics
    uint32 currentTime = getMSTime();
    auto timeSinceLastUpdate = std::chrono::steady_clock::now() - _metrics.lastUpdate;

    if (std::chrono::duration_cast<std::chrono::seconds>(timeSinceLastUpdate).count() >= 1)
    {
        // Calculate efficiency rates
        uint32 totalBlasts = _metrics.totalArcaneBlasts.load();
        uint32 fourStackBlasts = _metrics.fourStackBlasts.load();

        if (totalBlasts > 0)
        {
            float fourStackRate = static_cast<float>(fourStackBlasts) / totalBlasts;
            TC_LOG_DEBUG("playerbot.arcane", "Four-stack Arcane Blast rate: {:.1f}%", fourStackRate * 100.0f);
        }

        // Update average charges
        float avgCharges = _metrics.averageCharges.load();
        if (avgCharges < 2.0f)
        {
            TC_LOG_DEBUG("playerbot.arcane", "Low average charges detected: {:.1f}", avgCharges);
        }

        // Update burn phase efficiency
        if (_inBurnPhase.load())
        {
            float burnEfficiency = _metrics.burnPhaseEfficiency.load();
            _metrics.burnPhaseEfficiency = burnEfficiency + 0.1f;
        }

        _metrics.lastUpdate = std::chrono::steady_clock::now();
    }
}

// Additional helper methods

bool ArcaneSpecialization::IsOptimalBurnTime()
{
    // Determine if current conditions are optimal for burn phase
    float manaPct = GetManaPercent();
    bool hasCooldowns = CanUseAbility(ARCANE_POWER) || CanUseAbility(PRESENCE_OF_MIND);
    uint32 conserveDuration = getMSTime() - _conservePhaseStartTime;

    return manaPct >= BURN_ENTRY_THRESHOLD &&
           conserveDuration >= MIN_CONSERVE_DURATION &&
           hasCooldowns;
}

void ArcaneSpecialization::LogPhaseTransition(const std::string& fromPhase, const std::string& toPhase)
{
    float manaPct = GetManaPercent();
    uint32 charges = GetArcaneCharges();

    TC_LOG_DEBUG("playerbot.arcane", "Phase transition: {} -> {} (Mana: {:.1f}%, Charges: {})",
                 fromPhase, toPhase, manaPct * 100.0f, charges);
}

void ArcaneSpecialization::EnterBurnPhase()
{
    if (_inBurnPhase.load())
        return;

    LogPhaseTransition("Conserve", "Burn");

    _inBurnPhase.store(true);
    _inConservePhase.store(false);
    _burnPhaseStartTime = getMSTime();
    _phaseStartTime = std::chrono::steady_clock::now();

    // Calculate optimal burn duration
    CalculateOptimalBurnDuration();

    // Reset burn phase metrics
    _metrics.burnPhaseEfficiency.store(0.0f);
}

void ArcaneSpecialization::EnterConservePhase()
{
    if (_inConservePhase.load())
        return;

    LogPhaseTransition("Burn", "Conserve");

    _inBurnPhase.store(false);
    _inConservePhase.store(true);
    _conservePhaseStartTime = getMSTime();
    _phaseStartTime = std::chrono::steady_clock::now();

    // Reset charge stacks for conserve phase
    _arcaneBlastStacks.store(0);
}

} // namespace Playerbot