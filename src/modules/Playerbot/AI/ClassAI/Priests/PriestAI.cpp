/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PriestAI.h"
#include "HolySpecialization.h"
#include "DisciplineSpecialization.h"
#include "ShadowSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Group.h"
#include "Item.h"
#include "MotionMaster.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "../CooldownManager.h"
#include "Spell.h"
#include "../../../Movement/BotMovementUtil.h"
#include "SpellAuras.h"
#include "SpellDefines.h"
#include "../BaselineRotationManager.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include <algorithm>
#include <chrono>

namespace Playerbot
{

// Talent IDs for specialization detection
enum PriestTalents
{
    // Holy talents
    TALENT_SPIRIT_OF_REDEMPTION = 20711,
    TALENT_CIRCLE_OF_HEALING = 34861,
    TALENT_GUARDIAN_SPIRIT = 47788,
    TALENT_LIGHTWELL = 724,
    TALENT_BLESSED_RECOVERY = 27811,
    TALENT_EMPOWERED_HEALING = 33158,

    // Discipline talents
    TALENT_POWER_INFUSION = 10060,
    TALENT_PAIN_SUPPRESSION = 33206,
    TALENT_PENANCE = 47540,
    TALENT_BORROWED_TIME = 52795,
    TALENT_SOUL_WARDING = 63574,
    TALENT_DIVINE_AEGIS = 47509,

    // Shadow talents
    TALENT_SHADOWFORM = 15473,
    TALENT_VAMPIRIC_EMBRACE = 15286,
    TALENT_VAMPIRIC_TOUCH = 34914,
    TALENT_DISPERSION = 47585,
    TALENT_SHADOW_WEAVING = 15257,
    TALENT_MISERY = 33191
};

// Combat constants
static const float OPTIMAL_HEALING_RANGE = 40.0f;
static const float OPTIMAL_DPS_RANGE = 30.0f;
static const float SAFE_DISTANCE = 20.0f;
static const uint32 DISPEL_COOLDOWN = 8000;
static const uint32 FEAR_WARD_COOLDOWN = 180000;
static const uint32 PSYCHIC_SCREAM_COOLDOWN = 30000;
static const uint32 INNER_FIRE_DURATION = 600000; // 10 minutes
static const float MANA_CONSERVATION_THRESHOLD = 0.3f;
static const float EMERGENCY_HEALTH_THRESHOLD = 0.25f;

PriestAI::PriestAI(Player* bot) : ClassAI(bot),
    _currentSpec(PriestSpec::HOLY),
    _manaSpent(0),
    _healingDone(0),
    _damageDealt(0),
    _playersHealed(0),
    _damagePrevented(0),
    _lastDispel(0),
    _lastFearWard(0),
    _lastPsychicScream(0),
    _lastInnerFire(0),
    _lastSilence(0),
    _lastMassDispel(0),
    _lastDesperatePrayer(0),
    _lastPowerInfusion(0),
    _lastShadowfiend(0),
    _lastDispersion(0),
    _lastPowerWordBarrier(0),
    _lastVoidEruption(0),
    _insanityLevel(0),
    _dotRefreshTime(0),
    _inVoidForm(false),
    _lastPenance(0),
    _graceStacks(0),
    _lastCircleOfHealing(0),
    _serendipityStacks(0),
    _lastGuardianSpirit(0)
{
    InitializeSpecialization();

    TC_LOG_DEBUG("module.playerbot.ai", "PriestAI created for player {} with specialization {}",
                 bot ? bot->GetName() : "null",
                 _specialization ? _specialization->GetSpecializationName() : "none");
}

PriestAI::~PriestAI() = default;

void PriestAI::UpdateRotation(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(GetBot());

        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))
            return;

        // Fallback: basic ranged attack
        if (!GetBot()->IsNonMeleeSpellCast(false))
        {
            if (target && GetBot()->GetDistance(target) <= 35.0f)
            {
                GetBot()->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // Check if we need to switch specialization
    PriestSpec newSpec = DetectCurrentSpecialization();
    if (newSpec != _currentSpec)
    {
        SwitchSpecialization(newSpec);
    }

    // Update shared priest mechanics
    UpdatePriestBuffs();

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Full priority system
    // ========================================================================

    // Execute combat behavior priorities - if any return true, they handled the action
    if (HandleCombatBehaviorPriorities(target))
        return;

    // Track combat metrics
    if (GetBot()->IsInCombat())
    {
        _damageDealt += CalculateDamageDealt(target);
        _healingDone += CalculateHealingDone();
        _manaSpent += CalculateManaUsage();
    }
}

void PriestAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(GetBot());
        return;
    }

    UpdatePriestBuffs();
    UpdateFortitudeBuffs();

    // Delegate to specialization for specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

void PriestAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update priest-specific cooldown tracking
    if (_cooldownManager)
        _cooldownManager->Update(diff);

    // Check for major cooldown availability
    // CheckMajorCooldowns(); // TODO: Implement when needed

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

bool PriestAI::CanUseAbility(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Check basic requirements
    if (!this->IsSpellReady(spellId) || !HasEnoughResource(spellId))
        return false;

    // Check priest-specific conditions
    if (IsHealingSpell(spellId) && ShouldConserveMana())
        return false;

    // Delegate to specialization for additional checks
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

void PriestAI::OnCombatStart(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} entering combat with {}",
                 GetBot()->GetName(), target->GetName());

    // Apply pre-combat preparations
    CastPowerWordFortitude();
    CastInnerFire();

    // Apply initial shield on self if needed
    if (GetBot()->GetHealthPct() < 90.0f)
    {
        this->CastSpell(GetBot(), POWER_WORD_SHIELD);
    }

    // Cast Fear Ward on tank if available
    if (Group* group = GetBot()->GetGroup())
    {
        if (::Unit* tank = FindGroupTank())
        {
            CastFearWard();
        }
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    // Initialize combat tracking
    _combatTime = 0;
    _inCombat = true;
    _currentTarget = target;
}

void PriestAI::OnCombatEnd()
{
    if (!GetBot())
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} leaving combat. Metrics - Healing: {}, Damage: {}, Mana Used: {}, Players Healed: {}",
                 GetBot()->GetName(), _healingDone, _damageDealt, _manaSpent, _playersHealed);

    // Post-combat healing
    HealGroupMembers();

    // Reapply buffs if needed
    UpdatePriestBuffs();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }

    // Reset combat tracking
    _inCombat = false;
    _currentTarget = nullptr;

    // Log performance metrics
    AnalyzeHealingEfficiency();
}

bool PriestAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA && GetBot()->GetPower(POWER_MANA) < int32(cost.Amount))
            return false;
    }

    // Check for mana conservation mode
    if (ShouldConserveMana() && !IsEmergencyHeal(spellId))
    {
        float manaPercent = GetManaPercent();
        if (manaPercent < MANA_CONSERVATION_THRESHOLD)
            return false;
    }

    // Delegate additional checks to specialization
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    return true;
}

void PriestAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Track mana consumption
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
            _manaSpent += cost.Amount;
    }

    // Track specific spell usage
    if (IsHealingSpell(spellId))
    {
        _playersHealed++;
    }
    else if (IsDamageSpell(spellId))
    {
        _damageDealt += 100; // Simplified
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

Position PriestAI::GetOptimalPosition(::Unit* target)
{
    if (!GetBot() || !target)
        return Position();

    // Delegate to specialization for position preference
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    // Default positioning for priests - maintain range
    float optimalRange = GetOptimalRange(target);
    float angle = GetBot()->GetAbsoluteAngle(target);

    // Position behind and to the side for safety
    angle += M_PI / 4; // 45 degrees offset

    float x = target->GetPositionX() - optimalRange * std::cos(angle);
    float y = target->GetPositionY() - optimalRange * std::sin(angle);
    float z = target->GetPositionZ();

    return Position(x, y, z);
}

float PriestAI::GetOptimalRange(::Unit* target)
{
    if (!GetBot() || !target)
        return OPTIMAL_HEALING_RANGE;

    // Delegate to specialization for range preference
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    // Default range based on spec
    switch (_currentSpec)
    {
        case PriestSpec::SHADOW:
            return OPTIMAL_DPS_RANGE;
        case PriestSpec::HOLY:
        case PriestSpec::DISCIPLINE:
        default:
            return OPTIMAL_HEALING_RANGE;
    }
}

void PriestAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void PriestAI::UpdateSpecialization()
{
    PriestSpec newSpec = DetectCurrentSpecialization();
    if (newSpec != _currentSpec)
    {
        SwitchSpecialization(newSpec);
    }
}

PriestSpec PriestAI::DetectCurrentSpecialization()
{
    if (!GetBot())
        return PriestSpec::HOLY;

    // Check for key Shadow talents
    if (GetBot()->HasSpell(TALENT_SHADOWFORM) ||
        GetBot()->HasSpell(TALENT_VAMPIRIC_TOUCH) ||
        GetBot()->HasSpell(TALENT_DISPERSION))
    {
        return PriestSpec::SHADOW;
    }

    // Check for key Discipline talents
    if (GetBot()->HasSpell(TALENT_PENANCE) ||
        GetBot()->HasSpell(TALENT_PAIN_SUPPRESSION) ||
        GetBot()->HasSpell(TALENT_POWER_INFUSION))
    {
        return PriestSpec::DISCIPLINE;
    }

    // Check for key Holy talents
    if (GetBot()->HasSpell(TALENT_CIRCLE_OF_HEALING) ||
        GetBot()->HasSpell(TALENT_GUARDIAN_SPIRIT) ||
        GetBot()->HasSpell(TALENT_SPIRIT_OF_REDEMPTION))
    {
        return PriestSpec::HOLY;
    }

    // Default to Holy if no clear specialization
    return PriestSpec::HOLY;
}

void PriestAI::SwitchSpecialization(PriestSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;
    _specialization.reset();

    switch (newSpec)
    {
        case PriestSpec::HOLY:
            _specialization = std::make_unique<HolySpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to Holy specialization", GetBot()->GetName());
            break;
        case PriestSpec::DISCIPLINE:
            _specialization = std::make_unique<DisciplineSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to Discipline specialization", GetBot()->GetName());
            break;
        case PriestSpec::SHADOW:
            _specialization = std::make_unique<ShadowSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to Shadow specialization", GetBot()->GetName());
            break;
    }

    OptimizeForSpecialization();
}

void PriestAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization || !target)
        return;

    _specialization->UpdateRotation(target);
}

void PriestAI::UpdatePriestBuffs()
{
    if (!GetBot())
        return;

    // Maintain Inner Fire
    if (!HasAura(INNER_FIRE) || (getMSTime() - _lastInnerFire > INNER_FIRE_DURATION))
    {
        CastInnerFire();
    }

    // Maintain Power Word: Fortitude
    if (!HasAura(POWER_WORD_FORTITUDE) && !HasAura(PRAYER_OF_FORTITUDE))
    {
        CastPowerWordFortitude();
    }

    // Maintain Divine Spirit if available
    if (GetBot()->HasSpell(DIVINE_SPIRIT) && !HasAura(DIVINE_SPIRIT) && !HasAura(PRAYER_OF_SPIRIT))
    {
        this->CastSpell(GetBot(), DIVINE_SPIRIT);
    }

    // Shadow Protection for shadow damage heavy encounters
    if (ShouldUseShadowProtection() && !HasAura(SHADOW_PROTECTION))
    {
        UseShadowProtection();
    }
}

void PriestAI::CastInnerFire()
{
    if (!GetBot() || !this->IsSpellReady(INNER_FIRE))
        return;

    if (this->CastSpell(GetBot(), INNER_FIRE))
    {
        _lastInnerFire = getMSTime();
    }
}

void PriestAI::UpdateFortitudeBuffs()
{
    if (!GetBot())
        return;

    // Check group members for fortitude
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (!player->HasAura(POWER_WORD_FORTITUDE) && !player->HasAura(PRAYER_OF_FORTITUDE))
                {
                    // Use Prayer of Fortitude if we have it and multiple people need it
                    if (GetBot()->HasSpell(PRAYER_OF_FORTITUDE) && CountUnbuffedGroupMembers(POWER_WORD_FORTITUDE) >= 3)
                    {
                        this->CastSpell(PRAYER_OF_FORTITUDE);
                        return;
                    }
                    else
                    {
                        this->CastSpell(player, POWER_WORD_FORTITUDE);
                        return;
                    }
                }
            }
        }
    }
}

void PriestAI::CastPowerWordFortitude()
{
    if (!GetBot() || !this->IsSpellReady(POWER_WORD_FORTITUDE))
        return;

    this->CastSpell(GetBot(), POWER_WORD_FORTITUDE);
}

bool PriestAI::HasEnoughMana(uint32 amount)
{
    return GetBot() && GetBot()->GetPower(POWER_MANA) >= int32(amount);
}

uint32 PriestAI::GetMana()
{
    return GetBot() ? GetBot()->GetPower(POWER_MANA) : 0;
}

uint32 PriestAI::GetMaxMana()
{
    return GetBot() ? GetBot()->GetMaxPower(POWER_MANA) : 0;
}

float PriestAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (float(GetMana()) / float(maxMana) * 100.0f) : 0.0f;
}

void PriestAI::OptimizeManaUsage()
{
    if (!GetBot())
        return;

    float manaPercent = GetManaPercent();

    // Use mana regeneration abilities
    if (manaPercent < 20.0f)
    {
        UseManaRegeneration();
    }

    // Use Shadowfiend if Shadow spec and low on mana
    if (_currentSpec == PriestSpec::SHADOW && manaPercent < 30.0f)
    {
        if (this->IsSpellReady(34433)) // Shadowfiend
        {
            if (Unit* target = GetTargetUnit())
                this->CastSpell(target, 34433);
        }
    }
}

bool PriestAI::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD * 100.0f;
}

void PriestAI::UseManaRegeneration()
{
    if (!GetBot())
        return;

    // Use Hymn of Hope if available
    if (this->IsSpellReady(HYMN_OF_HOPE))
    {
        CastHymnOfHope();
    }

    // Use mana potion if available
    // This would need item handling implementation
}

void PriestAI::CastHymnOfHope()
{
    if (!GetBot() || !this->IsSpellReady(HYMN_OF_HOPE))
        return;

    // Channel Hymn of Hope
    this->CastSpell(HYMN_OF_HOPE);
}

void PriestAI::UseDefensiveAbilities()
{
    if (!GetBot() || !IsInDanger())
        return;

    // Use Fade to reduce threat
    if (HasTooMuchThreat())
    {
        CastFade();
    }

    // Use Psychic Scream if enemies are too close
    if (GetNearestEnemy(8.0f))
    {
        CastPsychicScream();
    }

    // Use Desperate Prayer if health is low
    if (GetBot()->GetHealthPct() < 30.0f && this->IsSpellReady(48173)) // Desperate Prayer
    {
        this->CastSpell(GetBot(), 48173);
    }
}

void PriestAI::CastPsychicScream()
{
    if (!GetBot() || !this->IsSpellReady(PSYCHIC_SCREAM))
        return;

    if (getMSTime() - _lastPsychicScream < PSYCHIC_SCREAM_COOLDOWN)
        return;

    if (this->CastSpell(PSYCHIC_SCREAM))
    {
        _lastPsychicScream = getMSTime();
    }
}

void PriestAI::CastFade()
{
    if (!GetBot() || !this->IsSpellReady(FADE))
        return;

    this->CastSpell(FADE);
}

void PriestAI::CastDispelMagic()
{
    if (!GetBot())
        return;

    ::Unit* target = GetBestDispelTarget();
    if (target && this->IsSpellReady(DISPEL_MAGIC))
    {
        if (getMSTime() - _lastDispel > DISPEL_COOLDOWN)
        {
            if (this->CastSpell(target, DISPEL_MAGIC))
            {
                _lastDispel = getMSTime();
            }
        }
    }
}

void PriestAI::CastFearWard()
{
    if (!GetBot() || !this->IsSpellReady(FEAR_WARD))
        return;

    if (getMSTime() - _lastFearWard < FEAR_WARD_COOLDOWN)
        return;

    // Find best target for Fear Ward
    ::Unit* target = nullptr;
    if (Group* group = GetBot()->GetGroup())
    {
        target = FindGroupTank();
    }

    if (!target)
        target = GetBot();

    if (this->CastSpell(target, FEAR_WARD))
    {
        _lastFearWard = getMSTime();
    }
}

void PriestAI::UseShadowProtection()
{
    if (!GetBot())
        return;

    // Use Prayer of Shadow Protection if available for group
    if (GetBot()->HasSpell(PRAYER_OF_SHADOW_PROTECTION) && GetBot()->GetGroup())
    {
        this->CastSpell(PRAYER_OF_SHADOW_PROTECTION);
    }
    else if (this->IsSpellReady(SHADOW_PROTECTION))
    {
        this->CastSpell(GetBot(), SHADOW_PROTECTION);
    }
}

void PriestAI::UseCrowdControl(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    // Use Mind Control on humanoids
    if (target->GetCreatureType() == CREATURE_TYPE_HUMANOID)
    {
        CastMindControl(target);
    }
    // Use Shackle Undead on undead
    else if (target->GetCreatureType() == CREATURE_TYPE_UNDEAD)
    {
        CastShackleUndead(target);
    }
    // Use Silence on casters (if Shadow spec)
    else if (target->GetPowerType() == POWER_MANA && _currentSpec == PriestSpec::SHADOW)
    {
        CastSilence(target);
    }
}

void PriestAI::CastMindControl(::Unit* target)
{
    if (!GetBot() || !target || !this->IsSpellReady(MIND_CONTROL))
        return;

    // Don't mind control if we already have one
    if (!_mindControlTargets.empty())
        return;

    if (this->CastSpell(target, MIND_CONTROL))
    {
        _mindControlTargets[target->GetGUID()] = getMSTime();
    }
}

void PriestAI::CastShackleUndead(::Unit* target)
{
    if (!GetBot() || !target || !this->IsSpellReady(SHACKLE_UNDEAD))
        return;

    this->CastSpell(target, SHACKLE_UNDEAD);
}

void PriestAI::CastSilence(::Unit* target)
{
    if (!GetBot() || !target || !this->IsSpellReady(15487)) // Silence
        return;

    this->CastSpell(target, 15487);
}

void PriestAI::UpdatePriestPositioning()
{
    if (!GetBot() || !_currentTarget)
        return;

    // Check if we need to reposition
    if (!IsAtOptimalHealingRange(nullptr))
    {
        MaintainHealingPosition();
    }

    // Move away from danger
    if (IsInDanger())
    {
        FindSafePosition();
    }
}

bool PriestAI::IsAtOptimalHealingRange(::Unit* target)
{
    if (!GetBot())
        return false;

    if (!target)
    {
        // Check range to group members
        if (Group* group = GetBot()->GetGroup())
        {
            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                {
                    if (GetBot()->GetDistance(player) > OPTIMAL_HEALING_RANGE)
                        return false;
                }
            }
        }
        return true;
    }

    return GetBot()->GetDistance(target) <= OPTIMAL_HEALING_RANGE;
}

void PriestAI::MaintainHealingPosition()
{
    if (!GetBot())
        return;

    // Find center of group
    if (Group* group = GetBot()->GetGroup())
    {
        float avgX = 0, avgY = 0, avgZ = 0;
        uint32 count = 0;

        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                avgX += player->GetPositionX();
                avgY += player->GetPositionY();
                avgZ += player->GetPositionZ();
                count++;
            }
        }

        if (count > 0)
        {
            avgX /= count;
            avgY /= count;
            avgZ /= count;

            // Move towards group center but maintain safe distance from enemies
            Position centerPos(avgX, avgY, avgZ);
            BotMovementUtil::MoveToPosition(GetBot(), centerPos);
        }
    }
}

bool PriestAI::IsInDanger()
{
    if (!GetBot())
        return false;

    // Check if we have aggro
    if (HasTooMuchThreat())
        return true;

    // Check if enemies are too close
    if (GetNearestEnemy(10.0f))
        return true;

    // Check if health is low
    if (GetBot()->GetHealthPct() < 30.0f)
        return true;

    return false;
}

void PriestAI::FindSafePosition()
{
    if (!GetBot())
        return;

    // Move away from nearest enemy
    if (::Unit* enemy = GetNearestEnemy(20.0f))
    {
        float angle = GetBot()->GetAbsoluteAngle(enemy) + M_PI; // Opposite direction
        float distance = SAFE_DISTANCE;

        float x = GetBot()->GetPositionX() + distance * std::cos(angle);
        float y = GetBot()->GetPositionY() + distance * std::sin(angle);
        float z = GetBot()->GetPositionZ();

        GetBot()->GetMotionMaster()->MovePoint(0, x, y, z);
    }
}

::Unit* PriestAI::GetBestHealTarget()
{
    if (!GetBot())
        return nullptr;

    ::Unit* lowestHealthTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check self first
    if (GetBot()->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
        return GetBot();

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (!player->IsAlive())
                    continue;

                float healthPct = player->GetHealthPct();
                if (healthPct < lowestHealthPct && GetBot()->GetDistance(player) <= OPTIMAL_HEALING_RANGE)
                {
                    lowestHealthPct = healthPct;
                    lowestHealthTarget = player;
                }
            }
        }
    }

    // Return target if they need healing
    if (lowestHealthTarget && lowestHealthPct < 80.0f)
        return lowestHealthTarget;

    return nullptr;
}

::Unit* PriestAI::GetBestDispelTarget()
{
    if (!GetBot())
        return nullptr;

    // Priority: Self > Tank > Healer > DPS

    // Check self for dispellable debuffs
    if (HasDispellableDebuff(GetBot()))
        return GetBot();

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        // First pass - tanks
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (IsTank(player) && HasDispellableDebuff(player))
                    return player;
            }
        }

        // Second pass - healers
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (IsHealer(player) && HasDispellableDebuff(player))
                    return player;
            }
        }

        // Third pass - any member
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (HasDispellableDebuff(player))
                    return player;
            }
        }
    }

    return nullptr;
}

::Unit* PriestAI::GetBestMindControlTarget()
{
    if (!GetBot() || !_mindControlTargets.empty())
        return nullptr;

    // Look for humanoid enemies
    std::list<::Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(GetBot(), GetBot(), 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, checker);
    Cell::VisitAllObjects(GetBot(), searcher, 30.0f);

    for (::Unit* target : targets)
    {
        if (target->GetCreatureType() == CREATURE_TYPE_HUMANOID &&
            !target->HasAura(MIND_CONTROL) &&
            target->GetLevel() <= GetBot()->GetLevel() + 2)
        {
            return target;
        }
    }

    return nullptr;
}

::Unit* PriestAI::GetLowestHealthAlly(float maxRange)
{
    if (!GetBot())
        return nullptr;

    ::Unit* lowestHealthTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check self first
    if (GetBot()->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
        return GetBot();

    // Check group members
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (!player->IsAlive())
                    continue;

                float healthPct = player->GetHealthPct();
                if (healthPct < lowestHealthPct && GetBot()->GetDistance(player) <= maxRange)
                {
                    lowestHealthPct = healthPct;
                    lowestHealthTarget = player;
                }
            }
        }
    }

    // Return target if they need healing
    if (lowestHealthTarget && lowestHealthPct < 80.0f)
        return lowestHealthTarget;

    return nullptr;
}

::Unit* PriestAI::GetHighestPriorityPatient()
{
    if (!GetBot())
        return nullptr;

    struct PatientInfo
    {
        ::Unit* unit;
        float priority;
    };

    std::vector<PatientInfo> patients;

    // Calculate priority for each potential patient
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (!player->IsAlive() || player->GetHealthPct() >= 95.0f)
                    continue;

                float priority = 100.0f - player->GetHealthPct();

                // Increase priority for tanks (Warriors, Paladins, Death Knights)
                uint8 playerClass = player->GetClass();
                if (playerClass == CLASS_WARRIOR || playerClass == CLASS_PALADIN || playerClass == CLASS_DEATH_KNIGHT)
                    priority *= 1.5f;

                // Increase priority for healers (Priests, Druids, Shamans, Paladins)
                if (playerClass == CLASS_PRIEST || playerClass == CLASS_DRUID || playerClass == CLASS_SHAMAN)
                    priority *= 1.3f;

                // Increase priority if in combat
                if (player->IsInCombat())
                    priority *= 1.2f;

                patients.push_back({player, priority});
            }
        }
    }

    // Sort by priority
    std::sort(patients.begin(), patients.end(),
              [](const PatientInfo& a, const PatientInfo& b) { return a.priority > b.priority; });

    return patients.empty() ? nullptr : patients.front().unit;
}

void PriestAI::ProvideUtilitySupport()
{
    if (!GetBot())
        return;

    // Levitate falling allies
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (player->IsFalling() && !player->HasAura(LEVITATE))
                {
                    this->CastSpell(player, LEVITATE);
                    return;
                }
            }
        }
    }
}

void PriestAI::UpdateDispelling()
{
    if (!GetBot() || getMSTime() - _lastDispel < DISPEL_COOLDOWN)
        return;

    ::Unit* dispelTarget = GetBestDispelTarget();
    if (dispelTarget)
    {
        CastDispelMagic();
    }
}

void PriestAI::CheckForDebuffs()
{
    if (!GetBot())
        return;

    // Check for diseases to cure
    if (GetBot()->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) && this->IsSpellReady(CURE_DISEASE))
    {
        this->CastSpell(GetBot(), CURE_DISEASE);
    }

    // Check group members for debuffs
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (player->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) && this->IsSpellReady(CURE_DISEASE))
                {
                    this->CastSpell(player, CURE_DISEASE);
                    return;
                }
            }
        }
    }
}

void PriestAI::AssistGroupMembers()
{
    if (!GetBot())
        return;

    // Provide utility support
    ProvideUtilitySupport();

    // Update dispelling
    UpdateDispelling();
}

void PriestAI::AdaptToGroupRole()
{
    if (!GetBot())
        return;

    // Check group composition
    if (Group* group = GetBot()->GetGroup())
    {
        uint32 healerCount = 0;
        uint32 dpsCount = 0;

        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (IsHealer(player))
                    healerCount++;
                else
                    dpsCount++;
            }
        }

        // Adapt role based on group needs
        if (healerCount == 1 && _currentSpec != PriestSpec::SHADOW)
        {
            // We're the only healer, focus on healing
            SwitchToHealingRole();
        }
        else if (healerCount >= 2 && _currentSpec == PriestSpec::SHADOW)
        {
            // Enough healers, can DPS
            SwitchToDamageRole();
        }
    }
}

void PriestAI::SwitchToHealingRole()
{
    if (!GetBot())
        return;

    // Prioritize healing abilities
    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to healing role", GetBot()->GetName());
}

void PriestAI::SwitchToDamageRole()
{
    if (!GetBot())
        return;

    // Prioritize damage abilities
    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to damage role", GetBot()->GetName());
}

void PriestAI::DetermineOptimalRole()
{
    AdaptToGroupRole();
}

void PriestAI::ManageHolyPower()
{
    // Holy-specific mechanics
    if (_currentSpec != PriestSpec::HOLY)
        return;

    // Manage Serendipity stacks for faster heals
    ManageSerendipity();

    // Update Circle of Healing usage
    UpdateCircleOfHealing();
}

void PriestAI::UpdateCircleOfHealing()
{
    if (!GetBot() || !this->IsSpellReady(CIRCLE_OF_HEALING))
        return;

    // Count injured group members
    uint32 injuredCount = 0;
    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (player->GetHealthPct() < 80.0f)
                    injuredCount++;
            }
        }
    }

    // Use Circle of Healing if 3+ injured
    if (injuredCount >= 3)
    {
        this->CastSpell(CIRCLE_OF_HEALING);
    }
}

void PriestAI::ManageSerendipity()
{
    // Track and optimize Serendipity buff usage
    if (!GetBot())
        return;

    uint32 serendipityStacks = GetAuraStacks(63733, GetBot()); // Serendipity

    // Use faster Greater Heal or Prayer of Healing at 3 stacks
    if (serendipityStacks >= 3)
    {
        ::Unit* healTarget = GetHighestPriorityPatient();
        if (healTarget && healTarget->GetHealthPct() < 50.0f)
        {
            this->CastSpell(healTarget, GREATER_HEAL);
        }
    }
}

void PriestAI::ManageDisciplineMechanics()
{
    // Discipline-specific mechanics
    if (_currentSpec != PriestSpec::DISCIPLINE)
        return;

    UpdateBorrowedTime();
    ManageGrace();
}

void PriestAI::UpdateBorrowedTime()
{
    // Manage Borrowed Time buff for haste after shield
    if (!GetBot())
        return;

    // Cast Power Word: Shield to trigger Borrowed Time
    ::Unit* target = GetHighestPriorityPatient();
    if (target && !target->HasAura(POWER_WORD_SHIELD) && this->IsSpellReady(POWER_WORD_SHIELD))
    {
        this->CastSpell(target, POWER_WORD_SHIELD);
    }
}

void PriestAI::ManageGrace()
{
    // Manage Grace buff stacking on healing targets
    if (!GetBot())
        return;

    // Grace stacks from Flash Heal, Greater Heal, and Penance
    ::Unit* target = GetHighestPriorityPatient();
    if (target)
    {
        uint32 graceStacks = GetAuraStacks(47509, target); // Grace

        // Maintain Grace stacks on tank
        if (IsTank(target) && graceStacks < 3)
        {
            this->CastSpell(target, FLASH_HEAL);
        }
    }
}

void PriestAI::ManageShadowMechanics()
{
    // Shadow-specific mechanics
    if (_currentSpec != PriestSpec::SHADOW)
        return;

    UpdateShadowWeaving();
    ManageVampiricEmbrace();
}

void PriestAI::UpdateShadowWeaving()
{
    // Manage Shadow Weaving debuff stacking
    if (!GetBot() || !_currentTarget)
        return;

    Unit* target = GetTargetUnit();
    if (!target)
        return;

    uint32 shadowWeavingStacks = GetAuraStacks(15258, target); // Shadow Weaving

    // Maintain 5 stacks of Shadow Weaving
    if (shadowWeavingStacks < 5)
    {
        this->CastSpell(target, MIND_BLAST);
    }
}

void PriestAI::ManageVampiricEmbrace()
{
    // Manage Vampiric Embrace for group healing
    if (!GetBot() || !this->IsSpellReady(VAMPIRIC_EMBRACE))
        return;

    // Activate Vampiric Embrace when group needs healing
    if (Group* group = GetBot()->GetGroup())
    {
        uint32 injuredCount = 0;
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (player->GetHealthPct() < 70.0f)
                    injuredCount++;
            }
        }

        if (injuredCount >= 2 && !HasAura(VAMPIRIC_EMBRACE))
        {
            this->CastSpell(VAMPIRIC_EMBRACE);
        }
    }
}

void PriestAI::ManageThreat()
{
    if (!GetBot())
        return;

    if (HasTooMuchThreat())
    {
        ReduceThreat();
    }
}

bool PriestAI::HasTooMuchThreat()
{
    Unit* target = GetTargetUnit();
    if (!GetBot() || !target)
        return false;

    // Check if we have aggro
    return target->GetVictim() == GetBot();
}

void PriestAI::ReduceThreat()
{
    UseFade();
}

void PriestAI::UseFade()
{
    CastFade();
}

void PriestAI::RecordHealingDone(uint32 amount, ::Unit* target)
{
    _healingDone += amount;
    if (target && target->IsPlayer())
        _playersHealed++;
}

void PriestAI::RecordDamageDone(uint32 amount, ::Unit* target)
{
    _damageDealt += amount;
}

void PriestAI::AnalyzeHealingEfficiency()
{
    if (!GetBot() || _healingDone == 0 || _manaSpent == 0)
        return;

    float efficiency = float(_healingDone) / float(_manaSpent);

    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} healing efficiency: {:.2f} healing per mana",
                 GetBot()->GetName(), efficiency);
}

void PriestAI::OptimizeHealingRotation()
{
    // Analyze and optimize healing spell usage
    AnalyzeHealingEfficiency();
}

bool PriestAI::IsHealingSpell(uint32 spellId)
{
    switch (spellId)
    {
                case HEAL:
                case GREATER_HEAL:
                case FLASH_HEAL:
                case RENEW:
                case PRAYER_OF_HEALING:
                case CIRCLE_OF_HEALING:
                case BINDING_HEAL:
                case GUARDIAN_SPIRIT:
                case PENANCE:
            return true;
        default:
            return false;
    }
}

bool PriestAI::IsDamageSpell(uint32 spellId)
{
    switch (spellId)
    {
                case SHADOW_WORD_PAIN:
                case MIND_BLAST:
                case MIND_FLAY:
                case VAMPIRIC_TOUCH:
                case DEVOURING_PLAGUE:
                case SHADOW_WORD_DEATH:
            return true;
        default:
            return false;
    }
}

uint32 PriestAI::GetSpellHealAmount(uint32 spellId)
{
    // Simplified heal amount calculation
    // In real implementation, this would calculate based on spell power, talents, etc.
    switch (spellId)
    {
                case FLASH_HEAL:
            return 2000;
                case GREATER_HEAL:
            return 4000;
                case HEAL:
            return 3000;
        default:
            return 1000;
    }
}

uint32 PriestAI::GetHealOverTimeRemaining(::Unit* target, uint32 spellId)
{
    if (!target)
        return 0;

    // Check remaining HoT duration
    if (Aura* aura = target->GetAura(spellId))
    {
        return aura->GetDuration();
    }

    return 0;
}

bool PriestAI::TargetHasHoT(::Unit* target, uint32 spellId)
{
    return target && target->HasAura(spellId);
}

PriestSpec PriestAI::DetectSpecialization()
{
    return DetectCurrentSpecialization();
}

void PriestAI::OptimizeForSpecialization()
{
    if (!GetBot())
        return;

    // Optimize action bars and priorities based on spec
    switch (_currentSpec)
    {
        case PriestSpec::HOLY:
            TC_LOG_DEBUG("module.playerbot.ai", "Optimizing {} for Holy healing", GetBot()->GetName());
            break;
        case PriestSpec::DISCIPLINE:
            TC_LOG_DEBUG("module.playerbot.ai", "Optimizing {} for Discipline support", GetBot()->GetName());
            break;
        case PriestSpec::SHADOW:
            TC_LOG_DEBUG("module.playerbot.ai", "Optimizing {} for Shadow damage", GetBot()->GetName());
            break;
    }
}

bool PriestAI::HasTalent(uint32 talentId)
{
    return GetBot() && GetBot()->HasSpell(talentId);
}

// Helper methods implementation
bool PriestAI::ShouldPrioritizeHealing(::Unit* target)
{
    if (!target)
        return false;

    // Always prioritize emergency healing
    if (target->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD * 100.0f)
        return true;

    // Prioritize tank healing
    if (IsTank(target) && target->GetHealthPct() < 50.0f)
        return true;

    // Prioritize self-healing if in danger
    if (target == GetBot() && GetBot()->GetHealthPct() < 40.0f)
        return true;

    return false;
}

void PriestAI::HealTarget(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    float healthPct = target->GetHealthPct();

    // Emergency healing
    if (healthPct < 30.0f)
    {
        if (this->IsSpellReady(FLASH_HEAL))
            this->CastSpell(target, FLASH_HEAL);
        else if (this->IsSpellReady(BINDING_HEAL) && GetBot()->GetHealthPct() < 70.0f)
            this->CastSpell(target, BINDING_HEAL);
    }
    // Moderate healing
    else if (healthPct < 60.0f)
    {
        if (!this->TargetHasHoT(target, RENEW))
            this->CastSpell(target, RENEW);

        if (this->IsSpellReady(GREATER_HEAL))
            this->CastSpell(target, GREATER_HEAL);
    }
    // Maintenance healing
    else if (healthPct < 80.0f)
    {
        if (!this->TargetHasHoT(target, RENEW))
            this->CastSpell(target, RENEW);
        else if (this->IsSpellReady(HEAL))
            this->CastSpell(target, HEAL);
    }
}

void PriestAI::HealGroupMembers()
{
    if (!GetBot())
        return;

    if (Group* group = GetBot()->GetGroup())
    {
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
            {
                if (player->IsAlive() && player->GetHealthPct() < 90.0f)
                {
                    HealTarget(player);
                }
            }
        }
    }
}


bool PriestAI::ShouldUseShadowProtection()
{
    // Check if we're facing shadow damage enemies
    // Simplified - would check actual enemy abilities in real implementation
    return false;
}


// Helper method implementations
void PriestAI::CheckMajorCooldowns()
{
    if (!GetBot())
        return;

    // Check if major cooldowns are available
    // Guardian Spirit, Pain Suppression, Power Infusion, etc.
}

bool PriestAI::IsTank(::Unit* unit)
{
    if (!unit || !unit->IsPlayer())
        return false;

    Player* player = unit->ToPlayer();

    // Simple tank detection
    return player->GetClass() == CLASS_WARRIOR ||
           player->GetClass() == CLASS_PALADIN ||
           player->GetClass() == CLASS_DEATH_KNIGHT;
}

bool PriestAI::IsHealer(::Unit* unit)
{
    if (!unit || !unit->IsPlayer())
        return false;

    Player* player = unit->ToPlayer();

    // Simple healer detection
    return player->GetClass() == CLASS_PRIEST ||
           player->GetClass() == CLASS_PALADIN ||
           player->GetClass() == CLASS_SHAMAN ||
           player->GetClass() == CLASS_DRUID;
}

bool PriestAI::HasDispellableDebuff(::Unit* unit)
{
    if (!unit)
        return false;

    // Check for magic debuffs that can be dispelled by Dispel Magic
    const Unit::AuraApplicationMap& auras = unit->GetAppliedAuras();
    for (auto const& itr : auras)
    {
        if (Aura* aura = itr.second->GetBase())
        {
            if (SpellInfo const* spellInfo = aura->GetSpellInfo())
            {
                // Check if it's a magic debuff that can be dispelled
                if ((spellInfo->GetDispelMask() & (1 << DISPEL_MAGIC)) &&
                    !spellInfo->IsPositive() &&
                    spellInfo->Dispel == DISPEL_MAGIC)
                {
                    return true;
                }
            }
        }
    }

    return false;
}


uint32 PriestAI::CalculateDamageDealt(::Unit* target) const
{
    // Simplified damage calculation
    return _currentSpec == PriestSpec::SHADOW ? 200 : 50;
}

uint32 PriestAI::CalculateHealingDone() const
{
    // Simplified healing calculation
    return _currentSpec != PriestSpec::SHADOW ? 300 : 100;
}

uint32 PriestAI::CalculateManaUsage() const
{
    // Simplified mana usage calculation
    return 100;
}

// PriestHealCalculator implementation
std::unordered_map<uint32, uint32> PriestHealCalculator::_baseHealCache;
std::unordered_map<uint32, float> PriestHealCalculator::_efficiencyCache;
std::mutex PriestHealCalculator::_cacheMutex;

uint32 PriestHealCalculator::CalculateHealAmount(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // Simplified heal calculation
    uint32 baseHeal = 1000;

    switch (spellId)
    {
        case 2061: // Flash Heal
            baseHeal = 2000;
            break;
        case 2060: // Greater Heal
            baseHeal = 4000;
            break;
        case 2050: // Heal
            baseHeal = 3000;
            break;
        case 139: // Renew
            baseHeal = 1500;
            break;
        case 596: // Prayer of Healing
            baseHeal = 2500;
            break;
    }

    // Apply spell power scaling (simplified)
    if (Player* player = caster->ToPlayer())
    {
        float spellPower = player->GetBaseSpellPowerBonus();
        baseHeal += uint32(spellPower * 0.8f);
    }

    return baseHeal;
}

uint32 PriestHealCalculator::CalculateHealOverTime(uint32 spellId, Player* caster)
{
    if (!caster)
        return 0;

    // Simplified HoT calculation
    switch (spellId)
    {
        case 139: // Renew
            return 3000; // Total over duration
        default:
            return 0;
    }
}

float PriestHealCalculator::CalculateHealEfficiency(uint32 spellId, Player* caster)
{
    if (!caster)
        return 0.0f;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    auto it = _efficiencyCache.find(spellId);
    if (it != _efficiencyCache.end())
        return it->second;

    // Calculate heal per mana
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0.0f;

    auto powerCosts = spellInfo->CalcPowerCost(caster, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
            manaCost = cost.Amount;
    }

    if (manaCost == 0)
        return 999.0f; // Free spell

    uint32 healAmount = CalculateHealAmount(spellId, caster, caster);
    float efficiency = float(healAmount) / float(manaCost);

    _efficiencyCache[spellId] = efficiency;
    return efficiency;
}

float PriestHealCalculator::CalculateHealPerMana(uint32 spellId, Player* caster)
{
    return CalculateHealEfficiency(spellId, caster);
}

uint32 PriestHealCalculator::GetOptimalHealForSituation(Player* caster, ::Unit* target, uint32 missingHealth)
{
    if (!caster || !target)
        return 0;

    // Choose heal based on missing health
    if (missingHealth > 4000)
        return 2060; // Greater Heal
    else if (missingHealth > 2000)
        return 2050; // Heal
    else if (missingHealth > 1000)
        return 2061; // Flash Heal
    else
        return 139; // Renew
}

bool PriestHealCalculator::ShouldUseDirectHeal(Player* caster, ::Unit* target)
{
    if (!target)
        return false;

    return target->GetHealthPct() < 70.0f;
}

bool PriestHealCalculator::ShouldUseHealOverTime(Player* caster, ::Unit* target)
{
    if (!target)
        return false;

    return target->GetHealthPct() > 70.0f && !target->HasAura(139); // Renew
}

bool PriestHealCalculator::ShouldUseGroupHeal(Player* caster, const std::vector<::Unit*>& targets)
{
    if (targets.size() < 3)
        return false;

    uint32 injuredCount = 0;
    for (::Unit* target : targets)
    {
        if (target && target->GetHealthPct() < 80.0f)
            injuredCount++;
    }

    return injuredCount >= 3;
}

float PriestHealCalculator::CalculateHealThreat(uint32 healAmount, Player* caster)
{
    // Healing generates 0.5 threat per point healed
    return float(healAmount) * 0.5f;
}

bool PriestHealCalculator::WillOverheal(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return false;

    uint32 healAmount = CalculateHealAmount(spellId, caster, target);
    uint32 missingHealth = target->GetMaxHealth() - target->GetHealth();

    return healAmount > missingHealth * 1.1f; // 10% overheal threshold
}

void PriestHealCalculator::CacheHealData(uint32 spellId)
{
    // Pre-cache heal data for performance
    std::lock_guard<std::mutex> lock(_cacheMutex);

    if (_baseHealCache.find(spellId) == _baseHealCache.end())
    {
        // Cache base heal values
        switch (spellId)
        {
            case 2061: // Flash Heal
                _baseHealCache[spellId] = 2000;
                break;
            case 2060: // Greater Heal
                _baseHealCache[spellId] = 4000;
                break;
            case 2050: // Heal
                _baseHealCache[spellId] = 3000;
                break;
            default:
                _baseHealCache[spellId] = 1000;
                break;
        }
    }
}



// Missing utility method implementations (stubs)

bool PriestAI::IsEmergencyHeal(::Unit* target)
{
    if (!target)
        return false;
    return target->GetHealthPct() < 30.0f;
}

bool PriestAI::IsEmergencyHeal(uint32 spellId)
{
    // Define emergency healing spells
    switch (spellId)
    {
        case 2061:   // Flash Heal
        case 2060:   // Greater Heal
        case 139:    // Renew
        case 596:    // Prayer of Healing
        case 33076:  // Prayer of Mending
        case 64843:  // Divine Hymn
        case 64901:  // Hymn of Hope
            return true;
        default:
            return false;
    }
}

uint32 PriestAI::CountUnbuffedGroupMembers(uint32 spellId)
{
    // Stub implementation
    return 0;
}

::Unit* PriestAI::FindGroupTank()
{
    if (!GetBot() || !GetBot()->GetGroup())
        return nullptr;

    Group* group = GetBot()->GetGroup();
    for (Group::MemberSlot const& member : group->GetMemberSlots())
    {
        if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
        {
            if (IsTank(player))
                return player;
        }
    }
    return nullptr;
}

// ============================================================================
// COMBAT BEHAVIOR INTEGRATION IMPLEMENTATION
// ============================================================================

bool PriestAI::HandleCombatBehaviorPriorities(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors)
        return ExecuteNormalRotation(target);

    // Priority 1: Interrupts (Silence for Shadow spec)
    if (HandleInterruptPriority(target))
        return true;

    // Priority 2: Defensives (Dispersion, Power Word: Shield, Desperate Prayer)
    if (HandleDefensivePriority())
        return true;

    // Priority 3: Positioning - Maintain max range for safety
    if (HandlePositioningPriority(target))
        return true;

    // Priority 4: Dispel - Mass Dispel, Purify for group members
    if (HandleDispelPriority())
        return true;

    // Priority 5: Target Switching - Priority targets
    if (HandleTargetSwitchPriority(target))
        return true;

    // Priority 6: Crowd Control - Psychic Scream, Mind Control
    if (HandleCrowdControlPriority(target))
        return true;

    // Priority 7: AoE Decisions - Mind Sear, Shadow Crash for Shadow
    if (HandleAoEPriority(target))
        return true;

    // Priority 8: Offensive Cooldowns - Void Eruption, Power Infusion, Shadow Fiend
    if (HandleCooldownPriority(target))
        return true;

    // Priority 9: Resource Management - Insanity/Mana management
    if (HandleResourceManagement())
        return true;

    // Priority 10: Normal Rotation - DoTs, healing, spec-specific
    return ExecuteNormalRotation(target);
}

bool PriestAI::HandleInterruptPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldInterrupt(target))
        return false;

    // Shadow priests have Silence
    if (_currentSpec == PriestSpec::SHADOW)
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (!interruptTarget)
            interruptTarget = target;

        if (interruptTarget && interruptTarget->IsNonMeleeSpellCast(false))
        {
            // Use Silence
            if (IsSpellReady(SILENCE) && getMSTime() - _lastSilence > 45000)
            {
                if (CastSpell(interruptTarget, SILENCE))
                {
                    _lastSilence = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} silenced {}",
                                 GetBot()->GetName(), interruptTarget->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

bool PriestAI::HandleDefensivePriority()
{
    if (!GetBot())
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->NeedsDefensive())
        return false;

    float healthPct = GetBot()->GetHealthPct();

    // Emergency: Dispersion for Shadow (90% damage reduction)
    if (_currentSpec == PriestSpec::SHADOW && healthPct < 20.0f)
    {
        if (IsSpellReady(DISPERSION) && getMSTime() - _lastDispersion > 120000)
        {
            if (CastSpell(DISPERSION))
            {
                _lastDispersion = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Dispersion at {}% health",
                             GetBot()->GetName(), healthPct);
                return true;
            }
        }
    }

    // Critical: Power Word: Shield
    if (healthPct < 30.0f)
    {
        if (!HasAura(POWER_WORD_SHIELD) && IsSpellReady(POWER_WORD_SHIELD))
        {
            if (CastSpell(GetBot(), POWER_WORD_SHIELD))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} shielded self at {}% health",
                             GetBot()->GetName(), healthPct);
                return true;
            }
        }
    }

    // Low health: Desperate Prayer (instant self heal)
    if (healthPct < 40.0f)
    {
        if (IsSpellReady(DESPERATE_PRAYER) && getMSTime() - _lastDesperatePrayer > 120000)
        {
            if (CastSpell(GetBot(), DESPERATE_PRAYER))
            {
                _lastDesperatePrayer = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Desperate Prayer at {}% health",
                             GetBot()->GetName(), healthPct);
                return true;
            }
        }
    }

    // Discipline: Pain Suppression on self or tank
    if (_currentSpec == PriestSpec::DISCIPLINE && healthPct < 35.0f)
    {
        if (IsSpellReady(PAIN_SUPPRESSION))
        {
            Unit* psTarget = GetBot();
            if (Unit* tank = FindGroupTank())
            {
                if (tank->GetHealthPct() < 30.0f)
                    psTarget = tank;
            }

            if (CastSpell(psTarget, PAIN_SUPPRESSION))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Pain Suppression on {}",
                             GetBot()->GetName(), psTarget->GetName());
                return true;
            }
        }
    }

    // Holy: Guardian Spirit
    if (_currentSpec == PriestSpec::HOLY)
    {
        if (IsSpellReady(GUARDIAN_SPIRIT) && getMSTime() - _lastGuardianSpirit > 180000)
        {
            Unit* gsTarget = nullptr;
            if (healthPct < 25.0f)
                gsTarget = GetBot();
            else if (Unit* tank = FindGroupTank())
            {
                if (tank->GetHealthPct() < 20.0f)
                    gsTarget = tank;
            }

            if (gsTarget && CastSpell(gsTarget, GUARDIAN_SPIRIT))
            {
                _lastGuardianSpirit = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Guardian Spirit on {}",
                             GetBot()->GetName(), gsTarget->GetName());
                return true;
            }
        }
    }

    // Fade to drop aggro
    if (HasTooMuchThreat())
    {
        if (IsSpellReady(FADE))
        {
            if (CastSpell(FADE))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Fade to drop threat",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    return false;
}

bool PriestAI::HandlePositioningPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->NeedsRepositioning())
        return false;

    float currentDistance = GetBot()->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    // Too close - need to move back
    if (currentDistance < 20.0f)
    {
        // Use Psychic Scream if enemies are very close
        if (currentDistance < 8.0f && IsSpellReady(PSYCHIC_SCREAM))
        {
            if (getMSTime() - _lastPsychicScream > 30000)
            {
                if (CastSpell(PSYCHIC_SCREAM))
                {
                    _lastPsychicScream = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Psychic Scream for positioning",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }

        // Movement handled by BotAI movement strategies
        // Just ensure we're using instant casts while repositioning
        if (GetBot()->isMoving())
        {
            // Shadow: Shadow Word: Pain, Vampiric Touch (instant with talent)
            if (_currentSpec == PriestSpec::SHADOW)
            {
                if (!target->HasAura(SHADOW_WORD_PAIN) && IsSpellReady(SHADOW_WORD_PAIN))
                {
                    CastSpell(target, SHADOW_WORD_PAIN);
                    return true;
                }
            }
            // Holy/Discipline: Renew, Power Word: Shield
            else
            {
                Unit* healTarget = GetLowestHealthAlly(40.0f);
                if (healTarget && healTarget->GetHealthPct() < 80.0f)
                {
                    if (!healTarget->HasAura(RENEW) && IsSpellReady(RENEW))
                    {
                        CastSpell(healTarget, RENEW);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool PriestAI::HandleDispelPriority()
{
    if (!GetBot())
        return false;

    // Check dispel cooldown
    if (getMSTime() - _lastDispel < 8000)
        return false;

    Unit* dispelTarget = GetBestDispelTarget();
    if (!dispelTarget)
        return false;

    // Mass Dispel for multiple targets or important dispels
    if (IsSpellReady(MASS_DISPEL) && getMSTime() - _lastMassDispel > 45000)
    {
        // Check if multiple allies need dispelling
        uint32 needsDispelCount = 0;
        if (Group* group = GetBot()->GetGroup())
        {
            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                {
                    if (HasDispellableDebuff(player))
                        needsDispelCount++;
                }
            }
        }

        if (needsDispelCount >= 3)
        {
            if (CastSpell(dispelTarget, MASS_DISPEL))
            {
                _lastMassDispel = getMSTime();
                _lastDispel = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Mass Dispel",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    // Single target dispel (Dispel Magic for enemies, Purify for allies)
    if (dispelTarget->IsFriendlyTo(GetBot()))
    {
        if (IsSpellReady(PURIFY))
        {
            if (CastSpell(dispelTarget, PURIFY))
            {
                _lastDispel = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} purified {}",
                             GetBot()->GetName(), dispelTarget->GetName());
                return true;
            }
        }
    }
    else
    {
        if (IsSpellReady(DISPEL_MAGIC))
        {
            if (CastSpell(dispelTarget, DISPEL_MAGIC))
            {
                _lastDispel = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} dispelled magic on {}",
                             GetBot()->GetName(), dispelTarget->GetName());
                return true;
            }
        }
    }

    return false;
}

bool PriestAI::HandleTargetSwitchPriority(::Unit*& target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldSwitchTarget())
        return false;

    Unit* priorityTarget = behaviors->GetPriorityTarget();
    if (!priorityTarget || priorityTarget == target)
        return false;

    // Mind Control the old target if possible (Shadow spec preferred)
    if (_currentSpec == PriestSpec::SHADOW && target->GetCreatureType() == CREATURE_TYPE_HUMANOID)
    {
        if (IsSpellReady(MIND_CONTROL) && _mindControlTargets.empty())
        {
            if (CastSpell(target, MIND_CONTROL))
            {
                _mindControlTargets[target->GetGUID()] = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} mind controlled {} before switching",
                             GetBot()->GetName(), target->GetName());
            }
        }
    }

    // Switch to priority target
    target = priorityTarget;
    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} switching to priority target {}",
                 GetBot()->GetName(), priorityTarget->GetName());

    // Apply initial debuffs to new target
    if (_currentSpec == PriestSpec::SHADOW)
    {
        RefreshDoTs(target);
    }

    return true;
}

bool PriestAI::HandleCrowdControlPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldUseCrowdControl())
        return false;

    Unit* ccTarget = behaviors->GetCrowdControlTarget();
    if (!ccTarget)
        ccTarget = target;

    // Shackle Undead for undead enemies
    if (ccTarget->GetCreatureType() == CREATURE_TYPE_UNDEAD)
    {
        if (IsSpellReady(SHACKLE_UNDEAD))
        {
            if (CastSpell(ccTarget, SHACKLE_UNDEAD))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} shackled undead {}",
                             GetBot()->GetName(), ccTarget->GetName());
                return true;
            }
        }
    }

    // Mind Control for humanoids
    if (ccTarget->GetCreatureType() == CREATURE_TYPE_HUMANOID && _mindControlTargets.empty())
    {
        if (IsSpellReady(MIND_CONTROL))
        {
            if (CastSpell(ccTarget, MIND_CONTROL))
            {
                _mindControlTargets[ccTarget->GetGUID()] = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} mind controlled {}",
                             GetBot()->GetName(), ccTarget->GetName());
                return true;
            }
        }
    }

    // Psychic Scream for emergency CC (multiple melee enemies)
    if (GetNearestEnemy(8.0f))
    {
        uint32 nearbyEnemies = 0;
        std::list<Unit*> enemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(GetBot(), GetBot(), 8.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), enemies, checker);
        Cell::VisitAllObjects(GetBot(), searcher, 8.0f);
        nearbyEnemies = enemies.size();

        if (nearbyEnemies >= 2 && IsSpellReady(PSYCHIC_SCREAM))
        {
            if (getMSTime() - _lastPsychicScream > 30000)
            {
                if (CastSpell(PSYCHIC_SCREAM))
                {
                    _lastPsychicScream = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Psychic Scream for crowd control",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

bool PriestAI::HandleAoEPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldAOE())
        return false;

    // Count nearby enemies
    uint32 nearbyEnemies = 0;
    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(GetBot(), GetBot(), 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), enemies, checker);
    Cell::VisitAllObjects(GetBot(), searcher, 30.0f);
    nearbyEnemies = enemies.size();

    // Shadow: Mind Sear for 4+ enemies
    if (_currentSpec == PriestSpec::SHADOW && nearbyEnemies >= 4)
    {
        if (IsSpellReady(MIND_SEAR))
        {
            if (CastSpell(target, MIND_SEAR))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} using Mind Sear on {} enemies",
                             GetBot()->GetName(), nearbyEnemies);
                return true;
            }
        }

        // Shadow Crash for AoE burst
        if (IsSpellReady(SHADOW_CRASH))
        {
            Position aoePos = behaviors->GetOptimalPosition();
            if (CastSpell(target, SHADOW_CRASH))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} using Shadow Crash",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    // Holy: Prayer of Healing for group healing
    if (_currentSpec == PriestSpec::HOLY)
    {
        uint32 injuredCount = 0;
        if (Group* group = GetBot()->GetGroup())
        {
            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                {
                    if (player->GetHealthPct() < 75.0f)
                        injuredCount++;
                }
            }
        }

        if (injuredCount >= 3)
        {
            // Circle of Healing for instant group heal
            if (IsSpellReady(CIRCLE_OF_HEALING) && getMSTime() - _lastCircleOfHealing > 15000)
            {
                if (CastSpell(CIRCLE_OF_HEALING))
                {
                    _lastCircleOfHealing = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Circle of Healing",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Prayer of Healing
            if (IsSpellReady(PRAYER_OF_HEALING))
            {
                if (CastSpell(GetBot(), PRAYER_OF_HEALING))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} casting Prayer of Healing",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    // Discipline: Power Word: Barrier for raid damage
    if (_currentSpec == PriestSpec::DISCIPLINE)
    {
        if (IsSpellReady(POWER_WORD_BARRIER) && getMSTime() - _lastPowerWordBarrier > 180000)
        {
            // Use barrier when multiple allies are taking damage
            uint32 damagedCount = 0;
            if (Group* group = GetBot()->GetGroup())
            {
                for (Group::MemberSlot const& member : group->GetMemberSlots())
                {
                    if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                    {
                        if (player->GetHealthPct() < 60.0f)
                            damagedCount++;
                    }
                }
            }

            if (damagedCount >= 3)
            {
                if (CastSpell(GetBot(), POWER_WORD_BARRIER))
                {
                    _lastPowerWordBarrier = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Power Word: Barrier",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

bool PriestAI::HandleCooldownPriority(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !behaviors->ShouldUseCooldowns())
        return false;

    // Shadow: Void Eruption / Shadow Fiend
    if (_currentSpec == PriestSpec::SHADOW)
    {
        // Void Eruption - Transform into Void Form
        if (ShouldUseVoidEruption() && IsSpellReady(VOID_ERUPTION))
        {
            if (getMSTime() - _lastVoidEruption > 90000)
            {
                if (CastSpell(target, VOID_ERUPTION))
                {
                    _lastVoidEruption = getMSTime();
                    EnterVoidForm();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} entered Void Form",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }

        // Shadowfiend for mana and damage
        if (GetManaPercent() < 40.0f || behaviors->ShouldUseCooldowns())
        {
            if (IsSpellReady(SHADOWFIEND) && getMSTime() - _lastShadowfiend > 180000)
            {
                if (CastSpell(target, SHADOWFIEND))
                {
                    _lastShadowfiend = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} summoned Shadowfiend",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    // Discipline/Holy: Power Infusion
    if (IsSpellReady(POWER_INFUSION) && getMSTime() - _lastPowerInfusion > 120000)
    {
        Unit* piTarget = GetBot();

        // Cast on highest DPS in group if available
        if (Group* group = GetBot()->GetGroup())
        {
            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                {
                    // Prioritize casters for Power Infusion
                    if (player->GetPowerType() == POWER_MANA && !IsHealer(player))
                    {
                        piTarget = player;
                        break;
                    }
                }
            }
        }

        if (CastSpell(piTarget, POWER_INFUSION))
        {
            _lastPowerInfusion = getMSTime();
            TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Power Infusion on {}",
                         GetBot()->GetName(), piTarget->GetName());
            return true;
        }
    }

    // Holy: Divine Hymn for raid healing
    if (_currentSpec == PriestSpec::HOLY)
    {
        uint32 criticallyInjured = 0;
        if (Group* group = GetBot()->GetGroup())
        {
            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                {
                    if (player->GetHealthPct() < 40.0f)
                        criticallyInjured++;
                }
            }
        }

        if (criticallyInjured >= 3 && IsSpellReady(DIVINE_HYMN))
        {
            if (CastSpell(DIVINE_HYMN))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} channeling Divine Hymn",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    return false;
}

bool PriestAI::HandleResourceManagement()
{
    if (!GetBot())
        return false;

    // Shadow: Insanity management
    if (_currentSpec == PriestSpec::SHADOW)
    {
        ManageInsanity();

        // Exit Void Form if insanity depleted
        if (_inVoidForm && _insanityLevel <= 0)
        {
            ExitVoidForm();
        }

        // Use Dispersion to extend Void Form
        if (_inVoidForm && _insanityLevel < 30)
        {
            if (IsSpellReady(DISPERSION) && getMSTime() - _lastDispersion > 120000)
            {
                if (CastSpell(DISPERSION))
                {
                    _lastDispersion = getMSTime();
                    _insanityLevel += 50; // Dispersion pauses insanity drain
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} used Dispersion to maintain Void Form",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    // Mana management for healers
    float manaPercent = GetManaPercent();
    if (manaPercent < 20.0f)
    {
        // Hymn of Hope for mana regeneration
        if (IsSpellReady(HYMN_OF_HOPE))
        {
            if (CastSpell(HYMN_OF_HOPE))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Priest {} channeling Hymn of Hope for mana",
                             GetBot()->GetName());
                return true;
            }
        }

        // Shadowfiend for mana (all specs can use)
        if (IsSpellReady(SHADOWFIEND) && getMSTime() - _lastShadowfiend > 180000)
        {
            if (Unit* target = GetTargetUnit())
            {
                if (CastSpell(target, SHADOWFIEND))
                {
                    _lastShadowfiend = getMSTime();
                    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} summoned Shadowfiend for mana",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

bool PriestAI::ExecuteNormalRotation(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    // Check for healing needs first (all specs can emergency heal)
    Unit* healTarget = GetLowestHealthAlly(40.0f);
    if (healTarget && healTarget->GetHealthPct() < 50.0f)
    {
        // Emergency heal takes priority
        if (_currentSpec != PriestSpec::SHADOW || healTarget->GetHealthPct() < 30.0f)
        {
            HealTarget(healTarget);
            return true;
        }
    }

    // Shadow DPS rotation
    if (_currentSpec == PriestSpec::SHADOW)
    {
        // Maintain Shadowform
        if (!HasAura(SHADOWFORM) && IsSpellReady(SHADOWFORM))
        {
            CastSpell(SHADOWFORM);
            return true;
        }

        // Void Bolt in Void Form (replaces Mind Blast)
        if (_inVoidForm && IsSpellReady(VOID_BOLT))
        {
            if (CastSpell(target, VOID_BOLT))
            {
                _insanityLevel += 15;
                return true;
            }
        }

        // Maintain DoTs
        if (!HasAllDoTs(target))
        {
            RefreshDoTs(target);
            return true;
        }

        // Shadow Word: Death for execute
        if (target->GetHealthPct() < 20.0f && IsSpellReady(SHADOW_WORD_DEATH))
        {
            if (CastSpell(target, SHADOW_WORD_DEATH))
                return true;
        }

        // Mind Blast on cooldown
        if (!_inVoidForm && IsSpellReady(MIND_BLAST))
        {
            if (CastSpell(target, MIND_BLAST))
            {
                _insanityLevel += 12;
                return true;
            }
        }

        // Mind Flay as filler
        if (IsSpellReady(MIND_FLAY))
        {
            if (CastSpell(target, MIND_FLAY))
            {
                _insanityLevel += 6;
                return true;
            }
        }
    }
    // Discipline healing/damage rotation
    else if (_currentSpec == PriestSpec::DISCIPLINE)
    {
        // Maintain atonements
        RefreshAtonements();

        // Penance on cooldown (heal or damage)
        if (IsSpellReady(PENANCE) && getMSTime() - _lastPenance > 9000)
        {
            Unit* penanceTarget = healTarget && healTarget->GetHealthPct() < 70.0f ? healTarget : target;
            if (CastSpell(penanceTarget, PENANCE))
            {
                _lastPenance = getMSTime();
                if (penanceTarget->IsFriendlyTo(GetBot()))
                    ApplyAtonement(penanceTarget);
                return true;
            }
        }

        // Power Word: Shield on low health allies
        if (healTarget && healTarget->GetHealthPct() < 60.0f)
        {
            if (!healTarget->HasAura(POWER_WORD_SHIELD) && IsSpellReady(POWER_WORD_SHIELD))
            {
                if (CastSpell(healTarget, POWER_WORD_SHIELD))
                {
                    ApplyAtonement(healTarget);
                    return true;
                }
            }
        }

        // Shadow Word: Pain for Atonement healing
        if (!target->HasAura(SHADOW_WORD_PAIN) && IsSpellReady(SHADOW_WORD_PAIN))
        {
            if (CastSpell(target, SHADOW_WORD_PAIN))
            {
                _shadowWordPainTargets[target->GetGUID()] = getMSTime();
                return true;
            }
        }

        // Mind Blast for damage/healing
        if (IsSpellReady(MIND_BLAST))
        {
            if (CastSpell(target, MIND_BLAST))
                return true;
        }

        // Flash Heal for direct healing
        if (healTarget && healTarget->GetHealthPct() < 50.0f)
        {
            if (IsSpellReady(FLASH_HEAL))
            {
                if (CastSpell(healTarget, FLASH_HEAL))
                {
                    ApplyAtonement(healTarget);
                    return true;
                }
            }
        }
    }
    // Holy healing rotation
    else if (_currentSpec == PriestSpec::HOLY)
    {
        // Prayer of Mending on cooldown
        if (IsSpellReady(PRAYER_OF_MENDING))
        {
            Unit* pomTarget = FindGroupTank();
            if (!pomTarget)
                pomTarget = healTarget;

            if (pomTarget && CastSpell(pomTarget, PRAYER_OF_MENDING))
                return true;
        }

        // Circle of Healing for group healing
        uint32 injuredCount = 0;
        if (Group* group = GetBot()->GetGroup())
        {
            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::GetPlayer(GetBot()->GetMap(), member.guid))
                {
                    if (player->GetHealthPct() < 80.0f)
                        injuredCount++;
                }
            }
        }

        if (injuredCount >= 3 && IsSpellReady(CIRCLE_OF_HEALING))
        {
            if (getMSTime() - _lastCircleOfHealing > 15000)
            {
                if (CastSpell(CIRCLE_OF_HEALING))
                {
                    _lastCircleOfHealing = getMSTime();
                    return true;
                }
            }
        }

        // Maintain Renew on tanks and injured
        if (healTarget)
        {
            if (!healTarget->HasAura(RENEW) && IsSpellReady(RENEW))
            {
                if (CastSpell(healTarget, RENEW))
                    return true;
            }
        }

        // Use Serendipity stacks for faster heals
        if (_serendipityStacks >= 2 && healTarget && healTarget->GetHealthPct() < 60.0f)
        {
            if (IsSpellReady(GREATER_HEAL))
            {
                if (CastSpell(healTarget, GREATER_HEAL))
                {
                    _serendipityStacks = 0;
                    return true;
                }
            }
        }

        // Flash Heal for quick healing
        if (healTarget && healTarget->GetHealthPct() < 70.0f)
        {
            if (IsSpellReady(FLASH_HEAL))
            {
                if (CastSpell(healTarget, FLASH_HEAL))
                {
                    _serendipityStacks++;
                    return true;
                }
            }
        }

        // Heal as efficient option
        if (healTarget && healTarget->GetHealthPct() < 85.0f)
        {
            if (IsSpellReady(HEAL))
            {
                if (CastSpell(healTarget, HEAL))
                    return true;
            }
        }

        // Offensive abilities when no healing needed
        if (!healTarget || healTarget->GetHealthPct() > 90.0f)
        {
            // Holy Fire on cooldown
            if (IsSpellReady(MIND_BLAST))
            {
                if (CastSpell(target, MIND_BLAST))
                    return true;
            }
        }
    }

    // Fallback to wand attack
    if (target && GetBot()->GetDistance(target) <= 30.0f)
    {
        GetBot()->AttackerStateUpdate(target);
    }

    return false;
}

// Shadow DoT management implementation
bool PriestAI::NeedsDoTRefresh(::Unit* target, uint32 spellId)
{
    if (!target)
        return true;

    if (!target->HasAura(spellId))
        return true;

    // Refresh DoTs at 30% duration
    if (Aura* aura = target->GetAura(spellId))
    {
        int32 duration = aura->GetDuration();
        int32 maxDuration = aura->GetMaxDuration();

        if (maxDuration > 0)
        {
            float remainingPercent = float(duration) / float(maxDuration) * 100.0f;
            return remainingPercent < 30.0f;
        }
    }

    return false;
}

void PriestAI::RefreshDoTs(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    // Shadow Word: Pain
    if (NeedsDoTRefresh(target, SHADOW_WORD_PAIN) && IsSpellReady(SHADOW_WORD_PAIN))
    {
        if (CastSpell(target, SHADOW_WORD_PAIN))
        {
            _shadowWordPainTargets[target->GetGUID()] = getMSTime();
            _dotRefreshTime = getMSTime();
        }
    }

    // Vampiric Touch
    if (NeedsDoTRefresh(target, VAMPIRIC_TOUCH) && IsSpellReady(VAMPIRIC_TOUCH))
    {
        if (CastSpell(target, VAMPIRIC_TOUCH))
        {
            _vampiricTouchTargets[target->GetGUID()] = getMSTime();
            _dotRefreshTime = getMSTime();
        }
    }

    // Devouring Plague
    if (NeedsDoTRefresh(target, DEVOURING_PLAGUE) && IsSpellReady(DEVOURING_PLAGUE))
    {
        if (CastSpell(target, DEVOURING_PLAGUE))
        {
            _dotRefreshTime = getMSTime();
        }
    }
}

bool PriestAI::HasAllDoTs(::Unit* target)
{
    if (!target)
        return false;

    return target->HasAura(SHADOW_WORD_PAIN) &&
           target->HasAura(VAMPIRIC_TOUCH) &&
           target->HasAura(DEVOURING_PLAGUE);
}

void PriestAI::ManageInsanity()
{
    if (!GetBot())
        return;

    // Insanity drains while in Void Form
    if (_inVoidForm)
    {
        // Drain 1 insanity per second (simplified)
        if (getMSTime() % 1000 == 0)
        {
            _insanityLevel = _insanityLevel > 0 ? _insanityLevel - 1 : 0;
        }
    }

    // Cap insanity at 100
    if (_insanityLevel > 100)
        _insanityLevel = 100;
}

void PriestAI::EnterVoidForm()
{
    _inVoidForm = true;
    _insanityLevel = 100;
    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} entered Void Form with {} insanity",
                 GetBot()->GetName(), _insanityLevel);
}

void PriestAI::ExitVoidForm()
{
    _inVoidForm = false;
    _insanityLevel = 0;
    TC_LOG_DEBUG("module.playerbot.ai", "Priest {} exited Void Form",
                 GetBot()->GetName());
}

bool PriestAI::ShouldUseVoidEruption()
{
    // Use Void Eruption at 100 insanity or when it's optimal
    return !_inVoidForm && _insanityLevel >= 90;
}

// Discipline Atonement management
void PriestAI::ApplyAtonement(::Unit* target)
{
    if (!target)
        return;

    _atonementTargets[target->GetGUID()] = getMSTime() + 15000; // 15 second duration
}

void PriestAI::RefreshAtonements()
{
    if (!GetBot())
        return;

    // Clean up expired atonements
    uint32 currentTime = getMSTime();
    for (auto it = _atonementTargets.begin(); it != _atonementTargets.end();)
    {
        if (it->second < currentTime)
            it = _atonementTargets.erase(it);
        else
            ++it;
    }

    // Refresh atonements on important targets
    if (Group* group = GetBot()->GetGroup())
    {
        // Priority: Tank > Low health > Others
        Unit* tank = FindGroupTank();
        if (tank && !HasAtonement(tank))
        {
            if (!tank->HasAura(POWER_WORD_SHIELD) && IsSpellReady(POWER_WORD_SHIELD))
            {
                if (CastSpell(tank, POWER_WORD_SHIELD))
                    ApplyAtonement(tank);
            }
        }
    }
}

bool PriestAI::HasAtonement(::Unit* target)
{
    if (!target)
        return false;

    auto it = _atonementTargets.find(target->GetGUID());
    if (it != _atonementTargets.end())
        {
        return it->second > getMSTime();
    }
    return false;
}

uint32 PriestAI::CountAtonementTargets()
{
    uint32 count = 0;
    uint32 currentTime = getMSTime();

    for (auto const& pair : _atonementTargets)
    {
        if (pair.second > currentTime)
            count++;
    }

    return count;
}

void PriestAI::UpdatePenanceHealing()
{
    // Penance heals through Atonement when cast on enemies
    if (_currentSpec == PriestSpec::DISCIPLINE && CountAtonementTargets() > 0)
    {
        // Healing is distributed to atonement targets
        // This is handled by the spell system itself
    }
}

} // namespace Playerbot