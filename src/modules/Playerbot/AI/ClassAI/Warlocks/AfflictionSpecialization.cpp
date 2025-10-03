/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AfflictionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Map.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

namespace Playerbot
{

AfflictionSpecialization::AfflictionSpecialization(Player* bot)
    : WarlockSpecialization(bot)
    , _corruptionTargets(0)
    , _curseOfAgonyTargets(0)
    , _unstableAfflictionStacks(0)
    , _lastDrainLife(0)
    , _lastDrainSoul(0)
    , _lastDarkRitual(0)
    , _lastLifeTap(0)
    , _isChanneling(false)
    , _drainTarget(nullptr)
    , _afflictionMetrics() // Initialize metrics struct with defaults
    , _maxDoTTargets(MAX_DOT_TARGETS)
    , _lastDoTSpread(0)
{
}

void AfflictionSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateDoTManagement();
    UpdateDrainRotation();
    ManageLifeTap();

    if (ShouldCastUnstableAffliction(target))
    {
        CastUnstableAffliction(target);
        return;
    }

    if (ShouldCastCorruption(target))
    {
        CastCorruption(target);
        return;
    }

    if (ShouldCastCurseOfAgony(target))
    {
        CastCurseOfAgony(target);
        return;
    }

    if (ShouldCastDrainLife(target))
    {
        CastDrainLife(target);
        return;
    }

    if (ShouldCastSeedOfCorruption(target))
    {
        CastSeedOfCorruption(target);
        return;
    }

    CastShadowBolt(target);
}

void AfflictionSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasAura(FEL_ARMOR) && bot->HasSpell(FEL_ARMOR))
    {
        bot->CastSpell(bot, FEL_ARMOR, false);
    }

    UpdatePetManagement();
}

void AfflictionSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_lastDrainLife > diff)
        _lastDrainLife -= diff;
    else
        _lastDrainLife = 0;

    if (_lastDrainSoul > diff)
        _lastDrainSoul -= diff;
    else
        _lastDrainSoul = 0;

    if (_lastDarkRitual > diff)
        _lastDarkRitual -= diff;
    else
        _lastDarkRitual = 0;

    if (_lastLifeTap > diff)
        _lastLifeTap -= diff;
    else
        _lastLifeTap = 0;
}

bool AfflictionSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void AfflictionSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SummonOptimalPet();
    _dotTargets.clear();
    _isChanneling = false;
}

void AfflictionSpecialization::OnCombatEnd()
{
    _isChanneling = false;
    _drainTarget = nullptr;
    _dotTargets.clear();
    _activeDoTs.clear();
    _cooldowns.clear();
}

bool AfflictionSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return true;

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }
    return bot->GetPower(POWER_MANA) >= manaCost;
}

void AfflictionSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }
    if (bot->GetPower(POWER_MANA) >= manaCost)
        bot->SetPower(POWER_MANA, bot->GetPower(POWER_MANA) - manaCost);
}

Position AfflictionSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    float distance = OPTIMAL_CASTING_RANGE * 0.8f;
    float angle = target->GetAbsoluteAngle(bot) + M_PI;

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float AfflictionSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

void AfflictionSpecialization::UpdatePetManagement()
{
    if (!IsPetAlive())
        SummonOptimalPet();
}

void AfflictionSpecialization::SummonOptimalPet()
{
    WarlockPet optimalPet = GetOptimalPetForSituation();
    SummonPet(optimalPet);
}

WarlockPet AfflictionSpecialization::GetOptimalPetForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockPet::IMP;

    if (bot->IsInCombat())
        return WarlockPet::FELHUNTER;
    else
        return WarlockPet::IMP;
}

void AfflictionSpecialization::CommandPet(uint32 action, ::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !IsPetAlive())
        return;

    switch (action)
    {
        case PET_ATTACK:
            if (target)
                PetAttackTarget(target);
            break;
        case PET_FOLLOW:
            PetFollow();
            break;
    }
}

void AfflictionSpecialization::UpdateDoTManagement()
{
    uint32 now = getMSTime();
    if (now - _lastDoTCheck < DOT_CHECK_INTERVAL)
        return;
    _lastDoTCheck = now;

    RefreshExpiringDoTs();
    SpreadDoTsToMultipleTargets();
}

void AfflictionSpecialization::ApplyDoTsToTarget(::Unit* target)
{
    if (!target || !IsTargetWorthDoTting(target))
        return;

    if (ShouldCastCorruption(target))
        CastCorruption(target);

    if (ShouldCastCurseOfAgony(target))
        CastCurseOfAgony(target);

    if (ShouldCastUnstableAffliction(target))
        CastUnstableAffliction(target);
}

bool AfflictionSpecialization::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    if (!target || !IsTargetWorthDoTting(target))
        return false;

    return !IsDoTActive(target, spellId);
}

void AfflictionSpecialization::UpdateCurseManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    ::Unit* target = bot->GetSelectedUnit();
    if (target)
    {
        uint32 optimalCurse = GetOptimalCurseForTarget(target);
        if (optimalCurse && !target->HasAura(optimalCurse))
            CastCurse(target, optimalCurse);
    }
}

uint32 AfflictionSpecialization::GetOptimalCurseForTarget(::Unit* target)
{
    if (!target)
        return 0;

    if (target->GetCreatureType() == CREATURE_TYPE_ELEMENTAL)
        return CURSE_OF_ELEMENTS;

    return CURSE_OF_AGONY;
}

void AfflictionSpecialization::UpdateSoulShardManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Basic soul shard management - conserve when low
    if (_soulShards.count < 5)
        _soulShards.conserveMode = true;
    else if (_soulShards.count > 15)
        _soulShards.conserveMode = false;
}

bool AfflictionSpecialization::HasSoulShardsAvailable(uint32 required)
{
    return _soulShards.count >= required;
}

void AfflictionSpecialization::UseSoulShard(uint32 spellId)
{
    if (_soulShards.count > 0)
    {
        _soulShards.count--;
        _soulShards.lastUsed = getMSTime();
    }
}

bool AfflictionSpecialization::ShouldCastCorruption(::Unit* target)
{
    return target && !IsDoTActive(target, CORRUPTION) &&
           HasEnoughResource(CORRUPTION) &&
           IsTargetWorthDoTting(target);
}

bool AfflictionSpecialization::ShouldCastUnstableAffliction(::Unit* target)
{
    return target && !IsDoTActive(target, UNSTABLE_AFFLICTION) &&
           HasEnoughResource(UNSTABLE_AFFLICTION) &&
           IsTargetWorthDoTting(target) &&
           HasSoulShardsAvailable(1);
}

bool AfflictionSpecialization::ShouldCastCurseOfAgony(::Unit* target)
{
    return target && !IsDoTActive(target, CURSE_OF_AGONY) &&
           HasEnoughResource(CURSE_OF_AGONY) &&
           IsTargetWorthDoTting(target);
}

bool AfflictionSpecialization::ShouldCastDrainLife(::Unit* target)
{
    Player* bot = GetBot();
    return target && bot && bot->GetHealthPct() < DRAIN_HEALTH_THRESHOLD &&
           HasEnoughResource(DRAIN_LIFE) &&
           _lastDrainLife == 0;
}

bool AfflictionSpecialization::ShouldCastSeedOfCorruption(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    std::list<Unit*> units;
    Trinity::AnyUnitInObjectRangeCheck u_check(target, 15.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(target, units, u_check);
    Cell::VisitAllObjects(target, searcher, 15.0f);

    uint32 enemyCount = 0;
    for (Unit* unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive())
            enemyCount++;
    }

    return enemyCount >= 4 && HasEnoughResource(SEED_OF_CORRUPTION);
}

void AfflictionSpecialization::CastCorruption(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(CORRUPTION))
    {
        bot->CastSpell(target, CORRUPTION, false);
        ConsumeResource(CORRUPTION);
        _corruptionTargets++;
    }
}

void AfflictionSpecialization::CastUnstableAffliction(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(UNSTABLE_AFFLICTION) && HasSoulShardsAvailable(1))
    {
        bot->CastSpell(target, UNSTABLE_AFFLICTION, false);
        ConsumeResource(UNSTABLE_AFFLICTION);
        UseSoulShard(UNSTABLE_AFFLICTION);
        _unstableAfflictionStacks++;
    }
}

