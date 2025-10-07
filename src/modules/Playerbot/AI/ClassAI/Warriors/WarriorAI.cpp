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
#include "../../Combat/CombatBehaviorIntegration.h"
#include "Player.h"
#include "Log.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include "Unit.h"
#include "SpellAuras.h"

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

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Priority-based decision making
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Priority 1: Handle interrupts (Pummel)
    if (behaviors && behaviors->ShouldInterrupt(target))
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (interruptTarget && CanUseAbility(PUMMEL))
        {
            // Cast Pummel on the interrupt target
            if (CastSpell(interruptTarget, PUMMEL))
            {
                RecordInterruptAttempt(interruptTarget, PUMMEL, true);
                TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} interrupted {} with Pummel",
                             GetBot()->GetName(), interruptTarget->GetName());
                return;
            }
        }
    }

    // Priority 2: Handle defensives (Shield Wall, Last Stand, etc.)
    if (behaviors && behaviors->NeedsDefensive())
    {
        // Use defensive cooldowns when health is critical
        UseDefensiveCooldowns();
        if (GetBot()->HasUnitState(UNIT_STATE_CASTING))
            return;
    }

    // Priority 3: Check for target switching
    if (behaviors && behaviors->ShouldSwitchTarget())
    {
        Unit* priorityTarget = behaviors->GetPriorityTarget();
        if (priorityTarget && priorityTarget != target)
        {
            OnTargetChanged(priorityTarget);
            target = priorityTarget;
            TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} switching target to {}",
                         GetBot()->GetName(), priorityTarget->GetName());
        }
    }

    // Priority 4: AoE vs Single-Target decision
    if (behaviors && behaviors->ShouldAOE())
    {
        // Whirlwind for AoE damage
        if (CanUseAbility(WHIRLWIND))
        {
            if (CastSpell(WHIRLWIND))
            {
                RecordAbilityUsage(WHIRLWIND);
                TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} using Whirlwind for AoE",
                             GetBot()->GetName());
                return;
            }
        }

        // Thunder Clap for threat and slow
        if (CanUseAbility(THUNDER_CLAP))
        {
            if (CastSpell(THUNDER_CLAP))
            {
                RecordAbilityUsage(THUNDER_CLAP);
                TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} using Thunder Clap for AoE",
                             GetBot()->GetName());
                return;
            }
        }

        // Bladestorm for massive AoE
        if (CanUseAbility(BLADESTORM))
        {
            if (CastSpell(BLADESTORM))
            {
                RecordAbilityUsage(BLADESTORM);
                TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Bladestorm",
                             GetBot()->GetName());
                return;
            }
        }
    }

    // Priority 5: Use major cooldowns at optimal time
    if (behaviors && behaviors->ShouldUseCooldowns())
    {
        // Recklessness for damage boost
        if (CanUseAbility(RECKLESSNESS))
        {
            if (CastSpell(RECKLESSNESS))
            {
                RecordAbilityUsage(RECKLESSNESS);
                TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Recklessness",
                             GetBot()->GetName());
            }
        }

        // Avatar for overall boost
        if (CanUseAbility(AVATAR))
        {
            if (CastSpell(AVATAR))
            {
                RecordAbilityUsage(AVATAR);
                TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Avatar",
                             GetBot()->GetName());
            }
        }
    }

    // Priority 6: Check positioning requirements
    if (behaviors && behaviors->NeedsRepositioning())
    {
        Position optimalPos = behaviors->GetOptimalPosition();
        // Movement will be handled by BotAI movement strategies
        // Just flag that we need to move
        _needsIntercept = true;
        _needsCharge = true;
    }

    // Priority 7: Execute normal rotation through specialization
    if (_specialization)
    {
        _specialization->UpdateRotation(target);
    }
    else
    {
        // Fallback rotation when no specialization is available
        ExecuteBasicWarriorRotation(target);
    }

    // Handle warrior-specific abilities that don't conflict with behaviors
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

void WarriorAI::ExecuteBasicWarriorRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Basic rotation for warriors without specialization
    // Priority: Maintain debuffs -> Generate rage -> Spend rage

    // Apply Sunder Armor for armor reduction
    if (CanUseAbility(SUNDER_ARMOR))
    {
        if (CastSpell(target, SUNDER_ARMOR))
        {
            RecordAbilityUsage(SUNDER_ARMOR);
            return;
        }
    }

    // Apply Rend for bleed damage
    if (CanUseAbility(REND))
    {
        if (!target->HasAura(REND, GetBot()->GetGUID()))
        {
            if (CastSpell(target, REND))
            {
                RecordAbilityUsage(REND);
                return;
            }
        }
    }

    // Use Execute if target is low health
    if (target->GetHealthPct() < 20.0f && CanUseAbility(EXECUTE))
    {
        if (CastSpell(target, EXECUTE))
        {
            RecordAbilityUsage(EXECUTE);
            return;
        }
    }

    // Use Overpower if available
    if (CanUseAbility(OVERPOWER))
    {
        if (CastSpell(target, OVERPOWER))
        {
            RecordAbilityUsage(OVERPOWER);
            return;
        }
    }

    // Use Mortal Strike or Bloodthirst if available
    if (CanUseAbility(MORTAL_STRIKE))
    {
        if (CastSpell(target, MORTAL_STRIKE))
        {
            RecordAbilityUsage(MORTAL_STRIKE);
            return;
        }
    }

    if (CanUseAbility(BLOODTHIRST))
    {
        if (CastSpell(target, BLOODTHIRST))
        {
            RecordAbilityUsage(BLOODTHIRST);
            return;
        }
    }

    // Rage dump with Heroic Strike or Cleave
    if (GetBot()->GetPower(POWER_RAGE) > RAGE_DUMP_THRESHOLD)
    {
        // Use Cleave if multiple enemies
        if (GetNearbyEnemyCount(8.0f) > 1 && CanUseAbility(CLEAVE))
        {
            if (CastSpell(CLEAVE))
            {
                RecordAbilityUsage(CLEAVE);
                return;
            }
        }

        // Otherwise use Heroic Strike
        if (CanUseAbility(HEROIC_STRIKE))
        {
            if (CastSpell(HEROIC_STRIKE))
            {
                RecordAbilityUsage(HEROIC_STRIKE);
                return;
            }
        }
    }
}

void WarriorAI::RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success)
{
    if (success)
    {
        _successfulInterrupts++;
        TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} successfully interrupted with spell {}",
                     GetBot()->GetName(), spellId);
    }
}

void WarriorAI::UseDefensiveCooldowns()
{
    if (!GetBot())
        return;

    float healthPct = GetBot()->GetHealthPct();

    // Shield Wall at critical health
    if (healthPct < HEALTH_EMERGENCY_THRESHOLD && CanUseAbility(SHIELD_WALL))
    {
        if (CastSpell(SHIELD_WALL))
        {
            RecordAbilityUsage(SHIELD_WALL);
            TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Shield Wall",
                         GetBot()->GetName());
            return;
        }
    }

    // Last Stand for health boost
    if (healthPct < DEFENSIVE_COOLDOWN_THRESHOLD && CanUseAbility(LAST_STAND))
    {
        if (CastSpell(LAST_STAND))
        {
            RecordAbilityUsage(LAST_STAND);
            TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Last Stand",
                         GetBot()->GetName());
            return;
        }
    }

    // Shield Block for mitigation (Protection warriors)
    if (healthPct < 60.0f && CanUseAbility(SHIELD_BLOCK))
    {
        if (CastSpell(SHIELD_BLOCK))
        {
            RecordAbilityUsage(SHIELD_BLOCK);
            TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Shield Block",
                         GetBot()->GetName());
            return;
        }
    }

    // Spell Reflection against casters
    Unit* target = GetBot()->GetSelectedUnit();
    if (target && target->HasUnitState(UNIT_STATE_CASTING) && CanUseAbility(SPELL_REFLECTION))
    {
        if (CastSpell(SPELL_REFLECTION))
        {
            RecordAbilityUsage(SPELL_REFLECTION);
            TC_LOG_DEBUG("module.playerbot.ai", "Warrior {} activated Spell Reflection",
                         GetBot()->GetName());
            return;
        }
    }
}

uint32 WarriorAI::GetNearbyEnemyCount(float range) const
{
    if (!GetBot())
        return 0;

    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, range);

    for (auto& target : targets)
    {
        if (GetBot()->IsValidAttackTarget(target))
            count++;
    }

    return count;
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