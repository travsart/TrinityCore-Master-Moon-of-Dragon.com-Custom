/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShadowSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

ShadowSpecialization::ShadowSpecialization(Player* bot)
    : PriestSpecialization(bot)
    , _currentRole(PriestRole::DPS)
    , _inShadowForm(false)
    , _inVoidForm(false)
    , _voidFormStacks(0)
    , _currentInsanity(0)
    , _lastShadowformToggle(0)
    , _voidFormStartTime(0)
    , _lastTargetScan(0)
    , _lastDoTCheck(0)
    , _lastInsanityCheck(0)
    , _lastHealCheck(0)
    , _lastMultiTargetCheck(0)
    , _lastRotationUpdate(0)
    , _lastMindControl(0)
{
}

void ShadowSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot->IsAlive() || !target->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Update shadow form state
    UpdateShadowForm();
    UpdateVoidForm();
    UpdateInsanity();

    // Enter shadow form if not already in it
    if (ShouldEnterShadowForm() && !_inShadowForm)
    {
        EnterShadowForm();
        return;
    }

    // Void Form rotation
    if (_inVoidForm)
    {
        UpdateVoidFormRotation(target);
        return;
    }

    // Emergency healing (limited in shadow form)
    if (_currentRole == PriestRole::HYBRID)
    {
        UpdateHealing();
        if (ShouldHeal())
        {
            ::Unit* healTarget = GetBestHealTarget();
            if (healTarget && healTarget->GetHealthPct() < SHADOW_HEAL_THRESHOLD)
            {
                HealTarget(healTarget);
                return;
            }
        }
    }

    // Multi-target situations
    if (ShouldUseAoE())
    {
        HandleAoERotation();
        return;
    }

    // Enter Void Form when ready
    if (ShouldEnterVoidForm())
    {
        EnterVoidForm();
        return;
    }

    // DoT management
    UpdateDoTs();

    // Apply/refresh Shadow Word: Pain
    if (ShouldCastShadowWordPain(target))
    {
        CastShadowWordPain(target);
        return;
    }

    // Apply/refresh Vampiric Touch
    if (ShouldCastVampiricTouch(target))
    {
        CastVampiricTouch(target);
        return;
    }

    // Mind Blast for insanity generation
    if (ShouldCastMindBlast(target))
    {
        CastMindBlast(target);
        return;
    }

    // Mind Spike for quick damage
    if (CanUseAbility(MIND_SPIKE))
    {
        CastMindSpike(target);
        return;
    }

    // Mind Flay as filler
    if (ShouldCastMindFlay(target))
    {
        CastMindFlay(target);
        return;
    }

    // Shadow Word: Death for execute
    if (ShouldCastShadowWordDeath(target))
    {
        CastShadowWordDeath(target);
    }
}

void ShadowSpecialization::UpdateBuffs()
{
    // Shadow Form
    if (ShouldEnterShadowForm() && !_inShadowForm)
    {
        EnterShadowForm();
    }

    // Power Word: Fortitude (can cast outside shadow form)
    if (!_bot->HasAura(POWER_WORD_FORTITUDE))
    {
        // Temporarily exit shadow form to buff if needed
        bool wasInShadowForm = _inShadowForm;
        if (_inShadowForm)
            ExitShadowForm();

        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(POWER_WORD_FORTITUDE))
        {
            _bot->CastSpell(_bot, POWER_WORD_FORTITUDE, false);
        }

        if (wasInShadowForm)
            EnterShadowForm();
    }

    // Inner Fire
    if (!_bot->HasAura(INNER_FIRE))
    {
        bool wasInShadowForm = _inShadowForm;
        if (_inShadowForm)
            ExitShadowForm();

        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(INNER_FIRE))
        {
            _bot->CastSpell(_bot, INNER_FIRE, false);
        }

        if (wasInShadowForm)
            EnterShadowForm();
    }
}

void ShadowSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    // Update DoT timers
    for (auto& timer : _shadowWordPainTimers)
    {
        if (timer.second > diff)
            timer.second -= diff;
        else
            timer.second = 0;
    }

    for (auto& timer : _vampiricTouchTimers)
    {
        if (timer.second > diff)
            timer.second -= diff;
        else
            timer.second = 0;
    }

    UpdateShadowCooldowns();
}

bool ShadowSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    // Shadow form restrictions
    if (_inShadowForm)
    {
        // Can't cast non-shadow healing spells in shadow form
        if (spellId == HEAL || spellId == GREATER_HEAL || spellId == FLASH_HEAL)
            return false;
    }

    return true;
}

void ShadowSpecialization::OnCombatStart(::Unit* target)
{
    _currentInsanity = 0;
    _inVoidForm = false;
    _voidFormStacks = 0;
    _dotTargets.clear();
    _mindControlTargets.clear();
    _emergencyHealQueue = std::priority_queue<HealTarget>();

    // Enter shadow form at combat start
    if (ShouldEnterShadowForm())
    {
        EnterShadowForm();
    }
}

void ShadowSpecialization::OnCombatEnd()
{
    _currentInsanity = 0;
    _inVoidForm = false;
    _voidFormStacks = 0;
    _cooldowns.clear();
    _shadowWordPainTimers.clear();
    _vampiricTouchTimers.clear();
    _dotTargets.clear();
    _mindControlTargets.clear();
    _emergencyHealQueue = std::priority_queue<HealTarget>();
}

bool ShadowSpecialization::HasEnoughResource(uint32 spellId)
{
    // Check mana cost
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    uint32 manaCost = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    if (GetMana() < manaCost)
        return false;

    // Check insanity cost for void form spells
    if (spellId == VOID_BOLT && _inVoidForm)
    {
        return true; // Void Bolt generates insanity in void form
    }

    return true;
}

void ShadowSpecialization::ConsumeResource(uint32 spellId)
{
    // Mana is consumed automatically by spell casting

    // Generate insanity for certain spells
    switch (spellId)
    {
        case MIND_BLAST:
            GenerateInsanity(8);
            break;
        case SHADOW_WORD_PAIN:
            GenerateInsanity(4);
            break;
        case VAMPIRIC_TOUCH:
            GenerateInsanity(6);
            break;
        case MIND_FLAY:
            GenerateInsanity(2);
            break;
        case VOID_BOLT:
            if (_inVoidForm)
                GenerateInsanity(6);
            break;
    }
}

Position ShadowSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    float distance = GetOptimalRange(target);
    float angle = _bot->GetAngle(target);

    Position pos;
    target->GetNearPosition(pos, distance, angle + M_PI);
    return pos;
}

float ShadowSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_DPS_RANGE;
}

void ShadowSpecialization::UpdateHealing()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastHealCheck < 2000) // 2 second throttle
        return;
    _lastHealCheck = currentTime;

    // Clear old heal queue
    _emergencyHealQueue = std::priority_queue<HealTarget>();

    // Only emergency healing in shadow spec
    std::vector<::Unit*> groupMembers = GetGroupMembers();
    for (::Unit* member : groupMembers)
    {
        if (!member || !member->IsAlive())
            continue;

        float healthPercent = member->GetHealthPct();
        if (healthPercent < SHADOW_HEAL_THRESHOLD)
        {
            HealPriority priority = healthPercent < 15.0f ? HealPriority::EMERGENCY : HealPriority::CRITICAL;
            uint32 missingHealth = member->GetMaxHealth() - member->GetHealth();
            HealTarget healTarget(member, priority, healthPercent, missingHealth);
            _emergencyHealQueue.push(healTarget);
        }
    }
}

bool ShadowSpecialization::ShouldHeal()
{
    return !_emergencyHealQueue.empty() && _currentRole == PriestRole::HYBRID;
}

::Unit* ShadowSpecialization::GetBestHealTarget()
{
    if (_emergencyHealQueue.empty())
        return nullptr;

    HealTarget bestTarget = _emergencyHealQueue.top();
    return bestTarget.target;
}

