/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AugmentationSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"
#include "Group.h"

namespace Playerbot
{

AugmentationSpecialization::AugmentationSpecialization(Player* bot)
    : EvokerSpecialization(bot), _augmentationPhase(AugmentationRotationPhase::BUFF_APPLICATION),
      _lastEbonMightTime(0), _lastPrescienceTime(0), _lastBreathOfEonsTime(0), _lastBlisteryScalesTime(0),
      _lastBuffRefreshTime(0), _prioritizeBuffs(true), _optimizeBuffDistribution(true),
      _maxBuffTargets(4), _buffEfficiencyThreshold(0.8f)
{
    // Initialize buff abilities in priority order
    _buffAbilities = {EBON_MIGHT, PRESCIENCE, BLISTERY_SCALES, REACTIVE_HIDE};

    // Initialize empowered abilities
    _empoweredAbilities = {BREATH_OF_EONS_EMPOWERED};

    // Initialize damage abilities
    _damageAbilities = {LIVING_FLAME, AZURE_STRIKE, DISINTEGRATE};

    // Initialize utility abilities
    _utilityAbilities = {SPATIAL_PARADOX, TREMBLING_EARTH, VOLCANIC_UPSURGE};

    TC_LOG_DEBUG("playerbot", "AugmentationSpecialization: Initialized for bot {}", _bot->GetName());
}

void AugmentationSpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot)
        return;

    // Update all management systems
    UpdateResourceStates();
    UpdateTargetInfo(target);
    UpdateBuffManagement();
    UpdateEbonMightTracking();
    UpdatePrescienceTracking();
    UpdateBuffPriorities();
    UpdateEssenceManagement();
    UpdateEmpowermentSystem();
    UpdateAspectManagement();
    UpdateCombatPhase();
    UpdateAugmentationMetrics();

    // Execute rotation based on current phase
    switch (_augmentationPhase)
    {
        case AugmentationRotationPhase::BUFF_APPLICATION:
            ExecuteBuffApplication(target);
            break;
        case AugmentationRotationPhase::EBON_MIGHT_MANAGEMENT:
            ExecuteEbonMightManagement(target);
            break;
        case AugmentationRotationPhase::PRESCIENCE_DISTRIBUTION:
            ExecutePrescienceDistribution(target);
            break;
        case AugmentationRotationPhase::BREATH_OF_EONS_SETUP:
            ExecuteBreathOfEonsSetup(target);
            break;
        case AugmentationRotationPhase::DAMAGE_CONTRIBUTION:
            ExecuteDamageContribution(target);
            break;
        case AugmentationRotationPhase::BUFF_REFRESH:
            ExecuteBuffRefresh(target);
            break;
        case AugmentationRotationPhase::UTILITY_SUPPORT:
            ExecuteUtilitySupport(target);
            break;
        case AugmentationRotationPhase::EMERGENCY_SUPPORT:
            ExecuteEmergencySupport(target);
            break;
    }

    AnalyzeBuffEfficiency();
}

void AugmentationSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain optimal aspect
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _aspect.current && CanShiftAspect())
    {
        ShiftToAspect(optimalAspect);
    }

    // Apply and refresh buffs
    ApplyOptimalBuffs();
    RefreshExpiredBuffs();
}

void AugmentationSpecialization::UpdateCooldowns(uint32 diff)
{
    EvokerSpecialization::UpdateResourceStates();

    // Update buff timers
    for (auto& buff : _activeBuffs)
    {
        if (buff.isActive)
        {
            if (buff.timeRemaining > diff)
                buff.timeRemaining -= diff;
            else
            {
                buff.isActive = false;
                buff.timeRemaining = 0;
            }
        }
    }

    // Remove expired buffs
    _activeBuffs.erase(
        std::remove_if(_activeBuffs.begin(), _activeBuffs.end(),
                      [](const AugmentationBuffInfo& buff) { return !buff.isActive; }),
        _activeBuffs.end());
}

bool AugmentationSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check if we're channeling an empowered spell
    if (IsChannelingEmpoweredSpell() && spellId != _currentEmpoweredSpell.spellId)
        return false;

    return true;
}

void AugmentationSpecialization::OnCombatStart(::Unit* target)
{
    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = AugmentationMetrics();

    // Start with buff application phase
    _augmentationPhase = AugmentationRotationPhase::BUFF_APPLICATION;
    LogAugmentationDecision("Combat Start", "Beginning buff distribution");

    // Ensure we have optimal aspect
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _aspect.current && CanShiftAspect())
    {
        ShiftToAspect(optimalAspect);
    }
}

void AugmentationSpecialization::OnCombatEnd()
{
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "AugmentationSpecialization [{}]: Combat ended. Duration: {}ms, Buffs applied: {}, Damage contributed: {}",
                _bot->GetName(), combatDuration, _metrics.totalBuffsApplied, _metrics.totalDamageContributed);

    // Reset phases and state
    _augmentationPhase = AugmentationRotationPhase::BUFF_APPLICATION;
    _activeBuffs.clear();
    _currentTarget = nullptr;
}

bool AugmentationSpecialization::HasEnoughResource(uint32 spellId)
{
    uint32 essenceCost = GetEssenceCost(spellId);
    return GetEssence() >= essenceCost;
}

void AugmentationSpecialization::ConsumeResource(uint32 spellId)
{
    uint32 essenceCost = GetEssenceCost(spellId);
    if (essenceCost > 0)
    {
        SpendEssence(essenceCost);
    }
}

Position AugmentationSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!_bot)
        return Position();

    // Augmentation prefers central positioning to buff all allies
    if (Group* group = _bot->GetGroup())
    {
        // Calculate center position of group
        float avgX = 0.0f, avgY = 0.0f, avgZ = 0.0f;
        uint32 memberCount = 0;

        for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
        {
            if (Player* member = groupRef->GetSource())
            {
                if (member != _bot && _bot->IsWithinDistInMap(member, 100.0f))
                {
                    avgX += member->GetPositionX();
                    avgY += member->GetPositionY();
                    avgZ += member->GetPositionZ();
                    memberCount++;
                }
            }
        }

        if (memberCount > 0)
        {
            avgX /= memberCount;
            avgY /= memberCount;
            avgZ /= memberCount;
            return Position(avgX, avgY, avgZ, _bot->GetOrientation());
        }
    }

    // Fallback to medium range from target
    if (target)
    {
        float angle = target->GetOrientation() + (M_PI / 4); // 45 degrees
        float distance = 20.0f;

        float x = target->GetPositionX() + cos(angle) * distance;
        float y = target->GetPositionY() + sin(angle) * distance;
        float z = target->GetPositionZ();

        return Position(x, y, z, angle);
    }

    return Position();
}

float AugmentationSpecialization::GetOptimalRange(::Unit* target)
{
    return 30.0f; // Buff range
}

// Resource and system implementations
void AugmentationSpecialization::UpdateEssenceManagement() { EvokerSpecialization::UpdateEssenceManagement(); }
bool AugmentationSpecialization::HasEssence(uint32 required) { return EvokerSpecialization::HasEssence(required); }
uint32 AugmentationSpecialization::GetEssence() { return EvokerSpecialization::GetEssence(); }
void AugmentationSpecialization::SpendEssence(uint32 amount) { EvokerSpecialization::SpendEssence(amount); }
void AugmentationSpecialization::GenerateEssence(uint32 amount) { EvokerSpecialization::GenerateEssence(amount); }

bool AugmentationSpecialization::ShouldConserveEssence()
{
    // Conserve essence for buff application
    if (_prioritizeBuffs && _essence.state <= EssenceState::MEDIUM)
        return true;

    return EvokerSpecialization::ShouldConserveEssence();
}

void AugmentationSpecialization::UpdateEmpowermentSystem() { EvokerSpecialization::UpdateEmpowermentSystem(); }
void AugmentationSpecialization::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target) { EvokerSpecialization::StartEmpoweredSpell(spellId, targetLevel, target); }
void AugmentationSpecialization::UpdateEmpoweredChanneling() { EvokerSpecialization::UpdateEmpoweredChanneling(); }
void AugmentationSpecialization::ReleaseEmpoweredSpell() { EvokerSpecialization::ReleaseEmpoweredSpell(); }

EmpowermentLevel AugmentationSpecialization::CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target)
{
    if (spellId == BREATH_OF_EONS_EMPOWERED)
    {
        // Base level 3 for Breath of Eons
        EmpowermentLevel level = EmpowermentLevel::RANK_3;

        // Adjust based on essence state
        if (_essence.state <= EssenceState::LOW)
            level = EmpowermentLevel::RANK_1;
        else if (_essence.state >= EssenceState::HIGH)
            level = EmpowermentLevel::RANK_4;

        return level;
    }

    return EmpowermentLevel::RANK_2;
}

bool AugmentationSpecialization::ShouldEmpowerSpell(uint32 spellId)
{
    // Empower Breath of Eons when group needs damage boost
    if (spellId == BREATH_OF_EONS_EMPOWERED)
        return _essence.state >= EssenceState::MEDIUM;

    return false;
}

void AugmentationSpecialization::UpdateAspectManagement() { EvokerSpecialization::UpdateAspectManagement(); }
void AugmentationSpecialization::ShiftToAspect(EvokerAspect aspect) { EvokerSpecialization::ShiftToAspect(aspect); }

EvokerAspect AugmentationSpecialization::GetOptimalAspect()
{
    // Bronze aspect for utility and support
    if (_augmentationPhase == AugmentationRotationPhase::UTILITY_SUPPORT)
        return EvokerAspect::BRONZE;

    // Azure aspect for essence management
    if (_essence.state <= EssenceState::LOW)
        return EvokerAspect::AZURE;

    // Red aspect for damage contribution
    if (_augmentationPhase == AugmentationRotationPhase::DAMAGE_CONTRIBUTION)
        return EvokerAspect::RED;

    // Default to Bronze for utility
    return EvokerAspect::BRONZE;
}

bool AugmentationSpecialization::CanShiftAspect() { return EvokerSpecialization::CanShiftAspect(); }

void AugmentationSpecialization::UpdateCombatPhase()
{
    if (!_bot)
        return;

    // Emergency support takes priority
    if (_bot->GetHealthPct() < 30.0f)
    {
        _augmentationPhase = AugmentationRotationPhase::EMERGENCY_SUPPORT;
        return;
    }

    // Check if buffs need refresh
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBuffRefreshTime > BUFF_REFRESH_INTERVAL)
    {
        _augmentationPhase = AugmentationRotationPhase::BUFF_REFRESH;
        return;
    }

    // Ebon Might management
    ::Unit* ebonTarget = GetBestEbonMightTarget();
    if (ebonTarget && NeedsEbonMight(ebonTarget))
    {
        _augmentationPhase = AugmentationRotationPhase::EBON_MIGHT_MANAGEMENT;
        return;
    }

    // Prescience distribution
    ::Unit* prescienceTarget = GetBestPrescienceTarget();
    if (prescienceTarget && NeedsPrescience(prescienceTarget))
    {
        _augmentationPhase = AugmentationRotationPhase::PRESCIENCE_DISTRIBUTION;
        return;
    }

    // Breath of Eons setup
    if (_essence.state >= EssenceState::HIGH)
    {
        _augmentationPhase = AugmentationRotationPhase::BREATH_OF_EONS_SETUP;
        return;
    }

    // Default to damage contribution
    _augmentationPhase = AugmentationRotationPhase::DAMAGE_CONTRIBUTION;
}

CombatPhase AugmentationSpecialization::GetCurrentPhase()
{
    switch (_augmentationPhase)
    {
        case AugmentationRotationPhase::BREATH_OF_EONS_SETUP:
            return CombatPhase::EMPOWERMENT_WINDOW;
        case AugmentationRotationPhase::EMERGENCY_SUPPORT:
            return CombatPhase::EMERGENCY;
        case AugmentationRotationPhase::DAMAGE_CONTRIBUTION:
            return CombatPhase::SUSTAIN_PHASE;
        default:
            return CombatPhase::SUSTAIN_PHASE;
    }
}

