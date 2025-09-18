/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DemonHunterSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

DemonHunterSpecialization::DemonHunterSpecialization(Player* bot)
    : _bot(bot)
    , _lastSoulFragmentUpdate(0)
    , _metamorphosisState(MetamorphosisState::NONE)
    , _metamorphosisRemaining(0)
    , _lastMetamorphosisUpdate(0)
{
}

uint32 DemonHunterSpecialization::GetFury() const
{
    if (!_bot)
        return 0;
    return _bot->GetPower(POWER_FURY);
}

uint32 DemonHunterSpecialization::GetPain() const
{
    if (!_bot)
        return 0;
    return _bot->GetPower(POWER_PAIN);
}

void DemonHunterSpecialization::SpendFury(uint32 amount)
{
    if (!_bot)
        return;

    uint32 currentFury = GetFury();
    if (currentFury >= amount)
        _bot->SetPower(POWER_FURY, currentFury - amount);
}

void DemonHunterSpecialization::SpendPain(uint32 amount)
{
    if (!_bot)
        return;

    uint32 currentPain = GetPain();
    if (currentPain >= amount)
        _bot->SetPower(POWER_PAIN, currentPain - amount);
}

void DemonHunterSpecialization::GenerateFury(uint32 amount)
{
    if (!_bot)
        return;

    uint32 currentFury = GetFury();
    uint32 maxFury = _bot->GetMaxPower(POWER_FURY);
    _bot->SetPower(POWER_FURY, std::min(currentFury + amount, maxFury));
}

void DemonHunterSpecialization::GeneratePain(uint32 amount)
{
    if (!_bot)
        return;

    uint32 currentPain = GetPain();
    uint32 maxPain = _bot->GetMaxPower(POWER_PAIN);
    _bot->SetPower(POWER_PAIN, std::min(currentPain + amount, maxPain));
}

void DemonHunterSpecialization::AddSoulFragment(Position pos, bool isGreater)
{
    SoulFragment fragment(pos, isGreater);
    _soulFragments.push_back(fragment);

    // Limit the number of soul fragments
    if (_soulFragments.size() > 10)
        _soulFragments.erase(_soulFragments.begin());
}

void DemonHunterSpecialization::RemoveExpiredSoulFragments()
{
    uint32 now = getMSTime();
    if (now - _lastSoulFragmentUpdate < 1000) // Check every second
        return;

    _lastSoulFragmentUpdate = now;

    auto it = std::remove_if(_soulFragments.begin(), _soulFragments.end(),
        [](const SoulFragment& fragment) { return fragment.IsExpired(); });

    _soulFragments.erase(it, _soulFragments.end());
}

std::vector<SoulFragment> DemonHunterSpecialization::GetNearbySoulFragments(float range) const
{
    std::vector<SoulFragment> nearbyFragments;

    if (!_bot)
        return nearbyFragments;

    Position botPos = _bot->GetPosition();
    for (const auto& fragment : _soulFragments)
    {
        if (botPos.GetExactDist(fragment.position) <= range)
            nearbyFragments.push_back(fragment);
    }

    return nearbyFragments;
}

bool DemonHunterSpecialization::IsInMetamorphosis() const
{
    return _metamorphosisState != MetamorphosisState::NONE && _metamorphosisRemaining > 0;
}

uint32 DemonHunterSpecialization::GetMetamorphosisRemaining() const
{
    return _metamorphosisRemaining;
}

bool DemonHunterSpecialization::HasSigil(uint32 sigilType) const
{
    if (!_bot)
        return false;

    // Check if the bot has the sigil spell
    return _bot->HasSpell(sigilType);
}

void DemonHunterSpecialization::CastSigil(uint32 sigilSpellId, Position targetPos)
{
    if (!_bot)
        return;

    if (HasSigil(sigilSpellId))
    {
        _bot->CastSpell(_bot, sigilSpellId, false);
    }
}

bool DemonHunterSpecialization::ShouldUseDefensiveCooldown() const
{
    if (!_bot)
        return false;

    return _bot->GetHealthPct() < 50.0f || _bot->getAttackers().size() > 2;
}

} // namespace Playerbot