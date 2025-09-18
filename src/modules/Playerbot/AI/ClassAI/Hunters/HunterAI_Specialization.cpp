/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HunterAI.h"
#include "BeastMasterySpecialization.h"
#include "MarksmanshipSpecialization.h"
#include "SurvivalSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

HunterAI::HunterAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void HunterAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void HunterAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void HunterAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool HunterAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void HunterAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void HunterAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool HunterAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void HunterAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position HunterAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float HunterAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 25.0f;
}

void HunterAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Detect specialization based on talents and spells
    HunterSpec detected = HunterSpec::BEAST_MASTERY; // Default

    // Check for Beast Mastery specific spells/talents
    if (bot->HasSpell(19574) || bot->HasSpell(19577) || bot->HasSpell(19578)) // BESTIAL_WRATH, INTIMIDATION, SPIRIT_BOND
    {
        detected = HunterSpec::BEAST_MASTERY;
    }
    // Check for Marksmanship specific spells/talents
    else if (bot->HasSpell(19434) || bot->HasSpell(19506) || bot->HasSpell(34490)) // AIMED_SHOT, TRUESHOT_AURA, SILENCING_SHOT
    {
        detected = HunterSpec::MARKSMANSHIP;
    }
    // Check for Survival specific spells/talents
    else if (bot->HasSpell(60053) || bot->HasSpell(3674) || bot->HasSpell(19386)) // EXPLOSIVE_SHOT, BLACK_ARROW, WYVERN_STING
    {
        detected = HunterSpec::SURVIVAL;
    }
    // Fallback detection based on general spells
    else if (bot->HasSpell(CALL_PET))
    {
        detected = HunterSpec::BEAST_MASTERY;
    }
    else if (bot->HasSpell(STEADY_SHOT))
    {
        detected = HunterSpec::MARKSMANSHIP;
    }
    else if (bot->HasSpell(RAPTOR_STRIKE))
    {
        detected = HunterSpec::SURVIVAL;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
}

void HunterAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    HunterSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case HunterSpec::BEAST_MASTERY:
            _specialization = std::make_unique<BeastMasterySpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "HunterAI: Initialized Beast Mastery specialization for bot {}", bot->GetName());
            break;
        case HunterSpec::MARKSMANSHIP:
            _specialization = std::make_unique<MarksmanshipSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "HunterAI: Initialized Marksmanship specialization for bot {}", bot->GetName());
            break;
        case HunterSpec::SURVIVAL:
            _specialization = std::make_unique<SurvivalSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "HunterAI: Initialized Survival specialization for bot {}", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<BeastMasterySpecialization>(bot);
            TC_LOG_WARN("playerbot", "HunterAI: Unknown specialization for bot {}, defaulting to Beast Mastery", bot->GetName());
            break;
    }
}

HunterSpec HunterAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot