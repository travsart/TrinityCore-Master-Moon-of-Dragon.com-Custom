/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PreservationSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"
#include "Group.h"

namespace Playerbot
{

PreservationSpecialization::PreservationSpecialization(Player* bot)
    : EvokerSpecialization(bot), _preservationPhase(PreservationRotationPhase::ASSESSMENT),
      _lastEmeraldBlossomTime(0), _lastVerdantEmbraceTime(0), _lastDreamBreathTime(0), _lastSpiritBloomTime(0),
      _lastTemporalAnomalyTime(0), _lastRenewingBlazeTime(0), _lastDreamFlightTime(0), _lastReversionTime(0),
      _lastEchoUpdate(0), _prioritizeEchoes(true), _conserveEssenceForEmergencies(true),
      _useGroupHealingOptimization(true), _maxEchoes(ECHO_MAX_COUNT), _healingEfficiencyTarget(HEALING_EFFICIENCY_THRESHOLD)
{
    // Initialize emergency heals in priority order
    _emergencyHeals = {RENEWING_BLAZE, VERDANT_EMBRACE, EMERALD_BLOSSOM};

    // Initialize sustain heals
    _sustainHeals = {REVERSION, VERDANT_EMBRACE, LIVING_FLAME};

    // Initialize group heals
    _groupHeals = {EMERALD_BLOSSOM, DREAM_BREATH_EMPOWERED, SPIRIT_BLOOM_EMPOWERED};

    // Initialize empowered heals
    _empoweredHeals = {DREAM_BREATH_EMPOWERED, SPIRIT_BLOOM_EMPOWERED};

    // Initialize temporal abilities
    _temporalAbilities = {TEMPORAL_ANOMALY, STASIS, TIME_DILATION};

    TC_LOG_DEBUG("playerbot", "PreservationSpecialization: Initialized for bot {}", _bot->GetName());
}

void PreservationSpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot)
        return;

    // Update all management systems
    UpdateResourceStates();
    UpdateEchoManagement();
    UpdateTemporalManagement();
    UpdateCallOfYseraTracking();
    UpdateHealingPriorities();
    UpdateGroupHealthAssessment();
    UpdateEssenceManagement();
    UpdateEmpowermentSystem();
    UpdateAspectManagement();
    UpdateCombatPhase();
    UpdatePreservationMetrics();

    // Execute rotation based on current phase
    switch (_preservationPhase)
    {
        case PreservationRotationPhase::ASSESSMENT:
            ExecuteAssessmentPhase(target);
            break;
        case PreservationRotationPhase::EMERGENCY_HEALING:
            ExecuteEmergencyHealing(target);
            break;
        case PreservationRotationPhase::ECHO_MANAGEMENT:
            ExecuteEchoManagement(target);
            break;
        case PreservationRotationPhase::EMPOWERED_HEALING:
            ExecuteEmpoweredHealing(target);
            break;
        case PreservationRotationPhase::SUSTAIN_HEALING:
            ExecuteSustainHealing(target);
            break;
        case PreservationRotationPhase::TEMPORAL_ABILITIES:
            ExecuteTemporalAbilities(target);
            break;
        case PreservationRotationPhase::GROUP_HEALING:
            ExecuteGroupHealing(target);
            break;
        case PreservationRotationPhase::DAMAGE_CONTRIBUTION:
            ExecuteDamageContribution(target);
            break;
        case PreservationRotationPhase::RESOURCE_RECOVERY:
            ExecuteResourceRecovery(target);
            break;
    }

    ProcessEchoHealing();
    AnalyzeHealingEfficiency();
}

void PreservationSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain optimal aspect for healing
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _aspect.current && CanShiftAspect())
    {
        ShiftToAspect(optimalAspect);
    }

    // Use Hover for positioning
    if (!HasAura(HOVER) && IsAtOptimalHealingRange())
    {
        CastSpell(HOVER);
    }
}

void PreservationSpecialization::UpdateCooldowns(uint32 diff)
{
    EvokerSpecialization::UpdateResourceStates();

    // Update echo timers
    for (auto& echo : _activeEchoes)
    {
        if (echo.isActive)
        {
            if (getMSTime() - echo.creationTime > ECHO_DURATION)
            {
                echo.isActive = false;
            }
        }
    }

    RemoveExpiredEchoes();
}

bool PreservationSpecialization::CanUseAbility(uint32 spellId)
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

void PreservationSpecialization::OnCombatStart(::Unit* target)
{
    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = PreservationMetrics();

    // Start with assessment phase
    _preservationPhase = PreservationRotationPhase::ASSESSMENT;
    LogPreservationDecision("Combat Start", "Beginning healing assessment");

    // Ensure we have optimal aspect
    EvokerAspect optimalAspect = GetOptimalAspect();
    if (optimalAspect != _aspect.current && CanShiftAspect())
    {
        ShiftToAspect(optimalAspect);
    }
}

void PreservationSpecialization::OnCombatEnd()
{
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "PreservationSpecialization [{}]: Combat ended. Duration: {}ms, HPS: {:.1f}, Echoes: {}",
                _bot->GetName(), combatDuration, _metrics.averageHealingPerSecond, _metrics.echoesCreated);

    // Reset phases and state
    _preservationPhase = PreservationRotationPhase::ASSESSMENT;
    _activeEchoes.clear();
    _currentTarget = nullptr;
}

bool PreservationSpecialization::HasEnoughResource(uint32 spellId)
{
    uint32 essenceCost = GetEssenceCost(spellId);
    return GetEssence() >= essenceCost;
}

void PreservationSpecialization::ConsumeResource(uint32 spellId)
{
    uint32 essenceCost = GetEssenceCost(spellId);
    if (essenceCost > 0)
    {
        SpendEssence(essenceCost);
    }
}

Position PreservationSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!_bot)
        return Position();

    // Preservation prefers central positioning to reach all allies
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
        float angle = target->GetOrientation() + M_PI; // Behind target
        float distance = 20.0f;

        float x = target->GetPositionX() + cos(angle) * distance;
        float y = target->GetPositionY() + sin(angle) * distance;
        float z = target->GetPositionZ();

        return Position(x, y, z, angle);
    }

    return Position();
}

float PreservationSpecialization::GetOptimalRange(::Unit* target)
{
    return 30.0f; // Healing range
}

// Implementation stubs for core interface
void PreservationSpecialization::UpdateEssenceManagement() { EvokerSpecialization::UpdateEssenceManagement(); }
bool PreservationSpecialization::HasEssence(uint32 required) { return EvokerSpecialization::HasEssence(required); }
uint32 PreservationSpecialization::GetEssence() { return EvokerSpecialization::GetEssence(); }
void PreservationSpecialization::SpendEssence(uint32 amount) { EvokerSpecialization::SpendEssence(amount); }
void PreservationSpecialization::GenerateEssence(uint32 amount) { EvokerSpecialization::GenerateEssence(amount); }

bool PreservationSpecialization::ShouldConserveEssence()
{
    // Conserve essence for emergencies
    if (_conserveEssenceForEmergencies && _essence.state <= EssenceState::MEDIUM)
    {
        std::vector<::Unit*> criticalAllies = GetAlliesNeedingHealing(EMERGENCY_HEALTH_THRESHOLD);
        if (!criticalAllies.empty())
            return false; // Don't conserve if someone needs emergency healing
        return true;
    }

    return EvokerSpecialization::ShouldConserveEssence();
}

void PreservationSpecialization::UpdateEmpowermentSystem() { EvokerSpecialization::UpdateEmpowermentSystem(); }
void PreservationSpecialization::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target) { EvokerSpecialization::StartEmpoweredSpell(spellId, targetLevel, target); }
void PreservationSpecialization::UpdateEmpoweredChanneling() { EvokerSpecialization::UpdateEmpoweredChanneling(); }
void PreservationSpecialization::ReleaseEmpoweredSpell() { EvokerSpecialization::ReleaseEmpoweredSpell(); }

EmpowermentLevel PreservationSpecialization::CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target)
{
    if (!target)
        return EmpowermentLevel::RANK_1;

    // Calculate based on healing needed
    std::vector<::Unit*> healTargets = GetGroupHealTargets(target, 30.0f);
    uint32 targetCount = healTargets.size();

    // Base empowerment level
    EmpowermentLevel level = EmpowermentLevel::RANK_2;

    // Adjust based on essence state
    if (_essence.state <= EssenceState::LOW)
        level = EmpowermentLevel::RANK_1;
    else if (_essence.state >= EssenceState::HIGH)
        level = EmpowermentLevel::RANK_3;

    // Boost for group healing
    if (targetCount >= 3)
        level = static_cast<EmpowermentLevel>(std::min(4, static_cast<int>(level) + 1));

    return level;
}

