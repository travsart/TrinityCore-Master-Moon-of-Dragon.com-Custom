/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "VengeanceSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "Group.h"

namespace Playerbot
{

VengeanceSpecialization::VengeanceSpecialization(Player* bot)
    : DemonHunterSpecialization(bot)
    , _pain(0)
    , _maxPain(PAIN_MAX)
    , _lastPainRegen(0)
    , _vengeanceMetaRemaining(0)
    , _inVengeanceMeta(false)
    , _lastVengeanceMeta(0)
    , _demonSpikesCharges(2)
    , _demonSpikesReady(0)
    , _fieryBrandReady(0)
    , _soulBarrierReady(0)
    , _lastDemonSpikes(0)
    , _lastFieryBrand(0)
    , _lastSoulBarrier(0)
    , _lastSigil(0)
    , _lastThreatUpdate(0)
    , _totalThreatGenerated(0)
    , _painSpent(0)
    , _damageAbsorbed(0)
{
}

void VengeanceSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdatePainManagement();
    UpdateThreatManagement();
    UpdateMetamorphosis();
    UpdateSoulFragments();
    UpdateDefensiveCooldowns();
    UpdateSigilManagement();

    // Emergency defensive abilities
    if (bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100)
    {
        ManageEmergencyAbilities();
        return;
    }

    // Use defensive cooldowns if needed
    if (bot->GetHealthPct() < 60.0f)
    {
        UseDefensiveCooldowns();
    }

    // Maintain Immolation Aura
    if (ShouldCastImmolationAura())
    {
        CastImmolationAura();
        return;
    }

    // Threat generation priority for multiple targets
    std::vector<::Unit*> threatTargets = GetThreatTargets();
    if (threatTargets.size() > 1)
    {
        // Multi-target: prioritize Soul Cleave and Sigils
        if (ShouldCastSoulCleave())
        {
            CastSoulCleave();
            return;
        }

        if (_lastSigil == 0)
        {
            CastSigilOfFlame(target->GetPosition());
            return;
        }
    }

    // Metamorphosis rotation in meta form
    if (_inVengeanceMeta)
    {
        if (GetPain() >= 40 && bot->IsWithinMeleeRange(target))
        {
            CastSoulSunder(target);
            return;
        }
    }

    // Single target rotation
    if (ShouldCastSoulCleave() && GetAvailableSoulFragments() >= 2)
    {
        CastSoulCleave();
        return;
    }

    if (ShouldCastShear(target))
    {
        CastShear(target);
        return;
    }

    // Use Infernal Strike for gap closing and damage
    if (bot->GetDistance(target) > MELEE_RANGE && bot->HasSpell(INFERNAL_STRIKE))
    {
        CastInfernalStrike(target);
        return;
    }

    // Use Felblade for gap closing and pain generation
    if (bot->GetDistance(target) > MELEE_RANGE && bot->HasSpell(FELBLADE))
    {
        CastFelblade(target);
        return;
    }

    // Throw Glaive for ranged damage and threat
    if (bot->GetDistance(target) > MELEE_RANGE)
    {
        CastThrowGlaive(target);
    }
}

void VengeanceSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Demon Spikes when in combat
    if (bot->IsInCombat() && ShouldCastDemonSpikes())
    {
        CastDemonSpikes();
    }

    // Maintain Immolation Aura
    if (!bot->HasAura(IMMOLATION_AURA) && bot->HasSpell(IMMOLATION_AURA))
    {
        bot->CastSpell(bot, IMMOLATION_AURA, false);
    }
}

void VengeanceSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    for (auto& sigil : _sigilCooldowns)
        if (sigil.second > diff)
            sigil.second -= diff;
        else
            sigil.second = 0;

    if (_demonSpikesReady > diff)
        _demonSpikesReady -= diff;
    else
        _demonSpikesReady = 0;

    if (_fieryBrandReady > diff)
        _fieryBrandReady -= diff;
    else
        _fieryBrandReady = 0;

    if (_soulBarrierReady > diff)
        _soulBarrierReady -= diff;
    else
        _soulBarrierReady = 0;

    if (_lastDemonSpikes > diff)
        _lastDemonSpikes -= diff;
    else
        _lastDemonSpikes = 0;

    if (_lastFieryBrand > diff)
        _lastFieryBrand -= diff;
    else
        _lastFieryBrand = 0;

    if (_lastSoulBarrier > diff)
        _lastSoulBarrier -= diff;
    else
        _lastSoulBarrier = 0;

    if (_lastSigil > diff)
        _lastSigil -= diff;
    else
        _lastSigil = 0;

    if (_vengeanceMetaRemaining > diff)
        _vengeanceMetaRemaining -= diff;
    else
    {
        _vengeanceMetaRemaining = 0;
        _inVengeanceMeta = false;
    }

    if (_lastVengeanceMeta > diff)
        _lastVengeanceMeta -= diff;
    else
        _lastVengeanceMeta = 0;

    if (_lastThreatUpdate > diff)
        _lastThreatUpdate -= diff;
    else
        _lastThreatUpdate = 0;
}

bool VengeanceSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void VengeanceSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    _pain = _maxPain / 3; // Start with some pain
    _demonSpikesCharges = 2;

    // Apply initial defensive buffs
    if (ShouldCastDemonSpikes())
        CastDemonSpikes();
}

void VengeanceSpecialization::OnCombatEnd()
{
    _pain = 0;
    _inVengeanceMeta = false;
    _vengeanceMetaRemaining = 0;
    _demonSpikesCharges = 2;
    _threatTargets.clear();
    _cooldowns.clear();
    _sigilCooldowns.clear();
}

bool VengeanceSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    switch (spellId)
    {
        case SOUL_CLEAVE:
            return _pain >= 30;
        case IMMOLATION_AURA:
            return _pain >= 20;
        case DEMON_SPIKES:
            return _demonSpikesCharges > 0;
        case FIERY_BRAND:
            return _fieryBrandReady == 0;
        case SOUL_BARRIER:
            return _soulBarrierReady == 0;
        case METAMORPHOSIS_VENGEANCE:
            return _lastVengeanceMeta == 0;
        case SIGIL_OF_FLAME:
        case SIGIL_OF_SILENCE:
        case SIGIL_OF_MISERY:
        case SIGIL_OF_CHAINS:
            {
                auto it = _sigilCooldowns.find(spellId);
                return it == _sigilCooldowns.end() || it->second == 0;
            }
        case INFERNAL_STRIKE:
            {
                auto it = _cooldowns.find(spellId);
                return it == _cooldowns.end() || it->second == 0;
            }
        default:
            return true;
    }
}

void VengeanceSpecialization::ConsumeResource(uint32 spellId)
{
    switch (spellId)
    {
        case SOUL_CLEAVE:
            SpendPain(30);
            break;
        case IMMOLATION_AURA:
            SpendPain(20);
            break;
        case DEMON_SPIKES:
            if (_demonSpikesCharges > 0)
                _demonSpikesCharges--;
            _lastDemonSpikes = DEMON_SPIKES_COOLDOWN;
            break;
        case FIERY_BRAND:
            _fieryBrandReady = FIERY_BRAND_COOLDOWN;
            _lastFieryBrand = getMSTime();
            break;
        case SOUL_BARRIER:
            _soulBarrierReady = SOUL_BARRIER_COOLDOWN;
            _lastSoulBarrier = getMSTime();
            break;
        case METAMORPHOSIS_VENGEANCE:
            _inVengeanceMeta = true;
            _vengeanceMetaRemaining = VENGEANCE_META_DURATION;
            _lastVengeanceMeta = 180000; // 3 minute cooldown
            break;
        case SIGIL_OF_FLAME:
        case SIGIL_OF_SILENCE:
        case SIGIL_OF_MISERY:
        case SIGIL_OF_CHAINS:
            _sigilCooldowns[spellId] = SIGIL_COOLDOWN;
            _lastSigil = 2000; // 2 second global sigil cooldown
            break;
        case INFERNAL_STRIKE:
            _cooldowns[spellId] = INFERNAL_STRIKE_COOLDOWN;
            break;
        default:
            break;
    }
}

Position VengeanceSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Tank stays in front of target
    float distance = MELEE_RANGE * 0.8f;
    float angle = target->GetAngle(bot);

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        target->GetAngle(bot) + M_PI
    );
}

float VengeanceSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

void VengeanceSpecialization::UpdateMetamorphosis()
{
    if (ShouldUseMetamorphosis())
    {
        TriggerMetamorphosis();
    }
}

bool VengeanceSpecialization::ShouldUseMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot || _inVengeanceMeta || _lastVengeanceMeta > 0)
        return false;

    // Use when facing multiple enemies or low on health
    return bot->GetHealthPct() < 40.0f ||
           _threatTargets.size() > 3;
}

void VengeanceSpecialization::TriggerMetamorphosis()
{
    EnterVengeanceMetamorphosis();
}

MetamorphosisState VengeanceSpecialization::GetMetamorphosisState() const
{
    return _inVengeanceMeta ? MetamorphosisState::VENGEANCE_META : MetamorphosisState::NONE;
}

void VengeanceSpecialization::UpdateSoulFragments()
{
    RemoveExpiredSoulFragments();

    if (ShouldConsumeSoulFragments())
    {
        ConsumeSoulFragments();
    }
}

void VengeanceSpecialization::ConsumeSoulFragments()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Soul fragments provide pain and healing in Vengeance
    uint32 fragments = GetAvailableSoulFragments();
    if (fragments > 0)
    {
        GeneratePain(fragments * 20); // 20 pain per fragment
        bot->SetHealth(std::min(bot->GetHealth() + (fragments * 1500), bot->GetMaxHealth()));

        // Clear consumed fragments
        _soulFragments.clear();
    }
}

bool VengeanceSpecialization::ShouldConsumeSoulFragments()
{
    return GetAvailableSoulFragments() >= SOUL_FRAGMENT_CONSUME_THRESHOLD ||
           GetPain() < 30;
}

uint32 VengeanceSpecialization::GetAvailableSoulFragments() const
{
    return static_cast<uint32>(_soulFragments.size());
}

void VengeanceSpecialization::UpdatePainManagement()
{
    uint32 now = getMSTime();
    if (_lastPainRegen == 0)
        _lastPainRegen = now;

    // Passive pain regeneration
    uint32 timeDiff = now - _lastPainRegen;
    if (timeDiff >= 1000) // Regenerate every second
    {
        uint32 painToAdd = (timeDiff / 1000) * 3; // 3 pain per second
        _pain = std::min(_pain + painToAdd, _maxPain);
        _lastPainRegen = now;
    }

    // Regenerate Demon Spikes charges
    if (_demonSpikesCharges < 2 && _lastDemonSpikes == 0)
    {
        _demonSpikesCharges = std::min(_demonSpikesCharges + 1, 2u);
        if (_demonSpikesCharges < 2)
            _lastDemonSpikes = DEMON_SPIKES_COOLDOWN;
    }
}

void VengeanceSpecialization::UpdateThreatManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastThreatUpdate < 1000) // Update every second
        return;

    _threatTargets.clear();

    // Find all hostile targets in threat range
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            // Check for enemies targeting group members
            for (auto& threatRef : member->getHostileRefManager())
            {
                if (::Unit* enemy = threatRef.GetSource()->GetOwner())
                {
                    if (enemy->IsWithinDistInMap(bot, 30.0f))
                        _threatTargets.push_back(enemy);
                }
            }
        }
    }

    _lastThreatUpdate = now;
}

