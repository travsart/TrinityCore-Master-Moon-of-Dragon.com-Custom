/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EvokerSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"
#include "Group.h"

namespace Playerbot
{

EvokerSpecialization::EvokerSpecialization(Player* bot)
    : _bot(bot), _combatPhase(CombatPhase::OPENER), _currentTarget(nullptr),
      _lastUpdateTime(0), _combatStartTime(0), _lastEssenceCheck(0), _lastAspectCheck(0), _lastEmpowermentCheck(0),
      _totalDamageDealt(0), _totalHealingDone(0), _totalEssenceSpent(0), _totalEmpoweredSpells(0),
      _burstPhaseCount(0), _averageCombatTime(0.0f)
{
    InitializeCooldowns();
    UpdateResourceStates();
}

void EvokerSpecialization::InitializeCooldowns()
{
    // Core abilities cooldowns
    _cooldowns[DRAGONRAGE] = CooldownInfo(DRAGONRAGE, 120000);        // 2 minutes
    _cooldowns[DEEP_BREATH] = CooldownInfo(DEEP_BREATH, 120000);      // 2 minutes
    _cooldowns[SHATTERING_STAR] = CooldownInfo(SHATTERING_STAR, 20000); // 20 seconds
    _cooldowns[FIRESTORM] = CooldownInfo(FIRESTORM, 20000);           // 20 seconds
    _cooldowns[OBSIDIAN_SCALES] = CooldownInfo(OBSIDIAN_SCALES, 90000); // 1.5 minutes
    _cooldowns[RENEWING_BLAZE] = CooldownInfo(RENEWING_BLAZE, 60000); // 1 minute
    _cooldowns[TIME_SPIRAL] = CooldownInfo(TIME_SPIRAL, 120000);      // 2 minutes
    _cooldowns[RESCUE] = CooldownInfo(RESCUE, 60000);                 // 1 minute
    _cooldowns[WING_BUFFET] = CooldownInfo(WING_BUFFET, 90000);       // 1.5 minutes
    _cooldowns[TAIL_SWIPE] = CooldownInfo(TAIL_SWIPE, 90000);         // 1.5 minutes
    _cooldowns[SLEEP_WALK] = CooldownInfo(SLEEP_WALK, 15000);         // 15 seconds
    _cooldowns[QUELL] = CooldownInfo(QUELL, 40000);                   // 40 seconds

    // Preservation abilities
    _cooldowns[DREAM_FLIGHT] = CooldownInfo(DREAM_FLIGHT, 120000);    // 2 minutes
    _cooldowns[TEMPORAL_ANOMALY] = CooldownInfo(TEMPORAL_ANOMALY, 60000); // 1 minute
    _cooldowns[STASIS] = CooldownInfo(STASIS, 90000);                 // 1.5 minutes

    // Augmentation abilities
    _cooldowns[SPATIAL_PARADOX] = CooldownInfo(SPATIAL_PARADOX, 120000); // 2 minutes
    _cooldowns[TREMBLING_EARTH] = CooldownInfo(TREMBLING_EARTH, 60000);  // 1 minute

    TC_LOG_DEBUG("playerbot", "EvokerSpecialization: Initialized {} cooldowns for bot {}", _cooldowns.size(), _bot->GetName());
}

void EvokerSpecialization::UpdateResourceStates()
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();

    // Update Essence State
    _essence.current = _bot->GetPower(POWER_ESSENCE);
    _essence.maximum = _bot->GetMaxPower(POWER_ESSENCE);

    // Update essence state enum
    _essence.UpdateState();

    // Handle essence regeneration
    if (_essence.isRegenerating && _essence.current < _essence.maximum)
    {
        if (currentTime - _essence.lastGenerated >= _essence.generationRate)
        {
            _essence.GenerateEssence(1);
            _essence.lastGenerated = currentTime;
        }
    }

    // Update Aspect State
    if (_aspect.canShift && currentTime - _aspect.lastShift >= _aspect.cooldown)
    {
        _aspect.canShift = true;
    }

    // Update Empowered Spell State
    if (_currentEmpoweredSpell.isChanneling)
    {
        uint32 channelTime = _currentEmpoweredSpell.GetChannelTime();
        if (channelTime >= _currentEmpoweredSpell.GetRequiredChannelTime())
        {
            _currentEmpoweredSpell.currentLevel = _currentEmpoweredSpell.targetLevel;
        }
        else
        {
            // Calculate current level based on channel time
            uint32 levelTime = 1000; // 1 second per level
            _currentEmpoweredSpell.currentLevel = static_cast<EmpowermentLevel>(std::min(4u, channelTime / levelTime));
        }
    }

    _lastUpdateTime = currentTime;
}

void EvokerSpecialization::UpdateTargetInfo(::Unit* target)
{
    if (!target)
    {
        _currentTarget = nullptr;
        return;
    }

    _currentTarget = target;
}

void EvokerSpecialization::LogRotationDecision(const std::string& decision, const std::string& reason)
{
    TC_LOG_DEBUG("playerbot", "EvokerAI [{}]: {} - Reason: {} [Essence: {}/{}, Phase: {}]",
                _bot->GetName(), decision, reason, _essence.current, _essence.maximum,
                static_cast<uint8>(_combatPhase));
}

bool EvokerSpecialization::IsInRange(::Unit* target, float range)
{
    if (!target || !_bot)
        return false;

    float distance = _bot->GetDistance(target);
    return distance <= range;
}

bool EvokerSpecialization::IsInMeleeRange(::Unit* target)
{
    return IsInRange(target, MELEE_RANGE);
}

bool EvokerSpecialization::HasAura(uint32 spellId, ::Unit* unit)
{
    if (!unit)
        unit = _bot;

    if (!unit)
        return false;

    return unit->HasAura(spellId);
}

uint32 EvokerSpecialization::GetAuraTimeRemaining(uint32 spellId, ::Unit* unit)
{
    if (!unit)
        unit = _bot;

    if (!unit)
        return 0;

    if (Aura* aura = unit->GetAura(spellId))
        return aura->GetDuration();

    return 0;
}

uint8 EvokerSpecialization::GetAuraStacks(uint32 spellId, ::Unit* unit)
{
    if (!unit)
        unit = _bot;

    if (!unit)
        return 0;

    if (Aura* aura = unit->GetAura(spellId))
        return aura->GetStackAmount();

    return 0;
}

uint32 EvokerSpecialization::GetEmpowermentChannelTime(EmpowermentLevel level)
{
    return static_cast<uint32>(level) * 1000; // 1 second per level
}

float EvokerSpecialization::CalculateEmpowermentEfficiency(uint32 spellId, EmpowermentLevel level, ::Unit* target)
{
    if (!target || level == EmpowermentLevel::NONE)
        return 0.0f;

    // Base efficiency calculation - higher levels are more efficient per essence spent
    float baseEfficiency = static_cast<float>(level) / 4.0f; // 0.25, 0.5, 0.75, 1.0

    // Adjust based on target health for healing spells
    if (spellId == DREAM_BREATH_EMPOWERED || spellId == SPIRIT_BLOOM_EMPOWERED)
    {
        float healthPercent = target->GetHealthPct() / 100.0f;
        baseEfficiency *= (1.0f - healthPercent + 0.2f); // More efficient on low health targets
    }

    // Adjust based on target count for AoE spells
    if (spellId == FIRE_BREATH_EMPOWERED || spellId == DREAM_BREATH_EMPOWERED)
    {
        std::vector<::Unit*> targets = GetEmpoweredSpellTargets(spellId);
        baseEfficiency *= std::min(3.0f, static_cast<float>(targets.size()));
    }

    return baseEfficiency;
}

bool EvokerSpecialization::IsChannelingEmpoweredSpell()
{
    return _currentEmpoweredSpell.isChanneling;
}

uint32 EvokerSpecialization::GetEssenceCost(uint32 spellId)
{
    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    // Essence costs for Evoker abilities
    switch (spellId)
    {
        case DISINTEGRATE:
        case PYRE:
        case FIRE_BREATH:
        case ETERNITYS_SURGE:
        case DREAM_BREATH:
        case SPIRIT_BLOOM:
        case BREATH_OF_EONS:
            return 3;
        case AZURE_STRIKE:
        case LIVING_FLAME:
        case EMERALD_BLOSSOM:
        case VERDANT_EMBRACE:
        case EBON_MIGHT:
        case PRESCIENCE:
            return 2;
        case DEEP_BREATH:
        case SHATTERING_STAR:
        case FIRESTORM:
        case TEMPORAL_ANOMALY:
        case SPATIAL_PARADOX:
            return 4;
        case HOVER:
        case SOAR:
        case WING_BUFFET:
        case RESCUE:
        case TIME_SPIRAL:
        case OBSIDIAN_SCALES:
        case RENEWING_BLAZE:
            return 0; // Utility spells typically don't cost essence
        default:
            return 1; // Default essence cost
    }
}

uint32 EvokerSpecialization::GetEssenceGenerated(uint32 spellId)
{
    // Some abilities generate essence
    switch (spellId)
    {
        case AZURE_STRIKE:
            return 1;
        case LIVING_FLAME:
            return 1;
        case DISINTEGRATE:
            return 1; // With certain talents
        default:
            return 0;
    }
}

float EvokerSpecialization::CalculateTargetPriority(::Unit* target)
{
    if (!target || !_bot)
        return 0.0f;

    float priority = 1.0f;

    // Distance priority (closer is better)
    float distance = _bot->GetDistance(target);
    priority += (30.0f - distance) / 30.0f;

    // Health priority (lower health = higher priority for healing, higher for damage)
    float healthPercent = target->GetHealthPct() / 100.0f;
    if (target->IsFriendlyTo(_bot))
    {
        priority += (1.0f - healthPercent) * 2.0f; // Healing priority
    }
    else
    {
        priority += healthPercent; // Damage priority
    }

    // Threat priority
    if (target->IsInCombat())
        priority += 0.5f;

    return priority;
}

bool EvokerSpecialization::IsValidTarget(::Unit* target)
{
    if (!target || !_bot)
        return false;

    if (target->isDead())
        return false;

    if (!target->IsInWorld())
        return false;

    if (!_bot->IsWithinDistInMap(target, 100.0f))
        return false;

    return true;
}

std::vector<::Unit*> EvokerSpecialization::GetNearbyAllies(float range)
{
    std::vector<::Unit*> allies;

    if (!_bot)
        return allies;

    // Add group members
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
        {
            if (Player* member = groupRef->GetSource())
            {
                if (member != _bot && IsValidTarget(member) && _bot->IsWithinDistInMap(member, range))
                {
                    allies.push_back(member);
                }
            }
        }
    }

    // Add nearby friendly NPCs
    std::list<Unit*> nearbyUnits;
    Trinity::AnyFriendlyUnitInObjectRangeCheck check(_bot, _bot, range);
    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, range);

    for (Unit* unit : nearbyUnits)
    {
        if (IsValidTarget(unit))
            allies.push_back(unit);
    }

    return allies;
}

std::vector<::Unit*> EvokerSpecialization::GetNearbyEnemies(float range)
{
    std::vector<::Unit*> enemies;

    if (!_bot)
        return enemies;

    std::list<Unit*> nearbyUnits;
    Trinity::AnyAttackableUnitExceptForOriginator check(_bot);
    Trinity::UnitListSearcher<Trinity::AnyAttackableUnitExceptForOriginator> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, range);

    for (Unit* unit : nearbyUnits)
    {
        if (IsValidTarget(unit) && _bot->IsWithinDistInMap(unit, range))
            enemies.push_back(unit);
    }

    return enemies;
}

bool EvokerSpecialization::CastSpell(uint32 spellId, ::Unit* target)
{
    if (!_bot)
        return false;

    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
    {
        TC_LOG_ERROR("playerbot", "EvokerSpecialization::CastSpell: Invalid spell ID {} for bot {}", spellId, _bot->GetName());
        return false;
    }

    if (!HasSpell(spellId))
    {
        TC_LOG_DEBUG("playerbot", "EvokerSpecialization::CastSpell: Bot {} doesn't have spell {}", _bot->GetName(), spellId);
        return false;
    }

    if (!HasEnoughResource(spellId))
    {
        TC_LOG_DEBUG("playerbot", "EvokerSpecialization::CastSpell: Bot {} doesn't have enough essence for spell {}", _bot->GetName(), spellId);
        return false;
    }

    // Cast the spell
    if (target && spellInfo->IsTargetingLocation())
    {
        _bot->CastSpell(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), spellId, false);
    }
    else if (target)
    {
        _bot->CastSpell(target, spellId, false);
    }
    else
    {
        _bot->CastSpell(_bot, spellId, false);
    }

    // Consume essence
    uint32 essenceCost = GetEssenceCost(spellId);
    if (essenceCost > 0)
    {
        SpendEssence(essenceCost);
        _totalEssenceSpent += essenceCost;
    }

    // Generate essence if applicable
    uint32 essenceGenerated = GetEssenceGenerated(spellId);
    if (essenceGenerated > 0)
    {
        GenerateEssence(essenceGenerated);
    }

    TC_LOG_DEBUG("playerbot", "EvokerSpecialization::CastSpell: Bot {} cast spell {} on target {}",
                _bot->GetName(), spellId, target ? target->GetName() : "self");

    return true;
}

bool EvokerSpecialization::HasSpell(uint32 spellId)
{
    if (!_bot)
        return false;

    return _bot->HasSpell(spellId);
}

SpellInfo const* EvokerSpecialization::GetSpellInfo(uint32 spellId)
{
    return sSpellMgr->GetSpellInfo(spellId);
}

uint32 EvokerSpecialization::GetSpellCooldown(uint32 spellId)
{
    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    return spellInfo->RecoveryTime;
}

