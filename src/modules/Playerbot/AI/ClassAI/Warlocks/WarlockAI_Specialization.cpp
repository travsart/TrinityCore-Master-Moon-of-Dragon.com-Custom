/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarlockAI.h"
#include "AfflictionSpecialization.h"
#include "DemonologySpecialization.h"
#include "DestructionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

void WarlockAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

WarlockSpec WarlockAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockSpec::AFFLICTION;

    // Check for Demonology specialization indicators
    if (bot->HasSpell(30146) || bot->HasSpell(47193)) // Summon Felguard or Demonic Empowerment
        return WarlockSpec::DEMONOLOGY;

    // Check for Destruction specialization indicators
    if (bot->HasSpell(17962) || bot->HasSpell(50796)) // Conflagrate or Chaos Bolt
        return WarlockSpec::DESTRUCTION;

    // Default to Affliction
    return WarlockSpec::AFFLICTION;
}

void WarlockAI::SwitchSpecialization(WarlockSpec newSpec)
{
    _currentSpec = newSpec;

    switch (newSpec)
    {
        case WarlockSpec::AFFLICTION:
            _specialization = std::make_unique<AfflictionSpecialization>(GetBot());
            break;
        case WarlockSpec::DEMONOLOGY:
            _specialization = std::make_unique<DemonologySpecialization>(GetBot());
            break;
        case WarlockSpec::DESTRUCTION:
            _specialization = std::make_unique<DestructionSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<AfflictionSpecialization>(GetBot());
            break;
    }
}

void WarlockAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void WarlockAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    DelegateToSpecialization(target);
}

void WarlockAI::UpdateBuffs()
{
    UpdateWarlockBuffs();
    if (_specialization)
        _specialization->UpdateBuffs();
}

void WarlockAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool WarlockAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void WarlockAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void WarlockAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool WarlockAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return GetBot()->GetPower(POWER_MANA) >= 100;
}

void WarlockAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position WarlockAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return GetBot()->GetPosition();
}

float WarlockAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 30.0f;
}

WarlockAI::WarlockAI(Player* bot)
    : ClassAI(bot)
    , _manaSpent(0)
    , _damageDealt(0)
    , _dotDamage(0)
    , _petDamage(0)
{
    InitializeSpecialization();
}

void WarlockAI::UpdateWarlockBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Basic armor spell management
    if (!bot->HasAura(28176) && !bot->HasAura(706) && !bot->HasAura(687)) // Fel Armor, Demon Armor, Demon Skin
    {
        if (bot->HasSpell(28176))
            bot->CastSpell(bot, 28176, false); // Fel Armor
        else if (bot->HasSpell(706))
            bot->CastSpell(bot, 706, false); // Demon Armor
        else if (bot->HasSpell(687))
            bot->CastSpell(bot, 687, false); // Demon Skin
    }
}

void WarlockAI::UpdatePetCheck()
{
    if (_specialization)
        _specialization->UpdatePetManagement();
}

void WarlockAI::UpdateSoulShardCheck()
{
    if (_specialization)
        _specialization->UpdateSoulShardManagement();
}

} // namespace Playerbot