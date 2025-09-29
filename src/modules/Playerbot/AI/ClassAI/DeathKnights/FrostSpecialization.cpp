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
    UpdateFrostRotation();

    // Ensure we're in Frost Presence
    if (ShouldUseFrostPresence())
    {
        EnterFrostPresence();
        return;
    }

    // Use offensive cooldowns when appropriate
    if (bot->GetHealthPct() > 70.0f && bot->IsInCombat())
    {
        UseOffensiveCooldowns();
    }

    // Disease application priority - Frost Fever for debuff
    if (ShouldApplyDisease(target, DiseaseType::FROST_FEVER))
    {
        CastIcyTouch(target);
        return;
    }

    // Emergency Empower Rune Weapon if all runes are on cooldown
    if (GetTotalAvailableRunes() == 0 && ShouldCastEmpowerRuneWeapon())
    {
        CastEmpowerRuneWeapon();
        return;
    }

    // Killing Machine proc consumption - highest priority
    if (_killingMachineActive && ShouldCastObliterate(target))
    {
        CastObliterate(target);
        ConsumeKillingMachineProc();
        return;
    }

    // Rime proc consumption - free Howling Blast
    if (_rimeActive && ShouldCastHowlingBlast(target))
    {
        CastHowlingBlast(target);
        ConsumeRimeProc();
        return;
    }

    // Runic Power management - dump at high amounts
    if (GetRunicPower() >= (GetRunicPowerThreshold() * _maxRunicPower) && ShouldCastFrostStrike(target))
    {
        CastFrostStrike(target);
        return;
    }

    // Core rotation based on weapon strategy
    if (ShouldUseDualWieldRotation())
    {
        UpdateDualWieldRotation(target);
    }
    else
    {
        UpdateTwoHandedRotation(target);
    }
}

void FrostSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Frost Presence for attack speed and runic power generation
    if (!bot->HasAura(FROST_PRESENCE) && bot->HasSpell(FROST_PRESENCE))
    {
        bot->CastSpell(bot, FROST_PRESENCE, false);
    }

    // Maintain Horn of Winter for stats
    if (!bot->HasAura(HORN_OF_WINTER) && bot->HasSpell(HORN_OF_WINTER))
    {
        bot->CastSpell(bot, HORN_OF_WINTER, false);
    }

    // Check for weapon enchants or temporary weapon buffs
    UpdateWeaponBuffs();
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

    if (_lastWeaponCheck > diff)
        _lastWeaponCheck -= diff;
    else
        _lastWeaponCheck = 0;

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

    // Enter Frost Presence for optimal DPS
    if (ShouldUseFrostPresence())
        EnterFrostPresence();

    // Reset proc states for new combat
    _killingMachineActive = false;
    _rimeActive = false;
    _killingMachineExpires = 0;
    _rimeExpires = 0;

    // Check weapon configuration
    UpdateWeaponStrategy();
}

void FrostSpecialization::OnCombatEnd()
{
    _killingMachineActive = false;
    _rimeActive = false;
    _killingMachineExpires = 0;
    _rimeExpires = 0;
    _cooldowns.clear();
    _activeDiseases.clear();
}

bool FrostSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

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
        case CHAINS_OF_ICE:
            return HasAvailableRunes(RuneType::FROST, 1);
        case MIND_FREEZE:
            return true; // No resource cost
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

            // Chance to trigger Killing Machine or Rime
            if (urand(1, 100) <= 25) // 25% chance
                TriggerKillingMachine();
            break;

        case FROST_STRIKE:
            SpendRunicPower(40);

            // Chance to trigger Killing Machine
            if (urand(1, 100) <= 15) // 15% chance
                TriggerKillingMachine();
            break;

        case HOWLING_BLAST:
            if (!_rimeActive) // Only consume rune if not procced
                ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);

            // Chance to trigger Rime
            if (urand(1, 100) <= 20) // 20% chance
                TriggerRime();
            break;

        case ICY_TOUCH:
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);
            break;

        case CHAINS_OF_ICE:
            ConsumeRunes(RuneType::FROST, 1);
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

            // Reset all rune cooldowns and give runic power
            for (auto& rune : _runes)
            {
                rune.available = true;
                rune.cooldownRemaining = 0;
            }
            GenerateRunicPower(25);
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

    // Melee DPS positioning - prefer flanking for dual-wield
    float distance = FROST_MELEE_RANGE * 0.8f;
    float angle = target->GetAngle(bot);

    if (_isDualWielding)
        angle += M_PI / 4; // Side positioning for dual-wield
    else
        angle += M_PI / 6; // Slight side positioning for two-handed

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float FrostSpecialization::GetOptimalRange(::Unit* target)
{
    return FROST_MELEE_RANGE;
}

