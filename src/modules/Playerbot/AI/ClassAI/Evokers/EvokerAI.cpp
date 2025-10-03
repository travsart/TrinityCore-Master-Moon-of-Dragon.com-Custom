/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EvokerAI.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "Group.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "../BaselineRotationManager.h"

namespace Playerbot
{

EvokerAI::EvokerAI(Player* bot) : ClassAI(bot)
{
    _specialization = DetectSpecialization();
    _currentAspect = EvokerAspect::NONE;
    _damageDealt = 0;
    _healingDone = 0;
    _essenceGenerated = 0;
    _empoweredSpellsCast = 0;
    _echoHealsPerformed = 0;

    // Initialize essence management
    _essence = EssenceInfo();
    _lastEssenceUpdate = 0;
    _essenceUpdateInterval = ESSENCE_GENERATION_RATE;

    // Initialize empowerment system
    _currentEmpoweredSpell = EmpoweredSpell();
    _lastEmpoweredSpell = 0;
    _isChannelingEmpowered = false;

    // Initialize echo system
    _lastEchoUpdate = 0;
    _echoUpdateInterval = ECHO_HEAL_INTERVAL;
    _maxEchoes = ECHO_MAX_COUNT;

    // Initialize Devastation tracking
    _burnoutStacks = 0;
    _essenceBurstStacks = 0;
    _dragonrageStacks = 0;
    _lastEternity = 0;
    _lastDisintegrate = 0;
    _eternitysSurgeReady = true;

    // Initialize Preservation tracking
    _temporalCompressionStacks = 0;
    _callOfYseraStacks = 0;
    _lastVerdantEmbrace = 0;
    _lastTemporalAnomaly = 0;

    // Initialize Augmentation tracking
    _prescientStacks = 0;
    _blisteryScalesStacks = 0;
    _lastEbon = 0;
    _lastBreathOfEons = 0;

    // Initialize aspect management
    _aspectDuration = 0;
    _lastAspectShift = 0;
    _aspectCooldown = ASPECT_SHIFT_COOLDOWN;
    _canShiftAspect = true;

    // Initialize utility tracking
    _lastSoar = 0;
    _lastHover = 0;
    _lastRescue = 0;
    _lastTimeSpiral = 0;
    _hoverRemaining = 0;
    _isHovering = false;
}

void EvokerAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(_bot);

        if (baselineManager.ExecuteBaselineRotation(_bot, target))
            return;

        // Fallback: basic ranged attack
        if (!_bot->IsNonMeleeSpellCast(false))
        {
            if (_bot->GetDistance(target) <= 35.0f)
            {
                _bot->AttackerStateUpdate(target);
            }
        }
        return;
    }

    UpdateEssenceManagement();
    UpdateEmpowermentSystem();
    UpdateAspectManagement();

    switch (_specialization)
    {
        case EvokerSpec::DEVASTATION:
            UpdateDevastationRotation(target);
            break;
        case EvokerSpec::PRESERVATION:
            UpdatePreservationRotation(target);
            UpdateEchoSystem();
            break;
        case EvokerSpec::AUGMENTATION:
            UpdateAugmentationRotation(target);
            break;
    }

    OptimizeResourceUsage();
}

void EvokerAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(_bot);
        return;
    }

    ManageBuffs();

    // Maintain hover for mobility if needed
    if (ShouldUseHover() && CanUseAbility(HOVER))
        CastHover();

    // Shift to optimal aspect
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _currentAspect && CanShiftAspect())
        ShiftToAspect(optimalAspect);
}

void EvokerAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);

    // Update essence regeneration
    _lastEssenceUpdate += diff;
    if (_lastEssenceUpdate >= _essenceUpdateInterval)
    {
        if (_essence.isRegenerating && _essence.current < _essence.maximum)
        {
            GenerateEssence(1);
            _lastEssenceUpdate = 0;
        }
    }

    // Update empowered spell channeling
    if (_isChannelingEmpowered)
        UpdateEmpoweredChanneling();

    // Update echo healing
    if (_specialization == EvokerSpec::PRESERVATION)
    {
        _lastEchoUpdate += diff;
        if (_lastEchoUpdate >= _echoUpdateInterval)
        {
            ProcessEchoHealing();
            _lastEchoUpdate = 0;
        }
    }

    // Update hover duration
    if (_isHovering && _hoverRemaining > 0)
    {
        if (_hoverRemaining <= diff)
        {
            _hoverRemaining = 0;
            _isHovering = false;
        }
        else
        {
            _hoverRemaining -= diff;
        }
    }

    // Update aspect cooldown
    if (_lastAspectShift > 0)
    {
        _lastAspectShift = _lastAspectShift > diff ? _lastAspectShift - diff : 0;
        if (_lastAspectShift == 0)
            _canShiftAspect = true;
    }

    // Update various stack counters
    if (_burnoutStacks > 0)
    {
        static uint32 burnoutDecay = 0;
        burnoutDecay += diff;
        if (burnoutDecay >= 10000) // 10 second duration
        {
            _burnoutStacks = _burnoutStacks > 0 ? _burnoutStacks - 1 : 0;
            burnoutDecay = 0;
        }
    }
}

bool EvokerAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Cannot cast while channeling empowered spells
    if (_isChannelingEmpowered && !IsEmpoweredSpell(spellId))
        return false;

    return true;
}

void EvokerAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Shift to combat-appropriate aspect
    EvokerAspect combatAspect = GetOptimalAspect();
    if (combatAspect != _currentAspect)
        ShiftToAspect(combatAspect);
}

void EvokerAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Reset combat-specific tracking
    _burnoutStacks = 0;
    _essenceBurstStacks = 0;
    _dragonrageStacks = 0;
    _temporalCompressionStacks = 0;
    _prescientStacks = 0;

    // Process any remaining echoes
    if (!_activeEchoes.empty())
        ProcessEchoHealing();
}

bool EvokerAI::HasEnoughResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Most evoker abilities require essence
    switch (spellId)
    {
        case AZURE_STRIKE:
        case LIVING_FLAME:
            return true; // Free abilities

        case ETERNITYS_SURGE:
        case DISINTEGRATE:
        case PYRE:
        case FIRE_BREATH:
        case EMERALD_BLOSSOM:
        case VERDANT_EMBRACE:
        case DREAM_BREATH:
        case SPIRIT_BLOOM:
        case EBON_MIGHT:
        case PRESCIENCE:
        {
            auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
            for (auto const& cost : powerCosts)
                if (cost.Power == POWER_MANA)
                    return HasEssence(cost.Amount);
            return true;
        }

        default:
            return true;
    }
}

void EvokerAI::ConsumeResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return;

    switch (spellId)
    {
        case AZURE_STRIKE:
            GenerateEssence(1); // Generates essence
            break;

        case ETERNITYS_SURGE:
        case DISINTEGRATE:
        case PYRE:
        case FIRE_BREATH:
        case EMERALD_BLOSSOM:
        case VERDANT_EMBRACE:
        case DREAM_BREATH:
        case SPIRIT_BLOOM:
        case EBON_MIGHT:
        case PRESCIENCE:
        {
            auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
            for (auto const& cost : powerCosts)
                if (cost.Power == POWER_MANA)
                    SpendEssence(cost.Amount);
            break;
        }
    }
}

Position EvokerAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    Position pos = _bot->GetPosition();
    float distance = _bot->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    if (distance > optimalRange || distance < optimalRange * 0.8f)
    {
        pos = target->GetPosition();
        pos.m_positionX += optimalRange * cos(target->GetOrientation() + M_PI);
        pos.m_positionY += optimalRange * sin(target->GetOrientation() + M_PI);
    }

    return pos;
}

float EvokerAI::GetOptimalRange(::Unit* target)
{
    if (_isChannelingEmpowered)
        return EMPOWERED_SPELL_RANGE;

    return OPTIMAL_CASTING_RANGE;
}

void EvokerAI::UpdateDevastationRotation(::Unit* target)
{
    if (!target)
        return;

    // Use empowered abilities when available
    if (!_isChannelingEmpowered)
    {
        // Eternity's Surge with empowerment
        if (_eternitysSurgeReady && CanUseAbility(ETERNITYS_SURGE))
        {
            EmpowermentLevel level = CalculateOptimalEmpowermentLevel(ETERNITYS_SURGE, target);
            CastEmpoweredEternitysSurge(target, level);
            return;
        }

        // Fire Breath for AoE situations
        std::vector<::Unit*> enemies = GetEmpoweredSpellTargets(FIRE_BREATH);
        if (enemies.size() >= 3 && CanUseAbility(FIRE_BREATH))
        {
            EmpowermentLevel level = CalculateOptimalEmpowermentLevel(FIRE_BREATH, target);
            CastEmpoweredFireBreath(target, level);
            return;
        }
    }

    // Non-empowered abilities
    if (HasEssence(2) && CanUseAbility(DISINTEGRATE))
    {
        CastDisintegrate(target);
        return;
    }

    if (HasEssence(3) && CanUseAbility(PYRE))
    {
        CastPyre(target);
        return;
    }

    // Living Flame for damage and essence generation
    if (CanUseAbility(LIVING_FLAME))
    {
        CastLivingFlame(target);
        return;
    }

    // Azure Strike for essence generation
    if (CanUseAbility(AZURE_STRIKE))
    {
        CastAzureStrike(target);
    }
}

void EvokerAI::UpdatePreservationRotation(::Unit* target)
{
    // Prioritize healing over damage
    UsePreservationAbilities();

    // DPS if no healing needed
    if (!GetBestHealTarget() && target)
    {
        if (CanUseAbility(LIVING_FLAME))
            CastLivingFlame(target);
        else if (CanUseAbility(AZURE_STRIKE))
            CastAzureStrike(target);
    }
}

void EvokerAI::UpdateAugmentationRotation(::Unit* target)
{
    if (!target)
        return;

    UseAugmentationAbilities(target);
    ManageAugmentationBuffs();

    // Augment allies with buffs
    ::Unit* augmentTarget = GetBestAugmentationTarget();
    if (augmentTarget)
    {
        if (CanUseAbility(EBON_MIGHT))
            CastEbonMight(augmentTarget);
        else if (CanUseAbility(PRESCIENCE))
            CastPrescience(augmentTarget);
    }

    // DPS rotation when augmentation is maintained
    if (CanUseAbility(LIVING_FLAME))
        CastLivingFlame(target);
    else if (CanUseAbility(AZURE_STRIKE))
        CastAzureStrike(target);
}

void EvokerAI::UpdateEssenceManagement()
{
    // Essence regenerates naturally over time (handled in UpdateCooldowns)

    // Optimize essence usage based on specialization
    if (_essence.current >= _essence.maximum * 0.9f)
    {
        // Spend excess essence
        ::Unit* target = ObjectAccessor::GetUnit(*GetBot(), GetBot()->GetTarget());
        if (target)
        {
            if (_specialization == EvokerSpec::DEVASTATION && CanUseAbility(DISINTEGRATE))
                CastDisintegrate(target);
            else if (_specialization == EvokerSpec::PRESERVATION)
            {
                ::Unit* healTarget = GetBestHealTarget();
                if (healTarget && CanUseAbility(VERDANT_EMBRACE))
                    CastVerdantEmbrace(healTarget);
            }
        }
    }
}

void EvokerAI::GenerateEssence(uint32 amount)
{
    _essence.GenerateEssence(amount);
    _essenceGenerated += amount;
}

void EvokerAI::SpendEssence(uint32 amount)
{
    _essence.SpendEssence(amount);
}

bool EvokerAI::HasEssence(uint32 required)
{
    return _essence.HasEssence(required);
}

uint32 EvokerAI::GetEssence()
{
    return _essence.current;
}

uint32 EvokerAI::GetMaxEssence()
{
    return _essence.maximum;
}

float EvokerAI::GetEssencePercent()
{
    return _essence.maximum > 0 ? (float)_essence.current / _essence.maximum : 0.0f;
}

void EvokerAI::UpdateEmpowermentSystem()
{
    if (_isChannelingEmpowered)
        UpdateEmpoweredChanneling();
}

void EvokerAI::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target)
{
    if (_isChannelingEmpowered)
        return;

    _currentEmpoweredSpell = EmpoweredSpell(spellId, targetLevel, target);
    _isChannelingEmpowered = true;
    _lastEmpoweredSpell = getMSTime();
}

void EvokerAI::UpdateEmpoweredChanneling()
{
    if (!_isChannelingEmpowered)
        return;

    // Check if we should release the spell
    if (_currentEmpoweredSpell.ShouldRelease())
    {
        ReleaseEmpoweredSpell();
    }
}

void EvokerAI::ReleaseEmpoweredSpell()
{
    if (!_isChannelingEmpowered)
        return;

    // Cast the empowered spell at the achieved level
    uint32 spellId = _currentEmpoweredSpell.spellId;
    ::Unit* target = _currentEmpoweredSpell.target;

    if (target && CanUseAbility(spellId))
    {
        _bot->CastSpell(target, spellId, false);
        ConsumeResource(spellId);
        _empoweredSpellsCast++;
    }

    // Reset empowered spell state
    _currentEmpoweredSpell = EmpoweredSpell();
    _isChannelingEmpowered = false;
}

