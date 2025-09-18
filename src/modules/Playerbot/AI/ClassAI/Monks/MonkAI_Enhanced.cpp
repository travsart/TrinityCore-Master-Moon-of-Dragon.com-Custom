/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MonkAI.h"
#include "BrewmasterSpecialization_Enhanced.h"
#include "MistweaverSpecialization_Enhanced.h"
#include "WindwalkerSpecialization_Enhanced.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

MonkAI::MonkAI(Player* bot) : ClassAI(bot), _detectedSpec(MonkSpec::WINDWALKER)
{
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbots", "MonkAI initialized for player {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_detectedSpec));
}

void MonkAI::UpdateRotation(::Unit* target)
{
    if (!target || !_specialization)
        return;

    // Ensure we have proper resource management
    if (_bot->GetPower(POWER_ENERGY) < 20 && _bot->GetPower(POWER_CHI) == 0)
    {
        // Wait for energy regeneration or generate chi
        if (_specialization->CanUseAbility(TIGER_PALM) && _bot->GetPower(POWER_ENERGY) >= 50)
        {
            _bot->CastSpell(target, TIGER_PALM, false);
            return;
        }
        return;
    }

    // Update specialization-specific rotation
    _specialization->UpdateRotation(target);

    // Handle chi management
    _specialization->UpdateChiManagement();

    // Handle energy management
    _specialization->UpdateEnergyManagement();

    // Handle shared monk abilities
    HandleSharedMonkAbilities(target);
}

void MonkAI::UpdateBuffs()
{
    if (!_specialization)
        return;

    _specialization->UpdateBuffs();

    // Apply Legacy of the White Tiger if needed
    if (!_bot->HasAura(LEGACY_OF_THE_WHITE_TIGER) && _specialization->CanUseAbility(LEGACY_OF_THE_WHITE_TIGER))
    {
        _bot->CastSpell(_bot, LEGACY_OF_THE_WHITE_TIGER, false);
    }

    // Specialization-specific buff management
    switch (_detectedSpec)
    {
        case MonkSpec::BREWMASTER:
            // Ensure defensive stance
            if (auto brewSpec = dynamic_cast<BrewmasterSpecialization_Enhanced*>(_specialization.get()))
            {
                brewSpec->ManageBrewChargesOptimally();
                brewSpec->ManageStaggerOptimally();
            }
            break;

        case MonkSpec::MISTWEAVER:
            // Manage healing buffs
            if (auto mistSpec = dynamic_cast<MistweaverSpecialization_Enhanced*>(_specialization.get()))
            {
                mistSpec->ManageThunderFocusTeaOptimally();
                mistSpec->ManageHoTsIntelligently();
            }
            break;

        case MonkSpec::WINDWALKER:
            // Manage DPS buffs
            if (auto windSpec = dynamic_cast<WindwalkerSpecialization_Enhanced*>(_specialization.get()))
            {
                windSpec->ManageMarkOfCraneOptimally();
                windSpec->ManageChiOptimally();
            }
            break;
    }
}

void MonkAI::UpdateCooldowns(uint32 diff)
{
    if (!_specialization)
        return;

    _specialization->UpdateCooldowns(diff);
}

bool MonkAI::CanUseAbility(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->CanUseAbility(spellId);
}