void VengeanceSpecialization::UpdateDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if we need Demon Spikes
    if (bot->GetHealthPct() < 70.0f && ShouldCastDemonSpikes())
    {
        CastDemonSpikes();
    }

    // Check if we need Fiery Brand
    if (bot->GetHealthPct() < 50.0f && ShouldCastFieryBrand(bot->GetTarget()))
    {
        CastFieryBrand(bot->GetTarget());
    }
}

bool VengeanceSpecialization::ShouldCastShear(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target);
}

bool VengeanceSpecialization::ShouldCastSoulCleave()
{
    return _pain >= 30 && (GetAvailableSoulFragments() >= 2 || _threatTargets.size() > 1);
}

bool VengeanceSpecialization::ShouldCastImmolationAura()
{
    return !GetBot()->HasAura(IMMOLATION_AURA) && _pain >= 20;
}

bool VengeanceSpecialization::ShouldCastDemonSpikes()
{
    return _demonSpikesCharges > 0 && !GetBot()->HasAura(DEMON_SPIKES);
}

bool VengeanceSpecialization::ShouldCastFieryBrand(::Unit* target)
{
    return target && _fieryBrandReady == 0 && GetBot()->IsWithinMeleeRange(target);
}

bool VengeanceSpecialization::ShouldCastInfernalStrike(::Unit* target)
{
    return target && GetBot()->GetDistance(target) > 10.0f &&
           _cooldowns[INFERNAL_STRIKE] == 0;
}

void VengeanceSpecialization::GeneratePainFromAbility(uint32 spellId)
{
    switch (spellId)
    {
        case SHEAR:
            GeneratePain(10);
            break;
        case FELBLADE:
            GeneratePain(15);
            break;
        default:
            break;
    }
}

bool VengeanceSpecialization::HasEnoughPain(uint32 required)
{
    return _pain >= required;
}

uint32 VengeanceSpecialization::GetPain()
{
    return _pain;
}

float VengeanceSpecialization::GetPainPercent()
{
    return static_cast<float>(_pain) / static_cast<float>(_maxPain);
}

void VengeanceSpecialization::BuildThreat(::Unit* target)
{
    if (!target)
        return;

    // Generate threat through damage and abilities
    _totalThreatGenerated += 1000; // Approximate threat value
}

void VengeanceSpecialization::MaintainThreat()
{
    // Use AoE abilities when facing multiple targets
    if (_threatTargets.size() > 1)
    {
        if (ShouldCastSoulCleave())
            CastSoulCleave();
        else if (_lastSigil == 0)
            CastSigilOfFlame(GetBot()->GetPosition());
    }
}

std::vector<::Unit*> VengeanceSpecialization::GetThreatTargets()
{
    return _threatTargets;
}

bool VengeanceSpecialization::NeedsThreat(::Unit* target)
{
    if (!target)
        return false;

    // Check if target is attacking someone other than the tank
    return target->GetTarget() != GetBot();
}

void VengeanceSpecialization::CastShear(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->IsWithinMeleeRange(target))
    {
        bot->CastSpell(target, SHEAR, false);
        GeneratePainFromAbility(SHEAR);
        BuildThreat(target);

        // Chance to generate soul fragment
        if (urand(1, 100) <= 30)
            AddSoulFragment(target->GetPosition());
    }
}

void VengeanceSpecialization::CastSoulCleave()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SOUL_CLEAVE))
    {
        bot->CastSpell(bot, SOUL_CLEAVE, false);
        ConsumeResource(SOUL_CLEAVE);

        // Consume nearby soul fragments
        ConsumeSoulFragments();

        // Generate threat on all nearby enemies
        for (::Unit* target : _threatTargets)
            BuildThreat(target);
    }
}

void VengeanceSpecialization::CastImmolationAura()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(IMMOLATION_AURA))
    {
        bot->CastSpell(bot, IMMOLATION_AURA, false);
        ConsumeResource(IMMOLATION_AURA);
    }
}

