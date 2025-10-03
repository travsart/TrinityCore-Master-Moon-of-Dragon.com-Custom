/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PriestAI.h"
#include "DisciplinePriestRefactored.h"
#include "HolyPriestRefactored.h"
#include "ShadowPriestRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Group.h"

// Common Priest spell constants
constexpr uint32 INNER_FIRE = 588;
constexpr uint32 POWER_WORD_FORTITUDE = 21562; // Updated for WoW 11.2
constexpr uint32 PENANCE = 47540;
constexpr uint32 PAIN_SUPPRESSION = 33206;
constexpr uint32 CIRCLE_OF_HEALING = 34861;
constexpr uint32 GUARDIAN_SPIRIT = 47788;

namespace Playerbot
{

void PriestAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void PriestAI::UpdateSpecialization()
{
    PriestSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        TC_LOG_DEBUG("playerbot.priest", "PriestAI specialization changed from {} to {} for {}",
                     static_cast<uint32>(_currentSpec), static_cast<uint32>(detectedSpec), GetBot()->GetName());
        SwitchSpecialization(detectedSpec);
    }
}

PriestSpec PriestAI::DetectCurrentSpecialization()
{
    // Detect based on primary talents or highest skilled spec
    Player* bot = GetBot();
    if (!bot)
        return PriestSpec::HOLY; // Default fallback

    // Check for key specialization spells or talents
    // This is a simplified detection - real implementation would check talent trees

    // Check for Shadow specialization indicators
    // Using direct spell IDs: 15473 (Shadowform rank 1), 232698 (Shadowform WoW 11.2), 15407 (Mind Flay), 34914 (Vampiric Touch)
    if (bot->HasSpell(15473) || bot->HasSpell(232698) || bot->HasSpell(15407) || bot->HasSpell(34914))
        return PriestSpec::SHADOW;

    // Check for Discipline specialization indicators
    if (bot->HasSpell(PENANCE) || bot->HasSpell(PAIN_SUPPRESSION))
        return PriestSpec::DISCIPLINE;

    // Check for Holy specialization indicators
    if (bot->HasSpell(CIRCLE_OF_HEALING) || bot->HasSpell(GUARDIAN_SPIRIT))
        return PriestSpec::HOLY;

    // Default to Holy if no clear specialization detected
    return PriestSpec::HOLY;
}

void PriestAI::SwitchSpecialization(PriestSpec newSpec)
{
    _currentSpec = newSpec;

    // TODO: Re-enable refactored specialization classes once template issues are fixed
    _specialization = nullptr;
    TC_LOG_WARN("module.playerbot.priest", "Priest specialization switching temporarily disabled for {}",
                 GetBot()->GetName());

    /* // Create appropriate specialization instance
    switch (newSpec)
    {
        case PriestSpec::DISCIPLINE:
            _specialization = std::make_unique<DisciplinePriestRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.priest", "Priest {} switched to Discipline specialization",
                         GetBot()->GetName());
            break;
        case PriestSpec::HOLY:
            _specialization = std::make_unique<HolyPriestRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.priest", "Priest {} switched to Holy specialization",
                         GetBot()->GetName());
            break;
        case PriestSpec::SHADOW:
            _specialization = std::make_unique<ShadowPriestRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.priest", "Priest {} switched to Shadow specialization",
                         GetBot()->GetName());
            break;
        default:
            _specialization = std::make_unique<HolyPriestRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.priest", "Priest {} defaulted to Holy specialization",
                         GetBot()->GetName());
            break;
    } */
}

void PriestAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization)
    {
        TC_LOG_ERROR("playerbot.priest", "PriestAI specialization not initialized for {}", GetBot()->GetName());
        return;
    }

    // Delegate rotation to current specialization
    _specialization->UpdateRotation(target);
}

// Override UpdateRotation to use specialization
void PriestAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Update specialization if needed
    UpdateSpecialization();

    // Delegate to current specialization
    DelegateToSpecialization(target);
}

// Override UpdateBuffs to use specialization
void PriestAI::UpdateBuffs()
{
    // Update shared priest buffs
    UpdatePriestBuffs();

    // Delegate specialization-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

// Override UpdateCooldowns to use specialization
void PriestAI::UpdateCooldowns(uint32 diff)
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
bool PriestAI::CanUseAbility(uint32 spellId)
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
void PriestAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }
}

// Override OnCombatEnd to notify specialization
void PriestAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }
}

// Override HasEnoughResource to use specialization
bool PriestAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    // Fallback to base mana check
    return HasEnoughMana(100); // Basic mana check
}

// Override ConsumeResource to use specialization
void PriestAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

// Override GetOptimalPosition to use specialization
Position PriestAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    // Fallback to base position
    return GetBot()->GetPosition();
}

// Override GetOptimalRange to use specialization
float PriestAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    // Fallback to default healing range
    return 40.0f;
}

// Modified constructor to initialize specialization
PriestAI::PriestAI(Player* bot)
    : ClassAI(bot)
    , _manaSpent(0)
    , _healingDone(0)
    , _damageDealt(0)
    , _playersHealed(0)
    , _damagePrevented(0)
    , _lastDispel(0)
    , _lastFearWard(0)
    , _lastPsychicScream(0)
    , _lastInnerFire(0)
{
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot.priest", "PriestAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec));
}

// Destructor implementation
PriestAI::~PriestAI() = default;

// Implementation of missing methods that were in PriestAI.cpp
bool PriestAI::HasEnoughMana(uint32 amount)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->GetPower(POWER_MANA) >= amount;
}

void PriestAI::UpdatePriestBuffs()
{
    // Update shared priest buffs like Inner Fire, Power Word: Fortitude
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    // Check for Inner Fire
    if (!HasAura(INNER_FIRE) && CanUseAbility(INNER_FIRE))
    {
        CastSpell(INNER_FIRE);
    }

    // Check for Power Word: Fortitude on self and group members
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->IsAlive() && !member->HasAura(POWER_WORD_FORTITUDE) &&
                    bot->GetDistance(member) <= 40.0f && CanUseAbility(POWER_WORD_FORTITUDE))
                {
                    CastSpell(member, POWER_WORD_FORTITUDE);
                    break; // Cast one at a time
                }
            }
        }
    }
}

} // namespace Playerbot