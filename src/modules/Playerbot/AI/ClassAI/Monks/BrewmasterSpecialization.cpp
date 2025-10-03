/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BrewmasterSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"

namespace Playerbot
{

BrewmasterSpecialization::BrewmasterSpecialization(Player* bot)
    : MonkSpecialization(bot), _brewmasterPhase(BrewmasterRotationPhase::THREAT_ESTABLISHMENT),
      _lastKegSmashTime(0), _lastBreathOfFireTime(0), _lastTigerPalmTime(0), _lastBlackoutKickTime(0),
      _lastIronskinBrewTime(0), _lastPurifyingBrewTime(0), _lastStaggerUpdate(0), _lastBrewRecharge(0),
      _lastThreatCheck(0), _lastDefensiveCheck(0), _prioritizeStaggerManagement(true),
      _aggressiveBrewUsage(false), _conserveChiForDefense(true), _maxStaggerTolerance(1500), _threatMargin(0.2f)
{
    // Initialize stagger system
    _stagger = StaggerInfo();

    // Initialize brew system
    _brews = BrewInfo();

    // Initialize threat abilities in priority order
    _threatAbilities = {KEG_SMASH, BREATH_OF_FIRE, TIGER_PALM};

    // Initialize defensive abilities
    _defensiveAbilities = {IRONSKIN_BREW, FORTIFYING_BREW, ZEN_MEDITATION, DAMPEN_HARM};

    // Initialize brew abilities
    _brewAbilities = {IRONSKIN_BREW, PURIFYING_BREW};

    // Initialize AoE abilities
    _aoeAbilities = {BREATH_OF_FIRE, SPINNING_CRANE_KICK, KEG_SMASH};

    TC_LOG_DEBUG("playerbot", "BrewmasterSpecialization: Initialized for bot {}", _bot->GetName());
}

void BrewmasterSpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot)
        return;

    // Update all management systems
    UpdateChiManagement();
    UpdateEnergyManagement();
    UpdateStaggerManagement();
    UpdateBrewManagement();
    UpdateThreatManagement();
    UpdateDefensiveCooldowns();
    UpdateBrewmasterMetrics();

    // Execute rotation based on current phase
    switch (_brewmasterPhase)
    {
        case BrewmasterRotationPhase::THREAT_ESTABLISHMENT:
            ExecuteThreatEstablishment(target);
            break;
        case BrewmasterRotationPhase::STAGGER_MANAGEMENT:
            ExecuteStaggerManagement(target);
            break;
        case BrewmasterRotationPhase::BREW_OPTIMIZATION:
            ExecuteBrewOptimization(target);
            break;
        case BrewmasterRotationPhase::AOE_CONTROL:
            ExecuteAoEControl(target);
            break;
        case BrewmasterRotationPhase::DEFENSIVE_COOLDOWNS:
            ExecuteDefensiveCooldowns(target);
            break;
        case BrewmasterRotationPhase::CHI_GENERATION:
            ExecuteChiGeneration(target);
            break;
        case BrewmasterRotationPhase::DAMAGE_DEALING:
            ExecuteDamageDealing(target);
            break;
        case BrewmasterRotationPhase::EMERGENCY_SURVIVAL:
            ExecuteEmergencySurvival(target);
            break;
    }

    AnalyzeTankingEfficiency();
}

void BrewmasterSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    UpdateSharedBuffs();
    MaintainDefensiveBuffs();

    // Ensure Ironskin Brew is active when tanking
    if (_bot->IsInCombat() && !HasAura(IRONSKIN_BREW) && _brews.HasIronskinCharges())
    {
        CastIronskinBrew();
    }
}

void BrewmasterSpecialization::UpdateCooldowns(uint32 diff)
{
    MonkSpecialization::UpdateChiManagement();
    MonkSpecialization::UpdateEnergyManagement();

    // Update stagger timer
    if (_stagger.remainingTime > 0)
    {
        if (_stagger.remainingTime <= diff)
        {
            _stagger.remainingTime = 0;
            _stagger.totalDamage = 0;
            _stagger.tickDamage = 0;
        }
        else
        {
            _stagger.remainingTime -= diff;
        }
        _stagger.UpdateStaggerLevel();
    }

    // Update brew recharge
    RechargeBrews(diff);
}

bool BrewmasterSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    // Special checks for brew abilities
    if (spellId == IRONSKIN_BREW && !_brews.HasIronskinCharges())
        return false;

    if (spellId == PURIFYING_BREW && !_brews.HasPurifyingCharges())
        return false;

    return true;
}

void BrewmasterSpecialization::OnCombatStart(::Unit* target)
{
    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = BrewmasterMetrics();

    // Start with threat establishment
    _brewmasterPhase = BrewmasterRotationPhase::THREAT_ESTABLISHMENT;
    LogBrewmasterDecision("Combat Start", "Beginning threat establishment");

    // Activate initial defensive buffs
    if (!HasAura(IRONSKIN_BREW) && _brews.HasIronskinCharges())
    {
        CastIronskinBrew();
    }
}

void BrewmasterSpecialization::OnCombatEnd()
{
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "BrewmasterSpecialization [{}]: Combat ended. Duration: {}ms, Stagger mitigated: {}, Brews used: {}",
                _bot->GetName(), combatDuration, _metrics.staggerDamageMitigated,
                _metrics.ironskinBrewUses + _metrics.purifyingBrewUses);

    // Reset phases and state
    _brewmasterPhase = BrewmasterRotationPhase::THREAT_ESTABLISHMENT;
    _stagger = StaggerInfo();
    _currentTarget = nullptr;
}

bool BrewmasterSpecialization::HasEnoughResource(uint32 spellId)
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

void BrewmasterSpecialization::ConsumeResource(uint32 spellId)
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
            _mana = _mana >= spellInfo->ManaCost ? _mana - spellInfo->ManaCost : 0;
            break;
    }
}

Position BrewmasterSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!_bot || !target)
        return Position();

    // Tank positioning - face away from group
    Position pos = target->GetPosition();

    // Position slightly away from target to allow for cleave avoidance
    float angle = target->GetOrientation() + M_PI; // Opposite direction
    float distance = MELEE_RANGE * 0.8f;

    pos.m_positionX += cos(angle) * distance;
    pos.m_positionY += sin(angle) * distance;
    pos.SetOrientation(target->GetOrientation());

    return pos;
}

float BrewmasterSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE; // Brewmaster needs to be in melee range
}

::Unit* BrewmasterSpecialization::GetBestTarget()
{
    // Prioritize targets by threat level
    ::Unit* highestThreat = GetHighestThreatTarget();
    if (highestThreat)
        return highestThreat;

    // Fallback to untagged enemies
    std::vector<::Unit*> untagged = GetUntaggedEnemies();
    if (!untagged.empty())
        return untagged[0];

    // Last resort - any nearby enemy
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    return enemies.empty() ? nullptr : enemies[0];
}

// Phase execution methods
void BrewmasterSpecialization::ExecuteThreatEstablishment(::Unit* target)
{
    if (!target)
        return;

    // Use Keg Smash for AoE threat
    if (HasSpell(KEG_SMASH) && CanUseAbility(KEG_SMASH))
    {
        CastKegSmash(target);
        _brewmasterPhase = BrewmasterRotationPhase::STAGGER_MANAGEMENT;
        return;
    }

    // Tiger Palm for single target threat and chi
    if (HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        _brewmasterPhase = BrewmasterRotationPhase::CHI_GENERATION;
        return;
    }
}

void BrewmasterSpecialization::ExecuteStaggerManagement(::Unit* target)
{
    // Check if we need to purify stagger
    if (ShouldUsePurifyingBrew() && CanUseAbility(PURIFYING_BREW))
    {
        CastPurifyingBrew();
        LogBrewmasterDecision("Purifying Brew", "Heavy stagger damage");
    }

    // Maintain Ironskin Brew
    if (ShouldUseIronskinBrew() && CanUseAbility(IRONSKIN_BREW))
    {
        CastIronskinBrew();
        LogBrewmasterDecision("Ironskin Brew", "Stagger mitigation");
    }

    _brewmasterPhase = BrewmasterRotationPhase::BREW_OPTIMIZATION;
}

void BrewmasterSpecialization::ExecuteBrewOptimization(::Unit* target)
{
    OptimizeBrewUsage();

    uint32 nearbyEnemies = GetNearbyEnemyCount();
    if (nearbyEnemies >= AOE_THRESHOLD)
    {
        _brewmasterPhase = BrewmasterRotationPhase::AOE_CONTROL;
    }
    else
    {
        _brewmasterPhase = BrewmasterRotationPhase::DAMAGE_DEALING;
    }
}

void BrewmasterSpecialization::ExecuteAoEControl(::Unit* target)
{
    // Breath of Fire for AoE damage and debuff
    if (HasSpell(BREATH_OF_FIRE) && CanUseAbility(BREATH_OF_FIRE))
    {
        CastBreathOfFire();
        _brewmasterPhase = BrewmasterRotationPhase::CHI_GENERATION;
        return;
    }

    // Spinning Crane Kick for AoE
    if (HasSpell(SPINNING_CRANE_KICK) && CanUseAbility(SPINNING_CRANE_KICK))
    {
        CastSpinningCraneKick();
        _brewmasterPhase = BrewmasterRotationPhase::CHI_GENERATION;
        return;
    }

    _brewmasterPhase = BrewmasterRotationPhase::DAMAGE_DEALING;
}

void BrewmasterSpecialization::ExecuteDefensiveCooldowns(::Unit* target)
{
    if (NeedsDefensiveCooldown())
    {
        UseEmergencyDefensives();
    }

    _brewmasterPhase = BrewmasterRotationPhase::THREAT_ESTABLISHMENT;
}

void BrewmasterSpecialization::ExecuteChiGeneration(::Unit* target)
{
    if (!target)
        return;

    // Generate chi with Tiger Palm
    if (GetChi() < 2 && HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        return;
    }

    // Use Expel Harm for chi and healing
    if (GetChi() < 3 && _bot->GetHealthPct() < 80.0f && HasSpell(EXPEL_HARM) && CanUseAbility(EXPEL_HARM))
    {
        CastExpelHarm();
        return;
    }

    _brewmasterPhase = BrewmasterRotationPhase::DAMAGE_DEALING;
}

void BrewmasterSpecialization::ExecuteDamageDealing(::Unit* target)
{
    if (!target)
        return;

    // Blackout Kick for damage
    if (HasChi(1) && HasSpell(BLACKOUT_KICK) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
        _brewmasterPhase = BrewmasterRotationPhase::THREAT_ESTABLISHMENT;
        return;
    }

    _brewmasterPhase = BrewmasterRotationPhase::CHI_GENERATION;
}

void BrewmasterSpecialization::ExecuteEmergencySurvival(::Unit* target)
{
    UseEmergencyDefensives();

    if (_bot->GetHealthPct() > 50.0f)
        _brewmasterPhase = BrewmasterRotationPhase::STAGGER_MANAGEMENT;
}

// Core ability implementations
void BrewmasterSpecialization::CastKegSmash(::Unit* target)
{
    if (CastSpell(KEG_SMASH, target))
    {
        _metrics.kegSmashCasts++;
        _lastKegSmashTime = getMSTime();
        GenerateChi(1); // Keg Smash generates chi
        LogBrewmasterDecision("Cast Keg Smash", "AoE threat and chi generation");
    }
}

void BrewmasterSpecialization::CastBreathOfFire()
{
    if (CastSpell(BREATH_OF_FIRE))
    {
        _metrics.breathOfFireCasts++;
        _lastBreathOfFireTime = getMSTime();
        LogBrewmasterDecision("Cast Breath of Fire", "AoE damage and debuff");
    }
}

void BrewmasterSpecialization::CastTigerPalm(::Unit* target)
{
    if (CastSpell(TIGER_PALM, target))
    {
        _metrics.tigerPalmCasts++;
        _lastTigerPalmTime = getMSTime();
        GenerateChi(1); // Tiger Palm generates chi
        LogBrewmasterDecision("Cast Tiger Palm", "Chi generation and threat");
    }
}

void BrewmasterSpecialization::CastBlackoutKick(::Unit* target)
{
    if (CastSpell(BLACKOUT_KICK, target))
    {
        _metrics.blackoutKickCasts++;
        _lastBlackoutKickTime = getMSTime();
        LogBrewmasterDecision("Cast Blackout Kick", "Chi spender for damage");
    }
}

void BrewmasterSpecialization::CastSpinningCraneKick()
{
    if (CastSpell(SPINNING_CRANE_KICK))
    {
        LogBrewmasterDecision("Cast Spinning Crane Kick", "AoE damage");
    }
}

void BrewmasterSpecialization::CastIronskinBrew()
{
    if (CastSpell(IRONSKIN_BREW))
    {
        _metrics.ironskinBrewUses++;
        _brews.UseIronskinBrew();
        _lastIronskinBrewTime = getMSTime();
        LogBrewmasterDecision("Cast Ironskin Brew", "Stagger mitigation");
    }
}

void BrewmasterSpecialization::CastPurifyingBrew()
{
    if (CastSpell(PURIFYING_BREW))
    {
        _metrics.purifyingBrewUses++;
        _brews.UsePurifyingBrew();
        _lastPurifyingBrewTime = getMSTime();

        // Clear stagger
        _metrics.staggerDamageMitigated += _stagger.totalDamage;
        _stagger.totalDamage = 0;
        _stagger.tickDamage = 0;
        _stagger.remainingTime = 0;
        _stagger.UpdateStaggerLevel();

        LogBrewmasterDecision("Cast Purifying Brew", "Clear heavy stagger");
    }
}

void BrewmasterSpecialization::CastFortifyingBrew()
{
    if (CastSpell(FORTIFYING_BREW))
    {
        LogBrewmasterDecision("Cast Fortifying Brew", "Emergency defensive");
    }
}

void BrewmasterSpecialization::CastZenMeditation()
{
    if (CastSpell(ZEN_MEDITATION))
    {
        LogBrewmasterDecision("Cast Zen Meditation", "Damage reduction channel");
    }
}

void BrewmasterSpecialization::CastDampenHarm()
{
    if (CastSpell(DAMPEN_HARM))
    {
        LogBrewmasterDecision("Cast Dampen Harm", "Damage reduction");
    }
}

void BrewmasterSpecialization::CastExpelHarm()
{
    if (CastSpell(EXPEL_HARM))
    {
        GenerateChi(1); // Expel Harm generates chi
        LogBrewmasterDecision("Cast Expel Harm", "Self-heal and chi generation");
    }
}

// Stagger management
bool BrewmasterSpecialization::ShouldUsePurifyingBrew()
{
    if (!_brews.HasPurifyingCharges())
        return false;

    // Use on heavy stagger
    if (_stagger.isHeavy)
        return true;

    // Use on moderate stagger if low on health
    if (_stagger.isModerate && _bot->GetHealthPct() < 60.0f)
        return true;

    return false;
}

bool BrewmasterSpecialization::ShouldUseIronskinBrew()
{
    if (!_brews.HasIronskinCharges())
        return false;

    // Use if not active and in combat
    if (_bot->IsInCombat() && !HasAura(IRONSKIN_BREW))
        return true;

    // Refresh if about to expire (less than 3 seconds)
    if (HasAura(IRONSKIN_BREW))
    {
        // Check remaining time on aura (simplified)
        uint32 timeSinceLastCast = getMSTime() - _lastIronskinBrewTime;
        if (timeSinceLastCast > 27000) // 30 second duration - 3 second buffer
            return true;
    }

    return false;
}

void BrewmasterSpecialization::RechargeBrews(uint32 diff)
{
    _lastBrewRecharge += diff;

    if (_lastBrewRecharge >= BREW_RECHARGE_TIME)
    {
        if (_brews.ironskinCharges < _brews.maxCharges)
            _brews.ironskinCharges++;
        if (_brews.purifyingCharges < _brews.maxCharges)
            _brews.purifyingCharges++;

        _lastBrewRecharge = 0;
    }
}

void BrewmasterSpecialization::OptimizeBrewUsage()
{
    // Prioritize brew usage based on situation
    if (_aggressiveBrewUsage)
    {
        // Use brews more frequently for maximum mitigation
        if (_brews.ironskinCharges >= 2 && !HasAura(IRONSKIN_BREW))
        {
            CastIronskinBrew();
        }
    }
    else
    {
        // Conservative usage - save brews for emergencies
        if (_brews.ironskinCharges >= 1 && !HasAura(IRONSKIN_BREW) && _bot->GetHealthPct() < 80.0f)
        {
            CastIronskinBrew();
        }
    }
}

uint32 BrewmasterSpecialization::GetNearbyEnemyCount()
{
    return static_cast<uint32>(GetAoETargets(8.0f).size());
}

bool BrewmasterSpecialization::NeedsDefensiveCooldown()
{
    return _bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f;
}

void BrewmasterSpecialization::UseEmergencyDefensives()
{
    if (_bot->GetHealthPct() < 30.0f)
    {
        if (HasSpell(FORTIFYING_BREW) && CanUseAbility(FORTIFYING_BREW))
        {
            CastFortifyingBrew();
            return;
        }

        if (HasSpell(ZEN_MEDITATION) && CanUseAbility(ZEN_MEDITATION))
        {
            CastZenMeditation();
            return;
        }

        if (HasSpell(DAMPEN_HARM) && CanUseAbility(DAMPEN_HARM))
        {
            CastDampenHarm();
            return;
        }
    }
}