void ShadowSpecialization::HealTarget(::Unit* target)
{
    if (!target)
        return;

    // Exit shadow form for healing
    bool wasInShadowForm = _inShadowForm;
    if (_inShadowForm)
        ExitShadowForm();

    // Emergency healing only
    if (target->GetHealthPct() < 20.0f)
    {
        if (CanUseAbility(FLASH_HEAL))
            CastFlashHeal(target);
        else if (CanUseAbility(HEAL))
            CastHeal(target);
    }

    // Re-enter shadow form
    if (wasInShadowForm)
        EnterShadowForm();
}

PriestRole ShadowSpecialization::GetCurrentRole()
{
    return _currentRole;
}

void ShadowSpecialization::SetRole(PriestRole role)
{
    _currentRole = role;
}

// Private methods

void ShadowSpecialization::UpdateShadowForm()
{
    _inShadowForm = _bot->HasAura(SHADOW_FORM);
}

void ShadowSpecialization::UpdateVoidForm()
{
    _inVoidForm = _bot->HasAura(VOIDFORM_BUFF);
    if (_inVoidForm)
    {
        ManageVoidFormStacks();
    }
    else
    {
        _voidFormStacks = 0;
        _voidFormStartTime = 0;
    }
}

void ShadowSpecialization::UpdateInsanity()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastInsanityCheck < 500) // 500ms throttle
        return;
    _lastInsanityCheck = currentTime;

    // Update current insanity (would read from player power in real implementation)
    // For now, simulate insanity tracking
}

void ShadowSpecialization::UpdateShadowWordPain()
{
    // Shadow Word: Pain ticks generate insanity
    // This would be handled by combat log events in real implementation
}

void ShadowSpecialization::UpdateVampiricTouch()
{
    // Vampiric Touch ticks generate insanity and heal
    // This would be handled by combat log events in real implementation
}

void ShadowSpecialization::UpdateMindFlay()
{
    // Mind Flay generates insanity per tick
    // This would be handled during channeling
}

bool ShadowSpecialization::ShouldEnterShadowForm()
{
    // Enter shadow form for DPS
    return _currentRole != PriestRole::HEALER && !_inShadowForm;
}

bool ShadowSpecialization::ShouldEnterVoidForm()
{
    return GetInsanityPercent() >= VOID_FORM_ENTRY_THRESHOLD &&
           !_inVoidForm &&
           CanUseAbility(VOID_FORM);
}

bool ShadowSpecialization::ShouldCastMindBlast(::Unit* target)
{
    return target && CanUseAbility(MIND_BLAST);
}

bool ShadowSpecialization::ShouldCastShadowWordPain(::Unit* target)
{
    if (!target || !CanUseAbility(SHADOW_WORD_PAIN))
        return false;

    return !HasShadowWordPain(target) ||
           GetShadowWordPainTimeRemaining(target) < DOT_REFRESH_THRESHOLD;
}

bool ShadowSpecialization::ShouldCastVampiricTouch(::Unit* target)
{
    if (!target || !CanUseAbility(VAMPIRIC_TOUCH))
        return false;

    return !HasVampiricTouch(target) ||
           GetVampiricTouchTimeRemaining(target) < DOT_REFRESH_THRESHOLD;
}

bool ShadowSpecialization::ShouldCastMindFlay(::Unit* target)
{
    return target &&
           CanUseAbility(MIND_FLAY) &&
           !_bot->isMoving(); // Can't channel while moving
}

bool ShadowSpecialization::ShouldCastShadowWordDeath(::Unit* target)
{
    return target &&
           target->GetHealthPct() < 25.0f && // Execute range
           CanUseAbility(SHADOW_WORD_DEATH);
}

void ShadowSpecialization::EnterShadowForm()
{
    if (!_inShadowForm && CanUseAbility(SHADOW_FORM))
    {
        _bot->CastSpell(_bot, SHADOW_FORM, false);
        _lastShadowformToggle = getMSTime();
    }
}

