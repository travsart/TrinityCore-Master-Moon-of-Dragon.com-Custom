/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarriorAI.h"
#include "ArmsSpecialization.h"
#include "FurySpecialization.h"
#include "ProtectionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

void WarriorAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void WarriorAI::UpdateSpecialization()
{
    WarriorSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        TC_LOG_DEBUG("playerbot.warrior", "WarriorAI specialization changed from {} to {} for {}",
                     static_cast<uint32>(_currentSpec), static_cast<uint32>(detectedSpec), GetBot()->GetName());
        SwitchSpecialization(detectedSpec);
    }
}

WarriorSpec WarriorAI::DetectCurrentSpecialization()
{
    // Detect based on primary talents or highest skilled spec
    Player* bot = GetBot();
    if (!bot)
        return WarriorSpec::ARMS; // Default fallback

    // Check for key specialization spells or talents
    // This is a simplified detection - real implementation would check talent trees

    // Check for Protection specialization indicators
    if (bot->HasSpell(23922) || bot->HasSpell(355)) // Shield Slam or Taunt
        return WarriorSpec::PROTECTION;

    // Check for Fury specialization indicators
    if (bot->HasSpell(23881) || bot->HasSpell(18499)) // Bloodthirst or Berserker Rage
        return WarriorSpec::FURY;

    // Check for Arms specialization indicators
    if (bot->HasSpell(12294) || bot->HasSpell(7384)) // Mortal Strike or Overpower
        return WarriorSpec::ARMS;

    // Default to Arms if no clear specialization detected
    return WarriorSpec::ARMS;
}

void WarriorAI::SwitchSpecialization(WarriorSpec newSpec)
{
    _currentSpec = newSpec;

    // Create appropriate specialization instance
    switch (newSpec)
    {
        case WarriorSpec::ARMS:
            _specialization = std::make_unique<ArmsSpecialization>(GetBot());
            break;
        case WarriorSpec::FURY:
            _specialization = std::make_unique<FurySpecialization>(GetBot());
            break;
        case WarriorSpec::PROTECTION:
            _specialization = std::make_unique<ProtectionSpecialization>(GetBot());
            break;
        default:
            _specialization = std::make_unique<ArmsSpecialization>(GetBot());
            break;
    }

    TC_LOG_DEBUG("playerbot.warrior", "WarriorAI switched to {} specialization for {}",
                 _specialization->GetSpecializationName(), GetBot()->GetName());
}

void WarriorAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization)
    {
        TC_LOG_ERROR("playerbot.warrior", "WarriorAI specialization not initialized for {}", GetBot()->GetName());
        return;
    }

    // Delegate rotation to current specialization
    _specialization->UpdateRotation(target);
}

// Override UpdateRotation to use specialization
void WarriorAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Update specialization if needed
    UpdateSpecialization();

    // Handle charge abilities first
    UseChargeAbilities(target);

    // Delegate to current specialization
    DelegateToSpecialization(target);
}

// Override UpdateBuffs to use specialization
void WarriorAI::UpdateBuffs()
{
    // Update shared warrior buffs
    UpdateWarriorBuffs();

    // Delegate specialization-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

// Override UpdateCooldowns to use specialization
void WarriorAI::UpdateCooldowns(uint32 diff)
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
bool WarriorAI::CanUseAbility(uint32 spellId)
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
void WarriorAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }
}

// Override OnCombatEnd to notify specialization
void WarriorAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }
}

// Override HasEnoughResource to use specialization
bool WarriorAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    // Fallback to base rage check
    return GetBot()->GetPower(POWER_RAGE) >= 10; // Basic rage check
}

// Override ConsumeResource to use specialization
void WarriorAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

// Override GetOptimalPosition to use specialization
Position WarriorAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    // Fallback to melee position
    return GetBot()->GetPosition();
}

// Override GetOptimalRange to use specialization
float WarriorAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    // Fallback to melee range
    return 5.0f;
}

// Modified constructor to initialize specialization
WarriorAI::WarriorAI(Player* bot)
    : ClassAI(bot)
    , _rageSpent(0)
    , _damageDealt(0)
    , _lastStanceChange(0)
    , _lastBattleShout(0)
    , _lastCommandingShout(0)
    , _needsIntercept(false)
    , _needsCharge(false)
    , _lastChargeTarget(nullptr)
    , _lastChargeTime(0)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot.warrior", "WarriorAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec));
}

} // namespace Playerbot