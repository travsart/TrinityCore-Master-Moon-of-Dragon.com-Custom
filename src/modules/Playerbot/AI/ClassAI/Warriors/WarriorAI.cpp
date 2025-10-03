/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarriorAI.h"
#include "ArmsWarriorRefactored.h"
#include "FuryWarriorRefactored.h"
#include "ProtectionWarriorRefactored.h"
#include "../BaselineRotationManager.h"
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

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        // Use baseline rotation manager for unspecialized bots
        static BaselineRotationManager baselineManager;

        // Try auto-specialization if level 10+
        baselineManager.HandleAutoSpecialization(GetBot());

        // Execute baseline rotation
        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))
            return;

        // Fallback to charge if nothing else worked
        UseChargeAbilities(target);
        return;
    }

    // Delegate to specialization for level 10+ bots with spec
    DelegateToSpecialization(target);

    // Handle warrior-specific abilities
    UseChargeAbilities(target);
    UpdateAdvancedCombatLogic(target);
}

void WarriorAI::UpdateBuffs()
{
    // Check if bot should use baseline buffs
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(GetBot());
        return;
    }

    // Use full warrior buff system for specialized bots
    UpdateWarriorBuffs();
}

void WarriorAI::UpdateCooldowns(uint32 diff)
{
    UpdateMetrics(diff);

    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool WarriorAI::CanUseAbility(uint32 spellId)
{
    if (!IsSpellReady(spellId) || !HasEnoughResource(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void WarriorAI::OnCombatStart(::Unit* target)
{
    _warriorMetrics.combatStartTime = std::chrono::steady_clock::now();

    if (_specialization)
        _specialization->OnCombatStart(target);
}

void WarriorAI::OnCombatEnd()
{
    AnalyzeCombatEffectiveness();

    if (_specialization)
        _specialization->OnCombatEnd();
}

bool WarriorAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    // Fallback: check if we have any rage
    return GetBot()->GetPower(POWER_RAGE) >= 10;
}

void WarriorAI::ConsumeResource(uint32 spellId)
{
    RecordAbilityUsage(spellId);

    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position WarriorAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !GetBot())
        return Position();

    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return CalculateOptimalChargePosition(target);
}

float WarriorAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);

    return OPTIMAL_MELEE_RANGE;
}

void WarriorAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
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
    _currentSpec = newSpec;

    // TODO: Re-enable refactored specialization classes once template issues are fixed
    _specialization = nullptr;
    TC_LOG_WARN("module.playerbot.warrior", "Warrior specialization switching temporarily disabled for {}",
                 GetBot()->GetName());

    /* switch (newSpec)
    {
        case WarriorSpec::ARMS:
            _specialization = std::make_unique<ArmsWarriorRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warrior", "Warrior {} switched to Arms specialization",
                         GetBot()->GetName());
            break;
        case WarriorSpec::FURY:
            _specialization = std::make_unique<FuryWarriorRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warrior", "Warrior {} switched to Fury specialization",
                         GetBot()->GetName());
            break;
        case WarriorSpec::PROTECTION:
            _specialization = std::make_unique<ProtectionWarriorRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warrior", "Warrior {} switched to Protection specialization",
                         GetBot()->GetName());
            break;
        default:
            _specialization = std::make_unique<ArmsWarriorRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warrior", "Warrior {} defaulted to Arms specialization",
                         GetBot()->GetName());
            break;
    } */
}

void WarriorAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void WarriorAI::UpdateWarriorBuffs()
{
    CastBattleShout();
    CastCommandingShout();

    if (_specialization)
        _specialization->UpdateBuffs();
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