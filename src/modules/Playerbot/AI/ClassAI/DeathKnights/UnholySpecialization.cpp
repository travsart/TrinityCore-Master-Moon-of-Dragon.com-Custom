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
#include "Pet.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Item.h"
#include "CharmInfo.h"
#include "GameTime.h"
#include "Creature.h"

namespace Playerbot
{

UnholySpecialization::UnholySpecialization(Player* bot)
    : DeathKnightSpecialization(bot)
    , _hasActiveGhoul(false)
    , _ghoulHealth(0)
    , _maxGhoulHealth(0)
    , _ghoulGuid(ObjectGuid::Empty)
    , _lastGhoulCommand(0)
    , _lastGhoulSummon(0)
    , _lastGhoulHealing(0)
    , _ghoulCommandCooldown(0)
    , _suddenDoomActive(false)
    , _suddenDoomExpires(0)
    , _suddenDoomStacks(0)
    , _necroticStrikeActive(false)
    , _necroticStrikeExpires(0)
    , _unholyFrenzyActive(false)
    , _unholyFrenzyExpires(0)
    , _corpseExploderActive(false)
    , _corpseExploderExpires(0)
    , _lastProcCheck(0)
    , _summonGargoyleReady(0)
    , _armyOfTheDeadReady(0)
    , _darkTransformationReady(0)
    , _boneArmorReady(0)
    , _antiMagicShellReady(0)
    , _deathPactReady(0)
    , _lastSummonGargoyle(0)
    , _lastArmyOfTheDead(0)
    , _lastDarkTransformation(0)
    , _lastBoneArmor(0)
    , _lastAntiMagicShell(0)
    , _lastDeathPact(0)
    , _lastDiseaseSpread(0)
    , _lastCorpseUpdate(0)
    , _lastRotationCheck(0)
    , _rotationPhase(OPENING)
    , _combatStartTime(0)
    , _totalDamageDealt(0)
    , _diseaseDamage(0)
    , _procActivations(0)
    , _runicPowerSpent(0)
    , _runicPowerGenerated(0)
    , _scourgeStrikesUsed(0)
    , _deathCoilsUsed(0)
    , _pestilenceCount(0)
    , _minionsActive(0)
    , _lastThreatCheck(0)
    , _threatReduction(0)
    , _emergencyHealUsed(false)
    , _lastEmergencyHeal(0)
    , _aoeTargetCount(0)
    , _lastAoeCheck(0)
    , _priorityTarget(ObjectGuid::Empty)
    , _lastTargetSwitch(0)
    , _diseaseApplicationPhase(BLOOD_PLAGUE_FIRST)
    , _lastDiseaseCheck(0)
    , _multiTargetMode(false)
    , _executePhase(false)
    , _burstPhase(false)
    , _lastBurstCheck(0)
    , _conserveMode(false)
    , _lastResourceCheck(0)
{
    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Initializing for bot {}", bot->GetName().c_str());

    // Initialize rotation state
    _rotationPhase = OPENING;
    _diseaseApplicationPhase = BLOOD_PLAGUE_FIRST;

    // Reset all cooldowns
    _summonGargoyleReady = 0;
    _armyOfTheDeadReady = 0;
    _darkTransformationReady = 0;
    _boneArmorReady = 0;
    _antiMagicShellReady = 0;
    _deathPactReady = 0;

    // Initialize proc states
    _suddenDoomActive = false;
    _necroticStrikeActive = false;
    _unholyFrenzyActive = false;
    _corpseExploderActive = false;

    // Initialize combat metrics
    _totalDamageDealt = 0;
    _diseaseDamage = 0;
    _procActivations = 0;
    _runicPowerSpent = 0;
    _runicPowerGenerated = 0;
    _scourgeStrikesUsed = 0;
    _deathCoilsUsed = 0;
    _pestilenceCount = 0;
    _minionsActive = 0;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Initialization complete for bot {}", bot->GetName().c_str());
}

void UnholySpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    uint32 now = getMSTime();

    // Throttle rotation updates for performance
    if (now - _lastRotationCheck < ROTATION_UPDATE_INTERVAL)
        return;
    _lastRotationCheck = now;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: UpdateRotation for bot {} targeting {}",
                 bot->GetName().c_str(), target->GetName());

    // Update all management systems
    UpdateRuneManagement();
    UpdateRunicPowerManagement();
    UpdateDiseaseManagement();
    UpdateGhoulManagement();
    UpdateProcManagement();
    UpdateDiseaseSpread();
    UpdateCorpseManagement();
    UpdateThreatManagement();
    UpdateTargetPrioritization();
    UpdateCombatPhase();

    // Emergency survival checks
    if (HandleEmergencySurvival())
        return;

    // Ensure we're in Unholy Presence for optimal DPS
    if (ShouldUseUnholyPresence())
    {
        EnterUnholyPresence();
        return;
    }

    // Maintain essential minions
    if (HandleMinionManagement())
        return;

    // Handle interrupts and utility
    if (HandleUtilitySpells(target))
        return;

    // Use major defensive cooldowns when needed
    if (HandleDefensiveCooldowns())
        return;

    // Execute rotation based on current phase and situation
    switch (_rotationPhase)
    {
        case OPENING:
            if (ExecuteOpeningRotation(target))
                return;
            break;

        case DISEASE_APPLICATION:
            if (ExecuteDiseaseApplicationRotation(target))
                return;
            break;

        case BURST_PHASE:
            if (ExecuteBurstRotation(target))
                return;
            break;

        case SUSTAIN_PHASE:
            if (ExecuteSustainRotation(target))
                return;
            break;

        case AOE_PHASE:
            if (ExecuteAoeRotation(target))
                return;
            break;

        case EXECUTE_PHASE:
            if (ExecuteExecuteRotation(target))
                return;
            break;

        default:
            if (ExecuteSustainRotation(target))
                return;
            break;
    }

    // Fallback to basic attacks if nothing else worked
    if (HandleFallbackRotation(target))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: No rotation action taken for bot {}", bot->GetName().c_str());
}

void UnholySpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: UpdateBuffs for bot {}", bot->GetName().c_str());

    // Maintain Unholy Presence - highest priority for DPS
    if (!bot->HasAura(UNHOLY_PRESENCE) && bot->HasSpell(UNHOLY_PRESENCE))
    {
        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Unholy Presence for bot {}", bot->GetName().c_str());
        bot->CastSpell(bot, UNHOLY_PRESENCE, TRIGGERED_NONE);
        return;
    }

    // Maintain Bone Armor for survivability
    if (ShouldCastBoneArmor())
    {
        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Bone Armor for bot {}", bot->GetName().c_str());
        CastBoneArmor();
        return;
    }

    // Maintain Horn of Winter for stats
    if (!bot->HasAura(HORN_OF_WINTER) && bot->HasSpell(HORN_OF_WINTER))
    {
        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Horn of Winter for bot {}", bot->GetName().c_str());
        bot->CastSpell(bot, HORN_OF_WINTER, TRIGGERED_NONE);
        return;
    }

    // Apply weapon enchantments if not present
    UpdateWeaponEnchantments();

    // Group buffs if in a group
    if (Group* group = bot->GetGroup())
    {
        // Apply group-wide buffs when appropriate
        if (!bot->HasAura(HORN_OF_WINTER))
        {
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (member && member->IsInWorld() && bot->IsWithinDistInMap(member, 40.0f))
                {
                    if (!member->HasAura(HORN_OF_WINTER) && bot->HasSpell(HORN_OF_WINTER))
                    {
                        bot->CastSpell(bot, HORN_OF_WINTER, TRIGGERED_NONE);
                        break;
                    }
                }
            }
        }
    }

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: UpdateBuffs complete for bot {}", bot->GetName().c_str());
}

