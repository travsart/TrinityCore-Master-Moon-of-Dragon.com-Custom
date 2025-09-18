/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MonkSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Group.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

MonkSpecialization::MonkSpecialization(Player* bot)
    : _bot(bot), _mana(0), _maxMana(0), _combatStartTime(0), _averageCombatTime(0),
      _currentTarget(nullptr), _lastChiGeneration(0), _lastEnergyRegen(0), _lastBuffUpdate(0),
      _lastUtilityUse(0), _lastRoll(0), _lastTeleport(0), _lastTranscendence(0),
      _inTranscendence(false), _damageDealt(0), _healingDone(0), _damageMitigated(0),
      _chiGenerated(0), _energySpent(0)
{
    // Initialize chi system
    _chi = ChiInfo();

    // Initialize energy system
    _energy = EnergyInfo();

    // Initialize mana for Mistweaver
    if (_bot)
    {
        _mana = _bot->GetPower(POWER_MANA);
        _maxMana = _bot->GetMaxPower(POWER_MANA);
    }

    TC_LOG_DEBUG("playerbot", "MonkSpecialization: Base specialization initialized for bot {}",
                _bot ? _bot->GetName() : "Unknown");
}

void MonkSpecialization::UpdateChiManagement()
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();

    // Natural chi regeneration
    if (currentTime - _lastChiGeneration >= CHI_GENERATION_INTERVAL)
    {
        if (_chi.isRegenerating && _chi.current < _chi.maximum)
        {
            GenerateChi(1);
        }
        _lastChiGeneration = currentTime;
    }

    // Optimize chi usage
    if (_chi.current > 3)
    {
        // We have excess chi, consider spending it
        ::Unit* target = GetCurrentTarget();
        if (target && HasSpell(BLACKOUT_KICK))
        {
            // Use efficient chi spender
            CastSpell(BLACKOUT_KICK, target);
        }
    }
    else if (_chi.current < 2 && _bot->IsInCombat())
    {
        // Low on chi, generate more
        ::Unit* target = GetCurrentTarget();
        if (target && HasSpell(TIGER_PALM) && HasEnergy(50))
        {
            CastSpell(TIGER_PALM, target);
        }
    }
}

void MonkSpecialization::UpdateEnergyManagement()
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();

    // Energy regeneration
    if (currentTime - _lastEnergyRegen >= 1000) // Every second
    {
        uint32 regenAmount = ENERGY_REGEN_RATE / 10; // 10 energy per second
        RegenEnergy(regenAmount);
        _lastEnergyRegen = currentTime;
    }

    // Avoid energy capping
    if (_energy.current >= _energy.maximum * 0.9f)
    {
        // Use energy to avoid waste
        ::Unit* target = GetCurrentTarget();
        if (target && HasSpell(TIGER_PALM))
        {
            CastSpell(TIGER_PALM, target);
        }
    }
}

bool MonkSpecialization::HasChi(uint32 required) const
{
    return _chi.HasChi(required);
}

uint32 MonkSpecialization::GetChi() const
{
    return _chi.current;
}

uint32 MonkSpecialization::GetMaxChi() const
{
    return _chi.maximum;
}

void MonkSpecialization::SpendChi(uint32 amount)
{
    _chi.SpendChi(amount);
}

void MonkSpecialization::GenerateChi(uint32 amount)
{
    _chi.GenerateChi(amount);
    _chiGenerated += amount;
}

bool MonkSpecialization::HasEnergy(uint32 required) const
{
    return _energy.HasEnergy(required);
}

uint32 MonkSpecialization::GetEnergy() const
{
    return _energy.current;
}

uint32 MonkSpecialization::GetMaxEnergy() const
{
    return _energy.maximum;
}

void MonkSpecialization::SpendEnergy(uint32 amount)
{
    _energy.SpendEnergy(amount);
    _energySpent += amount;
}

void MonkSpecialization::RegenEnergy(uint32 amount)
{
    _energy.RegenEnergy(amount);
}

float MonkSpecialization::GetEnergyPercent() const
{
    return _energy.GetPercent();
}

bool MonkSpecialization::CastSpell(uint32 spellId, ::Unit* target)
{
    if (!_bot)
        return false;

    if (!HasSpell(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Cast the spell
    bool result = false;
    if (target)
    {
        result = _bot->CastSpell(target, spellId, false);
    }
    else
    {
        result = _bot->CastSpell(_bot, spellId, false);
    }

    if (result)
    {
        ConsumeResource(spellId);
    }

    return result;
}

bool MonkSpecialization::HasSpell(uint32 spellId) const
{
    return _bot && _bot->HasSpell(spellId);
}

bool MonkSpecialization::HasAura(uint32 spellId, ::Unit* target) const
{
    if (!target)
        target = _bot;

    return target && target->HasAura(spellId);
}

uint32 MonkSpecialization::GetSpellCooldown(uint32 spellId) const
{
    if (!_bot)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    return _bot->GetSpellCooldownDelay(spellId);
}

bool MonkSpecialization::IsSpellReady(uint32 spellId) const
{
    return GetSpellCooldown(spellId) == 0;
}

std::vector<::Unit*> MonkSpecialization::GetNearbyEnemies(float range) const
{
    std::vector<::Unit*> enemies;

    if (!_bot)
        return enemies;

    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(_bot, _bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, range);

    for (Unit* unit : nearbyUnits)
    {
        if (unit && unit->IsAlive() && _bot->IsValidAttackTarget(unit))
        {
            enemies.push_back(unit);
        }
    }

    return enemies;
}

std::vector<::Unit*> MonkSpecialization::GetNearbyAllies(float range) const
{
    std::vector<::Unit*> allies;

    if (!_bot)
        return allies;

    // Add self
    allies.push_back(_bot);

    // Add group members
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
        {
            if (Player* member = groupRef->GetSource())
            {
                if (member != _bot && _bot->IsWithinDistInMap(member, range))
                {
                    allies.push_back(member);
                }
            }
        }
    }

    return allies;
}

std::vector<::Unit*> MonkSpecialization::GetAoETargets(float range) const
{
    std::vector<::Unit*> targets;

    if (!_bot)
        return targets;

    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(_bot, _bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, check);
    Cell::VisitAllObjects(_bot, searcher, range);

    for (Unit* enemy : nearbyEnemies)
    {
        if (enemy && enemy->IsAlive() && _bot->IsValidAttackTarget(enemy))
        {
            targets.push_back(enemy);
        }
    }

    return targets;
}

::Unit* MonkSpecialization::GetCurrentTarget() const
{
    if (!_bot)
        return nullptr;

    return _bot->GetSelectedUnit();
}

bool MonkSpecialization::IsInMeleeRange(::Unit* target) const
{
    if (!target || !_bot)
        return false;

    return _bot->GetDistance(target) <= MELEE_RANGE;
}

bool MonkSpecialization::IsAtOptimalRange(::Unit* target) const
{
    if (!target || !_bot)
        return false;

    float distance = _bot->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    return distance <= optimalRange && distance >= optimalRange * 0.8f;
}

float MonkSpecialization::GetDistance(::Unit* target) const
{
    if (!target || !_bot)
        return 0.0f;

    return _bot->GetDistance(target);
}

void MonkSpecialization::UpdateSharedBuffs()
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastBuffUpdate < 5000) // Check every 5 seconds
        return;

    _lastBuffUpdate = currentTime;

    // Legacy of the White Tiger for stats
    if (!HasAura(LEGACY_OF_THE_WHITE_TIGER) && HasSpell(LEGACY_OF_THE_WHITE_TIGER))
    {
        CastLegacyOfTheWhiteTiger();
    }

    // Legacy of the Emperor for alternative stats
    if (!HasAura(LEGACY_OF_THE_EMPEROR) && !HasAura(LEGACY_OF_THE_WHITE_TIGER) && HasSpell(LEGACY_OF_THE_EMPEROR))
    {
        CastLegacyOfTheEmperor();
    }
}

void MonkSpecialization::CastLegacyOfTheWhiteTiger()
{
    CastSpell(LEGACY_OF_THE_WHITE_TIGER);
}

void MonkSpecialization::CastLegacyOfTheEmperor()
{
    CastSpell(LEGACY_OF_THE_EMPEROR);
}

void MonkSpecialization::CastRoll()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastRoll < 20000) // 20 second cooldown
        return;

    if (CastSpell(ROLL))
    {
        _lastRoll = currentTime;
    }
}

void MonkSpecialization::CastTeleport()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastTeleport < 10000) // 10 second cooldown
        return;

    if (CastSpell(CHI_TORPEDO))
    {
        _lastTeleport = currentTime;
    }
}

void MonkSpecialization::CastTranscendence()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastTranscendence < 10000) // 10 second cooldown
        return;

    if (!_inTranscendence && CastSpell(TRANSCENDENCE))
    {
        _inTranscendence = true;
        _transcendencePosition = _bot->GetPosition();
        _lastTranscendence = currentTime;
    }
}

void MonkSpecialization::CastTranscendenceTransfer()
{
    if (_inTranscendence && CastSpell(TRANSCENDENCE_TRANSFER))
    {
        _inTranscendence = false;
    }
}

void MonkSpecialization::CastParalysis(::Unit* target)
{
    if (!target)
        return;

    CastSpell(PARALYSIS, target);
}

void MonkSpecialization::CastLegSweep()
{
    std::vector<::Unit*> aoeTargets = GetAoETargets(8.0f);
    if (aoeTargets.size() >= 2)
    {
        CastSpell(LEG_SWEEP);
    }
}

void MonkSpecialization::CastSpearHandStrike(::Unit* target)
{
    if (!target)
        return;

    // Use as interrupt
    if (target->IsNonMeleeSpellCast(false))
    {
        CastSpell(SPEAR_HAND_STRIKE, target);
    }
}

void MonkSpecialization::LogRotationDecision(const std::string& decision, const std::string& reason)
{
    TC_LOG_DEBUG("playerbot", "MonkSpecialization [{}]: {} - {}",
                _bot ? _bot->GetName() : "Unknown", decision, reason);
}

} // namespace Playerbot