void AfflictionSpecialization::CastCurseOfAgony(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(CURSE_OF_AGONY))
    {
        bot->CastSpell(target, CURSE_OF_AGONY, false);
        ConsumeResource(CURSE_OF_AGONY);
        _curseOfAgonyTargets++;
    }
}

void AfflictionSpecialization::CastDrainLife(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(DRAIN_LIFE))
    {
        bot->CastSpell(target, DRAIN_LIFE, false);
        ConsumeResource(DRAIN_LIFE);
        _lastDrainLife = DRAIN_CHANNEL_TIME;
        _isChanneling = true;
        _drainTarget = target;
    }
}

void AfflictionSpecialization::CastSeedOfCorruption(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(SEED_OF_CORRUPTION))
    {
        bot->CastSpell(target, SEED_OF_CORRUPTION, false);
        ConsumeResource(SEED_OF_CORRUPTION);
    }
}

void AfflictionSpecialization::CastShadowBolt(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(SHADOW_BOLT))
    {
        bot->CastSpell(target, SHADOW_BOLT, false);
        ConsumeResource(SHADOW_BOLT);
    }
}

void AfflictionSpecialization::ManageLifeTap()
{
    if (ShouldUseLifeTap())
        CastLifeTap();
}

bool AfflictionSpecialization::ShouldUseLifeTap()
{
    Player* bot = GetBot();
    return bot && bot->GetPowerPct(POWER_MANA) < LIFE_TAP_MANA_THRESHOLD &&
           bot->GetHealthPct() > 50.0f &&
           _lastLifeTap == 0;
}

void AfflictionSpecialization::CastLifeTap()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(LIFE_TAP))
    {
        bot->CastSpell(bot, LIFE_TAP, false);
        _lastLifeTap = 1500;
        _afflictionMetrics.manaFromLifeTap += 500; // Approximate
    }
}

void AfflictionSpecialization::RefreshExpiringDoTs()
{
    // Simplified DoT refresh logic
    for (auto& dotPair : _activeDoTs)
    {
        for (auto& dot : dotPair.second)
        {
            if (dot.remainingTime < 6000 && dot.target && dot.target->IsAlive())
            {
                ApplyDoTsToTarget(dot.target);
            }
        }
    }
}

void AfflictionSpecialization::SpreadDoTsToMultipleTargets()
{
    std::vector<::Unit*> targets = GetDoTTargets(_maxDoTTargets);
    for (::Unit* target : targets)
    {
        ApplyDoTsToTarget(target);
    }
}

::Unit* AfflictionSpecialization::GetBestDoTTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    return bot->GetSelectedUnit();
}

std::vector<::Unit*> AfflictionSpecialization::GetDoTTargets(uint32 maxTargets)
{
    std::vector<::Unit*> targets;
    Player* bot = GetBot();
    if (!bot)
        return targets;

    std::list<Unit*> units;
    Trinity::AnyUnitInObjectRangeCheck u_check(bot, OPTIMAL_CASTING_RANGE);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(bot, units, u_check);
    Cell::VisitAllObjects(bot, searcher, OPTIMAL_CASTING_RANGE);

    for (Unit* unit : units)
    {
        if (unit && unit->IsHostileTo(bot) && unit->IsAlive() && IsTargetWorthDoTting(unit))
        {
            targets.push_back(unit);
            if (targets.size() >= maxTargets)
                break;
        }
    }

    return targets;
}

bool AfflictionSpecialization::IsTargetWorthDoTting(::Unit* target)
{
    if (!target)
        return false;

    // Don't DoT targets that will die quickly
    return target->GetHealthPct() > 30.0f &&
           target->GetHealth() > 10000;
}

void AfflictionSpecialization::UpdateDrainRotation()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Only update drain rotation during combat
    if (!bot->IsInCombat())
        return;

    // If already channeling, check if we should continue or switch targets
    if (_isChanneling)
    {
        // Verify drain target is still valid
        if (!_drainTarget || !_drainTarget->IsAlive() || !bot->IsValidAttackTarget(_drainTarget))
        {
            // Stop channeling invalid target
            bot->InterruptSpell(CURRENT_CHANNELED_SPELL);
            _isChanneling = false;
            _drainTarget = nullptr;
            _lastDrainLife = 0;
            return;
        }

        // Continue channeling if target is still optimal
        if (ShouldChannelDrain())
            return;

        // Otherwise interrupt and switch
        bot->InterruptSpell(CURRENT_CHANNELED_SPELL);
        _isChanneling = false;
        _drainTarget = nullptr;
        _lastDrainLife = 0;
    }

    // Find best target for draining
    ::Unit* drainTarget = GetBestDrainTarget();
    if (!drainTarget)
        return;

    // Decide which drain spell to use based on situation
    if (ShouldCastDrainLife(drainTarget))
    {
        CastDrainLife(drainTarget);
    }
    else if (bot->GetPowerPct(POWER_MANA) < 30.0f && HasEnoughResource(DRAIN_MANA))
    {
        // Use Drain Mana when low on mana and target has mana
        if (drainTarget->GetPowerType() == POWER_MANA && drainTarget->GetPower(POWER_MANA) > 0)
        {
            if (bot->CastSpell(drainTarget, DRAIN_MANA, false) == SPELL_CAST_OK)
            {
                ConsumeResource(DRAIN_MANA);
                _isChanneling = true;
                _drainTarget = drainTarget;
            }
        }
    }
    else if (drainTarget->GetHealthPct() < 25.0f && HasEnoughResource(DRAIN_SOUL))
    {
        // Use Drain Soul on low health targets for soul shard generation
        CastDrainSoul(drainTarget);
    }
}

void AfflictionSpecialization::CastDrainSoul(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(DRAIN_SOUL))
    {
        if (bot->CastSpell(target, DRAIN_SOUL, false) == SPELL_CAST_OK)
        {
            ConsumeResource(DRAIN_SOUL);
            _isChanneling = true;
            _drainTarget = target;

            // Drain Soul generates soul shards when target dies during channel
            if (target->GetHealthPct() < 25.0f)
            {
                _soulShards.count = std::min(_soulShards.count + 1, 20u); // Cap at 20 soul shards
            }
        }
    }
}

bool AfflictionSpecialization::ShouldChannelDrain()
{
    Player* bot = GetBot();
    if (!bot || !_drainTarget)
        return false;

    // Continue channeling if target is still valid and in range
    if (!_drainTarget->IsAlive() || !bot->IsValidAttackTarget(_drainTarget))
        return false;

    // Check if target is still in range
    float distance = bot->GetDistance(_drainTarget);
    if (distance > OPTIMAL_CASTING_RANGE)
        return false;

    // Continue drain if target health is appropriate for current drain type
    if (_lastDrainLife > 0)
    {
        // Continue Drain Life if we still need health
        return bot->GetHealthPct() < DRAIN_HEALTH_THRESHOLD + 10.0f;
    }

    // Continue other drains for their full duration
    return true;
}

::Unit* AfflictionSpecialization::GetBestDrainTarget()
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    // Priority 1: Current selected target if valid
    ::Unit* currentTarget = bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() && bot->IsValidAttackTarget(currentTarget))
    {
        float distance = bot->GetDistance(currentTarget);
        if (distance <= OPTIMAL_CASTING_RANGE)
            return currentTarget;
    }

    // Priority 2: Find nearest hostile target in range
    std::list<Unit*> targets;
    Trinity::AnyUnitInObjectRangeCheck u_check(bot, OPTIMAL_CASTING_RANGE);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
    Cell::VisitAllObjects(bot, searcher, OPTIMAL_CASTING_RANGE);

    ::Unit* bestTarget = nullptr;
    float nearestDistance = OPTIMAL_CASTING_RANGE + 1.0f;

    for (Unit* target : targets)
    {
        if (!target || !target->IsAlive() || !bot->IsValidAttackTarget(target))
            continue;

        float distance = bot->GetDistance(target);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            bestTarget = target;
        }
    }

    return bestTarget;
}

} // namespace Playerbot