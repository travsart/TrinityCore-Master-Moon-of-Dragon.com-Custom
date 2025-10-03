/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DemonHunterAI.h"
#include "../BaselineRotationManager.h"
#include "HavocSpecialization_Enhanced.h"
#include "VengeanceSpecialization_Enhanced.h"
#include "HavocDemonHunterRefactored.h"
#include "VengeanceDemonHunterRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

DemonHunterAI::DemonHunterAI(Player* bot) : ClassAI(bot), _detectedSpec(DemonHunterSpec::HAVOC)
{
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbots", "DemonHunterAI initialized for player {} with basic implementation",
                 bot->GetName());
}

void DemonHunterAI::UpdateRotation(::Unit* target)
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

        // Fallback to basic melee attack if nothing else worked
        if (_bot->HasSpell(DEMONS_BITE) && CanUseAbility(DEMONS_BITE))
        {
            _bot->CastSpell(target, DEMONS_BITE, false);
        }
        return;
    }

    // Delegate to specialization for level 10+ bots with spec
    DelegateToSpecialization(target);
}

void DemonHunterAI::UpdateBuffs()
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

    // Use full Demon Hunter buff system for specialized bots
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
    if (!_bot || !_bot->HasSpell(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void DemonHunterAI::OnCombatStart(::Unit* target)
{
    if (!target)
        return;

    if (_specialization)
        _specialization->OnCombatStart(target);

    TC_LOG_DEBUG("module.playerbot.demonhunter", "DemonHunterAI combat started for player {}",
                 _bot->GetName());
}

void DemonHunterAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();

    TC_LOG_DEBUG("module.playerbot.demonhunter", "DemonHunterAI combat ended for player {}", _bot->GetName());
}

bool DemonHunterAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    return true;
}

void DemonHunterAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position DemonHunterAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return _bot->GetPosition();
}

float DemonHunterAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);

    return 5.0f; // Default melee range
}

DemonHunterSpec DemonHunterAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

// TODO: Implement advanced Demon Hunter functionality

// Placeholder stubs for methods referenced in header but not critical for compilation
void DemonHunterAI::DetectSpecialization()
{
    // TODO: Implement specialization detection
    _detectedSpec = DemonHunterSpec::HAVOC; // Default to Havoc
}

void DemonHunterAI::InitializeSpecialization()
{
    DetectSpecialization();
    SwitchSpecialization(_detectedSpec);
}

void DemonHunterAI::ExitMetamorphosis()
{
    // TODO: Implement metamorphosis exit logic
}

bool DemonHunterAI::ShouldUseMetamorphosis()
{
    // TODO: Implement metamorphosis decision logic
    return false;
}

void DemonHunterAI::CastMetamorphosisHavoc()
{
    // TODO: Implement Havoc metamorphosis
}

void DemonHunterAI::CastMetamorphosisVengeance()
{
    // TODO: Implement Vengeance metamorphosis
}

void DemonHunterAI::SpendPain(uint32 amount)
{
    // TODO: Implement pain spending
}

void DemonHunterAI::GeneratePain(uint32 amount)
{
    // TODO: Implement pain generation
}

bool DemonHunterAI::HasPain(uint32 amount)
{
    // TODO: Implement pain checking
    return true;
}

void DemonHunterAI::SpendFury(uint32 amount)
{
    // TODO: Implement fury spending
}

void DemonHunterAI::GenerateFury(uint32 amount)
{
    // TODO: Implement fury generation
}

bool DemonHunterAI::HasFury(uint32 amount)
{
    // TODO: Implement fury checking
    return true;
}

void DemonHunterAI::UpdatePainManagement(uint32 diff)
{
    // TODO: Implement pain management
}

void DemonHunterAI::DecayPain(uint32 diff)
{
    // TODO: Implement pain decay
}

uint32 DemonHunterAI::GetFury() const
{
    // TODO: Implement fury retrieval
    return 0;
}

uint32 DemonHunterAI::GetMaxFury() const
{
    // TODO: Implement max fury retrieval
    return 120;
}

void DemonHunterAI::UpdateHavocRotation(::Unit* target)
{
    // TODO: Implement Havoc rotation
}

void DemonHunterAI::UpdateVengeanceRotation(::Unit* target)
{
    // TODO: Implement Vengeance rotation
}

void DemonHunterAI::HandleMetamorphosisAbilities(::Unit* target)
{
    // TODO: Implement metamorphosis abilities
}

void DemonHunterAI::CastEyeBeam(::Unit* target)
{
    // TODO: Implement Eye Beam casting
}

void DemonHunterAI::CastChaosStrike(::Unit* target)
{
    // TODO: Implement Chaos Strike casting
}

void DemonHunterAI::CastBladeDance(::Unit* target)
{
    // TODO: Implement Blade Dance casting
}

void DemonHunterAI::CastDemonsBite(::Unit* target)
{
    // TODO: Implement Demon's Bite casting
}

void DemonHunterAI::CastSoulCleave(::Unit* target)
{
    // TODO: Implement Soul Cleave casting
}

void DemonHunterAI::CastShear(::Unit* target)
{
    // TODO: Implement Shear casting
}

std::vector<::Unit*> DemonHunterAI::GetAoETargets(float range)
{
    // TODO: Implement AoE target gathering
    return std::vector<::Unit*>();
}

void DemonHunterAI::SwitchSpecialization(DemonHunterSpec newSpec)
{
    _detectedSpec = newSpec;

    // TODO: Re-enable refactored specialization classes once template issues are fixed
    _specialization = nullptr;
    TC_LOG_WARN("module.playerbot.demonhunter", "DemonHunter specialization switching temporarily disabled for {}",
                 GetBot()->GetName());

    /* switch (newSpec)
    {
        case DemonHunterSpec::HAVOC:
            _specialization = std::make_unique<HavocDemonHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.demonhunter", "DemonHunter {} switched to Havoc specialization",
                         GetBot()->GetName());
            break;

        case DemonHunterSpec::VENGEANCE:
            _specialization = std::make_unique<VengeanceDemonHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.demonhunter", "DemonHunter {} switched to Vengeance specialization",
                         GetBot()->GetName());
            break;

        default:
            _specialization = std::make_unique<HavocDemonHunterRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.demonhunter", "DemonHunter {} defaulted to Havoc specialization",
                         GetBot()->GetName());
            break;
    } */
}

void DemonHunterAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

} // namespace Playerbot