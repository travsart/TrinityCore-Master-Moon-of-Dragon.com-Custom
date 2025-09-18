/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DeathKnightAI.h"
#include "BloodSpecialization.h"
#include "FrostSpecialization.h"
#include "UnholySpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

DeathKnightAI::DeathKnightAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void DeathKnightAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void DeathKnightAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void DeathKnightAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool DeathKnightAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void DeathKnightAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void DeathKnightAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool DeathKnightAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void DeathKnightAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position DeathKnightAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float DeathKnightAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 5.0f;
}

void DeathKnightAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Detect specialization based on talents and spells
    DeathKnightSpec detected = DeathKnightSpec::BLOOD; // Default

    // Check for specialization-specific spells or talents
    if (bot->HasSpell(55050) || bot->HasSpell(55233) || bot->HasSpell(49222)) // HEART_STRIKE, VAMPIRIC_BLOOD, BONE_SHIELD
    {
        detected = DeathKnightSpec::BLOOD;
    }
    else if (bot->HasSpell(49020) || bot->HasSpell(49143) || bot->HasSpell(49184)) // OBLITERATE, FROST_STRIKE, HOWLING_BLAST
    {
        detected = DeathKnightSpec::FROST;
    }
    else if (bot->HasSpell(55090) || bot->HasSpell(49206) || bot->HasSpell(42650)) // SCOURGE_STRIKE, SUMMON_GARGOYLE, ARMY_OF_THE_DEAD
    {
        detected = DeathKnightSpec::UNHOLY;
    }
    else if (bot->HasSpell(48266)) // BLOOD_PRESENCE
    {
        detected = DeathKnightSpec::BLOOD;
    }
    else if (bot->HasSpell(48263)) // FROST_PRESENCE
    {
        detected = DeathKnightSpec::FROST;
    }
    else if (bot->HasSpell(48265)) // UNHOLY_PRESENCE
    {
        detected = DeathKnightSpec::UNHOLY;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
}

void DeathKnightAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    DeathKnightSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case DeathKnightSpec::BLOOD:
            _specialization = std::make_unique<BloodSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Initialized Blood specialization for bot {}", bot->GetName());
            break;
        case DeathKnightSpec::FROST:
            _specialization = std::make_unique<FrostSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Initialized Frost specialization for bot {}", bot->GetName());
            break;
        case DeathKnightSpec::UNHOLY:
            _specialization = std::make_unique<UnholySpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Initialized Unholy specialization for bot {}", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<BloodSpecialization>(bot);
            TC_LOG_WARN("playerbot", "DeathKnightAI: Unknown specialization for bot {}, defaulting to Blood", bot->GetName());
            break;
    }
}

DeathKnightSpec DeathKnightAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot