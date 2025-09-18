/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DeathKnightSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

DeathKnightSpecialization::DeathKnightSpecialization(Player* bot)
    : _bot(bot)
    , _lastRuneRegen(0)
    , _runicPower(0)
    , _maxRunicPower(RUNIC_POWER_MAX)
    , _lastRunicPowerDecay(0)
    , _lastDiseaseUpdate(0)
    , _deathAndDecayRemaining(0)
    , _lastDeathAndDecay(0)
{
    // Initialize runes: 2 Blood, 2 Frost, 2 Unholy
    _runes[0] = RuneInfo(RuneType::BLOOD);
    _runes[1] = RuneInfo(RuneType::BLOOD);
    _runes[2] = RuneInfo(RuneType::FROST);
    _runes[3] = RuneInfo(RuneType::FROST);
    _runes[4] = RuneInfo(RuneType::UNHOLY);
    _runes[5] = RuneInfo(RuneType::UNHOLY);
}

void DeathKnightSpecialization::RegenerateRunes(uint32 diff)
{
    uint32 now = getMSTime();
    if (_lastRuneRegen == 0)
        _lastRuneRegen = now;

    uint32 timeDiff = now - _lastRuneRegen;
    if (timeDiff >= 1000) // Check every second
    {
        for (auto& rune : _runes)
        {
            if (!rune.available && rune.cooldownRemaining > 0)
            {
                if (rune.cooldownRemaining <= timeDiff)
                {
                    rune.cooldownRemaining = 0;
                    rune.available = true;
                }
                else
                {
                    rune.cooldownRemaining -= timeDiff;
                }
            }
        }
        _lastRuneRegen = now;
    }
}

bool DeathKnightSpecialization::CanConvertRune(RuneType from, RuneType to) const
{
    if (!_bot)
        return false;

    // Death runes can be converted from any type
    return from != RuneType::DEATH && to == RuneType::DEATH;
}

void DeathKnightSpecialization::ConvertRune(RuneType from, RuneType to)
{
    for (auto& rune : _runes)
    {
        if (rune.type == from && rune.available)
        {
            rune.type = to;
            break;
        }
    }
}

uint32 DeathKnightSpecialization::GetTotalAvailableRunes() const
{
    uint32 count = 0;
    for (const auto& rune : _runes)
    {
        if (rune.IsReady())
            count++;
    }
    return count;
}

void DeathKnightSpecialization::UpdateDiseaseTimers(uint32 diff)
{
    uint32 now = getMSTime();
    if (now - _lastDiseaseUpdate < 1000) // Update every second
        return;

    _lastDiseaseUpdate = now;

    for (auto& targetDiseases : _activeDiseases)
    {
        for (auto& disease : targetDiseases.second)
        {
            if (disease.remainingTime > 1000)
                disease.remainingTime -= 1000;
            else
                disease.remainingTime = 0;
        }
    }

    RemoveExpiredDiseases();
}

void DeathKnightSpecialization::RemoveExpiredDiseases()
{
    for (auto it = _activeDiseases.begin(); it != _activeDiseases.end();)
    {
        auto& diseases = it->second;
        diseases.erase(
            std::remove_if(diseases.begin(), diseases.end(),
                [](const DiseaseInfo& disease) { return !disease.IsActive(); }),
            diseases.end());

        if (diseases.empty())
            it = _activeDiseases.erase(it);
        else
            ++it;
    }
}

std::vector<DiseaseInfo> DeathKnightSpecialization::GetActiveDiseases(::Unit* target) const
{
    if (!target)
        return {};

    auto it = _activeDiseases.find(target->GetGUID());
    if (it != _activeDiseases.end())
        return it->second;

    return {};
}

uint32 DeathKnightSpecialization::GetDiseaseRemainingTime(::Unit* target, DiseaseType type) const
{
    if (!target)
        return 0;

    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
    {
        if (disease.type == type)
            return disease.remainingTime;
    }

    return 0;
}

bool DeathKnightSpecialization::ShouldUseDeathGrip(::Unit* target) const
{
    if (!_bot || !target)
        return false;

    // Use Death Grip to pull enemies at range
    float distance = _bot->GetDistance(target);
    return distance > 10.0f && distance < 30.0f && !target->IsWithinMeleeRange(_bot);
}

void DeathKnightSpecialization::CastDeathGrip(::Unit* target)
{
    if (!_bot || !target)
        return;

    if (_bot->HasSpell(DEATH_GRIP) && ShouldUseDeathGrip(target))
    {
        _bot->CastSpell(target, DEATH_GRIP, false);
    }
}

bool DeathKnightSpecialization::ShouldUseDeathCoil(::Unit* target) const
{
    if (!_bot || !target)
        return false;

    // Use Death Coil for healing or damage
    if (_bot->GetHealthPct() < 50.0f)
        return true; // Self-heal

    return _bot->GetDistance(target) > 5.0f; // Ranged damage
}

void DeathKnightSpecialization::CastDeathCoil(::Unit* target)
{
    if (!_bot || !target)
        return;

    if (_bot->HasSpell(DEATH_COIL) && ShouldUseDeathCoil(target))
    {
        if (_bot->GetHealthPct() < 50.0f)
            _bot->CastSpell(_bot, DEATH_COIL, false); // Heal self
        else
            _bot->CastSpell(target, DEATH_COIL, false); // Damage enemy
    }
}

} // namespace Playerbot