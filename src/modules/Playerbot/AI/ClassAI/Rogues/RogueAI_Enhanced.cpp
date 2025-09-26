/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RogueAI.h"
#include "AssassinationSpecialization_Enhanced.h"
#include "CombatSpecialization_Enhanced.h"
#include "SubtletySpecialization_Enhanced.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

RogueAI::RogueAI(Player* bot) : ClassAI(bot), _detectedSpec(RogueSpec::ASSASSINATION)
{
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbots", "RogueAI initialized for player {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_detectedSpec));
}

void RogueAI::UpdateRotation(::Unit* target)
{
    if (!target || !_specialization)
        return;

    // Ensure we have proper energy management
    if (_bot->GetPower(POWER_ENERGY) < 20)
    {
        // Wait for energy regeneration
        return;
    }

    // Handle stealth management for all specs (most important for Subtlety)
    _specialization->UpdateStealthManagement();

    // Update specialization-specific rotation
    _specialization->UpdateRotation(target);

    // Handle combo point management
    _specialization->UpdateComboPointManagement();

    // Handle poison management (most important for Assassination)
    _specialization->UpdatePoisonManagement();

    // Handle debuff management
    _specialization->UpdateDebuffManagement();

    // Handle energy management
    _specialization->UpdateEnergyManagement();

    // Handle combat phase management
    _specialization->UpdateCombatPhase();
}

void RogueAI::UpdateBuffs()
{
    if (!_specialization)
        return;

    _specialization->UpdateBuffs();

    // Apply poisons if needed
    if (_detectedSpec == RogueSpec::ASSASSINATION)
    {
        _specialization->ApplyPoisons();
    }

    // Handle stealth buffs
    if (_detectedSpec == RogueSpec::SUBTLETY)
    {
        if (_specialization->ShouldEnterStealth() && !_bot->isInCombat())
        {
            if (_specialization->CanUseAbility(STEALTH))
            {
                _bot->CastSpell(_bot, STEALTH, false);
            }
        }
    }
}

void RogueAI::UpdateCooldowns(uint32 diff)
{
    if (!_specialization)
        return;

    _specialization->UpdateCooldowns(diff);
    _specialization->UpdateCooldownTracking(diff);
}

bool RogueAI::CanUseAbility(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->CanUseAbility(spellId);
}

void RogueAI::OnCombatStart(::Unit* target)
{
    if (!_specialization || !target)
        return;

    TC_LOG_DEBUG("playerbots", "RogueAI combat started for player {} against {}",
                 _bot->GetName(), target->GetName());

    _specialization->OnCombatStart(target);

    // Execute stealth opener if possible
    if (_bot->HasAura(STEALTH) || _bot->HasAura(VANISH_EFFECT))
    {
        _specialization->ExecuteStealthOpener(target);
    }

    // Assassination specific combat start
    if (_detectedSpec == RogueSpec::ASSASSINATION)
    {
        // Ensure poisons are applied
        _specialization->ApplyPoisons();
    }

    // Combat specific combat start
    if (_detectedSpec == RogueSpec::COMBAT)
    {
        // Activate Slice and Dice early if we have combo points
        if (_bot->GetComboPoints() >= 2)
        {
            if (_specialization->CanUseAbility(SLICE_AND_DICE))
            {
                _bot->CastSpell(_bot, SLICE_AND_DICE, false);
            }
        }
    }

    // Subtlety specific combat start
    if (_detectedSpec == RogueSpec::SUBTLETY)
    {
        // Plan stealth windows
        if (auto subSpec = dynamic_cast<SubtletySpecialization_Enhanced*>(_specialization.get()))
        {
            subSpec->ManageStealthWindowsOptimally();
        }
    }
}

void RogueAI::OnCombatEnd()
{
    if (!_specialization)
        return;

    TC_LOG_DEBUG("playerbots", "RogueAI combat ended for player {}", _bot->GetName());

    _specialization->OnCombatEnd();

    // Enter stealth after combat if possible
    if (_detectedSpec == RogueSpec::SUBTLETY)
    {
        if (_specialization->ShouldEnterStealth() && _specialization->CanUseAbility(STEALTH))
        {
            _bot->CastSpell(_bot, STEALTH, false);
        }
    }

    // Apply poisons if needed
    if (_detectedSpec == RogueSpec::ASSASSINATION)
    {
        _specialization->ApplyPoisons();
    }
}

bool RogueAI::HasEnoughResource(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->HasEnoughResource(spellId);
}

void RogueAI::ConsumeResource(uint32 spellId)
{
    if (!_specialization)
        return;

    _specialization->ConsumeResource(spellId);
}

Position RogueAI::GetOptimalPosition(::Unit* target)
{
    if (!_specialization || !target)
        return _bot->GetPosition();

    return _specialization->GetOptimalPosition(target);
}

float RogueAI::GetOptimalRange(::Unit* target)
{
    if (!_specialization || !target)
        return 5.0f; // Default melee range

    return _specialization->GetOptimalRange(target);
}

RogueSpec RogueAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

void RogueAI::DetectSpecialization()
{
    if (!_bot)
        return;

    // Check talent points in each tree to determine specialization
    uint32 assassinationPoints = 0;
    uint32 combatPoints = 0;
    uint32 subtletyPoints = 0;

    // Count talent points in each tree
    for (uint32 i = 0; i < MAX_TALENT_TABS; ++i)
    {
        for (uint32 j = 0; j < MAX_TALENT_RANK; ++j)
        {
            if (PlayerTalentMap::iterator itr = _bot->GetTalentMap(PLAYER_TALENT_SPEC_ACTIVE)->find(i * MAX_TALENT_RANK + j);
                itr != _bot->GetTalentMap(PLAYER_TALENT_SPEC_ACTIVE)->end())
            {
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(itr->second->talentId);
                if (!talentInfo)
                    continue;

                switch (talentInfo->TalentTab)
                {
                    case 0: assassinationPoints += itr->second->currentRank; break;
                    case 1: combatPoints += itr->second->currentRank; break;
                    case 2: subtletyPoints += itr->second->currentRank; break;
                }
            }
        }
    }

    // Determine specialization based on highest talent investment
    if (assassinationPoints >= combatPoints && assassinationPoints >= subtletyPoints)
        _detectedSpec = RogueSpec::ASSASSINATION;
    else if (combatPoints >= subtletyPoints)
        _detectedSpec = RogueSpec::COMBAT;
    else
        _detectedSpec = RogueSpec::SUBTLETY;

    TC_LOG_DEBUG("playerbots", "Rogue specialization detected: ASS({}) COM({}) SUB({}) -> {}",
                 assassinationPoints, combatPoints, subtletyPoints, static_cast<uint32>(_detectedSpec));
}

void RogueAI::InitializeSpecialization()
{
    if (!_bot)
        return;

    switch (_detectedSpec)
    {
        case RogueSpec::ASSASSINATION:
            _specialization = std::make_unique<AssassinationSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Assassination specialization for {}", _bot->GetName());
            break;

        case RogueSpec::COMBAT:
            _specialization = std::make_unique<CombatSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Combat specialization for {}", _bot->GetName());
            break;

        case RogueSpec::SUBTLETY:
            _specialization = std::make_unique<SubtletySpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Subtlety specialization for {}", _bot->GetName());
            break;

        default:
            TC_LOG_ERROR("playerbots", "Unknown rogue specialization for player {}: {}",
                         _bot->GetName(), static_cast<uint32>(_detectedSpec));
            _specialization = std::make_unique<AssassinationSpecialization_Enhanced>(_bot);
            break;
    }

    if (!_specialization)
    {
        TC_LOG_ERROR("playerbots", "Failed to initialize rogue specialization for player {}", _bot->GetName());
        return;
    }

    TC_LOG_INFO("playerbots", "Successfully initialized Rogue AI for player {} with {} specialization",
                _bot->GetName(),
                _detectedSpec == RogueSpec::ASSASSINATION ? "Assassination" :
                _detectedSpec == RogueSpec::COMBAT ? "Combat" : "Subtlety");
}

// Enhanced Rogue AI methods for advanced functionality

void RogueAI::HandleAdvancedStealthManagement()
{
    if (!_specialization)
        return;

    // Subtlety specialization gets advanced stealth management
    if (_detectedSpec == RogueSpec::SUBTLETY)
    {
        if (auto subSpec = dynamic_cast<SubtletySpecialization_Enhanced*>(_specialization.get()))
        {
            subSpec->ManageStealthWindowsOptimally();
            subSpec->OptimizeShadowstepPositioning(_bot->GetSelectedUnit());
            subSpec->ManageShadowDanceOptimally();
        }
    }
}

void RogueAI::HandleAdvancedPoisonManagement()
{
    if (!_specialization)
        return;

    // Assassination specialization gets advanced poison management
    if (_detectedSpec == RogueSpec::ASSASSINATION)
    {
        if (auto assSpec = dynamic_cast<AssassinationSpecialization_Enhanced*>(_specialization.get()))
        {
            assSpec->ManagePoisonStackingOptimally();
            assSpec->OptimizePoisonApplication();
            assSpec->CoordinatePoisonTypes();
        }
    }
}

void RogueAI::HandleAdvancedCombatManagement()
{
    if (!_specialization)
        return;

    // Combat specialization gets advanced weapon and burst management
    if (_detectedSpec == RogueSpec::COMBAT)
    {
        if (auto combatSpec = dynamic_cast<CombatSpecialization_Enhanced*>(_specialization.get()))
        {
            combatSpec->ManageWeaponSpecializationOptimally();
            combatSpec->ManageAdrenalineRushOptimally();
            combatSpec->ManageBladeFlurryIntelligently();
        }
    }
}