bool AugmentationSpecialization::ShouldExecuteBurstRotation()
{
    return _essence.state >= EssenceState::HIGH && HasSpell(BREATH_OF_EONS_EMPOWERED);
}

::Unit* AugmentationSpecialization::GetBestTarget()
{
    // Prioritize buff targets over damage targets
    ::Unit* buffTarget = GetBestEbonMightTarget();
    if (buffTarget)
        return buffTarget;

    // Fallback to damage target
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    return enemies.empty() ? nullptr : enemies[0];
}

std::vector<::Unit*> AugmentationSpecialization::GetEmpoweredSpellTargets(uint32 spellId)
{
    std::vector<::Unit*> targets;

    if (spellId == BREATH_OF_EONS_EMPOWERED)
    {
        // Get all damage dealers in group
        targets = GetDamageDealer();
    }

    return targets;
}

// Phase execution methods
void AugmentationSpecialization::ExecuteBuffApplication(::Unit* target)
{
    ApplyOptimalBuffs();
    _augmentationPhase = AugmentationRotationPhase::EBON_MIGHT_MANAGEMENT;
}

void AugmentationSpecialization::ExecuteEbonMightManagement(::Unit* target)
{
    ::Unit* ebonTarget = GetBestEbonMightTarget();
    if (ebonTarget && HasEssence(2))
    {
        CastEbonMight(ebonTarget);
    }

    _augmentationPhase = AugmentationRotationPhase::PRESCIENCE_DISTRIBUTION;
}

void AugmentationSpecialization::ExecutePrescienceDistribution(::Unit* target)
{
    ::Unit* prescienceTarget = GetBestPrescienceTarget();
    if (prescienceTarget && HasEssence(2))
    {
        CastPrescience(prescienceTarget);
    }

    _augmentationPhase = AugmentationRotationPhase::DAMAGE_CONTRIBUTION;
}

void AugmentationSpecialization::ExecuteBreathOfEonsSetup(::Unit* target)
{
    if (HasSpell(BREATH_OF_EONS_EMPOWERED) && HasEssence(4))
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(BREATH_OF_EONS_EMPOWERED, target);
        CastEmpoweredBreathOfEons(target, level);
    }

    _augmentationPhase = AugmentationRotationPhase::DAMAGE_CONTRIBUTION;
}

void AugmentationSpecialization::ExecuteDamageContribution(::Unit* target)
{
    ContributeDamageAsAugmentation(target);
    _augmentationPhase = AugmentationRotationPhase::BUFF_APPLICATION;
}

void AugmentationSpecialization::ExecuteBuffRefresh(::Unit* target)
{
    RefreshExpiredBuffs();
    _lastBuffRefreshTime = getMSTime();
    _augmentationPhase = AugmentationRotationPhase::BUFF_APPLICATION;
}

void AugmentationSpecialization::ExecuteUtilitySupport(::Unit* target)
{
    ProvideUtilitySupport();
    _augmentationPhase = AugmentationRotationPhase::DAMAGE_CONTRIBUTION;
}

void AugmentationSpecialization::ExecuteEmergencySupport(::Unit* target)
{
    HandleEmergencySupport();

    if (_bot->GetHealthPct() > 50.0f)
        _augmentationPhase = AugmentationRotationPhase::BUFF_APPLICATION;
}

// Core ability implementations
void AugmentationSpecialization::CastEbonMight(::Unit* target)
{
    if (CastSpell(EBON_MIGHT, target))
    {
        _metrics.ebonMightApplications++;
        _lastEbonMightTime = getMSTime();

        // Track the buff
        _activeBuffs.emplace_back(target, EBON_MIGHT);
        _activeBuffs.back().timeRemaining = EBON_MIGHT_DURATION;

        LogAugmentationDecision("Cast Ebon Might", "Damage amplification buff");
    }
}

