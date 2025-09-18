/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FrostSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

FrostSpecialization::FrostSpecialization(Player* bot)
    : DeathKnightSpecialization(bot)
    , _killingMachineActive(false)
    , _rimeActive(false)
    , _killingMachineExpires(0)
    , _rimeExpires(0)
    , _lastProcCheck(0)
    , _unbreakableWillReady(0)
    , _deathchillReady(0)
    , _empowerRuneWeaponReady(0)
    , _lastUnbreakableWill(0)
    , _lastDeathchill(0)
    , _lastEmpowerRuneWeapon(0)
    , _isDualWielding(false)
    , _preferDualWield(true)
    , _lastWeaponCheck(0)
    , _totalDamageDealt(0)
    , _procActivations(0)
    , _runicPowerSpent(0)
{
}

void FrostSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateRuneManagement();
    UpdateRunicPowerManagement();
    UpdateDiseaseManagement();
    UpdateProcManagement();
    UpdateWeaponStrategy();

    // Ensure we're in Frost Presence
    if (ShouldUseFrostPresence())
    {
        EnterFrostPresence();
    }

    // Use offensive cooldowns when appropriate
    if (bot->GetHealthPct() > 70.0f)
    {
        UseOffensiveCooldowns();
    }

    // Disease application priority
    if (ShouldApplyDisease(target, DiseaseType::FROST_FEVER))
    {
        CastIcyTouch(target);
        return;
    }

    // Proc consumption priority
    if (_killingMachineActive && ShouldCastObliterate(target))
    {
        CastObliterate(target);
        ConsumeKillingMachineProc();
        return;
    }

    if (_rimeActive && ShouldCastHowlingBlast(target))
    {
        CastHowlingBlast(target);
        ConsumeRimeProc();
        return;
    }

    // Runic Power dump
    if (GetRunicPower() >= 100 && ShouldCastFrostStrike(target))
    {
        CastFrostStrike(target);
        return;
    }

    // Regular rotation
    if (ShouldCastObliterate(target))
    {
        CastObliterate(target);
        return;
    }

    if (ShouldCastFrostStrike(target))
    {
        CastFrostStrike(target);
        return;
    }

    if (ShouldCastHowlingBlast(target))
    {
        CastHowlingBlast(target);
        return;
    }

    // Utility abilities
    if (bot->GetDistance(target) > MELEE_RANGE)
    {
        if (ShouldUseDeathGrip(target))
            CastDeathGrip(target);
        else
            CastDeathCoil(target);
    }
}

void FrostSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Frost Presence
    if (!bot->HasAura(FROST_PRESENCE) && bot->HasSpell(FROST_PRESENCE))
    {
        bot->CastSpell(bot, FROST_PRESENCE, false);
    }

    // Maintain Horn of Winter
    if (!bot->HasAura(HORN_OF_WINTER) && bot->HasSpell(HORN_OF_WINTER))
    {
        bot->CastSpell(bot, HORN_OF_WINTER, false);
    }
}

void FrostSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_unbreakableWillReady > diff)
        _unbreakableWillReady -= diff;
    else
        _unbreakableWillReady = 0;

    if (_deathchillReady > diff)
        _deathchillReady -= diff;
    else
        _deathchillReady = 0;

    if (_empowerRuneWeaponReady > diff)
        _empowerRuneWeaponReady -= diff;
    else
        _empowerRuneWeaponReady = 0;

    if (_killingMachineExpires > diff)
        _killingMachineExpires -= diff;
    else
    {
        _killingMachineExpires = 0;
        _killingMachineActive = false;
    }

    if (_rimeExpires > diff)
        _rimeExpires -= diff;
    else
    {
        _rimeExpires = 0;
        _rimeActive = false;
    }

    RegenerateRunes(diff);
    UpdateDiseaseTimers(diff);
}

bool FrostSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void FrostSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Enter Frost Presence
    if (ShouldUseFrostPresence())
        EnterFrostPresence();

    // Reset proc states
    _killingMachineActive = false;
    _rimeActive = false;
}

void FrostSpecialization::OnCombatEnd()
{
    _killingMachineActive = false;
    _rimeActive = false;
    _cooldowns.clear();
    _activeDiseases.clear();
}

bool FrostSpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case OBLITERATE:
            return HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::UNHOLY, 1);
        case FROST_STRIKE:
            return HasEnoughRunicPower(40);
        case HOWLING_BLAST:
            return HasAvailableRunes(RuneType::FROST, 1);
        case ICY_TOUCH:
            return HasAvailableRunes(RuneType::FROST, 1);
        case UNBREAKABLE_WILL:
            return _unbreakableWillReady == 0;
        case DEATHCHILL:
            return _deathchillReady == 0;
        case EMPOWER_RUNE_WEAPON:
            return _empowerRuneWeaponReady == 0;
        default:
            return true;
    }
}