void EvokerSpecialization::UpdateEssenceManagement()
{
    UpdateResourceStates();

    // Automatic essence regeneration is handled in UpdateResourceStates
    if (_essence.isRegenerating && _essence.current < _essence.maximum)
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _essence.lastGenerated >= _essence.generationRate)
        {
            GenerateEssence(1);
        }
    }
}

bool EvokerSpecialization::HasEssence(uint32 required)
{
    return _essence.HasEssence(required);
}

uint32 EvokerSpecialization::GetEssence()
{
    return _essence.current;
}

void EvokerSpecialization::SpendEssence(uint32 amount)
{
    _essence.SpendEssence(amount);
    _totalEssenceSpent += amount;

    // Update bot's power if needed
    if (_bot)
    {
        _bot->SetPower(POWER_ESSENCE, _essence.current);
    }
}

void EvokerSpecialization::GenerateEssence(uint32 amount)
{
    _essence.GenerateEssence(amount);

    // Update bot's power if needed
    if (_bot)
    {
        _bot->SetPower(POWER_ESSENCE, _essence.current);
    }
}

bool EvokerSpecialization::ShouldConserveEssence()
{
    // Conserve essence when low and not in burst phase
    if (_essence.state <= EssenceState::LOW && _combatPhase != CombatPhase::BURST_PHASE)
        return true;

    // Conserve essence if important cooldowns are coming up
    if (_combatPhase == CombatPhase::OPENER || _combatPhase == CombatPhase::EMPOWERMENT_WINDOW)
        return true;

    return false;
}

void EvokerSpecialization::UpdateAspectManagement()
{
    uint32 currentTime = getMSTime();

    // Update aspect cooldown
    if (!_aspect.canShift && currentTime - _aspect.lastShift >= _aspect.cooldown)
    {
        _aspect.canShift = true;
    }

    // Update aspect duration
    if (_aspect.current != EvokerAspect::NONE && _aspect.duration > 0)
    {
        if (currentTime - _aspect.lastShift >= _aspect.duration)
        {
            _aspect.current = EvokerAspect::NONE;
        }
    }
}

void EvokerSpecialization::ShiftToAspect(EvokerAspect aspect)
{
    if (!CanShiftAspect() || _aspect.current == aspect)
        return;

    uint32 spellId = 0;
    switch (aspect)
    {
        case EvokerAspect::BRONZE:
            spellId = BRONZE_ASPECT;
            break;
        case EvokerAspect::AZURE:
            spellId = AZURE_ASPECT;
            break;
        case EvokerAspect::GREEN:
            spellId = GREEN_ASPECT;
            break;
        case EvokerAspect::RED:
            spellId = RED_ASPECT;
            break;
        case EvokerAspect::BLACK:
            spellId = BLACK_ASPECT;
            break;
        default:
            return;
    }

    if (CastSpell(spellId))
    {
        _aspect.current = aspect;
        _aspect.lastShift = getMSTime();
        _aspect.canShift = false;
        LogRotationDecision("Shifted to Aspect", "Optimizing combat effectiveness");
    }
}

bool EvokerSpecialization::CanShiftAspect()
{
    return _aspect.canShift;
}

void EvokerSpecialization::StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target)
{
    if (IsChannelingEmpoweredSpell())
        return;

    _currentEmpoweredSpell = EmpoweredSpell(spellId, targetLevel, target);
    _totalEmpoweredSpells++;
    LogRotationDecision("Started Empowered Spell", "Channeling for optimal level");
}

void EvokerSpecialization::UpdateEmpoweredChanneling()
{
    if (!IsChannelingEmpoweredSpell())
        return;

    // Update current empowerment level based on channel time
    uint32 channelTime = _currentEmpoweredSpell.GetChannelTime();
    uint32 levelTime = 1000; // 1 second per level

    EmpowermentLevel newLevel = static_cast<EmpowermentLevel>(std::min(4u, channelTime / levelTime + 1));
    if (newLevel != _currentEmpoweredSpell.currentLevel)
    {
        _currentEmpoweredSpell.currentLevel = newLevel;
    }

    // Check if we should release the spell
    if (_currentEmpoweredSpell.ShouldRelease())
    {
        ReleaseEmpoweredSpell();
    }
}

void EvokerSpecialization::ReleaseEmpoweredSpell()
{
    if (!IsChannelingEmpoweredSpell())
        return;

    // Cast the empowered version
    uint32 empoweredSpellId = _currentEmpoweredSpell.spellId;
    ::Unit* target = _currentEmpoweredSpell.target;

    if (CastSpell(empoweredSpellId, target))
    {
        LogRotationDecision("Released Empowered Spell",
                          "Level " + std::to_string(static_cast<uint8>(_currentEmpoweredSpell.currentLevel)));
    }

    // Reset empowered spell state
    _currentEmpoweredSpell = EmpoweredSpell();
}

} // namespace Playerbot