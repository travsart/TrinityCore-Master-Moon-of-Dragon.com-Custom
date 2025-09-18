/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarriorAI.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Log.h"

namespace Playerbot
{

WarriorAI::WarriorAI(Player* bot) : ClassAI(bot), _currentStance(WarriorStance::NONE),
    _lastStanceChange(0), _rageSpent(0), _damageDealt(0), _lastBattleShout(0),
    _lastCommandingShout(0), _needsIntercept(false), _needsCharge(false),
    _lastChargeTarget(nullptr), _lastChargeTime(0)
{
    _specialization = DetectSpecialization();
    TC_LOG_DEBUG("playerbot.warrior", "WarriorAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_specialization));
}

void WarriorAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Update stance based on situation
    UpdateStance();

    // Use charge abilities if needed
    UseChargeAbilities(target);

    // Execute rotation based on specialization
    switch (_specialization)
    {
        case WarriorSpec::ARMS:
            UpdateArmsRotation(target);
            break;
        case WarriorSpec::FURY:
            UpdateFuryRotation(target);
            break;
        case WarriorSpec::PROTECTION:
            UpdateProtectionRotation(target);
            break;
    }

    // Use utility abilities
    UseUtilityAbilities(target);

    // Multi-target abilities if multiple enemies
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies();
    if (nearbyEnemies.size() > 1)
    {
        UseAOEAbilities(nearbyEnemies);
    }
}

void WarriorAI::UpdateBuffs()
{
    UpdateWarriorBuffs();
}

void WarriorAI::UpdateCooldowns(uint32 diff)
{
    // Use defensive cooldowns if health is low
    if (GetBot()->GetHealthPct() < 30.0f)
    {
        UseDefensiveCooldowns();
    }

    // Use offensive cooldowns in good situations
    if (_inCombat && _currentTarget && GetBot()->GetHealthPct() > 50.0f)
    {
        UseOffensiveCooldowns();
    }

    // Protection warriors manage threat
    if (_specialization == WarriorSpec::PROTECTION)
    {
        ManageThreat();
    }
}

bool WarriorAI::CanUseAbility(uint32 spellId)
{
    if (!IsSpellReady(spellId) || !IsSpellUsable(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check stance requirements
    WarriorStance requiredStance = WarriorStance::NONE;

    // Determine required stance for spell
    switch (spellId)
    {
        case CHARGE:
        case OVERPOWER:
        case MORTAL_STRIKE:
            requiredStance = WarriorStance::BATTLE;
            break;
        case SHIELD_SLAM:
        case REVENGE:
        case SHIELD_WALL:
        case TAUNT:
            requiredStance = WarriorStance::DEFENSIVE;
            break;
        case INTERCEPT:
        case WHIRLWIND:
        case EXECUTE:
        case RECKLESSNESS:
            requiredStance = WarriorStance::BERSERKER;
            break;
    }

    if (requiredStance != WarriorStance::NONE && !IsInStance(requiredStance))
    {
        return false;
    }

    return true;
}

void WarriorAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Reset combat variables
    _rageSpent = 0;
    _damageDealt = 0;
    _needsCharge = CanCharge(target);
    _needsIntercept = false;

    TC_LOG_DEBUG("playerbot.warrior", "Warrior {} entering combat - Spec: {}, Stance: {}",
                 GetBot()->GetName(), static_cast<uint32>(_specialization), static_cast<uint32>(_currentStance));
}

void WarriorAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Analyze combat effectiveness
    AnalyzeCombatEffectiveness();

    // Reset combat state
    _needsCharge = false;
    _needsIntercept = false;
    _lastChargeTarget = nullptr;
}

bool WarriorAI::HasEnoughResource(uint32 spellId)
{
    return _resourceManager->HasEnoughResource(spellId);
}

void WarriorAI::ConsumeResource(uint32 spellId)
{
    uint32 rageCost = 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (spellInfo && spellInfo->PowerType == POWER_RAGE)
    {
        rageCost = spellInfo->ManaCost;
    }

    _resourceManager->ConsumeResource(spellId);
    _rageSpent += rageCost;

    TC_LOG_DEBUG("playerbot.warrior", "Consumed {} rage for spell {}", rageCost, spellId);
}

Position WarriorAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return GetBot()->GetPosition();

