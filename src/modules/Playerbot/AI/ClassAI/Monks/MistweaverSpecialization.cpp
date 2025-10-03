/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MistweaverSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"
#include "Group.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

MistweaverSpecialization::MistweaverSpecialization(Player* bot)
    : MonkSpecialization(bot), _mistweaverPhase(MistweaverRotationPhase::ASSESSMENT),
      _lastVivifyTime(0), _lastEnvelopingMistTime(0), _lastRenewingMistTime(0), _lastEssenceFontTime(0),
      _lastSoothingMistTime(0), _lastLifeCocoonTime(0), _lastHealingScanTime(0), _lastManaCheckTime(0),
      _lastFistweavingEval(0), _prioritizeFistweaving(false), _conserveManaAggressively(false),
      _useGroupHealingOptimization(true), _maxHoTTargets(5), _healingEfficiencyTarget(0.85f)
{
    // Initialize fistweaving system
    _fistweaving = FistweavingInfo();

    // Initialize emergency heals in priority order
    _emergencyHeals = {LIFE_COCOON, VIVIFY, ENVELOPING_MIST};

    // Initialize sustain heals
    _sustainHeals = {VIVIFY, ENVELOPING_MIST, RENEWING_MIST, SOOTHING_MIST};

    // Initialize group heals
    _groupHeals = {ESSENCE_FONT, VIVIFY};

    // Initialize HoT abilities
    _hotAbilities = {RENEWING_MIST, ENVELOPING_MIST};

    // Initialize fistweaving abilities
    _fistweavingAbilities = {TIGER_PALM, BLACKOUT_KICK, RISING_SUN_KICK};

    TC_LOG_DEBUG("playerbot", "MistweaverSpecialization: Initialized for bot {}", _bot->GetName());
}

void MistweaverSpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot)
        return;

    // Update all management systems
    UpdateChiManagement();
    UpdateEnergyManagement();
    UpdateHealingAssessment();
    UpdateFistweavingSystem();
    UpdateHoTManagement();
    UpdateManaManagement();
    UpdateCooldownManagement();
    UpdateMistweaverMetrics();

    // Execute rotation based on current phase
    switch (_mistweaverPhase)
    {
        case MistweaverRotationPhase::ASSESSMENT:
            ExecuteAssessmentPhase(target);
            break;
        case MistweaverRotationPhase::EMERGENCY_HEALING:
            ExecuteEmergencyHealing(target);
            break;
        case MistweaverRotationPhase::FISTWEAVING:
            ExecuteFistweaving(target);
            break;
        case MistweaverRotationPhase::HOT_MANAGEMENT:
            ExecuteHoTManagement(target);
            break;
        case MistweaverRotationPhase::GROUP_HEALING:
            ExecuteGroupHealing(target);
            break;
        case MistweaverRotationPhase::MANA_MANAGEMENT:
            ExecuteManaManagement(target);
            break;
        case MistweaverRotationPhase::DAMAGE_CONTRIBUTION:
            ExecuteDamageContribution(target);
            break;
        case MistweaverRotationPhase::UTILITY_SUPPORT:
            ExecuteUtilitySupport(target);
            break;
        case MistweaverRotationPhase::COOLDOWN_MANAGEMENT:
            ExecuteCooldownManagement(target);
            break;
    }

    AnalyzeHealingEfficiency();
    AnalyzeFistweavingEfficiency();
}

void MistweaverSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    UpdateSharedBuffs();

    // Maintain healing buffs
    if (!HasAura(TEACHINGS_OF_THE_MONASTERY) && HasSpell(TEACHINGS_OF_THE_MONASTERY))
    {
        CastTeachingsOfTheMonastery();
    }
}

void MistweaverSpecialization::UpdateCooldowns(uint32 diff)
{
    MonkSpecialization::UpdateChiManagement();
    MonkSpecialization::UpdateEnergyManagement();

    // Update HoT timers
    for (auto it = _renewingMistTimers.begin(); it != _renewingMistTimers.end();)
    {
        if (it->second <= diff)
        {
            it = _renewingMistTimers.erase(it);
        }
        else
        {
            it->second -= diff;
            ++it;
        }
    }

    for (auto it = _envelopingMistTimers.begin(); it != _envelopingMistTimers.end();)
    {
        if (it->second <= diff)
        {
            it = _envelopingMistTimers.erase(it);
        }
        else
        {
            it->second -= diff;
            ++it;
        }
    }
}

bool MistweaverSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    return true;
}

void MistweaverSpecialization::OnCombatStart(::Unit* target)
{
    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = MistweaverMetrics();

    // Start with assessment phase
    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
    LogMistweaverDecision("Combat Start", "Beginning healing assessment");

    // Evaluate fistweaving mode
    EvaluateFistweavingMode();
}

void MistweaverSpecialization::OnCombatEnd()
{
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "MistweaverSpecialization [{}]: Combat ended. Duration: {}ms, Healing done: {}, Fistweaving uptime: {:.1f}%",
                _bot->GetName(), combatDuration, _metrics.totalHealingDone, _metrics.fistweavingUptime * 100);

    // Reset phases and state
    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
    _fistweaving.isActive = false;
    _renewingMistTimers.clear();
    _envelopingMistTimers.clear();
    _currentTarget = nullptr;
}

bool MistweaverSpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    switch (spellInfo->PowerType)
    {
        case POWER_CHI:
            return HasChi(spellInfo->ManaCost);
        case POWER_ENERGY:
            return HasEnergy(spellInfo->ManaCost);
        case POWER_MANA:
            return _mana >= spellInfo->ManaCost;
        default:
            return true;
    }
}

void MistweaverSpecialization::ConsumeResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    switch (spellInfo->PowerType)
    {
        case POWER_CHI:
            SpendChi(spellInfo->ManaCost);
            break;
        case POWER_ENERGY:
            SpendEnergy(spellInfo->ManaCost);
            break;
        case POWER_MANA:
            if (_mana >= spellInfo->ManaCost)
            {
                _mana -= spellInfo->ManaCost;
                _metrics.manaSpent += spellInfo->ManaCost;
            }
            break;
    }
}

Position MistweaverSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!_bot)
        return Position();

    if (_fistweaving.isActive && target)
    {
        // Fistweaving - stay in melee range
        return target->GetPosition();
    }
    else
    {
        // Pure healing - central position to group
        return GetCentralPosition();
    }
}

float MistweaverSpecialization::GetOptimalRange(::Unit* target)
{
    if (_fistweaving.isActive)
        return MELEE_RANGE;
    else
        return HEALING_RANGE;
}

::Unit* MistweaverSpecialization::GetBestTarget()
{
    // Prioritize healing targets over damage targets
    ::Unit* healTarget = GetBestHealTarget();
    if (healTarget)
        return healTarget;

    // If no healing needed and fistweaving, find damage target
    if (_fistweaving.isActive)
    {
        std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
        return enemies.empty() ? nullptr : enemies[0];
    }

    return nullptr;
}

// Phase execution methods
void MistweaverSpecialization::ExecuteAssessmentPhase(::Unit* target)
{
    ScanForHealTargets();

    // Check for emergency healing
    ::Unit* criticalTarget = GetMostCriticalTarget();
    if (criticalTarget && criticalTarget->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
    {
        _mistweaverPhase = MistweaverRotationPhase::EMERGENCY_HEALING;
        return;
    }

    // Check for group healing needs
    if (ShouldUseGroupHealing())
    {
        _mistweaverPhase = MistweaverRotationPhase::GROUP_HEALING;
        return;
    }

    // Check if we should fistweave
    if (ShouldFistweave())
    {
        _mistweaverPhase = MistweaverRotationPhase::FISTWEAVING;
        return;
    }

    // Check for HoT management
    if (GetActiveHoTCount() < _maxHoTTargets)
    {
        _mistweaverPhase = MistweaverRotationPhase::HOT_MANAGEMENT;
        return;
    }

    // Default to damage contribution
    _mistweaverPhase = MistweaverRotationPhase::DAMAGE_CONTRIBUTION;
}

void MistweaverSpecialization::ExecuteEmergencyHealing(::Unit* target)
{
    ::Unit* criticalTarget = GetMostCriticalTarget();
    if (!criticalTarget)
    {
        _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
        return;
    }

    // Life Cocoon for extreme emergencies
    if (criticalTarget->GetHealthPct() < 15.0f && HasSpell(LIFE_COCOON) && CanUseAbility(LIFE_COCOON))
    {
        CastLifeCocoon(criticalTarget);
        return;
    }

    // Vivify for fast healing
    if (HasSpell(VIVIFY) && CanUseAbility(VIVIFY))
    {
        CastVivify(criticalTarget);
        return;
    }

    // Enveloping Mist if Vivify not available
    if (HasSpell(ENVELOPING_MIST) && CanUseAbility(ENVELOPING_MIST))
    {
        CastEnvelopingMist(criticalTarget);
        return;
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteFistweaving(::Unit* target)
{
    ::Unit* fistweavingTarget = GetBestFistweavingTarget();
    if (!fistweavingTarget)
    {
        _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
        return;
    }

    // Use chi generators for healing through Teachings of the Monastery
    if (HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(fistweavingTarget);
        ProcessFistweavingHealing();
        return;
    }

    if (HasChi(1) && HasSpell(BLACKOUT_KICK) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(fistweavingTarget);
        ProcessFistweavingHealing();
        return;
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteHoTManagement(::Unit* target)
{
    std::vector<::Unit*> healingNeeded = GetAlliesNeedingHealing(0.9f);

    for (::Unit* ally : healingNeeded)
    {
        if (NeedsRenewingMist(ally) && HasSpell(RENEWING_MIST) && CanUseAbility(RENEWING_MIST))
        {
            CastRenewingMist(ally);
            return;
        }

        if (NeedsEnvelopingMist(ally) && HasSpell(ENVELOPING_MIST) && CanUseAbility(ENVELOPING_MIST))
        {
            CastEnvelopingMist(ally);
            return;
        }
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteGroupHealing(::Unit* target)
{
    // Essence Font for AoE healing
    if (ShouldUseEssenceFont() && HasSpell(ESSENCE_FONT) && CanUseAbility(ESSENCE_FONT))
    {
        CastEssenceFont();
        return;
    }

    // Vivify with Thunder Focus Tea for enhanced group healing
    if (HasAura(THUNDER_FOCUS_TEA) && HasSpell(VIVIFY) && CanUseAbility(VIVIFY))
    {
        ::Unit* healTarget = GetBestGroupHealTarget();
        if (healTarget)
        {
            CastVivify(healTarget);
            return;
        }
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteManaManagement(::Unit* target)
{
    // Use Mana Tea if available
    if (GetManaPercent() < 0.5f && HasSpell(MANA_TEA) && CanUseAbility(MANA_TEA))
    {
        CastManaTea();
        return;
    }

    // Use Life Cycles for mana efficiency
    if (HasSpell(LIFE_COCOON) && CanUseAbility(LIFE_COCOON)) // Placeholder for Life Cycles
    {
        CastLifeCycles();
        return;
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteDamageContribution(::Unit* target)
{
    if (!target)
        return;

    // Contribute damage when healing is not needed
    if (HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        return;
    }

    if (HasChi(1) && HasSpell(BLACKOUT_KICK) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
        return;
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteUtilitySupport(::Unit* target)
{
    // Use utility abilities as needed
    std::vector<::Unit*> enemies = GetNearbyEnemies(8.0f);
    if (enemies.size() >= 3 && HasSpell(LEG_SWEEP) && CanUseAbility(LEG_SWEEP))
    {
        CastLegSweep();
        return;
    }

    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

void MistweaverSpecialization::ExecuteCooldownManagement(::Unit* target)
{
    ManageHealingCooldowns();
    _mistweaverPhase = MistweaverRotationPhase::ASSESSMENT;
}

// Core ability implementations
void MistweaverSpecialization::CastVivify(::Unit* target)
{
    if (CastSpell(VIVIFY, target))
    {
        _metrics.vivifyCasts++;
        _lastVivifyTime = getMSTime();
        uint32 healAmount = 2500; // Placeholder healing amount
        _metrics.totalHealingDone += healAmount;
        _metrics.directHealing += healAmount;
        LogMistweaverDecision("Cast Vivify", "Direct healing");
    }
}

void MistweaverSpecialization::CastEnvelopingMist(::Unit* target)
{
    if (CastSpell(ENVELOPING_MIST, target))
    {
        _metrics.envelopingMistCasts++;
        _lastEnvelopingMistTime = getMSTime();

        // Track HoT
        if (target)
        {
            _envelopingMistTimers[target->GetGUID()] = 30000; // 30 second duration
        }

        uint32 healAmount = 3000; // Placeholder healing amount
        _metrics.totalHealingDone += healAmount;
        LogMistweaverDecision("Cast Enveloping Mist", "HoT application");
    }
}

void MistweaverSpecialization::CastRenewingMist(::Unit* target)
{
    if (CastSpell(RENEWING_MIST, target))
    {
        _metrics.renewingMistCasts++;
        _lastRenewingMistTime = getMSTime();

        // Track HoT
        if (target)
        {
            _renewingMistTimers[target->GetGUID()] = 20000; // 20 second duration
        }

        uint32 healAmount = 1500; // Placeholder healing amount
        _metrics.totalHealingDone += healAmount;
        LogMistweaverDecision("Cast Renewing Mist", "HoT maintenance");
    }
}

void MistweaverSpecialization::CastEssenceFont()
{
    if (CastSpell(ESSENCE_FONT))
    {
        _metrics.essenceFontCasts++;
        _lastEssenceFontTime = getMSTime();
        uint32 healAmount = 4000; // Placeholder healing amount
        _metrics.totalHealingDone += healAmount;
        LogMistweaverDecision("Cast Essence Font", "AoE healing");
    }
}

void MistweaverSpecialization::CastSoothingMist(::Unit* target)
{
    if (CastSpell(SOOTHING_MIST, target))
    {
        _metrics.soothingMistChannels++;
        _lastSoothingMistTime = getMSTime();
        LogMistweaverDecision("Cast Soothing Mist", "Channel healing");
    }
}

void MistweaverSpecialization::CastLifeCocoon(::Unit* target)
{
    if (CastSpell(LIFE_COCOON, target))
    {
        _metrics.lifeCocoonCasts++;
        _lastLifeCocoonTime = getMSTime();
        LogMistweaverDecision("Cast Life Cocoon", "Emergency protection");
    }
}

void MistweaverSpecialization::CastTigerPalm(::Unit* target)
{
    if (CastSpell(TIGER_PALM, target))
    {
        GenerateChi(1); // Tiger Palm generates chi
        LogMistweaverDecision("Cast Tiger Palm", _fistweaving.isActive ? "Fistweaving" : "Damage contribution");
    }
}

void MistweaverSpecialization::CastBlackoutKick(::Unit* target)
{
    if (CastSpell(BLACKOUT_KICK, target))
    {
        LogMistweaverDecision("Cast Blackout Kick", _fistweaving.isActive ? "Fistweaving" : "Damage contribution");
    }
}

void MistweaverSpecialization::CastRisingSunKick(::Unit* target)
{
    if (CastSpell(RISING_SUN_KICK, target))
    {
        LogMistweaverDecision("Cast Rising Sun Kick", "High damage fistweaving");
    }
}

void MistweaverSpecialization::CastThunderFocusTea()
{
    if (CastSpell(THUNDER_FOCUS_TEA))
    {
        LogMistweaverDecision("Cast Thunder Focus Tea", "Healing enhancement");
    }
}

void MistweaverSpecialization::CastManaTea()
{
    if (CastSpell(MANA_TEA))
    {
        LogMistweaverDecision("Cast Mana Tea", "Mana recovery");
    }
}

void MistweaverSpecialization::CastTeachingsOfTheMonastery()
{
    if (CastSpell(TEACHINGS_OF_THE_MONASTERY))
    {
        LogMistweaverDecision("Cast Teachings of the Monastery", "Fistweaving buff");
    }
}

void MistweaverSpecialization::CastLegSweep()
{
    if (CastSpell(LEG_SWEEP))
    {
        LogMistweaverDecision("Cast Leg Sweep", "AoE stun");
    }
}

// Target management
void MistweaverSpecialization::ScanForHealTargets()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastHealingScanTime < HEALING_SCAN_INTERVAL)
        return;

    _lastHealingScanTime = currentTime;

    // Clear old targets
    while (!_healingTargets.empty())
        _healingTargets.pop();

    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);
    for (::Unit* ally : allies)
    {
        if (ally && ally->GetHealthPct() < 95.0f)
        {
            MistweaverTarget healTarget(ally, ally->GetHealthPct(), ally->GetMaxHealth() - ally->GetHealth());

            // Set priority based on health
            if (ally->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
                healTarget.priority = 1;
            else if (ally->GetHealthPct() < CRITICAL_HEALTH_THRESHOLD * 100.0f)
                healTarget.priority = 2;
            else if (ally->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f)
                healTarget.priority = 3;
            else
                healTarget.priority = 4;

            _healingTargets.push(healTarget);
        }
    }
}

::Unit* MistweaverSpecialization::GetBestHealTarget()
{
    if (_healingTargets.empty())
        return nullptr;

    MistweaverTarget bestTarget = _healingTargets.top();
    return bestTarget.target;
}

::Unit* MistweaverSpecialization::GetMostCriticalTarget()
{
    ::Unit* criticalTarget = nullptr;
    float lowestHealth = 100.0f;

    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);
    for (::Unit* ally : allies)
    {
        if (ally && ally->GetHealthPct() < lowestHealth)
        {
            criticalTarget = ally;
            lowestHealth = ally->GetHealthPct();
        }
    }

    return criticalTarget;
}

std::vector<::Unit*> MistweaverSpecialization::GetAlliesNeedingHealing(float healthThreshold)
{
    std::vector<::Unit*> needHealing;
    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);

    for (::Unit* ally : allies)
    {
        if (ally && ally->GetHealthPct() < healthThreshold * 100.0f)
        {
            needHealing.push_back(ally);
        }
    }

    return needHealing;
}

// Fistweaving management
void MistweaverSpecialization::EvaluateFistweavingMode()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastFistweavingEval < 5000) // Check every 5 seconds
        return;

    _lastFistweavingEval = currentTime;

    float groupHealth = GetAverageGroupHealth();

    // Switch to fistweaving if group health is high
    if (groupHealth > FISTWEAVING_SWITCH_THRESHOLD && !_fistweaving.isActive)
    {
        ToggleFistweaving();
    }
    // Switch to direct healing if group health is low
    else if (groupHealth < FISTWEAVING_SWITCH_THRESHOLD * 0.8f && _fistweaving.isActive)
    {
        ToggleFistweaving();
    }
}

void MistweaverSpecialization::ToggleFistweaving()
{
    _fistweaving.isActive = !_fistweaving.isActive;
    _fistweaving.lastToggle = getMSTime();

    LogMistweaverDecision(_fistweaving.isActive ? "Enable Fistweaving" : "Disable Fistweaving",
                         "Group health assessment");
}

bool MistweaverSpecialization::ShouldFistweave()
{
    return _fistweaving.isActive && GetBestFistweavingTarget() != nullptr;
}

::Unit* MistweaverSpecialization::GetBestFistweavingTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(MELEE_RANGE);
    return enemies.empty() ? nullptr : enemies[0];
}

void MistweaverSpecialization::ProcessFistweavingHealing()
{
    // Fistweaving heals nearby allies through Teachings of the Monastery
    if (HasAura(TEACHINGS_OF_THE_MONASTERY))
    {
        uint32 healAmount = 800; // Placeholder fistweaving healing
        _metrics.fistweavingHealing += healAmount;
        _metrics.totalHealingDone += healAmount;
    }
}

// HoT management
bool MistweaverSpecialization::NeedsRenewingMist(::Unit* target)
{
    if (!target)
        return false;

    auto it = _renewingMistTimers.find(target->GetGUID());
    return it == _renewingMistTimers.end() || it->second < HOT_REFRESH_THRESHOLD;
}

bool MistweaverSpecialization::NeedsEnvelopingMist(::Unit* target)
{
    if (!target)
        return false;

    auto it = _envelopingMistTimers.find(target->GetGUID());
    return it == _envelopingMistTimers.end() || it->second < HOT_REFRESH_THRESHOLD;
}

uint32 MistweaverSpecialization::GetActiveHoTCount()
{
    return static_cast<uint32>(_renewingMistTimers.size() + _envelopingMistTimers.size());
}

// Group healing
bool MistweaverSpecialization::ShouldUseGroupHealing()
{
    uint32 injuredCount = GetInjuredAllyCount();
    return injuredCount >= GROUP_HEAL_COUNT_THRESHOLD;
}

bool MistweaverSpecialization::ShouldUseEssenceFont()
{
    return GetInjuredAllyCount() >= 4 && GetManaPercent() > 0.4f;
}

uint32 MistweaverSpecialization::GetInjuredAllyCount()
{
    uint32 count = 0;
    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);

    for (::Unit* ally : allies)
    {
        if (ally && ally->GetHealthPct() < GROUP_HEAL_THRESHOLD * 100.0f)
        {
            count++;
        }
    }

    return count;
}

float MistweaverSpecialization::GetAverageGroupHealth()
{
    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);
    if (allies.empty())
        return 100.0f;

    float totalHealth = 0.0f;
    for (::Unit* ally : allies)
    {
        if (ally)
            totalHealth += ally->GetHealthPct();
    }

    return totalHealth / allies.size();
}

// Mana management
float MistweaverSpecialization::GetManaPercent()
{
    return _maxMana > 0 ? (float)_mana / _maxMana : 0.0f;
}

bool MistweaverSpecialization::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD;
}

Position MistweaverSpecialization::GetCentralPosition()
{
    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);
    if (allies.empty())
        return _bot->GetPosition();

    float avgX = 0.0f, avgY = 0.0f, avgZ = 0.0f;
    for (::Unit* ally : allies)
    {
        if (ally)
        {
            avgX += ally->GetPositionX();
            avgY += ally->GetPositionY();
            avgZ += ally->GetPositionZ();
        }
    }

    avgX /= allies.size();
    avgY /= allies.size();
    avgZ /= allies.size();

    return Position(avgX, avgY, avgZ, _bot->GetOrientation());
}

// System updates
void MistweaverSpecialization::UpdateHealingAssessment()
{
    // Determine next phase based on group state
    float groupHealth = GetAverageGroupHealth();
    uint32 criticalTargets = 0;

    std::vector<::Unit*> allies = GetNearbyAllies(HEALING_RANGE);
    for (::Unit* ally : allies)
    {
        if (ally && ally->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
            criticalTargets++;
    }

    if (criticalTargets > 0)
    {
        _mistweaverPhase = MistweaverRotationPhase::EMERGENCY_HEALING;
    }
    else if (ShouldConserveMana())
    {
        _mistweaverPhase = MistweaverRotationPhase::MANA_MANAGEMENT;
    }
    else if (groupHealth > FISTWEAVING_SWITCH_THRESHOLD)
    {
        _mistweaverPhase = MistweaverRotationPhase::FISTWEAVING;
    }
}

void MistweaverSpecialization::UpdateFistweavingSystem()
{
    EvaluateFistweavingMode();

    // Update fistweaving uptime
    if (_fistweaving.isActive)
    {
        _metrics.fistweavingUptime = (_metrics.fistweavingUptime + 0.1f) / 1.1f;
    }
}

void MistweaverSpecialization::UpdateHoTManagement()
{
    RefreshExpiredHoTs();
}

void MistweaverSpecialization::UpdateManaManagement()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastManaCheckTime < 2000) // Check every 2 seconds
        return;

    _lastManaCheckTime = currentTime;

    if (_bot)
    {
        _mana = _bot->GetPower(POWER_MANA);
        _maxMana = _bot->GetMaxPower(POWER_MANA);
    }

    if (ShouldConserveMana())
    {
        _mistweaverPhase = MistweaverRotationPhase::MANA_MANAGEMENT;
    }
}

void MistweaverSpecialization::UpdateCooldownManagement()
{
    // Manage major healing cooldowns
}

void MistweaverSpecialization::RefreshExpiredHoTs()
{
    // This would be handled by the timer updates in UpdateCooldowns
}

void MistweaverSpecialization::ManageHealingCooldowns()
{
    // Use Thunder Focus Tea when available and healing is needed
    if (HasSpell(THUNDER_FOCUS_TEA) && CanUseAbility(THUNDER_FOCUS_TEA))
    {
        if (GetInjuredAllyCount() >= 2)
        {
            CastThunderFocusTea();
        }
    }
}

void MistweaverSpecialization::UpdateMistweaverMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        _metrics.averageGroupHealth = GetAverageGroupHealth();

        if (_metrics.totalHealingDone > 0)
        {
            _metrics.healingEfficiency = (float)(_metrics.totalHealingDone - _metrics.overhealing) / _metrics.totalHealingDone;
        }

        if (_maxMana > 0)
        {
            _metrics.manaEfficiency = (float)_metrics.totalHealingDone / _metrics.manaSpent;
        }
    }
}

void MistweaverSpecialization::AnalyzeHealingEfficiency()
{
    // Performance analysis every 15 seconds
    if (getMSTime() % 15000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "MistweaverSpecialization [{}]: Efficiency - Healing: {:.1f}%, Mana: {:.1f}%, Group Health: {:.1f}%",
                    _bot->GetName(), _metrics.healingEfficiency * 100, GetManaPercent() * 100, _metrics.averageGroupHealth);
    }
}

void MistweaverSpecialization::AnalyzeFistweavingEfficiency()
{
    if (_metrics.directHealing > 0)
    {
        _fistweaving.efficiency = (float)_metrics.fistweavingHealing / _metrics.directHealing;
    }
}

void MistweaverSpecialization::LogMistweaverDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

} // namespace Playerbot