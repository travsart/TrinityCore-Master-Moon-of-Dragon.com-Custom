/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarriorAI.h"
#include "ArmsSpecialization.h"
#include "FurySpecialization.h"
#include "ProtectionSpecialization.h"
#include "Unit.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Log.h"
#include <algorithm>
#include <numeric>

namespace Playerbot
{

// Enhanced Warrior AI Implementation

void WarriorAI::UpdateAdvancedCombatLogic(::Unit* target)
{
    if (!target)
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Update combat systems
    if (_threatManager)
        _threatManager->UpdateThreatAnalysis();
    if (_positionManager)
    {
        PositionManager::MovementContext context{target, GetBot()};
        context.role = PositionManager::MovementRole::MELEE_DPS;
        context.preferredRange = OPTIMAL_MELEE_RANGE;
        _positionManager->UpdatePosition(context);
    }
    if (_interruptManager)
        _interruptManager->UpdateInterruptSystem(100);

    // Threat analysis and management
    if (_threatManager)
    {
        ThreatManager::ThreatAnalysis threatAnalysis = _threatManager->AnalyzeThreatSituation();

        if (threatAnalysis.threatLevel > ThreatManager::ThreatLevel::MODERATE)
        {
            HandleDefensiveSituation();
        }
    }

    // Rage management optimization
    ManageRageEfficiency();

    // Stance optimization
    OptimizeStanceForSituation(target);

    // Advanced target selection
    ::Unit* optimalTarget = SelectOptimalTarget(GetNearbyEnemies(THREAT_MANAGEMENT_RANGE));
    if (optimalTarget && optimalTarget != target)
        target = optimalTarget;

    // Multi-target scenario handling
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(10.0f);
    if (nearbyEnemies.size() >= MULTI_TARGET_THRESHOLD)
    {
        HandleMultipleEnemies(nearbyEnemies);
    }

    // Group combat coordination
    if (GetBot()->GetGroup())
    {
        HandleGroupCombatRole();
        CoordinateWithGroup();
    }

    // Performance metrics update
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Track execution efficiency
    uint32 callCount = _warriorMetrics.totalAbilitiesUsed.load();
    if (callCount > 0)
    {
        float avgEfficiency = _warriorMetrics.averageRageEfficiency.load();
        float currentEfficiency = CalculateRageEfficiency();
        float newAvg = (avgEfficiency * 0.9f) + (currentEfficiency * 0.1f);
        _warriorMetrics.averageRageEfficiency.store(newAvg);
    }
}

::Unit* WarriorAI::SelectOptimalTarget(const std::vector<::Unit*>& enemies)
{
    if (enemies.empty())
        return nullptr;

    if (!_targetSelector)
        return enemies.front();

    TargetSelector::SelectionContext context;
    context.currentTarget = GetBot()->GetTarget();
    context.maxRange = THREAT_MANAGEMENT_RANGE;

    // Role-based targeting
    if (_currentSpec == WarriorSpec::PROTECTION)
        context.role = TargetSelector::SelectionRole::TANK;
    else
        context.role = TargetSelector::SelectionRole::MELEE_DPS;

    context.prioritizeInterrupts = true;

    TargetSelector::SelectionResult result = _targetSelector->SelectBestTarget(context);

    if (result.success && result.target)
        return result.target;

    // Fallback to priority-based selection
    ::Unit* bestTarget = nullptr;
    float bestPriority = 0.0f;

    for (::Unit* enemy : enemies)
    {
        if (!enemy || !enemy->IsAlive())
            continue;

        float priority = CalculateTargetPriority(enemy);

        if (priority > bestPriority)
        {
            bestPriority = priority;
            bestTarget = enemy;
        }
    }

    return bestTarget;
}

float WarriorAI::CalculateTargetPriority(::Unit* target)
{
    if (!target)
        return 0.0f;

    float priority = 1.0f;

    // Role-specific priorities
    if (_currentSpec == WarriorSpec::PROTECTION)
    {
        // Tank priorities: highest threat generators
        if (target->HasUnitState(UNIT_STATE_CASTING))
            priority += 3.0f;

        // Priority for untanked enemies
        if (target->GetTarget() != GetBot())
            priority += 2.0f;

        // Priority for high damage dealers
        if (target->GetCreatureType() == CREATURE_TYPE_HUMANOID)
            priority += 1.5f;
    }
    else
    {
        // DPS priorities: low health, high value targets
        float healthPct = target->GetHealthPct();
        if (healthPct < 30.0f)
            priority += 2.5f;
        else if (healthPct < 60.0f)
            priority += 1.5f;

        // Priority for casters
        if (target->HasUnitState(UNIT_STATE_CASTING))
            priority += 2.0f;

        // Priority for closer targets (charge opportunities)
        float distance = GetBot()->GetDistance(target);
        if (distance >= CHARGE_MIN_RANGE && distance <= CHARGE_MAX_RANGE)
            priority += 1.5f;
    }

    // Lower priority for heavily armored targets (if DPS)
    if (_currentSpec != WarriorSpec::PROTECTION && target->GetArmor() > 8000)
        priority -= 0.5f;

    return priority;
}

void WarriorAI::HandleMultipleEnemies(const std::vector<::Unit*>& enemies)
{
    if (enemies.size() < MULTI_TARGET_THRESHOLD)
        return;

    // Use AoE abilities appropriate to specialization
    switch (_currentSpec)
    {
        case WarriorSpec::ARMS:
            // Sweeping Strikes + Whirlwind combo
            if (CanUseAbility(SWEEPING_STRIKES) && !GetBot()->HasAura(SWEEPING_STRIKES))
            {
                GetBot()->CastSpell(GetBot(), SWEEPING_STRIKES, false);
                RecordAbilityUsage(SWEEPING_STRIKES);
            }

            if (CanUseAbility(WHIRLWIND) && HasEnoughRage(25))
            {
                GetBot()->CastSpell(GetBot(), WHIRLWIND, false);
                RecordAbilityUsage(WHIRLWIND);
                _rageSpent += 25;
            }
            break;

        case WarriorSpec::FURY:
            // Whirlwind for Fury
            if (CanUseAbility(WHIRLWIND) && HasEnoughRage(25))
            {
                GetBot()->CastSpell(GetBot(), WHIRLWIND, false);
                RecordAbilityUsage(WHIRLWIND);
                _rageSpent += 25;
            }
            break;

        case WarriorSpec::PROTECTION:
            // Thunder Clap for threat on multiple targets
            if (CanUseAbility(THUNDER_CLAP) && HasEnoughRage(20))
            {
                GetBot()->CastSpell(GetBot(), THUNDER_CLAP, false);
                RecordAbilityUsage(THUNDER_CLAP);
                _rageSpent += 20;
                _threatGenerated += enemies.size() * 100; // Estimated threat per target
            }

            // Demoralizing Shout for damage reduction
            if (CanUseAbility(DEMORALIZING_SHOUT) && !GetBot()->HasAura(DEMORALIZING_SHOUT))
            {
                GetBot()->CastSpell(GetBot(), DEMORALIZING_SHOUT, false);
                RecordAbilityUsage(DEMORALIZING_SHOUT);
            }
            break;
    }

    // Cleave when appropriate
    if (enemies.size() >= 2 && CanUseAbility(CLEAVE) && HasEnoughRage(20))
    {
        ::Unit* primaryTarget = enemies.front();
        if (primaryTarget && IsInMeleeRange(primaryTarget))
        {
            GetBot()->CastSpell(primaryTarget, CLEAVE, false);
            RecordAbilityUsage(CLEAVE);
            _rageSpent += 20;
        }
    }

    // Consider defensive positioning for Protection
    if (_currentSpec == WarriorSpec::PROTECTION && _positionManager)
    {
        PositionManager::MovementContext context{enemies.front(), GetBot()};
        context.role = PositionManager::MovementRole::TANK;
        context.preferredRange = OPTIMAL_MELEE_RANGE;
        context.avoidAoE = true;
        _positionManager->UpdatePosition(context);
    }
}

void WarriorAI::OptimizeStanceForSituation(::Unit* target)
{
    if (!target)
        return;

    uint32 currentTime = getMSTime();
    static uint32 lastStanceOptimization = 0;

    if (currentTime - lastStanceOptimization < STANCE_OPTIMIZATION_INTERVAL)
        return;

    lastStanceOptimization = currentTime;

    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(15.0f);
    WarriorStance optimalStance = DetermineOptimalStance(target, nearbyEnemies);
    WarriorStance currentStance = GetCurrentStance();

    if (optimalStance != currentStance)
    {
        OptimizeStanceDancing(target);
    }
}

WarriorStance WarriorAI::DetermineOptimalStance(::Unit* target, const std::vector<::Unit*>& enemies)
{
    if (!target)
        return WarriorStance::BATTLE;

    // Specialization-based stance preferences
    switch (_currentSpec)
    {
        case WarriorSpec::PROTECTION:
            // Protection should prioritize Defensive Stance
            if (enemies.size() > 1 || GetBot()->GetHealthPct() < 60.0f)
                return WarriorStance::DEFENSIVE;
            else
                return WarriorStance::BATTLE;

        case WarriorSpec::FURY:
            // Fury benefits from Berserker Stance
            if (GetBot()->GetHealthPct() > 40.0f)
                return WarriorStance::BERSERKER;
            else
                return WarriorStance::BATTLE;

        case WarriorSpec::ARMS:
            // Arms is flexible - choose based on situation
            if (enemies.size() > 2)
                return WarriorStance::BERSERKER; // For AoE damage
            else if (GetBot()->GetHealthPct() < 50.0f)
                return WarriorStance::DEFENSIVE; // For survivability
            else
                return WarriorStance::BATTLE; // Default for Arms
    }

    return WarriorStance::BATTLE;
}

void WarriorAI::OptimizeStanceDancing(::Unit* target)
{
    if (!target)
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastStanceChange < STANCE_CHANGE_COOLDOWN)
        return;

    WarriorStance currentStance = GetCurrentStance();
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(15.0f);
    WarriorStance optimalStance = DetermineOptimalStance(target, nearbyEnemies);

    if (optimalStance != currentStance)
    {
        // Handle Tactical Mastery (retain rage when stance dancing)
        uint32 rageBeforeSwitch = GetRage();

        switch (optimalStance)
        {
            case WarriorStance::BATTLE:
                GetBot()->CastSpell(GetBot(), BATTLE_STANCE, false);
                break;
            case WarriorStance::DEFENSIVE:
                GetBot()->CastSpell(GetBot(), DEFENSIVE_STANCE, false);
                break;
            case WarriorStance::BERSERKER:
                GetBot()->CastSpell(GetBot(), BERSERKER_STANCE, false);
                break;
            default:
                return;
        }

        _lastStanceChange = currentTime;
        RecordStanceChange(currentStance, optimalStance);
        _warriorMetrics.successfulStanceChanges++;

        TC_LOG_DEBUG("playerbot.warrior", "Stance changed from {} to {} for optimal combat",
                     static_cast<uint32>(currentStance), static_cast<uint32>(optimalStance));
    }
}

void WarriorAI::ManageRageEfficiency()
{
    uint32 currentRage = GetRage();
    float ragePercent = static_cast<float>(currentRage) / GetMaxRage();

    // Rage starvation handling
    if (currentRage < RAGE_CONSERVATION_THRESHOLD)
    {
        HandleRageStarvation();
        return;
    }

    // Rage dump when capped
    if (currentRage > RAGE_DUMP_THRESHOLD)
    {
        _warriorMetrics.rageDumpInstances++;

        // Use rage efficiently based on specialization
        if (_currentSpec == WarriorSpec::PROTECTION)
        {
            // Use threat-generating abilities
            if (CanUseAbility(SHIELD_SLAM))
            {
                ::Unit* target = GetBot()->GetTarget();
                if (target)
                {
                    GetBot()->CastSpell(target, SHIELD_SLAM, false);
                    RecordAbilityUsage(SHIELD_SLAM);
                    _rageSpent += 20;
                }
            }
        }
        else
        {
            // Use damage abilities
            if (CanUseAbility(HEROIC_STRIKE))
            {
                ::Unit* target = GetBot()->GetTarget();
                if (target && IsInMeleeRange(target))
                {
                    GetBot()->CastSpell(target, HEROIC_STRIKE, false);
                    RecordAbilityUsage(HEROIC_STRIKE);
                    _rageSpent += 15;
                }
            }
        }
    }

    // Update efficiency metrics
    float efficiency = CalculateRageEfficiency();
    _warriorMetrics.averageRageEfficiency =
        (_warriorMetrics.averageRageEfficiency.load() * 0.9f) + (efficiency * 0.1f);
}

