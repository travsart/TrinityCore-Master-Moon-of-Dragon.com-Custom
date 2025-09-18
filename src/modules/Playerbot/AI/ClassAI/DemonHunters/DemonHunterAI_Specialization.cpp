/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DemonHunterAI.h"
#include "HavocSpecialization.h"
#include "VengeanceSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

DemonHunterAI::DemonHunterAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void DemonHunterAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void DemonHunterAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void DemonHunterAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool DemonHunterAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void DemonHunterAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void DemonHunterAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool DemonHunterAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void DemonHunterAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position DemonHunterAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float DemonHunterAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 5.0f;
}

void DemonHunterAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Detect specialization based on talents and spells
    DemonHunterSpec detected = DemonHunterSpec::HAVOC; // Default

    // Check for specialization-specific spells or talents
    if (bot->HasSpell(203720) || bot->HasSpell(204021) || bot->HasSpell(228477)) // DEMON_SPIKES, FIERY_BRAND, SOUL_CLEAVE
    {
        detected = DemonHunterSpec::VENGEANCE;
    }
    else if (bot->HasSpell(162794) || bot->HasSpell(188499) || bot->HasSpell(198013)) // CHAOS_STRIKE, BLADE_DANCE, EYE_BEAM
    {
        detected = DemonHunterSpec::HAVOC;
    }
    else if (bot->HasSpell(187827)) // METAMORPHOSIS_VENGEANCE
    {
        detected = DemonHunterSpec::VENGEANCE;
    }
    else if (bot->HasSpell(191427)) // METAMORPHOSIS_HAVOC
    {
        detected = DemonHunterSpec::HAVOC;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
}

void DemonHunterAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    DemonHunterSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case DemonHunterSpec::HAVOC:
            _specialization = std::make_unique<HavocSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DemonHunterAI: Initialized Havoc specialization for bot {}", bot->GetName());
            break;
        case DemonHunterSpec::VENGEANCE:
            _specialization = std::make_unique<VengeanceSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DemonHunterAI: Initialized Vengeance specialization for bot {}", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<HavocSpecialization>(bot);
            TC_LOG_WARN("playerbot", "DemonHunterAI: Unknown specialization for bot {}, defaulting to Havoc", bot->GetName());
            break;
    }
}

DemonHunterSpec DemonHunterAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot