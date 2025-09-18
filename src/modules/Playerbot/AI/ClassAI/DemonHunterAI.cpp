/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DemonHunterAI.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "Group.h"
#include "WorldSession.h"

namespace Playerbot
{

DemonHunterAI::DemonHunterAI(Player* bot) : ClassAI(bot)
{
    _specialization = DetectSpecialization();
    _metamorphosisState = MetamorphosisState::NONE;
    _damageDealt = 0;
    _damageMitigated = 0;
    _furyGenerated = 0;
    _painGenerated = 0;
    _soulFragmentsConsumed = 0;

    // Initialize resource management
    _fury = FuryInfo();
    _pain = PainInfo();
    _lastResourceUpdate = 0;
    _resourceUpdateInterval = RESOURCE_UPDATE_INTERVAL;

    // Initialize metamorphosis system
    _metamorphosisRemaining = 0;
    _lastMetamorphosis = 0;
    _metamorphosisCooldown = 240000; // 4 minutes
    _inMetamorphosis = false;
    _canMetamorphosis = true;

    // Initialize soul fragment system
    _lastSoulFragmentScan = 0;
    _soulFragmentScanInterval = SOUL_FRAGMENT_SCAN_INTERVAL;
    _soulFragmentsAvailable = 0;
    _lastSoulCleaverPosition = Position();

    // Initialize Havoc specialization tracking
    _chaosStrikeCharges = 0;
    _eyeBeamReady = 0;
    _bladeGuardStacks = 0;
    _demonicStacks = 0;
    _lastChaosStrike = 0;
    _lastEyeBeam = 0;
    _demonicFormActive = false;

    // Initialize Vengeance specialization tracking
    _soulCleaverCharges = 0;
    _demonSpikesStacks = 0;
    _immolationAuraRemaining = 0;
    _sigilOfFlameCharges = 2;
    _fieryBrandRemaining = 0;
    _lastSoulCleaver = 0;
    _lastDemonSpikes = 0;

    // Initialize mobility tracking
    _felRushCharges = 2;
    _vengefulRetreatReady = 0;
    _glideRemaining = 0;
    _lastFelRush = 0;
    _lastVengefulRetreat = 0;
    _doubleJumpReady = 0;
    _isGliding = false;

    // Initialize defensive tracking
    _blurReady = 0;
    _netherwalkReady = 0;
    _darknessReady = 0;
    _lastBlur = 0;
    _lastNetherwalk = 0;
    _lastDarkness = 0;

    // Initialize crowd control tracking
    _imprisonReady = 0;
    _chaosNovaReady = 0;
    _disruptReady = 0;
    _lastImprison = 0;
    _lastChaosNova = 0;
    _lastDisrupt = 0;
}

void DemonHunterAI::UpdateRotation(::Unit* target)
{
    if (!target || !_bot)
        return;

    UpdateFuryManagement();
    UpdatePainManagement();
    UpdateMetamorphosisSystem();
    UpdateSoulFragmentSystem();

    switch (_specialization)
    {
        case DemonHunterSpec::HAVOC:
            UpdateHavocRotation(target);
            break;
        case DemonHunterSpec::VENGEANCE:
            UpdateVengeanceRotation(target);
            break;
    }

    OptimizeResourceUsage();
}

void DemonHunterAI::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain Immolation Aura for Vengeance
    if (_specialization == DemonHunterSpec::VENGEANCE)
    {
        if (_immolationAuraRemaining == 0 && CanUseAbility(IMMOLATION_AURA))
            CastImmolationAura();

        // Maintain Demon Spikes
        if (_demonSpikesStacks == 0 && CanUseAbility(DEMON_SPIKES))
            CastDemonSpikes();
    }
}