void BrewmasterSpecialization::MaintainDefensiveBuffs()
{
    // Ensure Ironskin Brew is maintained
    if (_bot->IsInCombat() && ShouldUseIronskinBrew())
    {
        CastIronskinBrew();
    }
}

std::vector<::Unit*> BrewmasterSpecialization::GetUntaggedEnemies()
{
    std::vector<::Unit*> untagged;
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);

    for (::Unit* enemy : enemies)
    {
        if (enemy && enemy->GetVictim() != _bot)
        {
            untagged.push_back(enemy);
        }
    }

    return untagged;
}

::Unit* BrewmasterSpecialization::GetHighestThreatTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    ::Unit* highestThreat = nullptr;

    for (::Unit* enemy : enemies)
    {
        if (enemy && enemy->GetVictim() == _bot)
        {
            if (!highestThreat)
                highestThreat = enemy;
        }
    }

    return highestThreat;
}

// System updates
void BrewmasterSpecialization::UpdateStaggerManagement()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastStaggerUpdate < STAGGER_CHECK_INTERVAL)
        return;

    _lastStaggerUpdate = currentTime;

    // Update stagger level
    _stagger.UpdateStaggerLevel();

    // Check if we need emergency stagger management
    if (_stagger.isHeavy && _bot->GetHealthPct() < LOW_HEALTH_THRESHOLD * 100.0f)
    {
        _brewmasterPhase = BrewmasterRotationPhase::EMERGENCY_SURVIVAL;
    }
    else if (_stagger.isHeavy || _stagger.isModerate)
    {
        _brewmasterPhase = BrewmasterRotationPhase::STAGGER_MANAGEMENT;
    }
}

void BrewmasterSpecialization::UpdateBrewManagement()
{
    // Track brew efficiency
    uint32 totalBrews = _metrics.ironskinBrewUses + _metrics.purifyingBrewUses;
    uint32 combatTime = getMSTime() - _combatStartTime;

    if (combatTime > 0)
    {
        _metrics.brewUtilization = (float)totalBrews / (combatTime / 1000.0f);
    }
}

void BrewmasterSpecialization::UpdateThreatManagement()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastThreatCheck < THREAT_CHECK_INTERVAL)
        return;

    _lastThreatCheck = currentTime;

    // Check if we have proper threat on all enemies
    std::vector<::Unit*> untagged = GetUntaggedEnemies();
    if (!untagged.empty())
    {
        _brewmasterPhase = BrewmasterRotationPhase::THREAT_ESTABLISHMENT;
    }
}

void BrewmasterSpecialization::UpdateDefensiveCooldowns()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastDefensiveCheck < 2000) // Check every 2 seconds
        return;

    _lastDefensiveCheck = currentTime;

    if (NeedsDefensiveCooldown())
    {
        _brewmasterPhase = BrewmasterRotationPhase::DEFENSIVE_COOLDOWNS;
    }
}

void BrewmasterSpecialization::UpdateBrewmasterMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate stagger uptime
        _metrics.staggerUptime = _stagger.remainingTime > 0 ? (_metrics.staggerUptime + 1.0f) / 2.0f : _metrics.staggerUptime;

        // Calculate average stagger level
        float currentStaggerLevel = 0.0f;
        if (_stagger.isHeavy) currentStaggerLevel = 3.0f;
        else if (_stagger.isModerate) currentStaggerLevel = 2.0f;
        else if (_stagger.isLight) currentStaggerLevel = 1.0f;

        _metrics.averageStaggerLevel = (_metrics.averageStaggerLevel + currentStaggerLevel) / 2.0f;
    }
}

void BrewmasterSpecialization::AnalyzeTankingEfficiency()
{
    // Performance analysis every 10 seconds
    if (getMSTime() % 10000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "BrewmasterSpecialization [{}]: Efficiency - Stagger Uptime: {:.1f}%, Brew Usage: {:.1f}/min, Avg Stagger: {:.1f}",
                    _bot->GetName(), _metrics.staggerUptime * 100, _metrics.brewUtilization * 60, _metrics.averageStaggerLevel);
    }
}

void BrewmasterSpecialization::LogBrewmasterDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

} // namespace Playerbot