bool PreservationSpecialization::ShouldEmpowerSpell(uint32 spellId)
{
    // Always empower healing spells when group needs healing
    if (ShouldUseGroupHealing())
        return true;

    // Empower based on essence availability
    return _essence.state >= EssenceState::MEDIUM;
}

void PreservationSpecialization::UpdateAspectManagement() { EvokerSpecialization::UpdateAspectManagement(); }
void PreservationSpecialization::ShiftToAspect(EvokerAspect aspect) { EvokerSpecialization::ShiftToAspect(aspect); }

EvokerAspect PreservationSpecialization::GetOptimalAspect()
{
    // Green aspect for healing in most situations
    if (ShouldUseGroupHealing() || _preservationPhase == PreservationRotationPhase::EMERGENCY_HEALING)
        return EvokerAspect::GREEN;

    // Bronze aspect for temporal abilities
    if (_preservationPhase == PreservationRotationPhase::TEMPORAL_ABILITIES)
        return EvokerAspect::BRONZE;

    // Default to Green for healing
    return EvokerAspect::GREEN;
}

bool PreservationSpecialization::CanShiftAspect() { return EvokerSpecialization::CanShiftAspect(); }

void PreservationSpecialization::UpdateCombatPhase()
{
    if (!_bot)
        return;

    // Emergency healing takes priority
    if (ShouldUseEmergencyHealing())
    {
        _preservationPhase = PreservationRotationPhase::EMERGENCY_HEALING;
        return;
    }

    // Group healing phase
    if (ShouldUseGroupHealing())
    {
        _preservationPhase = PreservationRotationPhase::GROUP_HEALING;
        return;
    }

    // Echo management
    if (_prioritizeEchoes && GetActiveEchoCount() < _maxEchoes)
    {
        _preservationPhase = PreservationRotationPhase::ECHO_MANAGEMENT;
        return;
    }

    // Empowered healing
    if (_essence.state >= EssenceState::HIGH)
    {
        _preservationPhase = PreservationRotationPhase::EMPOWERED_HEALING;
        return;
    }

    // Default to sustain healing
    _preservationPhase = PreservationRotationPhase::SUSTAIN_HEALING;
}

CombatPhase PreservationSpecialization::GetCurrentPhase()
{
    switch (_preservationPhase)
    {
        case PreservationRotationPhase::EMERGENCY_HEALING:
            return CombatPhase::EMERGENCY;
        case PreservationRotationPhase::EMPOWERED_HEALING:
            return CombatPhase::EMPOWERMENT_WINDOW;
        case PreservationRotationPhase::GROUP_HEALING:
            return CombatPhase::AOE_PHASE;
        default:
            return CombatPhase::SUSTAIN_PHASE;
    }
}

bool PreservationSpecialization::ShouldExecuteBurstRotation()
{
    return ShouldUseEmergencyHealing() || ShouldUseGroupHealing();
}

::Unit* PreservationSpecialization::GetBestTarget()
{
    return GetBestHealTarget();
}

std::vector<::Unit*> PreservationSpecialization::GetEmpoweredSpellTargets(uint32 spellId)
{
    std::vector<::Unit*> targets;

    if (spellId == DREAM_BREATH_EMPOWERED || spellId == SPIRIT_BLOOM_EMPOWERED)
    {
        // Get allies needing healing
        targets = GetAlliesNeedingHealing(GROUP_HEAL_THRESHOLD);
    }

    return targets;
}

// Phase execution methods
void PreservationSpecialization::ExecuteAssessmentPhase(::Unit* target)
{
    UpdateHealingPriorities();

    // Transition based on assessment
    if (ShouldUseEmergencyHealing())
        _preservationPhase = PreservationRotationPhase::EMERGENCY_HEALING;
    else if (ShouldUseGroupHealing())
        _preservationPhase = PreservationRotationPhase::GROUP_HEALING;
    else
        _preservationPhase = PreservationRotationPhase::SUSTAIN_HEALING;
}

void PreservationSpecialization::ExecuteEmergencyHealing(::Unit* target)
{
    ::Unit* criticalTarget = GetMostInjuredAlly();
    if (criticalTarget)
    {
        if (HasSpell(RENEWING_BLAZE) && HasEssence(2))
        {
            CastRenewingBlaze(criticalTarget);
        }
        else if (HasSpell(VERDANT_EMBRACE) && HasEssence(2))
        {
            CastVerdantEmbrace(criticalTarget);
        }
    }

    // Transition when emergency is handled
    if (!ShouldUseEmergencyHealing())
        _preservationPhase = PreservationRotationPhase::ASSESSMENT;
}

void PreservationSpecialization::ExecuteEchoManagement(::Unit* target)
{
    ::Unit* echoTarget = GetBestEchoTarget();
    if (echoTarget && ShouldCreateEcho(echoTarget))
    {
        CreateEcho(echoTarget, 1000, 3); // Create echo with 1000 heal amount, 3 heals
    }

    _preservationPhase = PreservationRotationPhase::SUSTAIN_HEALING;
}

void PreservationSpecialization::ExecuteEmpoweredHealing(::Unit* target)
{
    std::vector<::Unit*> groupTargets = GetGroupHealTargets(target, 30.0f);

    if (groupTargets.size() >= 3)
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(DREAM_BREATH_EMPOWERED, target);
        CastEmpoweredDreamBreath(target, level);
    }
    else
    {
        EmpowermentLevel level = CalculateOptimalEmpowermentLevel(SPIRIT_BLOOM_EMPOWERED, target);
        CastEmpoweredSpiritBloom(target, level);
    }

    _preservationPhase = PreservationRotationPhase::SUSTAIN_HEALING;
}

void PreservationSpecialization::ExecuteSustainHealing(::Unit* target)
{
    ::Unit* healTarget = GetBestHealTarget();
    if (healTarget)
    {
        if (HasSpell(REVERSION) && HasEssence(2))
        {
            CastReversion(healTarget);
        }
        else if (HasSpell(VERDANT_EMBRACE) && HasEssence(2))
        {
            CastVerdantEmbrace(healTarget);
        }
    }
}

void PreservationSpecialization::ExecuteTemporalAbilities(::Unit* target)
{
    if (ShouldUseTemporalAnomaly())
    {
        CastTemporalAnomaly();
    }

    _preservationPhase = PreservationRotationPhase::SUSTAIN_HEALING;
}

void PreservationSpecialization::ExecuteGroupHealing(::Unit* target)
{
    if (HasSpell(EMERALD_BLOSSOM) && HasEssence(2))
    {
        CastEmeraldBlossom();
    }
    else
    {
        ExecuteEmpoweredHealing(target);
    }
}

void PreservationSpecialization::ExecuteDamageContribution(::Unit* target)
{
    if (target && HasSpell(LIVING_FLAME) && HasEssence(2))
    {
        CastSpell(LIVING_FLAME, target);
    }

    _preservationPhase = PreservationRotationPhase::SUSTAIN_HEALING;
}

void PreservationSpecialization::ExecuteResourceRecovery(::Unit* target)
{
    // Wait for essence regeneration
    if (_essence.state >= EssenceState::MEDIUM)
        _preservationPhase = PreservationRotationPhase::ASSESSMENT;
}

// Core healing ability implementations
void PreservationSpecialization::CastEmeraldBlossom()
{
    if (CastSpell(EMERALD_BLOSSOM))
    {
        _metrics.emeraldBlossomCasts++;
        _lastEmeraldBlossomTime = getMSTime();
        LogPreservationDecision("Cast Emerald Blossom", "Group healing");
    }
}

void PreservationSpecialization::CastVerdantEmbrace(::Unit* target)
{
    if (CastSpell(VERDANT_EMBRACE, target))
    {
        _metrics.verdantEmbraceCasts++;
        _lastVerdantEmbraceTime = getMSTime();
        LogPreservationDecision("Cast Verdant Embrace", "Single target heal");
    }
}

void PreservationSpecialization::CastReversion(::Unit* target)
{
    if (CastSpell(REVERSION, target))
    {
        _metrics.reversionCasts++;
        _lastReversionTime = getMSTime();
        LogPreservationDecision("Cast Reversion", "HoT application");
    }
}

