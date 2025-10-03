/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DeathKnightAI.h"
#include "BloodDeathKnightRefactored.h"
#include "FrostDeathKnightRefactored.h"
#include "UnholyDeathKnightRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

DeathKnightAI::DeathKnightAI(Player* bot)
    : ClassAI(bot)
    , _specialization(nullptr)
    , _detectedSpec(DeathKnightSpec::BLOOD)
    , _runicPowerSpent(0)
    , _runesUsed(0)
    , _diseasesApplied(0)
    , _lastPresence(0)
    , _lastHorn(0)
{
    InitializeCombatSystems();
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbot.deathknight", "DeathKnightAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_detectedSpec));
}

// Destructor implementation
DeathKnightAI::~DeathKnightAI() = default;

void DeathKnightAI::UpdateRotation(::Unit* target)
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

    // Handle burst situations
    if (_inCombat && target->GetHealthPct() > 80.0f)
    {
        ActivateBurstCooldowns(target);
    }
}

void DeathKnightAI::UpdateBuffs()
{
    if (_specialization)
        _specialization->UpdateBuffs();
}

void DeathKnightAI::UpdateCooldowns(uint32 diff)
{
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool DeathKnightAI::CanUseAbility(uint32 spellId)
{
    if (_specialization)
        return _specialization->CanUseAbility(spellId);
    return false;
}

void DeathKnightAI::OnCombatStart(::Unit* target)
{
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void DeathKnightAI::OnCombatEnd()
{
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool DeathKnightAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return false;
}

void DeathKnightAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position DeathKnightAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return Position();
}

float DeathKnightAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 5.0f;
}

void DeathKnightAI::DetectSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Detect specialization based on talents and spells
    DeathKnightSpec detected = DeathKnightSpec::BLOOD; // Default

    // Check for specialization-specific spells or talents
    if (bot->HasSpell(55050) || bot->HasSpell(55233) || bot->HasSpell(195181)) // HEART_STRIKE, VAMPIRIC_BLOOD, BONE_SHIELD (Updated for WoW 11.2)
    {
        detected = DeathKnightSpec::BLOOD;
    }
    else if (bot->HasSpell(49020) || bot->HasSpell(49143) || bot->HasSpell(49184)) // OBLITERATE, FROST_STRIKE, HOWLING_BLAST
    {
        detected = DeathKnightSpec::FROST;
    }
    else if (bot->HasSpell(55090) || bot->HasSpell(49206) || bot->HasSpell(42650)) // SCOURGE_STRIKE, SUMMON_GARGOYLE, ARMY_OF_THE_DEAD
    {
        detected = DeathKnightSpec::UNHOLY;
    }
    else if (bot->HasSpell(48266)) // BLOOD_PRESENCE
    {
        detected = DeathKnightSpec::BLOOD;
    }
    else if (bot->HasSpell(48263)) // FROST_PRESENCE
    {
        detected = DeathKnightSpec::FROST;
    }
    else if (bot->HasSpell(48265)) // UNHOLY_PRESENCE
    {
        detected = DeathKnightSpec::UNHOLY;
    }

    // Store the detected specialization for use in InitializeSpecialization
    _detectedSpec = detected;
}

void DeathKnightAI::InitializeSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    DeathKnightSpec spec = GetCurrentSpecialization();

    switch (spec)
    {
        case DeathKnightSpec::BLOOD:
            _specialization = std::make_unique<BloodDeathKnightRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.deathknight", "DeathKnight {} switched to Blood specialization", bot->GetName());
            break;
        case DeathKnightSpec::FROST:
            _specialization = std::make_unique<FrostDeathKnightRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.deathknight", "DeathKnight {} switched to Frost specialization", bot->GetName());
            break;
        case DeathKnightSpec::UNHOLY:
            _specialization = std::make_unique<UnholyDeathKnightRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.deathknight", "DeathKnight {} switched to Unholy specialization", bot->GetName());
            break;
        default:
            _specialization = std::make_unique<BloodDeathKnightRefactored>(bot);
            TC_LOG_DEBUG("module.playerbot.deathknight", "DeathKnight {} defaulted to Blood specialization", bot->GetName());
            break;
    }
}

DeathKnightSpec DeathKnightAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

void DeathKnightAI::InitializeCombatSystems()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Skip combat system components initialization for now
    // These managers may not have the expected constructors
    // _threatManager = std::make_unique<BotThreatManager>(bot);
    // _targetSelector = std::make_unique<TargetSelector>(bot);

    // Skip problematic managers for now - focus on core functionality
    // _positionManager = std::make_unique<PositionManager>(bot);
    // _interruptManager = std::make_unique<InterruptManager>(bot);
    // _cooldownManager = std::make_unique<CooldownManager>(bot);

    // Initialize Death Knight-specific systems - skip for now
    // _diseaseManager = std::make_unique<DiseaseManager>(bot);

    TC_LOG_DEBUG("playerbot.deathknight", "Combat systems initialized for {}", bot->GetName());
}

void DeathKnightAI::ExecuteFallbackRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Basic Death Knight rotation as fallback
    Player* bot = GetBot();

    // Apply diseases if missing
    if (!target->HasAura(55078) && bot->HasSpell(55050)) // Icy Touch -> Frost Fever
    {
        CastSpell(target, 45477); // Icy Touch
        return;
    }

    if (!target->HasAura(55095) && bot->HasSpell(49998)) // Death Grip -> Blood Plague
    {
        CastSpell(target, 49998); // Death Coil
        return;
    }

    // Basic attack rotation
    if (bot->HasSpell(55050) && IsSpellReady(55050)) // Heart Strike
    {
        CastSpell(target, 55050);
    }
    else if (bot->HasSpell(49020) && IsSpellReady(49020)) // Obliterate
    {
        CastSpell(target, 49020);
    }
    else if (bot->HasSpell(49143) && IsSpellReady(49143)) // Frost Strike
    {
        CastSpell(target, 49143);
    }
}

void DeathKnightAI::ActivateBurstCooldowns(::Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Use major cooldowns based on specialization
    switch (_detectedSpec)
    {
        case DeathKnightSpec::BLOOD:
            if (bot->HasSpell(55233) && IsSpellReady(55233)) // Vampiric Blood
                CastSpell(55233);
            break;
        case DeathKnightSpec::FROST:
            if (bot->HasSpell(51271) && IsSpellReady(51271)) // Pillar of Frost
                CastSpell(51271);
            break;
        case DeathKnightSpec::UNHOLY:
            if (bot->HasSpell(49206) && IsSpellReady(49206)) // Summon Gargoyle
                CastSpell(49206);
            break;
    }

    // Universal cooldowns
    if (bot->HasSpell(48792) && IsSpellReady(48792)) // Icebound Fortitude
    {
        if (bot->GetHealthPct() < 50.0f)
            CastSpell(48792);
    }
}

} // namespace Playerbot