    // Warriors want to be in melee range
    float distance = GetOptimalRange(target);
    float angle = GetBot()->GetAngle(target);

    // Position slightly behind target if possible (for backstab protection)
    angle += M_PI + (frand(-0.5f, 0.5f));

    Position pos;
    target->GetNearPosition(pos, distance, angle);
    return pos;
}

float WarriorAI::GetOptimalRange(::Unit* target)
{
    // Warriors are melee fighters
    return OPTIMAL_MELEE_RANGE;
}

void WarriorAI::UpdateArmsRotation(::Unit* target)
{
    if (!target || !IsInMeleeRange(target))
        return;

    // Arms priority rotation
    // 1. Colossus Smash if not up
    if (!target->HasAura(COLOSSUS_SMASH) && CanUseAbility(COLOSSUS_SMASH))
    {
        _actionQueue->AddAction(COLOSSUS_SMASH, ActionPriority::BURST, 100.0f, target);
    }

    // 2. Mortal Strike as primary ability
    if (CanUseAbility(MORTAL_STRIKE))
    {
        float score = 80.0f;
        if (!target->HasAura(MORTAL_STRIKE))
            score += 20.0f; // Higher priority if debuff not up

        _actionQueue->AddAction(MORTAL_STRIKE, ActionPriority::ROTATION, score, target);
    }

    // 3. Execute if target is low health
    if (target->GetHealthPct() < 20.0f && CanUseAbility(EXECUTE))
    {
        _actionQueue->AddAction(EXECUTE, ActionPriority::BURST, 90.0f, target);
    }

    // 4. Overpower if available
    if (CanUseAbility(OVERPOWER))
    {
        _actionQueue->AddAction(OVERPOWER, ActionPriority::ROTATION, 70.0f, target);
    }

    // 5. Rend if not up and target will live long enough
    if (!target->HasAura(REND) && target->GetHealthPct() > 30.0f && CanUseAbility(REND))
    {
        _actionQueue->AddAction(REND, ActionPriority::ROTATION, 50.0f, target);
    }

    // 6. Heroic Strike as rage dump
    if (GetRagePercent() > 0.8f && CanUseAbility(HEROIC_STRIKE))
    {
        _actionQueue->AddAction(HEROIC_STRIKE, ActionPriority::ROTATION, 30.0f, target);
    }
}

void WarriorAI::UpdateFuryRotation(::Unit* target)
{
    if (!target || !IsInMeleeRange(target))
        return;

    // Fury priority rotation
    // 1. Bloodthirst for healing and rage generation
    if (CanUseAbility(BLOODTHIRST))
    {
        float score = 85.0f;
        if (GetBot()->GetHealthPct() < 50.0f)
            score += 15.0f; // Higher priority when hurt

        _actionQueue->AddAction(BLOODTHIRST, ActionPriority::ROTATION, score, target);
    }

    // 2. Raging Blow if available
    if (CanUseAbility(RAGING_BLOW))
    {
        _actionQueue->AddAction(RAGING_BLOW, ActionPriority::ROTATION, 80.0f, target);
    }

    // 3. Execute in execute range
    if (target->GetHealthPct() < 20.0f && CanUseAbility(EXECUTE))
    {
        _actionQueue->AddAction(EXECUTE, ActionPriority::BURST, 95.0f, target);
    }

    // 4. Whirlwind for multiple targets or rage dump
    uint32 enemyCount = GetEnemyCount();
    if (enemyCount > 1 && CanUseAbility(WHIRLWIND))
    {
        float score = 70.0f + (enemyCount * 10.0f);
        _actionQueue->AddAction(WHIRLWIND, ActionPriority::ROTATION, score, target);
    }
    else if (GetRagePercent() > 0.7f && CanUseAbility(WHIRLWIND))
    {
        _actionQueue->AddAction(WHIRLWIND, ActionPriority::ROTATION, 40.0f, target);
    }

    // 5. Heroic Strike as rage dump
    if (GetRagePercent() > 0.8f && CanUseAbility(HEROIC_STRIKE))
    {
        _actionQueue->AddAction(HEROIC_STRIKE, ActionPriority::ROTATION, 35.0f, target);
    }
}

void WarriorAI::UpdateProtectionRotation(::Unit* target)
{
    if (!target)
        return;

    // Protection priority rotation (threat-focused)
    // 1. Shield Slam as primary threat generator
    if (IsInMeleeRange(target) && CanUseAbility(SHIELD_SLAM))
    {
        _actionQueue->AddAction(SHIELD_SLAM, ActionPriority::ROTATION, 90.0f, target);
    }

    // 2. Revenge if available (usually after blocking/parrying)
    if (CanUseAbility(REVENGE))
    {
        _actionQueue->AddAction(REVENGE, ActionPriority::ROTATION, 85.0f, target);
    }

    // 3. Devastate to maintain Sunder Armor stacks
    if (CanUseAbility(DEVASTATE))
    {
        float score = 70.0f;
        uint32 sunderStacks = target->GetAuraCount(SUNDER_ARMOR);
        if (sunderStacks < 5)
            score += (5 - sunderStacks) * 10.0f;

        _actionQueue->AddAction(DEVASTATE, ActionPriority::ROTATION, score, target);
    }

    // 4. Thunder Clap for AoE threat
    uint32 enemyCount = GetEnemyCount();
    if (enemyCount > 1 && CanUseAbility(THUNDER_CLAP))
    {
        float score = 60.0f + (enemyCount * 15.0f);
        _actionQueue->AddAction(THUNDER_CLAP, ActionPriority::ROTATION, score);
    }

    // 5. Shield Block for damage reduction
    if (GetBot()->GetHealthPct() < 70.0f && CanUseAbility(SHIELD_BLOCK))
    {
        _actionQueue->AddAction(SHIELD_BLOCK, ActionPriority::SURVIVAL, 80.0f);
    }
}

void WarriorAI::UpdateStance()
{
    WarriorStance optimalStance = GetOptimalStanceForSituation();

    if (optimalStance != _currentStance && ShouldSwitchStance(optimalStance))
    {
        SwitchStance(optimalStance);
    }
}

bool WarriorAI::ShouldSwitchStance(WarriorStance newStance)
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastStanceChange < STANCE_CHANGE_COOLDOWN)
        return false;

    // Don't change stance if we don't have enough rage
    if (GetRage() < 10)
        return false;

    return true;
}

void WarriorAI::SwitchStance(WarriorStance stance)
{
    uint32 stanceSpell = GetStanceSpellId(stance);
    if (stanceSpell && CanUseAbility(stanceSpell))
    {
        if (CastSpell(stanceSpell))
        {
            _currentStance = stance;
            _lastStanceChange = getMSTime();

            TC_LOG_DEBUG("playerbot.warrior", "Switched to stance {}", static_cast<uint32>(stance));
        }
    }
}

WarriorStance WarriorAI::GetOptimalStanceForSituation()
{
    // Protection warriors stay in defensive stance
    if (_specialization == WarriorSpec::PROTECTION)
        return WarriorStance::DEFENSIVE;

    if (!_inCombat)
        return WarriorStance::BATTLE;

    // In combat, choose based on situation
    if (_currentTarget)
    {
        float distance = GetBot()->GetDistance(_currentTarget);

        // Need to charge - use battle stance
        if (distance > CHARGE_MIN_RANGE && CanCharge(_currentTarget))
            return WarriorStance::BATTLE;

        // Need to intercept - use berserker stance
        if (distance > INTERCEPT_MIN_RANGE && CanIntercept(_currentTarget))
            return WarriorStance::BERSERKER;

        // Low health - defensive stance
        if (GetBot()->GetHealthPct() < 30.0f)
            return WarriorStance::DEFENSIVE;

        // Multiple enemies - berserker for whirlwind
        if (GetEnemyCount() > 2)
            return WarriorStance::BERSERKER;

        // Execute range - berserker stance
        if (_currentTarget->GetHealthPct() < 20.0f)
            return WarriorStance::BERSERKER;
    }

    // Default to battle stance for most situations
    return WarriorStance::BATTLE;
}

void WarriorAI::UpdateWarriorBuffs()
{
    uint32 currentTime = getMSTime();

    // Battle Shout
    if (currentTime - _lastBattleShout > BATTLE_SHOUT_DURATION)
    {
        CastBattleShout();
    }

    // Commanding Shout for health bonus
    if (currentTime - _lastCommandingShout > COMMANDING_SHOUT_DURATION &&
        GetBot()->GetHealthPct() < 80.0f)
    {
        CastCommandingShout();
    }

    // Weapon buffs if applicable
    UpdateWeaponBuffs();
}

void WarriorAI::CastBattleShout()
{
    if (CanUseAbility(BATTLE_SHOUT))
    {
        if (CastSpell(BATTLE_SHOUT))
        {
            _lastBattleShout = getMSTime();
            TC_LOG_DEBUG("playerbot.warrior", "Cast Battle Shout");
        }
    }
}

void WarriorAI::CastCommandingShout()
{
    if (CanUseAbility(COMMANDING_SHOUT))
    {
        if (CastSpell(COMMANDING_SHOUT))
        {
            _lastCommandingShout = getMSTime();
            TC_LOG_DEBUG("playerbot.warrior", "Cast Commanding Shout");
        }
    }
}

void WarriorAI::UpdateWeaponBuffs()
{
    // Placeholder for weapon enchantments/buffs if applicable
}

bool WarriorAI::HasEnoughRage(uint32 amount)
{
    return GetRage() >= amount;
}

uint32 WarriorAI::GetRage()
{
    return _resourceManager->GetResource(ResourceType::RAGE);
}

uint32 WarriorAI::GetMaxRage()
{
    return _resourceManager->GetMaxResource(ResourceType::RAGE);
}

float WarriorAI::GetRagePercent()
{
    return _resourceManager->GetResourcePercent(ResourceType::RAGE);
}

void WarriorAI::OptimizeRageUsage()
{
    // Ensure we're not rage-starved by managing abilities efficiently
    if (GetRagePercent() < 0.2f)
    {
        // Prioritize rage-generating abilities
        return;
    }

    if (GetRagePercent() > 0.8f)
    {
        // Use rage-dump abilities
        if (_currentTarget && CanUseAbility(HEROIC_STRIKE))
        {
            _actionQueue->AddAction(HEROIC_STRIKE, ActionPriority::ROTATION, 25.0f, _currentTarget);
        }
    }
}

void WarriorAI::UseChargeAbilities(::Unit* target)
{
    if (!target)
        return;

    float distance = GetBot()->GetDistance(target);

    // Use Charge from Battle Stance
    if (CanCharge(target) && _needsCharge)
    {
        UseCharge(target);
    }
    // Use Intercept from Berserker Stance
    else if (CanIntercept(target) && _needsIntercept)
    {
        UseIntercept(target);
    }
    // Use Heroic Leap if available and at range
    else if (distance > 10.0f && CanUseAbility(HEROIC_LEAP))
    {
        UseHeroicLeap(target);
    }
}

void WarriorAI::UseCharge(::Unit* target)
{
    if (!target || !CanCharge(target))
        return;

    if (CanUseAbility(CHARGE))
    {
        _actionQueue->AddAction(CHARGE, ActionPriority::MOVEMENT, 100.0f, target);
        _needsCharge = false;
        _lastChargeTarget = target;
        _lastChargeTime = getMSTime();

        TC_LOG_DEBUG("playerbot.warrior", "Queued Charge on {}", target->GetName());
    }
}

void WarriorAI::UseIntercept(::Unit* target)
{
    if (!target || !CanIntercept(target))
        return;

    if (CanUseAbility(INTERCEPT))
    {
        _actionQueue->AddAction(INTERCEPT, ActionPriority::MOVEMENT, 100.0f, target);
        _needsIntercept = false;

        TC_LOG_DEBUG("playerbot.warrior", "Queued Intercept on {}", target->GetName());
    }
}

