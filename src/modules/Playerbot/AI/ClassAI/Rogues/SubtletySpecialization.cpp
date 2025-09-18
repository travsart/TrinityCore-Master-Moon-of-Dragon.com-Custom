/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "SubtletySpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"

namespace Playerbot
{

SubtletySpecialization::SubtletySpecialization(Player* bot)
    : RogueSpecialization(bot), _subtletyPhase(SubtletyRotationPhase::STEALTH_PREPARATION),
      _lastStealthEntry(0), _lastStealthExit(0), _isPlanningStealth(false),
      _lastAmbushTime(0), _lastBackstabTime(0), _lastHemorrhageTime(0), _lastEviscerateeTime(0),
      _lastShadowstepTime(0), _lastVanishTime(0), _lastShadowDanceTime(0), _lastPreparationTime(0),
      _prioritizeStealthWindows(true), _useAggressivePositioning(true), _saveEnergyForBurst(true),
      _preferredStealthOpener(AMBUSH), _preferredComboBuilder(BACKSTAB), _preferredFinisher(EVISCERATE)
{
    // Initialize stealth openers in priority order
    _stealthOpeners = {AMBUSH, GARROTE, CHEAP_SHOT, PREMEDITATION};

    // Initialize combo builders in priority order for Subtlety
    _comboBuilders = {BACKSTAB, HEMORRHAGE, SINISTER_STRIKE};

    // Initialize finishers in priority order for Subtlety
    _finishers = {EVISCERATE, RUPTURE, SLICE_AND_DICE, EXPOSE_ARMOR};

    // Initialize stealth abilities
    _stealthAbilities = {STEALTH, VANISH, SHADOW_DANCE, SHADOWSTEP, PREPARATION};

    // Initialize defensive abilities
    _defensiveAbilities = {CLOAK_OF_SHADOWS, EVASION, BLIND, SAP, GOUGE};

    TC_LOG_DEBUG("playerbot", "SubtletySpecialization: Initialized for bot {}", _bot->GetName());
}

void SubtletySpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Update all management systems
    UpdateResourceStates();
    UpdateTargetInfo(target);
    UpdateStealthManagement();
    UpdateShadowDanceManagement();
    UpdateShadowstepManagement();
    UpdatePreparationManagement();
    UpdateHemorrhageManagement();
    UpdateStealthWindows();
    UpdateMasterOfSubtletyBuff();
    UpdateOpportunityTracking();
    UpdateComboPointManagement();
    UpdateEnergyManagement();
    UpdateCombatPhase();
    UpdateSubtletyMetrics();

    // Execute rotation based on current phase
    switch (_subtletyPhase)
    {
        case SubtletyRotationPhase::STEALTH_PREPARATION:
            ExecuteStealthPreparation(target);
            break;
        case SubtletyRotationPhase::STEALTH_OPENER:
            ExecuteStealthOpenerPhase(target);
            break;
        case SubtletyRotationPhase::SHADOW_DANCE_BURST:
            ExecuteShadowDanceBurst(target);
            break;
        case SubtletyRotationPhase::HEMORRHAGE_APPLICATION:
            ExecuteHemorrhageApplication(target);
            break;
        case SubtletyRotationPhase::COMBO_BUILDING:
            ExecuteComboBuildingPhase(target);
            break;
        case SubtletyRotationPhase::COMBO_SPENDING:
            ExecuteComboSpendingPhase(target);
            break;
        case SubtletyRotationPhase::STEALTH_RESET:
            ExecuteStealthReset(target);
            break;
        case SubtletyRotationPhase::SHADOWSTEP_POSITIONING:
            ExecuteShadowstepPositioning(target);
            break;
        case SubtletyRotationPhase::DEFENSIVE_STEALTH:
            ExecuteDefensiveStealth(target);
            break;
        case SubtletyRotationPhase::EXECUTE_PHASE:
            ExecuteExecutePhase(target);
            break;
        case SubtletyRotationPhase::EMERGENCY:
            ExecuteEmergencyPhase(target);
            break;
    }

    CoordinateCooldowns();
    AnalyzeSubtletyEfficiency();
}

void SubtletySpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain Master of Subtlety if available
    if (HasSpell(MASTER_OF_SUBTLETY) && !HasAura(MASTER_OF_SUBTLETY_EFFECT))
    {
        // Master of Subtlety is triggered by breaking stealth
        if (!IsStealthed() && (_lastStealthExit > 0) &&
            (getMSTime() - _lastStealthExit < MASTER_OF_SUBTLETY_DURATION))
        {
            _metrics.masterOfSubtletyProcs++;
        }
    }

    // Minimal poison application for Subtlety
    if (ShouldApplyPoisons())
        ApplyPoisons();

    // Maintain Slice and Dice if we have it
    if (ShouldUseSliceAndDice() && GetComboPoints() >= 1)
    {
        if (CastSpell(SLICE_AND_DICE))
        {
            LogSubtletyDecision("Cast Slice and Dice", "Maintaining attack speed");
        }
    }
}

void SubtletySpecialization::UpdateCooldowns(uint32 diff)
{
    RogueSpecialization::UpdateCooldownTracking(diff);

    // Update Shadow Dance timer
    if (_shadowDance.isActive)
    {
        if (_shadowDance.remainingTime > diff)
            _shadowDance.remainingTime -= diff;
        else
        {
            _shadowDance.isActive = false;
            _shadowDance.remainingTime = 0;
            LogSubtletyDecision("Shadow Dance Ended", "Burst window closed");
        }
    }

    // Update current stealth window
    if (IsStealthed() && _currentStealthWindow.startTime > 0)
    {
        _currentStealthWindow.duration = getMSTime() - _currentStealthWindow.startTime;
    }

    // Update shadowstep cooldown tracking
    if (_shadowstep.isOnCooldown)
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _shadowstep.lastUsed > SHADOWSTEP_COOLDOWN)
        {
            _shadowstep.isOnCooldown = false;
        }
    }
}

bool SubtletySpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughEnergyFor(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    // Stealth-only abilities
    if ((spellId == AMBUSH || spellId == GARROTE || spellId == CHEAP_SHOT) && !IsStealthed())
        return false;

    // Behind-target requirements
    if ((spellId == BACKSTAB || spellId == AMBUSH) && _currentTarget && !IsBehindTarget(_currentTarget))
        return false;

    // Combo point requirements
    if (spellId == EVISCERATE || spellId == RUPTURE || spellId == SLICE_AND_DICE)
    {
        if (GetComboPoints() == 0)
            return false;
    }

    // Shadowstep requires target and not on cooldown
    if (spellId == SHADOWSTEP && (_currentTarget == nullptr || _shadowstep.isOnCooldown))
        return false;

    return true;
}

void SubtletySpecialization::OnCombatStart(::Unit* target)
{
    if (!target)
        return;

    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = SubtletyMetrics();

    // Start with stealth preparation if not already stealthed
    if (IsStealthed())
    {
        _subtletyPhase = SubtletyRotationPhase::STEALTH_OPENER;
        LogSubtletyDecision("Combat Start", "Beginning with stealth opener");
    }
    else
    {
        _subtletyPhase = SubtletyRotationPhase::STEALTH_PREPARATION;
        LogSubtletyDecision("Combat Start", "Preparing stealth for opener");
    }

    // Plan initial stealth window
    PlanStealthWindow(target);
}

void SubtletySpecialization::OnCombatEnd()
{
    // Analyze final stealth window if active
    if (IsStealthed() && _currentStealthWindow.startTime > 0)
    {
        _currentStealthWindow.duration = getMSTime() - _currentStealthWindow.startTime;
        AnalyzeStealthWindow(_currentStealthWindow);
    }

    // Log combat statistics
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "SubtletySpecialization [{}]: Combat ended. Duration: {}ms, Stealth uptime: {:.1f}%, Ambush: {}, Backstab: {}",
                _bot->GetName(), combatDuration, _metrics.stealthUptime * 100, _metrics.ambushCasts, _metrics.backstabCasts);

    // Reset phases and state
    _subtletyPhase = SubtletyRotationPhase::STEALTH_PREPARATION;
    _shadowDance.isActive = false;
    _currentStealthWindow = StealthWindow();
    _currentTarget = nullptr;
}

bool SubtletySpecialization::HasEnoughResource(uint32 spellId)
{
    return HasEnoughEnergyFor(spellId);
}

void SubtletySpecialization::ConsumeResource(uint32 spellId)
{
    uint32 energyCost = GetEnergyCost(spellId);
    if (energyCost > 0)
    {
        _bot->ModifyPower(POWER_ENERGY, -static_cast<int32>(energyCost));
        _totalEnergySpent += energyCost;
    }
}

Position SubtletySpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    // Subtlety prefers behind target for Backstab and Ambush
    float angle = target->GetOrientation() + M_PI; // Directly behind target
    float distance = 1.5f; // Very close for stealth advantage

    // If Shadowstep is available, consider teleporting to optimal position
    if (!_shadowstep.isOnCooldown && ShouldUseShadowstep(target))
    {
        return GetShadowstepPosition(target);
    }

    float x = target->GetPositionX() + cos(angle) * distance;
    float y = target->GetPositionY() + sin(angle) * distance;
    float z = target->GetPositionZ();

    return Position(x, y, z, angle);
}

float SubtletySpecialization::GetOptimalRange(::Unit* target)
{
    // Subtlety prefers very close range for stealth advantage
    return 1.5f;
}

void SubtletySpecialization::UpdateStealthManagement()
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();

    // Track stealth state changes
    bool wasStealthed = (_lastStealthEntry > _lastStealthExit);
    bool isNowStealthed = IsStealthed();

    if (!wasStealthed && isNowStealthed)
    {
        // Entered stealth
        _lastStealthEntry = currentTime;
        _currentStealthWindow = StealthWindow();
        _currentStealthWindow.startTime = currentTime;
        _metrics.totalStealthTime = currentTime;
    }
    else if (wasStealthed && !isNowStealthed)
    {
        // Exited stealth
        _lastStealthExit = currentTime;
        if (_currentStealthWindow.startTime > 0)
        {
            _currentStealthWindow.duration = currentTime - _currentStealthWindow.startTime;
            AnalyzeStealthWindow(_currentStealthWindow);
            _stealthWindows.push(_currentStealthWindow);

            // Keep only recent stealth windows
            while (_stealthWindows.size() > 5)
                _stealthWindows.pop();
        }
    }

    // Plan stealth usage
    if (ShouldEnterStealth() && !IsStealthed())
    {
        EnterStealth();
    }
}

bool SubtletySpecialization::ShouldEnterStealth()
{
    if (!_bot)
        return false;

    // Always want stealth before combat
    if (_bot->IsOutOfCombat())
        return true;

    // Use stealth for burst windows
    if (ShouldUseShadowDance())
        return true;

    // Use Vanish for re-stealth in combat
    if (ShouldUseVanish())
        return true;

    // Emergency stealth
    if (_bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Stealth for optimal rotation timing
    if (_subtletyPhase == SubtletyRotationPhase::STEALTH_RESET)
        return true;

    return false;
}

bool SubtletySpecialization::CanBreakStealth()
{
    // Only break stealth for optimal openers
    if (_subtletyPhase == SubtletyRotationPhase::STEALTH_OPENER)
        return true;

    // Break stealth during Shadow Dance for multiple openers
    if (_shadowDance.isActive)
        return true;

    return false;
}

void SubtletySpecialization::ExecuteStealthOpener(::Unit* target)
{
    if (!target || !IsStealthed())
        return;

    // Track stealth opener usage
    _metrics.stealthOpeners++;
    _currentStealthWindow.abilitiesUsed++;

    if (ShouldUseAmbushOpener(target))
        ExecuteAmbushOpener(target);
    else if (ShouldUseGarroteOpener(target))
        ExecuteGarroteOpener(target);
    else if (ShouldUseCheapShotOpener(target))
        ExecuteCheapShotOpener(target);
    else if (ShouldUsePremeditationOpener(target))
        ExecutePremeditationOpener(target);
}

void SubtletySpecialization::UpdateComboPointManagement()
{
    _comboPoints.current = GetComboPoints();
    _comboPoints.shouldSpend = ShouldSpendComboPoints();
}

bool SubtletySpecialization::ShouldBuildComboPoints()
{
    return GetComboPoints() < 5 && !ShouldSpendComboPoints();
}

bool SubtletySpecialization::ShouldSpendComboPoints()
{
    uint8 comboPoints = GetComboPoints();

    // Always spend at 5 combo points
    if (comboPoints >= 5)
        return true;

    // Spend at 4+ if high energy or execute phase
    if (comboPoints >= 4)
    {
        if (_energy.state >= EnergyState::HIGH)
            return true;
        if (_currentTarget && _currentTarget->GetHealthPct() < (EXECUTE_HEALTH_THRESHOLD * 100))
            return true;
    }

    // Spend at 3+ for emergency situations
    if (comboPoints >= MIN_COMBO_FOR_EVISCERATE && _bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Don't spend if saving energy for stealth window
    if (_saveEnergyForBurst && _isPlanningStealth)
        return false;

    return false;
}

void SubtletySpecialization::ExecuteComboBuilder(::Unit* target)
{
    if (!target)
        return;

    // Backstab is preferred when behind target
    if (ShouldUseBackstab(target))
    {
        if (CastSpell(BACKSTAB, target))
        {
            _metrics.backstabCasts++;
            _totalCombosBuilt++;
            _lastBackstabTime = getMSTime();
            LogSubtletyDecision("Cast Backstab", "Positional combo builder");
            return;
        }
    }

    // Hemorrhage for debuff and combo building
    if (ShouldUseHemorrhage(target))
    {
        if (CastSpell(HEMORRHAGE, target))
        {
            _metrics.hemorrhageCasts++;
            _totalCombosBuilt++;
            _lastHemorrhageTime = getMSTime();
            LogSubtletyDecision("Cast Hemorrhage", "Debuff combo builder");
            return;
        }
    }

    // Fallback to Sinister Strike
    if (HasSpell(SINISTER_STRIKE))
    {
        if (CastSpell(SINISTER_STRIKE, target))
        {
            _totalCombosBuilt++;
            LogSubtletyDecision("Cast Sinister Strike", "Fallback combo builder");
        }
    }
}

void SubtletySpecialization::ExecuteComboSpender(::Unit* target)
{
    if (!target || GetComboPoints() == 0)
        return;

    uint8 comboPoints = GetComboPoints();
    _totalCombosSpent += comboPoints;

    // Eviscerate for high damage
    if (ShouldUseEviscerate(target))
    {
        if (CastSpell(EVISCERATE, target))
        {
            _metrics.eviscerateCasts++;
            _lastEviscerateeTime = getMSTime();
            LogSubtletyDecision("Cast Eviscerate", "High damage finisher");
            return;
        }
    }

    // Rupture for DoT damage
    if (ShouldUseRupture(target))
    {
        if (CastSpell(RUPTURE, target))
        {
            LogSubtletyDecision("Cast Rupture", "DoT finisher");
            return;
        }
    }

    // Slice and Dice for attack speed
    if (ShouldUseSliceAndDice())
    {
        if (CastSpell(SLICE_AND_DICE))
        {
            LogSubtletyDecision("Cast Slice and Dice", "Attack speed buff");
        }
    }
}

void SubtletySpecialization::UpdatePoisonManagement()
{
    // Subtlety uses minimal poisons
    uint32 currentTime = getMSTime();
    if (_lastPoisonApplicationTime == 0)
        _lastPoisonApplicationTime = currentTime;

    // Apply poisons less frequently than other specs
    if (currentTime - _lastPoisonApplicationTime > (POISON_REAPPLY_INTERVAL * 3))
    {
        ApplyPoisons();
        _lastPoisonApplicationTime = currentTime;
    }
}

void SubtletySpecialization::ApplyPoisons()
{
    // Subtlety typically uses Instant Poison on main hand only
    if (HasWeaponInMainHand())
    {
        if (HasSpell(INSTANT_POISON_10))
            CastSpell(INSTANT_POISON_10);
        else if (HasSpell(INSTANT_POISON_9))
            CastSpell(INSTANT_POISON_9);
    }

    LogSubtletyDecision("Applied Minimal Poisons", "Basic weapon enhancement");
}

PoisonType SubtletySpecialization::GetOptimalMainHandPoison()
{
    // Subtlety prefers Instant Poison for immediate damage
    if (HasSpell(INSTANT_POISON_10))
        return PoisonType::INSTANT;

    return PoisonType::NONE;
}

PoisonType SubtletySpecialization::GetOptimalOffHandPoison()
{
    // Subtlety rarely uses off-hand poisons
    return PoisonType::NONE;
}

void SubtletySpecialization::UpdateDebuffManagement()
{
    if (!_currentTarget)
        return;

    UpdateTargetInfo(_currentTarget);

    // Check Hemorrhage debuff
    if (ShouldRefreshHemorrhage(_currentTarget))
        _subtletyPhase = SubtletyRotationPhase::HEMORRHAGE_APPLICATION;
}

bool SubtletySpecialization::ShouldRefreshDebuff(uint32 spellId)
{
    if (spellId == HEMORRHAGE)
        return ShouldRefreshHemorrhage(_currentTarget);

    return false;
}

void SubtletySpecialization::ApplyDebuffs(::Unit* target)
{
    if (ShouldApplyHemorrhage(target))
        ApplyHemorrhage(target);
}

void SubtletySpecialization::UpdateEnergyManagement()
{
    RogueSpecialization::UpdateResourceStates();

    // Subtlety needs to manage energy for stealth windows
    OptimizeEnergyForStealth();
}

bool SubtletySpecialization::HasEnoughEnergyFor(uint32 spellId)
{
    uint32 cost = GetEnergyCost(spellId);
    uint32 currentEnergy = GetCurrentEnergy();

    // Reserve energy for stealth windows if planning burst
    if (_isPlanningStealth && _saveEnergyForBurst)
    {
        uint32 reservedEnergy = GetEnergyNeededForStealthRotation();
        return currentEnergy >= (cost + reservedEnergy);
    }

    return currentEnergy >= cost;
}

uint32 SubtletySpecialization::GetEnergyCost(uint32 spellId)
{
    return RogueSpecialization::GetEnergyCost(spellId);
}

bool SubtletySpecialization::ShouldWaitForEnergy()
{
    // Wait for energy if planning stealth window
    if (_isPlanningStealth && _energy.state < EnergyState::HIGH)
        return true;

    // Wait if critical energy and not emergency
    if (_energy.state == EnergyState::CRITICAL)
    {
        if (_bot && _bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
            return true;
    }

    return false;
}

void SubtletySpecialization::UpdateCooldownTracking(uint32 diff)
{
    RogueSpecialization::UpdateCooldownTracking(diff);
}

bool SubtletySpecialization::IsSpellReady(uint32 spellId)
{
    return RogueSpecialization::IsSpellReady(spellId);
}

void SubtletySpecialization::StartCooldown(uint32 spellId)
{
    RogueSpecialization::StartCooldown(spellId);
}

uint32 SubtletySpecialization::GetCooldownRemaining(uint32 spellId)
{
    return RogueSpecialization::GetCooldownRemaining(spellId);
}

void SubtletySpecialization::UpdateCombatPhase()
{
    if (!_bot || !_currentTarget)
        return;

    // Emergency phase check
    if (_bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
    {
        _subtletyPhase = SubtletyRotationPhase::EMERGENCY;
        return;
    }

    // Execute phase
    if (_currentTarget->GetHealthPct() < (EXECUTE_HEALTH_THRESHOLD * 100))
    {
        _subtletyPhase = SubtletyRotationPhase::EXECUTE_PHASE;
        return;
    }

    // Shadow Dance burst phase
    if (_shadowDance.isActive || ShouldUseShadowDance())
    {
        _subtletyPhase = SubtletyRotationPhase::SHADOW_DANCE_BURST;
        return;
    }

    // Stealth opener phase
    if (IsStealthed() && CanBreakStealth())
    {
        _subtletyPhase = SubtletyRotationPhase::STEALTH_OPENER;
        return;
    }

    // Shadowstep positioning
    if (ShouldUseShadowstep(_currentTarget))
    {
        _subtletyPhase = SubtletyRotationPhase::SHADOWSTEP_POSITIONING;
        return;
    }

    // Hemorrhage application
    if (ShouldRefreshHemorrhage(_currentTarget))
    {
        _subtletyPhase = SubtletyRotationPhase::HEMORRHAGE_APPLICATION;
        return;
    }

    // Stealth reset for burst windows
    if (ShouldUseVanish() || (ShouldEnterStealth() && !IsStealthed()))
    {
        _subtletyPhase = SubtletyRotationPhase::STEALTH_RESET;
        return;
    }

    // Combo spending
    if (ShouldSpendComboPoints())
    {
        _subtletyPhase = SubtletyRotationPhase::COMBO_SPENDING;
        return;
    }

    // Default to combo building
    _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
}

CombatPhase SubtletySpecialization::GetCurrentPhase()
{
    switch (_subtletyPhase)
    {
        case SubtletyRotationPhase::STEALTH_PREPARATION:
        case SubtletyRotationPhase::STEALTH_OPENER:
            return CombatPhase::STEALTH_OPENER;
        case SubtletyRotationPhase::SHADOW_DANCE_BURST:
            return CombatPhase::BURST_PHASE;
        case SubtletyRotationPhase::COMBO_SPENDING:
            return CombatPhase::COMBO_SPENDING;
        case SubtletyRotationPhase::EXECUTE_PHASE:
            return CombatPhase::EXECUTE_PHASE;
        case SubtletyRotationPhase::EMERGENCY:
        case SubtletyRotationPhase::DEFENSIVE_STEALTH:
            return CombatPhase::EMERGENCY;
        default:
            return CombatPhase::COMBO_BUILDING;
    }
}

bool SubtletySpecialization::ShouldExecuteBurstRotation()
{
    return _shadowDance.isActive || ShouldUseShadowDance() ||
           (_isPlanningStealth && _energy.state >= EnergyState::HIGH);
}

// Phase execution methods
void SubtletySpecialization::ExecuteStealthPreparation(::Unit* target)
{
    PlanStealthWindow(target);

    if (ShouldEnterStealth())
    {
        EnterStealth();
        _subtletyPhase = SubtletyRotationPhase::STEALTH_OPENER;
    }
    else if (_energy.state < EnergyState::HIGH)
    {
        // Wait for energy
        LogSubtletyDecision("Preparing for Stealth", "Building energy for stealth window");
    }
    else
    {
        // Fall back to normal rotation
        _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
    }
}

void SubtletySpecialization::ExecuteStealthOpenerPhase(::Unit* target)
{
    if (IsStealthed())
    {
        ExecuteStealthOpener(target);
    }
    else
    {
        // Stealth broke, continue normal rotation
        _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
    }
}

void SubtletySpecialization::ExecuteShadowDanceBurst(::Unit* target)
{
    if (!_shadowDance.isActive && ShouldUseShadowDance())
    {
        ActivateShadowDance();
    }

    if (_shadowDance.isActive)
    {
        ExecuteShadowDanceRotation(target);
    }
    else
    {
        _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
    }
}

void SubtletySpecialization::ExecuteHemorrhageApplication(::Unit* target)
{
    if (ShouldApplyHemorrhage(target) || ShouldRefreshHemorrhage(target))
    {
        ApplyHemorrhage(target);
    }

    _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
}

void SubtletySpecialization::ExecuteComboBuildingPhase(::Unit* target)
{
    if (ShouldBuildComboPoints())
    {
        ExecuteComboBuilder(target);
    }
    else
    {
        _subtletyPhase = SubtletyRotationPhase::COMBO_SPENDING;
    }
}

void SubtletySpecialization::ExecuteComboSpendingPhase(::Unit* target)
{
    ExecuteComboSpender(target);
    _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
}

void SubtletySpecialization::ExecuteStealthReset(::Unit* target)
{
    if (ShouldUseVanish())
    {
        ActivateVanish();
    }
    else if (ShouldUseShadowDance())
    {
        ActivateShadowDance();
        _subtletyPhase = SubtletyRotationPhase::SHADOW_DANCE_BURST;
        return;
    }
    else if (ShouldUsePreparation())
    {
        UsePreparation();
    }

    if (IsStealthed())
    {
        _subtletyPhase = SubtletyRotationPhase::STEALTH_OPENER;
    }
    else
    {
        _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
    }
}

void SubtletySpecialization::ExecuteShadowstepPositioning(::Unit* target)
{
    if (ShouldUseShadowstep(target))
    {
        ExecuteShadowstep(target);
    }

    _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
}

void SubtletySpecialization::ExecuteDefensiveStealth(::Unit* target)
{
    HandleStealthDefense();

    if (_bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
        _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
}

void SubtletySpecialization::ExecuteExecutePhase(::Unit* target)
{
    // Prioritize high damage abilities in execute phase
    if (GetComboPoints() >= MIN_COMBO_FOR_EVISCERATE)
    {
        ExecuteComboSpender(target);
    }
    else
    {
        ExecuteComboBuilder(target);
    }
}

void SubtletySpecialization::ExecuteEmergencyPhase(::Unit* target)
{
    HandleStealthDefense();

    if (_bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
        _subtletyPhase = SubtletyRotationPhase::COMBO_BUILDING;
}

// Stealth management methods
void SubtletySpecialization::EnterStealth()
{
    if (_bot->IsOutOfCombat() && !IsStealthed())
    {
        if (CastSpell(STEALTH))
        {
            LogSubtletyDecision("Entered Stealth", "Pre-combat preparation");
        }
    }
}

void SubtletySpecialization::ActivateVanish()
{
    if (CastSpell(VANISH))
    {
        _metrics.vanishUses++;
        _lastVanishTime = getMSTime();
        LogSubtletyDecision("Used Vanish", "Re-stealth for burst window");
    }
}

void SubtletySpecialization::ActivateShadowDance()
{
    if (CastSpell(SHADOW_DANCE))
    {
        _shadowDance.isActive = true;
        _shadowDance.remainingTime = SHADOW_DANCE_DURATION;
        _shadowDance.lastActivation = getMSTime();
        _metrics.shadowDanceActivations++;
        InitiateShadowDanceBurst();
        LogSubtletyDecision("Activated Shadow Dance", "Stealth burst window");
    }
}

void SubtletySpecialization::UsePreparation()
{
    if (CastSpell(PREPARATION))
    {
        _preparation.lastUsed = getMSTime();
        _preparation.totalUses++;
        _metrics.preparationUses++;
        LogSubtletyDecision("Used Preparation", "Reset cooldowns");
    }
}

void SubtletySpecialization::PlanStealthWindow(::Unit* target)
{
    if (!target)
        return;

    _isPlanningStealth = true;

    // Plan energy usage for optimal stealth window
    PlanEnergyForStealthWindow();

    LogSubtletyDecision("Planning Stealth Window", "Optimizing burst timing");
}

void SubtletySpecialization::ExecuteStealthWindow(::Unit* target)
{
    if (!IsStealthed() || !target)
        return;

    // Execute planned stealth rotation
    OptimizeStealthWindowUsage();

    if (_currentStealthWindow.abilitiesUsed < 3) // Can use more abilities
    {
        ExecuteStealthOpener(target);
    }
}

bool SubtletySpecialization::ShouldUseVanish()
{
    if (!HasSpell(VANISH) || !IsSpellReady(VANISH))
        return false;

    // Use Vanish for emergency escape
    if (_bot->GetHealthPct() < 25.0f)
        return true;

    // Use Vanish for burst windows
    if (_energy.state >= EnergyState::HIGH && !_shadowDance.isActive)
        return true;

    return false;
}

bool SubtletySpecialization::ShouldUseShadowDance()
{
    if (!HasSpell(SHADOW_DANCE) || !IsSpellReady(SHADOW_DANCE))
        return false;

    // Use on cooldown for maximum DPS
    return _energy.state >= EnergyState::HIGH;
}

bool SubtletySpecialization::ShouldUsePreparation()
{
    if (!HasSpell(PREPARATION) || !IsSpellReady(PREPARATION))
        return false;

    // Use when major cooldowns are down
    return !IsSpellReady(VANISH) && !IsSpellReady(SHADOW_DANCE);
}

// Shadowstep methods
void SubtletySpecialization::ExecuteShadowstep(::Unit* target)
{
    if (CastSpell(SHADOWSTEP, target))
    {
        _shadowstep.lastUsed = getMSTime();
        _shadowstep.totalUses++;
        _shadowstep.isOnCooldown = true;
        _metrics.shadowstepUses++;
        LogSubtletyDecision("Used Shadowstep", "Optimal positioning");
    }
}

bool SubtletySpecialization::ShouldUseShadowstep(::Unit* target)
{
    if (!HasSpell(SHADOWSTEP) || _shadowstep.isOnCooldown || !target)
        return false;

    // Use for positioning advantage
    if (!IsBehindTarget(target) && !IsInMeleeRange(target))
        return true;

    // Use for gap closing
    if (_bot->GetDistance(target) > 8.0f)
        return true;

    return false;
}

Position SubtletySpecialization::GetShadowstepPosition(::Unit* target)
{
    if (!target)
        return Position();

    // Position behind target after Shadowstep
    float angle = target->GetOrientation() + M_PI;
    float distance = 2.0f;

    float x = target->GetPositionX() + cos(angle) * distance;
    float y = target->GetPositionY() + sin(angle) * distance;
    float z = target->GetPositionZ();

    return Position(x, y, z, angle);
}

// Stealth opener methods
void SubtletySpecialization::ExecuteAmbushOpener(::Unit* target)
{
    if (CastSpell(AMBUSH, target))
    {
        _metrics.ambushCasts++;
        _lastAmbushTime = getMSTime();
        LogSubtletyDecision("Ambush Opener", "High damage stealth opener");
    }
}

void SubtletySpecialization::ExecuteGarroteOpener(::Unit* target)
{
    if (CastSpell(GARROTE, target))
    {
        LogSubtletyDecision("Garrote Opener", "DoT stealth opener");
    }
}

void SubtletySpecialization::ExecuteCheapShotOpener(::Unit* target)
{
    if (CastSpell(CHEAP_SHOT, target))
    {
        LogSubtletyDecision("Cheap Shot Opener", "Stun stealth opener");
    }
}

void SubtletySpecialization::ExecutePremeditationOpener(::Unit* target)
{
    if (HasSpell(PREMEDITATION) && CastSpell(PREMEDITATION, target))
    {
        LogSubtletyDecision("Premeditation Opener", "Combo point stealth opener");
    }
}

bool SubtletySpecialization::ShouldUseAmbushOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(AMBUSH) && IsBehindTarget(target);
}

bool SubtletySpecialization::ShouldUseGarroteOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(GARROTE) && !HasAura(GARROTE, target);
}

bool SubtletySpecialization::ShouldUseCheapShotOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(CHEAP_SHOT) && !target->HasUnitState(UNIT_STATE_STUNNED);
}

bool SubtletySpecialization::ShouldUsePremeditationOpener(::Unit* target)
{
    return IsStealthed() && HasSpell(PREMEDITATION) && GetComboPoints() < 2;
}

// Hemorrhage methods
void SubtletySpecialization::ApplyHemorrhage(::Unit* target)
{
    if (CastSpell(HEMORRHAGE, target))
    {
        _hemorrhage.lastApplication = getMSTime();
        _hemorrhage.totalApplications++;
        LogSubtletyDecision("Applied Hemorrhage", "Debuff and combo building");
    }
}

bool SubtletySpecialization::ShouldApplyHemorrhage(::Unit* target)
{
    if (!HasSpell(HEMORRHAGE) || !target)
        return false;

    return !HasAura(HEMORRHAGE, target);
}

bool SubtletySpecialization::ShouldRefreshHemorrhage(::Unit* target)
{
    if (!target)
        return false;

    uint32 remaining = GetHemorrhageTimeRemaining(target);
    return remaining < (HEMORRHAGE_DURATION * HEMORRHAGE_REFRESH_THRESHOLD);
}

uint32 SubtletySpecialization::GetHemorrhageTimeRemaining(::Unit* target)
{
    return GetAuraTimeRemaining(HEMORRHAGE, target);
}

// Combat optimization methods
bool SubtletySpecialization::ShouldUseBackstab(::Unit* target)
{
    if (!HasSpell(BACKSTAB) || !target)
        return false;

    return IsBehindTarget(target) && HasEnoughEnergyFor(BACKSTAB);
}

bool SubtletySpecialization::ShouldUseHemorrhage(::Unit* target)
{
    if (!HasSpell(HEMORRHAGE) || !target)
        return false;

    return ShouldApplyHemorrhage(target) || ShouldRefreshHemorrhage(target);
}

bool SubtletySpecialization::ShouldUseEviscerate(::Unit* target)
{
    return HasSpell(EVISCERATE) && GetComboPoints() >= MIN_COMBO_FOR_EVISCERATE;
}

bool SubtletySpecialization::ShouldUseRupture(::Unit* target)
{
    if (!HasSpell(RUPTURE) || !target)
        return false;

    return GetComboPoints() >= 4 && !HasAura(RUPTURE, target);
}

bool SubtletySpecialization::ShouldUseSliceAndDice()
{
    return HasSpell(SLICE_AND_DICE) && !HasAura(SLICE_AND_DICE) && GetComboPoints() >= 1;
}

// Shadow Dance methods
void SubtletySpecialization::InitiateShadowDanceBurst()
{
    _shadowDance.abilitiesUsedDuringDance = 0;
    _shadowDance.stealthOpenersDuringDance = 0;
    LogSubtletyDecision("Initiated Shadow Dance Burst", "Multiple stealth abilities");
}

void SubtletySpecialization::ExecuteShadowDanceRotation(::Unit* target)
{
    if (!_shadowDance.isActive)
        return;

    // Use stealth abilities multiple times during Shadow Dance
    if (_shadowDance.abilitiesUsedDuringDance < 3)
    {
        ExecuteStealthOpener(target);
        _shadowDance.abilitiesUsedDuringDance++;
    }
    else
    {
        // Normal rotation during remaining Shadow Dance time
        if (ShouldBuildComboPoints())
            ExecuteComboBuilder(target);
        else
            ExecuteComboSpender(target);
    }
}

// Defensive methods
void SubtletySpecialization::HandleStealthDefense()
{
    if (ShouldUseDefensiveStealth())
    {
        if (ShouldUseVanish())
            ActivateVanish();
        else if (ShouldUseCloak())
            ExecuteCloak();
        else if (ShouldUseCrowdControl())
            ExecuteBlind(_currentTarget);
    }
}

void SubtletySpecialization::ExecuteCloak()
{
    if (CastSpell(CLOAK_OF_SHADOWS))
    {
        LogSubtletyDecision("Activated Cloak of Shadows", "Magic immunity");
    }
}

bool SubtletySpecialization::ShouldUseDefensiveStealth()
{
    return _bot->GetHealthPct() < 40.0f;
}

bool SubtletySpecialization::ShouldUseCloak()
{
    return _bot->GetHealthPct() < 50.0f && IsSpellReady(CLOAK_OF_SHADOWS);
}

// Energy optimization
void SubtletySpecialization::OptimizeEnergyForStealth()
{
    if (_isPlanningStealth)
    {
        uint32 neededEnergy = GetEnergyNeededForStealthRotation();
        if (GetCurrentEnergy() < neededEnergy)
        {
            LogSubtletyDecision("Saving Energy for Stealth", "Building energy for burst window");
        }
    }
}

uint32 SubtletySpecialization::GetEnergyNeededForStealthRotation()
{
    // Calculate energy needed for optimal stealth window
    return STEALTH_ENERGY_RESERVE; // Reserve 80 energy for stealth burst
}

// Update methods
void SubtletySpecialization::UpdateShadowDanceManagement()
{
    if (_shadowDance.isActive)
    {
        _metrics.totalShadowDanceTime += 1000; // 1 second intervals
    }
}

void SubtletySpecialization::UpdateShadowstepManagement()
{
    // Update shadowstep cooldown tracking
    if (_shadowstep.isOnCooldown)
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _shadowstep.lastUsed > SHADOWSTEP_COOLDOWN)
        {
            _shadowstep.isOnCooldown = false;
        }
    }
}

void SubtletySpecialization::UpdatePreparationManagement()
{
    // Track preparation usage for cooldown coordination
}

void SubtletySpecialization::UpdateHemorrhageManagement()
{
    if (_currentTarget)
    {
        _hemorrhage.isActive = HasAura(HEMORRHAGE, _currentTarget);
        _hemorrhage.timeRemaining = GetHemorrhageTimeRemaining(_currentTarget);
    }
}

void SubtletySpecialization::UpdateStealthWindows()
{
    // Update current stealth window tracking
    if (IsStealthed() && _currentStealthWindow.startTime > 0)
    {
        _currentStealthWindow.duration = getMSTime() - _currentStealthWindow.startTime;
    }
}

void SubtletySpecialization::UpdateMasterOfSubtletyBuff()
{
    // Track Master of Subtlety buff uptime
    if (HasAura(MASTER_OF_SUBTLETY_EFFECT))
    {
        _metrics.masterOfSubtletyProcs++;
    }
}

void SubtletySpecialization::UpdateOpportunityTracking()
{
    // Track Opportunity procs
    if (HasAura(OPPORTUNITY))
    {
        _metrics.opportunityProcs++;
    }
}

void SubtletySpecialization::CoordinateCooldowns()
{
    // Plan cooldown usage for optimal burst windows
    PlanCooldownUsage();
}

void SubtletySpecialization::PlanCooldownUsage()
{
    // Coordinate Vanish, Shadow Dance, and Preparation usage
    if (ShouldSaveCooldownForBurst())
    {
        _saveEnergyForBurst = true;
    }
}

bool SubtletySpecialization::ShouldSaveCooldownForBurst()
{
    // Save cooldowns when multiple are available
    return IsSpellReady(VANISH) && IsSpellReady(SHADOW_DANCE);
}

void SubtletySpecialization::AnalyzeStealthWindow(const StealthWindow& window)
{
    if (window.duration < STEALTH_WINDOW_MIN_DURATION)
        return;

    float efficiency = CalculateStealthWindowEfficiency(window);
    _metrics.averageStealthWindowDuration = (_metrics.averageStealthWindowDuration + window.duration) / 2.0f;
    _metrics.averageDamagePerStealthWindow = (_metrics.averageDamagePerStealthWindow + window.damageDealt) / 2.0f;

    TC_LOG_DEBUG("playerbot", "SubtletySpecialization [{}]: Stealth window - Duration: {}ms, Abilities: {}, Efficiency: {:.2f}",
                _bot->GetName(), window.duration, window.abilitiesUsed, efficiency);
}

float SubtletySpecialization::CalculateStealthWindowEfficiency(const StealthWindow& window)
{
    if (window.duration == 0)
        return 0.0f;

    return static_cast<float>(window.abilitiesUsed * 1000) / window.duration; // Abilities per second
}

void SubtletySpecialization::UpdateSubtletyMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate uptime percentages
        _metrics.stealthUptime = static_cast<float>(_metrics.totalStealthTime) / combatTime;
        _metrics.shadowDanceUptime = static_cast<float>(_metrics.totalShadowDanceTime) / combatTime;

        if (_currentTarget)
        {
            _metrics.hemorrhageUptime = HasAura(HEMORRHAGE, _currentTarget) ?
                (_metrics.hemorrhageUptime + 1.0f) / 2.0f : _metrics.hemorrhageUptime;
        }

        // Calculate positional advantage
        if (_metrics.backstabCasts + _metrics.ambushCasts > 0)
        {
            _metrics.positionalAdvantagePercentage =
                static_cast<float>(_metrics.backstabCasts + _metrics.ambushCasts) /
                (_metrics.backstabCasts + _metrics.ambushCasts + _metrics.hemorrhageCasts);
        }
    }
}

void SubtletySpecialization::AnalyzeSubtletyEfficiency()
{
    // Performance analysis every 20 seconds
    if (getMSTime() % 20000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "SubtletySpecialization [{}]: Efficiency - Stealth: {:.1f}%, Position: {:.1f}%, Shadow Dance: {}",
                    _bot->GetName(), _metrics.stealthUptime * 100, _metrics.positionalAdvantagePercentage * 100,
                    _metrics.shadowDanceActivations);
    }
}

void SubtletySpecialization::LogSubtletyDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

void SubtletySpecialization::OptimizeStealthUsage()
{
    // Analyze and optimize stealth window usage
    OptimizeStealthWindowUsage();
}

void SubtletySpecialization::OptimizeStealthWindowUsage()
{
    // Optimize abilities used during stealth windows
    if (_stealthWindows.size() >= 3)
    {
        // Analyze recent stealth windows for optimization
        float avgEfficiency = 0.0f;
        std::queue<StealthWindow> tempQueue = _stealthWindows;

        while (!tempQueue.empty())
        {
            avgEfficiency += CalculateStealthWindowEfficiency(tempQueue.front());
            tempQueue.pop();
        }

        avgEfficiency /= _stealthWindows.size();

        if (avgEfficiency < 1.0f) // Less than 1 ability per second
        {
            _prioritizeStealthWindows = true;
        }
    }
}

void SubtletySpecialization::PlanEnergyForStealthWindow()
{
    // Reserve energy for upcoming stealth window
    uint32 neededEnergy = GetEnergyNeededForStealthRotation();

    if (GetCurrentEnergy() >= neededEnergy)
    {
        _isPlanningStealth = false; // Ready for stealth
    }
    else
    {
        _saveEnergyForBurst = true;
    }
}

uint32 SubtletySpecialization::GetNextBurstWindowTime()
{
    // Calculate when next burst window should occur
    uint32 shadowDanceCooldown = GetCooldownRemaining(SHADOW_DANCE);
    uint32 vanishCooldown = GetCooldownRemaining(VANISH);

    return std::min(shadowDanceCooldown, vanishCooldown);
}

bool SubtletySpecialization::IsInOptimalPosition(::Unit* target)
{
    return IsBehindTarget(target) && IsInMeleeRange(target);
}

void SubtletySpecialization::OptimizePositionalAdvantage(::Unit* target)
{
    if (!IsInOptimalPosition(target))
    {
        if (ShouldUseShadowstep(target))
        {
            _subtletyPhase = SubtletyRotationPhase::SHADOWSTEP_POSITIONING;
        }
    }
}

} // namespace Playerbot