void DemonHunterAI::UpdateCooldowns(uint32 diff)
{
    ClassAI::UpdateCooldowns(diff);

    // Update resource regeneration/decay
    _lastResourceUpdate += diff;
    if (_lastResourceUpdate >= _resourceUpdateInterval)
    {
        if (_specialization == DemonHunterSpec::HAVOC)
            RegenerateFury();
        else if (_specialization == DemonHunterSpec::VENGEANCE)
            DecayPain();

        _lastResourceUpdate = 0;
    }

    // Update metamorphosis duration
    if (_inMetamorphosis && _metamorphosisRemaining > 0)
    {
        if (_metamorphosisRemaining <= diff)
        {
            _metamorphosisRemaining = 0;
            ExitMetamorphosis();
        }
        else
        {
            _metamorphosisRemaining -= diff;
        }
    }

    // Update Fel Rush charges
    static uint32 felRushRecharge = 0;
    felRushRecharge += diff;
    if (felRushRecharge >= 10000 && _felRushCharges < 2) // 10 second recharge
    {
        _felRushCharges++;
        felRushRecharge = 0;
    }

    // Update Sigil of Flame charges
    static uint32 sigilRecharge = 0;
    sigilRecharge += diff;
    if (sigilRecharge >= 30000 && _sigilOfFlameCharges < 2) // 30 second recharge
    {
        _sigilOfFlameCharges++;
        sigilRecharge = 0;
    }

    // Update various buff/debuff timers
    if (_immolationAuraRemaining > 0)
    {
        _immolationAuraRemaining = _immolationAuraRemaining > diff ? _immolationAuraRemaining - diff : 0;
    }

    if (_fieryBrandRemaining > 0)
    {
        _fieryBrandRemaining = _fieryBrandRemaining > diff ? _fieryBrandRemaining - diff : 0;
    }

    if (_demonSpikesStacks > 0)
    {
        static uint32 demonSpikesDecay = 0;
        demonSpikesDecay += diff;
        if (demonSpikesDecay >= 6000) // 6 second duration
        {
            _demonSpikesStacks = _demonSpikesStacks > 0 ? _demonSpikesStacks - 1 : 0;
            demonSpikesDecay = 0;
        }
    }
}

bool DemonHunterAI::CanUseAbility(uint32 spellId)
{
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check metamorphosis requirements
    if ((spellId == DEATH_SWEEP || spellId == ANNIHILATION) && !_inMetamorphosis)
        return false;

    return true;
}

void DemonHunterAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Consider using metamorphosis at combat start for major encounters
    if (ShouldUseMetamorphosis())
    {
        if (_specialization == DemonHunterSpec::HAVOC)
            CastMetamorphosisHavoc();
        else
            CastMetamorphosisVengeance();
    }
}

void DemonHunterAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Reset combat-specific tracking
    _chaosStrikeCharges = 0;
    _bladeGuardStacks = 0;
    _demonicStacks = 0;

    // Consume remaining soul fragments
    if (!_soulFragments.empty())
        ConsumeSoulFragments();
}

bool DemonHunterAI::HasEnoughResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (_specialization == DemonHunterSpec::HAVOC)
    {
        // Fury-based abilities
        switch (spellId)
        {
            case CHAOS_STRIKE:
            case BLADE_DANCE:
            case EYE_BEAM:
                return HasFury(spellInfo->ManaCost);
            default:
                return true; // Most abilities are free or generate fury
        }
    }
    else if (_specialization == DemonHunterSpec::VENGEANCE)
    {
        // Pain-based abilities
        switch (spellId)
        {
            case SOUL_CLEAVE:
                return HasPain(spellInfo->ManaCost);
            default:
                return true; // Most abilities are free or generate pain
        }
    }

    return true;
}

void DemonHunterAI::ConsumeResource(uint32 spellId)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    if (_specialization == DemonHunterSpec::HAVOC)
    {
        switch (spellId)
        {
            case CHAOS_STRIKE:
            case BLADE_DANCE:
            case EYE_BEAM:
                SpendFury(spellInfo->ManaCost);
                break;
            case DEMONS_BITE:
                GenerateFury(15); // Generates fury
                break;
        }
    }
    else if (_specialization == DemonHunterSpec::VENGEANCE)
    {
        switch (spellId)
        {
            case SOUL_CLEAVE:
                SpendPain(spellInfo->ManaCost);
                break;
            case SHEAR:
                GeneratePain(10); // Generates pain
                break;
        }
    }
}

Position DemonHunterAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    Position pos = _bot->GetPosition();
    float distance = _bot->GetDistance(target);

    // Demon Hunters are melee fighters
    if (distance > MELEE_RANGE)
    {
        pos = target->GetPosition();
        pos.m_positionX += MELEE_RANGE * cos(target->GetOrientation());
        pos.m_positionY += MELEE_RANGE * sin(target->GetOrientation());
    }

    return pos;
}

float DemonHunterAI::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

void DemonHunterAI::UpdateHavocRotation(::Unit* target)
{
    if (!target)
        return;

    // Use metamorphosis abilities if available
    if (_inMetamorphosis)
    {
        HandleMetamorphosisAbilities(target);
        return;
    }

    // Eye Beam for AoE situations
    std::vector<::Unit*> enemies = GetAoETargets();
    if (enemies.size() >= 3 && CanUseAbility(EYE_BEAM))
    {
        CastEyeBeam(target);
        return;
    }

    // Chaos Strike for fury spending
    if (HasFury(40) && CanUseAbility(CHAOS_STRIKE))
    {
        CastChaosStrike(target);
        return;
    }

    // Blade Dance for AoE
    if (enemies.size() >= 2 && HasFury(35) && CanUseAbility(BLADE_DANCE))
    {
        CastBladeDance(target);
        return;
    }

    // Demon's Bite for fury generation
    if (_fury.current < 50 && CanUseAbility(DEMONS_BITE))
    {
        CastDemonsBite(target);
    }
}

void DemonHunterAI::UpdateVengeanceRotation(::Unit* target)
{
    if (!target)
        return;

    // Use metamorphosis abilities if available
    if (_inMetamorphosis)
    {
        HandleMetamorphosisAbilities(target);
        return;
    }

    // Fiery Brand for major damage reduction
    if (_bot->GetHealthPct() < 60.0f && CanUseAbility(FIERY_BRAND))
    {
        CastFieryBrand(target);
        return;
    }

    // Sigil of Flame for AoE threat
    std::vector<::Unit*> enemies = GetAoETargets();
    if (enemies.size() >= 2 && _sigilOfFlameCharges > 0 && CanUseAbility(SIGIL_OF_FLAME))
    {
        CastSigilOfFlame(target);
        return;
    }

    // Soul Cleave for healing and AoE damage
    if (HasPain(30) && (enemies.size() >= 2 || _bot->GetHealthPct() < 80.0f) && CanUseAbility(SOUL_CLEAVE))
    {
        CastSoulCleave(target);
        return;
    }

    // Shear for pain generation
    if (_pain.current < 60 && CanUseAbility(SHEAR))
    {
        CastShear(target);
    }
}

void DemonHunterAI::UpdateFuryManagement()
{
    if (_specialization != DemonHunterSpec::HAVOC)
        return;

    // Fury regenerates naturally over time
    RegenerateFury();

    // Optimize fury usage
    if (_fury.current >= _fury.maximum * 0.9f)
    {
        // Spend excess fury
        ::Unit* target = GetTarget();
        if (target && CanUseAbility(CHAOS_STRIKE))
            CastChaosStrike(target);
    }
}

void DemonHunterAI::UpdatePainManagement()
{
    if (_specialization != DemonHunterSpec::VENGEANCE)
        return;

    // Pain decays over time
    DecayPain();

    // Generate pain when low
    if (_pain.current < 30)
    {
        ::Unit* target = GetTarget();
        if (target && CanUseAbility(SHEAR))
            CastShear(target);
    }
}

void DemonHunterAI::GenerateFury(uint32 amount)
{
    _fury.GenerateFury(amount);
    _furyGenerated += amount;
}

void DemonHunterAI::SpendFury(uint32 amount)
{
    _fury.SpendFury(amount);
}

void DemonHunterAI::GeneratePain(uint32 amount)
{
    _pain.GeneratePain(amount);
    _painGenerated += amount;
}

void DemonHunterAI::SpendPain(uint32 amount)
{
    _pain.SpendPain(amount);
}

bool DemonHunterAI::HasFury(uint32 required)
{
    return _fury.HasFury(required);
}

uint32 DemonHunterAI::GetFury()
{
    return _fury.current;
}

uint32 DemonHunterAI::GetMaxFury()
{
    return _fury.maximum;
}

float DemonHunterAI::GetFuryPercent()
{
    return _fury.maximum > 0 ? (float)_fury.current / _fury.maximum : 0.0f;
}

void DemonHunterAI::RegenerateFury()
{
    if (_specialization != DemonHunterSpec::HAVOC)
        return;

    // Fury regenerates naturally at 20 per second
    _fury.current = std::min(_fury.current + FURY_GENERATION_RATE / 10, _fury.maximum);
}

bool DemonHunterAI::HasPain(uint32 required)
{
    return _pain.HasPain(required);
}

