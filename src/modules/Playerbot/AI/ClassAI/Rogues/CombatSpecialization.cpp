/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CombatSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"

namespace Playerbot
{

CombatSpecialization::CombatSpecialization(Player* bot)
    : RogueSpecialization(bot), _combatPhase(CombatRotationPhase::OPENER),
      _lastSinisterStrikeTime(0), _lastEviscerateeTime(0), _lastSliceAndDiceTime(0), _lastExposeArmorTime(0),
      _lastRiposteTime(0), _lastAdrenalineRushTime(0), _lastBladeFlurryTime(0), _lastDefensiveAbilityTime(0),
      _preferredComboBuilder(SINISTER_STRIKE), _preferredFinisher(EVISCERATE)
{
    // Initialize combo builders in priority order for Combat
    _comboBuilders = {SINISTER_STRIKE, BACKSTAB, HEMORRHAGE};

    // Initialize finishers in priority order for Combat
    _finishers = {EVISCERATE, SLICE_AND_DICE, EXPOSE_ARMOR, RUPTURE};

    // Initialize defensive abilities
    _defensiveAbilities = {EVASION, SPRINT, GOUGE, KICK, RIPOSTE};

    // Initialize interrupt abilities
    _interruptAbilities = {KICK, GOUGE, KIDNEY_SHOT};

    // Detect weapon types and specializations
    DetectWeaponTypes();

    TC_LOG_DEBUG("playerbot", "CombatSpecialization: Initialized for bot {}", _bot->GetName());
}

void CombatSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Update all management systems
    UpdateResourceStates();
    UpdateTargetInfo(target);
    UpdateWeaponSpecialization();
    UpdateAdrenalineRushManagement();
    UpdateBladeFlurryManagement();
    UpdateRiposteManagement();
    UpdateSliceAndDiceManagement();
    UpdateExposeArmorManagement();
    UpdateComboPointManagement();
    UpdateEnergyManagement();
    UpdateDefensiveAbilities();
    UpdateCombatPhase();
    UpdateCombatMetrics();

    // Execute rotation based on current phase
    switch (_combatPhase)
    {
        case CombatRotationPhase::OPENER:
            ExecuteOpenerPhase(target);
            break;
        case CombatRotationPhase::SLICE_AND_DICE_SETUP:
            ExecuteSliceAndDiceSetup(target);
            break;
        case CombatRotationPhase::SINISTER_STRIKE_SPAM:
            ExecuteSinisterStrikeSpam(target);
            break;
        case CombatRotationPhase::COMBO_SPENDING:
            ExecuteComboSpendingPhase(target);
            break;
        case CombatRotationPhase::ADRENALINE_RUSH_BURST:
            ExecuteAdrenalineRushBurst(target);
            break;
        case CombatRotationPhase::BLADE_FLURRY_AOE:
            ExecuteBladeFlurryAoE(target);
            break;
        case CombatRotationPhase::EXPOSE_ARMOR_DEBUFF:
            ExecuteExposeArmorDebuff(target);
            break;
        case CombatRotationPhase::DEFENSIVE_PHASE:
            ExecuteDefensivePhase(target);
            break;
        case CombatRotationPhase::EXECUTE_PHASE:
            ExecuteExecutePhase(target);
            break;
        case CombatRotationPhase::EMERGENCY:
            ExecuteEmergencyPhase(target);
            break;
    }

    AnalyzeCombatEfficiency();
}

void CombatSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain Slice and Dice
    if (ShouldRefreshSliceAndDice() && GetComboPoints() >= OPTIMAL_SLICE_AND_DICE_COMBO)
    {
        RefreshSliceAndDice();
    }

    // Apply minimal poisons (Combat doesn't focus on poisons)
    if (ShouldApplyPoisons())
        ApplyPoisons();

    // Use weapon-specific abilities if available
    if (HasSpell(COMBAT_EXPERTISE) && !HasAura(COMBAT_EXPERTISE))
    {
        CastSpell(COMBAT_EXPERTISE);
    }
}

void CombatSpecialization::UpdateCooldowns(uint32 diff)
{
    RogueSpecialization::UpdateCooldownTracking(diff);

    // Update Adrenaline Rush timer
    if (_adrenalineRush.isActive)
    {
        if (_adrenalineRush.remainingTime > diff)
            _adrenalineRush.remainingTime -= diff;
        else
        {
            _adrenalineRush.isActive = false;
            _adrenalineRush.remainingTime = 0;
            LogCombatDecision("Adrenaline Rush Ended", "Burst window closed");
        }
    }

    // Update Blade Flurry timer
    if (_bladeFlurry.isActive)
    {
        if (_bladeFlurry.remainingTime > diff)
            _bladeFlurry.remainingTime -= diff;
        else
        {
            _bladeFlurry.isActive = false;
            _bladeFlurry.remainingTime = 0;
            LogCombatDecision("Blade Flurry Ended", "AoE window closed");
        }
    }

    // Update Riposte window
    if (_riposte.lastParry > 0)
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _riposte.lastParry > RIPOSTE_WINDOW)
        {
            _riposte.canRiposte = false;
        }
    }
}

bool CombatSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughEnergyFor(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    // Riposte requires recent parry
    if (spellId == RIPOSTE && !_riposte.canRiposte)
        return false;

    // Behind-target requirements
    if (spellId == BACKSTAB && _currentTarget && !IsBehindTarget(_currentTarget))
        return false;

    // Combo point requirements
    if (spellId == EVISCERATE || spellId == SLICE_AND_DICE || spellId == EXPOSE_ARMOR)
    {
        if (GetComboPoints() == 0)
            return false;
    }

    return true;
}

void CombatSpecialization::OnCombatStart(::Unit* target)
{
    if (!target)
        return;

    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = CombatMetrics();

    // Start with opener phase
    _combatPhase = CombatRotationPhase::OPENER;
    LogCombatDecision("Combat Start", "Beginning Combat rotation");

    // Detect weapon types for this combat
    DetectWeaponTypes();

    // Apply basic poisons if available
    if (ShouldApplyPoisons())
        ApplyPoisons();
}

void CombatSpecialization::OnCombatEnd()
{
    // Log combat statistics
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "CombatSpecialization [{}]: Combat ended. Duration: {}ms, SS casts: {}, Eviscerate: {}, S&D uptime: {:.1f}%",
                _bot->GetName(), combatDuration, _metrics.sinisterStrikeCasts, _metrics.eviscerateCasts, _metrics.sliceAndDiceUptime * 100);

    // Reset phases and timers
    _combatPhase = CombatRotationPhase::OPENER;
    _adrenalineRush.isActive = false;
    _bladeFlurry.isActive = false;
    _riposte.canRiposte = false;
    _currentTarget = nullptr;
}

bool CombatSpecialization::HasEnoughResource(uint32 spellId)
{
    return HasEnoughEnergyFor(spellId);
}

void CombatSpecialization::ConsumeResource(uint32 spellId)
{
    uint32 energyCost = GetEnergyCost(spellId);
    if (energyCost > 0)
    {
        _bot->ModifyPower(POWER_ENERGY, -static_cast<int32>(energyCost));
        _totalEnergySpent += energyCost;
        _metrics.totalEnergyRegenerated += energyCost; // Track for efficiency
    }
}

Position CombatSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    // Combat rogues prefer front/side positioning for Sinister Strike
    float angle = target->GetOrientation() + (M_PI / 4); // 45 degrees to the side
    float distance = 2.5f; // Slightly longer melee range

    float x = target->GetPositionX() + cos(angle) * distance;
    float y = target->GetPositionY() + sin(angle) * distance;
    float z = target->GetPositionZ();

    return Position(x, y, z, angle);
}

float CombatSpecialization::GetOptimalRange(::Unit* target)
{
    // Combat specialization uses standard melee range
    return 2.5f;
}

void CombatSpecialization::UpdateStealthManagement()
{
    // Combat spec has limited stealth usage
    if (!_bot)
        return;

    // Only use stealth pre-combat
    if (ShouldEnterStealth() && !IsStealthed() && _bot->IsOutOfCombat())
    {
        if (IsSpellReady(STEALTH))
        {
            if (CastSpell(STEALTH))
            {
                _lastStealthTime = getMSTime();
                LogCombatDecision("Entered Stealth", "Pre-combat preparation");
            }
        }
    }
}

bool CombatSpecialization::ShouldEnterStealth()
{
    // Combat spec only uses stealth before combat starts
    return _bot->IsOutOfCombat() && !IsStealthed();
}

bool CombatSpecialization::CanBreakStealth()
{
    // Combat can break stealth for any opener
    return true;
}

void CombatSpecialization::ExecuteStealthOpener(::Unit* target)
{
    if (!target || !IsStealthed())
        return;

    // Simple Cheap Shot opener for Combat
    if (HasSpell(CHEAP_SHOT) && CastSpell(CHEAP_SHOT, target))
    {
        LogCombatDecision("Cheap Shot Opener", "Combat stealth opener");
        return;
    }

    // Fallback to breaking stealth with Sinister Strike
    if (ShouldUseSinisterStrike(target))
    {
        ExecuteComboBuilder(target);
    }
}

void CombatSpecialization::UpdateComboPointManagement()
{
    _comboPoints.current = GetComboPoints();
    _comboPoints.shouldSpend = ShouldSpendComboPoints();

    // Update metrics
    if (_comboPoints.current > 0)
    {
        _metrics.totalComboPointsGenerated++;
    }
}

bool CombatSpecialization::ShouldBuildComboPoints()
{
    return GetComboPoints() < 5 && !ShouldSpendComboPoints();
}

bool CombatSpecialization::ShouldSpendComboPoints()
{
    uint8 comboPoints = GetComboPoints();

    // Always spend at 5 combo points
    if (comboPoints >= 5)
        return true;

    // Spend for Slice and Dice refresh
    if (comboPoints >= OPTIMAL_SLICE_AND_DICE_COMBO && ShouldRefreshSliceAndDice())
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

    return false;
}

void CombatSpecialization::ExecuteComboBuilder(::Unit* target)
{
    if (!target)
        return;

    // Riposte takes priority if available
    if (_riposte.canRiposte && HasSpell(RIPOSTE))
    {
        if (CastSpell(RIPOSTE, target))
        {
            _riposte.ripostesExecuted++;
            _riposte.canRiposte = false;
            _metrics.riposteExecutions++;
            LogCombatDecision("Cast Riposte", "Counter-attack after parry");
            return;
        }
    }

    // Sinister Strike is the primary combo builder for Combat
    if (ShouldUseSinisterStrike(target))
    {
        if (CastSpell(SINISTER_STRIKE, target))
        {
            _metrics.sinisterStrikeCasts++;
            _totalCombosBuilt++;
            _lastSinisterStrikeTime = getMSTime();
            LogCombatDecision("Cast Sinister Strike", "Primary combo builder");
            return;
        }
    }

    // Backstab if behind target (less common for Combat)
    if (HasSpell(BACKSTAB) && IsBehindTarget(target))
    {
        if (CastSpell(BACKSTAB, target))
        {
            _totalCombosBuilt++;
            LogCombatDecision("Cast Backstab", "Positional combo builder");
            return;
        }
    }

    // Hemorrhage as alternative
    if (HasSpell(HEMORRHAGE))
    {
        if (CastSpell(HEMORRHAGE, target))
        {
            _totalCombosBuilt++;
            LogCombatDecision("Cast Hemorrhage", "Alternative combo builder");
        }
    }
}

void CombatSpecialization::ExecuteComboSpender(::Unit* target)
{
    if (!target || GetComboPoints() == 0)
        return;

    uint8 comboPoints = GetComboPoints();
    _metrics.totalComboPointsSpent += comboPoints;

    // Slice and Dice has highest priority if missing or low
    if (ShouldPrioritizeSliceAndDice())
    {
        RefreshSliceAndDice();
        return;
    }

    // Expose Armor for armor reduction
    if (ShouldPrioritizeExposeArmor(target))
    {
        ApplyExposeArmor(target);
        return;
    }

    // Eviscerate for damage
    if (ShouldUseEviscerate(target))
    {
        if (CastSpell(EVISCERATE, target))
        {
            _metrics.eviscerateCasts++;
            _lastEviscerateeTime = getMSTime();
            LogCombatDecision("Cast Eviscerate", "Direct damage finisher");
            return;
        }
    }

    // Fallback to any available finisher
    if (HasSpell(SLICE_AND_DICE))
    {
        RefreshSliceAndDice();
    }
}

void CombatSpecialization::UpdatePoisonManagement()
{
    // Combat uses minimal poisons
    uint32 currentTime = getMSTime();
    if (_lastPoisonApplicationTime == 0)
        _lastPoisonApplicationTime = currentTime;

    // Apply poisons less frequently than Assassination
    if (currentTime - _lastPoisonApplicationTime > (POISON_REAPPLY_INTERVAL * 2))
    {
        ApplyPoisons();
        _lastPoisonApplicationTime = currentTime;
    }
}

void CombatSpecialization::ApplyPoisons()
{
    // Combat typically uses Instant Poison on main hand only
    if (HasWeaponInMainHand())
    {
        if (HasSpell(INSTANT_POISON_10))
            CastSpell(INSTANT_POISON_10);
        else if (HasSpell(INSTANT_POISON_9))
            CastSpell(INSTANT_POISON_9);
    }

    LogCombatDecision("Applied Instant Poison", "Basic weapon enhancement");
}

PoisonType CombatSpecialization::GetOptimalMainHandPoison()
{
    // Combat prefers Instant Poison for immediate damage
    if (HasSpell(INSTANT_POISON_10))
        return PoisonType::INSTANT;

    return PoisonType::NONE;
}

PoisonType CombatSpecialization::GetOptimalOffHandPoison()
{
    // Combat rarely uses off-hand poisons
    return PoisonType::NONE;
}

void CombatSpecialization::UpdateDebuffManagement()
{
    if (!_currentTarget)
        return;

    UpdateTargetInfo(_currentTarget);

    // Check Expose Armor
    if (ShouldRefreshExposeArmor(_currentTarget))
        _combatPhase = CombatRotationPhase::EXPOSE_ARMOR_DEBUFF;
}

bool CombatSpecialization::ShouldRefreshDebuff(uint32 spellId)
{
    if (spellId == EXPOSE_ARMOR)
        return ShouldRefreshExposeArmor(_currentTarget);

    return false;
}

void CombatSpecialization::ApplyDebuffs(::Unit* target)
{
    if (ShouldApplyExposeArmor(target))
        ApplyExposeArmor(target);
}

void CombatSpecialization::UpdateEnergyManagement()
{
    RogueSpecialization::UpdateResourceStates();

    // Combat needs steady energy for Sinister Strike spam
    OptimizeEnergyUsage();
}

bool CombatSpecialization::HasEnoughEnergyFor(uint32 spellId)
{
    return RogueSpecialization::HasEnoughEnergyFor(spellId);
}

uint32 CombatSpecialization::GetEnergyCost(uint32 spellId)
{
    return RogueSpecialization::GetEnergyCost(spellId);
}

bool CombatSpecialization::ShouldWaitForEnergy()
{
    // Wait for energy if we're too low and not in emergency
    if (_energy.state == EnergyState::CRITICAL)
    {
        if (_bot && _bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
            return true;
    }

    return false;
}

void CombatSpecialization::UpdateCooldownTracking(uint32 diff)
{
    RogueSpecialization::UpdateCooldownTracking(diff);
}

bool CombatSpecialization::IsSpellReady(uint32 spellId)
{
    return RogueSpecialization::IsSpellReady(spellId);
}

void CombatSpecialization::StartCooldown(uint32 spellId)
{
    RogueSpecialization::StartCooldown(spellId);
}

uint32 CombatSpecialization::GetCooldownRemaining(uint32 spellId)
{
    return RogueSpecialization::GetCooldownRemaining(spellId);
}

void CombatSpecialization::UpdateCombatPhase()
{
    if (!_bot || !_currentTarget)
        return;

    // Emergency phase check
    if (_bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
    {
        _combatPhase = CombatRotationPhase::EMERGENCY;
        return;
    }

    // Execute phase
    if (_currentTarget->GetHealthPct() < (EXECUTE_HEALTH_THRESHOLD * 100))
    {
        _combatPhase = CombatRotationPhase::EXECUTE_PHASE;
        return;
    }

    // Adrenaline Rush burst phase
    if (_adrenalineRush.isActive || ShouldUseAdrenalineRush())
    {
        _combatPhase = CombatRotationPhase::ADRENALINE_RUSH_BURST;
        return;
    }

    // Blade Flurry AoE phase
    if (_bladeFlurry.isActive || (CountNearbyEnemies(_currentTarget) >= AOE_ENEMY_THRESHOLD))
    {
        _combatPhase = CombatRotationPhase::BLADE_FLURRY_AOE;
        return;
    }

    // Slice and Dice setup
    if (ShouldPrioritizeSliceAndDice())
    {
        _combatPhase = CombatRotationPhase::SLICE_AND_DICE_SETUP;
        return;
    }

    // Expose Armor application
    if (ShouldPrioritizeExposeArmor(_currentTarget))
    {
        _combatPhase = CombatRotationPhase::EXPOSE_ARMOR_DEBUFF;
        return;
    }

    // Combo spending
    if (ShouldSpendComboPoints())
    {
        _combatPhase = CombatRotationPhase::COMBO_SPENDING;
        return;
    }

    // Default to Sinister Strike spam
    _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
}

CombatPhase CombatSpecialization::GetCurrentPhase()
{
    switch (_combatPhase)
    {
        case CombatRotationPhase::OPENER:
        case CombatRotationPhase::SLICE_AND_DICE_SETUP:
        case CombatRotationPhase::SINISTER_STRIKE_SPAM:
            return CombatPhase::COMBO_BUILDING;
        case CombatRotationPhase::COMBO_SPENDING:
        case CombatRotationPhase::EXPOSE_ARMOR_DEBUFF:
            return CombatPhase::COMBO_SPENDING;
        case CombatRotationPhase::ADRENALINE_RUSH_BURST:
            return CombatPhase::BURST_PHASE;
        case CombatRotationPhase::BLADE_FLURRY_AOE:
            return CombatPhase::AOE_PHASE;
        case CombatRotationPhase::EXECUTE_PHASE:
            return CombatPhase::EXECUTE_PHASE;
        case CombatRotationPhase::EMERGENCY:
            return CombatPhase::EMERGENCY;
        default:
            return CombatPhase::SUSTAIN_PHASE;
    }
}

bool CombatSpecialization::ShouldExecuteBurstRotation()
{
    return ShouldUseAdrenalineRush() || _adrenalineRush.isActive;
}

// Phase execution methods
void CombatSpecialization::ExecuteOpenerPhase(::Unit* target)
{
    // Simple opener - build to 2 combo points for Slice and Dice
    if (GetComboPoints() < OPTIMAL_SLICE_AND_DICE_COMBO)
    {
        ExecuteComboBuilder(target);
    }
    else
    {
        _combatPhase = CombatRotationPhase::SLICE_AND_DICE_SETUP;
    }
}

void CombatSpecialization::ExecuteSliceAndDiceSetup(::Unit* target)
{
    if (GetComboPoints() >= OPTIMAL_SLICE_AND_DICE_COMBO)
    {
        RefreshSliceAndDice();
        _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
    }
    else
    {
        ExecuteComboBuilder(target);
    }
}

void CombatSpecialization::ExecuteSinisterStrikeSpam(::Unit* target)
{
    if (ShouldBuildComboPoints())
    {
        ExecuteComboBuilder(target);
    }
    else
    {
        _combatPhase = CombatRotationPhase::COMBO_SPENDING;
    }
}

void CombatSpecialization::ExecuteComboSpendingPhase(::Unit* target)
{
    ExecuteComboSpender(target);
    _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
}

void CombatSpecialization::ExecuteAdrenalineRushBurst(::Unit* target)
{
    if (!_adrenalineRush.isActive && ShouldUseAdrenalineRush())
    {
        ActivateAdrenalineRush();
    }

    if (_adrenalineRush.isActive)
    {
        OptimizeAdrenalineRushUsage(target);
    }
    else
    {
        _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
    }
}

void CombatSpecialization::ExecuteBladeFlurryAoE(::Unit* target)
{
    if (!_bladeFlurry.isActive && ShouldUseBladeFlurry())
    {
        ActivateBladeFlurry();
    }

    if (_bladeFlurry.isActive)
    {
        UpdateBladeFlurryAoE(target);
    }
    else
    {
        _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
    }
}

void CombatSpecialization::ExecuteExposeArmorDebuff(::Unit* target)
{
    if (ShouldApplyExposeArmor(target) && GetComboPoints() >= 1)
    {
        ApplyExposeArmor(target);
    }

    _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
}

void CombatSpecialization::ExecuteDefensivePhase(::Unit* target)
{
    HandleDefensiveSituations();

    // Return to normal rotation if health recovers
    if (_bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
        _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
}

void CombatSpecialization::ExecuteExecutePhase(::Unit* target)
{
    // Prioritize high damage finishers
    if (GetComboPoints() >= MIN_COMBO_FOR_EVISCERATE)
    {
        ExecuteComboSpender(target);
    }
    else
    {
        ExecuteComboBuilder(target);
    }
}

void CombatSpecialization::ExecuteEmergencyPhase(::Unit* target)
{
    HandleDefensiveSituations();

    // Try to recover
    if (_bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
        _combatPhase = CombatRotationPhase::SINISTER_STRIKE_SPAM;
}

// Weapon specialization methods
void CombatSpecialization::DetectWeaponTypes()
{
    if (!_bot)
        return;

    _weaponSpec = WeaponSpecialization();

    // Check main hand weapon
    Item* mainhand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (mainhand)
    {
        _weaponSpec.mainHandType = mainhand->GetTemplate()->SubClass;
    }

    // Check off hand weapon
    Item* offhand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (offhand)
    {
        _weaponSpec.offHandType = offhand->GetTemplate()->SubClass;
    }

    // Check for weapon specializations
    _weaponSpec.hasSwordSpec = HasSpell(SWORD_SPECIALIZATION);
    _weaponSpec.hasMaceSpec = HasSpell(MACE_SPECIALIZATION);
    _weaponSpec.hasDaggerSpec = HasSpell(DAGGER_SPECIALIZATION);
    _weaponSpec.hasFistSpec = HasSpell(FIST_WEAPON_SPECIALIZATION);

    TC_LOG_DEBUG("playerbot", "CombatSpecialization [{}]: Detected weapon types - MH: {}, OH: {}",
                _bot->GetName(), _weaponSpec.mainHandType, _weaponSpec.offHandType);
}

void CombatSpecialization::UpdateWeaponSpecializationProcs()
{
    // Track weapon specialization proc rates
    if (_weaponSpec.hasSwordSpec)
        _weaponSpec.swordSpecProc = 0.05f; // 5% proc rate

    if (_weaponSpec.hasMaceSpec)
        _weaponSpec.maceSpecProc = 0.05f; // 5% proc rate

    // Update metrics
    _metrics.weaponSpecializationProcs += _weaponSpec.swordSpecProc + _weaponSpec.maceSpecProc;
}

bool CombatSpecialization::ShouldUseSinisterStrike(::Unit* target)
{
    if (!HasSpell(SINISTER_STRIKE) || !target)
        return false;

    // Sinister Strike is the primary ability for Combat
    return HasEnoughEnergyFor(SINISTER_STRIKE);
}

bool CombatSpecialization::ShouldUseBackstab(::Unit* target)
{
    if (!HasSpell(BACKSTAB) || !target)
        return false;

    // Only use Backstab if behind target and have dagger spec
    return IsBehindTarget(target) && _weaponSpec.hasDaggerSpec;
}

float CombatSpecialization::GetWeaponSpecializationBonus()
{
    float bonus = 1.0f;

    if (_weaponSpec.hasSwordSpec)
        bonus += 0.05f;
    if (_weaponSpec.hasMaceSpec)
        bonus += 0.05f;
    if (_weaponSpec.hasDaggerSpec)
        bonus += 0.05f;
    if (_weaponSpec.hasFistSpec)
        bonus += 0.05f;

    return bonus;
}

// Adrenaline Rush methods
void CombatSpecialization::ActivateAdrenalineRush()
{
    if (CastSpell(ADRENALINE_RUSH))
    {
        _adrenalineRush.isActive = true;
        _adrenalineRush.remainingTime = ADRENALINE_RUSH_DURATION;
        _adrenalineRush.lastActivation = getMSTime();
        _metrics.adrenalineRushActivations++;
        LogCombatDecision("Activated Adrenaline Rush", "Energy regeneration burst");
    }
}

void CombatSpecialization::UpdateAdrenalineRushBurst(::Unit* target)
{
    if (!_adrenalineRush.isActive)
        return;

    // Spam abilities during Adrenaline Rush
    if (GetComboPoints() < 5)
    {
        ExecuteComboBuilder(target);
        _adrenalineRush.abilitiesCastDuringRush++;
    }
    else
    {
        ExecuteComboSpender(target);
        _adrenalineRush.abilitiesCastDuringRush++;
    }
}

bool CombatSpecialization::ShouldUseAdrenalineRush()
{
    if (!HasSpell(ADRENALINE_RUSH) || !IsSpellReady(ADRENALINE_RUSH))
        return false;

    // Use on cooldown for maximum DPS
    return true;
}

void CombatSpecialization::OptimizeAdrenalineRushUsage(::Unit* target)
{
    // During Adrenaline Rush, prioritize ability spam
    if (_energy.state >= EnergyState::MEDIUM)
    {
        if (GetComboPoints() >= 5)
            ExecuteComboSpender(target);
        else
            ExecuteComboBuilder(target);
    }
}

// Blade Flurry methods
void CombatSpecialization::ActivateBladeFlurry()
{
    if (CastSpell(BLADE_FLURRY))
    {
        _bladeFlurry.isActive = true;
        _bladeFlurry.remainingTime = BLADE_FLURRY_DURATION;
        _bladeFlurry.lastActivation = getMSTime();
        _metrics.bladeFlurryActivations++;
        LogCombatDecision("Activated Blade Flurry", "AoE damage window");
    }
}

void CombatSpecialization::UpdateBladeFlurryAoE(::Unit* target)
{
    if (!_bladeFlurry.isActive)
        return;

    // Continue normal rotation but prioritize high-damage abilities
    if (GetComboPoints() >= 4)
        ExecuteComboSpender(target);
    else
        ExecuteComboBuilder(target);
}

bool CombatSpecialization::ShouldUseBladeFlurry()
{
    if (!HasSpell(BLADE_FLURRY) || !IsSpellReady(BLADE_FLURRY))
        return false;

    // Use when there are multiple enemies nearby
    return CountNearbyEnemies(_currentTarget) >= AOE_ENEMY_THRESHOLD;
}

uint32 CombatSpecialization::CountNearbyEnemies(::Unit* target)
{
    if (!target || !_bot)
        return 1;

    // Simplified enemy counting
    // In a real implementation, this would check for nearby hostile units
    return 1; // Default to single target
}

// Riposte methods
void CombatSpecialization::ExecuteRiposte(::Unit* target)
{
    if (CastSpell(RIPOSTE, target))
    {
        _riposte.ripostesExecuted++;
        _metrics.riposteExecutions++;
        LogCombatDecision("Cast Riposte", "Counter-attack");
    }
}

bool CombatSpecialization::CanUseRiposte()
{
    return _riposte.canRiposte && HasSpell(RIPOSTE) && IsSpellReady(RIPOSTE);
}

void CombatSpecialization::UpdateParryTracking()
{
    // This would be called by combat events when a parry occurs
    _riposte.lastParry = getMSTime();
    _riposte.canRiposte = true;
}

// Slice and Dice methods
void CombatSpecialization::RefreshSliceAndDice()
{
    if (CastSpell(SLICE_AND_DICE))
    {
        _metrics.sliceAndDiceApplications++;
        _lastSliceAndDiceTime = getMSTime();
        LogCombatDecision("Cast Slice and Dice", "Attack speed buff");
    }
}

bool CombatSpecialization::ShouldRefreshSliceAndDice()
{
    uint32 remaining = GetSliceAndDiceTimeRemaining();
    uint32 duration = 21000; // Base 21 seconds

    return remaining < (duration * SLICE_AND_DICE_REFRESH_THRESHOLD);
}

uint32 CombatSpecialization::GetSliceAndDiceTimeRemaining()
{
    return GetAuraTimeRemaining(SLICE_AND_DICE);
}

uint8 CombatSpecialization::GetOptimalSliceAndDiceComboPoints()
{
    return OPTIMAL_SLICE_AND_DICE_COMBO;
}

// Expose Armor methods
void CombatSpecialization::ApplyExposeArmor(::Unit* target)
{
    if (CastSpell(EXPOSE_ARMOR, target))
    {
        _metrics.exposeArmorApplications++;
        _lastExposeArmorTime = getMSTime();
        LogCombatDecision("Cast Expose Armor", "Armor reduction debuff");
    }
}

bool CombatSpecialization::ShouldApplyExposeArmor(::Unit* target)
{
    if (!HasSpell(EXPOSE_ARMOR) || !target)
        return false;

    return !HasAura(EXPOSE_ARMOR, target) && GetComboPoints() >= 1;
}

bool CombatSpecialization::ShouldRefreshExposeArmor(::Unit* target)
{
    if (!target)
        return false;

    uint32 remaining = GetExposeArmorTimeRemaining(target);
    uint32 duration = 30000; // 30 seconds base

    return remaining < (duration * EXPOSE_ARMOR_REFRESH_THRESHOLD);
}

uint32 CombatSpecialization::GetExposeArmorTimeRemaining(::Unit* target)
{
    return GetAuraTimeRemaining(EXPOSE_ARMOR, target);
}

// Optimization methods
bool CombatSpecialization::ShouldUseEviscerate(::Unit* target)
{
    return HasSpell(EVISCERATE) && GetComboPoints() >= MIN_COMBO_FOR_EVISCERATE;
}

bool CombatSpecialization::ShouldPrioritizeSliceAndDice()
{
    return !HasAura(SLICE_AND_DICE) || ShouldRefreshSliceAndDice();
}

bool CombatSpecialization::ShouldPrioritizeExposeArmor(::Unit* target)
{
    if (!target)
        return false;

    return ShouldApplyExposeArmor(target) || ShouldRefreshExposeArmor(target);
}

// Defensive methods
void CombatSpecialization::HandleDefensiveSituations()
{
    if (_bot->GetHealthPct() < 30.0f)
    {
        ExecuteEvasion();
    }
    else if (_bot->GetHealthPct() < 50.0f)
    {
        ExecuteSprint(); // For mobility
    }

    // Interrupt dangerous casts
    if (_currentTarget && ShouldUseDefensiveAbility())
    {
        ExecuteKick(_currentTarget);
    }
}

void CombatSpecialization::ExecuteEvasion()
{
    if (CastSpell(EVASION))
    {
        LogCombatDecision("Activated Evasion", "Emergency defense");
    }
}

void CombatSpecialization::ExecuteSprint()
{
    if (CastSpell(SPRINT))
    {
        LogCombatDecision("Activated Sprint", "Mobility enhancement");
    }
}

void CombatSpecialization::ExecuteGouge(::Unit* target)
{
    if (CastSpell(GOUGE, target))
    {
        LogCombatDecision("Cast Gouge", "Incapacitate enemy");
    }
}

void CombatSpecialization::ExecuteKick(::Unit* target)
{
    if (CastSpell(KICK, target))
    {
        LogCombatDecision("Cast Kick", "Interrupt enemy cast");
    }
}

bool CombatSpecialization::ShouldUseDefensiveAbility()
{
    return _bot->GetHealthPct() < 60.0f;
}

// Energy optimization
void CombatSpecialization::OptimizeEnergyUsage()
{
    // Combat spec needs steady energy for Sinister Strike
    if (_energy.state == EnergyState::CRITICAL)
    {
        LogCombatDecision("Energy Critical", "Waiting for regeneration");
    }
}

void CombatSpecialization::PrioritizeEnergySpending(::Unit* target)
{
    // Prioritize abilities based on energy efficiency
    if (_energy.state >= EnergyState::HIGH)
    {
        if (GetComboPoints() >= 5)
            ExecuteComboSpender(target);
        else
            ExecuteComboBuilder(target);
    }
}

bool CombatSpecialization::ShouldDelayAbilityForEnergy(uint32 spellId)
{
    uint32 cost = GetEnergyCost(spellId);
    return (_energy.current < cost) && (_energy.state == EnergyState::CRITICAL);
}

// Update methods
void CombatSpecialization::UpdateWeaponSpecialization()
{
    UpdateWeaponSpecializationProcs();
}

void CombatSpecialization::UpdateAdrenalineRushManagement()
{
    // Update Adrenaline Rush state
    if (_adrenalineRush.isActive)
    {
        _metrics.adrenalineRushUptime += 1.0f;
    }
}

void CombatSpecialization::UpdateBladeFlurryManagement()
{
    // Update Blade Flurry state
    if (_bladeFlurry.isActive)
    {
        _metrics.bladeFlurryUptime += 1.0f;
    }
}

void CombatSpecialization::UpdateRiposteManagement()
{
    // Check if riposte window has expired
    if (_riposte.canRiposte)
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _riposte.lastParry > RIPOSTE_WINDOW)
        {
            _riposte.canRiposte = false;
        }
    }
}

void CombatSpecialization::UpdateSliceAndDiceManagement()
{
    // Update Slice and Dice uptime tracking
    if (HasAura(SLICE_AND_DICE))
    {
        _metrics.sliceAndDiceUptime += 1.0f;
    }
}

void CombatSpecialization::UpdateExposeArmorManagement()
{
    // Update Expose Armor uptime tracking
    if (_currentTarget && HasAura(EXPOSE_ARMOR, _currentTarget))
    {
        _metrics.exposeArmorUptime += 1.0f;
    }
}

void CombatSpecialization::UpdateDefensiveAbilities()
{
    // Check if defensive abilities are needed
    if (ShouldUseDefensiveAbility())
    {
        _combatPhase = CombatRotationPhase::DEFENSIVE_PHASE;
    }
}

void CombatSpecialization::UpdateCombatMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate efficiency metrics
        _metrics.averageEnergyEfficiency = static_cast<float>(_totalEnergySpent) / combatTime;

        // Normalize uptime percentages
        float timePercent = combatTime / 1000.0f;
        if (timePercent > 0)
        {
            _metrics.sliceAndDiceUptime /= timePercent;
            _metrics.exposeArmorUptime /= timePercent;
            _metrics.adrenalineRushUptime /= timePercent;
            _metrics.bladeFlurryUptime /= timePercent;
        }
    }
}

void CombatSpecialization::AnalyzeCombatEfficiency()
{
    // Performance analysis every 15 seconds
    if (getMSTime() % 15000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "CombatSpecialization [{}]: Efficiency - S&D: {:.1f}%, AR: {:.1f}%, Ripostes: {}",
                    _bot->GetName(), _metrics.sliceAndDiceUptime * 100, _metrics.adrenalineRushUptime * 100, _metrics.riposteExecutions);
    }
}

void CombatSpecialization::LogCombatDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

void CombatSpecialization::OptimizeCombatRotation(::Unit* target)
{
    // Overall rotation optimization
    PrioritizeEnergySpending(target);
}

void CombatSpecialization::OptimizeCombatPositioning(::Unit* target)
{
    // Combat rogues maintain standard melee range
    if (!IsInMeleeRange(target))
    {
        // Move to optimal position
        Position optimalPos = GetOptimalPosition(target);
        // Movement would be handled by movement system
    }
}

void CombatSpecialization::MaintainMeleeRange(::Unit* target)
{
    // Ensure we stay in melee range
    if (target && !IsInMeleeRange(target))
    {
        LogCombatDecision("Moving to Melee Range", "Maintaining combat distance");
    }
}

bool CombatSpecialization::ShouldRepositionForAdvantage(::Unit* target)
{
    // Combat doesn't need positional advantages as much as other specs
    return false;
}

// Implementation of pure virtual methods from RogueSpecialization base class
bool CombatSpecialization::CastSpell(uint32 spellId, ::Unit* target)
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
    if (spellId == SINISTER_STRIKE)
        _metrics.sinisterStrikeCasts++;
    else if (spellId == BLADE_FLURRY)
        _metrics.bladeFlurryUses++;
    else if (spellId == ADRENALINE_RUSH)
        _metrics.adrenalineRushUses++;
    else if (spellId == RIPOSTE)
        _metrics.riposteCasts++;
    else if (spellId == KILLING_SPREE)
        _metrics.killingSpreeUses++;

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

bool CombatSpecialization::HasSpell(uint32 spellId)
{
    if (!_bot)
        return false;

    return _bot->HasSpell(spellId);
}

SpellInfo const* CombatSpecialization::GetSpellInfo(uint32 spellId)
{
    if (!_bot)
        return nullptr;

    return sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());
}

uint32 CombatSpecialization::GetSpellCooldown(uint32 spellId)
{
    if (!_bot)
        return 0;

    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    // Check if spell has an active cooldown
    SpellCooldowns const& cooldowns = _bot->GetSpellCooldownMap();
    auto itr = cooldowns.find(spellId);
    if (itr != cooldowns.end())
    {
        uint32 currentTime = getMSTime();
        if (itr->second.end > currentTime)
        {
            return itr->second.end - currentTime;
        }
    }

    return 0;
}

} // namespace Playerbot