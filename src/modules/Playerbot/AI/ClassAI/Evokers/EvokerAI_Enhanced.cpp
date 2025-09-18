/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EvokerAI_Enhanced.h"
#include "DevastationSpecialization_Enhanced.h"
#include "PreservationSpecialization_Enhanced.h"
#include "AugmentationSpecialization_Enhanced.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "ObjectMgr.h"
#include "Group.h"
#include "Pet.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

EvokerAI_Enhanced::EvokerAI_Enhanced(Player* bot) : PlayerbotClassAI(bot), _currentSpec(SPEC_NONE),
    _currentEssence(0), _maxEssence(5), _lastEssenceUpdate(0), _aspectShiftCooldown(0),
    _globalCooldown(0), _lastGlobalCooldown(0), _currentAspect(EvokerAspect::DEVASTATION),
    _aspectShiftInProgress(false), _lastAspectShift(0), _empoweredSpellActive(false),
    _empoweredSpellId(0), _empoweredSpellStartTime(0), _empoweredSpellTargetLevel(EmpowermentLevel::LEVEL_1),
    _empoweredSpellCurrentLevel(EmpowermentLevel::LEVEL_1), _empoweredSpellTarget(),
    _combatPhase(CombatPhase::PREPARATION), _lastCombatPhaseUpdate(0), _combatStartTime(0),
    _totalDamageDealt(0), _totalHealingDone(0), _totalEssenceSpent(0), _totalEssenceGenerated(0),
    _averageDps(0.0f), _averageHps(0.0f), _combatUptime(0), _lastPerformanceUpdate(0),
    _essenceEfficiency(0.85f), _empowermentEfficiency(0.9f), _aspectOptimization(0.8f),
    _rotationEfficiency(0.85f), _adaptabilityScore(0.75f), _specializationPerformance(0.8f)
{
    InitializeSpellIds();
    InitializeSpecializations();
    UpdateTalentDependentSpells();
}

EvokerAI_Enhanced::~EvokerAI_Enhanced() = default;

// Core AI Interface Implementation
bool EvokerAI_Enhanced::DoSpecificAction(std::string const& text)
{
    if (!_bot || !_bot->IsInWorld())
        return false;

    // Enhanced command processing with specialization awareness
    if (text == "devastation")
    {
        if (_currentSpec != SPEC_EVOKER_DEVASTATION)
        {
            SwitchToSpecialization(SPEC_EVOKER_DEVASTATION);
            _ai->TellMaster("Switching to Devastation specialization for ranged DPS.");
        }
        return true;
    }
    else if (text == "preservation")
    {
        if (_currentSpec != SPEC_EVOKER_PRESERVATION)
        {
            SwitchToSpecialization(SPEC_EVOKER_PRESERVATION);
            _ai->TellMaster("Switching to Preservation specialization for healing.");
        }
        return true;
    }
    else if (text == "augmentation")
    {
        if (_currentSpec != SPEC_EVOKER_AUGMENTATION)
        {
            SwitchToSpecialization(SPEC_EVOKER_AUGMENTATION);
            _ai->TellMaster("Switching to Augmentation specialization for support.");
        }
        return true;
    }
    else if (text == "aspect devastation")
    {
        ShiftToAspect(EvokerAspect::DEVASTATION);
        return true;
    }
    else if (text == "aspect preservation")
    {
        ShiftToAspect(EvokerAspect::PRESERVATION);
        return true;
    }
    else if (text == "aspect azure")
    {
        ShiftToAspect(EvokerAspect::AZURE);
        return true;
    }
    else if (text == "stats")
    {
        ReportPerformanceStats();
        return true;
    }
    else if (text == "optimize")
    {
        OptimizeForCurrentSituation();
        return true;
    }

    return false;
}

void EvokerAI_Enhanced::DoNonCombatActions()
{
    if (!_bot || !_bot->IsAlive())
        return;

    // Update essence regeneration out of combat
    UpdateEssenceRegeneration();

    // Maintain optimal aspect out of combat
    MaintainOptimalAspectOutOfCombat();

    // Update specialization
    if (_currentSpecialization)
        _currentSpecialization->UpdateBuffs();

    // Handle out of combat buff maintenance
    HandleOutOfCombatPreparation();

    // Update performance metrics
    UpdatePerformanceMetrics();
}

bool EvokerAI_Enhanced::DoFirstCombatManeuver(Unit* target)
{
    if (!target || !_bot)
        return false;

    // Update combat phase
    _combatPhase = CombatPhase::OPENING;
    _combatStartTime = getMSTime();

    // Set optimal aspect for combat
    SetOptimalAspectForCombat(target);

    // Initialize specialization combat
    if (_currentSpecialization)
        _currentSpecialization->OnCombatStart(target);

    // Execute opening sequence based on specialization
    return ExecuteOpeningSequence(target);
}

void EvokerAI_Enhanced::DoNextCombatManeuver(Unit* target)
{
    if (!target || !_bot)
        return;

    uint32 now = getMSTime();

    // Update all systems
    UpdateEssenceSystem();
    UpdateEmpowermentSystem();
    UpdateAspectManagement();
    UpdateCombatPhase();

    // Update global cooldown
    UpdateGlobalCooldown();

    // Let specialization handle rotation
    if (_currentSpecialization)
        _currentSpecialization->UpdateRotation(target);

    // Update performance tracking
    UpdateCombatMetrics();
}

bool EvokerAI_Enhanced::CanCastSpell(std::string const& name, Unit* target, uint8 spec)
{
    if (!_bot)
        return false;

    uint32 spellId = GetSpellIdByName(name);
    if (!spellId)
        return false;

    return CanCastSpell(spellId, target, spec != 0 ? spec : _currentSpec);
}

bool EvokerAI_Enhanced::CanCastSpell(uint32 spellId, Unit* target, uint8 spec)
{
    if (!_bot || !spellId)
        return false;

    // Check if bot knows the spell
    if (!_bot->HasSpell(spellId))
        return false;

    // Check global cooldown
    if (_globalCooldown > getMSTime())
        return false;

    // Check spell cooldown
    if (_bot->HasSpellCooldown(spellId))
        return false;

    // Check specialization-specific requirements
    if (_currentSpecialization && !_currentSpecialization->CanUseAbility(spellId))
        return false;

    // Check essence requirements
    if (!HasSufficientEssence(spellId))
        return false;

    // Check range if target is specified
    if (target)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo)
        {
            float range = _bot->GetSpellMaxRangeForTarget(target, spellInfo);
            if (!_bot->IsWithinLOSInMap(target) || _bot->GetDistance(target) > range)
                return false;
        }
    }

    return true;
}

// Essence Management Implementation
void EvokerAI_Enhanced::UpdateEssenceSystem()
{
    uint32 now = getMSTime();

    // Update essence regeneration (1 essence per 5 seconds out of combat, varies in combat)
    if (now - _lastEssenceUpdate >= GetEssenceRegenerationInterval())
    {
        RegenerateEssence();
        _lastEssenceUpdate = now;
    }

    // Update specialization essence management
    if (_currentSpecialization)
        _currentSpecialization->UpdateEssenceManagement();
}

void EvokerAI_Enhanced::RegenerateEssence()
{
    if (_currentEssence < _maxEssence)
    {
        _currentEssence++;
        _totalEssenceGenerated++;

        // Track essence efficiency
        UpdateEssenceEfficiency();
    }
}

uint32 EvokerAI_Enhanced::GetEssenceRegenerationInterval() const
{
    if (_bot->IsInCombat())
    {
        // In combat, essence regeneration is modified by various factors
        uint32 baseInterval = 5000; // 5 seconds base

        // Faster regeneration based on specialization
        if (_currentSpec == SPEC_EVOKER_DEVASTATION)
            baseInterval = 4000; // Devastation gets faster essence regen
        else if (_currentSpec == SPEC_EVOKER_PRESERVATION)
            baseInterval = 4500; // Preservation moderate regen

        return baseInterval;
    }
    else
    {
        return 3000; // 3 seconds out of combat
    }
}

bool EvokerAI_Enhanced::HasSufficientEssence(uint32 spellId) const
{
    uint32 requiredEssence = GetEssenceCost(spellId);
    return _currentEssence >= requiredEssence;
}

uint32 EvokerAI_Enhanced::GetEssenceCost(uint32 spellId) const
{
    // Define essence costs for spells
    auto it = _essenceCosts.find(spellId);
    if (it != _essenceCosts.end())
        return it->second;

    // Default costs based on spell type
    if (IsEmpoweredSpell(spellId))
        return 3; // Empowered spells cost 3 essence
    else if (IsMajorSpell(spellId))
        return 2; // Major spells cost 2 essence
    else
        return 1; // Basic spells cost 1 essence
}

void EvokerAI_Enhanced::SpendEssence(uint32 amount)
{
    if (amount > _currentEssence)
        amount = _currentEssence;

    _currentEssence -= amount;
    _totalEssenceSpent += amount;

    // Update specialization
    if (_currentSpecialization)
        _currentSpecialization->ConsumeResource(amount);
}

// Empowerment System Implementation
void EvokerAI_Enhanced::UpdateEmpowermentSystem()
{
    if (!_empoweredSpellActive)
        return;

    uint32 now = getMSTime();
    uint32 channelDuration = now - _empoweredSpellStartTime;

    // Calculate current empowerment level based on channel time
    EmpowermentLevel newLevel = CalculateEmpowermentLevel(channelDuration);

    if (newLevel != _empoweredSpellCurrentLevel)
    {
        _empoweredSpellCurrentLevel = newLevel;

        // Check if we've reached target level or maximum level
        if (_empoweredSpellCurrentLevel >= _empoweredSpellTargetLevel ||
            _empoweredSpellCurrentLevel == EmpowermentLevel::LEVEL_4)
        {
            ReleaseEmpoweredSpell();
        }
    }

    // Update specialization empowerment
    if (_currentSpecialization)
        _currentSpecialization->UpdateEmpoweredChanneling();
}

void EvokerAI_Enhanced::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, Unit* target)
{
    if (_empoweredSpellActive)
        ReleaseEmpoweredSpell(); // Release any active empowered spell first

    _empoweredSpellActive = true;
    _empoweredSpellId = spellId;
    _empoweredSpellStartTime = getMSTime();
    _empoweredSpellTargetLevel = targetLevel;
    _empoweredSpellCurrentLevel = EmpowermentLevel::LEVEL_1;

    if (target)
        _empoweredSpellTarget = target->GetGUID();

    // Start channeling the empowered spell
    if (_bot->HasSpell(spellId))
    {
        _bot->CastSpell(target, spellId, false);
    }

    // Notify specialization
    if (_currentSpecialization)
        _currentSpecialization->StartEmpoweredSpell(spellId, targetLevel, target);
}

void EvokerAI_Enhanced::ReleaseEmpoweredSpell()
{
    if (!_empoweredSpellActive)
        return;

    // Stop channeling and release the spell at current empowerment level
    _bot->InterruptNonMeleeSpells(false);

    // Apply empowerment effects based on current level
    ApplyEmpowermentEffects();

    // Reset empowerment state
    _empoweredSpellActive = false;
    _empoweredSpellId = 0;
    _empoweredSpellStartTime = 0;
    _empoweredSpellTargetLevel = EmpowermentLevel::LEVEL_1;
    _empoweredSpellCurrentLevel = EmpowermentLevel::LEVEL_1;
    _empoweredSpellTarget.Clear();

    // Notify specialization
    if (_currentSpecialization)
        _currentSpecialization->ReleaseEmpoweredSpell();
}

EmpowermentLevel EvokerAI_Enhanced::CalculateEmpowermentLevel(uint32 channelDuration) const
{
    // Each empowerment level takes 1 second to reach
    if (channelDuration >= 4000)
        return EmpowermentLevel::LEVEL_4;
    else if (channelDuration >= 3000)
        return EmpowermentLevel::LEVEL_3;
    else if (channelDuration >= 2000)
        return EmpowermentLevel::LEVEL_2;
    else
        return EmpowermentLevel::LEVEL_1;
}

void EvokerAI_Enhanced::ApplyEmpowermentEffects()
{
    // Empowerment effects are handled by the spell system
    // This method tracks performance metrics

    float efficiency = static_cast<float>(_empoweredSpellCurrentLevel) / static_cast<float>(_empoweredSpellTargetLevel);
    UpdateEmpowermentEfficiency(efficiency);
}

// Aspect Management Implementation
void EvokerAI_Enhanced::UpdateAspectManagement()
{
    uint32 now = getMSTime();

    // Check if aspect shift cooldown has expired
    if (_aspectShiftCooldown > 0 && _aspectShiftCooldown <= now)
        _aspectShiftCooldown = 0;

    // Update specialization aspect management
    if (_currentSpecialization)
        _currentSpecialization->UpdateAspectManagement();
}

void EvokerAI_Enhanced::ShiftToAspect(EvokerAspect aspect)
{
    if (_currentAspect == aspect || _aspectShiftCooldown > getMSTime())
        return;

    _currentAspect = aspect;
    _aspectShiftInProgress = true;
    _lastAspectShift = getMSTime();
    _aspectShiftCooldown = getMSTime() + 1500; // 1.5 second cooldown

    // Apply aspect shift spell effect
    uint32 aspectSpellId = GetAspectSpellId(aspect);
    if (aspectSpellId && _bot->HasSpell(aspectSpellId))
    {
        _bot->CastSpell(_bot, aspectSpellId, false);
    }

    // Update specialization
    if (_currentSpecialization)
        _currentSpecialization->ShiftToAspect(aspect);

    _aspectShiftInProgress = false;
}

uint32 EvokerAI_Enhanced::GetAspectSpellId(EvokerAspect aspect) const
{
    switch (aspect)
    {
        case EvokerAspect::DEVASTATION:
            return DEVASTATION_ASPECT_SPELL_ID;
        case EvokerAspect::PRESERVATION:
            return PRESERVATION_ASPECT_SPELL_ID;
        case EvokerAspect::AZURE:
            return AZURE_ASPECT_SPELL_ID;
        default:
            return 0;
    }
}

EvokerAspect EvokerAI_Enhanced::GetOptimalAspectForSituation() const
{
    if (!_bot)
        return EvokerAspect::DEVASTATION;

    // Let specialization determine optimal aspect
    if (_currentSpecialization)
        return _currentSpecialization->GetOptimalAspect();

    // Default logic based on specialization
    switch (_currentSpec)
    {
        case SPEC_EVOKER_DEVASTATION:
            return EvokerAspect::DEVASTATION;
        case SPEC_EVOKER_PRESERVATION:
            return EvokerAspect::PRESERVATION;
        case SPEC_EVOKER_AUGMENTATION:
            return EvokerAspect::AZURE;
        default:
            return EvokerAspect::DEVASTATION;
    }
}

// Combat Phase Management
void EvokerAI_Enhanced::UpdateCombatPhase()
{
    if (!_bot->IsInCombat())
    {
        _combatPhase = CombatPhase::PREPARATION;
        return;
    }

    uint32 now = getMSTime();
    uint32 combatDuration = now - _combatStartTime;

    // Update specialization combat phase
    if (_currentSpecialization)
        _currentSpecialization->UpdateCombatPhase();

    // General phase logic
    CombatPhase newPhase = _combatPhase;

    if (combatDuration < 5000)
        newPhase = CombatPhase::OPENING;
    else if (combatDuration < 30000)
        newPhase = CombatPhase::SUSTAINED;
    else
        newPhase = CombatPhase::EXTENDED;

    // Check for burst phase conditions
    if (ShouldEnterBurstPhase())
        newPhase = CombatPhase::BURST;

    if (newPhase != _combatPhase)
    {
        _combatPhase = newPhase;
        _lastCombatPhaseUpdate = now;
    }
}

bool EvokerAI_Enhanced::ShouldEnterBurstPhase() const
{
    // Let specialization determine burst timing
    if (_currentSpecialization)
        return _currentSpecialization->ShouldExecuteBurstRotation();

    return false;
}

// Specialization Management
void EvokerAI_Enhanced::InitializeSpecializations()
{
    _devastationSpec = std::make_unique<DevastationSpecialization_Enhanced>(_bot);
    _preservationSpec = std::make_unique<PreservationSpecialization_Enhanced>(_bot);
    _augmentationSpec = std::make_unique<AugmentationSpecialization_Enhanced>(_bot);

    // Set current specialization based on talent spec
    UpdateCurrentSpecialization();
}

void EvokerAI_Enhanced::UpdateCurrentSpecialization()
{
    uint8 newSpec = _bot->GetPrimaryTalentTree();

    if (newSpec != _currentSpec)
    {
        _currentSpec = newSpec;
        SwitchToSpecialization(_currentSpec);
    }
}

void EvokerAI_Enhanced::SwitchToSpecialization(uint8 spec)
{
    switch (spec)
    {
        case SPEC_EVOKER_DEVASTATION:
            _currentSpecialization = _devastationSpec.get();
            break;
        case SPEC_EVOKER_PRESERVATION:
            _currentSpecialization = _preservationSpec.get();
            break;
        case SPEC_EVOKER_AUGMENTATION:
            _currentSpecialization = _augmentationSpec.get();
            break;
        default:
            _currentSpecialization = _devastationSpec.get();
            break;
    }

    _currentSpec = spec;

    // Update talent-dependent spells
    UpdateTalentDependentSpells();
}