uint32 DemonHunterAI::GetPain()
{
    return _pain.current;
}

uint32 DemonHunterAI::GetMaxPain()
{
    return _pain.maximum;
}

float DemonHunterAI::GetPainPercent()
{
    return _pain.maximum > 0 ? (float)_pain.current / _pain.maximum : 0.0f;
}

void DemonHunterAI::DecayPain()
{
    if (_specialization != DemonHunterSpec::VENGEANCE)
        return;

    // Pain decays naturally at 2 per second
    _pain.DecayPain(PAIN_DECAY_RATE / 10);
}

void DemonHunterAI::UpdateMetamorphosisSystem()
{
    uint32 now = getMSTime();

    // Check if metamorphosis cooldown is ready
    if (now - _lastMetamorphosis >= _metamorphosisCooldown)
        _canMetamorphosis = true;

    // Manage metamorphosis duration
    if (_inMetamorphosis)
        ManageMetamorphosisDuration();
}

void DemonHunterAI::EnterMetamorphosis()
{
    _inMetamorphosis = true;
    _metamorphosisState = _specialization == DemonHunterSpec::HAVOC ?
        MetamorphosisState::HAVOC_META : MetamorphosisState::VENGEANCE_META;
    _metamorphosisRemaining = METAMORPHOSIS_DURATION;
    _lastMetamorphosis = getMSTime();
    _canMetamorphosis = false;
}

void DemonHunterAI::ExitMetamorphosis()
{
    _inMetamorphosis = false;
    _metamorphosisState = MetamorphosisState::NONE;
    _metamorphosisRemaining = 0;
}

bool DemonHunterAI::CanUseMetamorphosis()
{
    return _canMetamorphosis && !_inMetamorphosis;
}

bool DemonHunterAI::ShouldUseMetamorphosis()
{
    if (!CanUseMetamorphosis())
        return false;

    // Use metamorphosis when facing multiple enemies or low health
    std::vector<::Unit*> enemies = GetAoETargets();
    return enemies.size() >= 3 || _bot->GetHealthPct() < 50.0f;
}

void DemonHunterAI::HandleMetamorphosisAbilities(::Unit* target)
{
    if (!target || !_inMetamorphosis)
        return;

    if (_specialization == DemonHunterSpec::HAVOC)
    {
        // Use Death Sweep instead of Blade Dance
        if (GetAoETargets().size() >= 2 && CanUseAbility(DEATH_SWEEP))
            _bot->CastSpell(target, DEATH_SWEEP, false);
        // Use Annihilation instead of Chaos Strike
        else if (CanUseAbility(ANNIHILATION))
            _bot->CastSpell(target, ANNIHILATION, false);
    }
    else if (_specialization == DemonHunterSpec::VENGEANCE)
    {
        // Enhanced abilities during Vengeance metamorphosis
        if (CanUseAbility(SOUL_CLEAVE))
            CastSoulCleave(target);
    }
}

void DemonHunterAI::UpdateSoulFragmentSystem()
{
    uint32 now = getMSTime();
    if (now - _lastSoulFragmentScan < _soulFragmentScanInterval)
        return;

    _lastSoulFragmentScan = now;

    // Scan for soul fragments in the area
    ScanForSoulFragments();

    // Remove expired fragments
    RemoveExpiredSoulFragments();

    // Consume fragments if beneficial
    if (ShouldConsumeSoulFragments())
        ConsumeSoulFragments();
}

void DemonHunterAI::ScanForSoulFragments()
{
    // Simulated soul fragment detection
    // In a real implementation, this would scan the game world for soul fragments
    _soulFragmentsAvailable = _soulFragments.size();
}

void DemonHunterAI::ConsumeSoulFragments()
{
    uint32 consumed = 0;
    auto it = _soulFragments.begin();

    while (it != _soulFragments.end() && consumed < 5) // Limit consumption per call
    {
        if (it->IsInRange(_bot->GetPosition(), SOUL_FRAGMENT_RANGE))
        {
            // Consume the fragment for healing
            uint32 healing = it->isGreater ? 300 : 150;
            _bot->ModifyHealth(healing);

            it = _soulFragments.erase(it);
            consumed++;
            _soulFragmentsConsumed++;
        }
        else
        {
            ++it;
        }
    }
}

