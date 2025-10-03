/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AssassinationSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "SpellAuras.h"
#include "Log.h"
#include "Player.h"
#include "Map.h"

namespace Playerbot
{

AssassinationSpecialization::AssassinationSpecialization(Player* bot)
    : RogueSpecialization(bot), _assassinationPhase(AssassinationRotationPhase::STEALTH_OPENER),
      _lastMutilateTime(0), _lastEnvenomTime(0), _lastRuptureTime(0), _lastGarroteTime(0),
      _lastPoisonApplicationTime(0), _burstPhaseStartTime(0), _lastStealthTime(0), _lastVanishTime(0),
      _preferredOpener(GARROTE), _preferredComboBuilder(MUTILATE), _preferredFinisher(ENVENOM)
{
    // Initialize DoT tracking
    _dots[RUPTURE] = DotInfo(RUPTURE, 22000);  // 22 seconds
    _dots[GARROTE] = DotInfo(GARROTE, 18000);  // 18 seconds
    // Modern WoW 11.2: Deadly Poison is now applied as character buff, not weapon coating
    _dots[DEADLY_POISON_MODERN] = DotInfo(DEADLY_POISON_MODERN, 12000); // 12 seconds

    // Initialize stealth openers in priority order
    _stealthOpeners = {GARROTE, CHEAP_SHOT, AMBUSH};

    // Initialize combo builders in priority order
    _comboBuilders = {MUTILATE, BACKSTAB, SINISTER_STRIKE, HEMORRHAGE};

    // Initialize finishers in priority order
    _finishers = {ENVENOM, RUPTURE, EVISCERATE, SLICE_AND_DICE};

    TC_LOG_DEBUG("playerbot", "AssassinationSpecialization: Initialized for bot {}", _bot->GetName());
}

void AssassinationSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Update all management systems
    UpdateResourceStates();
    UpdateTargetInfo(target);
    UpdateDotManagement();
    UpdatePoisonStacks();
    UpdateStealthManagement();
    UpdateComboPointManagement();
    UpdateEnergyManagement();
    UpdateCombatPhase();
    UpdateStealthAdvantage();
    UpdateBurstPhase();
    UpdateExecutePhase();
    UpdateAoEPhase();
    UpdateEmergencyPhase();
    UpdateCombatMetrics();

    // Execute rotation based on current phase
    switch (_assassinationPhase)
    {
        case AssassinationRotationPhase::STEALTH_OPENER:
            ExecuteStealthRotation(target);
            break;
        case AssassinationRotationPhase::GARROTE_APPLICATION:
            ExecuteGarrotePhase(target);
            break;
        case AssassinationRotationPhase::POISON_BUILDING:
            ExecutePoisonBuildingPhase(target);
            break;
        case AssassinationRotationPhase::MUTILATE_SPAM:
            ExecuteMutilatePhase(target);
            break;
        case AssassinationRotationPhase::COMBO_SPENDING:
            ExecuteComboSpendingPhase(target);
            break;
        case AssassinationRotationPhase::DOT_REFRESH:
            ExecuteDotRefreshPhase(target);
            break;
        case AssassinationRotationPhase::BURST_PHASE:
            ExecuteBurstPhase(target);
            break;
        case AssassinationRotationPhase::EXECUTE_PHASE:
            ExecuteExecutePhase(target);
            break;
        case AssassinationRotationPhase::AOE_PHASE:
            ExecuteAoEPhase(target);
            break;
        case AssassinationRotationPhase::EMERGENCY:
            ExecuteEmergencyPhase(target);
            break;
    }

    AnalyzeRotationEfficiency();
}

void AssassinationSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Apply poisons if needed
    if (ShouldApplyPoisons())
        ApplyPoisons();

    // Maintain Slice and Dice if we have it
    if (ShouldUseSliceAndDice() && GetComboPoints() >= 1)
    {
        if (CastSpell(SLICE_AND_DICE))
        {
            LogAssassinationDecision("Cast Slice and Dice", "Maintaining attack speed buff");
            _totalCombosSpent += GetComboPoints();
        }
    }

    // Use Find Weakness if available
    if (HasSpell(FIND_WEAKNESS) && !HasAura(FIND_WEAKNESS))
    {
        CastSpell(FIND_WEAKNESS);
    }
}

void AssassinationSpecialization::UpdateCooldowns(uint32 diff)
{
    RogueSpecialization::UpdateCooldownTracking(diff);

    // Update DoT timers
    UpdateDotTicks();

    // Update poison application timer
    if (_lastPoisonApplicationTime > 0)
        _lastPoisonApplicationTime += diff;
}

bool AssassinationSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughEnergyFor(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    // Stealth-only abilities
    if ((spellId == GARROTE || spellId == CHEAP_SHOT || spellId == AMBUSH) && !IsStealthed())
        return false;

    // Behind-target requirements
    if ((spellId == BACKSTAB || spellId == AMBUSH) && _currentTarget && !IsBehindTarget(_currentTarget))
        return false;

    // Combo point requirements
    if (spellId == ENVENOM || spellId == RUPTURE || spellId == EVISCERATE || spellId == SLICE_AND_DICE)
    {
        if (GetComboPoints() == 0)
            return false;
    }

    return true;
}

void AssassinationSpecialization::OnCombatStart(::Unit* target)
{
    if (!target)
        return;

    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = AssassinationMetrics();

    // Start with stealth opener if possible
    if (IsStealthed())
    {
        _assassinationPhase = AssassinationRotationPhase::STEALTH_OPENER;
        LogAssassinationDecision("Combat Start", "Beginning with stealth opener");
    }
    else
    {
        _assassinationPhase = AssassinationRotationPhase::POISON_BUILDING;
        LogAssassinationDecision("Combat Start", "Beginning without stealth");
    }

    // Apply poisons if not already applied
    if (ShouldApplyPoisons())
        ApplyPoisons();
}

void AssassinationSpecialization::OnCombatEnd()
{
    // Log combat statistics
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "AssassinationSpecialization [{}]: Combat ended. Duration: {}ms, Damage: {}, Energy spent: {}",
                _bot->GetName(), combatDuration, _totalDamageDealt, _totalEnergySpent);

    // Reset phase to stealth opener for next combat
    _assassinationPhase = AssassinationRotationPhase::STEALTH_OPENER;
    _currentTarget = nullptr;
}

bool AssassinationSpecialization::HasEnoughResource(uint32 spellId)
{
    return HasEnoughEnergyFor(spellId);
}

void AssassinationSpecialization::ConsumeResource(uint32 spellId)
{
    uint32 energyCost = GetEnergyCost(spellId);
    if (energyCost > 0)
    {
        _bot->ModifyPower(POWER_ENERGY, -static_cast<int32>(energyCost));
        _totalEnergySpent += energyCost;
    }
}

Position AssassinationSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    // Assassination rogues prefer to be behind the target for Backstab and Ambush
    float angle = target->GetOrientation() + M_PI; // Behind target
    float distance = 2.0f; // Close melee range

    float x = target->GetPositionX() + cos(angle) * distance;
    float y = target->GetPositionY() + sin(angle) * distance;
    float z = target->GetPositionZ();

    return Position(x, y, z, angle);
}

float AssassinationSpecialization::GetOptimalRange(::Unit* target)
{
    // Assassination is pure melee
    return 2.0f;
}

void AssassinationSpecialization::UpdateStealthManagement()
{
    if (!_bot)
        return;

    // Check if we should enter stealth
    if (ShouldEnterStealth() && !IsStealthed())
    {
        if (IsSpellReady(STEALTH) && !_bot->IsInCombat())
        {
            if (CastSpell(STEALTH))
            {
                _lastStealthTime = getMSTime();
                LogAssassinationDecision("Entered Stealth", "Preparing for opener");
            }
        }
        else if (IsSpellReady(VANISH) && _bot->IsInCombat())
        {
            if (CastSpell(VANISH))
            {
                _lastVanishTime = getMSTime();
                _metrics.vanishEscapes++;
                LogAssassinationDecision("Used Vanish", "Re-stealthing for advantage");
            }
        }
    }

    // Update stealth duration tracking
    if (IsStealthed())
    {
        _metrics.totalStealthTime += 1000; // Assume 1 second update intervals
    }
}