void UnholySpecialization::UpdateCooldowns(uint32 diff)
{
    // Update base cooldowns
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    // Update major cooldowns
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

    if (_boneArmorReady > diff)
        _boneArmorReady -= diff;
    else
        _boneArmorReady = 0;

    if (_antiMagicShellReady > diff)
        _antiMagicShellReady -= diff;
    else
        _antiMagicShellReady = 0;

    if (_deathPactReady > diff)
        _deathPactReady -= diff;
    else
        _deathPactReady = 0;

    // Update proc timers
    if (_suddenDoomExpires > diff)
        _suddenDoomExpires -= diff;
    else
    {
        _suddenDoomExpires = 0;
        _suddenDoomActive = false;
        _suddenDoomStacks = 0;
    }

    if (_necroticStrikeExpires > diff)
        _necroticStrikeExpires -= diff;
    else
    {
        _necroticStrikeExpires = 0;
        _necroticStrikeActive = false;
    }

    if (_unholyFrenzyExpires > diff)
        _unholyFrenzyExpires -= diff;
    else
    {
        _unholyFrenzyExpires = 0;
        _unholyFrenzyActive = false;
    }

    if (_corpseExploderExpires > diff)
        _corpseExploderExpires -= diff;
    else
    {
        _corpseExploderExpires = 0;
        _corpseExploderActive = false;
    }

    // Update management timers
    if (_lastGhoulCommand > diff)
        _lastGhoulCommand -= diff;
    else
        _lastGhoulCommand = 0;

    if (_ghoulCommandCooldown > diff)
        _ghoulCommandCooldown -= diff;
    else
        _ghoulCommandCooldown = 0;

    if (_lastGhoulHealing > diff)
        _lastGhoulHealing -= diff;
    else
        _lastGhoulHealing = 0;

    if (_lastDiseaseSpread > diff)
        _lastDiseaseSpread -= diff;
    else
        _lastDiseaseSpread = 0;

    if (_lastThreatCheck > diff)
        _lastThreatCheck -= diff;
    else
        _lastThreatCheck = 0;

    if (_lastEmergencyHeal > diff)
        _lastEmergencyHeal -= diff;
    else
        _lastEmergencyHeal = 0;

    if (_lastTargetSwitch > diff)
        _lastTargetSwitch -= diff;
    else
        _lastTargetSwitch = 0;

    if (_lastBurstCheck > diff)
        _lastBurstCheck -= diff;
    else
        _lastBurstCheck = 0;

    if (_lastResourceCheck > diff)
        _lastResourceCheck -= diff;
    else
        _lastResourceCheck = 0;

    // Update core systems
    RegenerateRunes(diff);
    UpdateDiseaseTimers(diff);
    UpdateRunicPowerDecay(diff);
}

bool UnholySpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    switch (spellId)
    {
        case SUMMON_GARGOYLE:
            return _summonGargoyleReady == 0;
        case ARMY_OF_THE_DEAD:
            return _armyOfTheDeadReady == 0;
        case DARK_TRANSFORMATION:
            return _darkTransformationReady == 0;
        case BONE_ARMOR:
            return _boneArmorReady == 0;
        case DeathKnightSpecialization::ANTI_MAGIC_SHELL:
            return _antiMagicShellReady == 0;
        case DEATH_PACT:
            return _deathPactReady == 0;
        default:
            return HasEnoughResource(spellId);
    }
}

void UnholySpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: OnCombatStart for bot {} targeting {}",
                 bot->GetName().c_str(), target ? target->GetName() : "unknown");

    _combatStartTime = getMSTime();
    _rotationPhase = OPENING;
    _diseaseApplicationPhase = BLOOD_PLAGUE_FIRST;

    // Enter Unholy Presence for maximum DPS
    if (ShouldUseUnholyPresence())
        EnterUnholyPresence();

    // Summon ghoul if not active
    if (!HasActiveGhoul() && bot->HasSpell(DeathKnightSpecialization::RAISE_DEAD))
        SummonGhoul();

    // Reset proc states
    _suddenDoomActive = false;
    _necroticStrikeActive = false;
    _unholyFrenzyActive = false;
    _corpseExploderActive = false;

    // Reset combat metrics
    _totalDamageDealt = 0;
    _diseaseDamage = 0;
    _scourgeStrikesUsed = 0;
    _deathCoilsUsed = 0;
    _pestilenceCount = 0;
    _emergencyHealUsed = false;
    _multiTargetMode = false;
    _executePhase = false;
    _burstPhase = false;
    _conserveMode = false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Combat initialization complete for bot {}", bot->GetName().c_str());
}

void UnholySpecialization::OnCombatEnd()
{
    TC_LOG_DEBUG("playerbot", "UnholySpecialization: OnCombatEnd for bot {}", GetBot()->GetName());

    // Reset all proc states
    _suddenDoomActive = false;
    _necroticStrikeActive = false;
    _unholyFrenzyActive = false;
    _corpseExploderActive = false;

    // Clear temporary states
    _cooldowns.clear();
    _activeDiseases.clear();
    _diseaseTargets.clear();
    _availableCorpses.clear();
    _priorityTarget = ObjectGuid::Empty;

    // Reset phases
    _rotationPhase = OPENING;
    _diseaseApplicationPhase = BLOOD_PLAGUE_FIRST;
    _multiTargetMode = false;
    _executePhase = false;
    _burstPhase = false;
    _conserveMode = false;
    _emergencyHealUsed = false;

    // Reset timing
    _combatStartTime = 0;
    _aoeTargetCount = 0;
    _minionsActive = 0;
    _threatReduction = 0;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Combat cleanup complete for bot {}", GetBot()->GetName());
}

bool UnholySpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case SCOURGE_STRIKE:
            return HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::UNHOLY, 1);
        case DeathKnightSpecialization::DEATH_COIL:
            return HasEnoughRunicPower(40);
        case BONE_ARMOR:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case PLAGUE_STRIKE:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case ICY_TOUCH:
            return HasAvailableRunes(RuneType::FROST, 1);
        case DeathKnightSpecialization::BLOOD_STRIKE:
            return HasAvailableRunes(RuneType::BLOOD, 1);
        case BLOOD_BOIL:
            return HasAvailableRunes(RuneType::BLOOD, 1);
        case PESTILENCE:
            return HasAvailableRunes(RuneType::BLOOD, 1);
        case DEATH_AND_DECAY:
            return HasAvailableRunes(RuneType::UNHOLY, 1) && HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::BLOOD, 1);
        case NECROTIC_STRIKE:
            return HasAvailableRunes(RuneType::UNHOLY, 1) && HasAvailableRunes(RuneType::FROST, 1);
        case DeathKnightSpecialization::DEATH_STRIKE:
            return HasAvailableRunes(RuneType::FROST, 1) && HasAvailableRunes(RuneType::UNHOLY, 1);
        case CORPSE_EXPLOSION:
            return HasAvailableCorpse();
        case SUMMON_GARGOYLE:
            return _summonGargoyleReady == 0 && HasEnoughRunicPower(60);
        case ARMY_OF_THE_DEAD:
            return _armyOfTheDeadReady == 0;
        case DARK_TRANSFORMATION:
            return _darkTransformationReady == 0 && HasActiveGhoul();
        case DeathKnightSpecialization::ANTI_MAGIC_SHELL:
            return _antiMagicShellReady == 0;
        case DeathKnightSpecialization::DEATH_PACT:
            return _deathPactReady == 0 && HasActiveGhoul();
        case DeathKnightSpecialization::RAISE_DEAD:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case DeathKnightSpecialization::DEATH_GRIP:
            return HasAvailableRunes(RuneType::UNHOLY, 1);
        case DeathKnightSpecialization::MIND_FREEZE:
            return true; // No resource cost, just cooldown
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
            _scourgeStrikesUsed++;
            break;
        case DeathKnightSpecialization::DEATH_COIL:
            SpendRunicPower(40);
            _deathCoilsUsed++;
            break;
        case BONE_ARMOR:
            ConsumeRunes(RuneType::UNHOLY, 1);
            _boneArmorReady = BONE_ARMOR_COOLDOWN;
            _lastBoneArmor = getMSTime();
            break;
        case PLAGUE_STRIKE:
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(10);
            break;
        case ICY_TOUCH:
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);
            break;
        case DeathKnightSpecialization::BLOOD_STRIKE:
            ConsumeRunes(RuneType::BLOOD, 1);
            GenerateRunicPower(10);
            break;
        case BLOOD_BOIL:
            ConsumeRunes(RuneType::BLOOD, 1);
            GenerateRunicPower(10);
            break;
        case PESTILENCE:
            ConsumeRunes(RuneType::BLOOD, 1);
            _pestilenceCount++;
            break;
        case DEATH_AND_DECAY:
            ConsumeRunes(RuneType::UNHOLY, 1);
            ConsumeRunes(RuneType::FROST, 1);
            ConsumeRunes(RuneType::BLOOD, 1);
            break;
        case NECROTIC_STRIKE:
            ConsumeRunes(RuneType::UNHOLY, 1);
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(15);
            break;
        case DeathKnightSpecialization::DEATH_STRIKE:
            ConsumeRunes(RuneType::FROST, 1);
            ConsumeRunes(RuneType::UNHOLY, 1);
            GenerateRunicPower(15);
            break;
        case SUMMON_GARGOYLE:
            SpendRunicPower(60);
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
        case DeathKnightSpecialization::ANTI_MAGIC_SHELL:
            _antiMagicShellReady = ANTI_MAGIC_SHELL_COOLDOWN;
            _lastAntiMagicShell = getMSTime();
            break;
        case DeathKnightSpecialization::DEATH_PACT:
            _deathPactReady = DEATH_PACT_COOLDOWN;
            _lastDeathPact = getMSTime();
            break;
        case DeathKnightSpecialization::RAISE_DEAD:
            ConsumeRunes(RuneType::UNHOLY, 1);
            break;
        case DeathKnightSpecialization::DEATH_GRIP:
            ConsumeRunes(RuneType::UNHOLY, 1);
            break;
        default:
            break;
    }

    // Set spell-specific cooldowns
    _cooldowns[spellId] = GetSpellCooldown(spellId);
}

Position UnholySpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Unholy Death Knights prefer melee range but with positioning for pet management
    float distance = UNHOLY_MELEE_RANGE * 0.8f; // Slightly closer than max melee range
    float angle = target->GetRelativeAngle(bot);

    // Adjust angle for pet positioning - avoid being directly behind target
    if (HasActiveGhoul())
    {
        angle += M_PI / 4; // 45 degrees offset to give pet room
    }

    // Prefer flanking positions for maximum DPS uptime
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        // Move to the side to avoid frontal cones
        angle += M_PI / 2;
    }

    Position optimalPos(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Optimal position calculated for bot {} at distance {:.2f}, angle {:.2f}",
                 bot->GetName().c_str(), distance, angle);

    return optimalPos;
}

float UnholySpecialization::GetOptimalRange(::Unit* target)
{
    return UNHOLY_MELEE_RANGE;
}

// ===== ROTATION PHASE IMPLEMENTATIONS =====

bool UnholySpecialization::ExecuteOpeningRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: ExecuteOpeningRotation for bot {}", bot->GetName().c_str());

    // Ensure ghoul is active first
    if (!HasActiveGhoul() && bot->HasSpell(DeathKnightSpecialization::RAISE_DEAD))
    {
        if (HasEnoughResource(DeathKnightSpecialization::RAISE_DEAD))
        {
            SummonGhoul();
            return true;
        }
    }

    // Apply Bone Armor if not present
    if (ShouldCastBoneArmor())
    {
        CastBoneArmor();
        return true;
    }

    // Use Army of the Dead on tough enemies (elite/boss)
    Creature* creature = target->ToCreature();
    if (target->GetCreatureType() == CREATURE_TYPE_HUMANOID || (creature && creature->IsElite()))
    {
        if (ShouldCastArmyOfTheDead())
        {
            CastArmyOfTheDead();
            return true;
        }
    }

    // Transition to disease application phase
    _rotationPhase = DISEASE_APPLICATION;
    return ExecuteDiseaseApplicationRotation(target);
}

bool UnholySpecialization::ExecuteDiseaseApplicationRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: ExecuteDiseaseApplicationRotation for bot {}", bot->GetName().c_str());

    // Check if we need to apply or refresh diseases
    bool needsBloodPlague = ShouldApplyDisease(target, DiseaseType::BLOOD_PLAGUE);
    bool needsFrostFever = ShouldApplyDisease(target, DiseaseType::FROST_FEVER);

    switch (_diseaseApplicationPhase)
    {
        case BLOOD_PLAGUE_FIRST:
            if (needsBloodPlague && HasEnoughResource(DeathKnightSpecialization::PLAGUE_STRIKE))
            {
                CastPlagueStrike(target);
                _diseaseApplicationPhase = FROST_FEVER_SECOND;
                return true;
            }
            else if (!needsBloodPlague)
            {
                _diseaseApplicationPhase = FROST_FEVER_SECOND;
            }
            break;

        case FROST_FEVER_SECOND:
            if (needsFrostFever && HasEnoughResource(DeathKnightSpecialization::ICY_TOUCH))
            {
                CastIcyTouch(target);
                _diseaseApplicationPhase = DISEASES_APPLIED;
                return true;
            }
            else if (!needsFrostFever)
            {
                _diseaseApplicationPhase = DISEASES_APPLIED;
            }
            break;

        case DISEASES_APPLIED:
            // Both diseases applied, check for spreading opportunities
            if (ShouldSpreadDiseases() && HasEnoughResource(PESTILENCE))
            {
                CastPestilence(target);
                _pestilenceCount++;
                // Transition to sustain phase after spreading
                _rotationPhase = SUSTAIN_PHASE;
                return true;
            }
            else
            {
                // Move to sustain rotation
                _rotationPhase = SUSTAIN_PHASE;
                return ExecuteSustainRotation(target);
            }
            break;
    }

    // If we can't apply diseases, fall back to basic rotation
    if (!needsBloodPlague && !needsFrostFever)
    {
        _rotationPhase = SUSTAIN_PHASE;
        return ExecuteSustainRotation(target);
    }

    return false;
}

bool UnholySpecialization::ExecuteBurstRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: ExecuteBurstRotation for bot {}", bot->GetName().c_str());

    // Use Summon Gargoyle for burst damage
    if (ShouldCastSummonGargoyle())
    {
        CastSummonGargoyle();
        return true;
    }

    // Use Dark Transformation on ghoul
    if (ShouldCastDarkTransformation())
    {
        CastDarkTransformation();
        return true;
    }

    // Consume Sudden Doom procs immediately
    if (_suddenDoomActive && HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
    {
        CastDeathCoil(target);
        ConsumeSuddenDoomProc();
        return true;
    }

    // Use Necrotic Strike with procs
    if (_necroticStrikeActive && HasEnoughResource(NECROTIC_STRIKE))
    {
        CastNecroticStrike(target);
        return true;
    }

    // Spam Scourge Strike during burst
    if (HasEnoughResource(SCOURGE_STRIKE))
    {
        CastScourgeStrike(target);
        return true;
    }

    // Use Death Coil to dump runic power
    if (GetRunicPower() >= 80 && HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
    {
        CastDeathCoil(target);
        return true;
    }

    // Transition back to sustain if no burst actions available
    _rotationPhase = SUSTAIN_PHASE;
    _burstPhase = false;
    return ExecuteSustainRotation(target);
}

bool UnholySpecialization::ExecuteSustainRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: ExecuteSustainRotation for bot {}", bot->GetName().c_str());

    // Maintain diseases - highest priority
    if (ShouldRefreshDiseases(target))
    {
        if (ShouldApplyDisease(target, DiseaseType::BLOOD_PLAGUE) && HasEnoughResource(DeathKnightSpecialization::PLAGUE_STRIKE))
        {
            CastPlagueStrike(target);
            return true;
        }
        if (ShouldApplyDisease(target, DiseaseType::FROST_FEVER) && HasEnoughResource(DeathKnightSpecialization::ICY_TOUCH))
        {
            CastIcyTouch(target);
            return true;
        }
    }

    // Use procs when available
    if (_suddenDoomActive && HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
    {
        CastDeathCoil(target);
        ConsumeSuddenDoomProc();
        return true;
    }

    // Scourge Strike - main damage ability
    if (HasEnoughResource(SCOURGE_STRIKE))
    {
        CastScourgeStrike(target);
        return true;
    }

    // Death Coil for runic power dump
    if (GetRunicPower() >= 60 && HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
    {
        CastDeathCoil(target);
        return true;
    }

    // Necrotic Strike as filler
    if (HasEnoughResource(NECROTIC_STRIKE))
    {
        CastNecroticStrike(target);
        return true;
    }

    // Blood Strike as last resort filler
    if (HasEnoughResource(DeathKnightSpecialization::BLOOD_STRIKE))
    {
        CastBloodStrike(target);
        return true;
    }

    return false;
}

bool UnholySpecialization::ExecuteAoeRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: ExecuteAoeRotation for bot {}", bot->GetName().c_str());

    // Death and Decay for major AoE
    if (ShouldCastDeathAndDecay() && HasEnoughResource(DEATH_AND_DECAY))
    {
        CastDeathAndDecay(target->GetPosition());
        return true;
    }

    // Spread diseases with Pestilence
    if (ShouldSpreadDiseases() && HasEnoughResource(PESTILENCE))
    {
        CastPestilence(target);
        return true;
    }

    // Blood Boil for AoE damage
    if (HasEnoughResource(BLOOD_BOIL))
    {
        CastBloodBoil(target);
        return true;
    }

    // Corpse Explosion if corpses available
    if (ShouldCastCorpseExplosion() && HasAvailableCorpse())
    {
        CastCorpseExplosion();
        return true;
    }

    // Fall back to single target if no AoE opportunities
    if (_aoeTargetCount <= 2)
    {
        _rotationPhase = SUSTAIN_PHASE;
        _multiTargetMode = false;
        return ExecuteSustainRotation(target);
    }

    return false;
}

bool UnholySpecialization::ExecuteExecuteRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: ExecuteExecuteRotation for bot {}", bot->GetName().c_str());

    // Spam Death Coil during execute phase
    if (HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
    {
        CastDeathCoil(target);
        return true;
    }

    // Use Scourge Strike with diseases
    if (HasDisease(target, DiseaseType::BLOOD_PLAGUE) && HasDisease(target, DiseaseType::FROST_FEVER))
    {
        if (HasEnoughResource(SCOURGE_STRIKE))
        {
            CastScourgeStrike(target);
            return true;
        }
    }

    // Necrotic Strike for execute
    if (HasEnoughResource(NECROTIC_STRIKE))
    {
        CastNecroticStrike(target);
        return true;
    }

    // Death Strike for healing if low
    if (bot->GetHealthPct() < 60.0f && HasEnoughResource(DeathKnightSpecialization::DEATH_STRIKE))
    {
        CastDeathStrike(target);
        return true;
    }

    return false;
}

bool UnholySpecialization::HandleFallbackRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: HandleFallbackRotation for bot {}", bot->GetName().c_str());

    // Basic melee range check
    if (bot->GetDistance(target) > UNHOLY_MELEE_RANGE)
    {
        // Use Death Grip to pull target
        if (ShouldUseDeathGrip(target) && HasEnoughResource(DeathKnightSpecialization::DEATH_GRIP))
        {
            CastDeathGrip(target);
            return true;
        }

        // Use Death Coil at range
        if (HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
        {
            CastDeathCoil(target);
            return true;
        }
    }

    // Auto-attack if nothing else is available
    if (bot->IsWithinMeleeRange(target))
    {
        if (!bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
        {
            bot->Attack(target, true);
            return true;
        }
    }

    return false;
}

// ===== UTILITY AND MANAGEMENT METHODS =====

bool UnholySpecialization::HandleEmergencySurvival()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    float healthPct = bot->GetHealthPct();

    // Emergency Death Pact for healing
    if (healthPct < 30.0f && !_emergencyHealUsed)
    {
        if (ShouldCastDeathPact())
        {
            CastDeathPact();
            _emergencyHealUsed = true;
            _lastEmergencyHeal = getMSTime();
            return true;
        }
    }

    // Anti-Magic Shell against magic damage
    if (healthPct < 50.0f && ShouldCastAntiMagicShell())
    {
        CastAntiMagicShell();
        return true;
    }

    // Death Strike for self-healing
    if (healthPct < 60.0f && bot->IsInCombat())
    {
        ::Unit* target = ObjectAccessor::GetUnit(*bot, bot->GetTarget());
        if (target && HasEnoughResource(DeathKnightSpecialization::DEATH_STRIKE))
        {
            CastDeathStrike(target);
            return true;
        }
    }

    return false;
}

bool UnholySpecialization::HandleMinionManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Summon ghoul if not active
    if (!HasActiveGhoul() && bot->HasSpell(DeathKnightSpecialization::RAISE_DEAD))
    {
        if (HasEnoughResource(DeathKnightSpecialization::RAISE_DEAD))
        {
            SummonGhoul();
            return true;
        }
    }

    // Use Dark Transformation when appropriate
    if (HasActiveGhoul() && ShouldCastDarkTransformation())
    {
        CastDarkTransformation();
        return true;
    }

    return false;
}

bool UnholySpecialization::HandleUtilitySpells(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    // Interrupt casting
    if (target->HasUnitState(UNIT_STATE_CASTING) && bot->HasSpell(MIND_FREEZE))
    {
        if (CanUseAbility(DeathKnightSpecialization::MIND_FREEZE))
        {
            CastMindFreeze(target);
            return true;
        }
    }

    // Death Grip to pull targets
    if (ShouldUseDeathGrip(target) && HasEnoughResource(DeathKnightSpecialization::DEATH_GRIP))
    {
        CastDeathGrip(target);
        return true;
    }

    return false;
}

bool UnholySpecialization::HandleDefensiveCooldowns()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    float healthPct = bot->GetHealthPct();

    // Anti-Magic Shell against heavy magic damage
    if (healthPct < 70.0f && ShouldCastAntiMagicShell())
    {
        CastAntiMagicShell();
        return true;
    }

    // Bone Armor for physical damage reduction
    if (!bot->HasAura(BONE_ARMOR) && ShouldCastBoneArmor())
    {
        CastBoneArmor();
        return true;
    }

    return false;
}

void UnholySpecialization::UpdateCombatPhase()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    uint32 now = getMSTime();
    uint32 combatDuration = now - _combatStartTime;
    ::Unit* target = ObjectAccessor::GetUnit(*bot, bot->GetTarget());

    if (!target)
        return;

    float targetHealthPct = target->GetHealthPct();

    // Determine current phase based on combat conditions
    UnholyRotationPhase newPhase = _rotationPhase;

    // Execute phase for low health targets
    if (targetHealthPct < 35.0f)
    {
        newPhase = EXECUTE_PHASE;
        _executePhase = true;
    }
    // AoE phase for multiple targets
    else if (_aoeTargetCount > 2)
    {
        newPhase = AOE_PHASE;
        _multiTargetMode = true;
    }
    // Burst phase for major cooldown usage
    else if (combatDuration > 10000 && !_burstPhase && targetHealthPct > 60.0f)
    {
        if (ShouldCastSummonGargoyle() || ShouldCastDarkTransformation())
        {
            newPhase = BURST_PHASE;
            _burstPhase = true;
        }
    }
    // Disease application phase if diseases are missing
    else if (!HasDisease(target, DiseaseType::BLOOD_PLAGUE) || !HasDisease(target, DiseaseType::FROST_FEVER))
    {
        newPhase = DISEASE_APPLICATION;
        _diseaseApplicationPhase = BLOOD_PLAGUE_FIRST;
    }
    // Default to sustain phase
    else
    {
        newPhase = SUSTAIN_PHASE;
    }

    if (newPhase != _rotationPhase)
    {
        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Phase transition for bot {} from {} to {}",
                     bot->GetName().c_str(), static_cast<int>(_rotationPhase), static_cast<int>(newPhase));
        _rotationPhase = newPhase;
    }
}

void UnholySpecialization::UpdateTargetPrioritization()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastTargetSwitch < TARGET_SWITCH_INTERVAL)
        return;

    // Update AoE target count
    _aoeTargetCount = 0;
    for (auto& attacker : bot->getAttackers())
    {
        if (attacker->IsWithinDistInMap(bot, AOE_CHECK_RANGE))
            _aoeTargetCount++;
    }

    _lastTargetSwitch = now;
}

void UnholySpecialization::UpdateThreatManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastThreatCheck < THREAT_CHECK_INTERVAL)
        return;

    // Unholy DKs should be careful about threat in groups
    if (Group* group = bot->GetGroup())
    {
        // TODO: Implement threat reduction if needed
        // This could involve using less aggressive abilities or spacing out attacks
    }

    _lastThreatCheck = now;
}

// ===== CORE SYSTEM IMPLEMENTATIONS =====

void UnholySpecialization::UpdateRuneManagement()
{
    RegenerateRunes(0);
}

bool UnholySpecialization::HasAvailableRunes(RuneType type, uint32 count)
{
    return GetAvailableRunes(type) >= count;
}

void UnholySpecialization::ConsumeRunes(RuneType type, uint32 count)
{
    uint32 consumed = 0;
    for (auto& rune : _runes)
    {
        if (rune.type == type && rune.IsReady() && consumed < count)
        {
            rune.Use();
            consumed++;
            TC_LOG_DEBUG("playerbot", "UnholySpecialization: Consumed {} rune for bot {}",
                         static_cast<int>(type), GetBot()->GetName());
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
    if (!bot)
        return;

    // Don't decay runic power during combat
    if (bot->IsInCombat())
        return;

    uint32 now = getMSTime();
    if (_lastRunicPowerDecay == 0)
        _lastRunicPowerDecay = now;

    UpdateRunicPowerDecay(now - _lastRunicPowerDecay);
    _lastRunicPowerDecay = now;
}

void UnholySpecialization::UpdateRunicPowerDecay(uint32 diff)
{
    Player* bot = GetBot();
    if (!bot || bot->IsInCombat())
        return;

    if (diff >= 1000) // Decay every second out of combat
    {
        uint32 decay = (diff / 1000) * RUNIC_POWER_DECAY_RATE;
        if (_runicPower > decay)
        {
            _runicPower -= decay;
        }
        else
        {
            _runicPower = 0;
        }
    }
}

void UnholySpecialization::GenerateRunicPower(uint32 amount)
{
    _runicPower = std::min(_runicPower + amount, _maxRunicPower);
    _runicPowerGenerated += amount;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Generated {} runic power for bot {} (total: {})",
                 amount, GetBot()->GetName(), _runicPower);
}

void UnholySpecialization::SpendRunicPower(uint32 amount)
{
    if (_runicPower >= amount)
    {
        _runicPower -= amount;
        _runicPowerSpent += amount;

        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Spent {} runic power for bot {} (remaining: {})",
                     amount, GetBot()->GetName(), _runicPower);
    }
}

uint32 UnholySpecialization::GetRunicPower() const
{
    return _runicPower;
}

bool UnholySpecialization::HasEnoughRunicPower(uint32 required) const
{
    return _runicPower >= required;
}

void UnholySpecialization::UpdateDiseaseManagement()
{
    UpdateDiseaseTimers(0);
    RefreshExpringDiseases();
}

void UnholySpecialization::ApplyDisease(::Unit* target, DiseaseType type, uint32 spellId)
{
    if (!target)
        return;

    uint32 duration = 21000; // 21 seconds base duration
    uint32 damage = 400; // Base damage per tick

    // Adjust based on disease type
    switch (type)
    {
        case DiseaseType::BLOOD_PLAGUE:
            damage = 500; // Higher damage for Blood Plague
            break;
        case DiseaseType::FROST_FEVER:
            damage = 300; // Lower damage but slowing effect
            break;
        default:
            break;
    }

    DiseaseInfo disease(type, spellId, duration);
    _activeDiseases[target->GetGUID()].push_back(disease);

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Applied disease {} to target {} for bot {}",
                 static_cast<int>(type), target->GetName(), GetBot()->GetName());
}

bool UnholySpecialization::HasDisease(::Unit* target, DiseaseType type) const
{
    if (!target)
        return false;

    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
    {
        if (disease.type == type && disease.IsActive())
            return true;
    }
    return false;
}

bool UnholySpecialization::ShouldApplyDisease(::Unit* target, DiseaseType type) const
{
    if (!target)
        return false;

    return !HasDisease(target, type) || GetDiseaseRemainingTime(target, type) < DISEASE_REFRESH_THRESHOLD;
}

bool UnholySpecialization::ShouldRefreshDiseases(::Unit* target) const
{
    if (!target)
        return false;

    return ShouldApplyDisease(target, DiseaseType::BLOOD_PLAGUE) ||
           ShouldApplyDisease(target, DiseaseType::FROST_FEVER);
}

void UnholySpecialization::RefreshExpringDiseases()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    for (auto& targetDiseases : _activeDiseases)
    {
        ::Unit* target = ObjectAccessor::GetUnit(*bot, targetDiseases.first);
        if (!target)
            continue;

        for (auto& disease : targetDiseases.second)
        {
            if (disease.needsRefresh || disease.remainingTime < DISEASE_REFRESH_THRESHOLD)
            {
                if (disease.type == DiseaseType::BLOOD_PLAGUE && HasEnoughResource(DeathKnightSpecialization::PLAGUE_STRIKE))
                {
                    CastPlagueStrike(target);
                    return;
                }
                else if (disease.type == DiseaseType::FROST_FEVER && HasEnoughResource(DeathKnightSpecialization::ICY_TOUCH))
                {
                    CastIcyTouch(target);
                    return;
                }
            }
        }
    }
}

uint32 UnholySpecialization::GetDiseaseRemainingTime(::Unit* target, DiseaseType type) const
{
    if (!target)
        return 0;

    auto diseases = GetActiveDiseases(target);
    for (const auto& disease : diseases)
    {
        if (disease.type == type && disease.IsActive())
            return disease.remainingTime;
    }
    return 0;
}

std::vector<DiseaseInfo> UnholySpecialization::GetActiveDiseases(::Unit* target) const
{
    if (!target)
        return {};

    auto it = _activeDiseases.find(target->GetGUID());
    if (it != _activeDiseases.end())
        return it->second;

    return {};
}

void UnholySpecialization::UpdateDiseaseSpread()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastDiseaseSpread < DISEASE_SPREAD_INTERVAL)
        return;

    _diseaseTargets.clear();

    // Find nearby enemies for potential disease spreading
    for (auto& attacker : bot->getAttackers())
    {
        if (attacker->IsWithinDistInMap(bot, DISEASE_SPREAD_RANGE))
        {
            _diseaseTargets.push_back(attacker);
        }
    }

    _lastDiseaseSpread = now;
}

bool UnholySpecialization::ShouldSpreadDiseases()
{
    return _diseaseTargets.size() > 1;
}

void UnholySpecialization::SpreadDiseases(::Unit* target)
{
    if (!target || !HasEnoughResource(PESTILENCE))
        return;

    CastPestilence(target);

    // Apply diseases to nearby targets
    for (::Unit* nearbyTarget : _diseaseTargets)
    {
        if (nearbyTarget != target && nearbyTarget->IsWithinDistInMap(target, 10.0f))
        {
            if (HasDisease(target, DiseaseType::BLOOD_PLAGUE))
                ApplyDisease(nearbyTarget, DiseaseType::BLOOD_PLAGUE, PLAGUE_STRIKE);
            if (HasDisease(target, DiseaseType::FROST_FEVER))
                ApplyDisease(nearbyTarget, DiseaseType::FROST_FEVER, ICY_TOUCH);
        }
    }
}

std::vector<::Unit*> UnholySpecialization::GetDiseaseTargets()
{
    return _diseaseTargets;
}

void UnholySpecialization::UpdateGhoulManagement()
{
    UpdatePetManagement();
    ManageGhoulHealth();
    CommandGhoulIfNeeded();
}

void UnholySpecialization::UpdatePetManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if ghoul is still alive and active
    Pet* pet = bot->GetPet();
    if (pet && pet->IsAlive())
    {
        _hasActiveGhoul = true;
        _ghoulGuid = pet->GetGUID();
        _ghoulHealth = pet->GetHealth();
        _maxGhoulHealth = pet->GetMaxHealth();
        _minionsActive = 1;
    }
    else
    {
        _hasActiveGhoul = false;
        _ghoulGuid = ObjectGuid::Empty;
        _ghoulHealth = 0;
        _maxGhoulHealth = 0;
        _minionsActive = 0;
    }
}

void UnholySpecialization::SummonGhoul()
{
    Player* bot = GetBot();
    if (!bot || !bot->HasSpell(DeathKnightSpecialization::RAISE_DEAD) || HasActiveGhoul())
        return;

    if (HasEnoughResource(DeathKnightSpecialization::RAISE_DEAD))
    {
        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Summoning ghoul for bot {}", bot->GetName().c_str());
        bot->CastSpell(bot, DeathKnightSpecialization::RAISE_DEAD, TRIGGERED_NONE);
        ConsumeResource(DeathKnightSpecialization::RAISE_DEAD);
        _lastGhoulSummon = getMSTime();
    }
}

void UnholySpecialization::CommandGhoulIfNeeded()
{
    if (!HasActiveGhoul() || _ghoulCommandCooldown > 0)
        return;

    Player* bot = GetBot();
    ::Unit* target = bot ? ObjectAccessor::GetUnit(*bot, bot->GetTarget()) : nullptr;

    if (target && target->IsHostileTo(bot))
    {
        CommandGhoul(target);
        _ghoulCommandCooldown = GHOUL_COMMAND_INTERVAL;
    }
}

void UnholySpecialization::CommandGhoul(::Unit* target)
{
    if (!target || !HasActiveGhoul())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && pet->IsAlive())
    {
        // Command pet to attack target
        pet->GetCharmInfo()->SetCommandState(COMMAND_ATTACK);
        pet->Attack(target, true);

        TC_LOG_DEBUG("playerbot", "UnholySpecialization: Commanded ghoul to attack {} for bot {}",
                     target->GetName(), bot->GetName().c_str());
    }
}

bool UnholySpecialization::HasActiveGhoul()
{
    return _hasActiveGhoul;
}

void UnholySpecialization::ManageGhoulHealth()
{
    if (!HasActiveGhoul())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (!pet)
        return;

    float ghoulHealthPct = (float)pet->GetHealth() / pet->GetMaxHealth() * 100.0f;

    // Use Death Pact if ghoul is very low on health and we need healing
    if (ghoulHealthPct < 20.0f && bot->GetHealthPct() < 40.0f)
    {
        uint32 now = getMSTime();
        if (now - _lastGhoulHealing > GHOUL_HEAL_INTERVAL && ShouldCastDeathPact())
        {
            CastDeathPact();
            _lastGhoulHealing = now;
        }
    }
}

void UnholySpecialization::UpdateProcManagement()
{
    uint32 now = getMSTime();
    if (now - _lastProcCheck < PROC_CHECK_INTERVAL)
        return;

    _lastProcCheck = now;

    Player* bot = GetBot();
    if (!bot)
        return;

    // Check for Sudden Doom proc
    if (bot->HasAura(SUDDEN_DOOM))
    {
        if (!_suddenDoomActive)
        {
            _suddenDoomActive = true;
            _suddenDoomExpires = now + SUDDEN_DOOM_DURATION;
            _suddenDoomStacks = 1; // Could check actual stacks
            _procActivations++;

            TC_LOG_DEBUG("playerbot", "UnholySpecialization: Sudden Doom proc activated for bot {}", bot->GetName().c_str());
        }
    }
    else if (_suddenDoomActive && now >= _suddenDoomExpires)
    {
        _suddenDoomActive = false;
        _suddenDoomStacks = 0;
    }

    // Check for Necrotic Strike proc (if applicable)
    if (bot->HasAura(NECROTIC_STRIKE))
    {
        if (!_necroticStrikeActive)
        {
            _necroticStrikeActive = true;
            _necroticStrikeExpires = now + NECROTIC_STRIKE_DURATION;
        }
    }

    // Check for Unholy Frenzy proc
    if (bot->HasAura(DeathKnightSpecialization::UNHOLY_FRENZY))
    {
        if (!_unholyFrenzyActive)
        {
            _unholyFrenzyActive = true;
            _unholyFrenzyExpires = now + UNHOLY_FRENZY_DURATION;
        }
    }
}

bool UnholySpecialization::HasSuddenDoomProc()
{
    return _suddenDoomActive;
}

void UnholySpecialization::ConsumeSuddenDoomProc()
{
    _suddenDoomActive = false;
    _suddenDoomExpires = 0;
    _suddenDoomStacks = 0;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Consumed Sudden Doom proc for bot {}", GetBot()->GetName());
}

void UnholySpecialization::UpdateCorpseManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();
    if (now - _lastCorpseUpdate < CORPSE_UPDATE_INTERVAL)
        return;

    _lastCorpseUpdate = now;

    // Clear old corpse positions
    _availableCorpses.clear();

    // Scan for nearby corpses (this would need proper implementation based on TrinityCore's corpse system)
    // For now, we'll assume corpses are available if there are recently killed creatures nearby
    if (bot->getAttackers().size() > 0)
    {
        // Simulate available corpses for Corpse Explosion
        _availableCorpses.push_back(bot->GetPosition());
    }
}

bool UnholySpecialization::HasAvailableCorpse()
{
    return !_availableCorpses.empty();
}

Position UnholySpecialization::GetNearestCorpsePosition()
{
    if (_availableCorpses.empty())
        return Position();

    return _availableCorpses[0];
}

void UnholySpecialization::UpdateWeaponEnchantments()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check main hand weapon
    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (mainHand)
    {
        // Apply appropriate weapon enchantment (e.g., Fallen Crusader, Cinderglacier)
        // This would need specific implementation based on available enchantments
    }

    // Check off hand weapon if dual wielding
    Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (offHand && offHand->GetTemplate()->GetInventoryType() == INVTYPE_WEAPON)
    {
        // Apply appropriate off-hand enchantment
    }
}

uint32 UnholySpecialization::GetSpellCooldown(uint32 spellId) const
{
    // Return appropriate cooldowns for spells
    switch (spellId)
    {
        case DeathKnightSpecialization::MIND_FREEZE:
            return 10000; // 10 seconds
        case DeathKnightSpecialization::DEATH_GRIP:
            return 35000; // 35 seconds
        case DeathKnightSpecialization::ANTI_MAGIC_SHELL:
            return 45000; // 45 seconds
        case CORPSE_EXPLOSION:
            return 5000;  // 5 seconds
        default:
            return 1500;  // Default 1.5 second GCD
    }
}

// ===== SPELL CASTING METHODS =====

bool UnholySpecialization::ShouldCastScourgeStrike(::Unit* target)
{
    return target && GetBot()->IsWithinMeleeRange(target) &&
           HasEnoughResource(SCOURGE_STRIKE) &&
           (HasDisease(target, DiseaseType::BLOOD_PLAGUE) || HasDisease(target, DiseaseType::FROST_FEVER));
}

bool UnholySpecialization::ShouldCastDeathCoil(::Unit* target)
{
    return target && HasEnoughResource(DeathKnightSpecialization::DEATH_COIL) &&
           (GetRunicPower() >= 60 || _suddenDoomActive);
}

bool UnholySpecialization::ShouldCastBoneArmor()
{
    Player* bot = GetBot();
    return bot && !bot->HasAura(BONE_ARMOR) && HasEnoughResource(BONE_ARMOR) && _boneArmorReady == 0;
}

bool UnholySpecialization::ShouldCastCorpseExplosion()
{
    return HasAvailableCorpse() && GetBot()->getAttackers().size() > 1;
}

bool UnholySpecialization::ShouldCastSummonGargoyle()
{
    Player* bot = GetBot();
    return bot && _summonGargoyleReady == 0 && bot->IsInCombat() &&
           HasEnoughResource(SUMMON_GARGOYLE) && bot->GetHealthPct() > 50.0f;
}

bool UnholySpecialization::ShouldCastArmyOfTheDead()
{
    Player* bot = GetBot();
    return bot && _armyOfTheDeadReady == 0 && bot->IsInCombat() &&
           HasEnoughResource(ARMY_OF_THE_DEAD) && bot->getAttackers().size() > 2;
}

bool UnholySpecialization::ShouldCastDarkTransformation()
{
    return _darkTransformationReady == 0 && HasActiveGhoul() && GetBot()->IsInCombat();
}

bool UnholySpecialization::ShouldCastAntiMagicShell()
{
    Player* bot = GetBot();
    return bot && _antiMagicShellReady == 0 && bot->GetHealthPct() < 60.0f;
}

bool UnholySpecialization::ShouldCastDeathPact()
{
    Player* bot = GetBot();
    return bot && _deathPactReady == 0 && HasActiveGhoul() && bot->GetHealthPct() < 50.0f;
}


void UnholySpecialization::CastScourgeStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(SCOURGE_STRIKE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Scourge Strike on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, SCOURGE_STRIKE, TRIGGERED_NONE);
    ConsumeResource(SCOURGE_STRIKE);

    // Calculate bonus damage based on diseases
    uint32 baseDamage = 3500;
    auto diseases = GetActiveDiseases(target);
    uint32 diseaseBonus = diseases.size() * 500;

    _totalDamageDealt += baseDamage + diseaseBonus;
    _diseaseDamage += diseaseBonus;
}

void UnholySpecialization::CastDeathCoil(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(DeathKnightSpecialization::DEATH_COIL))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Death Coil on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    if (target->IsHostileTo(bot))
    {
        bot->CastSpell(target, DeathKnightSpecialization::DEATH_COIL, TRIGGERED_NONE);
        _totalDamageDealt += 2000;

        // Bonus damage if Sudden Doom is active
        if (_suddenDoomActive)
        {
            _totalDamageDealt += 1000;
        }
    }
    else
    {
        // Heal friendly target
        bot->CastSpell(target, DeathKnightSpecialization::DEATH_COIL, TRIGGERED_NONE);
    }

    ConsumeResource(DeathKnightSpecialization::DEATH_COIL);
}

void UnholySpecialization::CastBoneArmor()
{
    Player* bot = GetBot();
    if (!bot || !HasEnoughResource(BONE_ARMOR))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Bone Armor for bot {}", bot->GetName().c_str());

    bot->CastSpell(bot, BONE_ARMOR, TRIGGERED_NONE);
    ConsumeResource(BONE_ARMOR);
}

void UnholySpecialization::CastPlagueStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(DeathKnightSpecialization::PLAGUE_STRIKE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Plague Strike on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, PLAGUE_STRIKE, TRIGGERED_NONE);
    ConsumeResource(PLAGUE_STRIKE);
    ApplyDisease(target, DiseaseType::BLOOD_PLAGUE, PLAGUE_STRIKE);
    _totalDamageDealt += 1800;
}

void UnholySpecialization::CastIcyTouch(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(DeathKnightSpecialization::ICY_TOUCH))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Icy Touch on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, ICY_TOUCH, TRIGGERED_NONE);
    ConsumeResource(ICY_TOUCH);
    ApplyDisease(target, DiseaseType::FROST_FEVER, ICY_TOUCH);
    _totalDamageDealt += 1500;
}

void UnholySpecialization::CastBloodStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(DeathKnightSpecialization::BLOOD_STRIKE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Blood Strike on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, DeathKnightSpecialization::BLOOD_STRIKE, TRIGGERED_NONE);
    ConsumeResource(DeathKnightSpecialization::BLOOD_STRIKE);
    _totalDamageDealt += 2000;

    // Bonus damage per disease
    auto diseases = GetActiveDiseases(target);
    uint32 diseaseBonus = diseases.size() * 200;
    _totalDamageDealt += diseaseBonus;
    _diseaseDamage += diseaseBonus;
}

void UnholySpecialization::CastBloodBoil(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(BLOOD_BOIL))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Blood Boil on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, BLOOD_BOIL, TRIGGERED_NONE);
    ConsumeResource(BLOOD_BOIL);
    _totalDamageDealt += 1200 * _aoeTargetCount; // AoE damage
}

void UnholySpecialization::CastPestilence(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(PESTILENCE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Pestilence on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, PESTILENCE, TRIGGERED_NONE);
    ConsumeResource(PESTILENCE);
}

void UnholySpecialization::CastNecroticStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(NECROTIC_STRIKE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Necrotic Strike on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, NECROTIC_STRIKE, TRIGGERED_NONE);
    ConsumeResource(NECROTIC_STRIKE);
    _totalDamageDealt += 2500;
}

void UnholySpecialization::CastDeathStrike(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(DeathKnightSpecialization::DEATH_STRIKE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Death Strike on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, DeathKnightSpecialization::DEATH_STRIKE, TRIGGERED_NONE);
    ConsumeResource(DeathKnightSpecialization::DEATH_STRIKE);
    _totalDamageDealt += 2200;

    // Heal for percentage of recent damage taken
    uint32 healing = bot->GetMaxHealth() * 0.15f; // 15% of max health
    bot->SetHealth(std::min(bot->GetHealth() + healing, bot->GetMaxHealth()));
}

void UnholySpecialization::CastCorpseExplosion()
{
    Player* bot = GetBot();
    if (!bot || !bot->HasSpell(CORPSE_EXPLOSION) || !HasAvailableCorpse())
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Corpse Explosion for bot {}", bot->GetName().c_str());

    bot->CastSpell(bot, CORPSE_EXPLOSION, TRIGGERED_NONE);
    ConsumeResource(CORPSE_EXPLOSION);
    _totalDamageDealt += 3000 * _aoeTargetCount;
}

void UnholySpecialization::CastSummonGargoyle()
{
    Player* bot = GetBot();
    if (!bot || !HasEnoughResource(SUMMON_GARGOYLE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Summon Gargoyle for bot {}", bot->GetName().c_str());

    bot->CastSpell(bot, SUMMON_GARGOYLE, TRIGGERED_NONE);
    ConsumeResource(SUMMON_GARGOYLE);
    _minionsActive++;
}

void UnholySpecialization::CastArmyOfTheDead()
{
    Player* bot = GetBot();
    if (!bot || !HasEnoughResource(ARMY_OF_THE_DEAD))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Army of the Dead for bot {}", bot->GetName().c_str());

    bot->CastSpell(bot, ARMY_OF_THE_DEAD, TRIGGERED_NONE);
    ConsumeResource(ARMY_OF_THE_DEAD);
    _minionsActive += 8; // Army summons 8 skeletons
}

void UnholySpecialization::CastDarkTransformation()
{
    Player* bot = GetBot();
    if (!bot || !HasActiveGhoul() || !CanUseAbility(DARK_TRANSFORMATION))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Dark Transformation for bot {}", bot->GetName().c_str());

    Pet* pet = bot->GetPet();
    if (pet)
    {
        bot->CastSpell(pet, DARK_TRANSFORMATION, TRIGGERED_NONE);
        ConsumeResource(DARK_TRANSFORMATION);
    }
}

void UnholySpecialization::CastAntiMagicShell()
{
    Player* bot = GetBot();
    if (!bot || !CanUseAbility(DeathKnightSpecialization::ANTI_MAGIC_SHELL))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Anti-Magic Shell for bot {}", bot->GetName().c_str());

    bot->CastSpell(bot, DeathKnightSpecialization::ANTI_MAGIC_SHELL, TRIGGERED_NONE);
    ConsumeResource(DeathKnightSpecialization::ANTI_MAGIC_SHELL);
}

void UnholySpecialization::CastDeathPact()
{
    Player* bot = GetBot();
    if (!bot || !HasActiveGhoul() || !CanUseAbility(DeathKnightSpecialization::DEATH_PACT))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Death Pact for bot {}", bot->GetName().c_str());

    Pet* pet = bot->GetPet();
    if (pet)
    {
        uint32 healing = pet->GetHealth(); // Heal for pet's current health
        bot->CastSpell(bot, DeathKnightSpecialization::DEATH_PACT, TRIGGERED_NONE);
        ConsumeResource(DeathKnightSpecialization::DEATH_PACT);

        // Heal the death knight
        bot->SetHealth(std::min(bot->GetHealth() + healing, bot->GetMaxHealth()));

        // Pet is sacrificed
        _hasActiveGhoul = false;
        _ghoulGuid = ObjectGuid::Empty;
        _minionsActive = std::max(0u, _minionsActive > 0 ? _minionsActive - 1 : 0);
    }
}

void UnholySpecialization::CastDeathGrip(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(DeathKnightSpecialization::DEATH_GRIP))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Death Grip for bot {}", bot->GetName().c_str());

    bot->CastSpell(target, DeathKnightSpecialization::DEATH_GRIP, TRIGGERED_NONE);
    ConsumeResource(DeathKnightSpecialization::DEATH_GRIP);
}

void UnholySpecialization::CastMindFreeze(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !CanUseAbility(DeathKnightSpecialization::MIND_FREEZE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Mind Freeze on {} for bot {}",
                 target->GetName(), bot->GetName().c_str());

    bot->CastSpell(target, DeathKnightSpecialization::MIND_FREEZE, TRIGGERED_NONE);
    _cooldowns[DeathKnightSpecialization::MIND_FREEZE] = GetSpellCooldown(DeathKnightSpecialization::MIND_FREEZE);
}

void UnholySpecialization::EnterUnholyPresence()
{
    Player* bot = GetBot();
    if (!bot || !bot->HasSpell(UNHOLY_PRESENCE) || bot->HasAura(UNHOLY_PRESENCE))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Entering Unholy Presence for bot {}", bot->GetName().c_str());

    bot->CastSpell(bot, UNHOLY_PRESENCE, TRIGGERED_NONE);
}

bool UnholySpecialization::ShouldUseUnholyPresence()
{
    Player* bot = GetBot();
    return bot && bot->HasSpell(UNHOLY_PRESENCE) && !bot->HasAura(UNHOLY_PRESENCE);
}

// ===== DEATH AND DECAY SPECIFIC METHODS =====

void UnholySpecialization::UpdateDeathAndDecay()
{
    // Death and Decay handling is integrated into the AoE rotation
}

bool UnholySpecialization::ShouldCastDeathAndDecay() const
{
    return _aoeTargetCount > 2; // Resource check will be done when actually casting
}

void UnholySpecialization::CastDeathAndDecay(Position targetPos)
{
    Player* bot = GetBot();
    if (!bot || !bot->HasSpell(DEATH_AND_DECAY) || !HasEnoughResource(DEATH_AND_DECAY))
        return;

    TC_LOG_DEBUG("playerbot", "UnholySpecialization: Casting Death and Decay for bot {}", bot->GetName().c_str());

    bot->CastSpell(targetPos, DEATH_AND_DECAY, TRIGGERED_NONE);
    ConsumeResource(DEATH_AND_DECAY);
    _totalDamageDealt += 2000 * _aoeTargetCount; // Estimated AoE damage
}

} // namespace Playerbot