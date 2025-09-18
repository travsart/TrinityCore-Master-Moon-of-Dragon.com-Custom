/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RogueSpecialization.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"

namespace Playerbot
{

RogueSpecialization::RogueSpecialization(Player* bot)
    : _bot(bot), _combatPhase(CombatPhase::STEALTH_OPENER), _currentTarget(nullptr),
      _lastUpdateTime(0), _combatStartTime(0), _lastEnergyCheck(0), _lastComboCheck(0), _lastStealthCheck(0),
      _totalDamageDealt(0), _totalEnergySpent(0), _totalCombosBuilt(0), _totalCombosSpent(0),
      _burstPhaseCount(0), _averageCombatTime(0.0f)
{
    InitializeCooldowns();
    UpdateResourceStates();
}

void RogueSpecialization::InitializeCooldowns()
{
    // Core abilities cooldowns
    _cooldowns[VANISH] = CooldownInfo(VANISH, 300000);           // 5 minutes
    _cooldowns[PREPARATION] = CooldownInfo(PREPARATION, 180000); // 3 minutes
    _cooldowns[ADRENALINE_RUSH] = CooldownInfo(ADRENALINE_RUSH, 300000); // 5 minutes
    _cooldowns[BLADE_FLURRY] = CooldownInfo(BLADE_FLURRY, 120000); // 2 minutes
    _cooldowns[COLD_BLOOD] = CooldownInfo(COLD_BLOOD, 60000);    // 1 minute
    _cooldowns[EVASION] = CooldownInfo(EVASION, 300000);         // 5 minutes
    _cooldowns[SPRINT] = CooldownInfo(SPRINT, 300000);           // 5 minutes
    _cooldowns[KICK] = CooldownInfo(KICK, 10000);                // 10 seconds
    _cooldowns[GOUGE] = CooldownInfo(GOUGE, 10000);              // 10 seconds
    _cooldowns[KIDNEY_SHOT] = CooldownInfo(KIDNEY_SHOT, 20000);  // 20 seconds
    _cooldowns[BLIND] = CooldownInfo(BLIND, 300000);             // 5 minutes
    _cooldowns[SAP] = CooldownInfo(SAP, 0);                      // No cooldown
    _cooldowns[CHEAP_SHOT] = CooldownInfo(CHEAP_SHOT, 0);        // No cooldown
    _cooldowns[SHADOWSTEP] = CooldownInfo(SHADOWSTEP, 30000);    // 30 seconds
    _cooldowns[CLOAK_OF_SHADOWS] = CooldownInfo(CLOAK_OF_SHADOWS, 60000); // 1 minute
    _cooldowns[DISMANTLE] = CooldownInfo(DISMANTLE, 60000);      // 1 minute
    _cooldowns[TRICKS_OF_THE_TRADE] = CooldownInfo(TRICKS_OF_THE_TRADE, 30000); // 30 seconds
    _cooldowns[SHADOW_DANCE] = CooldownInfo(SHADOW_DANCE, 60000); // 1 minute

    TC_LOG_DEBUG("playerbot", "RogueSpecialization: Initialized {} cooldowns for bot {}", _cooldowns.size(), _bot->GetName());
}

void RogueSpecialization::UpdateResourceStates()
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();

    // Update Energy State
    _energy.current = _bot->GetPower(POWER_ENERGY);
    _energy.maximum = _bot->GetMaxPower(POWER_ENERGY);

    if (_energy.current >= 80)
        _energy.state = EnergyState::FULL;
    else if (_energy.current >= 60)
        _energy.state = EnergyState::HIGH;
    else if (_energy.current >= 40)
        _energy.state = EnergyState::MEDIUM;
    else if (_energy.current >= 20)
        _energy.state = EnergyState::LOW;
    else
        _energy.state = EnergyState::CRITICAL;

    // Update Combo Points
    _comboPoints.current = GetComboPoints();
    _comboPoints.shouldSpend = (_comboPoints.current >= 4) ||
                               (_comboPoints.current >= 3 && _energy.state >= EnergyState::HIGH);

    // Update Stealth State
    if (IsStealthed())
    {
        if (HasAura(STEALTH))
            _stealth.state = StealthState::STEALTH;
        else if (HasAura(VANISH_EFFECT))
            _stealth.state = StealthState::VANISH;
        else if (HasAura(SHADOWSTEP_EFFECT))
            _stealth.state = StealthState::SHADOWSTEP;
        else if (HasAura(SHADOW_DANCE))
            _stealth.state = StealthState::SHADOW_DANCE;

        _stealth.canOpenFromStealth = true;
        _stealth.hasAdvantage = true;
    }
    else
    {
        _stealth.state = StealthState::NONE;
        _stealth.canOpenFromStealth = false;
        _stealth.hasAdvantage = false;
    }

    // Update Poison Information
    if (HasWeaponInMainHand())
    {
        if (HasAura(INSTANT_POISON_9))
            _poisons.mainHandPoison = PoisonType::INSTANT;
        else if (HasAura(DEADLY_POISON_9))
            _poisons.mainHandPoison = PoisonType::DEADLY;
        else if (HasAura(WOUND_POISON_5))
            _poisons.mainHandPoison = PoisonType::WOUND;
        else
            _poisons.mainHandPoison = PoisonType::NONE;
    }

    if (HasWeaponInOffHand())
    {
        if (HasAura(INSTANT_POISON_9))
            _poisons.offHandPoison = PoisonType::INSTANT;
        else if (HasAura(DEADLY_POISON_9))
            _poisons.offHandPoison = PoisonType::DEADLY;
        else if (HasAura(CRIPPLING_POISON_2))
            _poisons.offHandPoison = PoisonType::CRIPPLING;
        else
            _poisons.offHandPoison = PoisonType::NONE;
    }

    _lastUpdateTime = currentTime;
}

void RogueSpecialization::UpdateTargetInfo(::Unit* target)
{
    if (!target)
    {
        _targetDebuffs = TargetDebuffInfo();
        return;
    }

    _currentTarget = target;

    // Check debuffs on target
    _targetDebuffs.hasSliceAndDice = HasAura(SLICE_AND_DICE, _bot);
    _targetDebuffs.hasRupture = HasAura(RUPTURE, target);
    _targetDebuffs.hasGarrote = HasAura(GARROTE, target);
    _targetDebuffs.hasExposeArmor = HasAura(EXPOSE_ARMOR, target);

    // Get remaining times
    _targetDebuffs.sliceAndDiceRemaining = GetAuraTimeRemaining(SLICE_AND_DICE, _bot);
    _targetDebuffs.ruptureRemaining = GetAuraTimeRemaining(RUPTURE, target);
    _targetDebuffs.garroteRemaining = GetAuraTimeRemaining(GARROTE, target);
    _targetDebuffs.exposeArmorRemaining = GetAuraTimeRemaining(EXPOSE_ARMOR, target);

    // Check poison stacks
    _targetDebuffs.poisonStacks = 0;
    if (HasAura(DEADLY_POISON_9, target))
        _targetDebuffs.poisonStacks = 5; // Assume max stacks for simplicity

    _targetDebuffs.hasPoison = _targetDebuffs.poisonStacks > 0;
}

void RogueSpecialization::LogRotationDecision(const std::string& decision, const std::string& reason)
{
    TC_LOG_DEBUG("playerbot", "RogueAI [{}]: {} - Reason: {} [Energy: {}/{}, CP: {}, Phase: {}]",
                _bot->GetName(), decision, reason, _energy.current, _energy.maximum,
                _comboPoints.current, static_cast<uint8>(_combatPhase));
}

bool RogueSpecialization::IsInMeleeRange(::Unit* target)
{
    if (!target || !_bot)
        return false;

    float distance = _bot->GetDistance(target);
    return distance <= 5.0f; // Standard melee range
}

bool RogueSpecialization::IsBehindTarget(::Unit* target)
{
    if (!target || !_bot)
        return false;

    return _bot->isInBack(target);
}

bool RogueSpecialization::HasWeaponInMainHand()
{
    if (!_bot)
        return false;

    Item* mainhand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    return mainhand != nullptr;
}

bool RogueSpecialization::HasWeaponInOffHand()
{
    if (!_bot)
        return false;

    Item* offhand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    return offhand != nullptr;
}

uint8 RogueSpecialization::GetComboPoints()
{
    if (!_bot || !_currentTarget)
        return 0;

    return _bot->GetComboPoints();
}

uint32 RogueSpecialization::GetCurrentEnergy()
{
    if (!_bot)
        return 0;

    return _bot->GetPower(POWER_ENERGY);
}

bool RogueSpecialization::IsStealthed()
{
    if (!_bot)
        return false;

    return _bot->HasAuraType(SPELL_AURA_MOD_STEALTH) ||
           _bot->HasAuraType(SPELL_AURA_MOD_INVISIBILITY);
}

bool RogueSpecialization::HasAura(uint32 spellId, ::Unit* unit)
{
    if (!unit)
        unit = _bot;

    if (!unit)
        return false;

    return unit->HasAura(spellId);
}

uint32 RogueSpecialization::GetAuraTimeRemaining(uint32 spellId, ::Unit* unit)
{
    if (!unit)
        unit = _bot;

    if (!unit)
        return 0;

    if (Aura* aura = unit->GetAura(spellId))
        return aura->GetDuration();

    return 0;
}

bool RogueSpecialization::CastSpell(uint32 spellId, ::Unit* target)
{
    if (!_bot)
        return false;

    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
    {
        TC_LOG_ERROR("playerbot", "RogueSpecialization::CastSpell: Invalid spell ID {} for bot {}", spellId, _bot->GetName());
        return false;
    }

    if (!HasSpell(spellId))
    {
        TC_LOG_DEBUG("playerbot", "RogueSpecialization::CastSpell: Bot {} doesn't have spell {}", _bot->GetName(), spellId);
        return false;
    }

    if (!HasEnoughEnergyFor(spellId))
    {
        TC_LOG_DEBUG("playerbot", "RogueSpecialization::CastSpell: Bot {} doesn't have enough energy for spell {}", _bot->GetName(), spellId);
        return false;
    }

    if (!IsSpellReady(spellId))
    {
        TC_LOG_DEBUG("playerbot", "RogueSpecialization::CastSpell: Spell {} not ready for bot {}", spellId, _bot->GetName());
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

    // Start cooldown
    StartCooldown(spellId);

    // Update energy
    uint32 energyCost = GetEnergyCost(spellId);
    if (energyCost > 0)
    {
        _bot->ModifyPower(POWER_ENERGY, -static_cast<int32>(energyCost));
        _totalEnergySpent += energyCost;
    }

    TC_LOG_DEBUG("playerbot", "RogueSpecialization::CastSpell: Bot {} cast spell {} on target {}",
                _bot->GetName(), spellId, target ? target->GetName() : "self");

    return true;
}

bool RogueSpecialization::HasSpell(uint32 spellId)
{
    if (!_bot)
        return false;

    return _bot->HasSpell(spellId);
}

SpellInfo const* RogueSpecialization::GetSpellInfo(uint32 spellId)
{
    return sSpellMgr->GetSpellInfo(spellId);
}

uint32 RogueSpecialization::GetSpellCooldown(uint32 spellId)
{
    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    return spellInfo->RecoveryTime;
}

void RogueSpecialization::UpdateCooldownTracking(uint32 diff)
{
    auto currentTime = std::chrono::steady_clock::now();

    for (auto& [spellId, cooldown] : _cooldowns)
    {
        if (!cooldown.isReady)
        {
            auto timeSinceUse = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - cooldown.lastUsed);
            if (timeSinceUse.count() >= cooldown.cooldownMs)
            {
                cooldown.isReady = true;
                TC_LOG_DEBUG("playerbot", "RogueSpecialization: Spell {} cooldown ready for bot {}", spellId, _bot->GetName());
            }
        }
    }
}

bool RogueSpecialization::IsSpellReady(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
        return it->second.isReady;

    // If not tracked, assume ready
    return true;
}

void RogueSpecialization::StartCooldown(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        it->second.isReady = false;
        it->second.lastUsed = std::chrono::steady_clock::now();

        TC_LOG_DEBUG("playerbot", "RogueSpecialization: Started cooldown for spell {} for bot {}", spellId, _bot->GetName());
    }
}

uint32 RogueSpecialization::GetCooldownRemaining(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && !it->second.isReady)
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto timeSinceUse = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - it->second.lastUsed);

        if (timeSinceUse.count() < it->second.cooldownMs)
            return it->second.cooldownMs - timeSinceUse.count();
    }

    return 0;
}

bool RogueSpecialization::HasEnoughEnergyFor(uint32 spellId)
{
    uint32 cost = GetEnergyCost(spellId);
    return GetCurrentEnergy() >= cost;
}

uint32 RogueSpecialization::GetEnergyCost(uint32 spellId)
{
    SpellInfo const* spellInfo = GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    // Base energy costs for common rogue abilities
    switch (spellId)
    {
        case SINISTER_STRIKE:
        case MUTILATE:
        case HEMORRHAGE:
            return 40;
        case BACKSTAB:
        case AMBUSH:
            return 50;
        case EVISCERATE:
        case RUPTURE:
        case ENVENOM:
            return 35;
        case SLICE_AND_DICE:
            return 25;
        case EXPOSE_ARMOR:
            return 25;
        case GARROTE:
            return 50;
        case CHEAP_SHOT:
            return 60;
        case KIDNEY_SHOT:
            return 25;
        case GOUGE:
            return 45;
        case KICK:
            return 25;
        case FAN_OF_KNIVES:
            return 50;
        case SPRINT:
            return 0; // No energy cost, but has cooldown
        case VANISH:
        case STEALTH:
        case PREPARATION:
        case EVASION:
        case CLOAK_OF_SHADOWS:
            return 0; // Utility spells typically don't cost energy
        default:
            return spellInfo->ManaCost; // Fallback to spell data
    }
}

} // namespace Playerbot