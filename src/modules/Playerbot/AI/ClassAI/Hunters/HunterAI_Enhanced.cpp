/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HunterAI.h"
#include "BeastMasterySpecialization_Enhanced.h"
#include "MarksmanshipSpecialization_Enhanced.h"
#include "SurvivalSpecialization_Enhanced.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

HunterAI::HunterAI(Player* bot) : ClassAI(bot), _detectedSpec(HunterSpec::BEAST_MASTERY)
{
    DetectSpecialization();
    InitializeSpecialization();

    TC_LOG_DEBUG("playerbots", "HunterAI initialized for player {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_detectedSpec));
}

void HunterAI::UpdateRotation(::Unit* target)
{
    if (!target || !_specialization)
        return;

    // Ensure we have a pet if Beast Mastery
    if (_detectedSpec == HunterSpec::BEAST_MASTERY && !_bot->GetPet())
    {
        if (auto bmSpec = dynamic_cast<BeastMasterySpecialization_Enhanced*>(_specialization.get()))
        {
            bmSpec->SummonPet();
        }
    }

    // Update specialization-specific rotation
    _specialization->UpdateRotation(target);

    // Handle aspect management
    _specialization->UpdateAspectManagement();

    // Update pet management if applicable
    if (_detectedSpec == HunterSpec::BEAST_MASTERY)
        _specialization->UpdatePetManagement();

    // Handle trap management
    _specialization->UpdateTrapManagement();

    // Update range management
    _specialization->UpdateRangeManagement();
}

void HunterAI::UpdateBuffs()
{
    if (!_specialization)
        return;

    _specialization->UpdateBuffs();

    // Ensure hunter's mark is applied
    if (_bot->isInCombat())
    {
        if (::Unit* target = _bot->GetSelectedUnit())
        {
            if (!target->HasAura(SPELL_HUNTERS_MARK) && CanUseAbility(SPELL_HUNTERS_MARK))
            {
                _bot->CastSpell(target, SPELL_HUNTERS_MARK, false);
            }
        }
    }
}

void HunterAI::UpdateCooldowns(uint32 diff)
{
    if (!_specialization)
        return;

    _specialization->UpdateCooldowns(diff);
}

bool HunterAI::CanUseAbility(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->CanUseAbility(spellId);
}

void HunterAI::OnCombatStart(::Unit* target)
{
    if (!_specialization || !target)
        return;

    TC_LOG_DEBUG("playerbots", "HunterAI combat started for player {} against {}",
                 _bot->GetName(), target->GetName());

    _specialization->OnCombatStart(target);

    // Apply Hunter's Mark immediately
    if (!target->HasAura(SPELL_HUNTERS_MARK) && CanUseAbility(SPELL_HUNTERS_MARK))
    {
        _bot->CastSpell(target, SPELL_HUNTERS_MARK, false);
    }

    // Beast Mastery specific combat start
    if (_detectedSpec == HunterSpec::BEAST_MASTERY)
    {
        if (auto bmSpec = dynamic_cast<BeastMasterySpecialization_Enhanced*>(_specialization.get()))
        {
            // Ensure pet is attacking
            if (_bot->GetPet())
                bmSpec->CommandPetAttack(target);
        }
    }
}

void HunterAI::OnCombatEnd()
{
    if (!_specialization)
        return;

    TC_LOG_DEBUG("playerbots", "HunterAI combat ended for player {}", _bot->GetName());

    _specialization->OnCombatEnd();

    // Pet management after combat
    if (_detectedSpec == HunterSpec::BEAST_MASTERY)
    {
        if (auto bmSpec = dynamic_cast<BeastMasterySpecialization_Enhanced*>(_specialization.get()))
        {
            bmSpec->CommandPetFollow();
            bmSpec->MendPetIfNeeded();
            bmSpec->FeedPetIfNeeded();
        }
    }
}

bool HunterAI::HasEnoughResource(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->HasEnoughResource(spellId);
}

void HunterAI::ConsumeResource(uint32 spellId)
{
    if (!_specialization)
        return;

    _specialization->ConsumeResource(spellId);
}

Position HunterAI::GetOptimalPosition(::Unit* target)
{
    if (!_specialization || !target)
        return _bot->GetPosition();

    return _specialization->GetOptimalPosition(target);
}

float HunterAI::GetOptimalRange(::Unit* target)
{
    if (!_specialization || !target)
        return 25.0f; // Default hunter range

    return _specialization->GetOptimalRange(target);
}

HunterSpec HunterAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

void HunterAI::DetectSpecialization()
{
    if (!_bot)
        return;

    // Check talent points in each tree to determine specialization
    uint32 beastMasteryPoints = 0;
    uint32 marksmanshipPoints = 0;
    uint32 survivalPoints = 0;

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
                    case 0: beastMasteryPoints += itr->second->currentRank; break;
                    case 1: marksmanshipPoints += itr->second->currentRank; break;
                    case 2: survivalPoints += itr->second->currentRank; break;
                }
            }
        }
    }

    // Determine specialization based on highest talent investment
    if (beastMasteryPoints >= marksmanshipPoints && beastMasteryPoints >= survivalPoints)
        _detectedSpec = HunterSpec::BEAST_MASTERY;
    else if (marksmanshipPoints >= survivalPoints)
        _detectedSpec = HunterSpec::MARKSMANSHIP;
    else
        _detectedSpec = HunterSpec::SURVIVAL;

    TC_LOG_DEBUG("playerbots", "Hunter specialization detected: BM({}) MM({}) SV({}) -> {}",
                 beastMasteryPoints, marksmanshipPoints, survivalPoints, static_cast<uint32>(_detectedSpec));
}

void HunterAI::InitializeSpecialization()
{
    if (!_bot)
        return;

    switch (_detectedSpec)
    {
        case HunterSpec::BEAST_MASTERY:
            _specialization = std::make_unique<BeastMasterySpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Beast Mastery specialization for {}", _bot->GetName());
            break;

        case HunterSpec::MARKSMANSHIP:
            _specialization = std::make_unique<MarksmanshipSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Marksmanship specialization for {}", _bot->GetName());
            break;

        case HunterSpec::SURVIVAL:
            _specialization = std::make_unique<SurvivalSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Survival specialization for {}", _bot->GetName());
            break;

        default:
            TC_LOG_ERROR("playerbots", "Unknown hunter specialization for player {}: {}",
                         _bot->GetName(), static_cast<uint32>(_detectedSpec));
            _specialization = std::make_unique<BeastMasterySpecialization_Enhanced>(_bot);
            break;
    }

    if (!_specialization)
    {
        TC_LOG_ERROR("playerbots", "Failed to initialize hunter specialization for player {}", _bot->GetName());
        return;
    }

    TC_LOG_INFO("playerbots", "Successfully initialized Hunter AI for player {} with {} specialization",
                _bot->GetName(),
                _detectedSpec == HunterSpec::BEAST_MASTERY ? "Beast Mastery" :
                _detectedSpec == HunterSpec::MARKSMANSHIP ? "Marksmanship" : "Survival");
}

// Hunter-specific spell IDs
constexpr uint32 SPELL_HUNTERS_MARK = 19506;
constexpr uint32 SPELL_ASPECT_OF_THE_HAWK = 13165;
constexpr uint32 SPELL_ASPECT_OF_THE_MONKEY = 13163;
constexpr uint32 SPELL_ASPECT_OF_THE_CHEETAH = 5118;
constexpr uint32 SPELL_ASPECT_OF_THE_PACK = 13159;
constexpr uint32 SPELL_ASPECT_OF_THE_WILD = 20043;
constexpr uint32 SPELL_ASPECT_OF_THE_VIPER = 34074;
constexpr uint32 SPELL_ASPECT_OF_THE_DRAGONHAWK = 61846;

} // namespace Playerbot