void AugmentationSpecialization::CastPrescience(::Unit* target)
{
    if (CastSpell(PRESCIENCE, target))
    {
        _metrics.prescienceApplications++;
        _lastPrescienceTime = getMSTime();

        // Track the buff
        _activeBuffs.emplace_back(target, PRESCIENCE);
        _activeBuffs.back().timeRemaining = PRESCIENCE_DURATION;

        LogAugmentationDecision("Cast Prescience", "Critical strike buff");
    }
}

void AugmentationSpecialization::CastBlisteryScales(::Unit* target)
{
    if (CastSpell(BLISTERY_SCALES, target))
    {
        _metrics.blisteryScalesApplications++;
        _lastBlisteryScalesTime = getMSTime();

        // Track the buff
        _activeBuffs.emplace_back(target, BLISTERY_SCALES);
        _activeBuffs.back().timeRemaining = BLISTERY_SCALES_DURATION;

        LogAugmentationDecision("Cast Blistery Scales", "Defensive buff");
    }
}

void AugmentationSpecialization::CastEmpoweredBreathOfEons(::Unit* target, EmpowermentLevel level)
{
    if (ShouldEmpowerSpell(BREATH_OF_EONS_EMPOWERED))
    {
        StartEmpoweredSpell(BREATH_OF_EONS_EMPOWERED, level, target);
        _metrics.breathOfEonsCasts++;
        _lastBreathOfEonsTime = getMSTime();
        LogAugmentationDecision("Started Empowered Breath of Eons", "Level " + std::to_string(static_cast<uint8>(level)));
    }
}

// Buff management implementations
void AugmentationSpecialization::ApplyOptimalBuffs()
{
    DistributeBuffsOptimally();
}

void AugmentationSpecialization::RefreshExpiredBuffs()
{
    uint32 currentTime = getMSTime();

    for (auto& buff : _activeBuffs)
    {
        if (buff.isActive && buff.timeRemaining < (GetSpellCooldown(buff.spellId) * BUFF_REFRESH_THRESHOLD))
        {
            // Refresh buff
            if (buff.spellId == EBON_MIGHT && HasEssence(2))
            {
                CastEbonMight(buff.target);
            }
            else if (buff.spellId == PRESCIENCE && HasEssence(2))
            {
                CastPrescience(buff.target);
            }
        }
    }
}

void AugmentationSpecialization::DistributeBuffsOptimally()
{
    std::vector<::Unit*> buffTargets = GetBuffTargets();

    for (::Unit* target : buffTargets)
    {
        if (NeedsEbonMight(target) && HasEssence(2))
        {
            CastEbonMight(target);
            break; // One at a time
        }
        else if (NeedsPrescience(target) && HasEssence(2))
        {
            CastPrescience(target);
            break;
        }
    }
}

::Unit* AugmentationSpecialization::GetBestEbonMightTarget()
{
    std::vector<::Unit*> damageDealer = GetDamageDealer();

    for (::Unit* ally : damageDealer)
    {
        if (NeedsEbonMight(ally))
            return ally;
    }

    return nullptr;
}

::Unit* AugmentationSpecialization::GetBestPrescienceTarget()
{
    std::vector<::Unit*> damageDealer = GetDamageDealer();

    for (::Unit* ally : damageDealer)
    {
        if (NeedsPrescience(ally))
            return ally;
    }

    return nullptr;
}

std::vector<::Unit*> AugmentationSpecialization::GetBuffTargets()
{
    return GetNearbyAllies(30.0f);
}

std::vector<::Unit*> AugmentationSpecialization::GetDamageDealer()
{
    std::vector<::Unit*> damageDealer;
    std::vector<::Unit*> allies = GetNearbyAllies(30.0f);

    for (::Unit* ally : allies)
    {
        // Simplified damage dealer detection - assume all players can deal damage
        if (ally->GetTypeId() == TYPEID_PLAYER)
        {
            damageDealer.push_back(ally);
        }
    }

    return damageDealer;
}

bool AugmentationSpecialization::NeedsEbonMight(::Unit* target)
{
    if (!target)
        return false;

    // Check if target already has Ebon Might
    for (const auto& buff : _activeBuffs)
    {
        if (buff.target == target && buff.spellId == EBON_MIGHT && buff.isActive)
            return false;
    }

    return true;
}