void PreservationSpecialization::CastRenewingBlaze(::Unit* target)
{
    if (CastSpell(RENEWING_BLAZE, target))
    {
        _metrics.renewingBlazeCasts++;
        _lastRenewingBlazeTime = getMSTime();
        LogPreservationDecision("Cast Renewing Blaze", "Emergency heal");
    }
}

void PreservationSpecialization::CastEmpoweredDreamBreath(::Unit* target, EmpowermentLevel level)
{
    if (ShouldEmpowerSpell(DREAM_BREATH_EMPOWERED))
    {
        StartEmpoweredSpell(DREAM_BREATH_EMPOWERED, level, target);
        _metrics.dreamBreathCasts++;
        _lastDreamBreathTime = getMSTime();
        LogPreservationDecision("Started Empowered Dream Breath", "Level " + std::to_string(static_cast<uint8>(level)));
    }
}

void PreservationSpecialization::CastEmpoweredSpiritBloom(::Unit* target, EmpowermentLevel level)
{
    if (ShouldEmpowerSpell(SPIRIT_BLOOM_EMPOWERED))
    {
        StartEmpoweredSpell(SPIRIT_BLOOM_EMPOWERED, level, target);
        _metrics.spiritBloomCasts++;
        _lastSpiritBloomTime = getMSTime();
        LogPreservationDecision("Started Empowered Spirit Bloom", "Level " + std::to_string(static_cast<uint8>(level)));
    }
}

void PreservationSpecialization::CastTemporalAnomaly()
{
    if (CastSpell(TEMPORAL_ANOMALY))
    {
        _metrics.temporalAnomalyCasts++;
        _lastTemporalAnomalyTime = getMSTime();
        LogPreservationDecision("Cast Temporal Anomaly", "Group healing boost");
    }
}

// Helper method implementations
::Unit* PreservationSpecialization::GetBestHealTarget()
{
    std::vector<::Unit*> allies = GetNearbyAllies(30.0f);
    ::Unit* bestTarget = nullptr;
    float lowestHealthPct = 100.0f;

    for (::Unit* ally : allies)
    {
        if (ally->GetHealthPct() < lowestHealthPct && ally->GetHealthPct() < 90.0f)
        {
            lowestHealthPct = ally->GetHealthPct();
            bestTarget = ally;
        }
    }

    return bestTarget;
}

::Unit* PreservationSpecialization::GetMostInjuredAlly()
{
    std::vector<::Unit*> allies = GetNearbyAllies(30.0f);
    ::Unit* mostInjured = nullptr;
    float lowestHealthPct = 100.0f;

    for (::Unit* ally : allies)
    {
        if (ally->GetHealthPct() < lowestHealthPct)
        {
            lowestHealthPct = ally->GetHealthPct();
            mostInjured = ally;
        }
    }

    return mostInjured;
}

std::vector<::Unit*> PreservationSpecialization::GetAlliesNeedingHealing(float healthThreshold)
{
    std::vector<::Unit*> needHealing;
    std::vector<::Unit*> allies = GetNearbyAllies(30.0f);

    for (::Unit* ally : allies)
    {
        if (ally->GetHealthPct() < (healthThreshold * 100))
        {
            needHealing.push_back(ally);
        }
    }

    return needHealing;
}

std::vector<::Unit*> PreservationSpecialization::GetGroupHealTargets(::Unit* center, float range)
{
    return GetAlliesNeedingHealing(GROUP_HEAL_THRESHOLD);
}

bool PreservationSpecialization::ShouldUseGroupHealing()
{
    std::vector<::Unit*> injuredAllies = GetAlliesNeedingHealing(GROUP_HEAL_THRESHOLD);
    return injuredAllies.size() >= GROUP_HEAL_COUNT_THRESHOLD;
}

bool PreservationSpecialization::ShouldUseEmergencyHealing()
{
    std::vector<::Unit*> criticalAllies = GetAlliesNeedingHealing(EMERGENCY_HEALTH_THRESHOLD);
    return !criticalAllies.empty();
}

void PreservationSpecialization::CreateEcho(::Unit* target, uint32 healAmount, uint32 numHeals)
{
    if (!target || _activeEchoes.size() >= _maxEchoes)
        return;

    _activeEchoes.emplace_back(target, numHeals, healAmount);
    _metrics.echoesCreated++;
    LogPreservationDecision("Created Echo", "Target: " + target->GetName());
}

void PreservationSpecialization::ProcessEchoHealing()
{
    uint32 currentTime = getMSTime();

    for (auto& echo : _activeEchoes)
    {
        if (echo.isActive && echo.ShouldHeal())
        {
            // Simulate echo healing
            echo.ProcessHeal();
            _metrics.echoHealsPerformed++;

            if (echo.remainingHeals == 0)
            {
                echo.isActive = false;
            }
        }
    }
}

void PreservationSpecialization::RemoveExpiredEchoes()
{
    _activeEchoes.erase(
        std::remove_if(_activeEchoes.begin(), _activeEchoes.end(),
                      [](const EchoTracker& echo) { return !echo.isActive; }),
        _activeEchoes.end());
}

uint32 PreservationSpecialization::GetActiveEchoCount()
{
    uint32 count = 0;
    for (const auto& echo : _activeEchoes)
    {
        if (echo.isActive)
            count++;
    }
    return count;
}

::Unit* PreservationSpecialization::GetBestEchoTarget()
{
    return GetMostInjuredAlly();
}

bool PreservationSpecialization::ShouldCreateEcho(::Unit* target)
{
    return target && GetActiveEchoCount() < _maxEchoes && target->GetHealthPct() < 80.0f;
}

bool PreservationSpecialization::ShouldUseTemporalAnomaly()
{
    return ShouldUseGroupHealing() && HasSpell(TEMPORAL_ANOMALY) && HasEssence(3);
}

bool PreservationSpecialization::IsAtOptimalHealingRange()
{
    std::vector<::Unit*> allies = GetNearbyAllies(30.0f);
    return allies.size() >= 2; // Can reach multiple allies
}

// Update methods
void PreservationSpecialization::UpdateEchoManagement()
{
    ProcessEchoHealing();
    RemoveExpiredEchoes();
}

void PreservationSpecialization::UpdateTemporalManagement()
{
    _temporal.compressionStacks = GetAuraStacks(TEMPORAL_COMPRESSION);
    _temporal.compressionTimeRemaining = GetAuraTimeRemaining(TEMPORAL_COMPRESSION);
    _temporal.anomalyActive = HasAura(TEMPORAL_ANOMALY);
}

void PreservationSpecialization::UpdateCallOfYseraTracking()
{
    _callOfYsera.isActive = HasAura(CALL_OF_YSERA);
    _callOfYsera.stacks = GetAuraStacks(CALL_OF_YSERA);
    _callOfYsera.timeRemaining = GetAuraTimeRemaining(CALL_OF_YSERA);
}

void PreservationSpecialization::UpdateHealingPriorities()
{
    // Update healing target priorities based on current situation
}

void PreservationSpecialization::UpdateGroupHealthAssessment()
{
    // Assess overall group health for phase decisions
}

void PreservationSpecialization::UpdatePreservationMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate HPS
        _metrics.averageHealingPerSecond = static_cast<float>(_totalHealingDone) / (combatTime / 1000.0f);

        // Calculate uptime percentages
        _metrics.echoUptime = (GetActiveEchoCount() > 0) ? (_metrics.echoUptime + 1.0f) / 2.0f : _metrics.echoUptime;
        _metrics.temporalCompressionUptime = _temporal.compressionStacks > 0 ? (_metrics.temporalCompressionUptime + 1.0f) / 2.0f : _metrics.temporalCompressionUptime;
        _metrics.callOfYseraUptime = _callOfYsera.isActive ? (_metrics.callOfYseraUptime + 1.0f) / 2.0f : _metrics.callOfYseraUptime;

        // Calculate healing efficiency
        if (_metrics.totalHealingDone > 0)
        {
            _metrics.healingEfficiency = static_cast<float>(_metrics.totalHealingDone - _metrics.overhealing) / _metrics.totalHealingDone;
        }
    }
}

void PreservationSpecialization::AnalyzeHealingEfficiency()
{
    // Performance analysis every 15 seconds
    if (getMSTime() % 15000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "PreservationSpecialization [{}]: Efficiency - HPS: {:.1f}, Echo uptime: {:.1f}%, Efficiency: {:.1f}%",
                    _bot->GetName(), _metrics.averageHealingPerSecond, _metrics.echoUptime * 100, _metrics.healingEfficiency * 100);
    }
}

void PreservationSpecialization::LogPreservationDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

} // namespace Playerbot