void ShadowSpecialization::ExitShadowForm()
{
    if (_inShadowForm)
    {
        _bot->RemoveAurasDueToSpell(SHADOW_FORM);
        _lastShadowformToggle = getMSTime();
    }
}

void ShadowSpecialization::EnterVoidForm()
{
    if (!_inVoidForm && CanUseAbility(VOID_FORM) && GetInsanity() >= VOID_FORM_INSANITY_COST)
    {
        _bot->CastSpell(_bot, VOID_FORM, false);
        _voidFormStartTime = getMSTime();
        ConsumeInsanity(VOID_FORM_INSANITY_COST);
    }
}

void ShadowSpecialization::ManageVoidFormStacks()
{
    _voidFormStacks = GetVoidFormStacks();
}

uint32 ShadowSpecialization::GetVoidFormStacks()
{
    if (!_inVoidForm)
        return 0;

    // Calculate stacks based on time in void form
    uint32 timeInVoidForm = getMSTime() - _voidFormStartTime;
    return timeInVoidForm / 1000; // 1 stack per second
}

void ShadowSpecialization::GenerateInsanity(uint32 amount)
{
    _currentInsanity = std::min(_currentInsanity + amount, MAX_INSANITY);
}

void ShadowSpecialization::ConsumeInsanity(uint32 amount)
{
    _currentInsanity = _currentInsanity > amount ? _currentInsanity - amount : 0;
}

uint32 ShadowSpecialization::GetInsanity()
{
    return _currentInsanity;
}

uint32 ShadowSpecialization::GetMaxInsanity()
{
    return MAX_INSANITY;
}

float ShadowSpecialization::GetInsanityPercent()
{
    return static_cast<float>(_currentInsanity) / MAX_INSANITY;
}

bool ShadowSpecialization::HasEnoughInsanity(uint32 amount)
{
    return _currentInsanity >= amount;
}

void ShadowSpecialization::UpdateDoTs()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastDoTCheck < 2000) // 2 second throttle
        return;
    _lastDoTCheck = currentTime;

    RefreshDoTs();
}

void ShadowSpecialization::CastShadowWordPain(::Unit* target)
{
    if (target && CanUseAbility(SHADOW_WORD_PAIN))
    {
        _bot->CastSpell(target, SHADOW_WORD_PAIN, false);
        _shadowWordPainTimers[target->GetGUID().GetCounter()] = getMSTime() + SHADOW_WORD_PAIN_DURATION;
        _dotTargets.insert(target->GetGUID().GetCounter());
    }
}

void ShadowSpecialization::CastVampiricTouch(::Unit* target)
{
    if (target && CanUseAbility(VAMPIRIC_TOUCH))
    {
        _bot->CastSpell(target, VAMPIRIC_TOUCH, false);
        _vampiricTouchTimers[target->GetGUID().GetCounter()] = getMSTime() + VAMPIRIC_TOUCH_DURATION;
        _dotTargets.insert(target->GetGUID().GetCounter());
    }
}

void ShadowSpecialization::RefreshDoTs()
{
    ::Unit* target = _bot->GetVictim();
    if (!target)
        return;

    // Refresh Shadow Word: Pain if needed
    if (ShouldCastShadowWordPain(target))
    {
        CastShadowWordPain(target);
    }

    // Refresh Vampiric Touch if needed
    if (ShouldCastVampiricTouch(target))
    {
        CastVampiricTouch(target);
    }
}

bool ShadowSpecialization::HasShadowWordPain(::Unit* target)
{
    if (!target)
        return false;

    auto it = _shadowWordPainTimers.find(target->GetGUID().GetCounter());
    return it != _shadowWordPainTimers.end() && it->second > getMSTime();
}

bool ShadowSpecialization::HasVampiricTouch(::Unit* target)
{
    if (!target)
        return false;

    auto it = _vampiricTouchTimers.find(target->GetGUID().GetCounter());
    return it != _vampiricTouchTimers.end() && it->second > getMSTime();
}