bool AugmentationSpecialization::NeedsPrescience(::Unit* target)
{
    if (!target)
        return false;

    // Check if target already has Prescience
    for (const auto& buff : _activeBuffs)
    {
        if (buff.target == target && buff.spellId == PRESCIENCE && buff.isActive)
            return false;
    }

    return true;
}

bool AugmentationSpecialization::NeedsBlisteryScales(::Unit* target)
{
    if (!target)
        return false;

    return !HasAura(BLISTERY_SCALES, target);
}

void AugmentationSpecialization::ContributeDamageAsAugmentation(::Unit* target)
{
    if (!target)
        return;

    // Use damage abilities when essence allows
    if (HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastSpell(LIVING_FLAME, target);
        _metrics.totalDamageContributed++;
    }
    else if (HasSpell(AZURE_STRIKE) && HasEssence(2))
    {
        CastSpell(AZURE_STRIKE, target);
        _metrics.totalDamageContributed++;
    }
}

bool AugmentationSpecialization::ShouldContributeDamage()
{
    // Contribute damage when buffs are maintained and essence allows
    return _essence.state >= EssenceState::MEDIUM;
}

void AugmentationSpecialization::ProvideUtilitySupport()
{
    // Use utility abilities as needed
    if (HasSpell(SPATIAL_PARADOX) && _essence.state >= EssenceState::HIGH)
    {
        CastSpell(SPATIAL_PARADOX);
    }
}

void AugmentationSpecialization::HandleEmergencySupport()
{
    // Use emergency abilities
    if (_bot->GetHealthPct() < 30.0f)
    {
        if (HasSpell(OBSIDIAN_SCALES))
        {
            CastSpell(OBSIDIAN_SCALES);
        }
        else if (HasSpell(RENEWING_BLAZE))
        {
            CastSpell(RENEWING_BLAZE);
        }
    }
}

// Update methods
void AugmentationSpecialization::UpdateBuffManagement()
{
    // Update active buff tracking
    for (auto& buff : _activeBuffs)
    {
        if (buff.isActive && buff.target)
        {
            bool hasAura = HasAura(buff.spellId, buff.target);
            if (!hasAura)
            {
                buff.isActive = false;
            }
        }
    }
}

void AugmentationSpecialization::UpdateEbonMightTracking()
{
    // Track Ebon Might applications and efficiency
}

void AugmentationSpecialization::UpdatePrescienceTracking()
{
    // Track Prescience applications and efficiency
}

void AugmentationSpecialization::UpdateBuffPriorities()
{
    // Update buff priority based on group composition and combat state
}

void AugmentationSpecialization::UpdateAugmentationMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate uptime percentages
        uint32 activeEbonMight = 0;
        uint32 activePrescience = 0;

        for (const auto& buff : _activeBuffs)
        {
            if (buff.isActive)
            {
                if (buff.spellId == EBON_MIGHT)
                    activeEbonMight++;
                else if (buff.spellId == PRESCIENCE)
                    activePrescience++;
            }
        }

        _metrics.ebonMightUptime = activeEbonMight > 0 ? (_metrics.ebonMightUptime + 1.0f) / 2.0f : _metrics.ebonMightUptime;
        _metrics.prescienceUptime = activePrescience > 0 ? (_metrics.prescienceUptime + 1.0f) / 2.0f : _metrics.prescienceUptime;
        _metrics.averageBuffsActive = static_cast<float>(_activeBuffs.size());
    }
}

void AugmentationSpecialization::AnalyzeBuffEfficiency()
{
    // Performance analysis every 10 seconds
    if (getMSTime() % 10000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "AugmentationSpecialization [{}]: Efficiency - Ebon Might: {:.1f}%, Prescience: {:.1f}%, Avg Buffs: {:.1f}",
                    _bot->GetName(), _metrics.ebonMightUptime * 100, _metrics.prescienceUptime * 100, _metrics.averageBuffsActive);
    }
}

void AugmentationSpecialization::LogAugmentationDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

} // namespace Playerbot