EmpowermentLevel EvokerAI::CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target)
{
    if (!target)
        return EmpowermentLevel::RANK_1;

    // Calculate based on situation
    std::vector<::Unit*> targets = GetEmpoweredSpellTargets(spellId);

    if (targets.size() >= 5)
        return EmpowermentLevel::RANK_4;
    else if (targets.size() >= 3)
        return EmpowermentLevel::RANK_3;
    else if (targets.size() >= 2)
        return EmpowermentLevel::RANK_2;
    else
        return EmpowermentLevel::RANK_1;
}

void EvokerAI::UpdateEchoSystem()
{
    if (_specialization != EvokerSpec::PRESERVATION)
        return;

    ProcessEchoHealing();
    RemoveExpiredEchoes();
}

void EvokerAI::CreateEcho(::Unit* target, uint32 healAmount, uint32 numHeals)
{
    if (!target || _activeEchoes.size() >= _maxEchoes)
        return;

    _activeEchoes.emplace_back(target, numHeals, healAmount);
}

void EvokerAI::ProcessEchoHealing()
{
    for (auto& echo : _activeEchoes)
    {
        if (echo.ShouldHeal() && echo.target)
        {
            // Perform echo healing
            _bot->CastSpell(echo.target, ECHO, false);
            echo.ProcessHeal();
            _echoHealsPerformed++;
        }
    }
}

void EvokerAI::RemoveExpiredEchoes()
{
    _activeEchoes.erase(
        std::remove_if(_activeEchoes.begin(), _activeEchoes.end(),
            [](const Echo& echo) { return echo.remainingHeals == 0 || !echo.target; }),
        _activeEchoes.end());
}

uint32 EvokerAI::GetActiveEchoCount()
{
    return static_cast<uint32>(_activeEchoes.size());
}

bool EvokerAI::ShouldCreateEcho(::Unit* target)
{
    return target && target->GetHealthPct() < 80.0f && GetActiveEchoCount() < _maxEchoes;
}

void EvokerAI::UpdateAspectManagement()
{
    // Aspect management is handled in UpdateBuffs()
}

void EvokerAI::ShiftToAspect(EvokerAspect aspect)
{
    if (!CanShiftAspect() || _currentAspect == aspect)
        return;

    uint32 aspectSpellId = 0;
    switch (aspect)
    {
        case EvokerAspect::BRONZE: aspectSpellId = BRONZE_ASPECT; break;
        case EvokerAspect::AZURE: aspectSpellId = AZURE_ASPECT; break;
        case EvokerAspect::GREEN: aspectSpellId = GREEN_ASPECT; break;
        case EvokerAspect::RED: aspectSpellId = RED_ASPECT; break;
        case EvokerAspect::BLACK: aspectSpellId = BLACK_ASPECT; break;
        default: return;
    }

    if (CanUseAbility(aspectSpellId))
    {
        _bot->CastSpell(_bot, aspectSpellId, false);
        _currentAspect = aspect;
        _lastAspectShift = _aspectCooldown;
        _canShiftAspect = false;
    }
}

EvokerAspect EvokerAI::GetOptimalAspect()
{
    switch (_specialization)
    {
        case EvokerSpec::DEVASTATION:
            return EvokerAspect::RED; // Red for damage
        case EvokerSpec::PRESERVATION:
            return EvokerAspect::GREEN; // Green for healing
        case EvokerSpec::AUGMENTATION:
            return EvokerAspect::BRONZE; // Bronze for support
        default:
            return EvokerAspect::AZURE;
    }
}

bool EvokerAI::CanShiftAspect()
{
    return _canShiftAspect && _lastAspectShift == 0;
}

::Unit* EvokerAI::GetBestHealTarget()
{
    ::Unit* lowestTarget = nullptr;
    float lowestHealth = 100.0f;

    // Check self
    if (_bot->GetHealthPct() < lowestHealth)
    {
        lowestTarget = _bot;
        lowestHealth = _bot->GetHealthPct();
    }

    // Check group members
    if (Group* group = _bot->GetGroup())
    {
        for (auto const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::FindPlayer(member.guid))
            {
                if (player->GetHealthPct() < lowestHealth && player->GetDistance(_bot) <= OPTIMAL_CASTING_RANGE)
                {
                    lowestTarget = player;
                    lowestHealth = player->GetHealthPct();
                }
            }
        }
    }

    return lowestTarget;
}

::Unit* EvokerAI::GetBestAugmentationTarget()
{
    // Find ally DPS for augmentation buffs
    if (Group* group = _bot->GetGroup())
    {
        for (auto const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::FindPlayer(member.guid))
            {
                if (player->GetDistance(_bot) <= OPTIMAL_CASTING_RANGE && player != _bot)
                {
                    // Prefer DPS classes for augmentation
                    return player;
                }
            }
        }
    }

    return nullptr;
}

std::vector<::Unit*> EvokerAI::GetEmpoweredSpellTargets(uint32 spellId)
{
    std::vector<::Unit*> targets;

    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, EMPOWERED_SPELL_RANGE);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, check);
    Cell::VisitAllObjects(_bot, searcher, EMPOWERED_SPELL_RANGE);

    for (Unit* enemy : nearbyEnemies)
    {
        targets.push_back(enemy);
    }

    return targets;
}

EvokerSpec EvokerAI::DetectSpecialization()
{
    if (!_bot)
        return EvokerSpec::DEVASTATION;

    // Simple detection based on known spells
    if (_bot->HasSpell(EMERALD_BLOSSOM) || _bot->HasSpell(DREAM_BREATH))
        return EvokerSpec::PRESERVATION;

    if (_bot->HasSpell(EBON_MIGHT) || _bot->HasSpell(PRESCIENCE))
        return EvokerSpec::AUGMENTATION;

    return EvokerSpec::DEVASTATION;
}

bool EvokerAI::IsEmpoweredSpell(uint32 spellId)
{
    switch (spellId)
    {
        case ETERNITYS_SURGE:
        case FIRE_BREATH:
        case DREAM_BREATH:
        case SPIRIT_BLOOM:
            return true;
        default:
            return false;
    }
}

// Combat ability implementations
void EvokerAI::CastAzureStrike(::Unit* target)
{
    if (!target || !CanUseAbility(AZURE_STRIKE))
        return;

    _bot->CastSpell(target, AZURE_STRIKE, false);
    ConsumeResource(AZURE_STRIKE);
}

void EvokerAI::CastLivingFlame(::Unit* target)
{
    if (!target || !CanUseAbility(LIVING_FLAME))
        return;

    _bot->CastSpell(target, LIVING_FLAME, false);
    ConsumeResource(LIVING_FLAME);
}

void EvokerAI::CastEmpoweredEternitysSurge(::Unit* target, EmpowermentLevel level)
{
    if (!target || !CanUseAbility(ETERNITYS_SURGE))
        return;

    StartEmpoweredSpell(ETERNITYS_SURGE, level, target);
    _eternitysSurgeReady = false;
}

void EvokerAI::CastDisintegrate(::Unit* target)
{
    if (!target || !CanUseAbility(DISINTEGRATE))
        return;

    _bot->CastSpell(target, DISINTEGRATE, false);
    _lastDisintegrate = getMSTime();
    ConsumeResource(DISINTEGRATE);
}

void EvokerAI::CastPyre(::Unit* target)
{
    if (!target || !CanUseAbility(PYRE))
        return;

    _bot->CastSpell(target, PYRE, false);
    ConsumeResource(PYRE);
}

void EvokerAI::CastEmpoweredFireBreath(::Unit* target, EmpowermentLevel level)
{
    if (!target || !CanUseAbility(FIRE_BREATH))
        return;

    StartEmpoweredSpell(FIRE_BREATH, level, target);
}

void EvokerAI::UsePreservationAbilities()
{
    ::Unit* healTarget = GetBestHealTarget();
    if (!healTarget)
        return;

    float healthPercent = healTarget->GetHealthPct();

    // Emergency healing
    if (healthPercent < 30.0f)
    {
        if (!_isChannelingEmpowered && CanUseAbility(SPIRIT_BLOOM))
        {
            EmpowermentLevel level = CalculateOptimalEmpowermentLevel(SPIRIT_BLOOM, healTarget);
            CastEmpoweredSpiritBloom(healTarget, level);
        }
        else if (CanUseAbility(VERDANT_EMBRACE))
            CastVerdantEmbrace(healTarget);
    }
    // Moderate healing
    else if (healthPercent < 70.0f)
    {
        if (CanUseAbility(EMERALD_BLOSSOM))
            CastEmeraldBlossom();
        else if (ShouldCreateEcho(healTarget) && CanUseAbility(VERDANT_EMBRACE))
        {
            CastVerdantEmbrace(healTarget);
            CreateEcho(healTarget, 1000, 3);
        }
    }
    // Group healing
    else if (CanUseAbility(EMERALD_BLOSSOM))
    {
        CastEmeraldBlossom();
    }
}

void EvokerAI::CastEmeraldBlossom()
{
    if (!CanUseAbility(EMERALD_BLOSSOM))
        return;

    _bot->CastSpell(_bot, EMERALD_BLOSSOM, false);
    ConsumeResource(EMERALD_BLOSSOM);
}

void EvokerAI::CastVerdantEmbrace(::Unit* target)
{
    if (!target || !CanUseAbility(VERDANT_EMBRACE))
        return;

    _bot->CastSpell(target, VERDANT_EMBRACE, false);
    _lastVerdantEmbrace = getMSTime();
    ConsumeResource(VERDANT_EMBRACE);
}

void EvokerAI::CastEmpoweredDreamBreath(::Unit* target, EmpowermentLevel level)
{
    if (!target || !CanUseAbility(DREAM_BREATH))
        return;

    StartEmpoweredSpell(DREAM_BREATH, level, target);
}

void EvokerAI::CastEmpoweredSpiritBloom(::Unit* target, EmpowermentLevel level)
{
    if (!target || !CanUseAbility(SPIRIT_BLOOM))
        return;

    StartEmpoweredSpell(SPIRIT_BLOOM, level, target);
}

void EvokerAI::UseAugmentationAbilities(::Unit* target)
{
    ::Unit* augmentTarget = GetBestAugmentationTarget();
    if (!augmentTarget)
        return;

    if (CanUseAbility(EBON_MIGHT))
        CastEbonMight(augmentTarget);
    else if (CanUseAbility(PRESCIENCE))
        CastPrescience(augmentTarget);
}

void EvokerAI::CastEbonMight(::Unit* target)
{
    if (!target || !CanUseAbility(EBON_MIGHT))
        return;

    _bot->CastSpell(target, EBON_MIGHT, false);
    _lastEbon = getMSTime();
    ConsumeResource(EBON_MIGHT);
}

void EvokerAI::CastPrescience(::Unit* target)
{
    if (!target || !CanUseAbility(PRESCIENCE))
        return;

    _bot->CastSpell(target, PRESCIENCE, false);
    ConsumeResource(PRESCIENCE);
}

void EvokerAI::CastHover()
{
    if (!CanUseAbility(HOVER))
        return;

    _bot->CastSpell(_bot, HOVER, false);
    _isHovering = true;
    _hoverRemaining = 8000; // 8 seconds
}

bool EvokerAI::ShouldUseHover()
{
    // Use hover for mobility when repositioning or under threat
    return !_isHovering && (_bot->IsInCombat() && _bot->GetHealthPct() < 50.0f);
}

void EvokerAI::ManageBuffs()
{
    // Maintain aspect and other buffs
}

void EvokerAI::ManageAugmentationBuffs()
{
    // Manage augmentation buff timers and priorities
}

void EvokerAI::OptimizeResourceUsage()
{
    // Optimize essence usage based on situation
    if (_essence.current >= _essence.maximum * 0.9f)
    {
        ::Unit* target = ObjectAccessor::GetUnit(*GetBot(), GetBot()->GetTarget());
        if (target && _specialization == EvokerSpec::DEVASTATION)
        {
            if (CanUseAbility(DISINTEGRATE))
                CastDisintegrate(target);
        }
    }
}

void EvokerAI::RecordDamageDealt(uint32 damage, ::Unit* target)
{
    _damageDealt += damage;
}

void EvokerAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
}

void EvokerAI::RecordEchoHealing(uint32 amount)
{
    _healingDone += amount;
    _echoHealsPerformed++;
}

// Utility class implementations
uint32 EvokerCalculator::CalculateAzureStrikeDamage(Player* caster, ::Unit* target)
{
    return 600; // Placeholder
}

uint32 EvokerCalculator::CalculateLivingFlameDamage(Player* caster, ::Unit* target)
{
    return 800; // Placeholder
}

uint32 EvokerCalculator::CalculateEmpoweredSpellDamage(uint32 spellId, EmpowermentLevel level, Player* caster, ::Unit* target)
{
    uint32 baseDamage = 1000;
    return baseDamage * (static_cast<uint32>(level) + 1); // Scales with empowerment level
}

uint32 EvokerCalculator::CalculateEmeraldBlossomHealing(Player* caster)
{
    return 800; // Placeholder
}

uint32 EvokerCalculator::CalculateVerdantEmbraceHealing(Player* caster, ::Unit* target)
{
    return 1200; // Placeholder
}

uint32 EvokerCalculator::CalculateEchoHealing(Player* caster, ::Unit* target)
{
    return 300; // Placeholder
}

EmpowermentLevel EvokerCalculator::GetOptimalEmpowermentLevel(uint32 spellId, Player* caster, ::Unit* target)
{
    return EmpowermentLevel::RANK_2; // Placeholder
}

uint32 EvokerCalculator::CalculateEmpowermentChannelTime(EmpowermentLevel level)
{
    return static_cast<uint32>(level) * 1000; // 1 second per rank
}

float EvokerCalculator::CalculateEmpowermentEfficiency(uint32 spellId, EmpowermentLevel level, Player* caster)
{
    return 1.0f + static_cast<float>(level) * 0.25f; // 25% per rank
}

uint32 EvokerCalculator::CalculateEssenceGeneration(uint32 spellId, Player* caster)
{
    switch (spellId)
    {
        case EvokerAI::AZURE_STRIKE: return 1;
        default: return 0;
    }
}

float EvokerCalculator::CalculateEssenceEfficiency(uint32 spellId, Player* caster)
{
    return 1.0f; // Placeholder
}

bool EvokerCalculator::ShouldConserveEssence(Player* caster, uint32 currentEssence)
{
    return currentEssence < 2; // Conserve when below 2 essence
}

uint32 EvokerCalculator::CalculateOptimalEchoTargets(Player* caster, const std::vector<::Unit*>& allies)
{
    return std::min(static_cast<uint32>(allies.size()), 8u);
}

bool EvokerCalculator::ShouldCreateEcho(Player* caster, ::Unit* target)
{
    return target && target->GetHealthPct() < 80.0f;
}

uint32 EvokerCalculator::CalculateEchoValue(Player* caster, ::Unit* target)
{
    return target ? (100 - static_cast<uint32>(target->GetHealthPct())) : 0;
}

uint32 EvokerCalculator::CalculateBuffEfficiency(uint32 spellId, Player* caster, ::Unit* target)
{
    return 100; // Placeholder
}

::Unit* EvokerCalculator::GetOptimalAugmentationTarget(Player* caster, const std::vector<::Unit*>& allies)
{
    return allies.empty() ? nullptr : allies[0]; // Placeholder
}

void EvokerCalculator::CacheEvokerData()
{
    // Cache implementation
}

// EssenceManager implementation
EssenceManager::EssenceManager(EvokerAI* owner) : _owner(owner), _lastUpdate(0), _updateInterval(1500)
{
}

void EssenceManager::Update(uint32 diff)
{
    _lastUpdate += diff;
    if (_lastUpdate >= _updateInterval)
    {
        UpdateEssenceRegeneration();
        _lastUpdate = 0;
    }
}

void EssenceManager::GenerateEssence(uint32 amount)
{
    _essence.GenerateEssence(amount);
}

void EssenceManager::SpendEssence(uint32 amount)
{
    _essence.SpendEssence(amount);
}

bool EssenceManager::HasEssence(uint32 required) const
{
    return _essence.HasEssence(required);
}

uint32 EssenceManager::GetEssence() const
{
    return _essence.current;
}

float EssenceManager::GetEssencePercent() const
{
    return _essence.maximum > 0 ? (float)_essence.current / _essence.maximum : 0.0f;
}

void EssenceManager::UpdateEssenceRegeneration()
{
    if (_essence.isRegenerating && _essence.current < _essence.maximum)
    {
        GenerateEssence(1);
    }
}

void EssenceManager::OptimizeEssenceUsage()
{
    // Optimization logic
}

bool EssenceManager::ShouldConserveEssence() const
{
    return GetEssencePercent() < 0.3f;
}

uint32 EssenceManager::GetOptimalEssenceLevel() const
{
    return _essence.maximum / 2; // Keep around 50%
}

// EmpowermentController implementation
EmpowermentController::EmpowermentController(EvokerAI* owner) : _owner(owner), _lastUpdate(0)
{
}

void EmpowermentController::Update(uint32 diff)
{
    if (IsChanneling())
    {
        UpdateChanneling();
    }
}

void EmpowermentController::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target)
{
    _currentSpell = EmpoweredSpell(spellId, targetLevel, target);
}

void EmpowermentController::UpdateChanneling()
{
    UpdateEmpowermentLevel();

    if (ShouldReleaseSpell())
        ReleaseSpell();
}

bool EmpowermentController::ShouldReleaseSpell()
{
    return _currentSpell.ShouldRelease();
}

void EmpowermentController::ReleaseSpell()
{
    // Release the empowered spell
    _currentSpell = EmpoweredSpell();
}

bool EmpowermentController::IsChanneling() const
{
    return _currentSpell.isChanneling;
}

EmpowermentLevel EmpowermentController::GetCurrentLevel() const
{
    return _currentSpell.currentLevel;
}

uint32 EmpowermentController::GetChannelTime() const
{
    return _currentSpell.GetChannelTime();
}

uint32 EmpowermentController::GetSpellId() const
{
    return _currentSpell.spellId;
}

EmpowermentLevel EmpowermentController::CalculateOptimalLevel(uint32 spellId, ::Unit* target)
{
    return EmpowermentLevel::RANK_2; // Placeholder
}

bool EmpowermentController::ShouldEmpowerSpell(uint32 spellId)
{
    return true; // Placeholder
}

void EmpowermentController::UpdateEmpowermentLevel()
{
    // Update empowerment level based on channel time
    uint32 channelTime = GetChannelTime();
    if (channelTime >= 4000)
        _currentSpell.currentLevel = EmpowermentLevel::RANK_4;
    else if (channelTime >= 3000)
        _currentSpell.currentLevel = EmpowermentLevel::RANK_3;
    else if (channelTime >= 2000)
        _currentSpell.currentLevel = EmpowermentLevel::RANK_2;
    else if (channelTime >= 1000)
        _currentSpell.currentLevel = EmpowermentLevel::RANK_1;
}

uint32 EmpowermentController::GetRequiredChannelTime(EmpowermentLevel level)
{
    return static_cast<uint32>(level) * 1000;
}

float EmpowermentController::CalculateEmpowermentValue(EmpowermentLevel level)
{
    return 1.0f + static_cast<float>(level) * 0.25f;
}

// EchoController implementation
EchoController::EchoController(EvokerAI* owner) : _owner(owner), _lastUpdate(0), _maxEchoes(8)
{
}

void EchoController::Update(uint32 diff)
{
    _lastUpdate += diff;
    if (_lastUpdate >= 2000) // Update every 2 seconds
    {
        ProcessEchoHealing();
        RemoveExpiredEchoes();
        _lastUpdate = 0;
    }
}

void EchoController::CreateEcho(::Unit* target, uint32 healAmount, uint32 numHeals)
{
    if (_echoes.size() >= _maxEchoes)
        return;

    _echoes.emplace_back(target, numHeals, healAmount);
}

void EchoController::ProcessEchoHealing()
{
    for (auto& echo : _echoes)
    {
        if (echo.ShouldHeal())
        {
            echo.ProcessHeal();
        }
    }
}

void EchoController::RemoveExpiredEchoes()
{
    _echoes.erase(
        std::remove_if(_echoes.begin(), _echoes.end(),
            [](const Echo& echo) { return echo.remainingHeals == 0; }),
        _echoes.end());
}

uint32 EchoController::GetActiveEchoCount() const
{
    return static_cast<uint32>(_echoes.size());
}

bool EchoController::HasEcho(::Unit* target) const
{
    return std::any_of(_echoes.begin(), _echoes.end(),
        [target](const Echo& echo) { return echo.target == target; });
}

bool EchoController::ShouldCreateEcho(::Unit* target) const
{
    return target && !HasEcho(target) && target->GetHealthPct() < 80.0f;
}

::Unit* EchoController::GetBestEchoTarget() const
{
    return nullptr; // Placeholder
}

void EchoController::UpdateEchoStates()
{
    // Update echo states
}

bool EchoController::IsEchoExpired(const Echo& echo) const
{
    return echo.remainingHeals == 0 || !echo.target;
}

uint32 EchoController::CalculateEchoValue(::Unit* target) const
{
    return target ? (100 - static_cast<uint32>(target->GetHealthPct())) : 0;
}

void EchoController::OptimizeEchoPlacement()
{
    // Echo optimization logic
}

// Cache static variables
std::unordered_map<uint32, uint32> EvokerCalculator::_damageCache;
std::unordered_map<uint32, uint32> EvokerCalculator::_healingCache;
std::unordered_map<EmpowermentLevel, uint32> EvokerCalculator::_empowermentCache;
std::mutex EvokerCalculator::_cacheMutex;

} // namespace Playerbot