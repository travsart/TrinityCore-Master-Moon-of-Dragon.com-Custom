/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MageAI.h"
#include "ArcaneSpecialization.h"
#include "FireSpecialization.h"
#include "FrostSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

void MageAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void MageAI::UpdateSpecialization()
{
    MageSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        TC_LOG_DEBUG("playerbot.mage", "MageAI specialization changed from {} to {} for {}",
                     static_cast<uint32>(_currentSpec), static_cast<uint32>(detectedSpec), GetBot()->GetName());
        SwitchSpecialization(detectedSpec);
    }
}

MageSpec MageAI::DetectCurrentSpecialization()
{
    // Detect based on primary talents or highest skilled spec
    Player* bot = GetBot();
    if (!bot)
        return MageSpec::ARCANE; // Default fallback

    // Check for key specialization spells or talents
    // This is a simplified detection - real implementation would check talent trees

    // Check for Fire specialization indicators
    if (bot->HasSpell(COMBUSTION) || bot->HasSpell(PYROBLAST))
        return MageSpec::FIRE;

    // Check for Frost specialization indicators
    if (bot->HasSpell(ICY_VEINS) || bot->HasSpell(WATER_ELEMENTAL))
        return MageSpec::FROST;

    // Check for Arcane specialization indicators
    if (bot->HasSpell(ARCANE_POWER) || bot->HasSpell(ARCANE_BARRAGE))
        return MageSpec::ARCANE;

    // Default to Arcane if no clear specialization detected
    return MageSpec::ARCANE;
}

void MageAI::SwitchSpecialization(MageSpec newSpec)
{
    _currentSpec = newSpec;

    // Create appropriate specialization instance
    switch (newSpec)
    {
        case MageSpec::ARCANE:
            _specialization = std::make_unique<ArcaneSpecialization>(GetBot());
            break;
        case MageSpec::FIRE:
            _specialization = std::make_unique<FireSpecialization>(GetBot());
            break;
        case MageSpec::FROST:
            _specialization = std::make_unique<FrostSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<ArcaneSpecialization>(GetBot());
            break;
    }

    TC_LOG_DEBUG("playerbot.mage", "MageAI switched to {} specialization for {}",
                 _specialization->GetSpecializationName(), GetBot()->GetName());
}

void MageAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization)
    {
        TC_LOG_ERROR("playerbot.mage", "MageAI specialization not initialized for {}", GetBot()->GetName());
        return;
    }

    // Delegate rotation to current specialization
    _specialization->UpdateRotation(target);
}

// Override UpdateBuffs to use specialization
void MageAI::UpdateBuffs()
{
    // Update shared mage buffs
    UpdateMageBuffs();

    // Delegate specialization-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

// Override UpdateCooldowns to use specialization
void MageAI::UpdateCooldowns(uint32 diff)
{
    // Update shared cooldowns
    ClassAI::UpdateCooldowns(diff);

    // Delegate specialization-specific cooldowns
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

// Override CanUseAbility to check specialization
bool MageAI::CanUseAbility(uint32 spellId)
{
    // Check base requirements first
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    // Delegate to specialization for additional checks
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

// Override OnCombatStart to notify specialization
void MageAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }
}

// Override OnCombatEnd to notify specialization
void MageAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }
}

// Override HasEnoughResource to use specialization
bool MageAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    // Fallback to base mana check
    return HasEnoughMana(100); // Basic mana check
}

// Override ConsumeResource to use specialization
void MageAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

// Override GetOptimalPosition to use specialization
Position MageAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    // Fallback to base position
    return GetBot()->GetPosition();
}

// Override GetOptimalRange to use specialization
float MageAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    // Fallback to default casting range
    return OPTIMAL_CASTING_RANGE;
}

} // namespace Playerbot