float WarriorAI::CalculateRageEfficiency()
{
    uint32 totalRageSpent = _rageSpent.load();
    uint32 totalDamageDealt = _damageDealt.load();

    if (totalRageSpent == 0)
        return 0.0f;

    // Calculate damage per rage point
    float damagePerRage = static_cast<float>(totalDamageDealt) / totalRageSpent;

    // Normalize to a 0-1 scale (assuming 10 damage per rage is baseline)
    float efficiency = std::min(1.0f, damagePerRage / 10.0f);

    return efficiency;
}

void WarriorAI::HandleRageStarvation()
{
    // Strategies for when rage is low

    // Switch to more rage-efficient stance if needed
    WarriorStance currentStance = GetCurrentStance();
    if (currentStance != WarriorStance::BERSERKER && _currentSpec != WarriorSpec::PROTECTION)
    {
        // Berserker stance generates more rage from damage taken
        OptimizeStanceDancing(GetBot()->GetTarget());
    }

    // Use charge abilities to generate rage
    ::Unit* target = GetBot()->GetTarget();
    if (target)
    {
        float distance = GetBot()->GetDistance(target);
        if (distance >= CHARGE_MIN_RANGE && distance <= CHARGE_MAX_RANGE)
        {
            if (CanUseAbility(CHARGE))
            {
                HandleChargeOpportunities(target);
            }
            else if (CanUseAbility(INTERCEPT))
            {
                ManageInterceptUsage(target);
            }
        }
    }

    // Consider using Bloodrage if available (generates rage)
    uint32 bloodrageSpell = 2687;
    if (CanUseAbility(bloodrageSpell) && GetBot()->GetHealthPct() > 50.0f)
    {
        GetBot()->CastSpell(GetBot(), bloodrageSpell, false);
        RecordAbilityUsage(bloodrageSpell);
        TC_LOG_DEBUG("playerbot.warrior", "Using Bloodrage for rage generation");
    }
}

void WarriorAI::HandleDefensiveSituation()
{
    float healthPct = GetBot()->GetHealthPct();

    // Critical health - use emergency abilities
    if (healthPct < HEALTH_EMERGENCY_THRESHOLD)
    {
        // Last Stand for emergency health
        if (CanUseAbility(LAST_STAND) && !GetBot()->HasAura(LAST_STAND))
        {
            GetBot()->CastSpell(GetBot(), LAST_STAND, false);
            RecordAbilityUsage(LAST_STAND);
            TC_LOG_DEBUG("playerbot.warrior", "Emergency Last Stand activated");
        }

        // Enraged Regeneration for healing
        if (CanUseAbility(ENRAGED_REGENERATION) && !GetBot()->HasAura(ENRAGED_REGENERATION))
        {
            GetBot()->CastSpell(GetBot(), ENRAGED_REGENERATION, false);
            RecordAbilityUsage(ENRAGED_REGENERATION);
        }
    }
    // Moderate danger - use defensive cooldowns
    else if (healthPct < DEFENSIVE_COOLDOWN_THRESHOLD)
    {
        UseDefensiveCooldowns();
    }

    // Switch to defensive stance if not Protection
    if (_currentSpec != WarriorSpec::PROTECTION)
    {
        WarriorStance currentStance = GetCurrentStance();
        if (currentStance != WarriorStance::DEFENSIVE)
        {
            OptimizeStanceDancing(GetBot()->GetTarget());
        }
    }
}

void WarriorAI::UseDefensiveCooldowns()
{
    // Shield Wall for damage reduction
    if (CanUseAbility(SHIELD_WALL) && !GetBot()->HasAura(SHIELD_WALL))
    {
        GetBot()->CastSpell(GetBot(), SHIELD_WALL, false);
        RecordAbilityUsage(SHIELD_WALL);
        TC_LOG_DEBUG("playerbot.warrior", "Shield Wall activated for damage reduction");
    }

    // Spell Reflection for magic attacks
    std::vector<::Unit*> casters = GetNearbyCasters(20.0f);
    if (!casters.empty() && CanUseAbility(SPELL_REFLECTION))
    {
        GetBot()->CastSpell(GetBot(), SPELL_REFLECTION, false);
        RecordAbilityUsage(SPELL_REFLECTION);
        TC_LOG_DEBUG("playerbot.warrior", "Spell Reflection activated against casters");
    }

    // Shield Block if Protection and has shield
    if (_currentSpec == WarriorSpec::PROTECTION && CanUseAbility(SHIELD_BLOCK))
    {
        if (HasShieldEquipped())
        {
            GetBot()->CastSpell(GetBot(), SHIELD_BLOCK, false);
            RecordAbilityUsage(SHIELD_BLOCK);
        }
    }
}

void WarriorAI::HandleChargeOpportunities(::Unit* target)
{
    if (!target)
        return;

    float distance = GetBot()->GetDistance(target);

    if (distance < CHARGE_MIN_RANGE || distance > CHARGE_MAX_RANGE)
        return;

    if (!CanUseAbility(CHARGE))
        return;

    // Calculate optimal charge position
    Position chargePos = CalculateOptimalChargePosition(target);

    if (chargePos.IsPositionValid())
    {
        GetBot()->CastSpell(target, CHARGE, false);
        RecordChargeSuccess(target, true);
        _successfulCharges++;

        TC_LOG_DEBUG("playerbot.warrior", "Charge executed against {} at distance {:.1f}",
                     target->GetName(), distance);
    }
    else
    {
        RecordChargeSuccess(target, false);
    }
}

Position WarriorAI::CalculateOptimalChargePosition(::Unit* target)
{
    if (!target)
        return Position();

    // Calculate position slightly behind target for optimal melee positioning
    float targetOrientation = target->GetOrientation();
    Position targetPos = target->GetPosition();

    // Position 2 yards behind target
    float behindX = targetPos.GetPositionX() - (2.0f * cos(targetOrientation));
    float behindY = targetPos.GetPositionY() - (2.0f * sin(targetOrientation));
    float behindZ = targetPos.GetPositionZ();

    Position optimalPos(behindX, behindY, behindZ, targetOrientation);

    // Validate position is accessible
    if (GetBot()->GetMap()->IsInLineOfSight(GetBot()->GetPositionX(), GetBot()->GetPositionY(), GetBot()->GetPositionZ(),
                                          behindX, behindY, behindZ))
    {
        return optimalPos;
    }

    // Fallback to target's position
    return targetPos;
}

void WarriorAI::ManageInterceptUsage(::Unit* target)
{
    if (!target)
        return;

    float distance = GetBot()->GetDistance(target);

    if (distance < INTERCEPT_MIN_RANGE || distance > INTERCEPT_MAX_RANGE)
        return;

    if (!CanUseAbility(INTERCEPT))
        return;

    // Intercept requires Berserker Stance
    WarriorStance currentStance = GetCurrentStance();
    if (currentStance != WarriorStance::BERSERKER)
    {
        // Switch to Berserker Stance first
        GetBot()->CastSpell(GetBot(), BERSERKER_STANCE, false);
        _lastStanceChange = getMSTime();
        return; // Will intercept on next update
    }

    GetBot()->CastSpell(target, INTERCEPT, false);
    RecordAbilityUsage(INTERCEPT);
    _successfulCharges++;

    TC_LOG_DEBUG("playerbot.warrior", "Intercept executed against {} at distance {:.1f}",
                 target->GetName(), distance);
}

void WarriorAI::HandleGroupCombatRole()
{
    Group* group = GetBot()->GetGroup();
    if (!group)
        return;

    // Role-specific group behaviors
    switch (_currentSpec)
    {
        case WarriorSpec::PROTECTION:
            // Tank role - manage threat and positioning
            ManageThreatInGroup();
            OptimizeFormationPosition();
            break;

        case WarriorSpec::ARMS:
        case WarriorSpec::FURY:
            // DPS role - focus damage and assist tank
            CoordinateWithGroup();
            break;
    }
}

void WarriorAI::ManageThreatInGroup()
{
    if (_currentSpec != WarriorSpec::PROTECTION)
        return;

    // Get all enemies targeting group members
    std::vector<::Unit*> threats;
    Group* group = GetBot()->GetGroup();

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive())
        {
            // Check if member is being attacked
            if (Unit* attacker = member->GetAttacker())
            {
                if (attacker->GetTarget() != GetBot())
                {
                    threats.push_back(attacker);
                }
            }
        }
    }

    // Use threat-generating abilities on loose enemies
    for (::Unit* threat : threats)
    {
        if (GetBot()->GetDistance(threat) <= 30.0f)
        {
            // Taunt if available
            if (CanUseAbility(TAUNT) && GetBot()->GetDistance(threat) <= 30.0f)
            {
                GetBot()->CastSpell(threat, TAUNT, false);
                RecordAbilityUsage(TAUNT);
                _threatGenerated += 1000;

                TC_LOG_DEBUG("playerbot.warrior", "Taunted {} to protect group member", threat->GetName());
                break; // One taunt per update
            }
        }
    }
}

void WarriorAI::OptimizeFormationPosition()
{
    if (!_formationManager)
        return;

    Group* group = GetBot()->GetGroup();
    if (!group)
        return;

    // Get group members for formation
    std::vector<Player*> groupMembers;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive())
        {
            groupMembers.push_back(member);
        }
    }

    if (groupMembers.size() >= 2)
    {
        // Tank should be at front of formation
        FormationManager::FormationType formation = (_currentSpec == WarriorSpec::PROTECTION) ?
            FormationManager::FormationType::DUNGEON : FormationManager::FormationType::LINE;

        _formationManager->JoinFormation(groupMembers, formation);
    }
}

void WarriorAI::CoordinateWithGroup()
{
    Group* group = GetBot()->GetGroup();
    if (!group)
        return;

    // Focus fire on tank's target if tank exists
    Player* tank = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive())
        {
            // Simple heuristic: player in defensive stance or with shield is likely tank
            if (member->HasAura(DEFENSIVE_STANCE) || member->GetShield())
            {
                tank = member;
                break;
            }
        }
    }

    if (tank && tank != GetBot())
    {
        Unit* tankTarget = tank->GetTarget();
        if (tankTarget && tankTarget->IsAlive())
        {
            // Switch to tank's target if different
            Unit* currentTarget = GetBot()->GetTarget();
            if (currentTarget != tankTarget)
            {
                GetBot()->SetTarget(tankTarget->GetGUID());
                TC_LOG_DEBUG("playerbot.warrior", "Switching to tank's target for coordination");
            }
        }
    }
}

std::vector<::Unit*> WarriorAI::GetNearbyCasters(float range)
{
    std::vector<::Unit*> casters;
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(range);

    for (::Unit* enemy : nearbyEnemies)
    {
        if (enemy && enemy->IsAlive())
        {
            // Check if unit is casting or is a known caster type
            if (enemy->HasUnitState(UNIT_STATE_CASTING) ||
                enemy->GetPowerType() == POWER_MANA ||
                enemy->GetCreatureType() == CREATURE_TYPE_HUMANOID)
            {
                casters.push_back(enemy);
            }
        }
    }

    return casters;
}

bool WarriorAI::HasShieldEquipped()
{
    Item* offhandItem = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    return offhandItem && offhandItem->GetTemplate()->GetInventoryType() == INVTYPE_SHIELD;
}

WarriorStance WarriorAI::GetCurrentStance()
{
    if (GetBot()->HasAura(BATTLE_STANCE))
        return WarriorStance::BATTLE;
    else if (GetBot()->HasAura(DEFENSIVE_STANCE))
        return WarriorStance::DEFENSIVE;
    else if (GetBot()->HasAura(BERSERKER_STANCE))
        return WarriorStance::BERSERKER;
    else
        return WarriorStance::NONE;
}

void WarriorAI::RecordStanceChange(WarriorStance fromStance, WarriorStance toStance)
{
    TC_LOG_DEBUG("playerbot.warrior", "Stance change recorded: {} -> {}",
                 static_cast<uint32>(fromStance), static_cast<uint32>(toStance));

    // Update stance optimization score
    float currentScore = _warriorMetrics.stanceOptimizationScore.load();
    float improvement = 0.1f; // Assume stance changes are generally beneficial
    _warriorMetrics.stanceOptimizationScore.store(currentScore + improvement);
}

void WarriorAI::RecordChargeSuccess(::Unit* target, bool success)
{
    if (success)
    {
        _successfulCharges++;
        TC_LOG_DEBUG("playerbot.warrior", "Successful charge recorded against {}",
                     target ? target->GetName() : "Unknown");
    }
    else
    {
        TC_LOG_DEBUG("playerbot.warrior", "Failed charge attempt against {}",
                     target ? target->GetName() : "Unknown");
    }
}

