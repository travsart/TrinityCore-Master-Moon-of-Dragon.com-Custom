/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HavocSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

HavocSpecialization::HavocSpecialization(Player* bot)
    : DemonHunterSpecialization(bot)
    , _fury(0)
    , _maxFury(FURY_MAX)
    , _lastFuryRegen(0)
    , _havocMetaRemaining(0)
    , _inHavocMeta(false)
    , _lastHavocMeta(0)
    , _nemesisReady(0)
    , _chaosBladesReady(0)
    , _eyeBeamReady(0)
    , _lastFelRush(0)
    , _lastVengefulRetreat(0)
    , _felRushCharges(2)
    , _totalDamageDealt(0)
    , _furySpent(0)
    , _soulFragmentsConsumed(0)
{
}

void HavocSpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateFuryManagement();
    UpdateMobilityRotation();
    UpdateMetamorphosis();
    UpdateSoulFragments();
    UpdateOffensiveCooldowns();

    // Emergency movement
    if (bot->GetHealthPct() < 30.0f && ShouldCastVengefulRetreat())
    {
        CastVengefulRetreat();
        return;
    }

    // Use offensive cooldowns when appropriate
    if (bot->GetHealthPct() > 70.0f)
    {
        UseOffensiveCooldowns();
    }

    // Metamorphosis rotation in meta form
    if (_inHavocMeta)
    {
        if (GetFury() >= 40 && bot->IsWithinMeleeRange(target))
        {
            CastAnnihilation(target);
            return;
        }

        if (GetAvailableSoulFragments() >= 2)
        {
            CastDeathSweep();
            return;
        }
    }

    // Normal rotation
    if (ShouldCastEyeBeam(target))
    {
        CastEyeBeam(target);
        return;
    }

    if (ShouldCastBladeDance())
    {
        CastBladeDance();
        return;
    }

    if (ShouldCastChaosStrike(target))
    {
        CastChaosStrike(target);
        return;
    }

    // Use Felblade for gap closing and fury generation
    if (bot->GetDistance(target) > MELEE_RANGE && bot->HasSpell(FELBLADE))
    {
        CastFelblade(target);
        return;
    }

    // Fel Rush for gap closing if target is far
    if (bot->GetDistance(target) > 15.0f && ShouldCastFelRush(target))
    {
        CastFelRush(target);
        return;
    }

    if (ShouldCastDemonsBite(target))
    {
        CastDemonsBite(target);
        return;
    }

    // Throw Glaive for ranged damage
    if (bot->GetDistance(target) > MELEE_RANGE)
    {
        CastThrowGlaive(target);
    }
}

void HavocSpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Immolation Aura if available
    if (!bot->HasAura(IMMOLATION_AURA) && bot->HasSpell(IMMOLATION_AURA))
    {
        bot->CastSpell(bot, IMMOLATION_AURA, false);
    }
}

void HavocSpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_nemesisReady > diff)
        _nemesisReady -= diff;
    else
        _nemesisReady = 0;

    if (_chaosBladesReady > diff)
        _chaosBladesReady -= diff;
    else
        _chaosBladesReady = 0;

    if (_eyeBeamReady > diff)
        _eyeBeamReady -= diff;
    else
        _eyeBeamReady = 0;

    if (_lastFelRush > diff)
        _lastFelRush -= diff;
    else
        _lastFelRush = 0;

    if (_lastVengefulRetreat > diff)
        _lastVengefulRetreat -= diff;
    else
        _lastVengefulRetreat = 0;

    if (_havocMetaRemaining > diff)
        _havocMetaRemaining -= diff;
    else
    {
        _havocMetaRemaining = 0;
        _inHavocMeta = false;
    }

    if (_lastHavocMeta > diff)
        _lastHavocMeta -= diff;
    else
        _lastHavocMeta = 0;
}

bool HavocSpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void HavocSpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    _fury = _maxFury / 2; // Start with some fury
    _felRushCharges = 2;
}

void HavocSpecialization::OnCombatEnd()
{
    _fury = 0;
    _inHavocMeta = false;
    _havocMetaRemaining = 0;
    _felRushCharges = 2;
    _cooldowns.clear();
}

bool HavocSpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    switch (spellId)
    {
        case CHAOS_STRIKE:
        case ANNIHILATION:
            return _fury >= 40;
        case BLADE_DANCE:
        case DEATH_SWEEP:
            return _fury >= 35;
        case EYE_BEAM:
            return _fury >= 30 && _eyeBeamReady == 0;
        case FEL_RUSH:
            return _felRushCharges > 0;
        case VENGEFUL_RETREAT:
            return _lastVengefulRetreat == 0;
        case METAMORPHOSIS_HAVOC:
            return _lastHavocMeta == 0;
        case NEMESIS:
            return _nemesisReady == 0;
        case CHAOS_BLADES:
            return _chaosBladesReady == 0;
        default:
            return true;
    }
}

void HavocSpecialization::ConsumeResource(uint32 spellId)
{
    switch (spellId)
    {
        case CHAOS_STRIKE:
        case ANNIHILATION:
            SpendFury(40);
            break;
        case BLADE_DANCE:
        case DEATH_SWEEP:
            SpendFury(35);
            break;
        case EYE_BEAM:
            SpendFury(30);
            _eyeBeamReady = EYE_BEAM_COOLDOWN;
            break;
        case FEL_RUSH:
            if (_felRushCharges > 0)
                _felRushCharges--;
            _lastFelRush = FEL_RUSH_COOLDOWN;
            break;
        case VENGEFUL_RETREAT:
            _lastVengefulRetreat = VENGEFUL_RETREAT_COOLDOWN;
            break;
        case METAMORPHOSIS_HAVOC:
            _inHavocMeta = true;
            _havocMetaRemaining = HAVOC_META_DURATION;
            _lastHavocMeta = 240000; // 4 minute cooldown
            break;
        case NEMESIS:
            _nemesisReady = NEMESIS_COOLDOWN;
            break;
        case CHAOS_BLADES:
            _chaosBladesReady = CHAOS_BLADES_COOLDOWN;
            break;
        default:
            break;
    }
}

Position HavocSpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Stay in melee range for most abilities
    float distance = MELEE_RANGE * 0.8f;
    float angle = target->GetAngle(bot);

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float HavocSpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

void HavocSpecialization::UpdateMetamorphosis()
{
    if (ShouldUseMetamorphosis())
    {
        TriggerMetamorphosis();
    }
}

bool HavocSpecialization::ShouldUseMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot || _inHavocMeta || _lastHavocMeta > 0)
        return false;

    // Use when facing tough enemies or low on health
    return bot->GetHealthPct() < 50.0f ||
           (bot->IsInCombat() && bot->getAttackers().size() > 2);
}

void HavocSpecialization::TriggerMetamorphosis()
{
    EnterHavocMetamorphosis();
}

MetamorphosisState HavocSpecialization::GetMetamorphosisState() const
{
    return _inHavocMeta ? MetamorphosisState::HAVOC_META : MetamorphosisState::NONE;
}

void HavocSpecialization::UpdateSoulFragments()
{
    RemoveExpiredSoulFragments();

    if (ShouldConsumeSoulFragments())
    {
        ConsumeSoulFragments();
    }
}

void HavocSpecialization::ConsumeSoulFragments()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Soul fragments provide fury and healing in Havoc
    uint32 fragments = GetAvailableSoulFragments();
    if (fragments > 0)
    {
        GenerateFury(fragments * 20); // 20 fury per fragment
        bot->SetHealth(std::min(bot->GetHealth() + (fragments * 1000), bot->GetMaxHealth()));
        _soulFragmentsConsumed += fragments;

        // Clear consumed fragments
        _soulFragments.clear();
    }
}

bool HavocSpecialization::ShouldConsumeSoulFragments()
{
    return GetAvailableSoulFragments() >= SOUL_FRAGMENT_CONSUME_THRESHOLD ||
           GetFury() < 50;
}

uint32 HavocSpecialization::GetAvailableSoulFragments() const
{
    return static_cast<uint32>(_soulFragments.size());
}

void HavocSpecialization::UpdateFuryManagement()
{
    uint32 now = getMSTime();
    if (_lastFuryRegen == 0)
        _lastFuryRegen = now;

    // Passive fury regeneration
    uint32 timeDiff = now - _lastFuryRegen;
    if (timeDiff >= 1000) // Regenerate every second
    {
        uint32 furyToAdd = (timeDiff / 1000) * 2; // 2 fury per second
        _fury = std::min(_fury + furyToAdd, _maxFury);
        _lastFuryRegen = now;
    }

    // Regenerate Fel Rush charges
    if (_felRushCharges < 2 && _lastFelRush == 0)
    {
        _felRushCharges = std::min(_felRushCharges + 1, 2u);
        if (_felRushCharges < 2)
            _lastFelRush = FEL_RUSH_COOLDOWN;
    }
}

void HavocSpecialization::UpdateMobilityRotation()
{
    // Mobility management is handled in main rotation
}

void HavocSpecialization::UpdateOffensiveCooldowns()
{
    // Offensive cooldowns are used in main rotation
}

bool HavocSpecialization::ShouldCastDemonsBite(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) && _fury < 80;
}

bool HavocSpecialization::ShouldCastChaosStrike(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) && _fury >= 40;
}

bool HavocSpecialization::ShouldCastBladeDance()
{
    return _fury >= 35 && GetBot()->getAttackers().size() > 1;
}

bool HavocSpecialization::ShouldCastEyeBeam(::Unit* target)
{
    return target && _fury >= 30 && _eyeBeamReady == 0 &&
           GetBot()->GetDistance(target) <= 20.0f;
}

bool HavocSpecialization::ShouldCastFelRush(::Unit* target)
{
    return target && _felRushCharges > 0 &&
           GetBot()->GetDistance(target) > 10.0f;
}

bool HavocSpecialization::ShouldCastVengefulRetreat()
{
    return _lastVengefulRetreat == 0 && GetBot()->GetHealthPct() < 40.0f;
}

void HavocSpecialization::GenerateFuryFromAbility(uint32 spellId)
{
    switch (spellId)
    {
        case DEMONS_BITE:
            GenerateFury(25);
            break;
        case FELBLADE:
            GenerateFury(35);
            break;
        default:
            break;
    }
}

bool HavocSpecialization::HasEnoughFury(uint32 required)
{
    return _fury >= required;
}

uint32 HavocSpecialization::GetFury()
{
    return _fury;
}

float HavocSpecialization::GetFuryPercent()
{
    return static_cast<float>(_fury) / static_cast<float>(_maxFury);
}

void HavocSpecialization::CastDemonsBite(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->IsWithinMeleeRange(target))
    {
        bot->CastSpell(target, DEMONS_BITE, false);
        GenerateFuryFromAbility(DEMONS_BITE);

        // Chance to generate soul fragment
        if (urand(1, 100) <= 25)
            AddSoulFragment(target->GetPosition());
    }
}

void HavocSpecialization::CastChaosStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(CHAOS_STRIKE))
    {
        uint32 spellId = _inHavocMeta ? ANNIHILATION : CHAOS_STRIKE;
        bot->CastSpell(target, spellId, false);
        ConsumeResource(spellId);
        _totalDamageDealt += 3000; // Approximate damage
    }
}

void HavocSpecialization::CastBladeDance()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(BLADE_DANCE))
    {
        uint32 spellId = _inHavocMeta ? DEATH_SWEEP : BLADE_DANCE;
        bot->CastSpell(bot, spellId, false);
        ConsumeResource(spellId);
        _totalDamageDealt += 2500; // Approximate AoE damage
    }
}

void HavocSpecialization::CastEyeBeam(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(EYE_BEAM))
    {
        bot->CastSpell(target, EYE_BEAM, false);
        ConsumeResource(EYE_BEAM);
        _totalDamageDealt += 4000; // Approximate damage
    }
}

void HavocSpecialization::CastFelRush(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (HasEnoughResource(FEL_RUSH))
    {
        bot->CastSpell(target, FEL_RUSH, false);
        ConsumeResource(FEL_RUSH);

        // Update position tracking
        _lastPosition = bot->GetPosition();
    }
}

void HavocSpecialization::CastVengefulRetreat()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(VENGEFUL_RETREAT))
    {
        bot->CastSpell(bot, VENGEFUL_RETREAT, false);
        ConsumeResource(VENGEFUL_RETREAT);
        GenerateFury(80); // Vengeful Retreat generates fury
    }
}

void HavocSpecialization::CastThrowGlaive(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->HasSpell(THROW_GLAIVE))
    {
        bot->CastSpell(target, THROW_GLAIVE, false);
        _totalDamageDealt += 1500; // Approximate damage
    }
}

void HavocSpecialization::CastFelblade(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->HasSpell(FELBLADE))
    {
        bot->CastSpell(target, FELBLADE, false);
        GenerateFuryFromAbility(FELBLADE);
        _totalDamageDealt += 2000; // Approximate damage
    }
}

void HavocSpecialization::EnterHavocMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (HasEnoughResource(METAMORPHOSIS_HAVOC))
    {
        bot->CastSpell(bot, METAMORPHOSIS_HAVOC, false);
        ConsumeResource(METAMORPHOSIS_HAVOC);
    }
}

void HavocSpecialization::CastDeathSweep()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (_inHavocMeta && HasEnoughResource(DEATH_SWEEP))
    {
        bot->CastSpell(bot, DEATH_SWEEP, false);
        ConsumeResource(DEATH_SWEEP);
        _totalDamageDealt += 3500; // Approximate AoE damage
    }
}

void HavocSpecialization::CastAnnihilation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (_inHavocMeta && HasEnoughResource(ANNIHILATION))
    {
        bot->CastSpell(target, ANNIHILATION, false);
        ConsumeResource(ANNIHILATION);
        _totalDamageDealt += 4000; // Approximate damage
    }
}

void HavocSpecialization::UseOffensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Use Nemesis on tough enemies
    if (_nemesisReady == 0 && bot->IsInCombat())
    {
        CastNemesis(bot->GetTarget());
    }

    // Use Chaos Blades for burst damage
    if (_chaosBladesReady == 0 && bot->GetHealthPct() > 50.0f)
    {
        CastChaosBlades();
    }
}

void HavocSpecialization::CastNemesis(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (bot->HasSpell(NEMESIS) && HasEnoughResource(NEMESIS))
    {
        bot->CastSpell(target, NEMESIS, false);
        ConsumeResource(NEMESIS);
    }
}

void HavocSpecialization::CastChaosBlades()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->HasSpell(CHAOS_BLADES) && HasEnoughResource(CHAOS_BLADES))
    {
        bot->CastSpell(bot, CHAOS_BLADES, false);
        ConsumeResource(CHAOS_BLADES);
    }
}

} // namespace Playerbot