void FrostSpecialization::ConsumeResource(uint32 spellId)
{
    switch (spellId)
    {
        case OBLITERATE:
            ConsumeRunes(RuneType::FROST, 1);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(15);
            break;
        case FROST_STRIKE:
            SpendRunicPower(40);
            break;
        case HOWLING_BLAST:
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);
            break;
        case ICY_TOUCH:
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);
            break;
        case UNBREAKABLE_WILL:
            _unbreakableWillReady = UNBREAKABLE_WILL_COOLDOWN;
            _lastUnbreakableWill = getMSTime();
            break;
        case DEATHCHILL:
            _deathchillReady = DEATHCHILL_COOLDOWN;
            _lastDeathchill = getMSTime();
            break;
        case EMPOWER_RUNE_WEAPON:
            _empowerRuneWeaponReady = EMPOWER_RUNE_WEAPON_COOLDOWN;
            _lastEmpowerRuneWeapon = getMSTime();
            // Reset all rune cooldowns
            for (auto& rune : _runes)
            {
                rune.available = true;
                rune.cooldownRemaining = 0;
            }
            break;
        default:
            break;
    }
}

Position FrostSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Melee DPS positioning
    float distance = MELEE_RANGE * 0.8f;
    float angle = target->GetAngle(bot) + M_PI / 4; // Side positioning

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float FrostSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

// Implement all pure virtual methods from base class with simplified logic
void FrostSpecialization::UpdateRuneManagement() { RegenerateRunes(0); }
bool FrostSpecialization::HasAvailableRunes(RuneType type, uint32 count) { return GetAvailableRunes(type) >= count; }
void FrostSpecialization::ConsumeRunes(RuneType type, uint32 count)
{
    uint32 consumed = 0;
    for (auto& rune : _runes)
    {
        if (rune.type == type && rune.IsReady() && consumed < count)
        {
            rune.Use();
            consumed++;
        }
    }
}
uint32 FrostSpecialization::GetAvailableRunes(RuneType type) const
{
    uint32 count = 0;
    for (const auto& rune : _runes)
    {
        if (rune.type == type && rune.IsReady())
            count++;
    }
    return count;
}

void FrostSpecialization::UpdateRunicPowerManagement()
{
    Player* bot = GetBot();
    if (!bot || bot->IsInCombat()) return;

    uint32 now = getMSTime();
    if (_lastRunicPowerDecay == 0) _lastRunicPowerDecay = now;

    uint32 timeDiff = now - _lastRunicPowerDecay;
    if (timeDiff >= 1000)
    {
        uint32 decay = (timeDiff / 1000) * RUNIC_POWER_DECAY_RATE;
        if (_runicPower > decay) _runicPower -= decay;
        else _runicPower = 0;
        _lastRunicPowerDecay = now;
    }
}

void FrostSpecialization::GenerateRunicPower(uint32 amount) { _runicPower = std::min(_runicPower + amount, _maxRunicPower); }
void FrostSpecialization::SpendRunicPower(uint32 amount) { if (_runicPower >= amount) { _runicPower -= amount; _runicPowerSpent += amount; } }
uint32 FrostSpecialization::GetRunicPower() const { return _runicPower; }
bool FrostSpecialization::HasEnoughRunicPower(uint32 required) const { return _runicPower >= required; }

void FrostSpecialization::UpdateDiseaseManagement() { UpdateDiseaseTimers(0); RefreshExpringDiseases(); }
void FrostSpecialization::ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId)
{
    if (!target) return;
    DiseaseInfo disease(type, spellId, 15000, 300);
    _activeDiseases[target->GetGUID()].push_back(disease);
}
bool FrostSpecialization::HasDisease(::Unit* target, DiseaseType type) const
{
    if (!target) return false;
    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
        if (disease.type == type && disease.IsActive()) return true;
    return false;
}
bool FrostSpecialization::ShouldApplyDisease(::Unit* target, DiseaseType type) const
{
    return target && (!HasDisease(target, type) || GetDiseaseRemainingTime(target, type) < DISEASE_REFRESH_THRESHOLD);
}
void FrostSpecialization::RefreshExpringDiseases()
{
    for (auto& targetDiseases : _activeDiseases)
    {
        for (auto& disease : targetDiseases.second)
        {
            if (disease.type == DiseaseType::FROST_FEVER && disease.NeedsRefresh())
            {
                Player* bot = GetBot();
                if (bot)
                {
                    ::Unit* target = ObjectAccessor::GetUnit(*bot, targetDiseases.first);
                    if (target && HasEnoughResource(ICY_TOUCH))
                        CastIcyTouch(target);
                }
            }
        }
    }
}

void FrostSpecialization::UpdateDeathAndDecay() { /* Handled in rotation */ }
bool FrostSpecialization::ShouldCastDeathAndDecay() const { return GetBot()->getAttackers().size() > 2; }
void FrostSpecialization::CastDeathAndDecay(Position targetPos)
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(DEATH_AND_DECAY))
        bot->CastSpell(bot, DEATH_AND_DECAY, false);
}

void FrostSpecialization::UpdateProcManagement()
{
    uint32 now = getMSTime();
    if (now - _lastProcCheck < PROC_CHECK_INTERVAL) return;
    _lastProcCheck = now;

    Player* bot = GetBot();
    if (!bot) return;

    // Check for Killing Machine proc
    if (bot->HasAura(KILLING_MACHINE) && !_killingMachineActive)
    {
        _killingMachineActive = true;
        _killingMachineExpires = now + KILLING_MACHINE_DURATION;
        _procActivations++;
    }

    // Check for Rime proc
    if (bot->HasAura(RIME) && !_rimeActive)
    {
        _rimeActive = true;
        _rimeExpires = now + RIME_DURATION;
        _procActivations++;
    }
}

bool FrostSpecialization::HasKillingMachineProc() { return _killingMachineActive; }
bool FrostSpecialization::HasRimeProc() { return _rimeActive; }
void FrostSpecialization::ConsumeKillingMachineProc() { _killingMachineActive = false; _killingMachineExpires = 0; }
void FrostSpecialization::ConsumeRimeProc() { _rimeActive = false; _rimeExpires = 0; }

bool FrostSpecialization::ShouldCastObliterate(::Unit* target) { return target && GetBot()->IsWithinMeleeRange(target) && HasEnoughResource(OBLITERATE); }
bool FrostSpecialization::ShouldCastFrostStrike(::Unit* target) { return target && GetBot()->IsWithinMeleeRange(target) && HasEnoughResource(FROST_STRIKE); }
bool FrostSpecialization::ShouldCastHowlingBlast(::Unit* target) { return target && HasEnoughResource(HOWLING_BLAST); }

void FrostSpecialization::CastObliterate(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(OBLITERATE))
    {
        bot->CastSpell(target, OBLITERATE, false);
        ConsumeResource(OBLITERATE);
        _totalDamageDealt += 4000;
    }
}

void FrostSpecialization::CastFrostStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(FROST_STRIKE))
    {
        bot->CastSpell(target, FROST_STRIKE, false);
        ConsumeResource(FROST_STRIKE);
        _totalDamageDealt += 3000;
    }
}

void FrostSpecialization::CastHowlingBlast(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(HOWLING_BLAST))
    {
        bot->CastSpell(target, HOWLING_BLAST, false);
        ConsumeResource(HOWLING_BLAST);
        _totalDamageDealt += 2500;
    }
}

void FrostSpecialization::CastIcyTouch(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(ICY_TOUCH))
    {
        bot->CastSpell(target, ICY_TOUCH, false);
        ConsumeResource(ICY_TOUCH);
        ApplyDisease(target, DiseaseType::FROST_FEVER, ICY_TOUCH);
        _totalDamageDealt += 1500;
    }
}

void FrostSpecialization::UseOffensiveCooldowns()
{
    if (_unbreakableWillReady == 0) CastUnbreakableWill();
    if (_deathchillReady == 0) CastDeathchill();
}

void FrostSpecialization::CastUnbreakableWill()
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(UNBREAKABLE_WILL) && HasEnoughResource(UNBREAKABLE_WILL))
    {
        bot->CastSpell(bot, UNBREAKABLE_WILL, false);
        ConsumeResource(UNBREAKABLE_WILL);
    }
}

void FrostSpecialization::CastDeathchill()
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(DEATHCHILL) && HasEnoughResource(DEATHCHILL))
    {
        bot->CastSpell(bot, DEATHCHILL, false);
        ConsumeResource(DEATHCHILL);
    }
}

void FrostSpecialization::EnterFrostPresence()
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(FROST_PRESENCE) && !bot->HasAura(FROST_PRESENCE))
        bot->CastSpell(bot, FROST_PRESENCE, false);
}

bool FrostSpecialization::ShouldUseFrostPresence()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(FROST_PRESENCE) && !bot->HasAura(FROST_PRESENCE);
}

void FrostSpecialization::UpdateWeaponStrategy() { /* Weapon strategy updates */ }

} // namespace Playerbot