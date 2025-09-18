/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MarksmanshipSpecialization.h"
#include "Player.h"
#include "Pet.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

MarksmanshipSpecialization::MarksmanshipSpecialization(Player* bot)
    : HunterSpecialization(bot)
    , _mmRotationPhase(MMRotationPhase::OPENING)
    , _currentStance(CombatStance::AGGRESSIVE)
    , _lastAutoShotStart(0)
    , _autoShotDuration(0)
    , _nextAutoShotTime(0)
    , _lastShotWeaveUpdate(0)
    , _lastStanceUpdate(0)
    , _lastManaCheck(0)
    , _lastCooldownCheck(0)
    , _lastTargetAnalysis(0)
    , _rapidFireReady(0)
    , _readinessReady(0)
    , _silencingShotReady(0)
    , _lastRapidFire(0)
    , _lastReadiness(0)
    , _lastSilencingShot(0)
    , _totalShotsFired(0)
    , _aimedShotsHit(0)
    , _steadyShotsHit(0)
    , _chimeraShots(0)
    , _killShots(0)
    , _multiShotsUsed(0)
    , _autoShotsHit(0)
    , _averageShotDamage(0.0f)
    , _dpsLastInterval(0.0f)
    , _manaEfficiency(0)
    , _nearbyTargetCount(0)
    , _priorityTargetCount(0)
    , _primaryTarget(ObjectGuid::Empty)
    , _secondaryTarget(ObjectGuid::Empty)
    , _inBurstMode(false)
    , _conservingMana(false)
    , _castingAimedShot(false)
    , _rapidFireActive(false)
    , _trueshotAuraActive(false)
    , _improvedSteadyShotActive(false)
    , _piercingShotsActive(false)
    , _markedForDeathActive(false)
    , _globalCooldownEnd(0)
    , _aimedShotCastStart(0)
    , _aimedShotCastEnd(0)
    , _steadyShotCastTime(1500)
    , _optimalShotInterval(1500)
    , _lastPositionUpdate(0)
    , _needsRepositioning(false)
    , _currentRange(0.0f)
    , _targetRange(OPTIMAL_RANGE)
{
    TC_LOG_DEBUG("playerbot", "MarksmanshipSpecialization: Initializing for bot {}", bot->GetName());

    // Initialize Marksmanship specific state
    _mmRotationPhase = MMRotationPhase::OPENING;
    _currentStance = CombatStance::AGGRESSIVE;

    // Reset all cooldowns
    _rapidFireReady = 0;
    _readinessReady = 0;
    _silencingShotReady = 0;

    // Initialize shot tracking
    _totalShotsFired = 0;
    _averageShotDamage = 0.0f;
    _manaEfficiency = 100;

    // Set initial optimal aspect
    _currentAspect = ASPECT_OF_THE_HAWK;

    // Initialize auto-shot timing
    _autoShotDuration = 2000; // Default 2 seconds
    _steadyShotCastTime = 1500; // 1.5 seconds

    TC_LOG_DEBUG("playerbot", "MarksmanshipSpecialization: Initialization complete for bot {}", bot->GetName());
}

void MarksmanshipSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    uint32 now = getMSTime();

    // Throttle rotation updates
    if (now - _lastRangeCheck < ROTATION_UPDATE_INTERVAL)
        return;
    _lastRangeCheck = now;

    TC_LOG_DEBUG("playerbot", "MarksmanshipSpecialization: UpdateRotation for bot {} targeting {}",
                 bot->GetName(), target->GetName());

    // Update management systems
    UpdateAutoShotTiming();
    UpdateManaManagement();
    UpdateCombatStance();
    UpdateShotWeaving();
    UpdateMultiTargetAnalysis();

    // Handle emergency situations
    if (bot->GetHealthPct() < 30.0f)
    {
        if (ExecuteManaConservation(target))
            return;
    }

    // Execute rotation based on phase
    switch (_mmRotationPhase)
    {
        case MMRotationPhase::OPENING:
            if (ExecuteOpeningRotation(target))
                return;
            break;

        case MMRotationPhase::AIMED_SHOT_CYCLE:
            if (ExecuteAimedShotCycle(target))
                return;
            break;

        case MMRotationPhase::STEADY_SHOT_SPAM:
            if (ExecuteSteadyShotSpam(target))
                return;
            break;

        case MMRotationPhase::EXECUTE_PHASE:
            if (ExecuteExecutePhase(target))
                return;
            break;

        case MMRotationPhase::AOE_PHASE:
            if (ExecuteAoePhase(target))
                return;
            break;

        case MMRotationPhase::BURST_PHASE:
            if (ExecuteBurstPhase(target))
                return;
            break;

        case MMRotationPhase::MANA_CONSERVATION:
            if (ExecuteManaConservation(target))
                return;
            break;

        default:
            if (ExecuteSteadyShotSpam(target))
                return;
            break;
    }

    // Handle dead zone
    if (IsInDeadZone(target))
    {
        HandleDeadZone(target);
        return;
    }

    // Auto-shot fallback
    OptimizeAutoShotWeaving(target);
}

bool MarksmanshipSpecialization::ExecuteOpeningRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    // Apply Hunter's Mark
    if (!target->HasAura(HUNTERS_MARK) && HasEnoughResource(HUNTERS_MARK))
    {
        bot->CastSpell(target, HUNTERS_MARK, false);
        ConsumeResource(HUNTERS_MARK);
        return true;
    }

    // Start with Aimed Shot if available
    if (ShouldUseAimedShot(target))
    {
        CastAimedShot(target);
        _mmRotationPhase = MMRotationPhase::AIMED_SHOT_CYCLE;
        return true;
    }

    // Transition to steady shot phase
    _mmRotationPhase = MMRotationPhase::STEADY_SHOT_SPAM;
    return ExecuteSteadyShotSpam(target);
}

bool MarksmanshipSpecialization::ExecuteSteadyShotSpam(::Unit* target)
{
    if (!target)
        return false;

    // Use Kill Shot in execute range
    if (ShouldUseKillShot(target))
    {
        CastKillShot(target);
        return true;
    }

    // Use Chimera Shot if available
    if (ShouldUseChimeraShot(target))
    {
        CastChimeraShot(target);
        return true;
    }

    // Use Aimed Shot if we have time
    if (ShouldUseAimedShot(target) && !ShouldClipAutoShot())
    {
        CastAimedShot(target);
        return true;
    }

    // Steady Shot for consistent DPS
    if (ShouldUseSteadyShot(target))
    {
        CastSteadyShot(target);
        return true;
    }

    // Arcane Shot as instant filler
    if (ShouldUseArcaneShot(target))
    {
        CastArcaneShot(target);
        return true;
    }

    return false;
}

bool MarksmanshipSpecialization::ExecuteExecutePhase(::Unit* target)
{
    if (!target)
        return false;

    // Spam Kill Shot
    if (ShouldUseKillShot(target))
    {
        CastKillShot(target);
        return true;
    }

    // Use other high-damage shots
    if (ShouldUseAimedShot(target))
    {
        CastAimedShot(target);
        return true;
    }

    return ExecuteSteadyShotSpam(target);
}

// Shot casting methods
void MarksmanshipSpecialization::CastAimedShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(AIMED_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "MarksmanshipSpecialization: Casting Aimed Shot for bot {}", bot->GetName());

    bot->CastSpell(target, AIMED_SHOT, false);
    ConsumeResource(AIMED_SHOT);
    _aimedShotsHit++;
    _totalShotsFired++;
    _castingAimedShot = true;
    _aimedShotCastStart = getMSTime();
    _aimedShotCastEnd = _aimedShotCastStart + 2500; // 2.5 second cast
}

void MarksmanshipSpecialization::CastSteadyShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(STEADY_SHOT))
        return;

    bot->CastSpell(target, STEADY_SHOT, false);
    ConsumeResource(STEADY_SHOT);
    _steadyShotsHit++;
    _totalShotsFired++;
}

void MarksmanshipSpecialization::CastKillShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(KILL_SHOT))
        return;

    bot->CastSpell(target, KILL_SHOT, false);
    ConsumeResource(KILL_SHOT);
    _killShots++;
    _totalShotsFired++;
}

// Resource and condition checks
bool MarksmanshipSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    uint32 manaCost = 0;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (spellInfo)
        manaCost = spellInfo->ManaCost;

    switch (spellId)
    {
        case AIMED_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 410);
        case STEADY_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 110);
        case KILL_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 150);
        case ARCANE_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 230);
        case MULTI_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 350);
        default:
            return bot->GetPower(POWER_MANA) >= manaCost;
    }
}

bool MarksmanshipSpecialization::ShouldUseAimedShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(AIMED_SHOT) &&
           IsCooldownReady(AIMED_SHOT) && !_castingAimedShot && GetBot()->GetPowerPct(POWER_MANA) > 40.0f;
}

bool MarksmanshipSpecialization::ShouldUseSteadyShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(STEADY_SHOT) &&
           IsCooldownReady(STEADY_SHOT);
}

bool MarksmanshipSpecialization::ShouldUseKillShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(KILL_SHOT) &&
           IsCooldownReady(KILL_SHOT) && target->GetHealthPct() < 20.0f;
}

// Interface implementations (simplified for space)
void MarksmanshipSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot) return;

    if (!HasCorrectAspect())
        SwitchToOptimalAspect();

    if (bot->HasSpell(TRUESHOT_AURA) && !bot->HasAura(TRUESHOT_AURA))
        bot->CastSpell(bot, TRUESHOT_AURA, false);
}

void MarksmanshipSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff) cooldown.second -= diff;
        else cooldown.second = 0;

    if (_rapidFireReady > diff) _rapidFireReady -= diff;
    else _rapidFireReady = 0;
}

bool MarksmanshipSpecialization::CanUseAbility(uint32 spellId)
{
    return IsCooldownReady(spellId) && HasEnoughResource(spellId);
}

void MarksmanshipSpecialization::OnCombatStart(::Unit* target)
{
    _mmRotationPhase = MMRotationPhase::OPENING;
    _totalShotsFired = 0;
    _currentStance = CombatStance::AGGRESSIVE;
}

void MarksmanshipSpecialization::OnCombatEnd()
{
    _mmRotationPhase = MMRotationPhase::OPENING;
    _castingAimedShot = false;
    _inBurstMode = false;
    _conservingMana = false;
}

void MarksmanshipSpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot) return;

    uint32 manaCost = 0;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (spellInfo) manaCost = spellInfo->ManaCost;

    if (manaCost > 0)
    {
        bot->ModifyPower(POWER_MANA, -int32(manaCost));
        _manaConsumed += manaCost;
    }

    UpdateCooldown(spellId, GetSpellCooldown(spellId));
}

Position MarksmanshipSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return Position();

    float distance = OPTIMAL_RANGE;
    float angle = target->GetAngle(bot) + M_PI / 4;

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(), angle
    );
}

float MarksmanshipSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_RANGE;
}

// Minimal pet management for Marksmanship
void MarksmanshipSpecialization::UpdatePetManagement() { UpdatePetInfo(); }
void MarksmanshipSpecialization::SummonPet() { /* Optional for Marksmanship */ }
void MarksmanshipSpecialization::CommandPetAttack(::Unit* target) { /* Minimal pet usage */ }
void MarksmanshipSpecialization::CommandPetFollow() { /* Minimal pet usage */ }
void MarksmanshipSpecialization::CommandPetStay() { /* Minimal pet usage */ }
void MarksmanshipSpecialization::MendPetIfNeeded() { /* Minimal pet usage */ }
void MarksmanshipSpecialization::FeedPetIfNeeded() { /* Minimal pet usage */ }
bool MarksmanshipSpecialization::HasActivePet() const { return _petInfo.guid != ObjectGuid::Empty; }
PetInfo MarksmanshipSpecialization::GetPetInfo() const { return _petInfo; }

// Basic trap management
void MarksmanshipSpecialization::UpdateTrapManagement() { /* Basic implementation */ }
void MarksmanshipSpecialization::PlaceTrap(uint32 trapSpell, Position position) { /* Basic implementation */ }
bool MarksmanshipSpecialization::ShouldPlaceTrap() const { return false; }
uint32 MarksmanshipSpecialization::GetOptimalTrapSpell() const { return FREEZING_TRAP; }
std::vector<TrapInfo> MarksmanshipSpecialization::GetActiveTraps() const { return _activeTraps; }

// Aspect management
void MarksmanshipSpecialization::UpdateAspectManagement()
{
    if (!HasCorrectAspect()) SwitchToOptimalAspect();
}

void MarksmanshipSpecialization::SwitchToOptimalAspect()
{
    Player* bot = GetBot();
    if (!bot) return;

    uint32 optimal = GetOptimalAspect();
    if (optimal != _currentAspect && bot->HasSpell(optimal))
    {
        bot->CastSpell(bot, optimal, false);
        _currentAspect = optimal;
    }
}

uint32 MarksmanshipSpecialization::GetOptimalAspect() const
{
    Player* bot = GetBot();
    if (!bot) return ASPECT_OF_THE_HAWK;

    if (bot->IsInCombat())
        return bot->HasSpell(ASPECT_OF_THE_DRAGONHAWK) ? ASPECT_OF_THE_DRAGONHAWK : ASPECT_OF_THE_HAWK;
    else
        return bot->HasSpell(ASPECT_OF_THE_PACK) ? ASPECT_OF_THE_PACK : ASPECT_OF_THE_CHEETAH;
}

bool MarksmanshipSpecialization::HasCorrectAspect() const
{
    return GetBot()->HasAura(GetOptimalAspect());
}

// Range management
void MarksmanshipSpecialization::UpdateRangeManagement() { /* Implemented in rotation */ }
bool MarksmanshipSpecialization::IsInDeadZone(::Unit* target) const
{
    float distance = GetDistanceToTarget(target);
    return distance > DEAD_ZONE_MIN && distance < DEAD_ZONE_MAX;
}

bool MarksmanshipSpecialization::ShouldKite(::Unit* target) const
{
    return GetBot()->GetHealthPct() < 50.0f || IsInDeadZone(target);
}

Position MarksmanshipSpecialization::GetKitePosition(::Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target) return Position();

    float angle = target->GetAngle(bot) + M_PI;
    float distance = OPTIMAL_RANGE;

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(), angle
    );
}

void MarksmanshipSpecialization::HandleDeadZone(::Unit* target)
{
    if (ShouldUseArcaneShot(target))
        CastArcaneShot(target);
}

// Tracking management
void MarksmanshipSpecialization::UpdateTracking() { /* Basic implementation */ }
uint32 MarksmanshipSpecialization::GetOptimalTracking() const { return TRACK_HUMANOIDS; }
void MarksmanshipSpecialization::ApplyTracking(uint32 trackingSpell)
{
    Player* bot = GetBot();
    if (bot && trackingSpell && bot->HasSpell(trackingSpell))
    {
        bot->CastSpell(bot, trackingSpell, false);
        _currentTracking = trackingSpell;
    }
}

// Helper method implementations
bool MarksmanshipSpecialization::ExecuteAimedShotCycle(::Unit* target) { return ExecuteSteadyShotSpam(target); }
bool MarksmanshipSpecialization::ExecuteAoePhase(::Unit* target) { return ExecuteSteadyShotSpam(target); }
bool MarksmanshipSpecialization::ExecuteBurstPhase(::Unit* target) { return ExecuteSteadyShotSpam(target); }
bool MarksmanshipSpecialization::ExecuteManaConservation(::Unit* target) { return ExecuteSteadyShotSpam(target); }

void MarksmanshipSpecialization::UpdateShotWeaving() { /* Basic implementation */ }
void MarksmanshipSpecialization::UpdateManaManagement() { /* Basic implementation */ }
void MarksmanshipSpecialization::UpdateCombatStance() { /* Basic implementation */ }
void MarksmanshipSpecialization::UpdateMultiTargetAnalysis() { /* Basic implementation */ }
void MarksmanshipSpecialization::UpdateAutoShotTiming() { /* Basic implementation */ }
bool MarksmanshipSpecialization::ShouldClipAutoShot() const { return false; }
void MarksmanshipSpecialization::OptimizeAutoShotWeaving(::Unit* target) { /* Basic implementation */ }

bool MarksmanshipSpecialization::ShouldUseChimeraShot(::Unit* target) const { return false; }
bool MarksmanshipSpecialization::ShouldUseArcaneShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(ARCANE_SHOT) && IsCooldownReady(ARCANE_SHOT);
}
bool MarksmanshipSpecialization::ShouldUseMultiShot(::Unit* target) const { return false; }

void MarksmanshipSpecialization::CastChimeraShot(::Unit* target) { /* Implementation */ }
void MarksmanshipSpecialization::CastArcaneShot(::Unit* target)
{
    Player* bot = GetBot();
    if (bot && target && HasEnoughResource(ARCANE_SHOT))
    {
        bot->CastSpell(target, ARCANE_SHOT, false);
        ConsumeResource(ARCANE_SHOT);
        _totalShotsFired++;
    }
}
void MarksmanshipSpecialization::CastMultiShot(::Unit* target) { /* Implementation */ }

} // namespace Playerbot