bool AssassinationSpecialization::ShouldEnterStealth()
{
    if (!_bot)
        return false;

    // Enter stealth before combat
    if (!_bot->IsInCombat() && !IsStealthed())
        return true;

    // Use Vanish in emergencies
    if (_bot->IsInCombat() && _bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Use Vanish for re-opener in long fights
    if (_bot->IsInCombat() && IsSpellReady(VANISH))
    {
        uint32 combatTime = getMSTime() - _combatStartTime;
        if (combatTime > 60000) // After 1 minute of combat
            return true;
    }

    return false;
}

bool AssassinationSpecialization::CanBreakStealth()
{
    // Always allow breaking stealth for openers
    return true;
}

void AssassinationSpecialization::ExecuteStealthOpener(::Unit* target)
{
    if (!target || !IsStealthed())
        return;

    if (ShouldUseGarroteOpener(target))
        ExecuteGarroteOpener(target);
    else if (ShouldUseCheapShotOpener(target))
        ExecuteCheapShotOpener(target);
    else if (ShouldUseAmbushOpener(target))
        ExecuteAmbushOpener(target);
}

void AssassinationSpecialization::UpdateComboPointManagement()
{
    _comboPoints.current = GetComboPoints();

    // Determine if we should build or spend
    _comboPoints.shouldSpend = ShouldSpendComboPoints();

    // Update combo point metrics
    if (_comboPoints.current > 0)
    {
        float currentAverage = _metrics.averageComboPointsOnSpend;
        _metrics.averageComboPointsOnSpend = (currentAverage + _comboPoints.current) / 2.0f;
    }
}

bool AssassinationSpecialization::ShouldBuildComboPoints()
{
    return GetComboPoints() < 5 && !ShouldSpendComboPoints();
}

bool AssassinationSpecialization::ShouldSpendComboPoints()
{
    uint8 comboPoints = GetComboPoints();

    // Always spend at 5 combo points
    if (comboPoints >= 5)
        return true;

    // Spend at 4+ if high energy
    if (comboPoints >= 4 && _energy.state >= EnergyState::HIGH)
        return true;

    // Spend at 3+ for emergency finishers
    if (comboPoints >= 3 && _bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Spend for DoT refresh if needed
    if (comboPoints >= MIN_COMBO_FOR_RUPTURE && ShouldRefreshRupture(_currentTarget))
        return true;

    return false;
}

void AssassinationSpecialization::ExecuteComboBuilder(::Unit* target)
{
    if (!target)
        return;

    // Mutilate is preferred if we have it and dual wield
    if (HasSpell(MUTILATE) && HasWeaponInMainHand() && HasWeaponInOffHand())
    {
        if (CastSpell(MUTILATE, target))
        {
            _metrics.mutilateCasts++;
            _totalCombosBuilt += 2; // Mutilate generates 2 combo points
            LogAssassinationDecision("Cast Mutilate", "Primary combo builder");
            return;
        }
    }

    // Backstab if behind target
    if (HasSpell(BACKSTAB) && IsBehindTarget(target))
    {
        if (CastSpell(BACKSTAB, target))
        {
            _metrics.backStabCasts++;
            _totalCombosBuilt += 1;
            LogAssassinationDecision("Cast Backstab", "Behind target combo builder");
            return;
        }
    }

    // Hemorrhage if available
    if (HasSpell(HEMORRHAGE))
    {
        if (CastSpell(HEMORRHAGE, target))
        {
            _totalCombosBuilt += 1;
            LogAssassinationDecision("Cast Hemorrhage", "Alternative combo builder");
            return;
        }
    }

    // Fallback to Sinister Strike
    if (HasSpell(SINISTER_STRIKE))
    {
        if (CastSpell(SINISTER_STRIKE, target))
        {
            _totalCombosBuilt += 1;
            LogAssassinationDecision("Cast Sinister Strike", "Fallback combo builder");
        }
    }
}

void AssassinationSpecialization::ExecuteComboSpender(::Unit* target)
{
    if (!target || GetComboPoints() == 0)
        return;

    uint8 comboPoints = GetComboPoints();

    // Envenom if we have poison effects or high combo points
    if (ShouldUseEnvenom(target))
    {
        if (CastSpell(ENVENOM, target))
        {
            _metrics.envenomCasts++;
            _lastEnvenomTime = getMSTime();
            LogAssassinationDecision("Cast Envenom", "Poison-enhanced finisher");
            return;
        }
    }

    // Rupture for DoT damage
    if (ShouldUseRupture(target))
    {
        if (CastSpell(RUPTURE, target))
        {
            _metrics.ruptureApplications++;
            _lastRuptureTime = getMSTime();
            LogAssassinationDecision("Cast Rupture", "DoT application/refresh");
            return;
        }
    }

    // Slice and Dice for attack speed
    if (ShouldUseSliceAndDice())
    {
        if (CastSpell(SLICE_AND_DICE))
        {
            LogAssassinationDecision("Cast Slice and Dice", "Attack speed buff");
            return;
        }
    }

    // Eviscerate as fallback
    if (ShouldUseEviscerate(target))
    {
        if (CastSpell(EVISCERATE, target))
        {
            LogAssassinationDecision("Cast Eviscerate", "Direct damage finisher");
        }
    }
}

void AssassinationSpecialization::UpdatePoisonManagement()
{
    // WoW 11.2 Modern Poison Management
    // Poisons are now character buffs lasting 1 hour, not weapon charges

    // Only check/apply poisons if needed (buffs missing or expiring)
    if (ShouldApplyPoisons())
    {
        ApplyPoisons();
        _lastPoisonApplicationTime = getMSTime();
    }

    // Update poison application tracking for combat metrics
    UpdatePoisonStacks();
}

void AssassinationSpecialization::ApplyPoisons()
{
    // WoW 11.2 Modern Poison System: Poisons are now character buffs, not weapon coatings
    // Each poison type persists for 1 hour and applies automatically in combat

    PoisonType lethalPoison = GetOptimalLethalPoison();
    PoisonType nonLethalPoison = GetOptimalNonLethalPoison();

    // Apply lethal poison (character buff)
    if (lethalPoison == PoisonType::DEADLY)
        ApplyDeadlyPoison();
    else if (lethalPoison == PoisonType::AMPLIFYING)
        ApplyAmplifyingPoison();
    else if (lethalPoison == PoisonType::INSTANT)
        ApplyInstantPoison();
    else if (lethalPoison == PoisonType::WOUND)
        ApplyWoundPoison();

    // Apply non-lethal poison (character buff)
    if (nonLethalPoison == PoisonType::CRIPPLING)
        ApplyCripplingPoison();
    else if (nonLethalPoison == PoisonType::NUMBING)
        ApplyNumbingPoison();
    else if (nonLethalPoison == PoisonType::ATROPHIC)
        ApplyAtrophicPoison();

    _metrics.poisonApplications++;

    // SPAM FIX: Only log poison application once every 30 seconds to prevent log spam
    static uint32 lastPoisonLogTime = 0;
    uint32 currentTime = getMSTime();
    if (currentTime - lastPoisonLogTime > 30000) // 30 seconds between logs
    {
        LogAssassinationDecision("Applied Poisons", "Maintaining character poison buffs (WoW 11.2)");
        lastPoisonLogTime = currentTime;
    }
}

// WoW 11.2 Modern Poison Selection - Lethal and Non-Lethal Categories
PoisonType AssassinationSpecialization::GetOptimalLethalPoison()
{
    // WoW 11.2 Guide: "Your Lethal Poison should always be Deadly Poison and Amplifying Poison"
    // Priority: Deadly Poison for sustained damage
    if (HasSpell(DEADLY_POISON_MODERN))
        return PoisonType::DEADLY;

    // Amplifying Poison for burst windows with Envenom
    if (HasSpell(AMPLIFYING_POISON))
        return PoisonType::AMPLIFYING;

    // Wound Poison for PvP (healing reduction)
    if (HasSpell(WOUND_POISON_MODERN))
        return PoisonType::WOUND;

    // Fallback to Instant Poison
    if (HasSpell(INSTANT_POISON_MODERN))
        return PoisonType::INSTANT;

    return PoisonType::NONE;
}

PoisonType AssassinationSpecialization::GetOptimalNonLethalPoison()
{
    // WoW 11.2 Guide: Three choices - Crippling, Atrophic, Numbing

    // Atrophic Poison for raiding (damage reduction)
    if (HasSpell(ATROPHIC_POISON))
        return PoisonType::ATROPHIC;

    // Numbing Poison for M+ (attack/cast speed reduction)
    if (HasSpell(NUMBING_POISON))
        return PoisonType::NUMBING;

    // Crippling Poison for kiting and mobility control
    if (HasSpell(CRIPPLING_POISON_MODERN))
        return PoisonType::CRIPPLING;

    return PoisonType::NONE;
}

// Legacy methods for compatibility
PoisonType AssassinationSpecialization::GetOptimalMainHandPoison()
{
    return GetOptimalLethalPoison();
}

PoisonType AssassinationSpecialization::GetOptimalOffHandPoison()
{
    return GetOptimalNonLethalPoison();
}

void AssassinationSpecialization::UpdateDebuffManagement()
{
    if (!_currentTarget)
        return;

    // Update target debuff info
    UpdateTargetInfo(_currentTarget);

    // Check if we need to refresh debuffs
    if (ShouldRefreshRupture(_currentTarget))
        _assassinationPhase = AssassinationRotationPhase::DOT_REFRESH;

    if (ShouldRefreshGarrote(_currentTarget))
        _assassinationPhase = AssassinationRotationPhase::DOT_REFRESH;
}

bool AssassinationSpecialization::ShouldRefreshDebuff(uint32 spellId)
{
    if (!_currentTarget)
        return false;

    uint32 remainingTime = GetAuraTimeRemaining(spellId, _currentTarget);
    uint32 duration = 0;

    if (spellId == RUPTURE)
        duration = 22000;
    else if (spellId == GARROTE)
        duration = 18000;
    else
        return false;

    // Refresh when less than 30% duration remains
    return remainingTime < (duration * DOT_REFRESH_THRESHOLD);
}

void AssassinationSpecialization::ApplyDebuffs(::Unit* target)
{
    if (!target)
        return;

    // Apply Garrote if stealthed
    if (IsStealthed() && !HasAura(GARROTE, target))
    {
        ExecuteGarroteOpener(target);
        return;
    }

    // Apply/refresh Rupture
    if (ShouldRefreshRupture(target) && GetComboPoints() >= MIN_COMBO_FOR_RUPTURE)
    {
        RefreshRupture(target);
    }
}

void AssassinationSpecialization::UpdateEnergyManagement()
{
    RogueSpecialization::UpdateResourceStates();

    // Assassination needs to manage energy efficiently for Mutilate spam
    if (_energy.state == EnergyState::CRITICAL)
    {
        // Wait for energy if we're too low
        if (ShouldWaitForEnergy())
        {
            LogAssassinationDecision("Waiting for Energy", "Energy too low for abilities");
            return;
        }
    }
}

bool AssassinationSpecialization::HasEnoughEnergyFor(uint32 spellId)
{
    return RogueSpecialization::HasEnoughEnergyFor(spellId);
}

uint32 AssassinationSpecialization::GetEnergyCost(uint32 spellId)
{
    return RogueSpecialization::GetEnergyCost(spellId);
}

bool AssassinationSpecialization::ShouldWaitForEnergy()
{
    // Wait if we have critical energy and no immediate threats
    if (_energy.state == EnergyState::CRITICAL)
    {
        if (_bot && _bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
            return true;
    }

    return false;
}

void AssassinationSpecialization::UpdateCooldownTracking(uint32 diff)
{
    RogueSpecialization::UpdateCooldownTracking(diff);
}

bool AssassinationSpecialization::IsSpellReady(uint32 spellId)
{
    return RogueSpecialization::IsSpellReady(spellId);
}

void AssassinationSpecialization::StartCooldown(uint32 spellId)
{
    RogueSpecialization::StartCooldown(spellId);
}

uint32 AssassinationSpecialization::GetCooldownRemaining(uint32 spellId)
{
    return RogueSpecialization::GetCooldownRemaining(spellId);
}

void AssassinationSpecialization::UpdateCombatPhase()
{
    if (!_bot || !_currentTarget)
        return;

    // Emergency phase check
    if (_bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
    {
        _assassinationPhase = AssassinationRotationPhase::EMERGENCY;
        return;
    }

    // Execute phase
    if (_currentTarget->GetHealthPct() < (EXECUTE_HEALTH_THRESHOLD * 100))
    {
        _assassinationPhase = AssassinationRotationPhase::EXECUTE_PHASE;
        return;
    }

    // Burst phase
    if (ShouldExecuteBurstRotation())
    {
        _assassinationPhase = AssassinationRotationPhase::BURST_PHASE;
        return;
    }

    // DoT refresh phase
    if (ShouldRefreshRupture(_currentTarget) || ShouldRefreshGarrote(_currentTarget))
    {
        _assassinationPhase = AssassinationRotationPhase::DOT_REFRESH;
        return;
    }

    // Combo spending phase
    if (ShouldSpendComboPoints())
    {
        _assassinationPhase = AssassinationRotationPhase::COMBO_SPENDING;
        return;
    }

    // Default to mutilate spam for combo building
    _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

CombatPhase AssassinationSpecialization::GetCurrentPhase()
{
    switch (_assassinationPhase)
    {
        case AssassinationRotationPhase::STEALTH_OPENER:
            return CombatPhase::STEALTH_OPENER;
        case AssassinationRotationPhase::COMBO_SPENDING:
            return CombatPhase::COMBO_SPENDING;
        case AssassinationRotationPhase::BURST_PHASE:
            return CombatPhase::BURST_PHASE;
        case AssassinationRotationPhase::EXECUTE_PHASE:
            return CombatPhase::EXECUTE_PHASE;
        case AssassinationRotationPhase::AOE_PHASE:
            return CombatPhase::AOE_PHASE;
        case AssassinationRotationPhase::EMERGENCY:
            return CombatPhase::EMERGENCY;
        default:
            return CombatPhase::COMBO_BUILDING;
    }
}

bool AssassinationSpecialization::ShouldExecuteBurstRotation()
{
    // Use burst when cooldowns are available
    if (IsSpellReady(COLD_BLOOD) || IsSpellReady(VENDETTA))
        return true;

    // Use burst at specific health thresholds
    if (_currentTarget && _currentTarget->GetHealthPct() > 80.0f)
        return true;

    return false;
}

// Phase execution methods
void AssassinationSpecialization::ExecuteStealthRotation(::Unit* target)
{
    if (!IsStealthed())
    {
        _assassinationPhase = AssassinationRotationPhase::POISON_BUILDING;
        return;
    }

    ExecuteStealthOpener(target);

    // Transition to next phase after opener
    if (!IsStealthed())
        _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecuteGarrotePhase(::Unit* target)
{
    if (IsStealthed() && HasSpell(GARROTE))
    {
        ExecuteGarroteOpener(target);
    }

    _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecutePoisonBuildingPhase(::Unit* target)
{
    // Apply poisons if needed
    if (ShouldApplyPoisons())
        ApplyPoisons();

    // Transition to combo building
    _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecuteMutilatePhase(::Unit* target)
{
    if (ShouldBuildComboPoints())
        ExecuteComboBuilder(target);
    else
        _assassinationPhase = AssassinationRotationPhase::COMBO_SPENDING;
}

void AssassinationSpecialization::ExecuteComboSpendingPhase(::Unit* target)
{
    ExecuteComboSpender(target);

    // After spending, go back to building
    _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecuteDotRefreshPhase(::Unit* target)
{
    bool refreshed = false;

    if (ShouldRefreshRupture(target) && GetComboPoints() >= MIN_COMBO_FOR_RUPTURE)
    {
        RefreshRupture(target);
        refreshed = true;
    }

    if (ShouldRefreshGarrote(target) && IsStealthed())
    {
        RefreshGarrote(target);
        refreshed = true;
    }

    if (refreshed)
        _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecuteBurstPhase(::Unit* target)
{
    InitiateBurstPhase();

    if (ShouldUseColdBlood())
        ExecuteColdBloodBurst(target);

    if (ShouldUseVendetta())
        ExecuteVendettaBurst(target);

    // Continue with normal rotation
    _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecuteExecutePhase(::Unit* target)
{
    // Prioritize high damage finishers in execute phase
    if (GetComboPoints() >= 3)
    {
        if (ShouldUseEnvenom(target))
            ExecuteComboSpender(target);
        else if (ShouldUseEviscerate(target))
            ExecuteComboSpender(target);
    }
    else
    {
        ExecuteComboBuilder(target);
    }
}

void AssassinationSpecialization::ExecuteAoEPhase(::Unit* target)
{
    // Use Fan of Knives for AoE
    if (HasSpell(FAN_OF_KNIVES))
    {
        if (CastSpell(FAN_OF_KNIVES))
        {
            LogAssassinationDecision("Cast Fan of Knives", "AoE combo building");
            return;
        }
    }

    // Fallback to single target
    _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

void AssassinationSpecialization::ExecuteEmergencyPhase(::Unit* target)
{
    HandleEmergencySituations();

    // Try to recover
    if (_bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
        _assassinationPhase = AssassinationRotationPhase::MUTILATE_SPAM;
}

// Helper method implementations
void AssassinationSpecialization::ExecuteGarroteOpener(::Unit* target)
{
    if (CastSpell(GARROTE, target))
    {
        _metrics.garroteApplications++;
        _lastGarroteTime = getMSTime();
        LogAssassinationDecision("Garrote Opener", "Stealth opener with DoT");
    }
}

void AssassinationSpecialization::ExecuteCheapShotOpener(::Unit* target)
{
    if (CastSpell(CHEAP_SHOT, target))
    {
        LogAssassinationDecision("Cheap Shot Opener", "Stealth opener with stun");
    }
}

void AssassinationSpecialization::ExecuteAmbushOpener(::Unit* target)
{
    if (IsBehindTarget(target) && CastSpell(AMBUSH, target))
    {
        LogAssassinationDecision("Ambush Opener", "High damage stealth opener");
    }
}

bool AssassinationSpecialization::ShouldUseGarroteOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(GARROTE) && !HasAura(GARROTE, target);
}

bool AssassinationSpecialization::ShouldUseCheapShotOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(CHEAP_SHOT) && !target->HasUnitState(UNIT_STATE_STUNNED);
}

bool AssassinationSpecialization::ShouldUseAmbushOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(AMBUSH) && IsBehindTarget(target);
}

bool AssassinationSpecialization::ShouldUseEnvenom(::Unit* target)
{
    if (!HasSpell(ENVENOM) || GetComboPoints() < MIN_COMBO_FOR_ENVENOM)
        return false;

    // WoW 11.2: Prioritize Envenom when Amplifying Poison has high stacks
    uint32 amplifyingStacks = GetPoisonStacks(target, PoisonType::AMPLIFYING);
    if (amplifyingStacks >= 10) // Envenom can consume 10 stacks for 35% increased damage
        return true;

    // Use if target has poison effects (any lethal poison)
    return GetPoisonStacks(target, PoisonType::DEADLY) > 0 ||
           GetPoisonStacks(target, PoisonType::INSTANT) > 0 ||
           GetPoisonStacks(target, PoisonType::WOUND) > 0 ||
           amplifyingStacks > 0;
}

bool AssassinationSpecialization::ShouldUseEviscerate(::Unit* target)
{
    return HasSpell(EVISCERATE) && GetComboPoints() >= 3;
}

bool AssassinationSpecialization::ShouldUseRupture(::Unit* target)
{
    if (!HasSpell(RUPTURE) || GetComboPoints() < MIN_COMBO_FOR_RUPTURE)
        return false;

    return ShouldRefreshRupture(target);
}

bool AssassinationSpecialization::ShouldUseSliceAndDice()
{
    return HasSpell(SLICE_AND_DICE) && !HasAura(SLICE_AND_DICE) && GetComboPoints() >= 1;
}

void AssassinationSpecialization::RefreshRupture(::Unit* target)
{
    if (CastSpell(RUPTURE, target))
    {
        _metrics.ruptureApplications++;
        _lastRuptureTime = getMSTime();
        LogAssassinationDecision("Refreshed Rupture", "DoT maintenance");
    }
}

void AssassinationSpecialization::RefreshGarrote(::Unit* target)
{
    if (IsStealthed() && CastSpell(GARROTE, target))
    {
        _metrics.garroteApplications++;
        _lastGarroteTime = getMSTime();
        LogAssassinationDecision("Refreshed Garrote", "Stealth DoT refresh");
    }
}

bool AssassinationSpecialization::ShouldRefreshRupture(::Unit* target)
{
    return ShouldRefreshDebuff(RUPTURE);
}

bool AssassinationSpecialization::ShouldRefreshGarrote(::Unit* target)
{
    return ShouldRefreshDebuff(GARROTE) && IsStealthed();
}

// WoW 11.2 Modern Poison Application Methods
void AssassinationSpecialization::ApplyDeadlyPoison()
{
    // Modern WoW 11.2: Apply as character buff
    if (HasSpell(DEADLY_POISON_MODERN))
        CastSpell(DEADLY_POISON_MODERN);
}

void AssassinationSpecialization::ApplyAmplifyingPoison()
{
    // WoW 11.2: New poison that enhances Envenom damage
    if (HasSpell(AMPLIFYING_POISON))
        CastSpell(AMPLIFYING_POISON);
}

void AssassinationSpecialization::ApplyInstantPoison()
{
    // Modern WoW 11.2: Apply as character buff
    if (HasSpell(INSTANT_POISON_MODERN))
        CastSpell(INSTANT_POISON_MODERN);
}

void AssassinationSpecialization::ApplyWoundPoison()
{
    // Modern WoW 11.2: Apply as character buff
    if (HasSpell(WOUND_POISON_MODERN))
        CastSpell(WOUND_POISON_MODERN);
}

void AssassinationSpecialization::ApplyCripplingPoison()
{
    // Modern WoW 11.2: Apply as character buff
    if (HasSpell(CRIPPLING_POISON_MODERN))
        CastSpell(CRIPPLING_POISON_MODERN);
}

void AssassinationSpecialization::ApplyNumbingPoison()
{
    // WoW 11.2: Replaces old Mind-numbing Poison
    if (HasSpell(NUMBING_POISON))
        CastSpell(NUMBING_POISON);
}

void AssassinationSpecialization::ApplyAtrophicPoison()
{
    // WoW 11.2: New poison that reduces enemy damage
    if (HasSpell(ATROPHIC_POISON))
        CastSpell(ATROPHIC_POISON);
}

bool AssassinationSpecialization::ShouldApplyPoisons()
{
    // WoW 11.2: Poisons are character buffs lasting 1 hour
    // Only reapply if the buff is missing or has less than 5 minutes remaining

    if (!_bot)
        return false;

    // Check if lethal poison buff is missing or expiring soon
    PoisonType lethalPoison = GetOptimalLethalPoison();
    bool needsLethalPoison = false;

    switch (lethalPoison)
    {
        case PoisonType::DEADLY:
            needsLethalPoison = !_bot->HasAura(DEADLY_POISON_MODERN) ||
                              GetAuraTimeRemaining(DEADLY_POISON_MODERN, _bot) < 300000; // 5 minutes
            break;
        case PoisonType::AMPLIFYING:
            needsLethalPoison = !_bot->HasAura(AMPLIFYING_POISON) ||
                              GetAuraTimeRemaining(AMPLIFYING_POISON, _bot) < 300000;
            break;
        case PoisonType::INSTANT:
            needsLethalPoison = !_bot->HasAura(INSTANT_POISON_MODERN) ||
                              GetAuraTimeRemaining(INSTANT_POISON_MODERN, _bot) < 300000;
            break;
        case PoisonType::WOUND:
            needsLethalPoison = !_bot->HasAura(WOUND_POISON_MODERN) ||
                              GetAuraTimeRemaining(WOUND_POISON_MODERN, _bot) < 300000;
            break;
        default:
            needsLethalPoison = true;
            break;
    }

    // Check if non-lethal poison buff is missing or expiring soon
    PoisonType nonLethalPoison = GetOptimalNonLethalPoison();
    bool needsNonLethalPoison = false;

    switch (nonLethalPoison)
    {
        case PoisonType::CRIPPLING:
            needsNonLethalPoison = !_bot->HasAura(CRIPPLING_POISON_MODERN) ||
                                 GetAuraTimeRemaining(CRIPPLING_POISON_MODERN, _bot) < 300000;
            break;
        case PoisonType::NUMBING:
            needsNonLethalPoison = !_bot->HasAura(NUMBING_POISON) ||
                                 GetAuraTimeRemaining(NUMBING_POISON, _bot) < 300000;
            break;
        case PoisonType::ATROPHIC:
            needsNonLethalPoison = !_bot->HasAura(ATROPHIC_POISON) ||
                                 GetAuraTimeRemaining(ATROPHIC_POISON, _bot) < 300000;
            break;
        default:
            needsNonLethalPoison = true;
            break;
    }

    return needsLethalPoison || needsNonLethalPoison;
}

uint32 AssassinationSpecialization::GetPoisonStacks(::Unit* target, PoisonType type)
{
    if (!target)
        return 0;

    // WoW 11.2 Modern poison stack tracking
    switch (type)
    {
        case PoisonType::DEADLY:
            return HasAura(DEADLY_POISON_MODERN, target) ? 1 : 0; // Deadly Poison no longer stacks
        case PoisonType::AMPLIFYING:
            // Amplifying Poison stacks up to 20 for Envenom consumption
            if (HasAura(AMPLIFYING_POISON, target))
            {
                if (Aura* aura = target->GetAura(AMPLIFYING_POISON))
                    return aura->GetStackAmount();
            }
            return 0;
        case PoisonType::INSTANT:
            return HasAura(INSTANT_POISON_MODERN, target) ? 1 : 0; // Instant damage, no stacks
        case PoisonType::WOUND:
            // Wound Poison stacks up to 3 times for healing reduction
            if (HasAura(WOUND_POISON_MODERN, target))
            {
                if (Aura* aura = target->GetAura(WOUND_POISON_MODERN))
                    return aura->GetStackAmount();
            }
            return 0;
        case PoisonType::CRIPPLING:
            return HasAura(CRIPPLING_POISON_MODERN, target) ? 1 : 0; // Movement slow, no stacks
        case PoisonType::NUMBING:
            return HasAura(NUMBING_POISON, target) ? 1 : 0; // Attack/cast speed slow, no stacks
        case PoisonType::ATROPHIC:
            return HasAura(ATROPHIC_POISON, target) ? 1 : 0; // Damage reduction, highest instance only
        default:
            return 0;
    }
}

// Burst phase methods
void AssassinationSpecialization::InitiateBurstPhase()
{
    _burstPhaseStartTime = getMSTime();
    _metrics.burstPhaseCount++;
    LogAssassinationDecision("Initiated Burst Phase", "Cooldown window opened");
}

void AssassinationSpecialization::ExecuteColdBloodBurst(::Unit* target)
{
    if (CastSpell(COLD_BLOOD))
    {
        _metrics.coldBloodUsages++;
        LogAssassinationDecision("Activated Cold Blood", "Burst damage window");
    }
}

void AssassinationSpecialization::ExecuteVendettaBurst(::Unit* target)
{
    if (HasSpell(VENDETTA) && CastSpell(VENDETTA, target))
    {
        LogAssassinationDecision("Cast Vendetta", "Target vulnerability window");
    }
}

bool AssassinationSpecialization::ShouldUseColdBlood()
{
    return IsSpellReady(COLD_BLOOD) && GetComboPoints() >= 4;
}

bool AssassinationSpecialization::ShouldUseVendetta()
{
    return HasSpell(VENDETTA) && IsSpellReady(VENDETTA);
}

// Emergency methods
void AssassinationSpecialization::HandleEmergencySituations()
{
    if (ShouldVanishEscape())
        ExecuteVanishEscape();
    else if (ShouldUseEvasion())
        ExecuteEvasion();
    else if (ShouldUseCloakOfShadows())
        ExecuteCloakOfShadows();
}

void AssassinationSpecialization::ExecuteVanishEscape()
{
    if (CastSpell(VANISH))
    {
        _metrics.vanishEscapes++;
        LogAssassinationDecision("Emergency Vanish", "Escape from danger");
    }
}

void AssassinationSpecialization::ExecuteEvasion()
{
    if (CastSpell(EVASION))
    {
        LogAssassinationDecision("Activated Evasion", "Emergency defense");
    }
}

void AssassinationSpecialization::ExecuteCloakOfShadows()
{
    if (CastSpell(CLOAK_OF_SHADOWS))
    {
        LogAssassinationDecision("Cloak of Shadows", "Magic immunity");
    }
}

bool AssassinationSpecialization::ShouldVanishEscape()
{
    return _bot->GetHealthPct() < 20.0f && IsSpellReady(VANISH);
}

bool AssassinationSpecialization::ShouldUseEvasion()
{
    return _bot->GetHealthPct() < 40.0f && IsSpellReady(EVASION);
}

bool AssassinationSpecialization::ShouldUseCloakOfShadows()
{
    return _bot->GetHealthPct() < 50.0f && IsSpellReady(CLOAK_OF_SHADOWS);
}

// Management update methods
void AssassinationSpecialization::UpdateDotManagement()
{
    for (auto& [spellId, dot] : _dots)
    {
        if (_currentTarget)
        {
            dot.isActive = HasAura(spellId, _currentTarget);
            dot.timeRemaining = GetAuraTimeRemaining(spellId, _currentTarget);
        }
        else
        {
            dot.isActive = false;
            dot.timeRemaining = 0;
        }
    }
}

void AssassinationSpecialization::UpdatePoisonStacks()
{
    for (auto& [type, stack] : _poisonStacks)
    {
        if (_currentTarget)
        {
            stack.stacks = GetPoisonStacks(_currentTarget, type);
            stack.timeRemaining = GetAuraTimeRemaining(DEADLY_POISON_9, _currentTarget); // Simplified
        }
        else
        {
            stack.stacks = 0;
            stack.timeRemaining = 0;
        }
    }
}

void AssassinationSpecialization::UpdateStealthAdvantage()
{
    _stealth.hasAdvantage = IsStealthed();
    if (IsStealthed())
    {
        _stealth.canOpenFromStealth = true;
    }
}

void AssassinationSpecialization::UpdateBurstPhase()
{
    if (_burstPhaseStartTime > 0)
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _burstPhaseStartTime > BURST_PHASE_DURATION)
        {
            _burstPhaseStartTime = 0; // End burst phase
        }
    }
}

void AssassinationSpecialization::UpdateExecutePhase()
{
    // Execute phase logic is handled in UpdateCombatPhase
}

void AssassinationSpecialization::UpdateAoEPhase()
{
    // AoE phase logic would check for multiple nearby enemies
    // Simplified for now
}

void AssassinationSpecialization::UpdateEmergencyPhase()
{
    // Emergency checks are handled in UpdateCombatPhase
}

void AssassinationSpecialization::UpdateDotTicks()
{
    // Update DoT tick damage (simplified)
    if (_currentTarget)
    {
        for (auto& [spellId, dot] : _dots)
        {
            if (dot.isActive)
            {
                _metrics.totalDotTicks++;
            }
        }
    }
}

void AssassinationSpecialization::UpdatePoisonCharges()
{
    // WoW 11.2: Poison charges no longer exist
    // Poisons are character buffs that persist for 1 hour
    // This method is kept for compatibility but does nothing
}

void AssassinationSpecialization::UpdateCombatMetrics()
{
    if (!_currentTarget)
        return;

    // Update uptime metrics
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        _metrics.ruptureUptime = HasAura(RUPTURE, _currentTarget) ?
            (_metrics.ruptureUptime + 1.0f) / 2.0f : _metrics.ruptureUptime;

        _metrics.garroteUptime = HasAura(GARROTE, _currentTarget) ?
            (_metrics.garroteUptime + 1.0f) / 2.0f : _metrics.garroteUptime;

        _metrics.poisonUptime = (_targetDebuffs.poisonStacks > 0) ?
            (_metrics.poisonUptime + 1.0f) / 2.0f : _metrics.poisonUptime;
    }
}

void AssassinationSpecialization::AnalyzeRotationEfficiency()
{
    // Performance analysis for optimization
    if (getMSTime() % 10000 == 0) // Every 10 seconds
    {
        TC_LOG_DEBUG("playerbot", "AssassinationSpecialization [{}]: Efficiency Analysis - Rupture: {:.1f}%, Poison: {:.1f}%, Avg CP: {:.1f}",
                    _bot->GetName(), _metrics.ruptureUptime * 100, _metrics.poisonUptime * 100, _metrics.averageComboPointsOnSpend);
    }
}

void AssassinationSpecialization::LogAssassinationDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

// Implementation of pure virtual methods from RogueSpecialization base class
bool AssassinationSpecialization::CastSpell(uint32 spellId, ::Unit* target)
{
    if (!_bot)
        return false;

    // Check if spell can be cast
    if (!CanUseAbility(spellId))
        return false;

    // Get spell info for validation
    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    // Start cooldown tracking
    StartCooldown(spellId);

    // Consume resources
    ConsumeResource(spellId);

    // Track metrics based on spell type
    if (spellId == MUTILATE)
        _metrics.mutilateCasts++;
    else if (spellId == ENVENOM)
        _metrics.envenomCasts++;
    else if (spellId == RUPTURE)
        _metrics.ruptureApplications++;
    else if (spellId == GARROTE)
        _metrics.garroteApplications++;
    else if (spellId == COLD_BLOOD)
        _metrics.coldBloodUsages++;
    else if (spellId == VENDETTA)
        _metrics.coldBloodUsages++; // Track vendetta uses in coldBloodUsages for now

    // Cast the spell through the bot
    if (target)
    {
        return _bot->CastSpell(target, spellId, false);
    }
    else
    {
        return _bot->CastSpell(_bot, spellId, false);
    }
}

bool AssassinationSpecialization::HasSpell(uint32 spellId)
{
    if (!_bot)
        return false;

    return _bot->HasSpell(spellId);
}

SpellInfo const* AssassinationSpecialization::GetSpellInfo(uint32 spellId)
{
    if (!_bot)
        return nullptr;

    return sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());
}

uint32 AssassinationSpecialization::GetSpellCooldown(uint32 spellId)
{
    if (!_bot)
        return 0;

    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    // Check if spell has an active cooldown using modern SpellHistory API
    SpellHistory* spellHistory = _bot->GetSpellHistory();
    if (spellHistory && spellHistory->HasCooldown(spellId))
    {
        auto cooldownDuration = spellHistory->GetRemainingCooldown(spellInfo);
        return static_cast<uint32>(cooldownDuration.count());
    }

    return 0;
}

} // namespace Playerbot