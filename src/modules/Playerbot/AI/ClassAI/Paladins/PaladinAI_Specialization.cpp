/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PaladinAI.h"
#include "HolySpecialization.h"
#include "ProtectionSpecialization.h"
#include "RetributionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

void PaladinAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

PaladinSpec PaladinAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return PaladinSpec::HOLY;

    // Check for Protection specialization indicators
    if (bot->HasSpell(31935) || bot->HasSpell(26573)) // Avenger's Shield or Consecration
        return PaladinSpec::PROTECTION;

    // Check for Retribution specialization indicators
    if (bot->HasSpell(35395) || bot->HasSpell(53385)) // Crusader Strike or Divine Storm
        return PaladinSpec::RETRIBUTION;

    // Default to Holy
    return PaladinSpec::HOLY;
}

void PaladinAI::SwitchSpecialization(PaladinSpec newSpec)
{
    _currentSpec = newSpec;

    switch (newSpec)
    {
        case PaladinSpec::HOLY:
            _specialization = std::make_unique<HolySpecialization>(GetBot());
            break;
        case PaladinSpec::PROTECTION:
            _specialization = std::make_unique<ProtectionSpecialization>(GetBot());
            break;
        case PaladinSpec::RETRIBUTION:
            _specialization = std::make_unique<RetributionSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<HolySpecialization>(GetBot());
            break;
    }
}

void PaladinAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void PaladinAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    DelegateToSpecialization(target);
}

void PaladinAI::UpdateBuffs()
{
    UpdatePaladinBuffs();
    if (_specialization)
        _specialization->UpdateBuffs();
}

void PaladinAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool PaladinAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void PaladinAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void PaladinAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool PaladinAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return GetBot()->GetPower(POWER_MANA) >= 100;
}

void PaladinAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position PaladinAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return GetBot()->GetPosition();
}

float PaladinAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 5.0f;
}

PaladinAI::PaladinAI(Player* bot)
    : ClassAI(bot)
    , _manaSpent(0)
    , _healingDone(0)
    , _damageDealt(0)
    , _lastBlessings(0)
    , _lastAura(0)
{
    InitializeSpecialization();
}

} // namespace Playerbot