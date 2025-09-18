/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DruidAI.h"
#include "BalanceSpecialization.h"
#include "FeralSpecialization.h"
#include "GuardianSpecialization.h"
#include "RestorationSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

DruidAI::DruidAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void DruidAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void DruidAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void DruidAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool DruidAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void DruidAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void DruidAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool DruidAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void DruidAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position DruidAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float DruidAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 30.0f;
}

void DruidAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Detect specialization based on talents and spells
    DruidSpec detected = DruidSpec::BALANCE; // Default

    // Check for specialization-specific spells or talents
    if (bot->HasSpell(24858) || bot->HasSpell(78674)) // MOONKIN_FORM or STARSURGE
    {
        detected = DruidSpec::BALANCE;
    }
    else if (bot->HasSpell(768) && (bot->HasSpell(5221) || bot->HasSpell(5217))) // CAT_FORM and (SHRED or TIGERS_FURY)
    {
        detected = DruidSpec::FERAL;
    }
    else if (bot->HasSpell(5487) && (bot->HasSpell(22842) || bot->HasSpell(61336))) // BEAR_FORM and (FRENZIED_REGENERATION or SURVIVAL_INSTINCTS)
    {
        detected = DruidSpec::GUARDIAN;
    }
    else if (bot->HasSpell(33891) || bot->HasSpell(18562) || bot->HasSpell(33763)) // TREE_OF_LIFE or SWIFTMEND or LIFEBLOOM
    {
        detected = DruidSpec::RESTORATION;
    }
    else if (bot->HasSpell(5185) || bot->HasSpell(774)) // HEALING_TOUCH or REJUVENATION
    {
        detected = DruidSpec::RESTORATION;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
}

void DruidAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    DruidSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case DruidSpec::BALANCE:
            _specialization = std::make_unique<BalanceSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DruidAI: Initialized Balance specialization for bot {}", bot->GetName());
            break;
        case DruidSpec::FERAL:
            _specialization = std::make_unique<FeralSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DruidAI: Initialized Feral specialization for bot {}", bot->GetName());
            break;
        case DruidSpec::GUARDIAN:
            _specialization = std::make_unique<GuardianSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DruidAI: Initialized Guardian specialization for bot {}", bot->GetName());
            break;
        case DruidSpec::RESTORATION:
            _specialization = std::make_unique<RestorationSpecialization>(bot);
            TC_LOG_DEBUG("playerbot", "DruidAI: Initialized Restoration specialization for bot {}", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<BalanceSpecialization>(bot);
            TC_LOG_WARN("playerbot", "DruidAI: Unknown specialization for bot {}, defaulting to Balance", bot->GetName());
            break;
    }
}

DruidSpec DruidAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot