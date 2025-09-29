/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarriorAI.h"
#include "Player.h"
#include "Log.h"

namespace Playerbot
{

WarriorAI::WarriorAI(Player* bot) : ClassAI(bot)
{
    _lastStanceChange = 0;
    _lastBattleShout = 0;
    _lastCommandingShout = 0;
    _needsIntercept = false;
    _needsCharge = false;
    _lastChargeTarget = nullptr;
    _lastChargeTime = 0;

    InitializeSpecialization();

    TC_LOG_DEBUG("module.playerbot.ai", "WarriorAI created for player {}",
                 bot ? bot->GetName() : "null");
}

void WarriorAI::UpdateRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Delegate to specialization
    DelegateToSpecialization(target);

    // Handle warrior-specific abilities
    UseChargeAbilities(target);
    UpdateAdvancedCombatLogic(target);
}

void WarriorAI::UpdateBuffs()
{
    UpdateWarriorBuffs();
}

void WarriorAI::UpdateCooldowns(uint32 diff)
{
    UpdateMetrics(diff);
}

bool WarriorAI::CanUseAbility(uint32 spellId)
{
    return IsSpellReady(spellId) && HasEnoughResource(spellId);
}

void WarriorAI::OnCombatStart(::Unit* target)
{
    _warriorMetrics.combatStartTime = std::chrono::steady_clock::now();

    if (_specialization)
    {
        // Delegate to specialization
    }
}

void WarriorAI::OnCombatEnd()
{
    AnalyzeCombatEffectiveness();

    if (_specialization)
    {
        // Delegate to specialization
    }
}

bool WarriorAI::HasEnoughResource(uint32 spellId)
{
    // Check rage requirements for warrior abilities
    return true; // Simplified for now
}

void WarriorAI::ConsumeResource(uint32 spellId)
{
    // Consume rage for warrior abilities
    RecordAbilityUsage(spellId);
}

Position WarriorAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !GetBot())
        return Position();

    return CalculateOptimalChargePosition(target);
}

float WarriorAI::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_MELEE_RANGE;
}

void WarriorAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    // Specialization creation will be implemented when classes are available
}

void WarriorAI::UpdateSpecialization()
{
    WarriorSpec newSpec = DetectCurrentSpecialization();
    if (newSpec != _currentSpec)
    {
        SwitchSpecialization(newSpec);
    }
}

WarriorSpec WarriorAI::DetectCurrentSpecialization()
{
    // Detect from talents - default to Arms for now
    return WarriorSpec::ARMS;
}

void WarriorAI::SwitchSpecialization(WarriorSpec newSpec)
{
    if (_currentSpec != newSpec)
    {
        _currentSpec = newSpec;
        InitializeSpecialization();
    }
}

void WarriorAI::DelegateToSpecialization(::Unit* target)
{
    // Delegate to specialization-specific logic
}

void WarriorAI::UpdateWarriorBuffs()
{
    CastBattleShout();
    CastCommandingShout();
}

void WarriorAI::CastBattleShout()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBattleShout > BATTLE_SHOUT_DURATION)
    {
        // Cast Battle Shout if available
        _lastBattleShout = currentTime;
    }
}

void WarriorAI::CastCommandingShout()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastCommandingShout > COMMANDING_SHOUT_DURATION)
    {
        // Cast Commanding Shout if available
        _lastCommandingShout = currentTime;
    }
}

void WarriorAI::UseChargeAbilities(::Unit* target)
{
    if (!target)
        return;

    if (CanCharge(target))
    {
        // Use charge abilities
        _needsCharge = true;
    }
}

bool WarriorAI::IsInMeleeRange(::Unit* target) const
{
    if (!target || !GetBot())
        return false;

    return GetBot()->GetDistance(target) <= OPTIMAL_MELEE_RANGE;
}

bool WarriorAI::CanCharge(::Unit* target) const
{
    if (!target || !GetBot())
        return false;

    float distance = GetBot()->GetDistance(target);
    return distance >= CHARGE_MIN_RANGE && distance <= CHARGE_MAX_RANGE;
}

void WarriorAI::UpdateAdvancedCombatLogic(::Unit* target)
{
    // Advanced combat logic
    OptimizeStanceDancing(target);
    ManageRageEfficiency();
}

void WarriorAI::OptimizeStanceDancing(::Unit* target)
{
    // Stance optimization logic
}

void WarriorAI::ManageRageEfficiency()
{
    // Rage management logic
}

void WarriorAI::RecordAbilityUsage(uint32 spellId)
{
    _abilityUsage[spellId]++;
    _warriorMetrics.totalAbilitiesUsed++;
}

void WarriorAI::AnalyzeCombatEffectiveness()
{
    // Analyze combat performance
}

void WarriorAI::UpdateMetrics(uint32 diff)
{
    _warriorMetrics.lastMetricsUpdate = std::chrono::steady_clock::now();
}

float WarriorAI::CalculateRageEfficiency()
{
    return RAGE_EFFICIENCY_TARGET;
}

Position WarriorAI::CalculateOptimalChargePosition(::Unit* target)
{
    if (!target || !GetBot())
        return Position();

    return GetBot()->GetPosition();
}

bool WarriorAI::IsValidTarget(::Unit* target)
{
    return target && target->IsAlive() && GetBot()->IsValidAttackTarget(target);
}

::Unit* WarriorAI::GetBestChargeTarget()
{
    // Find best charge target
    return GetBestAttackTarget();
}

} // namespace Playerbot