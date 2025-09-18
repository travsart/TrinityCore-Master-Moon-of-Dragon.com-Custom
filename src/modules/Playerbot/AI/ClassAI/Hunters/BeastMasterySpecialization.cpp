/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BeastMasterySpecialization.h"
#include "Player.h"
#include "Pet.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Item.h"
#include "ScriptMgr.h"

namespace Playerbot
{

BeastMasterySpecialization::BeastMasterySpecialization(Player* bot)
    : HunterSpecialization(bot)
    , _bmRotationPhase(BMRotationPhase::OPENING)
    , _currentPetStrategy(PetStrategy::AGGRESSIVE)
    , _bestialWrathReady(0)
    , _intimidationReady(0)
    , _callOfTheWildReady(0)
    , _silencingShotReady(0)
    , _mastersCallReady(0)
    , _lastBestialWrath(0)
    , _lastIntimidation(0)
    , _lastCallOfTheWild(0)
    , _lastSilencingShot(0)
    , _lastMastersCall(0)
    , _lastPetFeed(0)
    , _lastPetHappinessCheck(0)
    , _lastPetCommand(0)
    , _lastBurstCheck(0)
    , _petReviveAttempts(0)
    , _petInBurstMode(false)
    , _emergencyModeActive(false)
    , _multiTargetCount(0)
    , _totalPetDamage(0)
    , _totalHunterDamage(0)
    , _petDpsRatio(0.6f)
    , _steadyShotCount(0)
    , _arcaneShotCount(0)
    , _killShotCount(0)
    , _petPositionUpdateTime(0)
{
    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Initializing for bot {}", bot->GetName());

    // Initialize Beast Mastery specific state
    _bmRotationPhase = BMRotationPhase::OPENING;
    _currentPetStrategy = PetStrategy::AGGRESSIVE;

    // Reset all cooldowns
    _bestialWrathReady = 0;
    _intimidationReady = 0;
    _callOfTheWildReady = 0;
    _silencingShotReady = 0;
    _mastersCallReady = 0;

    // Initialize combat metrics
    _totalPetDamage = 0;
    _totalHunterDamage = 0;
    _petDpsRatio = 0.6f; // Beast Mastery pets do ~60% of total DPS
    _steadyShotCount = 0;
    _arcaneShotCount = 0;
    _killShotCount = 0;

    // Initialize pet management
    _petInBurstMode = false;
    _emergencyModeActive = false;
    _petReviveAttempts = 0;
    _multiTargetCount = 0;
    _multiTargets.clear();

    // Set initial optimal aspect
    _currentAspect = ASPECT_OF_THE_HAWK;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Initialization complete for bot {}", bot->GetName());
}

void BeastMasterySpecialization::UpdateRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return;

    if (!target->IsHostileTo(bot))
        return;

    uint32 now = getMSTime();

    // Throttle rotation updates for performance
    if (now - _lastRangeCheck < ROTATION_UPDATE_INTERVAL)
        return;
    _lastRangeCheck = now;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: UpdateRotation for bot {} targeting {}",
                 bot->GetName(), target->GetName());

    // Update all management systems
    UpdatePetManagement();
    UpdateRangeManagement();
    UpdateAspectManagement();
    UpdateTrapManagement();
    UpdateTracking();
    UpdateAdvancedPetManagement();
    UpdateCombatPhase();
    UpdateBurstPhase();

    // Handle multi-target situations
    HandleMultipleTargets();

    // Emergency handling takes priority
    if (_emergencyModeActive || bot->GetHealthPct() < 30.0f)
    {
        if (ExecuteEmergencyRotation(target))
            return;
    }

    // Execute rotation based on current phase
    switch (_bmRotationPhase)
    {
        case BMRotationPhase::OPENING:
            if (ExecuteOpeningRotation(target))
                return;
            break;

        case BMRotationPhase::BURST_WITH_PET:
            if (ExecuteBurstRotation(target))
                return;
            break;

        case BMRotationPhase::STEADY_DPS:
            if (ExecuteSteadyDpsRotation(target))
                return;
            break;

        case BMRotationPhase::PET_FOCUSED:
            if (ExecutePetFocusedRotation(target))
                return;
            break;

        case BMRotationPhase::RANGED_SUPPORT:
            if (ExecuteRangedSupportRotation(target))
                return;
            break;

        case BMRotationPhase::EMERGENCY:
            if (ExecuteEmergencyRotation(target))
                return;
            break;

        default:
            if (ExecuteSteadyDpsRotation(target))
                return;
            break;
    }

    // Handle dead zone situations
    if (IsInDeadZone(target))
    {
        HandleDeadZone(target);
        return;
    }

    // Fallback to basic auto-shot
    if (IsInRangedRange(target) && HasAmmo() && IsRangedWeaponEquipped())
    {
        if (now - _lastAutoShot > GetRangedAttackSpeed() * 1000)
        {
            bot->AttackStart(target);
            _lastAutoShot = now;
        }
    }

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: No rotation action taken for bot {}", bot->GetName());
}

void BeastMasterySpecialization::UpdateBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: UpdateBuffs for bot {}", bot->GetName());

    // Maintain optimal aspect
    if (!HasCorrectAspect())
    {
        SwitchToOptimalAspect();
        return;
    }

    // Maintain Trueshot Aura if available
    if (bot->HasSpell(TRUESHOT_AURA) && !bot->HasAura(TRUESHOT_AURA))
    {
        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Trueshot Aura for bot {}", bot->GetName());
        bot->CastSpell(bot, TRUESHOT_AURA, false);
        return;
    }

    // Maintain Hunter's Mark on target if in combat
    if (bot->IsInCombat())
    {
        ::Unit* target = bot->GetTarget();
        if (target && !target->HasAura(HUNTERS_MARK) && bot->HasSpell(HUNTERS_MARK))
        {
            TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Hunter's Mark for bot {}", bot->GetName());
            bot->CastSpell(target, HUNTERS_MARK, false);
            return;
        }
    }

    // Group buffs
    if (Group* group = bot->GetGroup())
    {
        // Apply group-wide buffs when appropriate
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            if (Player* member = ref->GetSource())
            {
                if (member->IsInWorld() && bot->IsWithinDistInMap(member, 40.0f))
                {
                    // Apply Trueshot Aura to group if not present
                    if (!member->HasAura(TRUESHOT_AURA) && bot->HasSpell(TRUESHOT_AURA))
                    {
                        bot->CastSpell(bot, TRUESHOT_AURA, false);
                        break;
                    }
                }
            }
        }
    }

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: UpdateBuffs complete for bot {}", bot->GetName());
}

void BeastMasterySpecialization::UpdateCooldowns(uint32 diff)
{
    // Update base cooldowns
    for (auto& cooldown : _cooldowns)
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;

    // Update Beast Mastery specific cooldowns
    if (_bestialWrathReady > diff)
        _bestialWrathReady -= diff;
    else
        _bestialWrathReady = 0;

    if (_intimidationReady > diff)
        _intimidationReady -= diff;
    else
        _intimidationReady = 0;

    if (_callOfTheWildReady > diff)
        _callOfTheWildReady -= diff;
    else
        _callOfTheWildReady = 0;

    if (_silencingShotReady > diff)
        _silencingShotReady -= diff;
    else
        _silencingShotReady = 0;

    if (_mastersCallReady > diff)
        _mastersCallReady -= diff;
    else
        _mastersCallReady = 0;

    // Update timing variables
    if (_lastPetFeed > diff)
        _lastPetFeed -= diff;
    else
        _lastPetFeed = 0;

    if (_lastPetHappinessCheck > diff)
        _lastPetHappinessCheck -= diff;
    else
        _lastPetHappinessCheck = 0;

    if (_lastPetCommand > diff)
        _lastPetCommand -= diff;
    else
        _lastPetCommand = 0;

    if (_lastBurstCheck > diff)
        _lastBurstCheck -= diff;
    else
        _lastBurstCheck = 0;

    if (_petPositionUpdateTime > diff)
        _petPositionUpdateTime -= diff;
    else
        _petPositionUpdateTime = 0;
}

bool BeastMasterySpecialization::CanUseAbility(uint32 spellId)
{
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    switch (spellId)
    {
        case BESTIAL_WRATH:
            return _bestialWrathReady == 0 && HasActivePet();
        case INTIMIDATION:
            return _intimidationReady == 0 && HasActivePet();
        case CALL_OF_THE_WILD:
            return _callOfTheWildReady == 0;
        case SILENCING_SHOT:
            return _silencingShotReady == 0;
        case MASTER_S_CALL:
            return _mastersCallReady == 0 && HasActivePet();
        default:
            return HasEnoughResource(spellId);
    }
}

void BeastMasterySpecialization::OnCombatStart(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: OnCombatStart for bot {} targeting {}",
                 bot->GetName(), target ? target->GetName() : "unknown");

    // Reset rotation phase to opening
    _bmRotationPhase = BMRotationPhase::OPENING;

    // Ensure we have optimal aspect
    SwitchToOptimalAspect();

    // Summon pet if not active
    if (!HasActivePet())
        SummonPet();

    // Set pet to aggressive mode
    _currentPetStrategy = PetStrategy::AGGRESSIVE;

    // Apply Hunter's Mark
    if (target && bot->HasSpell(HUNTERS_MARK))
    {
        bot->CastSpell(target, HUNTERS_MARK, false);
    }

    // Reset combat metrics
    _totalPetDamage = 0;
    _totalHunterDamage = 0;
    _steadyShotCount = 0;
    _arcaneShotCount = 0;
    _killShotCount = 0;
    _emergencyModeActive = false;
    _petInBurstMode = false;
    _petReviveAttempts = 0;

    // Clear multi-target tracking
    _multiTargets.clear();
    _multiTargetCount = 0;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Combat initialization complete for bot {}", bot->GetName());
}

void BeastMasterySpecialization::OnCombatEnd()
{
    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: OnCombatEnd for bot {}", GetBot()->GetName());

    // Reset rotation phase
    _bmRotationPhase = BMRotationPhase::OPENING;

    // Switch to appropriate out-of-combat aspect
    _currentAspect = ASPECT_OF_THE_HAWK;
    if (GetBot()->HasSpell(ASPECT_OF_THE_HAWK))
        GetBot()->CastSpell(GetBot(), ASPECT_OF_THE_HAWK, false);

    // Set pet to defensive mode
    _currentPetStrategy = PetStrategy::DEFENSIVE;
    CommandPetFollow();

    // Clear temporary states
    _cooldowns.clear();
    _multiTargets.clear();
    _multiTargetCount = 0;
    _emergencyModeActive = false;
    _petInBurstMode = false;

    // Reset timing
    _lastAutoShot = 0;
    _petPositionUpdateTime = 0;

    // Heal pet if needed
    MendPetIfNeeded();
    FeedPetIfNeeded();

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Combat cleanup complete for bot {}", GetBot()->GetName());
}

bool BeastMasterySpecialization::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    uint32 manaCost = 0;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (spellInfo)
        manaCost = spellInfo->ManaCost;

    // Special cases for specific spells
    switch (spellId)
    {
        case STEADY_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 110);
        case ARCANE_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 230);
        case MULTI_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 350);
        case AIMED_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 410);
        case KILL_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 150);
        case CONCUSSIVE_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 75);
        case BESTIAL_WRATH:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 100) && HasActivePet();
        case INTIMIDATION:
            return HasActivePet();
        case CALL_OF_THE_WILD:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 200);
        case SILENCING_SHOT:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 150);
        case CALL_PET:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 500);
        case MEND_PET:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 150);
        case REVIVE_PET:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 800);
        case HUNTERS_MARK:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 40);
        case SERPENT_STING:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 115);
        case VIPER_STING:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 85);
        case DISENGAGE:
            return bot->GetPower(POWER_MANA) >= (manaCost > 0 ? manaCost : 60);
        case FEIGN_DEATH:
            return true; // No mana cost
        default:
            return bot->GetPower(POWER_MANA) >= manaCost;
    }
}

void BeastMasterySpecialization::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 manaCost = 0;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (spellInfo)
        manaCost = spellInfo->ManaCost;

    // Consume mana
    if (manaCost > 0)
    {
        uint32 currentMana = bot->GetPower(POWER_MANA);
        if (currentMana >= manaCost)
        {
            bot->ModifyPower(POWER_MANA, -int32(manaCost));
            _manaConsumed += manaCost;
        }
    }

    // Set spell-specific cooldowns and track usage
    switch (spellId)
    {
        case STEADY_SHOT:
            _steadyShotCount++;
            break;
        case ARCANE_SHOT:
            _arcaneShotCount++;
            break;
        case KILL_SHOT:
            _killShotCount++;
            break;
        case BESTIAL_WRATH:
            _bestialWrathReady = 120000; // 2 minutes
            _lastBestialWrath = getMSTime();
            _petInBurstMode = true;
            break;
        case INTIMIDATION:
            _intimidationReady = 60000; // 1 minute
            _lastIntimidation = getMSTime();
            break;
        case CALL_OF_THE_WILD:
            _callOfTheWildReady = 300000; // 5 minutes
            _lastCallOfTheWild = getMSTime();
            break;
        case SILENCING_SHOT:
            _silencingShotReady = 20000; // 20 seconds
            _lastSilencingShot = getMSTime();
            break;
        case MASTER_S_CALL:
            _mastersCallReady = 60000; // 1 minute
            _lastMastersCall = getMSTime();
            break;
        default:
            break;
    }

    // Set base cooldown
    UpdateCooldown(spellId, GetSpellCooldown(spellId));
}

Position BeastMasterySpecialization::GetOptimalPosition(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Beast Mastery hunters prefer maximum range for safety while pet tanks
    float distance = OPTIMAL_RANGE;
    float angle = target->GetAngle(bot);

    // Adjust positioning based on pet location
    if (HasActivePet())
    {
        Pet* pet = bot->GetPet();
        if (pet)
        {
            float petDistance = pet->GetDistance(target);
            if (petDistance <= MELEE_RANGE)
            {
                // Pet is in melee, position at max range opposite side
                angle = pet->GetAngle(target) + M_PI; // Opposite side from pet
                distance = OPTIMAL_RANGE;
            }
            else
            {
                // Pet is not engaged, standard positioning
                angle += M_PI / 4; // 45 degrees offset
            }
        }
    }

    // Avoid being in melee range
    if (distance < DEAD_ZONE_MAX)
        distance = OPTIMAL_RANGE;

    Position optimalPos(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Optimal position calculated for bot {} at distance {:.2f}, angle {:.2f}",
                 bot->GetName(), distance, angle);

    return optimalPos;
}

float BeastMasterySpecialization::GetOptimalRange(::Unit* target)
{
    // Beast Mastery hunters prefer maximum range for safety
    return OPTIMAL_RANGE;
}

// ===== ROTATION IMPLEMENTATIONS =====

bool BeastMasterySpecialization::ExecuteOpeningRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: ExecuteOpeningRotation for bot {}", bot->GetName());

    // Ensure pet is active first
    if (!HasActivePet())
    {
        SummonPet();
        return true;
    }

    // Apply Hunter's Mark if not present
    if (!target->HasAura(HUNTERS_MARK) && HasEnoughResource(HUNTERS_MARK))
    {
        bot->CastSpell(target, HUNTERS_MARK, false);
        ConsumeResource(HUNTERS_MARK);
        return true;
    }

    // Command pet to attack
    CommandPetAttack(target);

    // Apply Serpent Sting for DoT
    if (!target->HasAura(SERPENT_STING) && HasEnoughResource(SERPENT_STING))
    {
        bot->CastSpell(target, SERPENT_STING, false);
        ConsumeResource(SERPENT_STING);
        return true;
    }

    // Transition to steady DPS phase
    _bmRotationPhase = BMRotationPhase::STEADY_DPS;
    return ExecuteSteadyDpsRotation(target);
}

bool BeastMasterySpecialization::ExecuteBurstRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: ExecuteBurstRotation for bot {}", bot->GetName());

    // Use Bestial Wrath if available and not already active
    if (ShouldUseBestialWrath())
    {
        CastBestialWrath();
        return true;
    }

    // Use Call of the Wild for group buff
    if (ShouldUseCallOfTheWild())
    {
        CastCallOfTheWild();
        return true;
    }

    // Use Intimidation for extra damage/stun
    if (ShouldUseIntimidation(target))
    {
        CastIntimidation(target);
        return true;
    }

    // Spam high-damage shots during burst
    if (ShouldUseKillShot(target))
    {
        CastKillShot(target);
        return true;
    }

    if (ShouldUseArcaneShot(target))
    {
        CastArcaneShot(target);
        return true;
    }

    if (ShouldUseMultiShot(target) && _multiTargetCount > 1)
    {
        CastMultiShot(target);
        return true;
    }

    if (ShouldUseSteadyShot(target))
    {
        CastSteadyShot(target);
        return true;
    }

    // If no burst abilities available, return to steady DPS
    _bmRotationPhase = BMRotationPhase::STEADY_DPS;
    _petInBurstMode = false;
    return ExecuteSteadyDpsRotation(target);
}

bool BeastMasterySpecialization::ExecuteSteadyDpsRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: ExecuteSteadyDpsRotation for bot {}", bot->GetName());

    // Maintain Serpent Sting DoT
    if (!target->HasAura(SERPENT_STING) && HasEnoughResource(SERPENT_STING))
    {
        bot->CastSpell(target, SERPENT_STING, false);
        ConsumeResource(SERPENT_STING);
        return true;
    }

    // Use Kill Shot in execute range
    if (ShouldUseKillShot(target))
    {
        CastKillShot(target);
        return true;
    }

    // Use Multi-Shot for multiple targets
    if (ShouldUseMultiShot(target) && _multiTargetCount > 2)
    {
        CastMultiShot(target);
        return true;
    }

    // Steady Shot for consistent DPS and mana efficiency
    if (ShouldUseSteadyShot(target))
    {
        CastSteadyShot(target);
        return true;
    }

    // Arcane Shot for instant damage
    if (ShouldUseArcaneShot(target))
    {
        CastArcaneShot(target);
        return true;
    }

    // Auto-shot fallback
    if (IsInRangedRange(target))
    {
        uint32 now = getMSTime();
        if (now - _lastAutoShot > GetRangedAttackSpeed() * 1000)
        {
            bot->AttackStart(target);
            _lastAutoShot = now;
            return true;
        }
    }

    return false;
}

bool BeastMasterySpecialization::ExecutePetFocusedRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: ExecutePetFocusedRotation for bot {}", bot->GetName());

    // Ensure pet is attacking
    CommandPetAttack(target);

    // Use pet-focused abilities
    if (ShouldUseBestialWrath())
    {
        CastBestialWrath();
        return true;
    }

    // Support pet with ranged attacks
    if (ShouldUseSteadyShot(target))
    {
        CastSteadyShot(target);
        return true;
    }

    // Maintain Hunter's Mark
    if (!target->HasAura(HUNTERS_MARK) && HasEnoughResource(HUNTERS_MARK))
    {
        bot->CastSpell(target, HUNTERS_MARK, false);
        ConsumeResource(HUNTERS_MARK);
        return true;
    }

    // Minimal mana abilities to conserve for pet healing
    if (bot->GetPowerPct(POWER_MANA) > 60.0f)
    {
        if (ShouldUseArcaneShot(target))
        {
            CastArcaneShot(target);
            return true;
        }
    }

    return false;
}

bool BeastMasterySpecialization::ExecuteRangedSupportRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: ExecuteRangedSupportRotation for bot {}", bot->GetName());

    // Focus on supporting the group/pet from maximum range
    if (!IsInRangedRange(target))
        return false;

    // Interrupt spellcasting
    if (target->HasUnitState(UNIT_STATE_CASTING) && bot->HasSpell(SILENCING_SHOT))
    {
        if (CanUseAbility(SILENCING_SHOT))
        {
            CastSilencingShot(target);
            return true;
        }
    }

    // Maintain DoTs
    if (!target->HasAura(SERPENT_STING) && HasEnoughResource(SERPENT_STING))
    {
        bot->CastSpell(target, SERPENT_STING, false);
        ConsumeResource(SERPENT_STING);
        return true;
    }

    // Conservative shot rotation
    if (ShouldUseSteadyShot(target))
    {
        CastSteadyShot(target);
        return true;
    }

    return false;
}

bool BeastMasterySpecialization::ExecuteEmergencyRotation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: ExecuteEmergencyRotation for bot {}", bot->GetName());

    float healthPct = bot->GetHealthPct();

    // Emergency escape abilities
    if (healthPct < 20.0f)
    {
        // Feign Death to drop aggro
        if (bot->HasSpell(FEIGN_DEATH) && IsCooldownReady(FEIGN_DEATH))
        {
            bot->CastSpell(bot, FEIGN_DEATH, false);
            UpdateCooldown(FEIGN_DEATH, GetSpellCooldown(FEIGN_DEATH));
            return true;
        }

        // Disengage to get away
        if (IsInMeleeRange(target) && bot->HasSpell(DISENGAGE) && HasEnoughResource(DISENGAGE))
        {
            bot->CastSpell(bot, DISENGAGE, false);
            ConsumeResource(DISENGAGE);
            return true;
        }
    }

    // Master's Call to break CC
    if (bot->HasAura(5782) || bot->HasAura(339)) // Fear or Entangling Roots
    {
        if (CanUseAbility(MASTER_S_CALL))
        {
            CastMastersCall();
            return true;
        }
    }

    // Concussive Shot to slow pursuers
    if (IsInDeadZone(target) && ShouldUseConcussiveShot(target))
    {
        CastConcussiveShot(target);
        return true;
    }

    // Try to create distance
    if (IsInMeleeRange(target))
    {
        // Move away while shooting
        if (ShouldUseArcaneShot(target))
        {
            CastArcaneShot(target);
            return true;
        }
    }

    // Default back to steady DPS if emergency is over
    if (healthPct > 50.0f)
    {
        _emergencyModeActive = false;
        _bmRotationPhase = BMRotationPhase::STEADY_DPS;
        return ExecuteSteadyDpsRotation(target);
    }

    return false;
}

// ===== PET MANAGEMENT IMPLEMENTATIONS =====

void BeastMasterySpecialization::UpdatePetManagement()
{
    uint32 now = getMSTime();
    if (now - _lastPetCheck < PET_CHECK_INTERVAL)
        return;

    _lastPetCheck = now;
    UpdatePetInfo();

    // Handle pet revival if needed
    if (!HasActivePet() && _petReviveAttempts < 3)
    {
        HandlePetRevive();
        return;
    }

    // Manage pet health and happiness
    ManagePetHealth();
    ManagePetHappiness();

    // Update pet behavior based on combat situation
    if (GetBot()->IsInCombat())
    {
        ::Unit* target = GetBot()->GetTarget();
        if (target)
            OptimizePetBehavior(target);
    }
}

void BeastMasterySpecialization::SummonPet()
{
    Player* bot = GetBot();
    if (!bot || HasActivePet())
        return;

    if (bot->HasSpell(CALL_PET) && HasEnoughResource(CALL_PET))
    {
        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Summoning pet for bot {}", bot->GetName());
        bot->CastSpell(bot, CALL_PET, false);
        ConsumeResource(CALL_PET);
        _petReviveAttempts = 0;
    }
}

void BeastMasterySpecialization::CommandPetAttack(::Unit* target)
{
    if (!target || !HasActivePet())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && pet->IsAlive())
    {
        uint32 now = getMSTime();
        if (now - _lastPetCommand > 1000) // Don't spam commands
        {
            pet->GetCharmInfo()->SetCommandState(COMMAND_ATTACK);
            pet->Attack(target, true);
            _lastPetCommand = now;

            // Use pet special abilities
            CommandPetSpecialAbilities(target);

            TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Commanded pet to attack {} for bot {}",
                         target->GetName(), bot->GetName());
        }
    }
}

void BeastMasterySpecialization::CommandPetFollow()
{
    if (!HasActivePet())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && pet->IsAlive())
    {
        pet->GetCharmInfo()->SetCommandState(COMMAND_FOLLOW);
        _currentPetStrategy = PetStrategy::DEFENSIVE;

        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Commanded pet to follow for bot {}", bot->GetName());
    }
}

void BeastMasterySpecialization::CommandPetStay()
{
    if (!HasActivePet())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && pet->IsAlive())
    {
        pet->GetCharmInfo()->SetCommandState(COMMAND_STAY);
        _currentPetStrategy = PetStrategy::PASSIVE;

        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Commanded pet to stay for bot {}", bot->GetName());
    }
}

void BeastMasterySpecialization::MendPetIfNeeded()
{
    if (!HasActivePet())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && pet->GetHealthPct() < 70.0f && HasEnoughResource(MEND_PET))
    {
        if (IsCooldownReady(MEND_PET))
        {
            TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Mending pet for bot {}", bot->GetName());
            bot->CastSpell(pet, MEND_PET, false);
            ConsumeResource(MEND_PET);
        }
    }
}

void BeastMasterySpecialization::FeedPetIfNeeded()
{
    if (!HasActivePet())
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && !IsPetHappy())
    {
        uint32 now = getMSTime();
        if (now - _lastPetFeed > 30000) // Feed every 30 seconds if unhappy
        {
            // TODO: Implement pet feeding with appropriate food items
            // This would involve finding food items in inventory and using them
            _lastPetFeed = now;

            TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Feeding pet for bot {}", bot->GetName());
        }
    }
}

bool BeastMasterySpecialization::HasActivePet() const
{
    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;
    return pet && pet->IsAlive();
}

PetInfo BeastMasterySpecialization::GetPetInfo() const
{
    return _petInfo;
}

void BeastMasterySpecialization::UpdateAdvancedPetManagement()
{
    if (!HasActivePet())
        return;

    // Handle burst phase for pet
    if (_petInBurstMode)
        HandlePetBurstPhase();

    // Handle pet emergency situations
    if (_petInfo.GetHealthPct() < 20.0f)
        HandlePetEmergency();

    // Update pet positioning
    uint32 now = getMSTime();
    if (now - _petPositionUpdateTime > 2000)
    {
        _petPositionUpdateTime = now;
        Pet* pet = GetBot()->GetPet();
        if (pet)
            _lastKnownPetPosition = pet->GetPosition();
    }
}

void BeastMasterySpecialization::OptimizePetBehavior(::Unit* target)
{
    if (!target || !HasActivePet())
        return;

    float targetHealthPct = target->GetHealthPct();
    float botHealthPct = GetBot()->GetHealthPct();

    // Adjust pet strategy based on situation
    if (botHealthPct < 30.0f)
    {
        // Pet should protect hunter
        _currentPetStrategy = PetStrategy::DEFENSIVE;
    }
    else if (targetHealthPct < 35.0f)
    {
        // Execute phase - full aggression
        _currentPetStrategy = PetStrategy::AGGRESSIVE;
    }
    else
    {
        // Standard combat
        _currentPetStrategy = PetStrategy::ASSIST;
    }

    // Command pet accordingly
    CommandPetAttack(target);
}

void BeastMasterySpecialization::HandlePetBurstPhase()
{
    // Pet is in Bestial Wrath mode - maximize damage
    ::Unit* target = GetBot()->GetTarget();
    if (target)
    {
        CommandPetSpecialAbilities(target);
    }

    // Check if burst phase should end
    uint32 now = getMSTime();
    if (now - _lastBestialWrath > 18000) // Bestial Wrath lasts 18 seconds
    {
        _petInBurstMode = false;
    }
}

void BeastMasterySpecialization::HandlePetEmergency()
{
    // Pet is in danger - prioritize its survival
    if (_petInfo.GetHealthPct() < 10.0f)
    {
        // Try to get pet out of danger
        CommandPetFollow();
        MendPetIfNeeded();
    }
    else if (_petInfo.GetHealthPct() < 30.0f)
    {
        // Heal pet if possible
        MendPetIfNeeded();
    }
}

void BeastMasterySpecialization::CommandPetSpecialAbilities(::Unit* target)
{
    if (!target || !HasActivePet())
        return;

    Pet* pet = GetBot()->GetPet();
    if (!pet || !pet->IsAlive())
        return;

    // Use pet abilities based on pet type and situation
    // This is a simplified implementation - real implementation would depend on specific pet abilities

    float distance = pet->GetDistance(target);
    if (distance <= MELEE_RANGE)
    {
        // Melee range abilities
        if (pet->HasSpell(CLAW) && IsCooldownReady(CLAW))
        {
            pet->CastSpell(target, CLAW, false);
            UpdateCooldown(CLAW, 6000);
        }
        else if (pet->HasSpell(BITE) && IsCooldownReady(BITE))
        {
            pet->CastSpell(target, BITE, false);
            UpdateCooldown(BITE, 10000);
        }
    }

    // Special abilities
    if (pet->HasSpell(GROWL) && target->GetThreatManager().GetMostHated() != pet)
    {
        if (IsCooldownReady(GROWL))
        {
            pet->CastSpell(target, GROWL, false);
            UpdateCooldown(GROWL, 5000);
        }
    }
}

void BeastMasterySpecialization::ManagePetHappiness()
{
    uint32 now = getMSTime();
    if (now - _lastPetHappinessCheck < 10000) // Check every 10 seconds
        return;

    _lastPetHappinessCheck = now;

    if (!IsPetHappy())
    {
        FeedPetIfNeeded();
    }
}

void BeastMasterySpecialization::ManagePetHealth()
{
    if (!HasActivePet())
        return;

    float petHealthPct = _petInfo.GetHealthPct();

    if (petHealthPct < 50.0f && !GetBot()->IsInCombat())
    {
        MendPetIfNeeded();
    }
    else if (petHealthPct < 30.0f && GetBot()->IsInCombat())
    {
        // Emergency pet healing during combat
        MendPetIfNeeded();
    }
}

void BeastMasterySpecialization::HandlePetRevive()
{
    Player* bot = GetBot();
    if (!bot || HasActivePet())
        return;

    if (bot->HasSpell(REVIVE_PET) && HasEnoughResource(REVIVE_PET))
    {
        if (IsCooldownReady(REVIVE_PET))
        {
            TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Reviving pet for bot {}", bot->GetName());
            bot->CastSpell(bot, REVIVE_PET, false);
            ConsumeResource(REVIVE_PET);
            _petReviveAttempts++;
        }
    }
    else if (_petReviveAttempts == 0)
    {
        // Try to summon a new pet if revive is not available
        SummonPet();
    }
}

// ===== BEAST MASTERY SPECIFIC ABILITIES =====

bool BeastMasterySpecialization::ShouldUseBestialWrath() const
{
    return CanUseAbility(BESTIAL_WRATH) && HasActivePet() && GetBot()->IsInCombat() &&
           !_petInBurstMode && GetBot()->GetTarget() && GetBot()->GetTarget()->GetHealthPct() > 30.0f;
}

bool BeastMasterySpecialization::ShouldUseIntimidation(::Unit* target) const
{
    return target && CanUseAbility(INTIMIDATION) && HasActivePet() &&
           GetBot()->GetDistance(target) <= 35.0f && target->GetHealthPct() > 50.0f;
}

bool BeastMasterySpecialization::ShouldUseCallOfTheWild() const
{
    return CanUseAbility(CALL_OF_THE_WILD) && GetBot()->IsInCombat() &&
           (GetBot()->GetGroup() != nullptr || _multiTargetCount > 1);
}

void BeastMasterySpecialization::CastBestialWrath()
{
    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;

    if (pet && CanUseAbility(BESTIAL_WRATH))
    {
        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Bestial Wrath for bot {}", bot->GetName());
        bot->CastSpell(pet, BESTIAL_WRATH, false);
        ConsumeResource(BESTIAL_WRATH);
        _petInBurstMode = true;
    }
}

void BeastMasterySpecialization::CastIntimidation(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !CanUseAbility(INTIMIDATION))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Intimidation on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, INTIMIDATION, false);
    ConsumeResource(INTIMIDATION);
}

void BeastMasterySpecialization::CastCallOfTheWild()
{
    Player* bot = GetBot();
    if (!bot || !CanUseAbility(CALL_OF_THE_WILD))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Call of the Wild for bot {}", bot->GetName());

    bot->CastSpell(bot, CALL_OF_THE_WILD, false);
    ConsumeResource(CALL_OF_THE_WILD);
}

void BeastMasterySpecialization::CastSilencingShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !CanUseAbility(SILENCING_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Silencing Shot on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, SILENCING_SHOT, false);
    ConsumeResource(SILENCING_SHOT);
}

void BeastMasterySpecialization::CastMastersCall()
{
    Player* bot = GetBot();
    if (!bot || !CanUseAbility(MASTER_S_CALL))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Master's Call for bot {}", bot->GetName());

    bot->CastSpell(bot, MASTER_S_CALL, false);
    ConsumeResource(MASTER_S_CALL);
}

// ===== SHOT IMPLEMENTATIONS =====

bool BeastMasterySpecialization::ShouldUseSteadyShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(STEADY_SHOT) &&
           IsCooldownReady(STEADY_SHOT) && GetBot()->GetPowerPct(POWER_MANA) > 20.0f;
}

bool BeastMasterySpecialization::ShouldUseArcaneShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(ARCANE_SHOT) &&
           IsCooldownReady(ARCANE_SHOT) && GetBot()->GetPowerPct(POWER_MANA) > 30.0f;
}

bool BeastMasterySpecialization::ShouldUseMultiShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(MULTI_SHOT) &&
           IsCooldownReady(MULTI_SHOT) && _multiTargetCount > 2 && GetBot()->GetPowerPct(POWER_MANA) > 40.0f;
}

bool BeastMasterySpecialization::ShouldUseKillShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(KILL_SHOT) &&
           IsCooldownReady(KILL_SHOT) && target->GetHealthPct() < 20.0f;
}

bool BeastMasterySpecialization::ShouldUseConcussiveShot(::Unit* target) const
{
    return target && IsInRangedRange(target) && HasEnoughResource(CONCUSSIVE_SHOT) &&
           IsCooldownReady(CONCUSSIVE_SHOT) && !target->HasAura(CONCUSSIVE_SHOT) &&
           (IsInDeadZone(target) || GetBot()->GetHealthPct() < 50.0f);
}

void BeastMasterySpecialization::CastSteadyShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(STEADY_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Steady Shot on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, STEADY_SHOT, false);
    ConsumeResource(STEADY_SHOT);
    _totalHunterDamage += 800; // Estimated damage
}

void BeastMasterySpecialization::CastArcaneShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(ARCANE_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Arcane Shot on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, ARCANE_SHOT, false);
    ConsumeResource(ARCANE_SHOT);
    _totalHunterDamage += 1200; // Estimated damage
}

void BeastMasterySpecialization::CastMultiShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(MULTI_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Multi-Shot on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, MULTI_SHOT, false);
    ConsumeResource(MULTI_SHOT);
    _totalHunterDamage += 1000 * _multiTargetCount; // AoE damage
}

void BeastMasterySpecialization::CastKillShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(KILL_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Kill Shot on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, KILL_SHOT, false);
    ConsumeResource(KILL_SHOT);
    _totalHunterDamage += 2500; // High execute damage
}

void BeastMasterySpecialization::CastConcussiveShot(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target || !HasEnoughResource(CONCUSSIVE_SHOT))
        return;

    TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Casting Concussive Shot on {} for bot {}",
                 target->GetName(), bot->GetName());

    bot->CastSpell(target, CONCUSSIVE_SHOT, false);
    ConsumeResource(CONCUSSIVE_SHOT);
    _totalHunterDamage += 600; // Utility shot with slow
}

// ===== UTILITY AND MANAGEMENT METHODS =====

void BeastMasterySpecialization::HandleTargetSwitching()
{
    // Beast Mastery hunters should focus on single targets unless AoE is beneficial
    Player* bot = GetBot();
    if (!bot)
        return;

    ::Unit* currentTarget = bot->GetTarget();
    if (!currentTarget || currentTarget->isDead())
    {
        // Find new target
        for (auto& attacker : bot->getAttackers())
        {
            if (attacker->IsAlive() && attacker->IsHostileTo(bot))
            {
                bot->SetTarget(attacker->GetGUID());
                CommandPetAttack(attacker);
                break;
            }
        }
    }
}

void BeastMasterySpecialization::UpdateBurstPhase()
{
    uint32 now = getMSTime();
    if (now - _lastBurstCheck < 5000) // Check every 5 seconds
        return;

    _lastBurstCheck = now;

    Player* bot = GetBot();
    ::Unit* target = bot ? bot->GetTarget() : nullptr;

    if (!target || !bot->IsInCombat())
        return;

    // Enter burst phase if conditions are met
    bool shouldBurst = false;

    // Elite or boss targets
    if (target->IsElite() || target->GetMaxHealth() > bot->GetMaxHealth() * 3)
        shouldBurst = true;

    // Multiple targets
    if (_multiTargetCount > 2)
        shouldBurst = true;

    // Target has high health and we have cooldowns available
    if (target->GetHealthPct() > 70.0f && ShouldUseBestialWrath())
        shouldBurst = true;

    if (shouldBurst && _bmRotationPhase != BMRotationPhase::BURST_WITH_PET)
    {
        _bmRotationPhase = BMRotationPhase::BURST_WITH_PET;
        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Entering burst phase for bot {}", bot->GetName());
    }
}

void BeastMasterySpecialization::UpdateCombatPhase()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->IsInCombat())
    {
        _bmRotationPhase = BMRotationPhase::OPENING;
        return;
    }

    ::Unit* target = bot->GetTarget();
    if (!target)
        return;

    float targetHealthPct = target->GetHealthPct();
    float botHealthPct = bot->GetHealthPct();

    // Emergency phase
    if (botHealthPct < 30.0f || !HasActivePet())
    {
        _bmRotationPhase = BMRotationPhase::EMERGENCY;
        _emergencyModeActive = true;
        return;
    }

    // Pet-focused phase when pet is doing most damage
    if (HasActivePet() && _petDpsRatio > 0.7f)
    {
        _bmRotationPhase = BMRotationPhase::PET_FOCUSED;
        return;
    }

    // Standard phase determination
    if (_petInBurstMode || ShouldUseCooldowns())
    {
        _bmRotationPhase = BMRotationPhase::BURST_WITH_PET;
    }
    else if (ShouldFocusOnPetDPS())
    {
        _bmRotationPhase = BMRotationPhase::PET_FOCUSED;
    }
    else
    {
        _bmRotationPhase = BMRotationPhase::STEADY_DPS;
    }
}

bool BeastMasterySpecialization::ShouldFocusOnPetDPS() const
{
    // Focus on pet when it's doing most of the damage
    return HasActivePet() && _petDpsRatio > 0.6f && GetBot()->GetPowerPct(POWER_MANA) < 40.0f;
}

bool BeastMasterySpecialization::ShouldUseCooldowns() const
{
    Player* bot = GetBot();
    ::Unit* target = bot ? bot->GetTarget() : nullptr;

    if (!target || !bot->IsInCombat())
        return false;

    // Use cooldowns on elite/boss targets
    if (target->IsElite())
        return true;

    // Use cooldowns when target has high health
    if (target->GetHealthPct() > 70.0f && target->GetMaxHealth() > bot->GetMaxHealth() * 2)
        return true;

    // Use cooldowns in group situations
    if (bot->GetGroup() != nullptr)
        return true;

    return false;
}

void BeastMasterySpecialization::HandleMultipleTargets()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Count nearby hostile targets
    _multiTargets.clear();
    _multiTargetCount = 0;

    for (auto& attacker : bot->getAttackers())
    {
        if (attacker->IsAlive() && attacker->IsWithinDistInMap(bot, 40.0f))
        {
            _multiTargets.insert(attacker->GetGUID());
            _multiTargetCount++;
        }
    }

    // Adjust strategy for multiple targets
    if (_multiTargetCount > 2 && _bmRotationPhase != BMRotationPhase::EMERGENCY)
    {
        // Consider AoE abilities more frequently
        _bmRotationPhase = BMRotationPhase::STEADY_DPS; // Multi-shot will be prioritized
    }
}

float BeastMasterySpecialization::CalculateOptimalPetPosition(::Unit* target)
{
    if (!target || !HasActivePet())
        return 0.0f;

    Pet* pet = GetBot()->GetPet();
    if (!pet)
        return 0.0f;

    float targetDistance = pet->GetDistance(target);

    // Optimal pet position is in melee range but positioned to avoid cleaves
    if (targetDistance <= MELEE_RANGE)
        return targetDistance;

    return MELEE_RANGE;
}

// ===== INTERFACE IMPLEMENTATIONS =====

void BeastMasterySpecialization::UpdateTrapManagement()
{
    // Beast Mastery rarely uses traps - focus on pet and ranged combat
    // Implement basic trap logic for emergency situations only
}

void BeastMasterySpecialization::PlaceTrap(uint32 trapSpell, Position position)
{
    // Minimal trap implementation for Beast Mastery
    Player* bot = GetBot();
    if (!bot || !HasEnoughResource(trapSpell))
        return;

    if (IsCooldownReady(trapSpell))
    {
        bot->CastSpell(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(),
                       trapSpell, false);
        ConsumeResource(trapSpell);

        TrapInfo trap(trapSpell, getMSTime(), position);
        _activeTraps.push_back(trap);
    }
}

bool BeastMasterySpecialization::ShouldPlaceTrap() const
{
    // Only place traps in emergency situations
    return _emergencyModeActive && GetBot()->GetHealthPct() < 30.0f;
}

uint32 BeastMasterySpecialization::GetOptimalTrapSpell() const
{
    // Freezing trap for emergency CC
    return FREEZING_TRAP;
}

std::vector<TrapInfo> BeastMasterySpecialization::GetActiveTraps() const
{
    return _activeTraps;
}

void BeastMasterySpecialization::UpdateAspectManagement()
{
    uint32 now = getMSTime();
    if (now - _lastAspectCheck < ASPECT_CHECK_INTERVAL)
        return;

    _lastAspectCheck = now;

    if (!HasCorrectAspect())
        SwitchToOptimalAspect();
}

void BeastMasterySpecialization::SwitchToOptimalAspect()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 optimalAspect = GetOptimalAspect();
    if (optimalAspect != _currentAspect && bot->HasSpell(optimalAspect))
    {
        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Switching to aspect {} for bot {}",
                     optimalAspect, bot->GetName());

        bot->CastSpell(bot, optimalAspect, false);
        _currentAspect = optimalAspect;
    }
}

uint32 BeastMasterySpecialization::GetOptimalAspect() const
{
    Player* bot = GetBot();
    if (!bot)
        return ASPECT_OF_THE_HAWK;

    // Beast Mastery prefers DPS aspects
    if (bot->IsInCombat())
    {
        if (bot->HasSpell(ASPECT_OF_THE_DRAGONHAWK))
            return ASPECT_OF_THE_DRAGONHAWK;
        else if (bot->HasSpell(ASPECT_OF_THE_HAWK))
            return ASPECT_OF_THE_HAWK;
    }
    else
    {
        // Out of combat - travel aspects
        if (bot->HasSpell(ASPECT_OF_THE_PACK))
            return ASPECT_OF_THE_PACK;
        else if (bot->HasSpell(ASPECT_OF_THE_CHEETAH))
            return ASPECT_OF_THE_CHEETAH;
    }

    return ASPECT_OF_THE_HAWK;
}

bool BeastMasterySpecialization::HasCorrectAspect() const
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    uint32 optimalAspect = GetOptimalAspect();
    return bot->HasAura(optimalAspect);
}

void BeastMasterySpecialization::UpdateRangeManagement()
{
    // Implemented in base rotation
}

bool BeastMasterySpecialization::IsInDeadZone(::Unit* target) const
{
    if (!target)
        return false;

    float distance = GetDistanceToTarget(target);
    return distance > DEAD_ZONE_MIN && distance < DEAD_ZONE_MAX;
}

bool BeastMasterySpecialization::ShouldKite(::Unit* target) const
{
    if (!target)
        return false;

    // Beast Mastery should kite when pet is not available or in danger
    return !HasActivePet() || _petInfo.GetHealthPct() < 20.0f || GetBot()->GetHealthPct() < 40.0f;
}

Position BeastMasterySpecialization::GetKitePosition(::Unit* target) const
{
    Player* bot = GetBot();
    if (!bot || !target)
        return Position();

    // Move to maximum range while maintaining line of sight
    float angle = target->GetAngle(bot) + M_PI; // Opposite direction
    float distance = OPTIMAL_RANGE;

    return Position(
        target->GetPositionX() + distance * cos(angle),
        target->GetPositionY() + distance * sin(angle),
        target->GetPositionZ(),
        angle
    );
}

void BeastMasterySpecialization::HandleDeadZone(::Unit* target)
{
    if (!target)
        return;

    Player* bot = GetBot();
    if (!bot)
        return;

    // Move out of dead zone
    if (ShouldKite(target))
    {
        // Move away to optimal range
        Position kitePos = GetKitePosition(target);
        // Movement would be handled by the movement system
    }
    else
    {
        // Use instant abilities while moving
        if (ShouldUseArcaneShot(target))
            CastArcaneShot(target);
        else if (ShouldUseConcussiveShot(target))
            CastConcussiveShot(target);
    }
}

void BeastMasterySpecialization::UpdateTracking()
{
    uint32 now = getMSTime();
    if (now - _lastTrackingUpdate < TRACKING_UPDATE_INTERVAL)
        return;

    _lastTrackingUpdate = now;

    uint32 optimalTracking = GetOptimalTracking();
    if (optimalTracking != _currentTracking)
        ApplyTracking(optimalTracking);
}

uint32 BeastMasterySpecialization::GetOptimalTracking() const
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    // Choose tracking based on environment and targets
    if (bot->IsInCombat())
    {
        ::Unit* target = bot->GetTarget();
        if (target)
        {
            if (target->GetCreatureType() == CREATURE_TYPE_BEAST)
                return TRACK_BEASTS;
            else if (target->GetCreatureType() == CREATURE_TYPE_HUMANOID)
                return TRACK_HUMANOIDS;
            else if (target->GetCreatureType() == CREATURE_TYPE_UNDEAD)
                return TRACK_UNDEAD;
            else if (target->GetCreatureType() == CREATURE_TYPE_DEMON)
                return TRACK_DEMONS;
            else if (target->GetCreatureType() == CREATURE_TYPE_ELEMENTAL)
                return TRACK_ELEMENTALS;
            else if (target->GetCreatureType() == CREATURE_TYPE_GIANT)
                return TRACK_GIANTS;
            else if (target->GetCreatureType() == CREATURE_TYPE_DRAGONKIN)
                return TRACK_DRAGONKIN;
        }
    }

    // Default to humanoid tracking
    return TRACK_HUMANOIDS;
}

void BeastMasterySpecialization::ApplyTracking(uint32 trackingSpell)
{
    Player* bot = GetBot();
    if (!bot || !trackingSpell || !bot->HasSpell(trackingSpell))
        return;

    if (_currentTracking != trackingSpell)
    {
        TC_LOG_DEBUG("playerbot", "BeastMasterySpecialization: Applying tracking {} for bot {}",
                     trackingSpell, bot->GetName());

        bot->CastSpell(bot, trackingSpell, false);
        _currentTracking = trackingSpell;
    }
}

} // namespace Playerbot