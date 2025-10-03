/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HunterAI.h"
#include "BeastMasteryHunterRefactored.h"
#include "MarksmanshipHunterRefactored.h"
#include "SurvivalHunterRefactored.h"
#include "../BaselineRotationManager.h"
#include "Player.h"
#include "Pet.h"
#include "Group.h"
#include "SpellMgr.h"
#include "WorldSession.h"
#include "ObjectAccessor.h"
#include "Log.h"

namespace Playerbot
{

HunterAI::HunterAI(Player* bot) :
    ClassAI(bot),
    _detectedSpec(HunterSpec::BEAST_MASTERY)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot", "HunterAI initialized for {}", bot->GetName());
}

void HunterAI::InitializeCombatSystems()
{
    // TODO: Implement combat systems initialization
}

void HunterAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
    {
        _detectedSpec = HunterSpec::BEAST_MASTERY;
        return;
    }

    // Check for Marksmanship specialization indicators
    if (bot->HasSpell(19434) || bot->HasSpell(53209)) // Aimed Shot or Chimera Shot
        _detectedSpec = HunterSpec::MARKSMANSHIP;
    // Check for Survival specialization indicators
    else if (bot->HasSpell(3674) || bot->HasSpell(60053)) // Black Arrow or Explosive Shot
        _detectedSpec = HunterSpec::SURVIVAL;
    // Default to Beast Mastery
    else
        _detectedSpec = HunterSpec::BEAST_MASTERY;
}

void HunterAI::InitializeSpecialization()
{
    DetectSpecialization();
    SwitchSpecialization(_detectedSpec);
}

void HunterAI::SwitchSpecialization(HunterSpec newSpec)
{
    _detectedSpec = newSpec;

    switch (newSpec)
    {
        case HunterSpec::BEAST_MASTERY:
            _specialization = std::make_unique<BeastMasteryHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.hunter", "Hunter {} switched to Beast Mastery specialization",
                         GetBot()->GetName());
            break;
        case HunterSpec::MARKSMANSHIP:
            _specialization = std::make_unique<MarksmanshipHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.hunter", "Hunter {} switched to Marksmanship specialization",
                         GetBot()->GetName());
            break;
        case HunterSpec::SURVIVAL:
            _specialization = std::make_unique<SurvivalHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.hunter", "Hunter {} switched to Survival specialization",
                         GetBot()->GetName());
            break;
        default:
            _specialization = std::make_unique<BeastMasteryHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.hunter", "Hunter {} defaulted to Beast Mastery specialization",
                         GetBot()->GetName());
            break;
    }
}

void HunterAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void HunterAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        // Use baseline rotation manager for unspecialized bots
        static BaselineRotationManager baselineManager;

        // Try auto-specialization if level 10+
        baselineManager.HandleAutoSpecialization(_bot);

        // Execute baseline rotation
        if (baselineManager.ExecuteBaselineRotation(_bot, target))
            return;

        // Fallback to basic ranged attack if nothing else worked
        if (_bot->HasSpell(ARCANE_SHOT) && CanUseAbility(ARCANE_SHOT))
        {
            _bot->CastSpell(target, ARCANE_SHOT, false);
        }
        return;
    }

    // Delegate to specialization for level 10+ bots with spec
    DelegateToSpecialization(target);
}

void HunterAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Check if bot should use baseline buffs
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(_bot);
        return;
    }

    // Use full hunter buff system for specialized bots
    ManageAspects();

    if (_specialization)
        _specialization->UpdateBuffs();
}

void HunterAI::UpdateCooldowns(uint32 diff)
{
    // UpdateCooldowns is provided by template base classes as 'final override'
    // The specialization handles its own cooldowns internally
    // No need to delegate here - just call base class implementation
    ClassAI::UpdateCooldowns(diff);
}

bool HunterAI::CanUseAbility(uint32 spellId)
{
    // CanUseAbility is provided by template base classes as 'final override'
    // Just call base class implementation
    return ClassAI::CanUseAbility(spellId);
}

void HunterAI::OnCombatStart(::Unit* target)
{
    _inCombat = true;
    _currentTarget = target ? target->GetGUID() : ObjectGuid::Empty;
    _combatTime = 0;

    // OnCombatStart is provided by template base classes as 'final override'
    ClassAI::OnCombatStart(target);
}

void HunterAI::OnCombatEnd()
{
    _inCombat = false;
    _currentTarget = ObjectGuid::Empty;
    _combatTime = 0;

    // OnCombatEnd is provided by template base classes as 'final override'
    ClassAI::OnCombatEnd();
}

bool HunterAI::HasEnoughResource(uint32 spellId)
{
    if (!_bot)
        return false;

    // Check focus cost - hunters use focus as primary resource
    uint32 focusCost = 10; // Default cost, should look up actual spell cost
    return _bot->GetPower(POWER_FOCUS) >= focusCost;
}

void HunterAI::ConsumeResource(uint32 spellId)
{
    // ConsumeResource implementation - hunters use focus
    // The actual resource consumption is handled by TrinityCore's spell system
    // This is just a notification hook for tracking purposes
    if (!_bot)
        return;

    // Note: Template base classes in refactored specs handle this automatically
    // This implementation is for the legacy HunterAI wrapper
}

Position HunterAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return _bot->GetPosition();
}

float HunterAI::GetOptimalRange(::Unit* target)
{
    // GetOptimalRange is provided by template base classes (RangedDpsSpecialization, etc.)
    // All hunter specs use 25.0f-35.0f range based on their spec
    // No need to delegate to specialization - this is the default implementation
    return 25.0f; // Default hunter range
}

HunterSpec HunterAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

// Private helper methods
void HunterAI::ManageAspects()
{
    if (!_bot)
        return;

    // Basic aspect management - ensure combat aspect is active
    if (_bot->HasSpell(ASPECT_OF_THE_HAWK) && !HasAura(ASPECT_OF_THE_HAWK))
    {
        CastSpell(ASPECT_OF_THE_HAWK);
    }
}

void HunterAI::HandlePositioning(::Unit* target)
{
    // TODO: Implement positioning logic
}

void HunterAI::HandleSharedAbilities(::Unit* target)
{
    // TODO: Implement shared abilities
}

void HunterAI::UpdateTracking()
{
    // TODO: Implement tracking management
}

bool HunterAI::HasAnyAspect()
{
    return HasAura(ASPECT_OF_THE_HAWK);
}

uint32 HunterAI::GetCurrentAspect()
{
    if (HasAura(ASPECT_OF_THE_HAWK)) return ASPECT_OF_THE_HAWK;
    return 0;
}

void HunterAI::SwitchToCombatAspect()
{
    // TODO: Implement combat aspect switching
}

bool HunterAI::ValidateAspectForAbility(uint32 spellId)
{
    return true;
}

Player* HunterAI::GetMainTank()
{
    // TODO: Implement tank detection
    return nullptr;
}

void HunterAI::LogCombatMetrics()
{
    // TODO: Implement combat metrics logging
}

} // namespace Playerbot