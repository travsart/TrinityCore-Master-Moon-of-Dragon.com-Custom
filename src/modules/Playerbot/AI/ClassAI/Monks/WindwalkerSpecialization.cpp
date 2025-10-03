/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WindwalkerSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"

namespace Playerbot
{

WindwalkerSpecialization::WindwalkerSpecialization(Player* bot)
    : MonkSpecialization(bot), _windwalkerPhase(WindwalkerRotationPhase::OPENING_SEQUENCE),
      _markOfTheCraneStacks(0), _lastTigerPalmTime(0), _lastBlackoutKickTime(0), _lastRisingSunKickTime(0),
      _lastFistsOfFuryTime(0), _lastWhirlingDragonPunchTime(0), _lastTouchOfDeathTime(0),
      _lastStormEarthAndFireTime(0), _lastSerenityTime(0), _lastBurstActivation(0), _lastComboCheck(0),
      _inBurstWindow(false), _burstWindowStart(0), _burstWindowDuration(BURST_WINDOW_DURATION),
      _prioritizeComboBuilding(true), _aggressiveBurstUsage(false), _conserveResourcesForBurst(true),
      _maxMarkTargets(8), _comboEfficiencyTarget(0.85f)
{
    // Initialize combo system
    _combo = ComboInfo();

    // Initialize chi generators in priority order
    _chiGenerators = {TIGER_PALM, EXPEL_HARM, CHI_WAVE, CHI_BURST};

    // Initialize chi spenders
    _chiSpenders = {RISING_SUN_KICK, FISTS_OF_FURY, BLACKOUT_KICK, WHIRLING_DRAGON_PUNCH};

    // Initialize AoE abilities
    _aoeAbilities = {SPINNING_CRANE_KICK, FISTS_OF_FURY, WHIRLING_DRAGON_PUNCH};

    // Initialize burst abilities
    _burstAbilities = {STORM_EARTH_AND_FIRE, TOUCH_OF_DEATH, FISTS_OF_FURY};

    // Initialize defensive abilities
    _defensiveAbilities = {TOUCH_OF_KARMA, DIFFUSE_MAGIC, DAMPEN_HARM};

    TC_LOG_DEBUG("playerbot", "WindwalkerSpecialization: Initialized for bot {}", _bot->GetName());
}

void WindwalkerSpecialization::UpdateRotation(::Unit* target)
{
    if (!_bot)
        return;

    // Update all management systems
    UpdateChiManagement();
    UpdateEnergyManagement();
    UpdateComboSystem();
    UpdateBurstWindows();
    UpdateMarkOfTheCrane();
    UpdateTouchOfDeath();
    UpdateMobility();
    UpdateWindwalkerMetrics();

    // Execute rotation based on current phase
    switch (_windwalkerPhase)
    {
        case WindwalkerRotationPhase::OPENING_SEQUENCE:
            ExecuteOpeningSequence(target);
            break;
        case WindwalkerRotationPhase::CHI_GENERATION:
            ExecuteChiGeneration(target);
            break;
        case WindwalkerRotationPhase::COMBO_BUILDING:
            ExecuteComboBuilding(target);
            break;
        case WindwalkerRotationPhase::COMBO_SPENDING:
            ExecuteComboSpending(target);
            break;
        case WindwalkerRotationPhase::BURST_WINDOW:
            ExecuteBurstWindow(target);
            break;
        case WindwalkerRotationPhase::AOE_ROTATION:
            ExecuteAoERotation(target);
            break;
        case WindwalkerRotationPhase::EXECUTE_PHASE:
            ExecuteExecutePhase(target);
            break;
        case WindwalkerRotationPhase::RESOURCE_RECOVERY:
            ExecuteResourceRecovery(target);
            break;
        case WindwalkerRotationPhase::EMERGENCY_SURVIVAL:
            ExecuteEmergencySurvival(target);
            break;
    }

    AnalyzeDamageEfficiency();
    AnalyzeComboEfficiency();
}

void WindwalkerSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    UpdateSharedBuffs();

