/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RogueAI.h"
#include "AssassinationSpecialization.h"
#include "CombatSpecialization.h"
#include "SubtletySpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

RogueAI::RogueAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void RogueAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void RogueAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void RogueAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool RogueAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void RogueAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void RogueAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool RogueAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void RogueAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position RogueAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float RogueAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 2.0f;
}

void RogueAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Detect specialization based on talents and spells
    RogueSpec detected = RogueSpec::ASSASSINATION; // Default

    // Check for Assassination specific spells/talents
    if (bot->HasSpell(MUTILATE) || bot->HasSpell(COLD_BLOOD) || bot->HasSpell(VENDETTA))
    {
        detected = RogueSpec::ASSASSINATION;
    }
    // Check for Combat specific spells/talents
    else if (bot->HasSpell(ADRENALINE_RUSH) || bot->HasSpell(BLADE_FLURRY) || bot->HasSpell(RIPOSTE))
    {
        detected = RogueSpec::COMBAT;
    }
    // Check for Subtlety specific spells/talents
    else if (bot->HasSpell(SHADOWSTEP) || bot->HasSpell(SHADOW_DANCE) || bot->HasSpell(HEMORRHAGE))
    {
        detected = RogueSpec::SUBTLETY;
    }
    // Fallback detection based on general spell availability
    else if (bot->HasSpell(DEADLY_POISON_9) || bot->HasSpell(ENVENOM))
    {
        detected = RogueSpec::ASSASSINATION;
    }
    else if (bot->HasSpell(SINISTER_STRIKE) && bot->HasSpell(EVISCERATE))
    {
        detected = RogueSpec::COMBAT;
    }
    else if (bot->HasSpell(BACKSTAB) && bot->HasSpell(STEALTH))
    {
        detected = RogueSpec::SUBTLETY;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
}

void RogueAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    RogueSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case RogueSpec::ASSASSINATION:
            _specialization = std::make_unique<AssassinationSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "RogueAI: Initialized Assassination specialization for bot {}", bot->GetName());
            break;
        case RogueSpec::COMBAT:
            _specialization = std::make_unique<CombatSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "RogueAI: Initialized Combat specialization for bot {}", bot->GetName());
            break;
        case RogueSpec::SUBTLETY:
            _specialization = std::make_unique<SubtletySpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "RogueAI: Initialized Subtlety specialization for bot {}", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<AssassinationSpecialization>(bot);
            TC_LOG_WARN("playerbot", "RogueAI: Unknown specialization for bot {}, defaulting to Assassination", bot->GetName());
            break;
    }
}

RogueSpec RogueAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot