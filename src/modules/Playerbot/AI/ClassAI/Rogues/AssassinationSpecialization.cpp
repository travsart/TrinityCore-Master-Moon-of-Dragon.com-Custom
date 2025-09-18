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
#include "Log.h"
#include "Player.h"

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
    _dots[DEADLY_POISON_9] = DotInfo(DEADLY_POISON_9, 12000); // 12 seconds

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
        if (IsSpellReady(STEALTH) && _bot->IsOutOfCombat())
        {
            if (CastSpell(STEALTH))
            {
                _lastStealthTime = getMSTime();
                LogAssassinationDecision("Entered Stealth", "Preparing for opener");
            }
        }
        else if (IsSpellReady(VANISH) && _bot->isInCombat())
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
    if (_bot->IsOutOfCombat() && !IsStealthed())
        return true;

    // Use Vanish in emergencies
    if (_bot->isInCombat() && _bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Use Vanish for re-opener in long fights
    if (_bot->isInCombat() && IsSpellReady(VANISH))
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
    // Update poison application timing
    uint32 currentTime = getMSTime();
    if (_lastPoisonApplicationTime == 0)
        _lastPoisonApplicationTime = currentTime;

    // Check if poisons need reapplication
    if (currentTime - _lastPoisonApplicationTime > POISON_REAPPLY_INTERVAL)
    {
        ApplyPoisons();
        _lastPoisonApplicationTime = currentTime;
    }

    // Update poison charges
    UpdatePoisonCharges();
}

void AssassinationSpecialization::ApplyPoisons()
{
    PoisonType mainHandPoison = GetOptimalMainHandPoison();
    PoisonType offHandPoison = GetOptimalOffHandPoison();

    // Apply main hand poison
    if (mainHandPoison == PoisonType::INSTANT)
        ApplyInstantPoison();
    else if (mainHandPoison == PoisonType::DEADLY)
        ApplyDeadlyPoison();
    else if (mainHandPoison == PoisonType::WOUND)
        ApplyWoundPoison();

    // Apply off hand poison
    if (offHandPoison == PoisonType::CRIPPLING)
        ApplyCripplingPoison();
    else if (offHandPoison == PoisonType::MIND_NUMBING)
        CastSpell(MIND_NUMBING_POISON_3);

    _metrics.poisonApplications++;
    LogAssassinationDecision("Applied Poisons", "Maintaining weapon enchants");
}

PoisonType AssassinationSpecialization::GetOptimalMainHandPoison()
{
    // Deadly poison for sustained damage
    if (HasSpell(DEADLY_POISON_9))
        return PoisonType::DEADLY;

    // Instant poison for immediate damage
    if (HasSpell(INSTANT_POISON_10))
        return PoisonType::INSTANT;

    return PoisonType::NONE;
}

PoisonType AssassinationSpecialization::GetOptimalOffHandPoison()
{
    // Crippling poison for utility
    if (HasSpell(CRIPPLING_POISON_2))
        return PoisonType::CRIPPLING;

    // Mind numbing against casters
    if (_currentTarget && _currentTarget->GetPowerType() == POWER_MANA)
    {
        if (HasSpell(MIND_NUMBING_POISON_3))
            return PoisonType::MIND_NUMBING;
    }

    return PoisonType::NONE;
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

    // Use if target has poison effects
    return GetPoisonStacks(target, PoisonType::DEADLY) > 0 ||
           GetPoisonStacks(target, PoisonType::INSTANT) > 0;
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

// Poison application methods
void AssassinationSpecialization::ApplyInstantPoison()
{
    if (HasSpell(INSTANT_POISON_10))
        CastSpell(INSTANT_POISON_10);
    else if (HasSpell(INSTANT_POISON_9))
        CastSpell(INSTANT_POISON_9);
}

void AssassinationSpecialization::ApplyDeadlyPoison()
{
    if (HasSpell(DEADLY_POISON_9))
        CastSpell(DEADLY_POISON_9);
}

void AssassinationSpecialization::ApplyWoundPoison()
{
    if (HasSpell(WOUND_POISON_5))
        CastSpell(WOUND_POISON_5);
}

void AssassinationSpecialization::ApplyCripplingPoison()
{
    if (HasSpell(CRIPPLING_POISON_2))
        CastSpell(CRIPPLING_POISON_2);
}

bool AssassinationSpecialization::ShouldApplyPoisons()
{
    uint32 currentTime = getMSTime();
    return (currentTime - _lastPoisonApplicationTime) > POISON_REAPPLY_INTERVAL;
}

uint32 AssassinationSpecialization::GetPoisonStacks(::Unit* target, PoisonType type)
{
    if (!target)
        return 0;

    // Simplified poison stack tracking
    switch (type)
    {
        case PoisonType::DEADLY:
            return HasAura(DEADLY_POISON_9, target) ? 5 : 0;
        case PoisonType::INSTANT:
            return HasAura(INSTANT_POISON_10, target) ? 1 : 0;
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
    // Simplified poison charge tracking
    _poisons.mainHandCharges = 100; // Assume full charges
    _poisons.offHandCharges = 100;
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

} // namespace Playerbot