    // Maintain Storm, Earth, and Fire if available
    if (IsInBurstWindow() && !_combo.stormEarthAndFireActive && HasSpell(STORM_EARTH_AND_FIRE) && CanUseAbility(STORM_EARTH_AND_FIRE))
    {
        CastStormEarthAndFire();
    }
}

void WindwalkerSpecialization::UpdateCooldowns(uint32 diff)
{
    MonkSpecialization::UpdateChiManagement();
    MonkSpecialization::UpdateEnergyManagement();

    // Update Mark of the Crane timers
    for (auto it = _markOfTheCraneTargets.begin(); it != _markOfTheCraneTargets.end();)
    {
        if (it->second <= diff)
        {
            it = _markOfTheCraneTargets.erase(it);
            if (_markOfTheCraneStacks > 0)
                _markOfTheCraneStacks--;
        }
        else
        {
            it->second -= diff;
            ++it;
        }
    }

    // Update burst window
    if (_inBurstWindow)
    {
        uint32 burstElapsed = getMSTime() - _burstWindowStart;
        if (burstElapsed >= _burstWindowDuration)
        {
            _inBurstWindow = false;
            _combo.stormEarthAndFireActive = false;
        }
    }
}

bool WindwalkerSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasSpell(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    // Special checks for Touch of Death
    if (spellId == TOUCH_OF_DEATH)
    {
        ::Unit* target = GetCurrentTarget();
        return target && ShouldUseTouchOfDeath(target);
    }

    return true;
}

void WindwalkerSpecialization::OnCombatStart(::Unit* target)
{
    _combatStartTime = getMSTime();
    _currentTarget = target;

    // Reset metrics for new combat
    _metrics = WindwalkerMetrics();

    // Reset combo system
    _combo = ComboInfo();
    _markOfTheCraneTargets.clear();
    _markOfTheCraneStacks = 0;

    // Start with opening sequence
    _windwalkerPhase = WindwalkerRotationPhase::OPENING_SEQUENCE;
    LogWindwalkerDecision("Combat Start", "Beginning DPS rotation");
}

void WindwalkerSpecialization::OnCombatEnd()
{
    uint32 combatDuration = getMSTime() - _combatStartTime;
    _averageCombatTime = (_averageCombatTime + combatDuration) / 2.0f;

    TC_LOG_DEBUG("playerbot", "WindwalkerSpecialization [{}]: Combat ended. Duration: {}ms, Damage dealt: {}, Combo efficiency: {:.1f}%",
                _bot->GetName(), combatDuration, _metrics.totalDamageDealt, _metrics.comboUptime * 100);

    // Reset phases and state
    _windwalkerPhase = WindwalkerRotationPhase::OPENING_SEQUENCE;
    _combo = ComboInfo();
    _markOfTheCraneTargets.clear();
    _markOfTheCraneStacks = 0;
    _inBurstWindow = false;
    _currentTarget = nullptr;
}

bool WindwalkerSpecialization::HasEnoughResource(uint32 spellId)
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

void WindwalkerSpecialization::ConsumeResource(uint32 spellId)
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

Position WindwalkerSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!_bot || !target)
        return Position();

    // Windwalker needs to be in melee range behind the target
    float angle = target->GetOrientation() + M_PI; // Behind target
    float distance = MELEE_RANGE * 0.8f;

    float x = target->GetPositionX() + cos(angle) * distance;
    float y = target->GetPositionY() + sin(angle) * distance;
    float z = target->GetPositionZ();

    return Position(x, y, z, target->GetOrientation());
}

float WindwalkerSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE; // Windwalker is melee DPS
}

::Unit* WindwalkerSpecialization::GetBestTarget()
{
    // Prioritize targets for execution
    ::Unit* executeTarget = GetBestExecuteTarget();
    if (executeTarget)
        return executeTarget;

    // Prioritize high-value targets
    ::Unit* priorityTarget = GetHighestPriorityTarget();
    if (priorityTarget)
        return priorityTarget;

    // Fallback to current target or nearest enemy
    ::Unit* currentTarget = GetCurrentTarget();
    if (currentTarget)
        return currentTarget;

    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    return enemies.empty() ? nullptr : enemies[0];
}

// Phase execution methods
void WindwalkerSpecialization::ExecuteOpeningSequence(::Unit* target)
{
    if (!target)
        return;

    // Start with Touch of Death if target is low health
    if (ShouldUseTouchOfDeath(target) && CanUseAbility(TOUCH_OF_DEATH))
    {
        CastTouchOfDeath(target);
        _windwalkerPhase = WindwalkerRotationPhase::BURST_WINDOW;
        return;
    }

    // Apply Mark of the Crane
    if (HasSpell(RISING_SUN_KICK) && CanUseAbility(RISING_SUN_KICK))
    {
        CastRisingSunKick(target);
        _windwalkerPhase = WindwalkerRotationPhase::CHI_GENERATION;
        return;
    }

    // Generate initial chi
    if (HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
        return;
    }
}

void WindwalkerSpecialization::ExecuteChiGeneration(::Unit* target)
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

    _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
}

void WindwalkerSpecialization::ExecuteComboBuilding(::Unit* target)
{
    if (!target)
        return;

    ComboState state = GetComboState();

    if (state == ComboState::READY_TO_SPEND)
    {
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_SPENDING;
        return;
    }

    // Build combo with Tiger Palm
    if (HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM))
    {
        CastTigerPalm(target);
        AddComboPoints(1);
        return;
    }

    _windwalkerPhase = WindwalkerRotationPhase::CHI_GENERATION;
}

void WindwalkerSpecialization::ExecuteComboSpending(::Unit* target)
{
    if (!target)
        return;

    // Rising Sun Kick for high damage and Mark of the Crane
    if (HasChi(2) && HasSpell(RISING_SUN_KICK) && CanUseAbility(RISING_SUN_KICK))
    {
        CastRisingSunKick(target);
        ResetComboPoints();
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
        return;
    }

    // Fists of Fury for high damage
    if (HasChi(3) && HasSpell(FISTS_OF_FURY) && CanUseAbility(FISTS_OF_FURY))
    {
        CastFistsOfFury(target);
        ResetComboPoints();
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
        return;
    }

    // Blackout Kick as filler
    if (HasChi(1) && HasSpell(BLACKOUT_KICK) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
        ResetComboPoints();
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
        return;
    }

    _windwalkerPhase = WindwalkerRotationPhase::CHI_GENERATION;
}

void WindwalkerSpecialization::ExecuteBurstWindow(::Unit* target)
{
    if (!target)
        return;

    // Activate burst cooldowns
    if (!_inBurstWindow && ShouldActivateBurst())
    {
        ActivateBurstWindow();
    }

    if (_inBurstWindow)
    {
        OptimizeBurstRotation(target);
    }
    else
    {
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
    }
}

void WindwalkerSpecialization::ExecuteAoERotation(::Unit* target)
{
    if (!target)
        return;

    // Spinning Crane Kick for AoE
    if (GetNearbyEnemyCount() >= AOE_THRESHOLD && HasSpell(SPINNING_CRANE_KICK) && CanUseAbility(SPINNING_CRANE_KICK))
    {
        CastSpinningCraneKick();
        return;
    }

    // Fists of Fury for AoE
    if (HasChi(3) && GetNearbyEnemyCount() >= 3 && HasSpell(FISTS_OF_FURY) && CanUseAbility(FISTS_OF_FURY))
    {
        CastFistsOfFury(target);
        return;
    }

    _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
}

void WindwalkerSpecialization::ExecuteExecutePhase(::Unit* target)
{
    if (!target)
        return;

    // Touch of Death for execute
    if (ShouldUseTouchOfDeath(target) && CanUseAbility(TOUCH_OF_DEATH))
    {
        CastTouchOfDeath(target);
        return;
    }

    // High damage abilities for execute
    if (HasChi(2) && HasSpell(RISING_SUN_KICK) && CanUseAbility(RISING_SUN_KICK))
    {
        CastRisingSunKick(target);
        return;
    }

    _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
}

void WindwalkerSpecialization::ExecuteResourceRecovery(::Unit* target)
{
    // Focus on chi and energy generation
    if (GetChi() < 2 && HasSpell(TIGER_PALM) && CanUseAbility(TIGER_PALM) && target)
    {
        CastTigerPalm(target);
        return;
    }

    if (GetEnergyPercent() < 0.5f)
    {
        // Wait for energy regeneration
        return;
    }

    _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
}

void WindwalkerSpecialization::ExecuteEmergencySurvival(::Unit* target)
{
    UseEmergencyDefensives();

    if (_bot->GetHealthPct() > 50.0f)
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
}

// Core ability implementations
void WindwalkerSpecialization::CastTigerPalm(::Unit* target)
{
    if (CastSpell(TIGER_PALM, target))
    {
        _metrics.tigerPalmCasts++;
        _lastTigerPalmTime = getMSTime();
        GenerateChi(1); // Tiger Palm generates chi
        LogWindwalkerDecision("Cast Tiger Palm", "Chi generation and combo building");
    }
}

void WindwalkerSpecialization::CastBlackoutKick(::Unit* target)
{
    if (CastSpell(BLACKOUT_KICK, target))
    {
        _metrics.blackoutKickCasts++;
        _lastBlackoutKickTime = getMSTime();
        uint32 damage = 1200; // Placeholder damage
        _metrics.totalDamageDealt += damage;
        LogWindwalkerDecision("Cast Blackout Kick", "Chi spender");
    }
}

void WindwalkerSpecialization::CastRisingSunKick(::Unit* target)
{
    if (CastSpell(RISING_SUN_KICK, target))
    {
        _metrics.risingSunKickCasts++;
        _lastRisingSunKickTime = getMSTime();
        ApplyMarkOfTheCrane(target);
        uint32 damage = 1800; // Placeholder damage
        _metrics.totalDamageDealt += damage;
        LogWindwalkerDecision("Cast Rising Sun Kick", "High damage and Mark of the Crane");
    }
}

void WindwalkerSpecialization::CastFistsOfFury(::Unit* target)
{
    if (CastSpell(FISTS_OF_FURY, target))
    {
        _metrics.fistsOfFuryCasts++;
        _lastFistsOfFuryTime = getMSTime();
        uint32 damage = 3000; // Placeholder damage
        _metrics.totalDamageDealt += damage;
        LogWindwalkerDecision("Cast Fists of Fury", "High damage channel");
    }
}

void WindwalkerSpecialization::CastWhirlingDragonPunch()
{
    if (CastSpell(WHIRLING_DRAGON_PUNCH))
    {
        _metrics.whirlingDragonPunchCasts++;
        _lastWhirlingDragonPunchTime = getMSTime();
        uint32 damage = 2500; // Placeholder damage
        _metrics.totalDamageDealt += damage;
        LogWindwalkerDecision("Cast Whirling Dragon Punch", "AoE damage");
    }
}

void WindwalkerSpecialization::CastSpinningCraneKick()
{
    if (CastSpell(SPINNING_CRANE_KICK))
    {
        uint32 damage = 800 * GetNearbyEnemyCount(); // Placeholder damage per target
        _metrics.totalDamageDealt += damage;
        LogWindwalkerDecision("Cast Spinning Crane Kick", "AoE damage");
    }
}

void WindwalkerSpecialization::CastTouchOfDeath(::Unit* target)
{
    if (CastSpell(TOUCH_OF_DEATH, target))
    {
        _metrics.touchOfDeathCasts++;
        _lastTouchOfDeathTime = getMSTime();
        uint32 damage = 10000; // Placeholder high damage
        _metrics.totalDamageDealt += damage;
        LogWindwalkerDecision("Cast Touch of Death", "Execute ability");
    }
}

