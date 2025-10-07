/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarlockAI.h"
#include "AfflictionWarlockRefactored.h"
#include "DemonologyWarlockRefactored.h"
#include "DestructionWarlockRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Timer.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

void WarlockAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

WarlockSpec WarlockAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockSpec::AFFLICTION;

    // Check for Demonology specialization indicators
    if (bot->HasSpell(30146) || bot->HasSpell(47193)) // Summon Felguard or Demonic Empowerment
        return WarlockSpec::DEMONOLOGY;

    // Check for Destruction specialization indicators
    if (bot->HasSpell(17962) || bot->HasSpell(50796)) // Conflagrate or Chaos Bolt
        return WarlockSpec::DESTRUCTION;

    // Default to Affliction
    return WarlockSpec::AFFLICTION;
}

void WarlockAI::SwitchSpecialization(WarlockSpec newSpec)
{
    _currentSpec = newSpec;

    switch (newSpec)
    {
        case WarlockSpec::AFFLICTION:
            _specialization = std::make_unique<AfflictionWarlockRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warlock", "Warlock {} switched to Affliction specialization",
                         GetBot()->GetName());
            break;
        case WarlockSpec::DEMONOLOGY:
            _specialization = std::make_unique<DemonologyWarlockRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warlock", "Warlock {} switched to Demonology specialization",
                         GetBot()->GetName());
            break;
        case WarlockSpec::DESTRUCTION:
            _specialization = std::make_unique<DestructionWarlockRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warlock", "Warlock {} switched to Destruction specialization",
                         GetBot()->GetName());
            break;
        default:
            _specialization = std::make_unique<AfflictionWarlockRefactored>(GetBot());
            TC_LOG_DEBUG("module.playerbot.warlock", "Warlock {} defaulted to Affliction specialization",
                         GetBot()->GetName());
            break;
    }
}

void WarlockAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Delegate to specialization implementation
    if (_specialization)
        _specialization->UpdateRotation(target);
}

void WarlockAI::UpdateBuffs()
{
    UpdateWarlockBuffs();
    if (_specialization)
        _specialization->UpdateBuffs();
}

void WarlockAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);
    if (_specialization)
        _specialization->UpdateCooldowns(diff);
}

bool WarlockAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void WarlockAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);
    if (_specialization)
        _specialization->OnCombatStart(target);
}

void WarlockAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();
    if (_specialization)
        _specialization->OnCombatEnd();
}

bool WarlockAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
        return _specialization->HasEnoughResource(spellId);
    return GetBot()->GetPower(POWER_MANA) >= 100;
}

void WarlockAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
        _specialization->ConsumeResource(spellId);
}

Position WarlockAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalPosition(target);
    return GetBot()->GetPosition();
}

float WarlockAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
        return _specialization->GetOptimalRange(target);
    return 30.0f;
}

WarlockAI::WarlockAI(Player* bot)
    : ClassAI(bot)
    , _manaSpent(0)
    , _damageDealt(0)
    , _soulshardsUsed(0)
    , _fearsUsed(0)
    , _petsSpawned(0)
    , _lastFear(0)
    , _lastPetSummon(0)
{
    _warlockMetrics.Reset();
    InitializeSpecialization();
    TC_LOG_DEBUG("playerbot.warlock", "WarlockAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec));
}

// Destructor implementation
WarlockAI::~WarlockAI() = default;

void WarlockAI::UpdateWarlockBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Basic armor spell management
    if (!bot->HasAura(28176) && !bot->HasAura(706) && !bot->HasAura(687)) // Fel Armor, Demon Armor, Demon Skin
    {
        if (bot->HasSpell(28176))
            bot->CastSpell(bot, 28176, false); // Fel Armor
        else if (bot->HasSpell(706))
            bot->CastSpell(bot, 706, false); // Demon Armor
        else if (bot->HasSpell(687))
            bot->CastSpell(bot, 687, false); // Demon Skin
    }
}

void WarlockAI::UpdatePetCheck()
{
    if (_specialization)
        _specialization->UpdatePetManagement();
}

void WarlockAI::UpdateSoulShardCheck()
{
    if (_specialization)
        _specialization->UpdateSoulShardManagement();
}

bool WarlockAI::HasEnoughMana(uint32 amount)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->GetPower(POWER_MANA) >= amount;
}

uint32 WarlockAI::GetMana()
{
    Player* bot = GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 WarlockAI::GetMaxMana()
{
    Player* bot = GetBot();
    return bot ? bot->GetMaxPower(POWER_MANA) : 1;
}

float WarlockAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    if (maxMana == 0)
        return 0.0f;

    return static_cast<float>(GetMana()) / static_cast<float>(maxMana);
}

void WarlockAI::UseDefensiveAbilities()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    // Use fear if being attacked in melee and not on cooldown
    if (bot->GetHealthPct() < 40.0f && !_currentTarget.IsEmpty())
    {
        Unit* target = ObjectAccessor::GetUnit(*bot, _currentTarget);
        if (target && bot->GetDistance(target) < 8.0f &&
            (getMSTime() - _lastFear) > 30000) // 30 second cooldown
        {
            if (bot->HasSpell(5782) && IsSpellReady(5782)) // Fear
            {
                CastSpell(target, 5782);
                _lastFear = getMSTime();
                _fearsUsed++;
            }
        }
    }

    // Use Death Coil for emergency healing
    if (bot->GetHealthPct() < 25.0f &&
        bot->HasSpell(6789) && IsSpellReady(6789)) // Death Coil
    {
        CastSpell(6789);
    }
}

void WarlockAI::UseCrowdControl(::Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Use Fear if not on cooldown
    if ((getMSTime() - _lastFear) > 30000 &&
        bot->HasSpell(5782) && IsSpellReady(5782))
    {
        CastSpell(target, 5782); // Fear
        _lastFear = getMSTime();
        _fearsUsed++;
    }
}

void WarlockAI::UpdatePetManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if we have a pet, if not summon appropriate one
    Pet* pet = bot->GetPet();
    if (!pet || !pet->IsAlive())
    {
        // Summon pet based on specialization
        uint32 petSpell = 0;
        switch (_currentSpec)
        {
            case WarlockSpec::AFFLICTION:
                petSpell = 691; // Summon Felhunter
                break;
            case WarlockSpec::DEMONOLOGY:
                petSpell = 30146; // Summon Felguard (if available)
                if (!bot->HasSpell(petSpell))
                    petSpell = 712; // Summon Succubus
                break;
            case WarlockSpec::DESTRUCTION:
                petSpell = 688; // Summon Imp
                break;
        }

        if (petSpell && bot->HasSpell(petSpell) &&
            (getMSTime() - _lastPetSummon) > 5000) // 5 second cooldown
        {
            CastSpell(petSpell);
            _lastPetSummon = getMSTime();
            _petsSpawned++;
        }
    }
}

WarlockSpec WarlockAI::GetCurrentSpecialization() const
{
    return _currentSpec;
}

bool WarlockAI::ShouldConserveMana()
{
    return GetManaPercent() < 0.3f; // 30%
}

} // namespace Playerbot