void WarriorAI::UseHeroicLeap(::Unit* target)
{
    if (!target)
        return;

    // Get position near target
    Position leapPos = GetOptimalPosition(target);

    if (CanUseAbility(HEROIC_LEAP))
    {
        _actionQueue->AddAction(HEROIC_LEAP, ActionPriority::MOVEMENT, 80.0f, target);
        TC_LOG_DEBUG("playerbot.warrior", "Queued Heroic Leap");
    }
}

void WarriorAI::UseDefensiveCooldowns()
{
    float healthPct = GetBot()->GetHealthPct();

    // Shield Wall for emergencies
    if (healthPct < 20.0f && CanUseAbility(SHIELD_WALL))
    {
        _actionQueue->AddAction(SHIELD_WALL, ActionPriority::EMERGENCY, 100.0f);
    }

    // Last Stand for health boost
    if (healthPct < 30.0f && CanUseAbility(LAST_STAND))
    {
        _actionQueue->AddAction(LAST_STAND, ActionPriority::EMERGENCY, 90.0f);
    }

    // Spell Reflection against casters
    if (_currentTarget && _currentTarget->GetPowerType() == POWER_MANA &&
        CanUseAbility(SPELL_REFLECTION))
    {
        _actionQueue->AddAction(SPELL_REFLECTION, ActionPriority::SURVIVAL, 80.0f);
    }
}

void WarriorAI::UseOffensiveCooldowns()
{
    if (!_currentTarget)
        return;

    // Recklessness for damage boost
    if (CanUseAbility(RECKLESSNESS))
    {
        float score = 70.0f;
        if (_currentTarget->GetHealthPct() > 50.0f) // Worth using on healthy targets
            score += 20.0f;

        _actionQueue->AddAction(RECKLESSNESS, ActionPriority::BURST, score);
    }

    // Avatar for overall combat enhancement
    if (CanUseAbility(AVATAR))
    {
        _actionQueue->AddAction(AVATAR, ActionPriority::BURST, 65.0f);
    }

    // Bladestorm for AoE or when surrounded
    uint32 enemyCount = GetEnemyCount();
    if (enemyCount > 1 && CanUseAbility(BLADESTORM))
    {
        float score = 60.0f + (enemyCount * 10.0f);
        _actionQueue->AddAction(BLADESTORM, ActionPriority::BURST, score);
    }
}

void WarriorAI::UseUtilityAbilities(::Unit* target)
{
    if (!target)
        return;

    // Pummel to interrupt casting
    if (target->IsNonMeleeSpellCast(false) && CanUseAbility(PUMMEL))
    {
        uint32 spellId = target->GetCurrentSpell(CURRENT_GENERIC_SPELL) ?
                        target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id : 0;

        _actionQueue->AddAction(PUMMEL, ActionPriority::INTERRUPT, 100.0f, target);
    }

    // Disarm weapon users
    if (target->GetTypeId() == TYPEID_PLAYER && CanUseAbility(DISARM))
    {
        _actionQueue->AddAction(DISARM, ActionPriority::SURVIVAL, 70.0f, target);
    }
}

void WarriorAI::ManageThreat()
{
    if (_specialization != WarriorSpec::PROTECTION)
        return;

    // Use Taunt on targets not attacking us
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);
    for (::Unit* enemy : enemies)
    {
        if (enemy->GetVictim() && enemy->GetVictim() != GetBot() && CanUseAbility(TAUNT))
        {
            _actionQueue->AddAction(TAUNT, ActionPriority::SURVIVAL, 90.0f, enemy);
            break; // Only taunt one target at a time
        }
    }

    // Use Thunder Clap for AoE threat
    if (GetEnemyCount() > 1 && CanUseAbility(THUNDER_CLAP))
    {
        _actionQueue->AddAction(THUNDER_CLAP, ActionPriority::ROTATION, 80.0f);
    }
}

void WarriorAI::UseAOEAbilities(const std::vector<::Unit*>& enemies)
{
    uint32 enemyCount = static_cast<uint32>(enemies.size());

    // Whirlwind for multiple enemies
    if (enemyCount > 1 && CanUseAbility(WHIRLWIND))
    {
        float score = 60.0f + (enemyCount * 10.0f);
        _actionQueue->AddAction(WHIRLWIND, ActionPriority::ROTATION, score);
    }

    // Cleave when appropriate
    if (enemyCount > 1 && CanUseAbility(CLEAVE))
    {
        float score = 50.0f + (enemyCount * 5.0f);
        _actionQueue->AddAction(CLEAVE, ActionPriority::ROTATION, score, enemies[0]);
    }

    // Thunder Clap for Protection warriors
    if (_specialization == WarriorSpec::PROTECTION && enemyCount > 1 && CanUseAbility(THUNDER_CLAP))
    {
        float score = 70.0f + (enemyCount * 15.0f);
        _actionQueue->AddAction(THUNDER_CLAP, ActionPriority::ROTATION, score);
    }
}

bool WarriorAI::IsInMeleeRange(::Unit* target) const
{
    if (!target)
        return false;

    return GetBot()->GetDistance(target) <= OPTIMAL_MELEE_RANGE;
}

bool WarriorAI::CanCharge(::Unit* target) const
{
    if (!target)
        return false;

    float distance = GetBot()->GetDistance(target);
    return distance >= CHARGE_MIN_RANGE && distance <= CHARGE_MAX_RANGE &&
           IsInStance(WarriorStance::BATTLE) && HasLineOfSight(target);
}

bool WarriorAI::CanIntercept(::Unit* target) const
{
    if (!target)
        return false;

    float distance = GetBot()->GetDistance(target);
    return distance >= INTERCEPT_MIN_RANGE && distance <= INTERCEPT_MAX_RANGE &&
           IsInStance(WarriorStance::BERSERKER) && HasLineOfSight(target);
}

std::vector<::Unit*> WarriorAI::GetNearbyEnemies(float range)
{
    std::vector<::Unit*> enemies;

    if (!GetBot() || !GetBot()->GetMap())
        return enemies;

    Map* map = GetBot()->GetMap();
    Map::PlayerList const& players = map->GetPlayers();

    for (Map::PlayerList::const_iterator iter = players.begin(); iter != players.end(); ++iter)
    {
        Player* player = iter->GetSource();
        if (!player || player == GetBot() || !player->IsInWorld())
            continue;

        if (GetBot()->IsValidAttackTarget(player))
        {
            float distance = GetBot()->GetDistance(player);
            if (distance <= range)
            {
                enemies.push_back(player);
            }
        }
    }

    return enemies;
}

uint32 WarriorAI::GetEnemyCount(float range)
{
    return static_cast<uint32>(GetNearbyEnemies(range).size());
}

WarriorSpec WarriorAI::DetectSpecialization()
{
    // This would detect the warrior's specialization based on talents
    // For now, return a default based on some heuristics

    if (!GetBot())
        return WarriorSpec::ARMS;

    // Simple detection based on equipped items or stats
    // This could be enhanced with actual talent detection
    return WarriorSpec::ARMS; // Default
}

bool WarriorAI::IsInStance(WarriorStance stance)
{
    return _currentStance == stance;
}

uint32 WarriorAI::GetStanceSpellId(WarriorStance stance)
{
    switch (stance)
    {
        case WarriorStance::BATTLE: return BATTLE_STANCE;
        case WarriorStance::DEFENSIVE: return DEFENSIVE_STANCE;
        case WarriorStance::BERSERKER: return BERSERKER_STANCE;
        default: return 0;
    }
}

bool WarriorAI::IsValidTarget(::Unit* target)
{
    if (!target || !GetBot())
        return false;

    return target->IsAlive() && GetBot()->IsValidAttackTarget(target);
}

void WarriorAI::RecordAbilityUsage(uint32 spellId)
{
    _abilityUsage[spellId]++;
}

void WarriorAI::AnalyzeCombatEffectiveness()
{
    // Analyze rage efficiency, damage dealt, etc.
    if (_rageSpent > 0)
    {
        float efficiency = static_cast<float>(_damageDealt) / _rageSpent;
        TC_LOG_DEBUG("playerbot.warrior", "Combat efficiency: {} damage per rage", efficiency);
    }

    // Record performance metrics
    RecordPerformanceMetric("rage_spent", _rageSpent);
    RecordPerformanceMetric("damage_dealt", _damageDealt);
}

} // namespace Playerbot