uint32 ShadowSpecialization::GetShadowWordPainTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _shadowWordPainTimers.find(target->GetGUID().GetCounter());
    if (it != _shadowWordPainTimers.end() && it->second > getMSTime())
    {
        return it->second - getMSTime();
    }
    return 0;
}

uint32 ShadowSpecialization::GetVampiricTouchTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _vampiricTouchTimers.find(target->GetGUID().GetCounter());
    if (it != _vampiricTouchTimers.end() && it->second > getMSTime())
    {
        return it->second - getMSTime();
    }
    return 0;
}

void ShadowSpecialization::CastMindBlast(::Unit* target)
{
    if (target && CanUseAbility(MIND_BLAST))
    {
        _bot->CastSpell(target, MIND_BLAST, false);
        _cooldowns[MIND_BLAST] = 8000; // 8 second cooldown
    }
}

void ShadowSpecialization::CastMindFlay(::Unit* target)
{
    if (target && CanUseAbility(MIND_FLAY))
    {
        _bot->CastSpell(target, MIND_FLAY, false);
    }
}

void ShadowSpecialization::CastShadowWordDeath(::Unit* target)
{
    if (target && CanUseAbility(SHADOW_WORD_DEATH))
    {
        _bot->CastSpell(target, SHADOW_WORD_DEATH, false);
        _cooldowns[SHADOW_WORD_DEATH] = 12000; // 12 second cooldown
    }
}

void ShadowSpecialization::CastMindSpike(::Unit* target)
{
    if (target && CanUseAbility(MIND_SPIKE))
    {
        _bot->CastSpell(target, MIND_SPIKE, false);
    }
}

void ShadowSpecialization::CastPsychicScream()
{
    if (CanUseAbility(PSYCHIC_SCREAM))
    {
        _bot->CastSpell(_bot, PSYCHIC_SCREAM, false);
        _cooldowns[PSYCHIC_SCREAM] = 27000; // 27 second cooldown
    }
}

void ShadowSpecialization::CastVoidBolt(::Unit* target)
{
    if (target && _inVoidForm && CanUseAbility(VOID_BOLT))
    {
        _bot->CastSpell(target, VOID_BOLT, false);
    }
}

void ShadowSpecialization::UpdateMultiTarget()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastMultiTargetCheck < 1000) // 1 second throttle
        return;
    _lastMultiTargetCheck = currentTime;

    // Update nearby enemy list
    // Implementation would scan for nearby enemies
}

void ShadowSpecialization::HandleAoERotation()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies();

    if (enemies.size() >= MULTI_TARGET_THRESHOLD)
    {
        // Mind Sear for AoE
        if (CanUseAbility(MIND_SEAR))
        {
            CastMindSear();
            return;
        }

        // Apply Shadow Word: Pain to multiple targets
        CastShadowWordPainAoE();
    }
}

std::vector<::Unit*> ShadowSpecialization::GetNearbyEnemies(float range)
{
    std::vector<::Unit*> enemies;
    // Implementation depends on TrinityCore's enemy detection system
    return enemies;
}

bool ShadowSpecialization::ShouldUseAoE()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies();
    return enemies.size() >= MULTI_TARGET_THRESHOLD;
}

void ShadowSpecialization::CastMindSear()
{
    if (CanUseAbility(MIND_SEAR))
    {
        _bot->CastSpell(_bot->GetVictim(), MIND_SEAR, false);
    }
}

void ShadowSpecialization::CastShadowWordPainAoE()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies();
    for (::Unit* enemy : enemies)
    {
        if (!HasShadowWordPain(enemy) && CanUseAbility(SHADOW_WORD_PAIN))
        {
            CastShadowWordPain(enemy);
            break; // One cast per update
        }
    }
}

void ShadowSpecialization::UpdateShadowCooldowns()
{
    // Update void form duration
    if (_inVoidForm && _voidFormStartTime > 0)
    {
        uint32 duration = VOID_FORM_BASE_DURATION - (_voidFormStacks * 1000); // Drain faster with stacks
        if (getMSTime() - _voidFormStartTime >= duration)
        {
            _inVoidForm = false;
            _voidFormStacks = 0;
            _voidFormStartTime = 0;
        }
    }
}

void ShadowSpecialization::UseShadowCooldowns()
{
    // Shadowfiend for mana regeneration
    if (GetManaPercent() < 50.0f && CanUseAbility(SHADOWFIEND))
    {
        CastShadowfiend();
    }

    // Dispersion for emergency mana
    if (GetManaPercent() < 20.0f && CanUseAbility(DISPERSION))
    {
        CastDispersion();
    }
}

void ShadowSpecialization::CastShadowfiend()
{
    if (CanUseAbility(SHADOWFIEND))
    {
        _bot->CastSpell(_bot, SHADOWFIEND, false);
        _cooldowns[SHADOWFIEND] = 300000; // 5 minute cooldown
    }
}

void ShadowSpecialization::CastMindControl(::Unit* target)
{
    if (target && CanUseAbility(MIND_CONTROL))
    {
        _bot->CastSpell(target, MIND_CONTROL, false);
        _mindControlTargets.insert(target->GetGUID().GetCounter());
        _lastMindControl = getMSTime();
        _cooldowns[MIND_CONTROL] = 8000; // 8 second cooldown
    }
}

void ShadowSpecialization::CastDispersion()
{
    if (CanUseAbility(DISPERSION))
    {
        _bot->CastSpell(_bot, DISPERSION, false);
        _cooldowns[DISPERSION] = 120000; // 2 minute cooldown
    }
}

void ShadowSpecialization::CastVampiricEmbrace(::Unit* target)
{
    if (target && CanUseAbility(VAMPIRIC_EMBRACE))
    {
        _bot->CastSpell(target, VAMPIRIC_EMBRACE, false);
    }
}

void ShadowSpecialization::UpdateVoidFormRotation(::Unit* target)
{
    if (!target)
        return;

    // Void Bolt priority in void form
    if (CanUseAbility(VOID_BOLT))
    {
        CastVoidBolt(target);
        return;
    }

    // Mind Blast for insanity
    if (ShouldCastMindBlast(target))
    {
        CastMindBlast(target);
        return;
    }

    // Maintain DoTs
    if (ShouldCastShadowWordPain(target))
    {
        CastShadowWordPain(target);
        return;
    }

    if (ShouldCastVampiricTouch(target))
    {
        CastVampiricTouch(target);
        return;
    }

    // Mind Flay as filler
    if (ShouldCastMindFlay(target))
    {
        CastMindFlay(target);
    }
}

void ShadowSpecialization::BuildInsanityForVoidForm()
{
    ::Unit* target = _bot->GetVictim();
    if (!target)
        return;

    // Prioritize insanity-generating spells
    if (ShouldCastMindBlast(target))
    {
        CastMindBlast(target);
    }
    else if (ShouldCastShadowWordPain(target))
    {
        CastShadowWordPain(target);
    }
    else if (ShouldCastVampiricTouch(target))
    {
        CastVampiricTouch(target);
    }
}

void ShadowSpecialization::OptimizeVoidFormUptime()
{
    if (_inVoidForm)
    {
        // Focus on void bolt and insanity generation
        ManageVoidFormStacks();
    }
    else if (GetInsanityPercent() > 0.8f)
    {
        // Prepare for void form entry
        BuildInsanityForVoidForm();
    }
}

bool ShadowSpecialization::IsInVoidForm()
{
    return _inVoidForm;
}

uint32 ShadowSpecialization::GetVoidFormTimeRemaining()
{
    if (!_inVoidForm || _voidFormStartTime == 0)
        return 0;

    uint32 duration = VOID_FORM_BASE_DURATION - (_voidFormStacks * 1000);
    uint32 elapsed = getMSTime() - _voidFormStartTime;
    return duration > elapsed ? duration - elapsed : 0;
}

} // namespace Playerbot