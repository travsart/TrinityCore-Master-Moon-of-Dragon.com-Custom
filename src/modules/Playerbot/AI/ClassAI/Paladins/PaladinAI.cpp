/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PaladinAI.h"
#include "HolyPaladinRefactored.h"
#include "ProtectionPaladinRefactored.h"
#include "RetributionSpecializationRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"
#include "../BaselineRotationManager.h"

namespace Playerbot
{

PaladinAI::PaladinAI(Player* bot) : ClassAI(bot), _currentSpec(PaladinSpec::RETRIBUTION)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("module.playerbot.paladin", "PaladinAI initialized for {}", bot->GetName());
}

void PaladinAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(_bot);

        if (baselineManager.ExecuteBaselineRotation(_bot, target))
            return;

        // Fallback: basic auto-attack
        if (!_bot->IsNonMeleeSpellCast(false))
        {
            if (_bot->GetDistance(target) <= 5.0f)
            {
                _bot->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // Specialized rotation for level 10+ with spec
    DelegateToSpecialization(target);
}

void PaladinAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(_bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(_bot);
        return;
    }

    // Specialized buff management for level 10+ with spec
    if (_specialization)
        _specialization->UpdateBuffs();
}

void PaladinAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool PaladinAI::CanUseAbility(uint32 spellId)
{
    if (!_bot || !IsSpellReady(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return HasEnoughResource(spellId);
}

void PaladinAI::OnCombatStart(::Unit* target)
{
    if (!target || !_bot)
        return;

    if (_specialization)
        _specialization->OnCombatStart(target);

    TC_LOG_DEBUG("module.playerbot.paladin", "PaladinAI {} entering combat with {}",
                 _bot->GetName(), target->GetName());

    _inCombat = true;
    _currentTarget = target->GetGUID();
    _combatTime = 0;
}

void PaladinAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();

    _inCombat = false;
    _currentTarget = ObjectGuid::Empty;
    _combatTime = 0;

    TC_LOG_DEBUG("module.playerbot.paladin", "PaladinAI {} leaving combat", _bot->GetName());
}

bool PaladinAI::HasEnoughResource(uint32 spellId)
{
    if (!_bot)
        return false;

    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    return _bot->GetPower(POWER_MANA) >= 100; // Simple placeholder
}

void PaladinAI::ConsumeResource(uint32 spellId)
{
    if (!_bot)
        return;

    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position PaladinAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !_bot)
        return Position();

    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    // Simple melee positioning fallback
    float angle = target->GetOrientation();
    float x = target->GetPositionX() + cos(angle) * 5.0f;
    float y = target->GetPositionY() + sin(angle) * 5.0f;
    float z = target->GetPositionZ();

    return Position(x, y, z, angle);
}

float PaladinAI::GetOptimalRange(::Unit* target)
{
    if (!target)
        return 5.0f;

    if (_specialization)
        return _specialization->GetOptimalRange(target);

    return 5.0f; // Default melee range
}

void PaladinAI::DetectSpecialization()
{
    // Detect from talents - default to Retribution for now
    _currentSpec = PaladinSpec::RETRIBUTION;
}

void PaladinAI::InitializeSpecialization()
{
    DetectSpecialization();
    SwitchSpecialization(_currentSpec);
}

void PaladinAI::SwitchSpecialization(PaladinSpec newSpec)
{
    _currentSpec = newSpec;

    // TODO: Re-enable refactored specialization classes once template issues are fixed
    _specialization = nullptr;
    TC_LOG_WARN("module.playerbot.paladin", "Paladin specialization switching temporarily disabled for {}",
                 GetBot()->GetName());

    /* switch (newSpec)
    {
        case PaladinSpec::HOLY:
            _specialization = std::make_unique<HolyPaladinRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.paladin", "Paladin {} switched to Holy specialization",
                         GetBot()->GetName());
            break;

        case PaladinSpec::PROTECTION:
            _specialization = std::make_unique<ProtectionPaladinRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.paladin", "Paladin {} switched to Protection specialization",
                         GetBot()->GetName());
            break;

        case PaladinSpec::RETRIBUTION:
            _specialization = std::make_unique<RetributionPaladinRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.paladin", "Paladin {} switched to Retribution specialization",
                         GetBot()->GetName());
            break;

        default:
            _specialization = std::make_unique<RetributionPaladinRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.paladin", "Paladin {} defaulted to Retribution specialization",
                         GetBot()->GetName());
            break;
    } */
}

void PaladinAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

} // namespace Playerbot