void RogueAI::HandleEmergencysituations()
{
    if (!_specialization)
        return;

    ::Unit* target = _bot->GetSelectedUnit();
    if (!target)
        return;

    // Check for low health emergency
    if (_bot->GetHealthPct() < 30.0f)
    {
        // Try to vanish if available
        if (_specialization->CanUseAbility(VANISH) && _specialization->ShouldEnterStealth())
        {
            _bot->CastSpell(_bot, VANISH, false);
            return;
        }

        // Try evasion if available
        if (_specialization->CanUseAbility(EVASION))
        {
            _bot->CastSpell(_bot, EVASION, false);
            return;
        }

        // Try sprint to escape
        if (_specialization->CanUseAbility(SPRINT))
        {
            _bot->CastSpell(_bot, SPRINT, false);
            return;
        }
    }

    // Handle multiple attackers
    std::vector<::Unit*> attackers;
    for (auto& threat : _bot->GetThreatManager().GetThreats())
    {
        if (::Unit* attacker = threat.getTarget())
        {
            if (attacker->IsInCombatWith(_bot))
                attackers.push_back(attacker);
        }
    }

    if (attackers.size() >= 3)
    {
        // Use AoE abilities if Combat spec
        if (_detectedSpec == RogueSpec::COMBAT)
        {
            if (_specialization->CanUseAbility(BLADE_FLURRY))
            {
                _bot->CastSpell(_bot, BLADE_FLURRY, false);
            }
        }

        // Use crowd control
        if (_specialization->CanUseAbility(BLIND))
        {
            // Find a secondary target to blind
            for (::Unit* attacker : attackers)
            {
                if (attacker != target)
                {
                    _bot->CastSpell(attacker, BLIND, false);
                    break;
                }
            }
        }
    }
}

void RogueAI::OptimizeRotationForTarget(::Unit* target)
{
    if (!_specialization || !target)
        return;

    // Specialization-specific optimization
    switch (_detectedSpec)
    {
        case RogueSpec::ASSASSINATION:
            if (auto assSpec = dynamic_cast<AssassinationSpecialization_Enhanced*>(_specialization.get()))
            {
                assSpec->ManageDoTsIntelligently();
                assSpec->ExecuteOptimalMutilateSequence(target);
                assSpec->OptimizeEnvenomTiming(target);
            }
            break;

        case RogueSpec::COMBAT:
            if (auto combatSpec = dynamic_cast<CombatSpecialization_Enhanced*>(_specialization.get()))
            {
                combatSpec->ManageSliceAndDiceOptimally();
                combatSpec->OptimizeComboPointGeneration();
                combatSpec->ManageRiposteOptimally();
            }
            break;

        case RogueSpec::SUBTLETY:
            if (auto subSpec = dynamic_cast<SubtletySpecialization_Enhanced*>(_specialization.get()))
            {
                subSpec->ManageHemorrhageOptimally();
                subSpec->OptimizeStealthOpenerSelection(target);
                subSpec->ExecutePerfectStealthSequence(target);
            }
            break;
    }
}

void RogueAI::HandleInterrupts(::Unit* target)
{
    if (!_specialization || !target)
        return;

    // Check if target is casting
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        // Use Kick if available
        if (_specialization->CanUseAbility(KICK) && target->IsInRange(_bot, 0.0f, 5.0f))
        {
            _bot->CastSpell(target, KICK, false);
            return;
        }

        // Use Gouge as backup interrupt
        if (_specialization->CanUseAbility(GOUGE) && target->IsInRange(_bot, 0.0f, 5.0f))
        {
            _bot->CastSpell(target, GOUGE, false);
            return;
        }

        // Use Cheap Shot from stealth
        if (_bot->HasAura(STEALTH) && _specialization->CanUseAbility(CHEAP_SHOT))
        {
            _bot->CastSpell(target, CHEAP_SHOT, false);
            return;
        }
    }
}

// Rogue-specific spell IDs
constexpr uint32 STEALTH = 1784;
constexpr uint32 VANISH = 1856;
constexpr uint32 VANISH_EFFECT = 11327;
constexpr uint32 SLICE_AND_DICE = 5171;
constexpr uint32 EVASION = 5277;
constexpr uint32 SPRINT = 2983;
constexpr uint32 BLADE_FLURRY = 13877;
constexpr uint32 BLIND = 2094;
constexpr uint32 KICK = 1766;
constexpr uint32 GOUGE = 1776;
constexpr uint32 CHEAP_SHOT = 1833;

} // namespace Playerbot