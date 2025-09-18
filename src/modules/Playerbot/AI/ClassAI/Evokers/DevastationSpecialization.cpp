/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DevastationSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"

namespace Playerbot
{

DevastationSpecialization::DevastationSpecialization(Player* bot)
    : EvokerSpecialization(bot), _devastationPhase(DevastationRotationPhase::OPENER),
      _lastAzureStrikeTime(0), _lastLivingFlameTime(0), _lastDisintegrateTime(0), _lastPyreTime(0),
      _lastFireBreathTime(0), _lastEternitysSurgeTime(0), _lastShatteringStarTime(0), _lastDragonrageTime(0),
      _lastDeepBreathTime(0), _lastFirestormTime(0), _prioritizeEmpowerment(true), _conserveEssenceForBurst(false),
      _useAggressivePositioning(true), _preferredEmpowermentLevel(3), _preferredAoEThreshold(3)
{
    // Initialize essence generators in priority order
    _essenceGenerators = {AZURE_STRIKE, LIVING_FLAME, DISINTEGRATE};

    // Initialize empowered abilities
    _empoweredAbilities = {FIRE_BREATH_EMPOWERED, ETERNITYS_SURGE_EMPOWERED};

    // Initialize burst abilities
    _burstAbilities = {DRAGONRAGE, SHATTERING_STAR, DEEP_BREATH, FIRESTORM};

    // Initialize AoE abilities
    _aoeAbilities = {PYRE, FIRE_BREATH_EMPOWERED, DEEP_BREATH, FIRESTORM};

    // Initialize filler abilities
    _fillerAbilities = {LIVING_FLAME, AZURE_STRIKE, DISINTEGRATE};

    TC_LOG_DEBUG("playerbot", "DevastationSpecialization: Initialized for bot {}", _bot->GetName());
}

void DevastationSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Update all management systems
    UpdateResourceStates();
    UpdateTargetInfo(target);
    UpdateBurnoutManagement();
    UpdateEssenceBurstTracking();
    UpdateDragonrageManagement();
    UpdateIridescenceTracking();
    UpdateShatteringStarWindow();
    UpdateAoETargeting();
    UpdateEssenceManagement();
    UpdateEmpowermentSystem();
    UpdateAspectManagement();
    UpdateCombatPhase();
    UpdateDevastationMetrics();

    // Execute rotation based on current phase
    switch (_devastationPhase)
    {
        case DevastationRotationPhase::OPENER:
            ExecuteOpenerPhase(target);
            break;
        case DevastationRotationPhase::ESSENCE_GENERATION:
            ExecuteEssenceGenerationPhase(target);
            break;
        case DevastationRotationPhase::EMPOWERMENT_WINDOW:
            ExecuteEmpowermentWindow(target);
            break;
        case DevastationRotationPhase::BURNOUT_MANAGEMENT:
            ExecuteBurnoutManagement(target);
            break;
        case DevastationRotationPhase::DRAGONRAGE_BURST:
            ExecuteDragonrageBurst(target);
            break;
        case DevastationRotationPhase::SHATTERING_STAR_WINDOW:
            ExecuteShatteringStarWindow(target);
            break;
        case DevastationRotationPhase::DEEP_BREATH_SETUP:
            ExecuteDeepBreathSetup(target);
            break;
        case DevastationRotationPhase::AOE_PHASE:
            ExecuteAoEPhase(target);
            break;
        case DevastationRotationPhase::EXECUTE_PHASE:
            ExecuteExecutePhase(target);
            break;
        case DevastationRotationPhase::EMERGENCY:
            ExecuteEmergencyPhase(target);
            break;
    }

    ManageMajorCooldowns();
    AnalyzeRotationEfficiency();
}

void DevastationSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain optimal aspect
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _aspect.current && CanShiftAspect())
    {
        ShiftToAspect(optimalAspect);
    }

    // Use Hover for positioning if needed
    if (ShouldUseHover() && !HasAura(HOVER))
    {
        CastSpell(HOVER);
    }
}

void DevastationSpecialization::UpdateCooldowns(uint32 diff)
{
    EvokerSpecialization::UpdateResourceStates();

    // Update Dragonrage timer
    if (_dragonrage.isActive)
    {
        if (_dragonrage.remainingTime > diff)
            _dragonrage.remainingTime -= diff;
        else
        {
            _dragonrage.isActive = false;
            _dragonrage.remainingTime = 0;
            LogDevastationDecision("Dragonrage Ended", "Burst window closed");
        }
    }

    // Update Burnout stacks
    if (_burnout.isActive)
    {
        if (_burnout.timeRemaining > diff)
            _burnout.timeRemaining -= diff;
        else
        {
            _burnout.isActive = false;
            _burnout.stacks = 0;
            _burnout.timeRemaining = 0;
        }
    }

    // Update Essence Burst
    if (_essenceBurst.isActive)
    {
        if (_essenceBurst.timeRemaining > diff)
            _essenceBurst.timeRemaining -= diff;
        else
        {
            _essenceBurst.isActive = false;
            _essenceBurst.stacks = 0;
            _essenceBurst.timeRemaining = 0;
        }
    }

    // Update Iridescence buffs
    if (_iridescence.hasBlue)
    {
        if (_iridescence.blueTimeRemaining > diff)
            _iridescence.blueTimeRemaining -= diff;
        else
            _iridescence.hasBlue = false;
    }

    if (_iridescence.hasRed)
    {
        if (_iridescence.redTimeRemaining > diff)
            _iridescence.redTimeRemaining -= diff;
        else
            _iridescence.hasRed = false;
    }
}

bool DevastationSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check if we're channeling an empowered spell
    if (IsChannelingEmpoweredSpell() && spellId != _currentEmpoweredSpell.spellId)
        return false;

    // Dragonrage restrictions
    if (spellId == DRAGONRAGE && _dragonrage.isActive)
        return false;

    // Burnout management
    if (ShouldAvoidBurnout() && (spellId == PYRE || spellId == DISINTEGRATE || spellId == FIRE_BREATH_EMPOWERED))
        return false;

    return true;
}

void DevastationSpecialization::OnCombatStart(::Unit* target)
{
    if (!target)
        return;

    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = DevastationMetrics();

    // Start with opener phase
    _devastationPhase = DevastationRotationPhase::OPENER;
    LogDevastationDecision("Combat Start", "Beginning Devastation rotation");

    // Ensure we have optimal aspect
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _aspect.current && CanShiftAspect())
    {
        ShiftToAspect(optimalAspect);
    }
}

void DevastationSpecialization::OnCombatEnd()
{
    // Log combat statistics
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "DevastationSpecialization [{}]: Combat ended. Duration: {}ms, DPS: {:.1f}, Empowered: {}",
                _bot->GetName(), combatDuration, _metrics.averageDamagePerSecond, _metrics.empoweredSpellsCast);

    // Reset phases and state
    _devastationPhase = DevastationRotationPhase::OPENER;
    _dragonrage.isActive = false;
    _burnout.isActive = false;
    _essenceBurst.isActive = false;
    _currentTarget = nullptr;
}

bool DevastationSpecialization::HasEnoughResource(uint32 spellId)
{
    uint32 essenceCost = GetEssenceCost(spellId);
    return GetEssence() >= essenceCost;
}

void DevastationSpecialization::ConsumeResource(uint32 spellId)
{
    uint32 essenceCost = GetEssenceCost(spellId);
    if (essenceCost > 0)
    {
        SpendEssence(essenceCost);
    }
}

Position DevastationSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    // Devastation prefers medium range for casting
    float angle = target->GetOrientation() + (M_PI / 2); // 90 degrees to the side
    float distance = OPTIMAL_CASTING_RANGE;

    float x = target->GetPositionX() + cos(angle) * distance;
    float y = target->GetPositionY() + sin(angle) * distance;
    float z = target->GetPositionZ();

    return Position(x, y, z, angle);
}

float DevastationSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

// Resource Management Implementation
void DevastationSpecialization::UpdateEssenceManagement()
{
    EvokerSpecialization::UpdateEssenceManagement();

    // Devastation-specific essence optimization
    OptimizeEssenceSpending();
}

bool DevastationSpecialization::HasEssence(uint32 required)
{
    return EvokerSpecialization::HasEssence(required);
}

uint32 DevastationSpecialization::GetEssence()
{
    return EvokerSpecialization::GetEssence();
}

void DevastationSpecialization::SpendEssence(uint32 amount)
{
    EvokerSpecialization::SpendEssence(amount);
}

void DevastationSpecialization::GenerateEssence(uint32 amount)
{
    EvokerSpecialization::GenerateEssence(amount);
}

bool DevastationSpecialization::ShouldConserveEssence()
{
    // Conserve essence for burst windows
    if (_conserveEssenceForBurst && _dragonrage.isActive)
        return false;

    // Conserve if major cooldowns are coming up soon
    if (_devastationPhase == DevastationRotationPhase::DRAGONRAGE_BURST)
        return true;

    return EvokerSpecialization::ShouldConserveEssence();
}

// Empowerment Management Implementation
void DevastationSpecialization::UpdateEmpowermentSystem()
{
    EvokerSpecialization::UpdateEmpowermentSystem();
    OptimizeEmpoweredSpellUsage(_currentTarget);
}

void DevastationSpecialization::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target)
{
    EvokerSpecialization::StartEmpoweredSpell(spellId, targetLevel, target);
    _metrics.empoweredSpellsCast++;
}

void DevastationSpecialization::UpdateEmpoweredChanneling()
{
    EvokerSpecialization::UpdateEmpoweredChanneling();
}

void DevastationSpecialization::ReleaseEmpoweredSpell()
{
    EvokerSpecialization::ReleaseEmpoweredSpell();
}

EmpowermentLevel DevastationSpecialization::CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target)
{
    if (!target)
        return EmpowermentLevel::RANK_1;

    // Calculate based on target count and essence availability
    uint32 targetCount = 1;
    if (spellId == FIRE_BREATH_EMPOWERED)
    {
        targetCount = CountNearbyEnemies(target, 8.0f);
    }

    // Base empowerment level
    EmpowermentLevel level = static_cast<EmpowermentLevel>(_preferredEmpowermentLevel);

    // Adjust based on essence state
    if (_essence.state <= EssenceState::LOW)
        level = EmpowermentLevel::RANK_1;
    else if (_essence.state == EssenceState::MEDIUM)
        level = EmpowermentLevel::RANK_2;

    // Boost for burst phases
    if (_dragonrage.isActive || _devastationPhase == DevastationRotationPhase::DRAGONRAGE_BURST)
        level = EmpowermentLevel::RANK_4;

    // Adjust for target count
    if (targetCount >= 3)
        level = static_cast<EmpowermentLevel>(std::min(4, static_cast<int>(level) + 1));

    return level;
}

bool DevastationSpecialization::ShouldEmpowerSpell(uint32 spellId)
{
    if (!_prioritizeEmpowerment)
        return false;

    // Don't empower if low on essence unless in burst
    if (_essence.state <= EssenceState::LOW && !_dragonrage.isActive)
        return false;

    // Always empower during burst windows
    if (_dragonrage.isActive || _devastationPhase == DevastationRotationPhase::EMPOWERMENT_WINDOW)
        return true;

    // Empower based on efficiency
    float efficiency = CalculateEmpowermentEfficiency(spellId, static_cast<EmpowermentLevel>(_preferredEmpowermentLevel), _currentTarget);
    return efficiency >= EMPOWERMENT_EFFICIENCY_THRESHOLD;
}

// Aspect Management Implementation
void DevastationSpecialization::UpdateAspectManagement()
{
    EvokerSpecialization::UpdateAspectManagement();
}

void DevastationSpecialization::ShiftToAspect(EvokerAspect aspect)
{
    EvokerSpecialization::ShiftToAspect(aspect);
}

EvokerAspect DevastationSpecialization::GetOptimalAspect()
{
    // Red aspect for damage in most situations
    if (_dragonrage.isActive || _devastationPhase == DevastationRotationPhase::DRAGONRAGE_BURST)
        return EvokerAspect::RED;

    // Azure aspect for essence management
    if (_essence.state <= EssenceState::LOW)
        return EvokerAspect::AZURE;

    // Default to Red for damage
    return EvokerAspect::RED;
}

bool DevastationSpecialization::CanShiftAspect()
{
    return EvokerSpecialization::CanShiftAspect();
}

// Combat Phase Management Implementation
void DevastationSpecialization::UpdateCombatPhase()
{
    if (!_bot || !_currentTarget)
        return;

    // Emergency phase check
    if (_bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
    {
        _devastationPhase = DevastationRotationPhase::EMERGENCY;
        return;
    }

    // Execute phase
    if (_currentTarget->GetHealthPct() < (EXECUTE_HEALTH_THRESHOLD * 100))
    {
        _devastationPhase = DevastationRotationPhase::EXECUTE_PHASE;
        return;
    }

    // Dragonrage burst phase
    if (_dragonrage.isActive || ShouldActivateDragonrage())
    {
        _devastationPhase = DevastationRotationPhase::DRAGONRAGE_BURST;
        return;
    }

    // AoE phase
    if (ShouldUseAoERotation())
    {
        _devastationPhase = DevastationRotationPhase::AOE_PHASE;
        return;
    }

    // Empowerment window
    if (ShouldPrioritizeEmpowerment())
    {
        _devastationPhase = DevastationRotationPhase::EMPOWERMENT_WINDOW;
        return;
    }

    // Shattering Star window
    if (HasAura(SHATTERING_STAR, _currentTarget))
    {
        _devastationPhase = DevastationRotationPhase::SHATTERING_STAR_WINDOW;
        return;
    }

    // Burnout management
    if (_burnout.stacks >= SAFE_BURNOUT_STACKS)
    {
        _devastationPhase = DevastationRotationPhase::BURNOUT_MANAGEMENT;
        return;
    }

    // Essence generation
    if (_essence.state <= EssenceState::MEDIUM)
    {
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
        return;
    }

    // Default to empowerment window
    _devastationPhase = DevastationRotationPhase::EMPOWERMENT_WINDOW;
}

CombatPhase DevastationSpecialization::GetCurrentPhase()
{
    switch (_devastationPhase)
    {
        case DevastationRotationPhase::OPENER:
            return CombatPhase::OPENER;
        case DevastationRotationPhase::DRAGONRAGE_BURST:
        case DevastationRotationPhase::SHATTERING_STAR_WINDOW:
            return CombatPhase::BURST_PHASE;
        case DevastationRotationPhase::EMPOWERMENT_WINDOW:
            return CombatPhase::EMPOWERMENT_WINDOW;
        case DevastationRotationPhase::AOE_PHASE:
            return CombatPhase::AOE_PHASE;
        case DevastationRotationPhase::EXECUTE_PHASE:
            return CombatPhase::EXECUTE_PHASE;
        case DevastationRotationPhase::EMERGENCY:
            return CombatPhase::EMERGENCY;
        default:
            return CombatPhase::SUSTAIN_PHASE;
    }
}

bool DevastationSpecialization::ShouldExecuteBurstRotation()
{
    return _dragonrage.isActive || ShouldActivateDragonrage() ||
           HasAura(SHATTERING_STAR, _currentTarget);
}

// Target Selection Implementation
::Unit* DevastationSpecialization::GetBestTarget()
{
    if (!_bot)
        return nullptr;

    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    if (enemies.empty())
        return nullptr;

    ::Unit* bestTarget = nullptr;
    float bestPriority = 0.0f;

    for (::Unit* enemy : enemies)
    {
        if (!IsValidTarget(enemy))
            continue;

        float priority = CalculateTargetPriority(enemy);

        // Boost priority for targets with Shattering Star
        if (HasAura(SHATTERING_STAR, enemy))
            priority += 2.0f;

        // Boost priority for low health targets
        if (enemy->GetHealthPct() < 35.0f)
            priority += 1.0f;

        if (priority > bestPriority)
        {
            bestPriority = priority;
            bestTarget = enemy;
        }
    }

    return bestTarget;
}

std::vector<::Unit*> DevastationSpecialization::GetEmpoweredSpellTargets(uint32 spellId)
{
    std::vector<::Unit*> targets;

    if (!_currentTarget)
        return targets;

    // Primary target
    targets.push_back(_currentTarget);

    // Additional targets for AoE empowered spells
    if (spellId == FIRE_BREATH_EMPOWERED)
    {
        std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(8.0f);
        for (::Unit* enemy : nearbyEnemies)
        {
            if (enemy != _currentTarget && IsValidTarget(enemy))
            {
                targets.push_back(enemy);
            }
        }
    }

    return targets;
}

// Phase execution methods
void DevastationSpecialization::ExecuteOpenerPhase(::Unit* target)
{
    // Use Living Flame as opener for essence generation
    if (HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastLivingFlame(target);
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
    }
}

void DevastationSpecialization::ExecuteEssenceGenerationPhase(::Unit* target)
{
    // Prioritize essence generators
    if (HasSpell(AZURE_STRIKE) && HasEssence(2))
    {
        CastAzureStrike(target);
    }
    else if (HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastLivingFlame(target);
    }

    // Transition when we have enough essence
    if (_essence.state >= EssenceState::HIGH)
    {
        _devastationPhase = DevastationRotationPhase::EMPOWERMENT_WINDOW;
    }
}

void DevastationSpecialization::ExecuteEmpowermentWindow(::Unit* target)
{
    if (ShouldUseEmpoweredFireBreath(target))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(FIRE_BREATH_EMPOWERED, target);
        CastEmpoweredFireBreath(target, level);
    }
    else if (ShouldUseEmpoweredEternitysSurge(target))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(ETERNITYS_SURGE_EMPOWERED, target);
        CastEmpoweredEternitysSurge(target, level);
    }
    else
    {
        // Fall back to essence generation
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
    }
}

void DevastationSpecialization::ExecuteBurnoutManagement(::Unit* target)
{
    ManageBurnoutStacks();

    // Use abilities that don't generate burnout
    if (HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastLivingFlame(target);
    }
    else if (HasSpell(AZURE_STRIKE) && HasEssence(2))
    {
        CastAzureStrike(target);
    }

    // Transition when burnout is manageable
    if (_burnout.stacks < SAFE_BURNOUT_STACKS)
    {
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
    }
}

void DevastationSpecialization::ExecuteDragonrageBurst(::Unit* target)
{
    if (!_dragonrage.isActive && ShouldActivateDragonrage())
    {
        ActivateDragonrage();
    }

    if (_dragonrage.isActive)
    {
        ExecuteDragonrageRotation(target);
    }
    else
    {
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
    }
}

void DevastationSpecialization::ExecuteShatteringStarWindow(::Unit* target)
{
    // Maximize damage during Shattering Star window
    if (ShouldUseEmpoweredEternitysSurge(target))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(ETERNITYS_SURGE_EMPOWERED, target);
        CastEmpoweredEternitysSurge(target, level);
    }
    else if (HasSpell(DISINTEGRATE) && HasEssence(3))
    {
        CastDisintegrate(target);
    }
    else if (HasSpell(PYRE) && HasEssence(3))
    {
        CastPyre(target);
    }

    // Transition when Shattering Star expires
    if (!HasAura(SHATTERING_STAR, target))
    {
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
    }
}

void DevastationSpecialization::ExecuteDeepBreathSetup(::Unit* target)
{
    if (HasSpell(DEEP_BREATH) && HasEssence(4))
    {
        CastDeepBreath(target);
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
    }
}

void DevastationSpecialization::ExecuteAoEPhase(::Unit* target)
{
    UpdateAoERotation(target);
}

void DevastationSpecialization::ExecuteExecutePhase(::Unit* target)
{
    // Prioritize high damage abilities in execute phase
    if (ShouldUseEmpoweredEternitysSurge(target))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(ETERNITYS_SURGE_EMPOWERED, target);
        CastEmpoweredEternitysSurge(target, level);
    }
    else if (HasSpell(DISINTEGRATE) && HasEssence(3))
    {
        CastDisintegrate(target);
    }
    else if (HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastLivingFlame(target);
    }
}

void DevastationSpecialization::ExecuteEmergencyPhase(::Unit* target)
{
    UseEmergencyAbilities();

    // Try to recover
    if (_bot->GetHealthPct() > EMERGENCY_HEALTH_THRESHOLD)
        _devastationPhase = DevastationRotationPhase::ESSENCE_GENERATION;
}

// Core ability implementations
void DevastationSpecialization::CastAzureStrike(::Unit* target)
{
    if (CastSpell(AZURE_STRIKE, target))
    {
        _metrics.azureStrikeCasts++;
        _lastAzureStrikeTime = getMSTime();
        LogDevastationDecision("Cast Azure Strike", "Essence generation");
    }
}

void DevastationSpecialization::CastLivingFlame(::Unit* target)
{
    if (CastSpell(LIVING_FLAME, target))
    {
        _metrics.livingFlameCasts++;
        _lastLivingFlameTime = getMSTime();
        LogDevastationDecision("Cast Living Flame", "Versatile damage/heal");
    }
}

void DevastationSpecialization::CastDisintegrate(::Unit* target)
{
    if (CastSpell(DISINTEGRATE, target))
    {
        _metrics.disintegrateCasts++;
        _lastDisintegrateTime = getMSTime();
        LogDevastationDecision("Cast Disintegrate", "High damage ability");
    }
}

void DevastationSpecialization::CastPyre(::Unit* target)
{
    if (CastSpell(PYRE, target))
    {
        _metrics.pyreCasts++;
        _lastPyreTime = getMSTime();
        LogDevastationDecision("Cast Pyre", "AoE damage");
    }
}

void DevastationSpecialization::CastEmpoweredFireBreath(::Unit* target, EmpowermentLevel level)
{
    if (ShouldEmpowerSpell(FIRE_BREATH_EMPOWERED))
    {
        StartEmpoweredSpell(FIRE_BREATH_EMPOWERED, level, target);
        _metrics.fireBreathCasts++;
        _lastFireBreathTime = getMSTime();
        LogDevastationDecision("Started Empowered Fire Breath", "Level " + std::to_string(static_cast<uint8>(level)));
    }
}

void DevastationSpecialization::CastEmpoweredEternitysSurge(::Unit* target, EmpowermentLevel level)
{
    if (ShouldEmpowerSpell(ETERNITYS_SURGE_EMPOWERED))
    {
        StartEmpoweredSpell(ETERNITYS_SURGE_EMPOWERED, level, target);
        _metrics.eternitysSurgeCasts++;
        _lastEternitysSurgeTime = getMSTime();
        LogDevastationDecision("Started Empowered Eternity's Surge", "Level " + std::to_string(static_cast<uint8>(level)));
    }
}

void DevastationSpecialization::CastShatteringStar(::Unit* target)
{
    if (CastSpell(SHATTERING_STAR, target))
    {
        _metrics.shatteringStarCasts++;
        _lastShatteringStarTime = getMSTime();
        LogDevastationDecision("Cast Shattering Star", "Damage vulnerability window");
    }
}

void DevastationSpecialization::ActivateDragonrage()
{
    if (CastSpell(DRAGONRAGE))
    {
        _dragonrage.isActive = true;
        _dragonrage.remainingTime = DRAGONRAGE_DURATION;
        _dragonrage.lastActivation = getMSTime();
        _metrics.dragonrageActivations++;
        LogDevastationDecision("Activated Dragonrage", "Major damage burst window");
    }
}

void DevastationSpecialization::ExecuteDragonrageRotation(::Unit* target)
{
    if (!_dragonrage.isActive)
        return;

    // Spam high damage abilities during Dragonrage
    if (ShouldUseEmpoweredEternitysSurge(target))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(ETERNITYS_SURGE_EMPOWERED, target);
        CastEmpoweredEternitysSurge(target, level);
    }
    else if (HasSpell(DISINTEGRATE) && HasEssence(3))
    {
        CastDisintegrate(target);
    }
    else if (HasSpell(PYRE) && HasEssence(3))
    {
        CastPyre(target);
    }
    else if (HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastLivingFlame(target);
    }

    _dragonrage.abilitiesUsedDuringRage++;
}

bool DevastationSpecialization::ShouldActivateDragonrage()
{
    if (!HasSpell(DRAGONRAGE))
        return false;

    // Use on cooldown for maximum DPS
    return _essence.state >= EssenceState::HIGH;
}

// Helper methods
bool DevastationSpecialization::ShouldUseEmpoweredFireBreath(::Unit* target)
{
    if (!HasSpell(FIRE_BREATH_EMPOWERED) || !target)
        return false;

    // Use for AoE situations
    uint32 targetCount = CountNearbyEnemies(target, 8.0f);
    return targetCount >= 2 && HasEssence(3);
}

bool DevastationSpecialization::ShouldUseEmpoweredEternitysSurge(::Unit* target)
{
    if (!HasSpell(ETERNITYS_SURGE_EMPOWERED) || !target)
        return false;

    return HasEssence(3) && _essence.state >= EssenceState::MEDIUM;
}

bool DevastationSpecialization::ShouldUseAoERotation()
{
    return CountNearbyEnemies(_currentTarget, 8.0f) >= AOE_ENEMY_THRESHOLD;
}

uint32 DevastationSpecialization::CountNearbyEnemies(::Unit* target, float range)
{
    if (!target)
        return 0;

    std::vector<::Unit*> enemies = GetNearbyEnemies(range);
    return enemies.size();
}

void DevastationSpecialization::UpdateAoERotation(::Unit* target)
{
    // Use AoE abilities when multiple enemies are present
    if (HasSpell(PYRE) && HasEssence(3))
    {
        CastPyre(target);
    }
    else if (ShouldUseEmpoweredFireBreath(target))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(FIRE_BREATH_EMPOWERED, target);
        CastEmpoweredFireBreath(target, level);
    }
    else if (HasSpell(DEEP_BREATH) && HasEssence(4))
    {
        CastDeepBreath(target);
    }
}

void DevastationSpecialization::CastDeepBreath(::Unit* target)
{
    if (CastSpell(DEEP_BREATH, target))
    {
        _metrics.deepBreathCasts++;
        _lastDeepBreathTime = getMSTime();
        LogDevastationDecision("Cast Deep Breath", "Ultimate AoE ability");
    }
}

// Management methods
void DevastationSpecialization::UpdateBurnoutManagement()
{
    _burnout.stacks = GetAuraStacks(BURNOUT);
    _burnout.timeRemaining = GetAuraTimeRemaining(BURNOUT);
    _burnout.isActive = _burnout.stacks > 0;
}

void DevastationSpecialization::UpdateEssenceBurstTracking()
{
    _essenceBurst.stacks = GetAuraStacks(ESSENCE_BURST);
    _essenceBurst.timeRemaining = GetAuraTimeRemaining(ESSENCE_BURST);
    _essenceBurst.isActive = _essenceBurst.stacks > 0;
}

void DevastationSpecialization::UpdateDragonrageManagement()
{
    if (!_dragonrage.isActive)
    {
        _dragonrage.isActive = HasAura(DRAGONRAGE);
        if (_dragonrage.isActive)
        {
            _dragonrage.remainingTime = GetAuraTimeRemaining(DRAGONRAGE);
        }
    }
}

void DevastationSpecialization::UpdateIridescenceTracking()
{
    _iridescence.hasBlue = HasAura(IRIDESCENCE_BLUE);
    _iridescence.hasRed = HasAura(IRIDESCENCE_RED);

    if (_iridescence.hasBlue)
        _iridescence.blueTimeRemaining = GetAuraTimeRemaining(IRIDESCENCE_BLUE);

    if (_iridescence.hasRed)
        _iridescence.redTimeRemaining = GetAuraTimeRemaining(IRIDESCENCE_RED);
}

void DevastationSpecialization::UpdateShatteringStarWindow()
{
    // Track Shattering Star debuff on target for priority
}

void DevastationSpecialization::UpdateAoETargeting()
{
    // Update AoE target counting and prioritization
}

bool DevastationSpecialization::ShouldAvoidBurnout()
{
    return _burnout.stacks >= MAX_BURNOUT_STACKS;
}

bool DevastationSpecialization::ShouldPrioritizeEmpowerment()
{
    return _prioritizeEmpowerment && _essence.state >= EssenceState::HIGH;
}

void DevastationSpecialization::OptimizeEmpoweredSpellUsage(::Unit* target)
{
    // Optimize empowered spell selection based on situation
    if (!target)
        return;

    uint32 enemyCount = CountNearbyEnemies(target, 8.0f);

    // Prefer Fire Breath for AoE
    if (enemyCount >= 3)
    {
        _preferredEmpowermentLevel = 3;
    }
    // Prefer Eternity's Surge for single target
    else
    {
        _preferredEmpowermentLevel = 3;
    }
}

void DevastationSpecialization::ManageMajorCooldowns()
{
    // Coordinate major cooldowns for maximum effectiveness
    if (ShouldUseMajorCooldown(SHATTERING_STAR))
    {
        CastShatteringStar(_currentTarget);
    }
}

bool DevastationSpecialization::ShouldUseMajorCooldown(uint32 spellId)
{
    // Use major cooldowns when we have good essence and target
    return _essence.state >= EssenceState::HIGH && _currentTarget;
}

void DevastationSpecialization::UseEmergencyAbilities()
{
    if (ShouldUseObsidianScales())
    {
        CastSpell(OBSIDIAN_SCALES);
    }
    else if (ShouldUseRenewingBlaze())
    {
        CastSpell(RENEWING_BLAZE);
    }
}

bool DevastationSpecialization::ShouldUseObsidianScales()
{
    return _bot->GetHealthPct() < 40.0f && HasSpell(OBSIDIAN_SCALES);
}

bool DevastationSpecialization::ShouldUseRenewingBlaze()
{
    return _bot->GetHealthPct() < 50.0f && HasSpell(RENEWING_BLAZE);
}

bool DevastationSpecialization::ShouldUseHover()
{
    return !HasAura(HOVER) && _useAggressivePositioning;
}

void DevastationSpecialization::OptimizeEssenceSpending()
{
    // Optimize essence spending based on current situation
    PlanEssenceUsage();
}

void DevastationSpecialization::PlanEssenceUsage()
{
    // Plan essence usage for upcoming rotation
    if (_devastationPhase == DevastationRotationPhase::DRAGONRAGE_BURST)
    {
        _conserveEssenceForBurst = false; // Spend freely during burst
    }
    else if (ShouldActivateDragonrage())
    {
        _conserveEssenceForBurst = true; // Save for upcoming burst
    }
}

void DevastationSpecialization::UpdateDevastationMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate uptime percentages
        _metrics.dragonrageUptime = _dragonrage.isActive ? (_metrics.dragonrageUptime + 1.0f) / 2.0f : _metrics.dragonrageUptime;
        _metrics.burnoutUptime = _burnout.isActive ? (_metrics.burnoutUptime + 1.0f) / 2.0f : _metrics.burnoutUptime;
        _metrics.essenceBurstUptime = _essenceBurst.isActive ? (_metrics.essenceBurstUptime + 1.0f) / 2.0f : _metrics.essenceBurstUptime;

        // Calculate DPS
        _metrics.averageDamagePerSecond = static_cast<float>(_totalDamageDealt) / (combatTime / 1000.0f);

        // Calculate average empowerment level
        if (_metrics.empoweredSpellsCast > 0)
        {
            _metrics.averageEmpowermentLevel = static_cast<float>(_preferredEmpowermentLevel);
        }
    }
}

void DevastationSpecialization::AnalyzeRotationEfficiency()
{
    // Performance analysis every 10 seconds
    if (getMSTime() % 10000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "DevastationSpecialization [{}]: Efficiency - DPS: {:.1f}, Dragonrage: {:.1f}%, Empowered: {}",
                    _bot->GetName(), _metrics.averageDamagePerSecond, _metrics.dragonrageUptime * 100, _metrics.empoweredSpellsCast);
    }
}

void DevastationSpecialization::LogDevastationDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

} // namespace Playerbot