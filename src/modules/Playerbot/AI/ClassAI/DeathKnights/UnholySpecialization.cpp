/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "UnholySpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

namespace Playerbot
{

UnholySpecialization::UnholySpecialization(Player* bot)
    : DeathKnightSpecialization(bot)
    , _hasActiveGhoul(false)
    , _ghoulHealth(0)
    , _lastGhoulCommand(0)
    , _lastGhoulSummon(0)
    , _suddenDoomActive(false)
    , _suddenDoomExpires(0)
    , _lastProcCheck(0)
    , _summonGargoyleReady(0)
    , _armyOfTheDeadReady(0)
    , _darkTransformationReady(0)
    , _lastSummonGargoyle(0)
    , _lastArmyOfTheDead(0)
    , _lastDarkTransformation(0)
    , _lastDiseaseSpread(0)
    , _lastCorpseUpdate(0)
    , _totalDamageDealt(0)
    , _diseaseDamage(0)
    , _procActivations(0)
    , _runicPowerSpent(0)
{
}

void UnholySpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    UpdateRuneManagement();
    UpdateRunicPowerManagement();
    UpdateDiseaseManagement();
    UpdateGhoulManagement();
    UpdateProcManagement();
    UpdateDiseaseSpread();

    // Ensure we're in Unholy Presence
    if (ShouldUseUnholyPresence())
    {
        EnterUnholyPresence();
    }

    // Summon ghoul if needed
    if (!HasActiveGhoul() && bot->HasSpell(RAISE_DEAD))
    {
        SummonGhoul();
        return;
    }

    // Use major cooldowns when appropriate
    if (bot->GetHealthPct() > 70.0f && bot->IsInCombat())
    {
        if (ShouldCastSummonGargoyle())
        {
            CastSummonGargoyle();
            return;
        }

        if (ShouldCastArmyOfTheDead())
        {
            CastArmyOfTheDead();
            return;
        }
    }

    // Disease application priority
    if (ShouldApplyDisease(target, DiseaseType::BLOOD_PLAGUE))
    {
        CastPlagueStrike(target);
        return;
    }

    // Proc consumption priority
    if (_suddenDoomActive && ShouldCastDeathCoil(target))
    {
        CastDeathCoil(target);
        ConsumeSuddenDoomProc();
        return;
    }

    // Spread diseases to multiple targets
    if (ShouldSpreadDiseases())
    {
        SpreadDiseases(target);
        return;
    }

    // Regular rotation
    if (ShouldCastScourgeStrike(target))
    {
        CastScourgeStrike(target);
        return;
    }

    if (ShouldCastDeathCoil(target))
    {
        CastDeathCoil(target);
        return;
    }

    // Corpse explosion for AoE
    if (ShouldCastCorpseExplosion() && HasAvailableCorpse())
    {
        CastCorpseExplosion();
        return;
    }

    // Basic attacks
    if (bot->GetDistance(target) > MELEE_RANGE)
    {
        if (ShouldUseDeathGrip(target))
            CastDeathGrip(target);
        else
            CastDeathCoil(target);
    }
}

void UnholySpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Maintain Unholy Presence
    if (!bot->HasAura(UNHOLY_PRESENCE) && bot->HasSpell(UNHOLY_PRESENCE))
    {
        bot->CastSpell(bot, UNHOLY_PRESENCE, false);
    }

    // Maintain Bone Armor
    if (ShouldCastBoneArmor())
    {
        CastBoneArmor();
    }

    // Maintain Horn of Winter
    if (!bot->HasAura(HORN_OF_WINTER) && bot->HasSpell(HORN_OF_WINTER))
    {
        bot->CastSpell(bot, HORN_OF_WINTER, false);
    }
}

void UnholySpecialization::UpdateCooldowns(uint32 diff)
{
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    if (_summonGargoyleReady > diff)
        _summonGargoyleReady -= diff;
    else
        _summonGargoyleReady = 0;

    if (_armyOfTheDeadReady > diff)
        _armyOfTheDeadReady -= diff;
    else
        _armyOfTheDeadReady = 0;

    if (_darkTransformationReady > diff)
        _darkTransformationReady -= diff;
    else
        _darkTransformationReady = 0;

    if (_suddenDoomExpires > diff)
        _suddenDoomExpires -= diff;
    else
    {
        _suddenDoomExpires = 0;
        _suddenDoomActive = false;
    }

    if (_lastGhoulCommand > diff)
        _lastGhoulCommand -= diff;
    else
        _lastGhoulCommand = 0;

    if (_lastDiseaseSpread > diff)
        _lastDiseaseSpread -= diff;
    else
        _lastDiseaseSpread = 0;

    RegenerateRunes(diff);
    UpdateDiseaseTimers(diff);
}

bool UnholySpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return HasEnoughResource(spellId);
}

void UnholySpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Enter Unholy Presence
    if (ShouldUseUnholyPresence())
        EnterUnholyPresence();

    // Summon ghoul if not active
    if (!HasActiveGhoul())
        SummonGhoul();

    // Reset proc states
    _suddenDoomActive = false;
}

void UnholySpecialization::OnCombatEnd()
{
    _suddenDoomActive = false;
    _cooldowns.clear();
    _activeDiseases.clear();
    _diseaseTargets.clear();
    _availableCorpses.clear();
}

bool UnholySpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case SCOURGE_STRIKE:
            return HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::UNHOLY, 1);
        case DEATH_COIL:
            return HasEnoughRunicPower(40);
        case BONE_ARMOR:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case PLAGUE_STRIKE:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case SUMMON_GARGOYLE:
            return _summonGargoyleReady == 0;
        case ARMY_OF_THE_DEAD:
            return _armyOfTheDeadReady == 0;
        case DARK_TRANSFORMATION:
            return _darkTransformationReady == 0 && HasActiveGhoul();
        default:
            return true;
    }
}

void UnholySpecialization::ConsumeResource(uint32 spellId)
{
    switch (spellId)
    {
        case SCOURGE_STRIKE:
            ConsumeRunes(RuneType::FROST, 1);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(15);
            break;
        case DEATH_COIL:
            SpendRunicPower(40);
            break;
        case BONE_ARMOR:
            ConsumeRunes(RuneType::UNHOLY, 1);
            break;
        case PLAGUE_STRIKE:
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            break;
        case SUMMON_GARGOYLE:
            _summonGargoyleReady = SUMMON_GARGOYLE_COOLDOWN;
            _lastSummonGargoyle = getMSTime();
            break;
        case ARMY_OF_THE_DEAD:
            _armyOfTheDeadReady = ARMY_OF_THE_DEAD_COOLDOWN;
            _lastArmyOfTheDead = getMSTime();
            break;
        case DARK_TRANSFORMATION:
            _darkTransformationReady = DARK_TRANSFORMATION_COOLDOWN;
            _lastDarkTransformation = getMSTime();
            break;
        default:
            break;
    }
}

Position UnholySpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Melee DPS positioning with room for pets
    float distance = MELEE_RANGE * 0.9f;
    float angle = target->GetAngle(bot) + M_PI / 3; // Angled positioning

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

float UnholySpecialization::GetOptimalRange(::Unit* target)
{
    return MELEE_RANGE;
}

// Implement all pure virtual methods from base class
void UnholySpecialization::UpdateRuneManagement() { RegenerateRunes(0); }
bool UnholySpecialization::HasAvailableRunes(RuneType type, uint32 count) { return GetAvailableRunes(type) >= count; }
void UnholySpecialization::ConsumeRunes(RuneType type, uint32 count)
{
    uint32 consumed = 0;
    for (auto& rune : _runes)
    {
        if (rune.type == type && rune.IsReady() && consumed < count)
        {
            rune.Use();
            consumed++;
        }
    }
}
uint32 UnholySpecialization::GetAvailableRunes(RuneType type) const
{
    uint32 count = 0;
    for (const auto& rune : _runes)
    {
        if (rune.type == type && rune.IsReady())
            count++;
    }
    return count;
}

void UnholySpecialization::UpdateRunicPowerManagement()
{
    Player* bot = GetBot();
    if (!bot || bot->IsInCombat()) return;

    uint32 now = getMSTime();
    if (_lastRunicPowerDecay == 0) _lastRunicPowerDecay = now;

    uint32 timeDiff = now - _lastRunicPowerDecay;
    if (timeDiff >= 1000)
    {
        uint32 decay = (timeDiff / 1000) * RUNIC_POWER_DECAY_RATE;
        if (_runicPower > decay) _runicPower -= decay;
        else _runicPower = 0;
        _lastRunicPowerDecay = now;
    }
}

void UnholySpecialization::GenerateRunicPower(uint32 amount) { _runicPower = std::min(_runicPower + amount, _maxRunicPower); }
void UnholySpecialization::SpendRunicPower(uint32 amount) { if (_runicPower >= amount) { _runicPower -= amount; _runicPowerSpent += amount; } }
uint32 UnholySpecialization::GetRunicPower() const { return _runicPower; }
bool UnholySpecialization::HasEnoughRunicPower(uint32 required) const { return _runicPower >= required; }

void UnholySpecialization::UpdateDiseaseManagement() { UpdateDiseaseTimers(0); RefreshExpringDiseases(); }
void UnholySpecialization::ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId)
{
    if (!target) return;
    DiseaseInfo disease(type, spellId, 21000, 400); // 21 seconds, 400 damage per tick
    _activeDiseases[target->GetGUID()].push_back(disease);
}
bool UnholySpecialization::HasDisease(::Unit* target, DiseaseType type) const
{
    if (!target) return false;
    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
        if (disease.type == type && disease.IsActive()) return true;
    return false;
}
bool UnholySpecialization::ShouldApplyDisease(::Unit* target, DiseaseType type) const
{
    return target && (!HasDisease(target, type) || GetDiseaseRemainingTime(target, type) < DISEASE_REFRESH_THRESHOLD);
}
void UnholySpecialization::RefreshExpringDiseases()
{
    for (auto& targetDiseases : _activeDiseases)
    {
        for (auto& disease : targetDiseases.second)
        {
            if (disease.type == DiseaseType::BLOOD_PLAGUE && disease.NeedsRefresh())
            {
                Player* bot = GetBot();
                if (bot)
                {
                    ::Unit* target = ObjectAccessor::GetUnit(*bot, targetDiseases.first);
                    if (target && HasEnoughResource(PLAGUE_STRIKE))
                        CastPlagueStrike(target);
                }
            }
        }
    }
}

void UnholySpecialization::UpdateDeathAndDecay() { /* Handled in rotation */ }
bool UnholySpecialization::ShouldCastDeathAndDecay() const { return GetBot()->getAttackers().size() > 2; }
void UnholySpecialization::CastDeathAndDecay(Position targetPos)
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(DEATH_AND_DECAY))
        bot->CastSpell(bot, DEATH_AND_DECAY, false);
}

void UnholySpecialization::UpdateGhoulManagement()
{
    UpdatePetManagement();
    ManageGhoulHealth();

    uint32 now = getMSTime();
    if (_lastGhoulCommand == 0 && HasActiveGhoul())
    {
        ::Unit* target = GetBot()->GetTarget();
        if (target)
        {
            CommandGhoul(target);
            _lastGhoulCommand = GHOUL_COMMAND_INTERVAL;
        }
    }
}

void UnholySpecialization::UpdateProcManagement()
{
    uint32 now = getMSTime();
    if (now - _lastProcCheck < PROC_CHECK_INTERVAL) return;
    _lastProcCheck = now;

    Player* bot = GetBot();
    if (!bot) return;

    // Check for Sudden Doom proc
    if (bot->HasAura(SUDDEN_DOOM) && !_suddenDoomActive)
    {
        _suddenDoomActive = true;
        _suddenDoomExpires = now + SUDDEN_DOOM_DURATION;
        _procActivations++;
    }
}

bool UnholySpecialization::HasSuddenDoomProc() { return _suddenDoomActive; }
void UnholySpecialization::ConsumeSuddenDoomProc() { _suddenDoomActive = false; _suddenDoomExpires = 0; }

void UnholySpecialization::UpdatePetManagement()
{
    // Check if ghoul is still alive and active
    Player* bot = GetBot();
    if (bot && bot->GetPet())
        _hasActiveGhoul = true;
    else
        _hasActiveGhoul = false;
}

void UnholySpecialization::SummonGhoul()
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(RAISE_DEAD) && !HasActiveGhoul())
    {
        bot->CastSpell(bot, RAISE_DEAD, false);
        _hasActiveGhoul = true;
        _lastGhoulSummon = getMSTime();
    }
}

void UnholySpecialization::CommandGhoul(::Unit* target)
{
    if (!target || !HasActiveGhoul()) return;
    // Command ghoul to attack target (implementation would depend on pet system)
}

bool UnholySpecialization::HasActiveGhoul() { return _hasActiveGhoul; }
void UnholySpecialization::ManageGhoulHealth() { /* Monitor and heal ghoul if needed */ }

void UnholySpecialization::UpdateDiseaseSpread()
{
    uint32 now = getMSTime();
    if (now - _lastDiseaseSpread < DISEASE_SPREAD_INTERVAL) return;
    _lastDiseaseSpread = now;

    _diseaseTargets.clear();
    // Find nearby enemies for disease spreading
    Player* bot = GetBot();
    if (bot)
    {
        for (auto& attacker : bot->getAttackers())
        {
            if (attacker->IsWithinDistInMap(bot, DISEASE_SPREAD_RANGE))
                _diseaseTargets.push_back(attacker);
        }
    }
}

bool UnholySpecialization::ShouldSpreadDiseases() { return _diseaseTargets.size() > 1; }
void UnholySpecialization::SpreadDiseases(::Unit* target) { /* Implement disease spreading logic */ }
std::vector<::Unit*> UnholySpecialization::GetDiseaseTargets() { return _diseaseTargets; }

bool UnholySpecialization::ShouldCastScourgeStrike(::Unit* target) { return target && GetBot()->IsWithinMeleeRange(target) && HasEnoughResource(SCOURGE_STRIKE); }
bool UnholySpecialization::ShouldCastDeathCoil(::Unit* target) { return target && HasEnoughResource(DEATH_COIL); }
bool UnholySpecialization::ShouldCastBoneArmor() { return !GetBot()->HasAura(BONE_ARMOR) && HasEnoughResource(BONE_ARMOR); }
bool UnholySpecialization::ShouldCastCorpseExplosion() { return HasAvailableCorpse() && GetBot()->getAttackers().size() > 1; }
bool UnholySpecialization::ShouldCastSummonGargoyle() { return _summonGargoyleReady == 0 && GetBot()->IsInCombat(); }
bool UnholySpecialization::ShouldCastArmyOfTheDead() { return _armyOfTheDeadReady == 0 && GetBot()->IsInCombat() && GetBot()->getAttackers().size() > 2; }

void UnholySpecialization::CastScourgeStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(SCOURGE_STRIKE))
    {
        bot->CastSpell(target, SCOURGE_STRIKE, false);
        ConsumeResource(SCOURGE_STRIKE);
        _totalDamageDealt += 3500;

        // Scourge Strike does extra damage per disease
        auto diseases = GetActiveDiseases(target);
        _diseaseDamage += diseases.size() * 500;
    }
}

void UnholySpecialization::CastDeathCoil(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(DEATH_COIL))
    {
        if (target->IsHostileTo(bot))
            bot->CastSpell(target, DEATH_COIL, false);
        else
            bot->CastSpell(target, DEATH_COIL, false); // Heal friendly target
        ConsumeResource(DEATH_COIL);
        _totalDamageDealt += 2000;
    }
}

void UnholySpecialization::CastBoneArmor()
{
    Player* bot = GetBot();
    if (bot && HasEnoughResource(BONE_ARMOR))
    {
        bot->CastSpell(bot, BONE_ARMOR, false);
        ConsumeResource(BONE_ARMOR);
    }
}

void UnholySpecialization::CastPlagueStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target) return;
    if (HasEnoughResource(PLAGUE_STRIKE))
    {
        bot->CastSpell(target, PLAGUE_STRIKE, false);
        ConsumeResource(PLAGUE_STRIKE);
        ApplyDisease(target, DiseaseType::BLOOD_PLAGUE, PLAGUE_STRIKE);
        _totalDamageDealt += 1800;
    }
}

void UnholySpecialization::CastCorpseExplosion()
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(CORPSE_EXPLOSION) && HasAvailableCorpse())
    {
        bot->CastSpell(bot, CORPSE_EXPLOSION, false);
        _totalDamageDealt += 3000;
    }
}

void UnholySpecialization::CastSummonGargoyle()
{
    Player* bot = GetBot();
    if (bot && HasEnoughResource(SUMMON_GARGOYLE))
    {
        bot->CastSpell(bot, SUMMON_GARGOYLE, false);
        ConsumeResource(SUMMON_GARGOYLE);
    }
}

void UnholySpecialization::CastArmyOfTheDead()
{
    Player* bot = GetBot();
    if (bot && HasEnoughResource(ARMY_OF_THE_DEAD))
    {
        bot->CastSpell(bot, ARMY_OF_THE_DEAD, false);
        ConsumeResource(ARMY_OF_THE_DEAD);
    }
}

void UnholySpecialization::EnterUnholyPresence()
{
    Player* bot = GetBot();
    if (bot && bot->HasSpell(UNHOLY_PRESENCE) && !bot->HasAura(UNHOLY_PRESENCE))
        bot->CastSpell(bot, UNHOLY_PRESENCE, false);
}

bool UnholySpecialization::ShouldUseUnholyPresence()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(UNHOLY_PRESENCE) && !bot->HasAura(UNHOLY_PRESENCE);
}

void UnholySpecialization::UpdateCorpseManagement()
{
    uint32 now = getMSTime();
    if (now - _lastCorpseUpdate < CORPSE_UPDATE_INTERVAL) return;
    _lastCorpseUpdate = now;

    // Update available corpses for spells like Corpse Explosion
    _availableCorpses.clear();
    // Implementation would scan for nearby corpses
}

bool UnholySpecialization::HasAvailableCorpse() { return !_availableCorpses.empty(); }
Position UnholySpecialization::GetNearestCorpsePosition() { return _availableCorpses.empty() ? Position() : _availableCorpses[0]; }

} // namespace Playerbot