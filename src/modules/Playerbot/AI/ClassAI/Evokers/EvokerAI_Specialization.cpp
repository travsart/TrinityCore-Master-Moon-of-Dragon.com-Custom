/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EvokerAI.h"
#include "DevastationEvokerRefactored.h"
#include "PreservationEvokerRefactored.h"
#include "AugmentationEvokerRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

EvokerAI::EvokerAI(Player* bot) : ClassAI(bot), _specialization(nullptr)
{
    DetectSpecialization();
    InitializeSpecialization();
}

void EvokerAI::UpdateRotation(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void EvokerAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void EvokerAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool EvokerAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void EvokerAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void EvokerAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool EvokerAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void EvokerAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position EvokerAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float EvokerAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 25.0f;
}

EvokerSpec EvokerAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return EvokerSpec::DEVASTATION;

    // Detect specialization based on talents and spells
    EvokerSpec detected = EvokerSpec::DEVASTATION; // Default

    // Check for Devastation specific spells/talents
    if (bot->HasSpell(DRAGONRAGE) || bot->HasSpell(SHATTERING_STAR) || bot->HasSpell(FIRESTORM))
    {
        detected = EvokerSpec::DEVASTATION;
    }
    // Check for Preservation specific spells/talents
    else if (bot->HasSpell(DREAM_FLIGHT) || bot->HasSpell(CALL_OF_YSERA) || bot->HasSpell(TEMPORAL_ANOMALY))
    {
        detected = EvokerSpec::PRESERVATION;
    }
    // Check for Augmentation specific spells/talents
    else if (bot->HasSpell(EBON_MIGHT) || bot->HasSpell(PRESCIENCE) || bot->HasSpell(SPATIAL_PARADOX))
    {
        detected = EvokerSpec::AUGMENTATION;
    }
    // Fallback detection based on general spell availability
    else if (bot->HasSpell(PYRE) || bot->HasSpell(DISINTEGRATE))
    {
        detected = EvokerSpec::DEVASTATION;
    }
    else if (bot->HasSpell(EMERALD_BLOSSOM) || bot->HasSpell(VERDANT_EMBRACE))
    {
        detected = EvokerSpec::PRESERVATION;
    }
    else if (bot->HasSpell(BLISTERY_SCALES) || bot->HasSpell(BREATH_OF_EONS))
    {
        detected = EvokerSpec::AUGMENTATION;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
    return detected;
}

void EvokerAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    EvokerSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case EvokerSpec::DEVASTATION:
            _specialization = std::make_unique<DevastationEvokerRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.evoker", "Evoker {} switched to Devastation specialization", bot->GetName());
            break;
        case EvokerSpec::PRESERVATION:
            _specialization = std::make_unique<PreservationEvokerRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.evoker", "Evoker {} switched to Preservation specialization", bot->GetName());
            break;
        case EvokerSpec::AUGMENTATION:
            _specialization = std::make_unique<AugmentationEvokerRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.evoker", "Evoker {} switched to Augmentation specialization", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<DevastationEvokerRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.evoker", "Evoker {} defaulted to Devastation specialization", bot->GetName());
            break;
    }
}

EvokerSpec EvokerAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

} // namespace Playerbot