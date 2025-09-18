/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FurySpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "Pet.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Log.h"
#include "PlayerbotAI.h"
#include "MotionMaster.h"
#include <algorithm>

namespace Playerbot
{

FurySpecialization::FurySpecialization(Player* bot)
    : WarriorSpecialization(bot), _isEnraged(false), _enrageEndTime(0), _flurryStacks(0),
      _flurryProc(false), _lastBerserkerRage(0), _lastBloodthirst(0), _lastEnrageCheck(0),
      _lastFlurryCheck(0), _lastDualWieldCheck(0), _lastRotationUpdate(0), _inExecutePhase(false),
      _executePhaseStartTime(0), _lastRageOptimization(0), _averageRageGeneration(0.0f)
{
}

void FurySpecialization::UpdateRotation(Unit* target)
{
    if (!target || !_bot)
        return;

    uint32 now = getMSTime();

    // Performance optimization - don't update rotation too frequently
    if (now - _lastRotationUpdate < 100) // 100ms throttle
        return;

    _lastRotationUpdate = now;

    // Update mechanics
    UpdateEnrage();
    UpdateFlurry();
    UpdateDualWield();
    UpdateStance();
    OptimizeRageGeneration();

    // Handle execute phase
    if (IsInExecutePhase(target))
    {
        HandleExecutePhase(target);
        return;
    }

    // Fury rotation priority
    // 1. Rampage if we have enough rage and enrage buff is ending
    if (ShouldCastRampage(target) && HasEnoughResource(RAMPAGE))
    {
        CastRampage(target);
        return;
    }

    // 2. Bloodthirst for rage generation and enrage
    if (ShouldCastBloodthirst(target) && HasEnoughResource(BLOODTHIRST))
    {
        CastBloodthirst(target);
        return;
    }

    // 3. Raging Blow if enraged
    if (IsEnraged() && !_bot->HasSpellCooldown(RAGING_BLOW) && HasEnoughResource(RAGING_BLOW_RAGE_COST))
    {
        CastRagingBlow(target);
        return;
    }

    // 4. Whirlwind for multiple enemies
    if (ShouldCastWhirlwind())
    {
        CastWhirlwind();
        return;
    }

    // 5. Furious Slash as filler
    if (!_bot->HasSpellCooldown(FURIOUS_SLASH) && HasEnoughResource(12))
    {
        CastFuriousSlash(target);
        return;
    }

    // 6. Heroic Strike if high on rage
    if (GetRagePercent() > RAGE_DUMP_THRESHOLD && HasEnoughResource(15))
    {
        if (_bot->CastSpell(target, HEROIC_STRIKE, false))
        {
            TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} cast heroic strike (rage dump)",
                        _bot->GetName());
        }
        return;
    }

    // 7. Basic attacks if in range
    if (IsInMeleeRange(target) && !_bot->HasUnitState(UNIT_STATE_CASTING))
    {
        _bot->AttackerStateUpdate(target);
    }
}

void FurySpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain battle shout
    CastShout();

    // Use berserker rage when needed
    if (ShouldUseBerserkerRage())
    {
        CastBerserkerRage();
    }

    // Use recklessness during high damage phases
    if (ShouldUseRecklessness())
    {
        UseRecklessness();
    }

    // Emergency regeneration
    if (ShouldUseEnragedRegeneration())
    {
        UseEnragedRegeneration();
    }

    // Maintain dual wield if available
    if (HasDualWieldWeapons() && !_bot->HasAura(DUAL_WIELD))
    {
        if (_bot->CastSpell(_bot, DUAL_WIELD, false))
        {
            TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} activated dual wield",
                        _bot->GetName());
        }
    }
}

void FurySpecialization::UpdateCooldowns(uint32 diff)
{
    // Update internal cooldown tracking
    for (auto& [spellId, cooldownEnd] : _cooldowns)
    {
        if (cooldownEnd > diff)
            cooldownEnd -= diff;
        else
            cooldownEnd = 0;
    }

    UpdateFuryCooldowns(diff);
}

bool FurySpecialization::CanUseAbility(uint32 spellId)
{
    if (!_bot)
        return false;

    if (_bot->HasSpellCooldown(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    return CanUseAbility();
}

void FurySpecialization::OnCombatStart(Unit* target)
{
    if (!target || !_bot)
        return;

    // Switch to berserker stance
    if (!IsInStance(WarriorStance::BERSERKER))
    {
        SwitchStance(WarriorStance::BERSERKER);
    }

    // Charge if not in melee range
    if (!IsInMeleeRange(target) && !_bot->HasSpellCooldown(CHARGE))
    {
        CastCharge(target);
    }

    // Reset combat state
    _inExecutePhase = false;
    _executePhaseStartTime = 0;
    _lastRageOptimization = getMSTime();

    TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} entered combat with target {}",
                _bot->GetName(), target->GetName());
}

void FurySpecialization::OnCombatEnd()
{
    // Reset combat state
    _isEnraged = false;
    _enrageEndTime = 0;
    _flurryStacks = 0;
    _flurryProc = false;
    _inExecutePhase = false;
    _executePhaseStartTime = 0;

    TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} combat ended",
                _bot->GetName());
}

bool FurySpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case BLOODTHIRST:
            return HasEnoughRage(BLOODTHIRST_RAGE_COST);
        case RAMPAGE:
            return HasEnoughRage(RAMPAGE_RAGE_COST);
        case RAGING_BLOW:
            return HasEnoughRage(RAGING_BLOW_RAGE_COST);
        case FURIOUS_SLASH:
            return HasEnoughRage(12);
        case EXECUTE:
            return HasEnoughRage(20);
        case WHIRLWIND:
            return HasEnoughRage(25);
        case BERSERKER_RAGE:
            return true; // No resource cost
        default:
            return HasEnoughRage(15); // Default rage cost
    }
}

void FurySpecialization::ConsumeResource(uint32 spellId)
{
    // Resources are consumed automatically by spell casting
    // This method is for tracking purposes
    uint32 now = getMSTime();
    _cooldowns[spellId] = now + 1500; // 1.5s global cooldown
}

Position FurySpecialization::GetOptimalPosition(Unit* target)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    // Stay in melee range for fury warrior
    Position targetPos = target->GetPosition();
    Position botPos = _bot->GetPosition();

    float distance = _bot->GetDistance2d(target);

    // If too far, move closer
    if (distance > OPTIMAL_MELEE_RANGE)
    {
        float angle = target->GetAngle(_bot);
        targetPos.m_positionX += cos(angle + M_PI) * (OPTIMAL_MELEE_RANGE - 1.0f);
        targetPos.m_positionY += sin(angle + M_PI) * (OPTIMAL_MELEE_RANGE - 1.0f);
        return targetPos;
    }

    // If too close, move to optimal range
    if (distance < 2.0f)
    {
        float angle = _bot->GetAngle(target);
        botPos.m_positionX += cos(angle) * 3.0f;
        botPos.m_positionY += sin(angle) * 3.0f;
        return botPos;
    }

    return botPos;
}

float FurySpecialization::GetOptimalRange(Unit* target)
{
    return OPTIMAL_MELEE_RANGE;
}

void FurySpecialization::UpdateStance()
{
    // Fury warriors should primarily use berserker stance
    if (!IsInStance(WarriorStance::BERSERKER))
    {
        SwitchStance(WarriorStance::BERSERKER);
    }
}

WarriorStance FurySpecialization::GetOptimalStance(Unit* target)
{
    return WarriorStance::BERSERKER;
}

void FurySpecialization::SwitchStance(WarriorStance stance)
{
    if (stance == WarriorStance::BERSERKER)
    {
        EnterBerserkerStance();
    }
    else
    {
        WarriorSpecialization::SwitchStance(stance);
    }
}

// Private methods

void FurySpecialization::UpdateEnrage()
{
    uint32 now = getMSTime();

    if (now - _lastEnrageCheck < 500) // Check every 500ms
        return;

    _lastEnrageCheck = now;

    // Check if we have enrage aura
    bool hasEnrageAura = _bot->HasAura(ENRAGE);

    if (hasEnrageAura && !_isEnraged)
    {
        _isEnraged = true;
        _enrageEndTime = now + ENRAGE_DURATION;
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} became enraged",
                    _bot->GetName());
    }
    else if (!hasEnrageAura && _isEnraged)
    {
        _isEnraged = false;
        _enrageEndTime = 0;
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} lost enrage",
                    _bot->GetName());
    }

    // Update enrage timer
    if (_isEnraged && now >= _enrageEndTime)
    {
        _isEnraged = false;
        _enrageEndTime = 0;
    }
}

void FurySpecialization::UpdateFlurry()
{
    uint32 now = getMSTime();

    if (now - _lastFlurryCheck < 500) // Check every 500ms
        return;

    _lastFlurryCheck = now;

    UpdateFlurryStacks();

    // Check for flurry proc
    _flurryProc = HasFlurryProc();

    if (_flurryProc)
    {
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} has flurry proc available",
                    _bot->GetName());
    }
}

void FurySpecialization::UpdateBerserkerRage()
{
    // Berserker rage management is handled in UpdateBuffs()
}

void FurySpecialization::UpdateDualWield()
{
    uint32 now = getMSTime();

    if (now - _lastDualWieldCheck < 2000) // Check every 2 seconds
        return;

    _lastDualWieldCheck = now;

    if (HasDualWieldWeapons())
    {
        OptimizeDualWield();
    }
}

bool FurySpecialization::ShouldCastBloodthirst(Unit* target)
{
    if (!target || !_bot)
        return false;

    // Always prioritize bloodthirst for rage generation
    if (_bot->HasSpellCooldown(BLOODTHIRST))
        return false;

    if (!IsInMeleeRange(target))
        return false;

    // Use bloodthirst to maintain enrage
    if (!_isEnraged)
        return true;

    // Use if we need rage
    if (GetRagePercent() < 40.0f)
        return true;

    return true; // Bloodthirst is core to fury rotation
}

bool FurySpecialization::ShouldCastWhirlwind()
{
    if (!_bot)
        return false;

    if (_bot->HasSpellCooldown(WHIRLWIND))
        return false;

    // Check for multiple enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 8.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 8.0f);

    return targets.size() >= 2;
}

bool FurySpecialization::ShouldCastRampage(Unit* target)
{
    if (!target || !_bot)
        return false;

    if (_bot->HasSpellCooldown(RAMPAGE))
        return false;

    if (!IsInMeleeRange(target))
        return false;

    // Use rampage when we have high rage
    if (GetRagePercent() >= 85.0f)
        return true;

    // Use rampage if enrage is about to expire
    if (_isEnraged && GetEnrageTimeRemaining() < 2000)
        return true;

    return false;
}

bool FurySpecialization::ShouldCastExecute(Unit* target)
{
    if (!target || !_bot)
        return false;

    if (_bot->HasSpellCooldown(EXECUTE))
        return false;

    return IsInExecutePhase(target);
}

bool FurySpecialization::ShouldUseBerserkerRage()
{
    if (!_bot)
        return false;

    uint32 now = getMSTime();

    if (now - _lastBerserkerRage < 30000) // 30 second cooldown
        return false;

    // Use when feared, charmed, or incapacitated
    if (_bot->HasUnitState(UNIT_STATE_FEARED | UNIT_STATE_CHARMED | UNIT_STATE_CONFUSED))
        return true;

    // Use for rage generation when low
    if (GetRagePercent() < 20.0f && !_isEnraged)
        return true;

    return false;
}

void FurySpecialization::OptimizeDualWield()
{
    if (!_bot)
        return;

    // Ensure dual wield passive is active
    if (!_bot->HasAura(DUAL_WIELD))
    {
        if (_bot->CastSpell(_bot, DUAL_WIELD, false))
        {
            TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} activated dual wield",
                        _bot->GetName());
        }
    }

    // Update offhand attacks
    UpdateOffhandAttacks();
}

void FurySpecialization::UpdateOffhandAttacks()
{
    // Offhand attacks are handled automatically by the core
    // This method is for any custom offhand attack logic
}

bool FurySpecialization::HasDualWieldWeapons()
{
    if (!_bot)
        return false;

    Item* mainHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    return mainHand && offHand && offHand->GetTemplate()->Class == ITEM_CLASS_WEAPON;
}

void FurySpecialization::CastFlurry()
{
    if (!_bot || !_flurryProc)
        return;

    if (_bot->CastSpell(_bot, FLURRY, false))
    {
        ConsumeFlurry();
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} used flurry proc",
                    _bot->GetName());
    }
}

void FurySpecialization::TriggerEnrage()
{
    if (!_bot)
        return;

    uint32 now = getMSTime();
    _isEnraged = true;
    _enrageEndTime = now + ENRAGE_DURATION;

    TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} triggered enrage",
                _bot->GetName());
}

void FurySpecialization::ManageEnrage()
{
    if (!_bot)
        return;

    UpdateEnrage();

    // Try to maintain enrage uptime
    if (!_isEnraged && !_bot->HasSpellCooldown(BLOODTHIRST))
    {
        // Bloodthirst has a chance to trigger enrage
        Unit* target = _bot->GetSelectedUnit();
        if (target && IsInMeleeRange(target))
        {
            CastBloodthirst(target);
        }
    }
}

bool FurySpecialization::IsEnraged()
{
    return _isEnraged;
}

uint32 FurySpecialization::GetEnrageTimeRemaining()
{
    if (!_isEnraged)
        return 0;

    uint32 now = getMSTime();
    if (now >= _enrageEndTime)
        return 0;

    return _enrageEndTime - now;
}

void FurySpecialization::ExtendEnrage()
{
    if (_isEnraged)
    {
        _enrageEndTime += 2000; // Extend by 2 seconds
    }
}

void FurySpecialization::CastBloodthirst(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, BLOODTHIRST, false))
    {
        _lastBloodthirst = getMSTime();
        ConsumeResource(BLOODTHIRST);
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} cast bloodthirst on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void FurySpecialization::CastRampage(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, RAMPAGE, false))
    {
        ConsumeResource(RAMPAGE);
        TriggerEnrage(); // Rampage triggers enrage
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} cast rampage on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void FurySpecialization::CastRagingBlow(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, RAGING_BLOW, false))
    {
        ConsumeResource(RAGING_BLOW);
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} cast raging blow on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void FurySpecialization::CastFuriousSlash(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, FURIOUS_SLASH, false))
    {
        ConsumeResource(FURIOUS_SLASH);
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} cast furious slash on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void FurySpecialization::CastExecute(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, EXECUTE, false))
    {
        ConsumeResource(EXECUTE);
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} executed target {}",
                    _bot->GetName(), target->GetName());
    }
}

void FurySpecialization::CastWhirlwind()
{
    if (!_bot)
        return;

    if (_bot->CastSpell(_bot, WHIRLWIND, false))
    {
        ConsumeResource(WHIRLWIND);
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} cast whirlwind",
                    _bot->GetName());
    }
}

void FurySpecialization::CastBerserkerRage()
{
    if (!_bot)
        return;

    if (_bot->CastSpell(_bot, BERSERKER_RAGE, false))
    {
        _lastBerserkerRage = getMSTime();
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} used berserker rage",
                    _bot->GetName());
    }
}

void FurySpecialization::UpdateBerserkerStance()
{
    if (!IsInStance(WarriorStance::BERSERKER))
    {
        SwitchStance(WarriorStance::BERSERKER);
    }
}

bool FurySpecialization::ShouldStayInBerserkerStance()
{
    // Fury warriors should always stay in berserker stance
    return true;
}

void FurySpecialization::OptimizeRageGeneration()
{
    uint32 now = getMSTime();

    if (now - _lastRageOptimization < 1000) // Check every second
        return;

    _lastRageOptimization = now;

    float currentRage = GetRagePercent();

    // Build rage if low
    if (currentRage < 30.0f)
    {
        BuildRage();
    }
    // Spend rage if high
    else if (currentRage > 80.0f)
    {
        SpendRageEfficiently();
    }
}

void FurySpecialization::BuildRage()
{
    if (!_bot)
        return;

    // Use berserker rage if available
    if (ShouldUseBerserkerRage())
    {
        CastBerserkerRage();
        return;
    }

    // Use bloodthirst for rage generation
    Unit* target = _bot->GetSelectedUnit();
    if (target && ShouldCastBloodthirst(target))
    {
        CastBloodthirst(target);
    }
}

bool FurySpecialization::ShouldConserveRage()
{
    return GetRagePercent() < 30.0f;
}

void FurySpecialization::SpendRageEfficiently()
{
    if (!_bot)
        return;

    Unit* target = _bot->GetSelectedUnit();
    if (!target)
        return;

    // Prioritize high-impact abilities
    if (ShouldCastRampage(target) && HasEnoughResource(RAMPAGE))
    {
        CastRampage(target);
    }
    else if (IsEnraged() && HasEnoughResource(RAGING_BLOW_RAGE_COST))
    {
        CastRagingBlow(target);
    }
    else if (HasEnoughResource(15))
    {
        // Use heroic strike as rage dump
        _bot->CastSpell(target, HEROIC_STRIKE, false);
    }
}

void FurySpecialization::UpdateFuryCooldowns(uint32 diff)
{
    // Update fury-specific cooldown tracking
    // Most cooldowns are handled by the core system
}

void FurySpecialization::UseRecklessness()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(RECKLESSNESS))
        return;

    if (_bot->CastSpell(_bot, RECKLESSNESS, false))
    {
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} used recklessness",
                    _bot->GetName());
    }
}

void FurySpecialization::UseEnragedRegeneration()
{
    if (!_bot)
        return;

    if (_bot->GetHealthPct() > 40.0f)
        return;

    if (_bot->HasSpellCooldown(ENRAGED_REGENERATION))
        return;

    if (_bot->CastSpell(_bot, ENRAGED_REGENERATION, false))
    {
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} used enraged regeneration",
                    _bot->GetName());
    }
}

bool FurySpecialization::ShouldUseRecklessness()
{
    if (!_bot)
        return false;

    // Use during high damage phases
    Unit* target = _bot->GetSelectedUnit();
    if (!target)
        return false;

    // Use when target is low health for execute phase
    if (IsInExecutePhase(target))
        return true;

    // Use when we have high rage and are enraged
    if (_isEnraged && GetRagePercent() > 60.0f)
        return true;

    return false;
}

bool FurySpecialization::ShouldUseEnragedRegeneration()
{
    if (!_bot)
        return false;

    return _bot->GetHealthPct() < 40.0f && !_bot->HasSpellCooldown(ENRAGED_REGENERATION);
}

void FurySpecialization::HandleExecutePhase(Unit* target)
{
    if (!target || !_bot)
        return;

    if (!_inExecutePhase)
    {
        _inExecutePhase = true;
        _executePhaseStartTime = getMSTime();
        TC_LOG_DEBUG("playerbots", "FurySpecialization: Bot {} entered execute phase",
                    _bot->GetName());
    }

    OptimizeExecuteRotation(target);
}

bool FurySpecialization::IsInExecutePhase(Unit* target)
{
    if (!target)
        return false;

    return target->GetHealthPct() <= EXECUTE_HEALTH_THRESHOLD;
}

void FurySpecialization::OptimizeExecuteRotation(Unit* target)
{
    if (!target || !_bot)
        return;

    // Execute phase priority: Execute > Bloodthirst > Raging Blow > Rampage

    // 1. Execute if available
    if (ShouldCastExecute(target) && HasEnoughResource(EXECUTE))
    {
        CastExecute(target);
        return;
    }

    // 2. Bloodthirst for rage generation
    if (ShouldCastBloodthirst(target) && HasEnoughResource(BLOODTHIRST))
    {
        CastBloodthirst(target);
        return;
    }

    // 3. Raging Blow if enraged
    if (IsEnraged() && !_bot->HasSpellCooldown(RAGING_BLOW) && HasEnoughResource(RAGING_BLOW_RAGE_COST))
    {
        CastRagingBlow(target);
        return;
    }

    // 4. Rampage if high rage
    if (GetRagePercent() > 80.0f && ShouldCastRampage(target) && HasEnoughResource(RAMPAGE))
    {
        CastRampage(target);
        return;
    }
}

void FurySpecialization::UpdateFlurryStacks()
{
    if (!_bot)
        return;

    // Check flurry aura stacks
    if (Aura* flurryAura = _bot->GetAura(FLURRY))
    {
        _flurryStacks = flurryAura->GetStackAmount();
    }
    else
    {
        _flurryStacks = 0;
    }
}

uint32 FurySpecialization::GetFlurryStacks()
{
    return _flurryStacks;
}

bool FurySpecialization::HasFlurryProc()
{
    if (!_bot)
        return false;

    return _bot->HasAura(FLURRY) && _flurryStacks > 0;
}

void FurySpecialization::ConsumeFlurry()
{
    _flurryProc = false;
    _flurryStacks = 0;
}

} // namespace Playerbot