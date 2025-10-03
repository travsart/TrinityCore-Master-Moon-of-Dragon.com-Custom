/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DruidAI.h"
#include "BalanceDruidRefactored.h"
#include "FeralDruidRefactored.h"
#include "GuardianDruidRefactored.h"
#include "RestorationDruidRefactored.h"
#include "../BaselineRotationManager.h"
#include "Player.h"
#include "Log.h"

namespace Playerbot
{

DruidAI::DruidAI(Player* bot) : ClassAI(bot), _detectedSpec(DruidSpec::BALANCE)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot", "DruidAI initialized for {}", bot->GetName());
}

void DruidAI::InitializeSpecialization()
{
    DetectSpecialization();
    SwitchSpecialization(_detectedSpec);
}

void DruidAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
    {
        _detectedSpec = DruidSpec::BALANCE;
        return;
    }

    // Check for Guardian specialization indicators
    if (bot->HasSpell(33917) || bot->HasSpell(77758)) // Mangle (Bear) or Thrash
        _detectedSpec = DruidSpec::GUARDIAN;
    // Check for Feral specialization indicators
    else if (bot->HasSpell(5221) || bot->HasSpell(22568)) // Shred or Ferocious Bite
        _detectedSpec = DruidSpec::FERAL;
    // Check for Restoration specialization indicators
    else if (bot->HasSpell(18562) || bot->HasSpell(33763)) // Swiftmend or Lifebloom
        _detectedSpec = DruidSpec::RESTORATION;
    // Default to Balance
    else
        _detectedSpec = DruidSpec::BALANCE;
}

void DruidAI::SwitchSpecialization(DruidSpec newSpec)
{
    _detectedSpec = newSpec;

    // TODO: Re-enable refactored specialization classes once template issues are fixed
    _specialization = nullptr;
    TC_LOG_WARN("module.playerbot.druid", "Druid specialization switching temporarily disabled for {}",
                 GetBot()->GetName());

    /* switch (newSpec)
    {
        case DruidSpec::BALANCE:
            _specialization = std::make_unique<BalanceDruidRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.druid", "Druid {} switched to Balance specialization",
                         GetBot()->GetName());
            break;
        case DruidSpec::FERAL:
            _specialization = std::make_unique<FeralDruidRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.druid", "Druid {} switched to Feral specialization",
                         GetBot()->GetName());
            break;
        case DruidSpec::GUARDIAN:
            _specialization = std::make_unique<GuardianDruidRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.druid", "Druid {} switched to Guardian specialization",
                         GetBot()->GetName());
            break;
        case DruidSpec::RESTORATION:
            _specialization = std::make_unique<RestorationDruidRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.druid", "Druid {} switched to Restoration specialization",
                         GetBot()->GetName());
            break;
        default:
            _specialization = std::make_unique<BalanceDruidRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.druid", "Druid {} defaulted to Balance specialization",
                         GetBot()->GetName());
            break;
    } */
}

void DruidAI::DelegateToSpecialization(::Unit* target)
{
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void DruidAI::UpdateRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(GetBot());

        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))
            return;

        // Fallback: basic melee or ranged attack based on form
        if (!GetBot()->IsNonMeleeSpellCast(false))
        {
            float distance = GetBot()->GetDistance(target);
            if (distance <= 5.0f || GetBot()->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT)) // In melee or shapeshifted
            {
                GetBot()->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // Delegate to specialization for level 10+ bots with spec
    DelegateToSpecialization(target);
}

void DruidAI::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(bot))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(bot);
        return;
    }

    // Delegate to specialization for buffs
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
    if (!GetBot() || !IsSpellReady(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void DruidAI::OnCombatStart(::Unit* target)
{
    if (!target || !GetBot())
        return;

    TC_LOG_DEBUG("playerbot", "DruidAI {} entering combat with {}",
                 GetBot()->GetName(), target->GetName());

    _inCombat = true;
    _currentTarget = target->GetGUID();
    _combatTime = 0;

    if (_specialization)
        _specialization->OnCombatStart(target);
}

void DruidAI::OnCombatEnd()
{
    _inCombat = false;
    _currentTarget = ObjectGuid::Empty;
    _combatTime = 0;

    TC_LOG_DEBUG("playerbot", "DruidAI {} leaving combat", GetBot()->GetName());

    if (_specialization)
        _specialization->OnCombatEnd();
}

bool DruidAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    if (_specialization)
        return _specialization->HasEnoughResource(spellId);

    // Fallback: check if we have any energy/mana/rage
    return GetBot()->GetPower(POWER_ENERGY) >= 10 ||
           GetBot()->GetPower(POWER_MANA) >= 10 ||
           GetBot()->GetPower(POWER_RAGE) >= 10;
}

void DruidAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position DruidAI::GetOptimalPosition(::Unit* target)
{
    if (!GetBot() || !target)
        return GetBot() ? GetBot()->GetPosition() : Position();

    if (_specialization)
        return _specialization->GetOptimalPosition(target);

    return GetBot()->GetPosition();
}

float DruidAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);

    return 25.0f; // Default druid range
}

} // namespace Playerbot