void WarriorAI::RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success)
{
    if (success)
    {
        _successfulInterrupts++;
        TC_LOG_DEBUG("playerbot.warrior", "Successful interrupt of spell {} on {}",
                     spellId, target ? target->GetName() : "Unknown");
    }
    else
    {
        TC_LOG_DEBUG("playerbot.warrior", "Failed interrupt attempt of spell {} on {}",
                     spellId, target ? target->GetName() : "Unknown");
    }
}

void WarriorAI::UpdateMetrics(uint32 diff)
{
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - _warriorMetrics.lastMetricsUpdate);

    if (timeSinceLastUpdate.count() >= 5) // Update every 5 seconds
    {
        // Calculate survivability score
        float healthPct = GetBot()->GetHealthPct();
        float survivabilityScore = healthPct / 100.0f;

        // Adjust based on defensive ability usage
        if (GetBot()->HasAura(SHIELD_WALL) || GetBot()->HasAura(LAST_STAND))
            survivabilityScore += 0.1f;

        _warriorMetrics.survivabilityScore.store(survivabilityScore);

        // Update efficiency metrics
        float rageEfficiency = CalculateRageEfficiency();
        _warriorMetrics.averageRageEfficiency.store(rageEfficiency);

        _warriorMetrics.lastMetricsUpdate = currentTime;

        TC_LOG_DEBUG("playerbot.warrior", "Metrics updated - Efficiency: {:.2f}, Survivability: {:.2f}",
                     rageEfficiency, survivabilityScore);
    }
}

void WarriorAI::OptimizeAbilityPriorities()
{
    // Dynamically adjust ability priorities based on current situation
    ::Unit* target = GetBot()->GetTarget();
    if (!target)
        return;

    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(10.0f);

    // Multi-target scenario
    if (nearbyEnemies.size() >= 3)
    {
        // Prioritize AoE abilities
        SetAbilityPriority(WHIRLWIND, 15);
        SetAbilityPriority(THUNDER_CLAP, 14);
        SetAbilityPriority(CLEAVE, 13);
    }
    // Single target
    else
    {
        // Prioritize single-target abilities
        switch (_currentSpec)
        {
            case WarriorSpec::ARMS:
                SetAbilityPriority(MORTAL_STRIKE, 15);
                SetAbilityPriority(COLOSSUS_SMASH, 14);
                SetAbilityPriority(OVERPOWER, 13);
                break;
            case WarriorSpec::FURY:
                SetAbilityPriority(BLOODTHIRST, 15);
                SetAbilityPriority(RAGING_BLOW, 14);
                SetAbilityPriority(RAMPAGE, 13);
                break;
            case WarriorSpec::PROTECTION:
                SetAbilityPriority(SHIELD_SLAM, 15);
                SetAbilityPriority(REVENGE, 14);
                SetAbilityPriority(DEVASTATE, 13);
                break;
        }
    }

    // Adjust for health levels
    float healthPct = GetBot()->GetHealthPct();
    if (healthPct < 40.0f)
    {
        // Prioritize defensive abilities
        SetAbilityPriority(SHIELD_WALL, 20);
        SetAbilityPriority(LAST_STAND, 19);
        SetAbilityPriority(ENRAGED_REGENERATION, 18);
    }
}

void WarriorAI::SetAbilityPriority(uint32 spellId, uint32 priority)
{
    // Implementation would depend on the action priority system
    // This is a placeholder for the priority setting mechanism
    TC_LOG_DEBUG("playerbot.warrior", "Set ability {} priority to {}", spellId, priority);
}

// Additional helper methods

uint32 WarriorAI::GetRage()
{
    return GetBot()->GetPower(POWER_RAGE);
}

uint32 WarriorAI::GetMaxRage()
{
    return GetBot()->GetMaxPower(POWER_RAGE);
}

bool WarriorAI::HasEnoughRage(uint32 amount)
{
    return GetRage() >= amount;
}

bool WarriorAI::IsInMeleeRange(::Unit* target)
{
    if (!target)
        return false;

    return GetBot()->GetDistance(target) <= OPTIMAL_MELEE_RANGE;
}

std::vector<::Unit*> WarriorAI::GetNearbyEnemies(float range)
{
    std::vector<::Unit*> enemies;

    // This would be implemented using TrinityCore's creature/unit search mechanisms
    // Simplified implementation for demonstration

    return enemies;
}

} // namespace Playerbot