void DemonHunterAI::GenerateSoulFragment(const Position& pos, ::Unit* source, bool greater)
{
    _soulFragments.emplace_back(pos, source, greater);
}

void DemonHunterAI::RemoveExpiredSoulFragments()
{
    _soulFragments.erase(
        std::remove_if(_soulFragments.begin(), _soulFragments.end(),
            [](const SoulFragment& fragment) { return fragment.IsExpired(); }),
        _soulFragments.end());
}

uint32 DemonHunterAI::GetAvailableSoulFragments()
{
    return static_cast<uint32>(std::count_if(_soulFragments.begin(), _soulFragments.end(),
        [this](const SoulFragment& fragment) {
            return fragment.IsInRange(_bot->GetPosition(), SOUL_FRAGMENT_RANGE);
        }));
}

bool DemonHunterAI::ShouldConsumeSoulFragments()
{
    // Consume fragments when health is low or we have many fragments
    return _bot->GetHealthPct() < 80.0f || GetAvailableSoulFragments() >= 3;
}

std::vector<::Unit*> DemonHunterAI::GetAoETargets()
{
    std::vector<::Unit*> targets;

    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(_bot, _bot, 8.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, check);
    Cell::VisitAllObjects(_bot, searcher, 8.0f);

    for (Unit* enemy : nearbyEnemies)
    {
        targets.push_back(enemy);
    }

    return targets;
}

DemonHunterSpec DemonHunterAI::DetectSpecialization()
{
    if (!_bot)
        return DemonHunterSpec::HAVOC;

    // Simple detection based on known spells
    if (_bot->HasSpell(SOUL_CLEAVE) || _bot->HasSpell(DEMON_SPIKES))
        return DemonHunterSpec::VENGEANCE;

    return DemonHunterSpec::HAVOC;
}

bool DemonHunterAI::IsInMeleeRange(::Unit* target)
{
    return target && _bot->GetDistance(target) <= MELEE_RANGE;
}

// Combat ability implementations
void DemonHunterAI::CastDemonsBite(::Unit* target)
{
    if (!target || !CanUseAbility(DEMONS_BITE))
        return;

    _bot->CastSpell(target, DEMONS_BITE, false);
    ConsumeResource(DEMONS_BITE);
}

void DemonHunterAI::CastChaosStrike(::Unit* target)
{
    if (!target || !CanUseAbility(CHAOS_STRIKE))
        return;

    _bot->CastSpell(target, CHAOS_STRIKE, false);
    _lastChaosStrike = getMSTime();
    ConsumeResource(CHAOS_STRIKE);
}

void DemonHunterAI::CastBladeDance(::Unit* target)
{
    if (!target || !CanUseAbility(BLADE_DANCE))
        return;

    _bot->CastSpell(target, BLADE_DANCE, false);
    ConsumeResource(BLADE_DANCE);
}

void DemonHunterAI::CastEyeBeam(::Unit* target)
{
    if (!target || !CanUseAbility(EYE_BEAM))
        return;

    _bot->CastSpell(target, EYE_BEAM, false);
    _lastEyeBeam = getMSTime();
    ConsumeResource(EYE_BEAM);
}

void DemonHunterAI::CastMetamorphosisHavoc()
{
    if (!CanUseAbility(METAMORPHOSIS_HAVOC))
        return;

    _bot->CastSpell(_bot, METAMORPHOSIS_HAVOC, false);
    EnterMetamorphosis();
}

void DemonHunterAI::CastShear(::Unit* target)
{
    if (!target || !CanUseAbility(SHEAR))
        return;

    _bot->CastSpell(target, SHEAR, false);
    ConsumeResource(SHEAR);
}

void DemonHunterAI::CastSoulCleave(::Unit* target)
{
    if (!target || !CanUseAbility(SOUL_CLEAVE))
        return;

    _bot->CastSpell(target, SOUL_CLEAVE, false);
    _lastSoulCleaver = getMSTime();
    ConsumeResource(SOUL_CLEAVE);
}

void DemonHunterAI::CastImmolationAura()
{
    if (!CanUseAbility(IMMOLATION_AURA))
        return;

    _bot->CastSpell(_bot, IMMOLATION_AURA, false);
    _immolationAuraRemaining = 6000; // 6 seconds
    ConsumeResource(IMMOLATION_AURA);
}

void DemonHunterAI::CastDemonSpikes()
{
    if (!CanUseAbility(DEMON_SPIKES))
        return;

    _bot->CastSpell(_bot, DEMON_SPIKES, false);
    _demonSpikesStacks = std::min(_demonSpikesStacks + 1, 2u);
    _lastDemonSpikes = getMSTime();
    ConsumeResource(DEMON_SPIKES);
}

void DemonHunterAI::CastSigilOfFlame(::Unit* target)
{
    if (!target || !CanUseAbility(SIGIL_OF_FLAME) || _sigilOfFlameCharges == 0)
        return;

    _bot->CastSpell(target, SIGIL_OF_FLAME, false);
    _sigilOfFlameCharges--;
    ConsumeResource(SIGIL_OF_FLAME);
}

void DemonHunterAI::CastFieryBrand(::Unit* target)
{
    if (!target || !CanUseAbility(FIERY_BRAND))
        return;

    _bot->CastSpell(target, FIERY_BRAND, false);
    _fieryBrandRemaining = 8000; // 8 seconds
    ConsumeResource(FIERY_BRAND);
}

void DemonHunterAI::CastMetamorphosisVengeance()
{
    if (!CanUseAbility(METAMORPHOSIS_VENGEANCE))
        return;

    _bot->CastSpell(_bot, METAMORPHOSIS_VENGEANCE, false);
    EnterMetamorphosis();
}

void DemonHunterAI::CastFelRush()
{
    if (!CanUseAbility(FEL_RUSH) || _felRushCharges == 0)
        return;

    _bot->CastSpell(_bot, FEL_RUSH, false);
    _felRushCharges--;
    _lastFelRush = getMSTime();
}

void DemonHunterAI::CastVengefulRetreat()
{
    if (!CanUseAbility(VENGEFUL_RETREAT))
        return;

    _bot->CastSpell(_bot, VENGEFUL_RETREAT, false);
    _lastVengefulRetreat = getMSTime();
}

void DemonHunterAI::OptimizeResourceUsage()
{
    if (_specialization == DemonHunterSpec::HAVOC)
    {
        // Optimize fury usage - spend when near cap
        if (_fury.current >= _fury.maximum * 0.9f)
        {
            ::Unit* target = GetTarget();
            if (target && CanUseAbility(CHAOS_STRIKE))
                CastChaosStrike(target);
        }
    }
    else if (_specialization == DemonHunterSpec::VENGEANCE)
    {
        // Optimize pain usage - maintain reasonable levels
        if (_pain.current >= _pain.maximum * 0.8f)
        {
            ::Unit* target = GetTarget();
            if (target && CanUseAbility(SOUL_CLEAVE))
                CastSoulCleave(target);
        }
    }
}

void DemonHunterAI::RecordDamageDealt(uint32 damage, ::Unit* target)
{
    _damageDealt += damage;
}

void DemonHunterAI::RecordDamageMitigated(uint32 amount)
{
    _damageMitigated += amount;
}

// Utility class implementations
uint32 DemonHunterCalculator::CalculateDemonsBiteDamage(Player* caster, ::Unit* target)
{
    return 800; // Placeholder
}

uint32 DemonHunterCalculator::CalculateChaosStrikeDamage(Player* caster, ::Unit* target)
{
    return 1500; // Placeholder
}

uint32 DemonHunterCalculator::CalculateEyeBeamDamage(Player* caster, ::Unit* target)
{
    return 2000; // Placeholder
}

uint32 DemonHunterCalculator::CalculateSoulCleaveDamage(Player* caster, ::Unit* target)
{
    return 1200; // Placeholder
}

float DemonHunterCalculator::CalculateDamageReduction(Player* caster)
{
    return 0.1f; // 10% placeholder
}

uint32 DemonHunterCalculator::CalculateSoulFragmentHealing(Player* caster)
{
    return 150; // Placeholder
}

uint32 DemonHunterCalculator::CalculateFuryGeneration(uint32 spellId, Player* caster)
{
    switch (spellId)
    {
        case DemonHunterAI::DEMONS_BITE: return 15;
        default: return 0;
    }
}

uint32 DemonHunterCalculator::CalculatePainGeneration(uint32 spellId, Player* caster, uint32 damageTaken)
{
    switch (spellId)
    {
        case DemonHunterAI::SHEAR: return 10;
        default: return damageTaken / 100; // 1% of damage taken
    }
}

float DemonHunterCalculator::CalculateResourceEfficiency(uint32 spellId, Player* caster)
{
    return 1.0f; // Placeholder
}

uint32 DemonHunterCalculator::CalculateMetamorphosisDuration(Player* caster)
{
    return 30000; // 30 seconds base
}

float DemonHunterCalculator::CalculateMetamorphosisDamageBonus(Player* caster)
{
    return 0.25f; // 25% damage bonus
}

bool DemonHunterCalculator::ShouldUseMetamorphosis(Player* caster, const std::vector<::Unit*>& enemies)
{
    return enemies.size() >= 3 || caster->GetHealthPct() < 50.0f;
}

Position DemonHunterCalculator::GetOptimalSoulFragmentPosition(Player* caster, const std::vector<::Unit*>& enemies)
{
    return caster->GetPosition(); // Placeholder
}

uint32 DemonHunterCalculator::CalculateSoulFragmentValue(const SoulFragment& fragment, Player* caster)
{
    return fragment.isGreater ? 300 : 150;
}

bool DemonHunterCalculator::ShouldConsumeSoulFragments(Player* caster, uint32 availableFragments)
{
    return caster->GetHealthPct() < 80.0f || availableFragments >= 3;
}

void DemonHunterCalculator::CacheDemonHunterData()
{
    // Cache implementation
}

// DemonHunterResourceManager implementation
DemonHunterResourceManager::DemonHunterResourceManager(DemonHunterAI* owner) : _owner(owner), _lastUpdate(0)
{
}

void DemonHunterResourceManager::Update(uint32 diff)
{
    _lastUpdate += diff;
    if (_lastUpdate >= 1000) // Update every second
    {
        UpdateFuryRegeneration();
        UpdatePainDecay();
        _lastUpdate = 0;
    }
}

void DemonHunterResourceManager::GenerateFury(uint32 amount)
{
    _fury.GenerateFury(amount);
}

void DemonHunterResourceManager::SpendFury(uint32 amount)
{
    _fury.SpendFury(amount);
}

void DemonHunterResourceManager::GeneratePain(uint32 amount)
{
    _pain.GeneratePain(amount);
}

void DemonHunterResourceManager::SpendPain(uint32 amount)
{
    _pain.SpendPain(amount);
}

uint32 DemonHunterResourceManager::GetFury() const
{
    return _fury.current;
}

uint32 DemonHunterResourceManager::GetPain() const
{
    return _pain.current;
}

bool DemonHunterResourceManager::HasFury(uint32 required) const
{
    return _fury.HasFury(required);
}

bool DemonHunterResourceManager::HasPain(uint32 required) const
{
    return _pain.HasPain(required);
}

float DemonHunterResourceManager::GetFuryPercent() const
{
    return _fury.maximum > 0 ? (float)_fury.current / _fury.maximum : 0.0f;
}

float DemonHunterResourceManager::GetPainPercent() const
{
    return _pain.maximum > 0 ? (float)_pain.current / _pain.maximum : 0.0f;
}

void DemonHunterResourceManager::UpdateFuryRegeneration()
{
    _fury.current = std::min(_fury.current + 2, _fury.maximum); // 2 fury per second
}

void DemonHunterResourceManager::UpdatePainDecay()
{
    _pain.DecayPain(2); // 2 pain decay per second
}

void DemonHunterResourceManager::OptimizeResourceUsage()
{
    // Resource optimization logic
}

bool DemonHunterResourceManager::ShouldConserveFury() const
{
    return GetFuryPercent() < 0.3f;
}

bool DemonHunterResourceManager::ShouldGeneratePain() const
{
    return GetPainPercent() < 0.5f;
}

// MetamorphosisController implementation
MetamorphosisController::MetamorphosisController(DemonHunterAI* owner) : _owner(owner),
    _state(MetamorphosisState::NONE), _remainingTime(0), _cooldownRemaining(240000), _lastActivation(0)
{
}

void MetamorphosisController::Update(uint32 diff)
{
    if (_remainingTime > 0)
    {
        _remainingTime = _remainingTime > diff ? _remainingTime - diff : 0;
        if (_remainingTime == 0)
            DeactivateMetamorphosis();
    }

    if (_cooldownRemaining > 0)
    {
        _cooldownRemaining = _cooldownRemaining > diff ? _cooldownRemaining - diff : 0;
    }
}

void MetamorphosisController::ActivateMetamorphosis()
{
    _state = MetamorphosisState::HAVOC_META; // Simplified
    _remainingTime = 30000; // 30 seconds
    _cooldownRemaining = 240000; // 4 minutes
    _lastActivation = getMSTime();
}

void MetamorphosisController::DeactivateMetamorphosis()
{
    _state = MetamorphosisState::NONE;
    _remainingTime = 0;
}

bool MetamorphosisController::CanUseMetamorphosis() const
{
    return _cooldownRemaining == 0 && _state == MetamorphosisState::NONE;
}

bool MetamorphosisController::ShouldUseMetamorphosis() const
{
    return CanUseMetamorphosis() && IsOptimalTimingForMetamorphosis();
}

bool MetamorphosisController::IsInMetamorphosis() const
{
    return _state != MetamorphosisState::NONE && _remainingTime > 0;
}

uint32 MetamorphosisController::GetRemainingTime() const
{
    return _remainingTime;
}

MetamorphosisState MetamorphosisController::GetState() const
{
    return _state;
}

bool MetamorphosisController::IsOptimalTimingForMetamorphosis() const
{
    return HasSufficientTargets(); // Simplified check
}

bool MetamorphosisController::HasSufficientTargets() const
{
    return true; // Placeholder
}

// SoulFragmentManager implementation
SoulFragmentManager::SoulFragmentManager(DemonHunterAI* owner) : _owner(owner), _lastScan(0), _scanInterval(500)
{
}

void SoulFragmentManager::Update(uint32 diff)
{
    _lastScan += diff;
    if (_lastScan >= _scanInterval)
    {
        ScanForNewFragments();
        UpdateFragmentStates();
        RemoveExpiredFragments();
        _lastScan = 0;
    }
}

void SoulFragmentManager::AddSoulFragment(const Position& pos, ::Unit* source, bool greater)
{
    _fragments.emplace_back(pos, source, greater);
}

void SoulFragmentManager::ConsumeSoulFragments()
{
    auto it = _fragments.begin();
    while (it != _fragments.end())
    {
        if (it->IsInRange(_owner->GetBot()->GetPosition(), 20.0f))
        {
            // Consume fragment
            it = _fragments.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SoulFragmentManager::RemoveExpiredFragments()
{
    _fragments.erase(
        std::remove_if(_fragments.begin(), _fragments.end(),
            [](const SoulFragment& fragment) { return fragment.IsExpired(); }),
        _fragments.end());
}

uint32 SoulFragmentManager::GetAvailableFragments() const
{
    return static_cast<uint32>(std::count_if(_fragments.begin(), _fragments.end(),
        [this](const SoulFragment& fragment) {
            return fragment.IsInRange(_owner->GetBot()->GetPosition(), 20.0f);
        }));
}

SoulFragment* SoulFragmentManager::GetNearestFragment()
{
    SoulFragment* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();

    for (auto& fragment : _fragments)
    {
        float distance = fragment.position.GetExactDist2d(_owner->GetBot()->GetPosition());
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = &fragment;
        }
    }

    return nearest;
}

bool SoulFragmentManager::HasFragmentsInRange() const
{
    return GetAvailableFragments() > 0;
}

bool SoulFragmentManager::ShouldConsumeFragments() const
{
    return _owner->GetBot()->GetHealthPct() < 80.0f || GetAvailableFragments() >= 3;
}

Position SoulFragmentManager::GetOptimalConsumptionPosition() const
{
    return _owner->GetBot()->GetPosition(); // Placeholder
}

void SoulFragmentManager::ScanForNewFragments()
{
    // Scan implementation
}

void SoulFragmentManager::UpdateFragmentStates()
{
    // Update fragment states
}

uint32 SoulFragmentManager::CalculateFragmentValue(const SoulFragment& fragment) const
{
    return fragment.isGreater ? 300 : 150;
}

void SoulFragmentManager::OptimizeFragmentConsumption()
{
    // Optimization logic
}

// Cache static variables
std::unordered_map<uint32, uint32> DemonHunterCalculator::_damageCache;
std::unordered_map<uint32, uint32> DemonHunterCalculator::_resourceCache;
std::mutex DemonHunterCalculator::_cacheMutex;

} // namespace Playerbot