void MonkAI::OnCombatStart(::Unit* target)
{
    if (!_specialization || !target)
        return;

    TC_LOG_DEBUG("playerbots", "MonkAI combat started for player {} against {}",
                 _bot->GetName(), target->GetName());

    _specialization->OnCombatStart(target);

    // Specialization-specific combat start
    switch (_detectedSpec)
    {
        case MonkSpec::BREWMASTER:
            // Ensure stagger is active and threat generation begins
            if (auto brewSpec = dynamic_cast<BrewmasterSpecialization_Enhanced*>(_specialization.get()))
            {
                brewSpec->ManageThreatOptimally();
                // Use Keg Smash to establish threat
                if (_specialization->CanUseAbility(KEG_SMASH))
                {
                    _bot->CastSpell(target, KEG_SMASH, false);
                }
            }
            break;

        case MonkSpec::MISTWEAVER:
            // Begin fistweaving assessment
            if (auto mistSpec = dynamic_cast<MistweaverSpecialization_Enhanced*>(_specialization.get()))
            {
                mistSpec->ManageFistweavingOptimally();
                // Apply Renewing Mist to nearby allies
                if (_specialization->CanUseAbility(RENEWING_MIST))
                {
                    std::vector<::Unit*> allies = _specialization->GetNearbyAllies();
                    for (::Unit* ally : allies)
                    {
                        if (ally->GetHealthPct() < 95.0f && !ally->HasAura(RENEWING_MIST))
                        {
                            _bot->CastSpell(ally, RENEWING_MIST, false);
                            break;
                        }
                    }
                }
            }
            break;

        case MonkSpec::WINDWALKER:
            // Begin combo building
            if (auto windSpec = dynamic_cast<WindwalkerSpecialization_Enhanced*>(_specialization.get()))
            {
                windSpec->ManageComboSequencesOptimally();
                // Start with Tiger Palm to generate chi
                if (_specialization->CanUseAbility(TIGER_PALM))
                {
                    _bot->CastSpell(target, TIGER_PALM, false);
                }
            }
            break;
    }
}

void MonkAI::OnCombatEnd()
{
    if (!_specialization)
        return;

    TC_LOG_DEBUG("playerbots", "MonkAI combat ended for player {}", _bot->GetName());

    _specialization->OnCombatEnd();

    // Post-combat healing for Mistweaver
    if (_detectedSpec == MonkSpec::MISTWEAVER)
    {
        if (auto mistSpec = dynamic_cast<MistweaverSpecialization_Enhanced*>(_specialization.get()))
        {
            // Heal any injured allies
            std::vector<::Unit*> allies = _specialization->GetNearbyAllies();
            for (::Unit* ally : allies)
            {
                if (ally->GetHealthPct() < 80.0f && _specialization->CanUseAbility(VIVIFY))
                {
                    _bot->CastSpell(ally, VIVIFY, false);
                    break;
                }
            }
        }
    }
}

bool MonkAI::HasEnoughResource(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->HasEnoughResource(spellId);
}

void MonkAI::ConsumeResource(uint32 spellId)
{
    if (!_specialization)
        return;

    _specialization->ConsumeResource(spellId);
}

Position MonkAI::GetOptimalPosition(::Unit* target)
{
    if (!_specialization || !target)
        return _bot->GetPosition();

    return _specialization->GetOptimalPosition(target);
}

float MonkAI::GetOptimalRange(::Unit* target)
{
    if (!_specialization || !target)
        return 5.0f; // Default melee range

    return _specialization->GetOptimalRange(target);
}

MonkSpec MonkAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

MonkSpec MonkAI::DetectSpecialization()
{
    if (!_bot)
        return MonkSpec::WINDWALKER;

    // Check talent points in each tree to determine specialization
    uint32 brewmasterPoints = 0;
    uint32 mistweaverPoints = 0;
    uint32 windwalkerPoints = 0;

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
                    case 0: brewmasterPoints += itr->second->currentRank; break;
                    case 1: mistweaverPoints += itr->second->currentRank; break;
                    case 2: windwalkerPoints += itr->second->currentRank; break;
                }
            }
        }
    }

    // Determine specialization based on highest talent investment
    if (brewmasterPoints >= mistweaverPoints && brewmasterPoints >= windwalkerPoints)
        _detectedSpec = MonkSpec::BREWMASTER;
    else if (mistweaverPoints >= windwalkerPoints)
        _detectedSpec = MonkSpec::MISTWEAVER;
    else
        _detectedSpec = MonkSpec::WINDWALKER;

    TC_LOG_DEBUG("playerbots", "Monk specialization detected: BM({}) MW({}) WW({}) -> {}",
                 brewmasterPoints, mistweaverPoints, windwalkerPoints, static_cast<uint32>(_detectedSpec));

    return _detectedSpec;
}

void MonkAI::InitializeSpecialization()
{
    if (!_bot)
        return;

    switch (_detectedSpec)
    {
        case MonkSpec::BREWMASTER:
            _specialization = std::make_unique<BrewmasterSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Brewmaster specialization for {}", _bot->GetName());
            break;

        case MonkSpec::MISTWEAVER:
            _specialization = std::make_unique<MistweaverSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Mistweaver specialization for {}", _bot->GetName());
            break;

        case MonkSpec::WINDWALKER:
            _specialization = std::make_unique<WindwalkerSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Windwalker specialization for {}", _bot->GetName());
            break;

        default:
            TC_LOG_ERROR("playerbots", "Unknown monk specialization for player {}: {}",
                         _bot->GetName(), static_cast<uint32>(_detectedSpec));
            _specialization = std::make_unique<WindwalkerSpecialization_Enhanced>(_bot);
            break;
    }

    if (!_specialization)
    {
        TC_LOG_ERROR("playerbots", "Failed to initialize monk specialization for player {}", _bot->GetName());
        return;
    }

    TC_LOG_INFO("playerbots", "Successfully initialized Monk AI for player {} with {} specialization",
                _bot->GetName(),
                _detectedSpec == MonkSpec::BREWMASTER ? "Brewmaster" :
                _detectedSpec == MonkSpec::MISTWEAVER ? "Mistweaver" : "Windwalker");
}

// Enhanced Monk AI methods for advanced functionality

void MonkAI::HandleSharedMonkAbilities(::Unit* target)
{
    if (!_specialization || !target)
        return;

    // Handle interrupt with Spear Hand Strike
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        if (_specialization->CanUseAbility(SPEAR_HAND_STRIKE))
        {
            _bot->CastSpell(target, SPEAR_HAND_STRIKE, false);
            return;
        }
    }

    // Handle crowd control with Paralysis (if not in combat with multiple enemies)
    std::vector<::Unit*> enemies = _specialization->GetNearbyEnemies(15.0f);
    if (enemies.size() == 1 && target->GetHealthPct() > 80.0f)
    {
        if (_specialization->CanUseAbility(PARALYSIS) && !target->HasAura(PARALYSIS))
        {
            _bot->CastSpell(target, PARALYSIS, false);
            return;
        }
    }

    // Handle AoE with Leg Sweep if multiple enemies
    if (enemies.size() >= 3)
    {
        if (_specialization->CanUseAbility(LEG_SWEEP))
        {
            _bot->CastSpell(_bot, LEG_SWEEP, false);
            return;
        }
    }
}

void MonkAI::HandleAdvancedBrewmasterManagement()
{
    if (!_specialization || _detectedSpec != MonkSpec::BREWMASTER)
        return;

    if (auto brewSpec = dynamic_cast<BrewmasterSpecialization_Enhanced*>(_specialization.get()))
    {
        brewSpec->ManageStaggerOptimally();
        brewSpec->ManageBrewChargesOptimally();
        brewSpec->ManageThreatOptimally();
        brewSpec->ManageDefensiveCooldownsOptimally();
    }
}

void MonkAI::HandleAdvancedMistweaverManagement()
{
    if (!_specialization || _detectedSpec != MonkSpec::MISTWEAVER)
        return;

    if (auto mistSpec = dynamic_cast<MistweaverSpecialization_Enhanced*>(_specialization.get()))
    {
        mistSpec->ManageFistweavingOptimally();
        mistSpec->ManageHoTsIntelligently();
        mistSpec->ManageEmergencyHealingOptimally();
        mistSpec->ManageGroupHealingOptimally();
        mistSpec->ManageManaOptimally();
    }
}

void MonkAI::HandleAdvancedWindwalkerManagement()
{
    if (!_specialization || _detectedSpec != MonkSpec::WINDWALKER)
        return;

    if (auto windSpec = dynamic_cast<WindwalkerSpecialization_Enhanced*>(_specialization.get()))
    {
        windSpec->ManageChiOptimally();
        windSpec->ManageComboSequencesOptimally();
        windSpec->ManageMarkOfCraneOptimally();
        windSpec->ManageStormEarthFireOptimally();
        windSpec->ManageTouchOfDeathOptimally();
    }
}

void MonkAI::HandleEmergencySituations()
{
    if (!_specialization)
        return;

    ::Unit* target = _bot->GetSelectedUnit();

    // Check for low health emergency
    if (_bot->GetHealthPct() < 30.0f)
    {
        // Use emergency abilities based on spec
        switch (_detectedSpec)
        {
            case MonkSpec::BREWMASTER:
                if (_specialization->CanUseAbility(FORTIFYING_BREW))
                {
                    _bot->CastSpell(_bot, FORTIFYING_BREW, false);
                    return;
                }
                break;

            case MonkSpec::MISTWEAVER:
                // Self-heal with Vivify
                if (_specialization->CanUseAbility(VIVIFY))
                {
                    _bot->CastSpell(_bot, VIVIFY, false);
                    return;
                }
                break;

            case MonkSpec::WINDWALKER:
                // Use Touch of Karma for damage reflection
                if (_specialization->CanUseAbility(TOUCH_OF_KARMA) && target)
                {
                    _bot->CastSpell(target, TOUCH_OF_KARMA, false);
                    return;
                }
                break;
        }

        // Use mobility to escape if available
        if (_specialization->CanUseAbility(ROLL))
        {
            // Calculate escape position
            Position escapePos = _bot->GetPosition();
            if (target)
            {
                float angle = target->GetAngle(_bot) + M_PI; // Opposite direction
                escapePos.m_positionX += 15.0f * cos(angle);
                escapePos.m_positionY += 15.0f * sin(angle);
            }
            _bot->CastSpell(escapePos.GetPositionX(), escapePos.GetPositionY(), escapePos.GetPositionZ(), ROLL, false);
            return;
        }
    }

    // Check for mana emergency (Mistweaver)
    if (_detectedSpec == MonkSpec::MISTWEAVER && _bot->GetPowerPct(POWER_MANA) < 20.0f)
    {
        if (_specialization->CanUseAbility(MANA_TEA))
        {
            _bot->CastSpell(_bot, MANA_TEA, false);
            return;
        }
    }

    // Check for multiple attackers
    std::vector<::Unit*> attackers;
    for (auto& threat : _bot->GetThreatMgr().GetThreats())
    {
        if (::Unit* attacker = threat.getTarget())
        {
            if (attacker->IsInCombatWith(_bot))
                attackers.push_back(attacker);
        }
    }

    if (attackers.size() >= 3)
    {
        // Use AoE crowd control
        if (_specialization->CanUseAbility(LEG_SWEEP))
        {
            _bot->CastSpell(_bot, LEG_SWEEP, false);
            return;
        }
    }
}

void MonkAI::OptimizeRotationForTarget(::Unit* target)
{
    if (!_specialization || !target)
        return;

    // Specialization-specific optimization
    switch (_detectedSpec)
    {
        case MonkSpec::BREWMASTER:
            if (auto brewSpec = dynamic_cast<BrewmasterSpecialization_Enhanced*>(_specialization.get()))
            {
                brewSpec->OptimizeThreatGeneration(target);
                brewSpec->ManageKegSmashOptimally();
            }
            break;

        case MonkSpec::MISTWEAVER:
            if (auto mistSpec = dynamic_cast<MistweaverSpecialization_Enhanced*>(_specialization.get()))
            {
                // If target is an ally, optimize healing
                if (target->IsFriendlyTo(_bot))
                {
                    mistSpec->OptimizeHealingTargetSelection();
                }
                else
                {
                    // Optimize fistweaving damage
                    mistSpec->CoordinateFistweavingRotation();
                }
            }
            break;

        case MonkSpec::WINDWALKER:
            if (auto windSpec = dynamic_cast<WindwalkerSpecialization_Enhanced*>(_specialization.get()))
            {
                windSpec->OptimizeComboExecution(target);
                windSpec->OptimizeMarkSpreading();
            }
            break;
    }
}

// Monk-specific spell IDs
constexpr uint32 TIGER_PALM = 100780;
constexpr uint32 BLACKOUT_KICK = 100784;
constexpr uint32 LEGACY_OF_THE_WHITE_TIGER = 116781;
constexpr uint32 KEG_SMASH = 121253;
constexpr uint32 RENEWING_MIST = 115151;
constexpr uint32 VIVIFY = 116670;
constexpr uint32 SPEAR_HAND_STRIKE = 116705;
constexpr uint32 PARALYSIS = 115078;
constexpr uint32 LEG_SWEEP = 119381;
constexpr uint32 FORTIFYING_BREW = 115203;
constexpr uint32 TOUCH_OF_KARMA = 122470;
constexpr uint32 ROLL = 109132;
constexpr uint32 MANA_TEA = 115294;

} // namespace Playerbot