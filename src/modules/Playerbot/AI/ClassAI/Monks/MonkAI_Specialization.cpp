/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MonkAI.h"
#include "BrewmasterMonkRefactored.h"
#include "MistweaverMonkRefactored.h"
#include "WindwalkerMonkRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

MonkAI::MonkAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void MonkAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void MonkAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void MonkAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool MonkAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void MonkAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void MonkAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool MonkAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void MonkAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position MonkAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float MonkAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 5.0f;
}

MonkSpec MonkAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return MonkSpec::WINDWALKER;

    // Detect specialization based on talents and spells
    MonkSpec detected = MonkSpec::WINDWALKER; // Default

    // Check for Brewmaster specific spells/talents
    if (bot->HasSpell(KEG_SMASH) || bot->HasSpell(IRONSKIN_BREW) || bot->HasSpell(PURIFYING_BREW))
    {
        detected = MonkSpec::BREWMASTER;
    }
    // Check for Mistweaver specific spells/talents
    else if (bot->HasSpell(VIVIFY) || bot->HasSpell(ENVELOPING_MIST) || bot->HasSpell(ESSENCE_FONT))
    {
        detected = MonkSpec::MISTWEAVER;
    }
    // Check for Windwalker specific spells/talents
    else if (bot->HasSpell(FISTS_OF_FURY) || bot->HasSpell(WHIRLING_DRAGON_PUNCH) || bot->HasSpell(STORM_EARTH_AND_FIRE))
    {
        detected = MonkSpec::WINDWALKER;
    }
    // Fallback detection based on general spell availability
    else if (bot->HasSpell(BREATH_OF_FIRE) || bot->HasSpell(FORTIFYING_BREW))
    {
        detected = MonkSpec::BREWMASTER;
    }
    else if (bot->HasSpell(RENEWING_MIST) || bot->HasSpell(SOOTHING_MIST))
    {
        detected = MonkSpec::MISTWEAVER;
    }
    else if (bot->HasSpell(RISING_SUN_KICK) || bot->HasSpell(TOUCH_OF_DEATH))
    {
        detected = MonkSpec::WINDWALKER;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
    return detected;
}

void MonkAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    MonkSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case MonkSpec::BREWMASTER:
            _specialization = std::make_unique<BrewmasterMonkRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.monk", "Monk {} switched to Brewmaster specialization", bot->GetName());
            break;
        case MonkSpec::MISTWEAVER:
            _specialization = std::make_unique<MistweaverMonkRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.monk", "Monk {} switched to Mistweaver specialization", bot->GetName());
            break;
        case MonkSpec::WINDWALKER:
            _specialization = std::make_unique<WindwalkerMonkRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.monk", "Monk {} switched to Windwalker specialization", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<WindwalkerMonkRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.monk", "Monk {} defaulted to Windwalker specialization", bot->GetName());
            break;
    }
}

MonkSpec MonkAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot