/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DemonHunterAI.h"
#include "HavocSpecialization_Enhanced.h"
#include "VengeanceSpecialization_Enhanced.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

DemonHunterAI::DemonHunterAI(Player* bot) : ClassAI(bot), _detectedSpec(DemonHunterSpec::HAVOC)
{
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbots", "DemonHunterAI initialized for player {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_detectedSpec));
}

void DemonHunterAI::UpdateRotation(::Unit* target)
{
    if (!target || !_specialization)
        return;

    // Ensure we have proper resource management
    if (_bot->GetPower(POWER_FURY) < 20 && _detectedSpec == DemonHunterSpec::HAVOC)
    {
        // Generate fury with Demon's Bite
        if (_specialization->CanUseAbility(DEMONS_BITE))
        {
            _bot->CastSpell(target, DEMONS_BITE, false);
            return;
        }
    }

    if (_bot->GetPower(POWER_PAIN) < 20 && _detectedSpec == DemonHunterSpec::VENGEANCE)
    {
        // Generate pain with Shear
        if (_specialization->CanUseAbility(SHEAR))
        {
            _bot->CastSpell(target, SHEAR, false);
            return;
        }
    }

    // Update specialization-specific rotation
    _specialization->UpdateRotation(target);

    // Handle metamorphosis management
    _specialization->UpdateMetamorphosis();

    // Handle soul fragment management
    _specialization->UpdateSoulFragments();

    // Handle shared demon hunter abilities
    HandleSharedDemonHunterAbilities(target);
}

void DemonHunterAI::UpdateBuffs()
{
    if (!_specialization)
        return;

    _specialization->UpdateBuffs();

    // Specialization-specific buff management
    switch (_detectedSpec)
    {
        case DemonHunterSpec::HAVOC:
            // Manage momentum and metamorphosis buffs
            if (auto havocSpec = dynamic_cast<HavocSpecialization_Enhanced*>(_specialization.get()))
            {
                havocSpec->ManageMomentumOptimally();
                havocSpec->ManageMetamorphosisOptimally();
            }
            break;

        case DemonHunterSpec::VENGEANCE:
            // Manage defensive buffs
            if (auto venSpec = dynamic_cast<VengeanceSpecialization_Enhanced*>(_specialization.get()))
            {
                venSpec->ManageDemonSpikesOptimally();
                venSpec->ManageImmolationAuraOptimally();
            }
            break;
    }
}

void DemonHunterAI::UpdateCooldowns(uint32 diff)
{
    if (!_specialization)
        return;

    _specialization->UpdateCooldowns(diff);
}

bool DemonHunterAI::CanUseAbility(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->CanUseAbility(spellId);
}

void DemonHunterAI::OnCombatStart(::Unit* target)
{
    if (!_specialization || !target)
        return;

    TC_LOG_DEBUG("playerbots", "DemonHunterAI combat started for player {} against {}",
                 _bot->GetName(), target->GetName());

    _specialization->OnCombatStart(target);

    // Specialization-specific combat start
    switch (_detectedSpec)
    {
        case DemonHunterSpec::HAVOC:
            // Begin with mobility and momentum building
            if (auto havocSpec = dynamic_cast<HavocSpecialization_Enhanced*>(_specialization.get()))
            {
                havocSpec->ManageMobilityOptimally();
                // Start with Fel Rush for momentum if in range
                if (_specialization->CanUseAbility(FEL_RUSH) &&
                    target->GetDistance(_bot) > 10.0f && target->GetDistance(_bot) < 20.0f)
                {
                    _bot->CastSpell(target, FEL_RUSH, false);
                }
                // Otherwise start with Demon's Bite
                else if (_specialization->CanUseAbility(DEMONS_BITE))
                {
                    _bot->CastSpell(target, DEMONS_BITE, false);
                }
            }
            break;

        case DemonHunterSpec::VENGEANCE:
            // Establish threat and activate defensive abilities
            if (auto venSpec = dynamic_cast<VengeanceSpecialization_Enhanced*>(_specialization.get()))
            {
                venSpec->ManageThreatOptimally();
                // Start with Infernal Strike for threat if in range
                if (_specialization->CanUseAbility(INFERNAL_STRIKE) &&
                    target->GetDistance(_bot) > 8.0f)
                {
                    _bot->CastSpell(target, INFERNAL_STRIKE, false);
                }
                // Activate Demon Spikes for mitigation
                if (_specialization->CanUseAbility(DEMON_SPIKES))
                {
                    _bot->CastSpell(_bot, DEMON_SPIKES, false);
                }
                // Apply Immolation Aura for threat
                if (_specialization->CanUseAbility(IMMOLATION_AURA))
                {
                    _bot->CastSpell(_bot, IMMOLATION_AURA, false);
                }
            }
            break;
    }
}

void DemonHunterAI::OnCombatEnd()
{
    if (!_specialization)
        return;

    TC_LOG_DEBUG("playerbots", "DemonHunterAI combat ended for player {}", _bot->GetName());

    _specialization->OnCombatEnd();

    // Post-combat soul fragment consumption for healing
    if (_detectedSpec == DemonHunterSpec::VENGEANCE)
    {
        if (auto venSpec = dynamic_cast<VengeanceSpecialization_Enhanced*>(_specialization.get()))
        {
            // Consume soul fragments for healing if injured
            if (_bot->GetHealthPct() < 80.0f && venSpec->GetAvailableSoulFragments() > 0)
            {
                venSpec->ConsumeSoulFragments();
            }
        }
    }
}

bool DemonHunterAI::HasEnoughResource(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->HasEnoughResource(spellId);
}

void DemonHunterAI::ConsumeResource(uint32 spellId)
{
    if (!_specialization)
        return;

    _specialization->ConsumeResource(spellId);
}

Position DemonHunterAI::GetOptimalPosition(::Unit* target)
{
    if (!_specialization || !target)
        return _bot->GetPosition();

    return _specialization->GetOptimalPosition(target);
}

float DemonHunterAI::GetOptimalRange(::Unit* target)
{
    if (!_specialization || !target)
        return 5.0f; // Default melee range

    return _specialization->GetOptimalRange(target);
}

DemonHunterSpec DemonHunterAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

void DemonHunterAI::DetectSpecialization()
{
    if (!_bot)
        return;

    // Check talent points in each tree to determine specialization
    uint32 havocPoints = 0;
    uint32 vengeancePoints = 0;

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
                    case 0: havocPoints += itr->second->currentRank; break;
                    case 1: vengeancePoints += itr->second->currentRank; break;
                }
            }
        }
    }

    // Determine specialization based on highest talent investment
    if (havocPoints >= vengeancePoints)
        _detectedSpec = DemonHunterSpec::HAVOC;
    else
        _detectedSpec = DemonHunterSpec::VENGEANCE;

    TC_LOG_DEBUG("playerbots", "Demon Hunter specialization detected: HAV({}) VEN({}) -> {}",
                 havocPoints, vengeancePoints, static_cast<uint32>(_detectedSpec));
}

void DemonHunterAI::InitializeSpecialization()
{
    if (!_bot)
        return;

    switch (_detectedSpec)
    {
        case DemonHunterSpec::HAVOC:
            _specialization = std::make_unique<HavocSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Havoc specialization for {}", _bot->GetName());
            break;

        case DemonHunterSpec::VENGEANCE:
            _specialization = std::make_unique<VengeanceSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Vengeance specialization for {}", _bot->GetName());
            break;

        default:
            TC_LOG_ERROR("playerbots", "Unknown demon hunter specialization for player {}: {}",
                         _bot->GetName(), static_cast<uint32>(_detectedSpec));
            _specialization = std::make_unique<HavocSpecialization_Enhanced>(_bot);
            break;
    }

    if (!_specialization)
    {
        TC_LOG_ERROR("playerbots", "Failed to initialize demon hunter specialization for player {}", _bot->GetName());
        return;
    }

    TC_LOG_INFO("playerbots", "Successfully initialized Demon Hunter AI for player {} with {} specialization",
                _bot->GetName(),
                _detectedSpec == DemonHunterSpec::HAVOC ? "Havoc" : "Vengeance");
}

// Enhanced Demon Hunter AI methods for advanced functionality

void DemonHunterAI::HandleSharedDemonHunterAbilities(::Unit* target)
{
    if (!_specialization || !target)
        return;

    // Handle Throw Glaive for ranged engagement or pulling
    if (target->GetDistance(_bot) > 15.0f && target->GetDistance(_bot) < 30.0f)
    {
        if (_specialization->CanUseAbility(THROW_GLAIVE))
        {
            _bot->CastSpell(target, THROW_GLAIVE, false);
            return;
        }
    }

    // Handle Felblade for gap closing and resource generation
    if (target->GetDistance(_bot) > 8.0f && target->GetDistance(_bot) < 15.0f)
    {
        if (_specialization->CanUseAbility(FELBLADE))
        {
            _bot->CastSpell(target, FELBLADE, false);
            return;
        }
    }
}

void DemonHunterAI::HandleAdvancedHavocManagement()
{
    if (!_specialization || _detectedSpec != DemonHunterSpec::HAVOC)
        return;

    if (auto havocSpec = dynamic_cast<HavocSpecialization_Enhanced*>(_specialization.get()))
    {
        havocSpec->ManageFuryOptimally();
        havocSpec->ManageMomentumOptimally();
        havocSpec->ManageMetamorphosisOptimally();
        havocSpec->ManageMobilityOptimally();
        havocSpec->ManageSoulFragmentsOptimally();
    }
}

void DemonHunterAI::HandleAdvancedVengeanceManagement()
{
    if (!_specialization || _detectedSpec != DemonHunterSpec::VENGEANCE)
        return;

    if (auto venSpec = dynamic_cast<VengeanceSpecialization_Enhanced*>(_specialization.get()))
    {
        venSpec->ManagePainOptimally();
        venSpec->ManageSoulFragmentsOptimally();
        venSpec->ManageSigilsOptimally();
        venSpec->ManageThreatOptimally();
        venSpec->ManageDefensiveCooldownsOptimally();
    }
}

void DemonHunterAI::HandleEmergencySituations()
{
    if (!_specialization)
        return;

    ::Unit* target = _bot->GetSelectedUnit();

    // Check for low health emergency
    if (_bot->GetHealthPct() < 30.0f)
    {
        // Use specialization-specific emergency abilities
        switch (_detectedSpec)
        {
            case DemonHunterSpec::HAVOC:
                // Use Blur for damage reduction if available
                if (_specialization->CanUseAbility(BLUR))
                {
                    _bot->CastSpell(_bot, BLUR, false);
                    return;
                }
                // Use Vengeful Retreat to escape
                if (_specialization->CanUseAbility(VENGEFUL_RETREAT))
                {
                    _bot->CastSpell(_bot, VENGEFUL_RETREAT, false);
                    return;
                }
                break;

            case DemonHunterSpec::VENGEANCE:
                // Use Soul Barrier for emergency shielding
                if (_specialization->CanUseAbility(SOUL_BARRIER))
                {
                    _bot->CastSpell(_bot, SOUL_BARRIER, false);
                    return;
                }
                // Use Metamorphosis for emergency health and leech
                if (_specialization->ShouldUseMetamorphosis())
                {
                    _specialization->TriggerMetamorphosis();
                    return;
                }
                // Consume soul fragments for healing
                if (_specialization->GetAvailableSoulFragments() > 0)
                {
                    _specialization->ConsumeSoulFragments();
                    return;
                }
                break;
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
        // Use AoE abilities based on specialization
        switch (_detectedSpec)
        {
            case DemonHunterSpec::HAVOC:
                // Use Blade Dance for AoE damage
                if (_specialization->CanUseAbility(BLADE_DANCE))
                {
                    _bot->CastSpell(_bot, BLADE_DANCE, false);
                    return;
                }
                // Use Eye Beam for channeled AoE
                if (_specialization->CanUseAbility(EYE_BEAM) && target)
                {
                    _bot->CastSpell(target, EYE_BEAM, false);
                    return;
                }
                break;

            case DemonHunterSpec::VENGEANCE:
                // Use Sigil of Flame for AoE threat and damage
                if (_specialization->CanUseAbility(SIGIL_OF_FLAME))
                {
                    Position sigilPos = _bot->GetPosition();
                    _bot->CastSpell(sigilPos.GetPositionX(), sigilPos.GetPositionY(), sigilPos.GetPositionZ(), SIGIL_OF_FLAME, false);
                    return;
                }
                break;
        }
    }
}

void DemonHunterAI::OptimizeRotationForTarget(::Unit* target)
{
    if (!_specialization || !target)
        return;

    // Specialization-specific optimization
    switch (_detectedSpec)
    {
        case DemonHunterSpec::HAVOC:
            if (auto havocSpec = dynamic_cast<HavocSpecialization_Enhanced*>(_specialization.get()))
            {
                havocSpec->OptimizeRotationForTarget(target);
                havocSpec->ManageEyeBeamOptimally();
                havocSpec->ManageNemesisOptimally();
            }
            break;

        case DemonHunterSpec::VENGEANCE:
            if (auto venSpec = dynamic_cast<VengeanceSpecialization_Enhanced*>(_specialization.get()))
            {
                venSpec->OptimizeThreatGeneration(target);
                venSpec->ManageFieryBrandOptimally();
            }
            break;
    }
}

void DemonHunterAI::HandleSoulFragmentManagement()
{
    if (!_specialization)
        return;

    // Update soul fragment tracking
    _specialization->UpdateSoulFragments();

    // Consume soul fragments based on need
    if (_specialization->ShouldConsumeSoulFragments())
    {
        _specialization->ConsumeSoulFragments();
    }
}

void DemonHunterAI::HandleMetamorphosisManagement()
{
    if (!_specialization)
        return;

    // Update metamorphosis state
    _specialization->UpdateMetamorphosis();

    // Trigger metamorphosis if conditions are met
    if (_specialization->ShouldUseMetamorphosis())
    {
        _specialization->TriggerMetamorphosis();
    }
}

// Demon Hunter-specific spell IDs
constexpr uint32 DEMONS_BITE = 162243;
constexpr uint32 SHEAR = 203782;
constexpr uint32 FEL_RUSH = 195072;
constexpr uint32 VENGEFUL_RETREAT = 198793;
constexpr uint32 INFERNAL_STRIKE = 189110;
constexpr uint32 THROW_GLAIVE = 185123;
constexpr uint32 FELBLADE = 232893;
constexpr uint32 DEMON_SPIKES = 203720;
constexpr uint32 IMMOLATION_AURA = 178740;
constexpr uint32 BLADE_DANCE = 188499;
constexpr uint32 EYE_BEAM = 198013;
constexpr uint32 SIGIL_OF_FLAME = 204596;
constexpr uint32 SOUL_BARRIER = 227225;
constexpr uint32 BLUR = 198589;

} // namespace Playerbot