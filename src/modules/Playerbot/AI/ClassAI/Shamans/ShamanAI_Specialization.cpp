/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShamanAI.h"
#include "ElementalSpecialization.h"
#include "EnhancementSpecialization.h"
#include "RestorationSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

void ShamanAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

ShamanSpec ShamanAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return ShamanSpec::ELEMENTAL;

    // Check for Enhancement specialization indicators
    if (bot->HasSpell(17364) || bot->HasSpell(60103)) // Stormstrike or Lava Lash
        return ShamanSpec::ENHANCEMENT;

    // Check for Restoration specialization indicators
    if (bot->HasSpell(61295) || bot->HasSpell(16188)) // Riptide or Nature's Swiftness
        return ShamanSpec::RESTORATION;

    // Default to Elemental
    return ShamanSpec::ELEMENTAL;
}

void ShamanAI::SwitchSpecialization(ShamanSpec newSpec)
{
    _currentSpec = newSpec;

    switch (newSpec)
    {
        case ShamanSpec::ELEMENTAL:
            _specialization = std::make_unique<ElementalSpecialization>(GetBot());
            break;
        case ShamanSpec::ENHANCEMENT:
            _specialization = std::make_unique<EnhancementSpecialization>(GetBot());
            break;
        case ShamanSpec::RESTORATION:
            _specialization = std::make_unique<RestorationSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<ElementalSpecialization>(GetBot());
            break;
    }
}

void ShamanAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void ShamanAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    DelegateToSpecialization(target);
}

void ShamanAI::UpdateBuffs()
{
    UpdateShamanBuffs();
    if (_specialization)
        _specialization->UpdateBuffs();
}

void ShamanAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool ShamanAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void ShamanAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void ShamanAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool ShamanAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return GetBot()->GetPower(POWER_MANA) >= 100;
}

void ShamanAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position ShamanAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return GetBot()->GetPosition();
}

float ShamanAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 30.0f;
}

ShamanAI::ShamanAI(Player* bot)
    : ClassAI(bot)
    , _manaSpent(0)
    , _damageDealt(0)
    , _healingDone(0)
    , _totemsDeploy(0)
    , _shocksUsed(0)
{
    InitializeSpecialization();
}

void ShamanAI::UpdateShamanBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Basic Lightning Shield management
    if (!bot->HasAura(324) && bot->HasSpell(324)) // Lightning Shield
    {
        bot->CastSpell(bot, 324, false);
    }
}

void ShamanAI::UpdateTotemCheck()
{
    if (_specialization)
        _specialization->UpdateTotemManagement();
}

void ShamanAI::UpdateShockRotation()
{
    ::Unit* target = GetBot()->GetSelectedUnit();
    if (_specialization && target)
        _specialization->UpdateShockRotation(target);
}

} // namespace Playerbot