// Utility Methods
void EvokerAI_Enhanced::InitializeSpellIds()
{
    // Initialize spell ID mappings
    _spellNameToId["Azure Strike"] = AZURE_STRIKE_SPELL_ID;
    _spellNameToId["Living Flame"] = LIVING_FLAME_SPELL_ID;
    _spellNameToId["Disintegrate"] = DISINTEGRATE_SPELL_ID;
    _spellNameToId["Fire Breath"] = FIRE_BREATH_SPELL_ID;
    _spellNameToId["Eternity's Surge"] = ETERNITYS_SURGE_SPELL_ID;
    _spellNameToId["Emerald Blossom"] = EMERALD_BLOSSOM_SPELL_ID;
    _spellNameToId["Verdant Embrace"] = VERDANT_EMBRACE_SPELL_ID;
    _spellNameToId["Dream Breath"] = DREAM_BREATH_SPELL_ID;
    _spellNameToId["Spirit Bloom"] = SPIRIT_BLOOM_SPELL_ID;
    _spellNameToId["Ebon Might"] = EBON_MIGHT_SPELL_ID;
    _spellNameToId["Prescience"] = PRESCIENCE_SPELL_ID;
    _spellNameToId["Breath of Eons"] = BREATH_OF_EONS_SPELL_ID;

    // Initialize essence costs
    _essenceCosts[FIRE_BREATH_SPELL_ID] = 3;
    _essenceCosts[ETERNITYS_SURGE_SPELL_ID] = 3;
    _essenceCosts[DREAM_BREATH_SPELL_ID] = 3;
    _essenceCosts[SPIRIT_BLOOM_SPELL_ID] = 3;
    _essenceCosts[BREATH_OF_EONS_SPELL_ID] = 3;
    _essenceCosts[DISINTEGRATE_SPELL_ID] = 2;
    _essenceCosts[EBON_MIGHT_SPELL_ID] = 1;
    _essenceCosts[PRESCIENCE_SPELL_ID] = 1;
    _essenceCosts[AZURE_STRIKE_SPELL_ID] = 0; // Generates essence
    _essenceCosts[LIVING_FLAME_SPELL_ID] = 0; // Generates essence
}

uint32 EvokerAI_Enhanced::GetSpellIdByName(std::string const& name) const
{
    auto it = _spellNameToId.find(name);
    return it != _spellNameToId.end() ? it->second : 0;
}

bool EvokerAI_Enhanced::IsEmpoweredSpell(uint32 spellId) const
{
    return spellId == FIRE_BREATH_SPELL_ID ||
           spellId == ETERNITYS_SURGE_SPELL_ID ||
           spellId == DREAM_BREATH_SPELL_ID ||
           spellId == SPIRIT_BLOOM_SPELL_ID ||
           spellId == BREATH_OF_EONS_SPELL_ID;
}

bool EvokerAI_Enhanced::IsMajorSpell(uint32 spellId) const
{
    return spellId == DISINTEGRATE_SPELL_ID ||
           spellId == EBON_MIGHT_SPELL_ID ||
           spellId == PRESCIENCE_SPELL_ID;
}

void EvokerAI_Enhanced::UpdateTalentDependentSpells()
{
    // Update spells based on current talents
    // This method should check for talent-specific spell variations
}

void EvokerAI_Enhanced::UpdateGlobalCooldown()
{
    uint32 now = getMSTime();
    if (_lastGlobalCooldown + 1500 <= now) // 1.5 second GCD
        _globalCooldown = 0;
    else
        _globalCooldown = _lastGlobalCooldown + 1500;
}

// Performance and Metrics
void EvokerAI_Enhanced::UpdatePerformanceMetrics()
{
    uint32 now = getMSTime();

    if (now - _lastPerformanceUpdate < 5000) // Update every 5 seconds
        return;

    if (_bot->IsInCombat())
    {
        uint32 combatDuration = now - _combatStartTime;
        if (combatDuration > 0)
        {
            _averageDps = static_cast<float>(_totalDamageDealt) / (combatDuration / 1000.0f);
            _averageHps = static_cast<float>(_totalHealingDone) / (combatDuration / 1000.0f);
        }
    }

    UpdateEssenceEfficiency();
    UpdateSpecializationPerformance();

    _lastPerformanceUpdate = now;
}

void EvokerAI_Enhanced::UpdateEssenceEfficiency()
{
    if (_totalEssenceGenerated > 0)
    {
        float wastedEssence = static_cast<float>(_totalEssenceGenerated - _totalEssenceSpent);
        _essenceEfficiency = 1.0f - (wastedEssence / static_cast<float>(_totalEssenceGenerated));
        _essenceEfficiency = std::max(0.0f, std::min(1.0f, _essenceEfficiency));
    }
}