void WindwalkerSpecialization::CastStormEarthAndFire()
{
    if (CastSpell(STORM_EARTH_AND_FIRE))
    {
        _metrics.stormEarthAndFireActivations++;
        _lastStormEarthAndFireTime = getMSTime();
        _combo.stormEarthAndFireActive = true;
        LogWindwalkerDecision("Cast Storm, Earth, and Fire", "Burst window activation");
    }
}

void WindwalkerSpecialization::CastExpelHarm()
{
    if (CastSpell(EXPEL_HARM))
    {
        GenerateChi(1); // Expel Harm generates chi
        LogWindwalkerDecision("Cast Expel Harm", "Chi generation and self-heal");
    }
}

void WindwalkerSpecialization::CastTouchOfKarma(::Unit* target)
{
    if (CastSpell(TOUCH_OF_KARMA, target))
    {
        LogWindwalkerDecision("Cast Touch of Karma", "Defensive absorption");
    }
}

void WindwalkerSpecialization::CastDiffuseMagic()
{
    if (CastSpell(DIFFUSE_MAGIC))
    {
        LogWindwalkerDecision("Cast Diffuse Magic", "Magic damage reduction");
    }
}

void WindwalkerSpecialization::CastDampenHarm()
{
    if (CastSpell(DAMPEN_HARM))
    {
        LogWindwalkerDecision("Cast Dampen Harm", "Physical damage reduction");
    }
}

// Combo system
uint32 WindwalkerSpecialization::GetComboPoints()
{
    return _combo.comboPower;
}

uint32 WindwalkerSpecialization::GetMaxComboPoints()
{
    return MAX_COMBO_POINTS;
}

void WindwalkerSpecialization::AddComboPoints(uint32 amount)
{
    _combo.comboPower = std::min(_combo.comboPower + amount, MAX_COMBO_POINTS);
    _metrics.comboPointsGenerated += amount;
}

void WindwalkerSpecialization::ResetComboPoints()
{
    _metrics.comboPointsSpent += _combo.comboPower;
    _combo.comboPower = 0;
}

ComboState WindwalkerSpecialization::GetComboState()
{
    if (_combo.comboPower >= OPTIMAL_COMBO_POINTS)
        return ComboState::READY_TO_SPEND;
    else if (_combo.comboPower > 0)
        return ComboState::BUILDING;
    else
        return ComboState::EMPTY;
}

// Mark of the Crane management
void WindwalkerSpecialization::ApplyMarkOfTheCrane(::Unit* target)
{
    if (!target)
        return;

    if (_markOfTheCraneTargets.find(target->GetGUID()) == _markOfTheCraneTargets.end())
    {
        _markOfTheCraneTargets[target->GetGUID()] = MARK_OF_CRANE_DURATION;
        _markOfTheCraneStacks++;
        _metrics.markOfTheCraneStacks = _markOfTheCraneStacks;
    }
    else
    {
        // Refresh duration
        _markOfTheCraneTargets[target->GetGUID()] = MARK_OF_CRANE_DURATION;
    }
}

uint32 WindwalkerSpecialization::GetMarkOfTheCraneStacks()
{
    return _markOfTheCraneStacks;
}

bool WindwalkerSpecialization::HasMarkOfTheCrane(::Unit* target)
{
    if (!target)
        return false;

    return _markOfTheCraneTargets.find(target->GetGUID()) != _markOfTheCraneTargets.end();
}

// Touch of Death optimization
bool WindwalkerSpecialization::ShouldUseTouchOfDeath(::Unit* target)
{
    if (!target)
        return false;

    // Use on targets below execute threshold
    return target->GetHealthPct() < EXECUTE_THRESHOLD * 100.0f;
}

bool WindwalkerSpecialization::CanExecuteTouchOfDeath(::Unit* target)
{
    return ShouldUseTouchOfDeath(target) && HasSpell(TOUCH_OF_DEATH) && CanUseAbility(TOUCH_OF_DEATH);
}

// Burst window management
void WindwalkerSpecialization::ActivateBurstWindow()
{
    _inBurstWindow = true;
    _burstWindowStart = getMSTime();
    _lastBurstActivation = getMSTime();

    // Activate Storm, Earth, and Fire
    if (HasSpell(STORM_EARTH_AND_FIRE) && CanUseAbility(STORM_EARTH_AND_FIRE))
    {
        CastStormEarthAndFire();
    }

    LogWindwalkerDecision("Activate Burst Window", "Maximize damage output");
}

bool WindwalkerSpecialization::ShouldActivateBurst()
{
    // Activate burst when we have resources and cooldowns available
    return GetChi() >= 3 && HasSpell(STORM_EARTH_AND_FIRE) && CanUseAbility(STORM_EARTH_AND_FIRE);
}

bool WindwalkerSpecialization::IsInBurstWindow()
{
    return _inBurstWindow;
}

void WindwalkerSpecialization::OptimizeBurstRotation(::Unit* target)
{
    if (!target)
        return;

    // Prioritize highest damage abilities during burst
    if (HasChi(3) && HasSpell(FISTS_OF_FURY) && CanUseAbility(FISTS_OF_FURY))
    {
        CastFistsOfFury(target);
        return;
    }

    if (HasChi(2) && HasSpell(RISING_SUN_KICK) && CanUseAbility(RISING_SUN_KICK))
    {
        CastRisingSunKick(target);
        return;
    }

    if (HasChi(1) && HasSpell(BLACKOUT_KICK) && CanUseAbility(BLACKOUT_KICK))
    {
        CastBlackoutKick(target);
        return;
    }
}

// AoE optimization
uint32 WindwalkerSpecialization::GetNearbyEnemyCount()
{
    return static_cast<uint32>(GetAoETargets(8.0f).size());
}

bool WindwalkerSpecialization::ShouldUseAoE()
{
    return GetNearbyEnemyCount() >= AOE_THRESHOLD;
}

// Target selection
::Unit* WindwalkerSpecialization::GetBestExecuteTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);

    for (::Unit* enemy : enemies)
    {
        if (enemy && ShouldUseTouchOfDeath(enemy))
            return enemy;
    }

    return nullptr;
}

::Unit* WindwalkerSpecialization::GetHighestPriorityTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);

    // Prioritize targets without Mark of the Crane
    for (::Unit* enemy : enemies)
    {
        if (enemy && !HasMarkOfTheCrane(enemy))
            return enemy;
    }

    // Fallback to any target
    return enemies.empty() ? nullptr : enemies[0];
}

void WindwalkerSpecialization::UseEmergencyDefensives()
{
    if (_bot->GetHealthPct() < 30.0f)
    {
        ::Unit* target = GetCurrentTarget();

        if (target && HasSpell(TOUCH_OF_KARMA) && CanUseAbility(TOUCH_OF_KARMA))
        {
            CastTouchOfKarma(target);
            return;
        }

        if (HasSpell(DIFFUSE_MAGIC) && CanUseAbility(DIFFUSE_MAGIC))
        {
            CastDiffuseMagic();
            return;
        }

        if (HasSpell(DAMPEN_HARM) && CanUseAbility(DAMPEN_HARM))
        {
            CastDampenHarm();
            return;
        }
    }
}

// System updates
void WindwalkerSpecialization::UpdateComboSystem()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastComboCheck < 1000) // Check every second
        return;

    _lastComboCheck = currentTime;

    ComboState state = GetComboState();

    // Update phase based on combo state
    if (state == ComboState::READY_TO_SPEND && GetChi() >= 2)
    {
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_SPENDING;
    }
    else if (state == ComboState::EMPTY && GetChi() < 2)
    {
        _windwalkerPhase = WindwalkerRotationPhase::CHI_GENERATION;
    }
    else if (state == ComboState::BUILDING)
    {
        _windwalkerPhase = WindwalkerRotationPhase::COMBO_BUILDING;
    }
}

void WindwalkerSpecialization::UpdateBurstWindows()
{
    // Check if we should activate burst
    if (!_inBurstWindow && ShouldActivateBurst())
    {
        _windwalkerPhase = WindwalkerRotationPhase::BURST_WINDOW;
    }

    // Check if we should use AoE
    if (ShouldUseAoE())
    {
        _windwalkerPhase = WindwalkerRotationPhase::AOE_ROTATION;
    }
}

void WindwalkerSpecialization::UpdateMarkOfTheCrane()
{
    // Mark tracking is handled in UpdateCooldowns
    RefreshMarkOfTheCrane();
}

void WindwalkerSpecialization::RefreshMarkOfTheCrane()
{
    // Apply marks to new targets when possible
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    for (::Unit* enemy : enemies)
    {
        if (enemy && !HasMarkOfTheCrane(enemy) && _markOfTheCraneStacks < _maxMarkTargets)
        {
            // We'll apply marks through Rising Sun Kick usage
            break;
        }
    }
}

void WindwalkerSpecialization::UpdateTouchOfDeath()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    for (::Unit* enemy : enemies)
    {
        if (enemy && ShouldUseTouchOfDeath(enemy))
        {
            _windwalkerPhase = WindwalkerRotationPhase::EXECUTE_PHASE;
            break;
        }
    }
}

void WindwalkerSpecialization::UpdateMobility()
{
    // Handle mobility and positioning
    ::Unit* target = GetCurrentTarget();
    if (target && !IsInMeleeRange(target))
    {
        // Use mobility abilities to close distance
        UseMobilityAbilities();
    }
}

void WindwalkerSpecialization::UseMobilityAbilities()
{
    // Use Roll or Chi Torpedo to close distance
    if (HasSpell(ROLL) && CanUseAbility(ROLL))
    {
        CastRoll();
    }
    else if (HasSpell(CHI_TORPEDO) && CanUseAbility(CHI_TORPEDO))
    {
        CastTeleport();
    }
}

void WindwalkerSpecialization::UpdateWindwalkerMetrics()
{
    uint32 combatTime = getMSTime() - _combatStartTime;
    if (combatTime > 0)
    {
        // Calculate DPS
        _metrics.averageDamagePerSecond = (float)_metrics.totalDamageDealt / (combatTime / 1000.0f);

        // Calculate combo uptime
        _metrics.comboUptime = _combo.comboPower > 0 ? (_metrics.comboUptime + 1.0f) / 2.0f : _metrics.comboUptime;

        // Calculate burst uptime
        _metrics.burstWindowUptime = _inBurstWindow ? (_metrics.burstWindowUptime + 1.0f) / 2.0f : _metrics.burstWindowUptime;

        // Calculate efficiency
        if (_metrics.comboPointsGenerated > 0)
        {
            _metrics.chiEfficiency = (float)_metrics.comboPointsSpent / _metrics.comboPointsGenerated;
        }

        _metrics.energyEfficiency = GetEnergyPercent();
    }
}

void WindwalkerSpecialization::AnalyzeDamageEfficiency()
{
    // Performance analysis every 10 seconds
    if (getMSTime() % 10000 == 0)
    {
        TC_LOG_DEBUG("playerbot", "WindwalkerSpecialization [{}]: Efficiency - DPS: {:.1f}, Chi: {:.1f}%, Combo Uptime: {:.1f}%",
                    _bot->GetName(), _metrics.averageDamagePerSecond, _metrics.chiEfficiency * 100, _metrics.comboUptime * 100);
    }
}

void WindwalkerSpecialization::AnalyzeComboEfficiency()
{
    // Track combo point efficiency
    if (_metrics.comboPointsGenerated > 0)
    {
        float efficiency = (float)_metrics.comboPointsSpent / _metrics.comboPointsGenerated;
        if (efficiency < _comboEfficiencyTarget)
        {
            // Adjust combo building strategy
            _prioritizeComboBuilding = true;
        }
    }
}

void WindwalkerSpecialization::LogWindwalkerDecision(const std::string& decision, const std::string& reason)
{
    LogRotationDecision(decision, reason);
}

} // namespace Playerbot