void VengeanceSpecialization::CastSigilOfFlame(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SIGIL_OF_FLAME))
    {
        bot->CastSpell(bot, SIGIL_OF_FLAME, false);
        ConsumeResource(SIGIL_OF_FLAME);

        // Generate threat on all enemies in area
        for (::Unit* target : _threatTargets)
            BuildThreat(target);
    }
}

void VengeanceSpecialization::CastInfernalStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(INFERNAL_STRIKE))
    {
        bot->CastSpell(target, INFERNAL_STRIKE, false);
        ConsumeResource(INFERNAL_STRIKE);
        BuildThreat(target);
    }
}

void VengeanceSpecialization::CastThrowGlaive(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->HasSpell(THROW_GLAIVE))
    {
        bot->CastSpell(target, THROW_GLAIVE, false);
        BuildThreat(target);
    }
}

void VengeanceSpecialization::CastFelblade(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->HasSpell(FELBLADE))
    {
        bot->CastSpell(target, FELBLADE, false);
        GeneratePainFromAbility(FELBLADE);
        BuildThreat(target);
    }
}

void VengeanceSpecialization::CastDemonSpikes()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(DEMON_SPIKES))
    {
        bot->CastSpell(bot, DEMON_SPIKES, false);
        ConsumeResource(DEMON_SPIKES);
    }
}

void VengeanceSpecialization::CastFieryBrand(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(FIERY_BRAND))
    {
        bot->CastSpell(target, FIERY_BRAND, false);
        ConsumeResource(FIERY_BRAND);
    }
}

void VengeanceSpecialization::CastSoulBarrier()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SOUL_BARRIER))
    {
        bot->CastSpell(bot, SOUL_BARRIER, false);
        ConsumeResource(SOUL_BARRIER);
    }
}

void VengeanceSpecialization::UseDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetHealthPct() < 40.0f && ShouldCastSoulBarrier())
        CastSoulBarrier();

    if (bot->GetHealthPct() < 60.0f && ShouldCastDemonSpikes())
        CastDemonSpikes();
}

void VengeanceSpecialization::ManageEmergencyAbilities()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Priority: Soul Barrier for absorption
    if (ShouldCastSoulBarrier())
    {
        CastSoulBarrier();
        return;
    }

    // Secondary: Demon Spikes for damage reduction
    if (ShouldCastDemonSpikes())
    {
        CastDemonSpikes();
        return;
    }

    // Tertiary: Metamorphosis for extra health and abilities
    if (ShouldUseMetamorphosis())
    {
        TriggerMetamorphosis();
        return;
    }
}

void VengeanceSpecialization::EnterVengeanceMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(METAMORPHOSIS_VENGEANCE))
    {
        bot->CastSpell(bot, METAMORPHOSIS_VENGEANCE, false);
        ConsumeResource(METAMORPHOSIS_VENGEANCE);
    }
}

void VengeanceSpecialization::CastSoulSunder(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (_inVengeanceMeta && bot->HasSpell(SOUL_SUNDER))
    {
        bot->CastSpell(target, SOUL_SUNDER, false);
        BuildThreat(target);
    }
}

void VengeanceSpecialization::UpdateSigilManagement()
{
    // Sigil usage is handled in main rotation
}

void VengeanceSpecialization::CastSigilOfSilence(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SIGIL_OF_SILENCE))
    {
        bot->CastSpell(bot, SIGIL_OF_SILENCE, false);
        ConsumeResource(SIGIL_OF_SILENCE);
    }
}

void VengeanceSpecialization::CastSigilOfMisery(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SIGIL_OF_MISERY))
    {
        bot->CastSpell(bot, SIGIL_OF_MISERY, false);
        ConsumeResource(SIGIL_OF_MISERY);
    }
}

void VengeanceSpecialization::CastSigilOfChains(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(SIGIL_OF_CHAINS))
    {
        bot->CastSpell(bot, SIGIL_OF_CHAINS, false);
        ConsumeResource(SIGIL_OF_CHAINS);
    }
}

} // namespace Playerbot