void EvokerAI_Enhanced::UpdateEmpowermentEfficiency(float efficiency)
{
    // Exponential moving average
    _empowermentEfficiency = _empowermentEfficiency * 0.8f + efficiency * 0.2f;
}

void EvokerAI_Enhanced::UpdateSpecializationPerformance()
{
    if (_currentSpecialization)
    {
        // Get specialization-specific metrics and update performance score
        // This is a placeholder for more detailed performance analysis
        _specializationPerformance = 0.8f; // Default value
    }
}

void EvokerAI_Enhanced::ReportPerformanceStats()
{
    if (!_ai)
        return;

    std::ostringstream stats;
    stats << "Evoker Performance Stats:\n";
    stats << "Current Essence: " << _currentEssence << "/" << _maxEssence << "\n";
    stats << "Essence Efficiency: " << std::fixed << std::setprecision(1) << (_essenceEfficiency * 100.0f) << "%\n";
    stats << "Empowerment Efficiency: " << std::fixed << std::setprecision(1) << (_empowermentEfficiency * 100.0f) << "%\n";
    stats << "Current Aspect: " << GetAspectName(_currentAspect) << "\n";
    stats << "Average DPS: " << std::fixed << std::setprecision(0) << _averageDps << "\n";
    stats << "Average HPS: " << std::fixed << std::setprecision(0) << _averageHps << "\n";

    if (_currentSpecialization)
    {
        stats << "Specialization: " << GetSpecializationName(_currentSpec) << "\n";
    }

    _ai->TellMaster(stats.str());
}

std::string EvokerAI_Enhanced::GetAspectName(EvokerAspect aspect) const
{
    switch (aspect)
    {
        case EvokerAspect::DEVASTATION: return "Devastation";
        case EvokerAspect::PRESERVATION: return "Preservation";
        case EvokerAspect::AZURE: return "Azure";
        default: return "Unknown";
    }
}

std::string EvokerAI_Enhanced::GetSpecializationName(uint8 spec) const
{
    switch (spec)
    {
        case SPEC_EVOKER_DEVASTATION: return "Devastation";
        case SPEC_EVOKER_PRESERVATION: return "Preservation";
        case SPEC_EVOKER_AUGMENTATION: return "Augmentation";
        default: return "Unknown";
    }
}

// Helper Methods
bool EvokerAI_Enhanced::ExecuteOpeningSequence(Unit* target)
{
    if (!target)
        return false;

    // Let specialization handle opening
    if (_currentSpecialization)
    {
        _currentSpecialization->OnCombatStart(target);
        return true;
    }

    return false;
}

void EvokerAI_Enhanced::SetOptimalAspectForCombat(Unit* target)
{
    EvokerAspect optimalAspect = GetOptimalAspectForSituation();
    if (optimalAspect != _currentAspect)
    {
        ShiftToAspect(optimalAspect);
    }
}

void EvokerAI_Enhanced::MaintainOptimalAspectOutOfCombat()
{
    // Maintain preservation aspect for healing or devastation for general use
    EvokerAspect preferredAspect = (_currentSpec == SPEC_EVOKER_PRESERVATION) ?
        EvokerAspect::PRESERVATION : EvokerAspect::DEVASTATION;

    if (_currentAspect != preferredAspect && _aspectShiftCooldown <= getMSTime())
    {
        ShiftToAspect(preferredAspect);
    }
}

void EvokerAI_Enhanced::HandleOutOfCombatPreparation()
{
    // Ensure full essence
    if (_currentEssence < _maxEssence)
        RegenerateEssence();

    // Any specialization-specific preparation
    if (_currentSpecialization)
    {
        // Specializations can override this for their specific needs
    }
}

void EvokerAI_Enhanced::OptimizeForCurrentSituation()
{
    if (!_ai)
        return;

    // Analyze current situation and optimize accordingly
    if (_bot->IsInCombat())
    {
        Unit* target = _ai->GetCurrentTarget();
        if (target)
        {
            SetOptimalAspectForCombat(target);
            _ai->TellMaster("Optimizing for combat situation.");
        }
    }
    else
    {
        MaintainOptimalAspectOutOfCombat();
        _ai->TellMaster("Optimizing for non-combat situation.");
    }
}

void EvokerAI_Enhanced::UpdateCombatMetrics()
{
    // Update damage and healing tracking
    // This would integrate with the combat log system to track actual performance
    _combatUptime = getMSTime() - _combatStartTime;
}

} // namespace Playerbot