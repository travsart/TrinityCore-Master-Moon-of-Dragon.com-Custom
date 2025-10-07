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
#include "ObjectAccessor.h"
#include "Timer.h"

namespace Playerbot
{

RogueAI::RogueAI(Player* bot)
    : ClassAI(bot)
    , _specialization(nullptr)
    , _detectedSpec(RogueSpec::ASSASSINATION)
    , _energySpent(0)
    , _comboPointsUsed(0)
    , _stealthsUsed(0)
    , _lastStealth(0)
    , _lastVanish(0)
{
    InitializeCombatSystems();
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbot.rogue", "RogueAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_detectedSpec));
}

// Destructor implementation
RogueAI::~RogueAI() = default;

void RogueAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Update specialization if needed
    if (_specialization)
    {
        _specialization->UpdateRotation(target);
    }
    else
    {
        // Fallback rotation if specialization is not available
        ExecuteFallbackRotation(target);
    }

    // Handle stealth situations
    if (!_inCombat && !GetBot()->HasAura(1784)) // Not in stealth
    {
        ConsiderStealth();
    }
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

void RogueAI::InitializeCombatSystems()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Initialize combat system components - skip for now to avoid constructor issues
    // _threatManager = std::make_unique<BotThreatManager>(bot);
    // _targetSelector = std::make_unique<TargetSelector>(bot);
    // _positionManager = std::make_unique<PositionManager>(bot);
    // _interruptManager = std::make_unique<InterruptManager>(bot);
    // _cooldownManager = std::make_unique<CooldownManager>(bot);

    // Initialize Rogue-specific systems - skip for now
    // _combatPositioning = std::make_unique<RogueCombatPositioning>(bot);
    // _energyManager = std::make_unique<EnergyManager>(bot);

    TC_LOG_DEBUG("playerbot.rogue", "Combat systems initialized for {}", bot->GetName());
}

void RogueAI::ExecuteFallbackRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();
    uint32 comboPoints = GetComboPoints(); // Use our own method

    // Use finishers if we have enough combo points
    if (comboPoints >= 5)
    {
        if (bot->HasSpell(1329) && IsSpellReady(1329)) // Mutilate/Eviscerate
        {
            CastSpell(target, 1329);
            return;
        }
    }

    // Build combo points
    if (bot->HasSpell(1752) && IsSpellReady(1752)) // Sinister Strike
    {
        CastSpell(target, 1752);
    }
    else if (bot->HasSpell(1757) && IsSpellReady(1757)) // Sinister Strike (rank 2)
    {
        CastSpell(target, 1757);
    }
}

void RogueAI::ConsiderStealth()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Use stealth if available and not in combat
    if (bot->HasSpell(1784) && IsSpellReady(1784) &&
        (getMSTime() - _lastStealth) > 10000) // 10 second cooldown
    {
        CastSpell(1784); // Stealth
        _lastStealth = getMSTime();
        _stealthsUsed++;
    }
}

void RogueAI::ActivateBurstCooldowns(::Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Use major cooldowns based on specialization
    switch (_detectedSpec)
    {
        case RogueSpec::ASSASSINATION:
            if (bot->HasSpell(14177) && IsSpellReady(14177)) // Cold Blood
                CastSpell(14177);
            break;
        case RogueSpec::COMBAT:
            if (bot->HasSpell(13750) && IsSpellReady(13750)) // Adrenaline Rush
                CastSpell(13750);
            break;
        case RogueSpec::SUBTLETY:
            if (bot->HasSpell(36554) && IsSpellReady(36554)) // Shadowstep
                CastSpell(target, 36554);
            break;
    }

    // Universal cooldowns
    if (bot->HasSpell(5277) && IsSpellReady(5277)) // Evasion
    {
        if (bot->GetHealthPct() < 30.0f)
            CastSpell(5277);
    }
}

bool RogueAI::HasEnoughEnergy(uint32 amount)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->GetPower(POWER_ENERGY) >= amount;
}

uint32 RogueAI::GetEnergy()
{
    Player* bot = GetBot();
    return bot ? bot->GetPower(POWER_ENERGY) : 0;
}

uint32 RogueAI::GetComboPoints()
{
    Player* bot = GetBot();
    if (!bot || !_currentTarget)
        return 0;

    // Simple fallback - assume we have combo points if we've been attacking
    // In a real implementation, this would check the TrinityCore combo point system
    // For now, return a reasonable estimate based on combat time
    if (_inCombat && _combatTime > 5000) // 5 seconds in combat
        return std::min(5u, _combatTime / 2000); // Gain combo points over time

    return 0;
}

void RogueAI::OnTargetChanged(Unit* newTarget)
{
    // Handle target change for Rogue - reset combo points and stealth tracking
    _currentTarget = newTarget ? newTarget->GetGUID() : ObjectGuid::Empty;

    if (_specialization)
    {
        // Reset combo point tracking when target changes
        // This allows specialization to start fresh rotation on new target
    }
}

} // namespace Playerbot