void FrostSpecialization::UpdateRuneManagement()
{
    RegenerateRunes(0);

    // Check for death rune conversion opportunities
    if (CanConvertRune(RuneType::BLOOD, RuneType::DEATH))
    {
        ConvertRune(RuneType::BLOOD, RuneType::DEATH);
    }
}

bool FrostSpecialization::HasAvailableRunes(RuneType type, uint32 count)
{
    return GetAvailableRunes(type) >= count;
}

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
    if (!bot)
        return;

    // Decay runic power out of combat
    if (!bot->IsInCombat())
    {
        uint32 now = getMSTime();
        if (_lastRunicPowerDecay == 0)
            _lastRunicPowerDecay = now;

        uint32 timeDiff = now - _lastRunicPowerDecay;
        if (timeDiff >= 1000) // Decay every second
        {
            uint32 decay = (timeDiff / 1000) * RUNIC_POWER_DECAY_RATE;
            if (_runicPower > decay)
                _runicPower -= decay;
            else
                _runicPower = 0;
            _lastRunicPowerDecay = now;
        }
    }
}

void FrostSpecialization::GenerateRunicPower(uint32 amount)
{
    _runicPower = std::min(_runicPower + amount, _maxRunicPower);
}

void FrostSpecialization::SpendRunicPower(uint32 amount)
{
    if (_runicPower >= amount)
    {
        _runicPower -= amount;
        _runicPowerSpent += amount;
    }
}

uint32 FrostSpecialization::GetRunicPower() const
{
    return _runicPower;
}

bool FrostSpecialization::HasEnoughRunicPower(uint32 required) const
{
    return _runicPower >= required;
}

void FrostSpecialization::UpdateDiseaseManagement()
{
    UpdateDiseaseTimers(0);
    RefreshExpringDiseases();
}

void FrostSpecialization::ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId)
{
    if (!target)
        return;

    DiseaseInfo disease(type, spellId, 15000, 300); // 15 seconds, 300 damage per tick
    _activeDiseases[target->GetGUID()].push_back(disease);
}

bool FrostSpecialization::HasDisease(::Unit* target, DiseaseType type) const
{
    if (!target)
        return false;

    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
    {
        if (disease.type == type && disease.IsActive())
            return true;
    }

    return false;
}

bool FrostSpecialization::ShouldApplyDisease(::Unit* target, DiseaseType type) const
{
    if (!target)
        return false;

    return !HasDisease(target, type) || GetDiseaseRemainingTime(target, type) < DISEASE_REFRESH_THRESHOLD;
}

void FrostSpecialization::RefreshExpringDiseases()
{
    // Frost specialization focuses on Frost Fever
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
                    {
                        CastIcyTouch(target);
                    }
                }
            }
        }
    }
}

void FrostSpecialization::UpdateDeathAndDecay()
{
    // Death and Decay management is handled in main rotation
}

bool FrostSpecialization::ShouldCastDeathAndDecay() const
{
    return GetBot()->getAttackers().size() > 2 && _lastDeathAndDecay == 0;
}

void FrostSpecialization::CastDeathAndDecay(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(DEATH_AND_DECAY) && ShouldCastDeathAndDecay())
    {
        bot->CastSpell(bot, DEATH_AND_DECAY, false);
        _deathAndDecayPos = targetPos;
        _deathAndDecayRemaining = DEATH_AND_DECAY_DURATION;
        _lastDeathAndDecay = DEATH_AND_DECAY_COOLDOWN;
    }
}

void FrostSpecialization::UpdateFrostRotation()
{
    // Frost-specific rotation updates are handled in main UpdateRotation
}

void FrostSpecialization::UpdateKillingMachineProcs()
{
    // Killing Machine proc management is handled in UpdateProcManagement
}

void FrostSpecialization::UpdateRimeProcs()
{
    // Rime proc management is handled in UpdateProcManagement
}

bool FrostSpecialization::ShouldCastObliterate(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) &&
           HasEnoughResource(OBLITERATE);
}

bool FrostSpecialization::ShouldCastFrostStrike(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) &&
           HasEnoughResource(FROST_STRIKE);
}

bool FrostSpecialization::ShouldCastHowlingBlast(::Unit* target)
{
    return target && HasEnoughResource(HOWLING_BLAST) &&
           (GetBot()->getAttackers().size() > 1 || _rimeActive);
}

bool FrostSpecialization::ShouldCastUnbreakableWill()
{
    return _unbreakableWillReady == 0 && GetBot()->IsInCombat();
}

bool FrostSpecialization::ShouldCastDeathchill()
{
    return _deathchillReady == 0 && GetBot()->IsInCombat();
}

bool FrostSpecialization::ShouldCastEmpowerRuneWeapon()
{
    return _empowerRuneWeaponReady == 0 && GetTotalAvailableRunes() <= 1;
}

void FrostSpecialization::UpdateProcManagement()
{
    uint32 now = getMSTime();
    if (now - _lastProcCheck < PROC_CHECK_INTERVAL)
        return;

    _lastProcCheck = now;

    Player* bot = GetBot();
    if (!bot)
        return;

    // Check for Killing Machine proc
    if (bot->HasAura(KILLING_MACHINE) && !_killingMachineActive)
    {
        _killingMachineActive = true;
        _killingMachineExpires = now + KILLING_MACHINE_DURATION;
        _procActivations++;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Killing Machine proc activated for bot {}", bot->GetName());
    }

    // Check for Rime proc
    if (bot->HasAura(RIME) && !_rimeActive)
    {
        _rimeActive = true;
        _rimeExpires = now + RIME_DURATION;
        _procActivations++;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Rime proc activated for bot {}", bot->GetName());
    }
}

bool FrostSpecialization::HasKillingMachineProc()
{
    return _killingMachineActive && _killingMachineExpires > getMSTime();
}

bool FrostSpecialization::HasRimeProc()
{
    return _rimeActive && _rimeExpires > getMSTime();
}

void FrostSpecialization::ConsumeKillingMachineProc()
{
    _killingMachineActive = false;
    _killingMachineExpires = 0;

    TC_LOG_DEBUG("playerbot", "FrostSpecialization: Killing Machine proc consumed for bot {}", GetBot()->GetName());
}

void FrostSpecialization::ConsumeRimeProc()
{
    _rimeActive = false;
    _rimeExpires = 0;

    TC_LOG_DEBUG("playerbot", "FrostSpecialization: Rime proc consumed for bot {}", GetBot()->GetName());
}

void FrostSpecialization::TriggerKillingMachine()
{
    uint32 now = getMSTime();
    _killingMachineActive = true;
    _killingMachineExpires = now + KILLING_MACHINE_DURATION;
    _procActivations++;
}

void FrostSpecialization::TriggerRime()
{
    uint32 now = getMSTime();
    _rimeActive = true;
    _rimeExpires = now + RIME_DURATION;
    _procActivations++;
}

void FrostSpecialization::CastObliterate(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(OBLITERATE))
    {
        bot->CastSpell(target, OBLITERATE, false);
        ConsumeResource(OBLITERATE);

        // Obliterate does extra damage based on diseases
        uint32 baseDamage = 4000;
        auto diseases = GetActiveDiseases(target);
        uint32 bonusDamage = diseases.size() * 500; // 500 bonus per disease

        _totalDamageDealt += baseDamage + bonusDamage;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Obliterate cast on {} for {} damage",
                    target->GetName(), baseDamage + bonusDamage);
    }
}

void FrostSpecialization::CastFrostStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(FROST_STRIKE))
    {
        bot->CastSpell(target, FROST_STRIKE, false);
        ConsumeResource(FROST_STRIKE);
        _totalDamageDealt += 3000;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Frost Strike cast on {}", target->GetName());
    }
}

void FrostSpecialization::CastHowlingBlast(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(HOWLING_BLAST))
    {
        bot->CastSpell(target, HOWLING_BLAST, false);
        ConsumeResource(HOWLING_BLAST);

        // AoE damage calculation
        uint32 baseDamage = 2500;
        uint32 targets = std::min(static_cast<uint32>(bot->getAttackers().size()), 8u);
        _totalDamageDealt += baseDamage * targets;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Howling Blast cast hitting {} targets", targets);
    }
}

void FrostSpecialization::CastIcyTouch(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(ICY_TOUCH))
    {
        bot->CastSpell(target, ICY_TOUCH, false);
        ConsumeResource(ICY_TOUCH);
        ApplyDisease(target, DiseaseType::FROST_FEVER, ICY_TOUCH);
        _totalDamageDealt += 1500;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Icy Touch cast on {}, applying Frost Fever", target->GetName());
    }
}

void FrostSpecialization::CastChainsOfIce(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(CHAINS_OF_ICE) && bot->GetDistance(target) > FROST_MELEE_RANGE)
    {
        bot->CastSpell(target, CHAINS_OF_ICE, false);
        ConsumeResource(CHAINS_OF_ICE);

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Chains of Ice cast on {} for slowing", target->GetName());
    }
}

void FrostSpecialization::CastMindFreeze(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    // Use Mind Freeze to interrupt casting
    if (target->HasUnitState(UNIT_STATE_CASTING) && bot->HasSpell(MIND_FREEZE))
    {
        bot->CastSpell(target, MIND_FREEZE, false);

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Mind Freeze used to interrupt {}", target->GetName());
    }
}

void FrostSpecialization::CastUnbreakableWill()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(UNBREAKABLE_WILL))
    {
        bot->CastSpell(bot, UNBREAKABLE_WILL, false);
        ConsumeResource(UNBREAKABLE_WILL);

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Unbreakable Will activated");
    }
}

void FrostSpecialization::CastDeathchill()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(DEATHCHILL))
    {
        bot->CastSpell(bot, DEATHCHILL, false);
        ConsumeResource(DEATHCHILL);

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Deathchill activated");
    }
}

void FrostSpecialization::CastEmpowerRuneWeapon()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(EMPOWER_RUNE_WEAPON))
    {
        bot->CastSpell(bot, EMPOWER_RUNE_WEAPON, false);
        ConsumeResource(EMPOWER_RUNE_WEAPON);

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Empower Rune Weapon used - all runes refreshed");
    }
}

void FrostSpecialization::UseOffensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Use cooldowns when in good position to maximize damage
    if (bot->GetHealthPct() > 50.0f && bot->IsInCombat())
    {
        if (ShouldCastUnbreakableWill())
        {
            CastUnbreakableWill();
        }

        if (ShouldCastDeathchill())
        {
            CastDeathchill();
        }
    }
}

void FrostSpecialization::EnterFrostPresence()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(FROST_PRESENCE) && !bot->HasAura(FROST_PRESENCE))
    {
        bot->CastSpell(bot, FROST_PRESENCE, false);

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Entered Frost Presence");
    }
}

bool FrostSpecialization::ShouldUseFrostPresence()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(FROST_PRESENCE) && !bot->HasAura(FROST_PRESENCE);
}

void FrostSpecialization::UpdateWeaponStrategy()
{
    uint32 now = getMSTime();
    if (now - _lastWeaponCheck < WEAPON_CHECK_INTERVAL)
        return;

    _lastWeaponCheck = now;

    Player* bot = GetBot();
    if (!bot)
        return;

    // Check current weapon configuration
    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    bool currentlyDualWielding = (mainHand && offHand && offHand->GetTemplate()->GetInventoryType() == INVTYPE_WEAPON);

    if (currentlyDualWielding != _isDualWielding)
    {
        _isDualWielding = currentlyDualWielding;

        TC_LOG_DEBUG("playerbot", "FrostSpecialization: Weapon strategy updated - {} wielding",
                    _isDualWielding ? "dual" : "two-handed");
    }
}

bool FrostSpecialization::IsDualWielding()
{
    return _isDualWielding;
}

bool FrostSpecialization::ShouldUseDualWieldRotation()
{
    return _isDualWielding;
}

void FrostSpecialization::UpdateDualWieldRotation(::Unit* target)
{
    if (!target)
        return;

    // Dual-wield priority: Obliterate > Frost Strike > Howling Blast
    if (ShouldCastObliterate(target))
    {
        CastObliterate(target);
        return;
    }

    if (GetRunicPower() >= 60 && ShouldCastFrostStrike(target))
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
    HandleUtilitySpells(target);
}

void FrostSpecialization::UpdateTwoHandedRotation(::Unit* target)
{
    if (!target)
        return;

    // Two-handed priority: Obliterate > Howling Blast > Frost Strike
    if (ShouldCastObliterate(target))
    {
        CastObliterate(target);
        return;
    }

    if (ShouldCastHowlingBlast(target))
    {
        CastHowlingBlast(target);
        return;
    }

    if (GetRunicPower() >= 80 && ShouldCastFrostStrike(target))
    {
        CastFrostStrike(target);
        return;
    }

    // Utility abilities
    HandleUtilitySpells(target);
}

void FrostSpecialization::HandleUtilitySpells(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    // Interrupt casting enemies
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        CastMindFreeze(target);
        return;
    }

    // Slow fleeing enemies
    if (bot->GetDistance(target) > FROST_MELEE_RANGE * 1.5f)
    {
        if (ShouldUseDeathGrip(target))
        {
            CastDeathGrip(target);
        }
        else
        {
            CastChainsOfIce(target);
        }
        return;
    }

    // Ranged damage when out of melee
    if (bot->GetDistance(target) > FROST_MELEE_RANGE)
    {
        CastDeathCoil(target);
    }
}

void FrostSpecialization::UpdateWeaponBuffs()
{
    // Check for and maintain weapon enchants or temporary buffs
    Player* bot = GetBot();
    if (!bot)
        return;

    // This would check for temporary weapon enchants like weapon oils
    // Implementation depends on specific temporary enchant system
}

float FrostSpecialization::GetRunicPowerThreshold() const
{
    // Adjust runic power dump threshold based on weapon strategy
    return _isDualWielding ? 0.7f : RUNIC_POWER_THRESHOLD;
}

} // namespace Playerbot