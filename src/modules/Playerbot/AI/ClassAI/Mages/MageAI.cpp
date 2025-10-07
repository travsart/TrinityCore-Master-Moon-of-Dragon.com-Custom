/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MageAI.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include "ArcaneSpecialization.h"
#include "FireSpecialization.h"
#include "FrostSpecialization.h"
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
#include "../CooldownManager.h"
#include "../BaselineRotationManager.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include <algorithm>
#include <chrono>
#include <mutex>

namespace Playerbot
{

// Initialize spell school mappings
const std::unordered_map<uint32, MageSchool> MageAI::_spellSchools = {
    // Arcane spells
    {ARCANE_MISSILES, MageSchool::ARCANE},
    {ARCANE_BLAST, MageSchool::ARCANE},
    {ARCANE_BARRAGE, MageSchool::ARCANE},
    {ARCANE_ORB, MageSchool::ARCANE},
    {ARCANE_POWER, MageSchool::ARCANE},
    {ARCANE_INTELLECT, MageSchool::ARCANE},
    {ARCANE_EXPLOSION, MageSchool::ARCANE},
    // Fire spells
    {FIREBALL, MageSchool::FIRE},
    {FIRE_BLAST, MageSchool::FIRE},
    {PYROBLAST, MageSchool::FIRE},
    {FLAMESTRIKE, MageSchool::FIRE},
    {SCORCH, MageSchool::FIRE},
    {COMBUSTION, MageSchool::FIRE},
    {LIVING_BOMB, MageSchool::FIRE},
    {DRAGON_BREATH, MageSchool::FIRE},
    // Frost spells
    {FROSTBOLT, MageSchool::FROST},
    {ICE_LANCE, MageSchool::FROST},
    {FROZEN_ORB, MageSchool::FROST},
    {BLIZZARD, MageSchool::FROST},
    {CONE_OF_COLD, MageSchool::FROST},
    {ICY_VEINS, MageSchool::FROST},
    {WATER_ELEMENTAL, MageSchool::FROST},
    {ICE_BARRIER, MageSchool::FROST},
    // Generic/utility spells
    {POLYMORPH, MageSchool::GENERIC},
    {FROST_NOVA, MageSchool::GENERIC},
    {COUNTERSPELL, MageSchool::GENERIC},
    {BLINK, MageSchool::GENERIC},
    {INVISIBILITY, MageSchool::GENERIC},
    {ICE_BLOCK, MageSchool::GENERIC},
    {MANA_SHIELD, MageSchool::GENERIC}
};

// Static member initialization for MageSpellCalculator
std::unordered_map<uint32, uint32> MageSpellCalculator::_baseDamageCache;
std::unordered_map<uint32, uint32> MageSpellCalculator::_manaCostCache;
std::unordered_map<uint32, uint32> MageSpellCalculator::_castTimeCache;
std::mutex MageSpellCalculator::_cacheMutex;

// Talent IDs for specialization detection
enum MageTalents
{
    // Arcane talents
    TALENT_ARCANE_POWER = 12042,
    TALENT_ARCANE_BARRAGE = 44425,
    TALENT_ARCANE_MISSILES_PROC = 79683,
    TALENT_PRESENCE_OF_MIND = 12043,
    TALENT_ARCANE_ORB = 153626,
    TALENT_NETHER_TEMPEST = 114923,

    // Fire talents
    TALENT_PYROBLAST = 11366,
    TALENT_COMBUSTION = 190319,
    TALENT_LIVING_BOMB = 44457,
    TALENT_DRAGON_BREATH = 31661,
    TALENT_IGNITE = 12846,
    TALENT_HOT_STREAK = 48108,

    // Frost talents
    TALENT_ICE_LANCE = 30455,
    TALENT_ICY_VEINS = 12472,
    TALENT_FROZEN_ORB = 84714,
    TALENT_WATER_ELEMENTAL = 31687,
    TALENT_COLD_SNAP = 11958,
    TALENT_DEEP_FREEZE = 44572
};

MageAI::MageAI(Player* bot) : ClassAI(bot),
    _currentSpec(MageSpec::FROST), // Default to Frost spec
    _manaSpent(0),
    _damageDealt(0),
    _spellsCast(0),
    _interruptedCasts(0),
    _criticalHits(0),
    _successfulPolymorphs(0),
    _successfulCounterspells(0),
    _lastPolymorph(0),
    _lastCounterspell(0),
    _lastBlink(0),
    _lastManaShield(0),
    _lastIceBarrier(0)
{
    // Initialize specialization
    InitializeSpecialization();

    // Initialize combat system components
    _threatManager = std::make_unique<BotThreatManager>(bot);
    _targetSelector = std::make_unique<TargetSelector>(bot, _threatManager.get());
    _positionManager = std::make_unique<PositionManager>(bot, _threatManager.get());
    _interruptManager = std::make_unique<InterruptManager>(bot);

    // Reset combat metrics
    _combatMetrics.Reset();

    TC_LOG_DEBUG("module.playerbot.ai", "MageAI created for player {} with specialization {}",
                 bot ? bot->GetName().c_str() : "null",
                 _specialization ? _specialization->GetSpecializationName() : "none");
}

MageAI::~MageAI() = default;

void MageAI::InitializeSpecialization()
{
    if (!GetBot())
        return;

    // Detect current specialization based on talents
    _currentSpec = DetectCurrentSpecialization();

    // Create appropriate specialization handler
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            _specialization = std::make_unique<ArcaneSpecialization>(GetBot());
            break;
        case MageSpec::FIRE:
            _specialization = std::make_unique<FireSpecialization>(GetBot());
            break;
        case MageSpec::FROST:
        default:
            _specialization = std::make_unique<FrostSpecialization>(GetBot());
            break;
    }
}

MageSpec MageAI::DetectCurrentSpecialization()
{
    if (!GetBot())
        return MageSpec::FROST;

    // Count points in each tree
    uint32 arcanePoints = 0;
    uint32 firePoints = 0;
    uint32 frostPoints = 0;

    // Check for signature talents
    if (GetBot()->HasSpell(TALENT_ARCANE_BARRAGE) || GetBot()->HasSpell(TALENT_ARCANE_POWER))
        arcanePoints += 10;

    if (GetBot()->HasSpell(TALENT_PYROBLAST) || GetBot()->HasSpell(TALENT_COMBUSTION))
        firePoints += 10;

    if (GetBot()->HasSpell(TALENT_ICY_VEINS) || GetBot()->HasSpell(TALENT_WATER_ELEMENTAL))
        frostPoints += 10;

    // Determine specialization based on point distribution
    if (arcanePoints > firePoints && arcanePoints > frostPoints)
        return MageSpec::ARCANE;
    else if (firePoints > arcanePoints && firePoints > frostPoints)
        return MageSpec::FIRE;
    else
        return MageSpec::FROST; // Default to Frost
}

void MageAI::SwitchSpecialization(MageSpec newSpec)
{
    if (_currentSpec == newSpec)
        return;

    _currentSpec = newSpec;

    // Recreate specialization handler
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            _specialization = std::make_unique<ArcaneSpecialization>(GetBot());
            break;
        case MageSpec::FIRE:
            _specialization = std::make_unique<FireSpecialization>(GetBot());
            break;
        case MageSpec::FROST:
            _specialization = std::make_unique<FrostSpecialization>(GetBot());
            break;
    }

    if (GetBot())
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} switched specialization to {}",
                     GetBot()->GetName(), _specialization ? _specialization->GetSpecializationName() : "none");
    else
        TC_LOG_DEBUG("module.playerbot.ai", "Null mage switched specialization to {}",
                     _specialization ? _specialization->GetSpecializationName() : "none");
}

void MageAI::UpdateRotation(::Unit* target)
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
            if (GetBot()->GetDistance(target) <= 35.0f)
            {
                GetBot()->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // Check if we need to switch specialization
    MageSpec newSpec = DetectCurrentSpecialization();
    if (newSpec != _currentSpec)
    {
        SwitchSpecialization(newSpec);
    }

    // Handle emergency situations first
    if (IsInCriticalDanger())
    {
        HandleEmergencySituation();
        return;
    }

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Intelligent decision making
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Priority 1: Interrupts (Counterspell)
    if (behaviors && behaviors->ShouldInterrupt(target))
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (interruptTarget && CanUseAbility(COUNTERSPELL))
        {
            if (CastSpell(interruptTarget, COUNTERSPELL))
            {
                _lastCounterspell = getMSTime();
                _successfulCounterspells++;
                RecordInterruptAttempt(COUNTERSPELL, interruptTarget, true);
                TC_LOG_DEBUG("module.playerbot.ai", "Mage {} counterspelled {}",
                             GetBot()->GetName(), interruptTarget->GetName());
                return;
            }
        }
    }

    // Priority 2: Defensives (Ice Block, Ice Barrier, etc.)
    if (behaviors && behaviors->NeedsDefensive())
    {
        // Use defensive cooldowns when health is critical
        float healthPct = GetBot()->GetHealthPct();
        if (healthPct < 20.0f)
        {
            UseIceBlock();
            if (GetBot()->HasAura(ICE_BLOCK))
                return;
        }
        else if (healthPct < 40.0f)
        {
            UseBarrierSpells();
            if (GetBot()->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    // Priority 3: Positioning (Mages need max range)
    if (behaviors && behaviors->NeedsRepositioning())
    {
        Position optimalPos = behaviors->GetOptimalPosition();
        // Movement handled by BotAI strategies
        // Just ensure we're maintaining optimal range
        if (NeedsToKite(target))
        {
            PerformKiting(target);
            if (GetBot()->isMoving())
            {
                // Use instant casts while moving
                OptimizeInstantCasts();
                return;
            }
        }
    }

    // Priority 4: Target switching for priority targets
    if (behaviors && behaviors->ShouldSwitchTarget())
    {
        Unit* priorityTarget = behaviors->GetPriorityTarget();
        if (priorityTarget && priorityTarget != target)
        {
            // Check if we should polymorph the old target
            if (CanPolymorphSafely(target))
            {
                UsePolymorph(target);
            }

            // Switch to priority target
            OnTargetChanged(priorityTarget);
            target = priorityTarget;
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} switching to priority target {}",
                         GetBot()->GetName(), priorityTarget->GetName());
        }
    }

    // Priority 5: AoE decision (Flamestrike, Blizzard, Arcane Explosion)
    if (behaviors && behaviors->ShouldAOE())
    {
        Position aoeCenter = behaviors->GetOptimalPosition();

        switch (_currentSpec)
        {
            case MageSpec::FROST:
                if (CanUseAbility(BLIZZARD))
                {
                    // Cast Blizzard at optimal position
                    // Note: Ground-targeted spells need special handling
                    TC_LOG_DEBUG("module.playerbot.ai", "Mage {} casting Blizzard for AoE",
                                 GetBot()->GetName());
                    return;
                }
                break;

            case MageSpec::FIRE:
                if (CanUseAbility(FLAMESTRIKE))
                {
                    // Cast Flamestrike at optimal position
                    TC_LOG_DEBUG("module.playerbot.ai", "Mage {} casting Flamestrike for AoE",
                                 GetBot()->GetName());
                    return;
                }

                // Dragon's Breath for close-range AoE
                if (GetBot()->GetDistance(target) < 12.0f && CanUseAbility(DRAGON_BREATH))
                {
                    if (CastSpell(DRAGON_BREATH))
                    {
                        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} using Dragon's Breath",
                                     GetBot()->GetName());
                        return;
                    }
                }
                break;

            case MageSpec::ARCANE:
                // Arcane Explosion for melee range AoE
                if (GetNearbyEnemyCount(10.0f) >= 3 && CanUseAbility(ARCANE_EXPLOSION))
                {
                    if (CastSpell(ARCANE_EXPLOSION))
                    {
                        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} using Arcane Explosion",
                                     GetBot()->GetName());
                        return;
                    }
                }
                break;
        }
    }

    // Priority 6: Cooldown stacking (Combustion, Arcane Power, Icy Veins)
    if (behaviors && behaviors->ShouldUseCooldowns())
    {
        switch (_currentSpec)
        {
            case MageSpec::FIRE:
                if (CanUseAbility(COMBUSTION))
                {
                    if (CastSpell(COMBUSTION))
                    {
                        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Combustion",
                                     GetBot()->GetName());
                    }
                }
                break;

            case MageSpec::ARCANE:
                if (CanUseAbility(ARCANE_POWER))
                {
                    if (CastSpell(ARCANE_POWER))
                    {
                        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Arcane Power",
                                     GetBot()->GetName());
                    }
                }
                break;

            case MageSpec::FROST:
                if (CanUseAbility(ICY_VEINS))
                {
                    if (CastSpell(ICY_VEINS))
                    {
                        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Icy Veins",
                                     GetBot()->GetName());
                    }
                }

                // Summon Water Elemental
                if (CanUseAbility(WATER_ELEMENTAL))
                {
                    if (CastSpell(WATER_ELEMENTAL))
                    {
                        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} summoned Water Elemental",
                                     GetBot()->GetName());
                    }
                }
                break;
        }

        // Universal cooldowns
        if (CanUseAbility(MIRROR_IMAGE))
        {
            if (CastSpell(MIRROR_IMAGE))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Mage {} summoned Mirror Images",
                             GetBot()->GetName());
            }
        }
    }

    // Priority 7: Crowd control for secondary targets
    if (behaviors && !behaviors->ShouldAOE()) // Only CC if not AoEing
    {
        ::Unit* polymorphTarget = GetBestPolymorphTarget();
        if (polymorphTarget && polymorphTarget != target && CanPolymorphSafely(polymorphTarget))
        {
            UsePolymorph(polymorphTarget);
            if (_polymorphTargets.find(polymorphTarget->GetGUID()) != _polymorphTargets.end())
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Mage {} polymorphed secondary target",
                             GetBot()->GetName());
                // Continue with main target after polymorph
            }
        }
    }

    // Update threat management
    ManageThreat();

    // Priority 8: Normal rotation - delegate to specialization
    if (_specialization && target)
    {
        _specialization->UpdateRotation(target);
    }
    else if (target)
    {
        // Fallback basic rotation
        ExecuteAdvancedRotation(target);
    }

    // Update combat metrics
    if (GetBot()->IsInCombat())
    {
        UpdatePerformanceMetrics(100);
        AnalyzeCastingEffectiveness();
    }
}

void MageAI::UpdateBuffs()
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

    UpdateMageBuffs();

    // Delegate to specialization for specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

void MageAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update shared cooldown trackers
    if (_cooldownManager)
    {
        _cooldownManager->Update(diff);
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }

    // Update performance metrics
    UpdatePerformanceMetrics(diff);
}

bool MageAI::CanUseAbility(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Check if spell is ready
    if (!IsSpellReady(spellId))
        return false;

    // Check resource requirements
    if (!HasEnoughResource(spellId))
        return false;

    // Delegate to specialization for specific checks
    if (_specialization)
        return _specialization->CanUseAbility(spellId);

    return true;
}

void MageAI::OnCombatStart(::Unit* target)
{
    if (!GetBot())
        return;

    // Reset combat metrics
    _combatMetrics.Reset();

    // Apply combat buffs
    UpdateArmorSpells();
    CastManaShield();

    // Notify specialization
    if (_specialization)
        _specialization->OnCombatStart(target);

    // Initialize positioning - move to optimal range
    if (_positionManager && target)
    {
        Position optimalPos = _positionManager->FindRangedPosition(target, OPTIMAL_CASTING_RANGE);
        GetBot()->GetMotionMaster()->MovePoint(0, optimalPos);
    }

    if (target)
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} entering combat with target {}", GetBot()->GetName(), target->GetName());
    else
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} entering combat with null target", GetBot()->GetName());
}

void MageAI::OnCombatEnd()
{
    if (!GetBot())
        return;

    // Notify specialization
    if (_specialization)
        _specialization->OnCombatEnd();

    // Perform post-combat actions
    UseManaRegeneration();

    // Log combat performance
    TC_LOG_DEBUG("module.playerbot.ai",
                 "Mage {} combat ended - Damage: {}, Mana spent: {}, Spells cast: {}, Crits: {}",
                 GetBot()->GetName(),
                 uint32(_combatMetrics.totalDamage.load()),
                 uint32(_combatMetrics.totalManaSpent.load()),
                 uint32(_spellsCast.load()),
                 uint32(_criticalHits.load()));
}

bool MageAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Calculate mana cost with modifiers
    uint32 manaCost = MageSpellCalculator::CalculateSpellManaCost(spellId, GetBot());

    // Check if we have enough mana
    return HasEnoughMana(manaCost);
}

void MageAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    // Calculate and consume mana
    uint32 manaCost = MageSpellCalculator::CalculateSpellManaCost(spellId, GetBot());
    _manaSpent += manaCost;
    _combatMetrics.totalManaSpent += manaCost;

    // Track spell cast
    _spellsCast++;
    RecordSpellCast(spellId, _currentTarget);
}

Position MageAI::GetOptimalPosition(::Unit* target)
{
    if (!GetBot() || !target)
        return GetBot() ? GetBot()->GetPosition() : Position();

    // Delegate to position manager
    if (_positionManager)
        return _positionManager->FindRangedPosition(target, OPTIMAL_CASTING_RANGE);

    // Fallback: maintain max range
    float angle = GetBot()->GetAbsoluteAngle(target->GetPositionX(), target->GetPositionY());
    float distance = OPTIMAL_CASTING_RANGE - 2.0f; // Small buffer

    Position pos;
    pos.m_positionX = target->GetPositionX() - distance * cos(angle);
    pos.m_positionY = target->GetPositionY() - distance * sin(angle);
    pos.m_positionZ = target->GetPositionZ();
    pos.SetOrientation(target->GetOrientation());

    return pos;
}

float MageAI::GetOptimalRange(::Unit* target)
{
    if (!target)
        return OPTIMAL_CASTING_RANGE;

    // Adjust range based on target type and situation
    if (NeedsToKite(target))
        return KITING_RANGE;

    return OPTIMAL_CASTING_RANGE;
}

// Mana management methods
bool MageAI::HasEnoughMana(uint32 amount)
{
    return GetBot() && GetBot()->GetPower(POWER_MANA) >= amount;
}

uint32 MageAI::GetMana()
{
    return GetBot() ? GetBot()->GetPower(POWER_MANA) : 0;
}

uint32 MageAI::GetMaxMana()
{
    return GetBot() ? GetBot()->GetMaxPower(POWER_MANA) : 0;
}

float MageAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    return maxMana > 0 ? (static_cast<float>(GetMana()) / maxMana) * 100.0f : 0.0f;
}

void MageAI::OptimizeManaUsage()
{
    if (!GetBot())
        return;

    float manaPercent = GetManaPercent();

    // Adjust casting priorities based on mana
    if (manaPercent < MANA_EMERGENCY_THRESHOLD * 100.0f)
    {
        HandleLowManaEmergency();
    }
    else if (manaPercent < MANA_CONSERVATION_THRESHOLD * 100.0f)
    {
        // Use more efficient spells
        TC_LOG_DEBUG("module.playerbot.ai",
                     "Mage {} conserving mana at {:.1f}%",
                     GetBot()->GetName(), manaPercent);
    }
}

bool MageAI::ShouldConserveMana()
{
    return GetManaPercent() < (MANA_CONSERVATION_THRESHOLD * 100.0f);
}

void MageAI::UseManaRegeneration()
{
    if (!GetBot() || GetBot()->IsInCombat())
        return;

    // Use Evocation if available
    if (IsSpellReady(12051) && GetManaPercent() < 50.0f) // Evocation spell ID
    {
        CastSpell(12051);
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} using Evocation for mana regeneration",
                     GetBot()->GetName());
    }

    // Use mana gems if available
    if (GetManaPercent() < 70.0f)
    {
        // Check for mana gems in inventory and use them
        // This would require inventory management implementation
    }
}

// Buff management methods
void MageAI::UpdateMageBuffs()
{
    if (!GetBot())
        return;

    // Maintain Arcane Intellect
    CastArcaneIntellect();

    // Update armor spells
    UpdateArmorSpells();

    // Check for missing buffs on allies
    if (Group* group = GetBot()->GetGroup())
    {
        for (auto const& slot : group->GetMemberSlots())
        {
            if (Player* member = ObjectAccessor::FindPlayer(slot.guid))
            {
                if (!member->HasAura(ARCANE_INTELLECT) &&
                    GetBot()->GetDistance2d(member) < 40.0f)
                {
                    CastSpell(member, ARCANE_INTELLECT);
                }
            }
        }
    }
}

void MageAI::CastMageArmor()
{
    if (!GetBot() || GetBot()->HasAura(MAGE_ARMOR))
        return;

    CastSpell(MAGE_ARMOR);
}

void MageAI::CastManaShield()
{
    if (!GetBot())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastManaShield < 10000) // 10 second cooldown check
        return;

    if (GetBot()->GetHealthPct() < 70.0f && !GetBot()->HasAura(MANA_SHIELD))
    {
        if (CastSpell(MANA_SHIELD))
        {
            _lastManaShield = currentTime;
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Mana Shield",
                         GetBot()->GetName());
        }
    }
}

void MageAI::CastIceBarrier()
{
    if (!GetBot() || _currentSpec != MageSpec::FROST)
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastIceBarrier < 30000) // 30 second cooldown
        return;

    if (!GetBot()->HasAura(ICE_BARRIER) && IsSpellReady(ICE_BARRIER))
    {
        if (CastSpell(ICE_BARRIER))
        {
            _lastIceBarrier = currentTime;
        }
    }
}

void MageAI::CastArcaneIntellect()
{
    if (!GetBot() || GetBot()->HasAura(ARCANE_INTELLECT))
        return;

    CastSpell(ARCANE_INTELLECT);
}

void MageAI::UpdateArmorSpells()
{
    if (!GetBot())
        return;

    // Choose armor based on specialization
    uint32 armorSpell = 0;

    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            armorSpell = MAGE_ARMOR;
            break;
        case MageSpec::FIRE:
            armorSpell = MOLTEN_ARMOR;
            break;
        case MageSpec::FROST:
            armorSpell = FROST_ARMOR;
            break;
    }

    // Cast appropriate armor if not active
    if (armorSpell && !GetBot()->HasAura(armorSpell))
    {
        CastSpell(armorSpell);
    }
}

// Defensive abilities
void MageAI::UseDefensiveAbilities()
{
    if (!GetBot())
        return;

    float healthPct = GetBot()->GetHealthPct();

    // Ice Block at critical health
    if (healthPct < 20.0f)
    {
        UseIceBlock();
    }
    // Invisibility to drop aggro
    else if (healthPct < 40.0f && HasTooMuchThreat())
    {
        UseInvisibility();
    }
    // Blink to create distance
    else if (healthPct < 60.0f && GetNearestEnemy(8.0f))
    {
        UseBlink();
    }

    // Use barrier spells
    UseBarrierSpells();
}

void MageAI::UseBlink()
{
    if (!GetBot() || !IsSpellReady(BLINK))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastBlink < BLINK_COOLDOWN)
        return;

    // Blink away from danger
    if (CastSpell(BLINK))
    {
        _lastBlink = currentTime;
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} used Blink to escape",
                     GetBot()->GetName());
    }
}

void MageAI::UseInvisibility()
{
    if (!GetBot() || !IsSpellReady(INVISIBILITY))
        return;

    if (CastSpell(INVISIBILITY))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} used Invisibility",
                     GetBot()->GetName());
    }
}

void MageAI::UseIceBlock()
{
    if (!GetBot() || !IsSpellReady(ICE_BLOCK))
        return;

    if (GetBot()->HasAura(ICE_BLOCK))
        return; // Already in Ice Block

    if (CastSpell(ICE_BLOCK))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Ice Block!",
                     GetBot()->GetName());
    }
}

void MageAI::UseColdSnap()
{
    if (!GetBot() || _currentSpec != MageSpec::FROST || !IsSpellReady(COLD_SNAP))
        return;

    // Cold Snap resets cooldowns
    if (CastSpell(COLD_SNAP))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} used Cold Snap",
                     GetBot()->GetName());
    }
}

void MageAI::UseBarrierSpells()
{
    CastManaShield();

    if (_currentSpec == MageSpec::FROST)
    {
        CastIceBarrier();
    }
}

// Offensive cooldowns
void MageAI::UseOffensiveCooldowns()
{
    if (!GetBot() || !GetBot()->IsInCombat())
        return;

    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            UseArcanePower();
            break;
        case MageSpec::FIRE:
            UseCombustion();
            break;
        case MageSpec::FROST:
            UseIcyVeins();
            break;
    }

    UsePresenceOfMind();
    UseMirrorImage();
}

void MageAI::UseArcanePower()
{
    if (_currentSpec != MageSpec::ARCANE || !IsSpellReady(ARCANE_POWER))
        return;

    if (CastSpell(ARCANE_POWER))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Arcane Power",
                     GetBot()->GetName());
    }
}

void MageAI::UseCombustion()
{
    if (_currentSpec != MageSpec::FIRE || !IsSpellReady(COMBUSTION))
        return;

    if (CastSpell(COMBUSTION))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Combustion",
                     GetBot()->GetName());
    }
}

void MageAI::UseIcyVeins()
{
    if (_currentSpec != MageSpec::FROST || !IsSpellReady(ICY_VEINS))
        return;

    if (CastSpell(ICY_VEINS))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Icy Veins",
                     GetBot()->GetName());
    }
}

void MageAI::UsePresenceOfMind()
{
    if (!IsSpellReady(PRESENCE_OF_MIND))
        return;

    if (CastSpell(PRESENCE_OF_MIND))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} activated Presence of Mind",
                     GetBot()->GetName());
    }
}

void MageAI::UseMirrorImage()
{
    if (!IsSpellReady(MIRROR_IMAGE))
        return;

    if (CastSpell(MIRROR_IMAGE))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} summoned Mirror Images",
                     GetBot()->GetName());
    }
}

// Crowd control abilities
void MageAI::UseCrowdControl(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Prioritize polymorph for humanoids/beasts
    if (target->GetTypeId() == TYPEID_UNIT && CanPolymorphSafely(target))
    {
        UsePolymorph(target);
    }
    // Use Frost Nova for melee enemies
    else if (GetBot()->GetDistance2d(target) < 10.0f)
    {
        UseFrostNova();
    }
    // Counterspell casters
    else if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        UseCounterspell(target);
    }
}

void MageAI::UsePolymorph(::Unit* target)
{
    if (!target || !IsSpellReady(POLYMORPH))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastPolymorph < POLYMORPH_COOLDOWN)
        return;

    // Check if target is already polymorphed
    if (_polymorphTargets.find(target->GetGUID()) != _polymorphTargets.end())
    {
        if (currentTime - _polymorphTargets[target->GetGUID()] < 8000) // 8 second duration
            return;
    }

    if (CastSpell(target, POLYMORPH))
    {
        _lastPolymorph = currentTime;
        _polymorphTargets[target->GetGUID()] = currentTime;
        _successfulPolymorphs++;

        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} polymorphed target {}",
                     GetBot()->GetName(), target->GetName());
    }
}

void MageAI::UseFrostNova()
{
    if (!GetBot() || !IsSpellReady(FROST_NOVA))
        return;

    // Check for nearby enemies
    if (GetNearestEnemy(10.0f))
    {
        if (CastSpell(FROST_NOVA))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} cast Frost Nova",
                         GetBot()->GetName());
        }
    }
}

void MageAI::UseCounterspell(::Unit* target)
{
    if (!target || !IsSpellReady(COUNTERSPELL))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastCounterspell < COUNTERSPELL_COOLDOWN)
        return;

    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return;

    if (CastSpell(target, COUNTERSPELL))
    {
        _lastCounterspell = currentTime;
        _successfulCounterspells++;

        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} counterspelled {}",
                     GetBot()->GetName(), target->GetName());
    }
}

void MageAI::UseBanish(::Unit* target)
{
    if (!target || !IsSpellReady(BANISH))
        return;

    // Banish is typically a Warlock spell, but implement if mages have it
    if (CastSpell(target, BANISH))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} banished target {}",
                     GetBot()->GetName(), target->GetName());
    }
}

// AoE abilities
void MageAI::UseAoEAbilities(const std::vector<::Unit*>& enemies)
{
    if (!GetBot() || enemies.size() < 3)
        return;

    // Choose AoE based on specialization
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            UseArcaneExplosion(enemies);
            break;
        case MageSpec::FIRE:
            UseFlamestrike(enemies);
            break;
        case MageSpec::FROST:
            UseBlizzard(enemies);
            UseConeOfCold(enemies);
            break;
    }
}

void MageAI::UseBlizzard(const std::vector<::Unit*>& enemies)
{
    if (!IsSpellReady(BLIZZARD) || enemies.empty())
        return;

    // Find center point of enemies
    Position centerPos;
    float avgX = 0, avgY = 0, avgZ = 0;
    for (auto enemy : enemies)
    {
        avgX += enemy->GetPositionX();
        avgY += enemy->GetPositionY();
        avgZ += enemy->GetPositionZ();
    }

    centerPos.m_positionX = avgX / enemies.size();
    centerPos.m_positionY = avgY / enemies.size();
    centerPos.m_positionZ = avgZ / enemies.size();

    // Cast Blizzard at center (would need ground-targeted spell implementation)
    TC_LOG_DEBUG("module.playerbot.ai", "Mage {} casting Blizzard on {} enemies",
                 GetBot()->GetName(), uint32(enemies.size()));
}

void MageAI::UseFlamestrike(const std::vector<::Unit*>& enemies)
{
    if (!IsSpellReady(FLAMESTRIKE) || enemies.empty())
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Mage {} casting Flamestrike on {} enemies",
                 GetBot()->GetName(), uint32(enemies.size()));
}

void MageAI::UseArcaneExplosion(const std::vector<::Unit*>& enemies)
{
    if (!IsSpellReady(ARCANE_EXPLOSION))
        return;

    // Check if enemies are close enough
    uint32 nearbyCount = 0;
    for (auto enemy : enemies)
    {
        if (GetBot()->GetDistance2d(enemy) < 10.0f)
            nearbyCount++;
    }

    if (nearbyCount >= 2)
    {
        if (CastSpell(ARCANE_EXPLOSION))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} cast Arcane Explosion hitting {} enemies",
                         GetBot()->GetName(), uint32(nearbyCount));
        }
    }
}

void MageAI::UseConeOfCold(const std::vector<::Unit*>& enemies)
{
    if (!IsSpellReady(CONE_OF_COLD))
        return;

    // Check for enemies in front
    uint32 frontalCount = 0;
    for (auto enemy : enemies)
    {
        if (GetBot()->GetDistance2d(enemy) < 10.0f && GetBot()->HasInArc(M_PI/2, enemy))
            frontalCount++;
    }

    if (frontalCount >= 2)
    {
        if (CastSpell(CONE_OF_COLD))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} cast Cone of Cold hitting {} enemies",
                         GetBot()->GetName(), uint32(frontalCount));
        }
    }
}

// Positioning and movement
void MageAI::UpdateMagePositioning()
{
    if (!GetBot() || !_currentTarget)
        return;

    // Check if we need to reposition
    if (NeedsToKite(_currentTarget))
    {
        PerformKiting(_currentTarget);
    }
    else if (IsInDanger())
    {
        FindSafeCastingPosition();
    }
    else if (!IsAtOptimalRange(_currentTarget))
    {
        MoveToTarget(_currentTarget, GetOptimalRange(_currentTarget));
    }
}

bool MageAI::IsAtOptimalRange(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    float distance = GetBot()->GetDistance2d(target);
    float optimalRange = GetOptimalRange(target);

    return distance >= (optimalRange - 5.0f) && distance <= optimalRange;
}

bool MageAI::NeedsToKite(::Unit* target)
{
    if (!GetBot() || !target)
        return false;

    // Kite if melee enemy is too close
    if (target->GetDistance2d(GetBot()) < MINIMUM_SAFE_RANGE)
    {
        return target->CanFreeMove() && !target->HasUnitState(UNIT_STATE_ROOT);
    }

    return false;
}

void MageAI::PerformKiting(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    // Use Frost Nova to root
    if (target->GetDistance2d(GetBot()) < 10.0f)
    {
        UseFrostNova();
    }

    // Blink away if needed
    if (target->GetDistance2d(GetBot()) < 8.0f)
    {
        UseBlink();
    }

    // Move to kiting range
    Position kitingPos = GetOptimalPosition(target);
    GetBot()->GetMotionMaster()->MovePoint(0, kitingPos);
}

bool MageAI::IsInDanger()
{
    if (!GetBot())
        return false;

    // Check health
    if (GetBot()->GetHealthPct() < 40.0f)
        return true;

    // Check for multiple enemies
    std::list<::Unit*> enemies;
    Trinity::AnyUnitInObjectRangeCheck check(GetBot(), 15.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(GetBot(), enemies, check);
    Cell::VisitAllObjects(GetBot(), searcher, 15.0f);
    if (enemies.size() > 2)
        return true;

    // Check threat level
    return HasTooMuchThreat();
}

void MageAI::FindSafeCastingPosition()
{
    if (!GetBot() || !_positionManager)
        return;

    Position safePos = _positionManager->FindSafePosition(GetBot()->GetPosition(), OPTIMAL_CASTING_RANGE);
    GetBot()->GetMotionMaster()->MovePoint(0, safePos);
}

// Targeting and priorities
::Unit* MageAI::GetBestPolymorphTarget()
{
    if (!GetBot())
        return nullptr;

    std::list<::Unit*> enemies;
    Trinity::AnyUnitInObjectRangeCheck check(GetBot(), 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(GetBot(), enemies, check);
    Cell::VisitAllObjects(GetBot(), searcher, 30.0f);

    ::Unit* bestTarget = nullptr;
    float highestPriority = 0.0f;

    for (auto enemy : enemies)
    {
        // Skip current target
        if (enemy == _currentTarget)
            continue;

        // Check if can be polymorphed
        if (!CanPolymorphSafely(enemy))
            continue;

        // Calculate priority (healers and casters first)
        float priority = 1.0f;
        if (enemy->GetPowerType() == POWER_MANA)
            priority += 2.0f;
        if (enemy->HasUnitState(UNIT_STATE_CASTING))
            priority += 3.0f;

        if (priority > highestPriority)
        {
            highestPriority = priority;
            bestTarget = enemy;
        }
    }

    return bestTarget;
}

::Unit* MageAI::GetBestCounterspellTarget()
{
    if (!GetBot())
        return nullptr;

    if (_interruptManager)
    {
        auto targets = _interruptManager->ScanForInterruptTargets();
        if (!targets.empty())
            return targets[0].unit; // Return the first (highest priority) target
    }

    // Fallback
    std::list<::Unit*> enemies;
    Trinity::AnyUnitInObjectRangeCheck check(GetBot(), 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(GetBot(), enemies, check);
    Cell::VisitAllObjects(GetBot(), searcher, 30.0f);

    for (auto enemy : enemies)
    {
        if (enemy->HasUnitState(UNIT_STATE_CASTING))
            return enemy;
    }

    return nullptr;
}

::Unit* MageAI::GetBestAoETarget()
{
    if (!GetBot())
        return nullptr;

    std::list<::Unit*> enemies;
    Trinity::AnyUnitInObjectRangeCheck check(GetBot(), 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(GetBot(), enemies, check);
    Cell::VisitAllObjects(GetBot(), searcher, 30.0f);

    // Find target with most enemies nearby
    ::Unit* bestTarget = nullptr;
    uint32 maxNearby = 0;

    for (auto enemy : enemies)
    {
        uint32 nearbyCount = 0;
        for (auto other : enemies)
        {
            if (enemy->GetDistance2d(other) < 10.0f)
                nearbyCount++;
        }

        if (nearbyCount > maxNearby)
        {
            maxNearby = nearbyCount;
            bestTarget = enemy;
        }
    }

    return bestTarget;
}

bool MageAI::ShouldInterrupt(::Unit* target)
{
    if (!target || !target->HasUnitState(UNIT_STATE_CASTING))
        return false;

    // Always interrupt healing spells
    if (target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        SpellInfo const* spellInfo = target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo();
        if (spellInfo && spellInfo->HasEffect(SPELL_EFFECT_HEAL))
            return true;
    }

    return false; // Default to false - local interrupt logic already handles most cases
}

bool MageAI::CanPolymorphSafely(::Unit* target)
{
    if (!target)
        return false;

    // Check if target is valid for polymorph
    if (target->GetTypeId() != TYPEID_UNIT)
        return false;

    Creature* creature = target->ToCreature();
    if (!creature)
        return false;

    // Check creature type (humanoid or beast)
    uint32 creatureType = creature->GetCreatureTemplate()->type;
    if (creatureType != CREATURE_TYPE_HUMANOID && creatureType != CREATURE_TYPE_BEAST)
        return false;

    // Check if already controlled
    if (target->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
        target->HasAuraType(SPELL_AURA_MOD_CHARM) ||
        target->HasAuraType(SPELL_AURA_MOD_STUN))
        return false;

    // Check if immune
    if (target->HasAuraType(SPELL_AURA_MECHANIC_IMMUNITY))
        return false;

    return true;
}

// Delegation to specialization
void MageAI::DelegateToSpecialization(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Basic fallback rotation if specialization is not available
    ExecuteAdvancedRotation(target);
}

// Advanced spell effectiveness tracking
void MageAI::RecordSpellCast(uint32 spellId, ::Unit* target)
{
    // Track spell casting metrics
    _spellsCast++;
}

void MageAI::RecordSpellHit(uint32 spellId, ::Unit* target, uint32 damage)
{
    _damageDealt += damage;
    _combatMetrics.totalDamage += damage;
}

void MageAI::RecordSpellCrit(uint32 spellId, ::Unit* target, uint32 damage)
{
    _criticalHits++;
    RecordSpellHit(spellId, target, damage);

    float currentRate = _combatMetrics.criticalHitRate.load();
    _combatMetrics.criticalHitRate = (currentRate * (_spellsCast - 1) + 1.0f) / _spellsCast;
}

void MageAI::RecordSpellResist(uint32 spellId, ::Unit* target)
{
    // Track spell resistance for adaptation
    if (target)
        TC_LOG_DEBUG("module.playerbot.ai", "Spell {} resisted by {}", spellId, target->GetName());
    else
        TC_LOG_DEBUG("module.playerbot.ai", "Spell {} resisted by null target", spellId);
}

void MageAI::RecordInterruptAttempt(uint32 spellId, ::Unit* target, bool success)
{
    if (success)
    {
        _successfulCounterspells++;

        float currentRate = _combatMetrics.interruptSuccessRate.load();
        float totalAttempts = _successfulCounterspells + _interruptedCasts;
        _combatMetrics.interruptSuccessRate = _successfulCounterspells / totalAttempts;
    }
    else
    {
        _interruptedCasts++;
    }
}

void MageAI::AnalyzeCastingEffectiveness()
{
    if (_spellsCast == 0)
        return;

    float critRate = static_cast<float>(_criticalHits) / _spellsCast * 100.0f;
    float manaEfficiency = _damageDealt.load() / std::max(1u, _manaSpent.load());

    if (GetBot())
        TC_LOG_DEBUG("module.playerbot.ai",
                     "Mage {} effectiveness - Crit: {:.1f}%, Mana efficiency: {:.2f} damage/mana",
                     GetBot()->GetName(), critRate, manaEfficiency);
    else
        TC_LOG_DEBUG("module.playerbot.ai",
                     "Null mage effectiveness - Crit: {:.1f}%, Mana efficiency: {:.2f} damage/mana",
                     critRate, manaEfficiency);
}

float MageAI::CalculateSpellEfficiency(uint32 spellId)
{
    if (!GetBot())
        return 0.0f;

    uint32 manaCost = MageSpellCalculator::CalculateSpellManaCost(spellId, GetBot());
    if (manaCost == 0)
        return 100.0f; // Free spells are maximally efficient

    // Calculate expected damage
    uint32 expectedDamage = 0;
    switch (spellId)
    {
        case FIREBALL:
            expectedDamage = MageSpellCalculator::CalculateFireballDamage(GetBot(), _currentTarget);
            break;
        case FROSTBOLT:
            expectedDamage = MageSpellCalculator::CalculateFrostboltDamage(GetBot(), _currentTarget);
            break;
        case ARCANE_MISSILES:
            expectedDamage = MageSpellCalculator::CalculateArcaneMissilesDamage(GetBot(), _currentTarget);
            break;
        default:
            expectedDamage = 100; // Default value
            break;
    }

    return static_cast<float>(expectedDamage) / manaCost;
}

void MageAI::OptimizeSpellPriorities()
{
    // Analyze and adjust spell priorities based on effectiveness
    if (_currentTarget)
    {
        AdaptToTargetResistances(_currentTarget);
    }
}

// Helper methods
bool MageAI::IsChanneling()
{
    return GetBot() && GetBot()->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
}

bool MageAI::IsCasting()
{
    return GetBot() &&
           (GetBot()->GetCurrentSpell(CURRENT_GENERIC_SPELL) ||
            GetBot()->GetCurrentSpell(CURRENT_CHANNELED_SPELL));
}

bool MageAI::CanCastSpell()
{
    return GetBot() && !IsCasting() && !IsChanneling() &&
           !GetBot()->HasUnitState(UNIT_STATE_STUNNED) &&
           !GetBot()->HasUnitState(UNIT_STATE_CONFUSED);
}

MageSchool MageAI::GetSpellSchool(uint32 spellId)
{
    auto it = _spellSchools.find(spellId);
    if (it != _spellSchools.end())
        return it->second;

    return MageSchool::GENERIC;
}

uint32 MageAI::GetSpellCastTime(uint32 spellId)
{
    return MageSpellCalculator::CalculateCastTime(spellId, GetBot());
}

bool MageAI::IsSpellInstant(uint32 spellId)
{
    return GetSpellCastTime(spellId) == 0;
}

// Specialization detection and optimization
MageSpec MageAI::DetectSpecialization()
{
    return DetectCurrentSpecialization();
}

void MageAI::OptimizeForSpecialization()
{
    // Adjust behavior based on specialization
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            // Prioritize mana management and burst
            break;
        case MageSpec::FIRE:
            // Prioritize DoTs and critical strikes
            break;
        case MageSpec::FROST:
            // Prioritize control and survivability
            break;
    }
}

bool MageAI::HasTalent(uint32 talentId)
{
    return GetBot() && GetBot()->HasSpell(talentId);
}

// Threat and aggro management
void MageAI::ManageThreat()
{
    if (!GetBot() || !_threatManager)
        return;

    if (HasTooMuchThreat())
    {
        ReduceThreat();
    }
}

bool MageAI::HasTooMuchThreat()
{
    // Simple threat check based on target's threat list
    ::Unit* target = GetBot()->GetSelectedUnit();
    if (!target)
        return false;

    // Check if we're high on threat list (simplified approach)
    ThreatManager& threatMgr = target->GetThreatManager();
    float myThreat = threatMgr.GetThreat(GetBot());
    Unit* victim = target->GetVictim();

    if (victim && victim == GetBot())
        return true; // We have aggro

    return false;
}

void MageAI::ReduceThreat()
{
    // Use threat reduction abilities
    UseInvisibility();

    // Consider using Ice Block in extreme cases
    if (HasTooMuchThreat())
    {
        UseIceBlock();
    }
}

// Advanced emergency responses
void MageAI::HandleEmergencySituation()
{
    if (!GetBot())
        return;

    float healthPct = GetBot()->GetHealthPct();

    // Critical health - use Ice Block
    if (healthPct < 20.0f)
    {
        UseIceBlock();
        return;
    }

    // Low health - defensive measures
    if (healthPct < 40.0f)
    {
        UseDefensiveAbilities();
    }

    // Multiple enemies
    std::list<::Unit*> enemies;
    Trinity::AnyUnitInObjectRangeCheck check(GetBot(), 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(GetBot(), enemies, check);
    Cell::VisitAllObjects(GetBot(), searcher, 30.0f);
    if (enemies.size() > 3)
    {
        std::vector<::Unit*> enemyVec(enemies.begin(), enemies.end());
        HandleMultipleEnemies(enemyVec);
    }

    // Low mana
    if (GetManaPercent() < 15.0f)
    {
        HandleLowManaEmergency();
    }
}

bool MageAI::IsInCriticalDanger()
{
    if (!GetBot())
        return false;

    return GetBot()->GetHealthPct() < 25.0f ||
           (GetBot()->GetHealthPct() < 40.0f && GetNearestEnemy(10.0f) != nullptr);
}

void MageAI::UseEmergencyEscape()
{
    UseBlink();
    UseInvisibility();

    // If still in danger, use Ice Block
    if (IsInCriticalDanger())
    {
        UseIceBlock();
    }
}

void MageAI::HandleMultipleEnemies(const std::vector<::Unit*>& enemies)
{
    // Use AoE crowd control
    UseFrostNova();

    // Use AoE damage
    UseAoEAbilities(enemies);

    // Create distance
    if (GetNearestEnemy(10.0f))
    {
        UseBlink();
    }
}

void MageAI::HandleLowManaEmergency()
{
    // Use mana gems if available
    // This would require inventory management

    // Use Evocation if safe
    if (!GetNearestEnemy(20.0f) && IsSpellReady(12051))
    {
        CastSpell(12051); // Evocation
    }

    // Switch to wand attacks
    // This would require weapon management
}

void MageAI::HandleHighThreatSituation()
{
    ReduceThreat();

    // Move away from threat source
    if (_currentTarget)
    {
        PerformKiting(_currentTarget);
    }
}

void MageAI::ExecuteEmergencyTeleport()
{
    // Teleport to safety if possible
    // This would require location management
    if (IsSpellReady(TELEPORT_STORMWIND))
    {
        CastSpell(TELEPORT_STORMWIND);
        TC_LOG_DEBUG("module.playerbot.ai", "Mage {} emergency teleporting!",
                     GetBot()->GetName());
    }
}

// Advanced combat AI
void MageAI::UpdateAdvancedCombatLogic(::Unit* target)
{
    if (!target)
        return;

    // Optimize casting sequence
    OptimizeCastingSequence(target);

    // Manage resources efficiently
    ManageResourceEfficiency();

    // Handle phase transitions
    HandleCombatPhaseTransitions();
}

void MageAI::OptimizeCastingSequence(::Unit* target)
{
    if (!target)
        return;

    // Determine most effective spell school
    MageSchool bestSchool = GetMostEffectiveSchool(target);

    // Adjust rotation based on effectiveness
    switch (bestSchool)
    {
        case MageSchool::ARCANE:
            // Use arcane spells
            break;
        case MageSchool::FIRE:
            // Use fire spells
            break;
        case MageSchool::FROST:
            // Use frost spells
            break;
        default:
            break;
    }
}

void MageAI::ManageResourceEfficiency()
{
    OptimizeManaUsage();

    // Track efficiency metrics
    uint32 manaSpent = _manaSpent.load();
    if (manaSpent > 0)
    {
        float efficiency = static_cast<float>(_damageDealt.load()) / static_cast<float>(manaSpent);
        if (GetBot())
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} resource efficiency: {:.2f}", GetBot()->GetName(), efficiency);
        else
            TC_LOG_DEBUG("module.playerbot.ai", "Null mage resource efficiency: {:.2f}", efficiency);
    }
}

void MageAI::HandleCombatPhaseTransitions()
{
    // Adapt to different combat phases
    // This would require boss/encounter knowledge
}

::Unit* MageAI::SelectOptimalTarget(const std::vector<::Unit*>& enemies)
{
    if (enemies.empty())
        return nullptr;

    if (_targetSelector)
    {
        SelectionContext context;
        context.bot = GetBot();
        context.botRole = ThreatRole::DPS;
        context.currentTarget = GetBot()->GetSelectedUnit();
        context.groupTarget = nullptr;
        context.spellId = 0;
        context.maxRange = OPTIMAL_CASTING_RANGE;
        context.inCombat = GetBot()->IsInCombat();
        context.emergencyMode = false;

        SelectionResult result = _targetSelector->SelectBestTarget(context);
        if (result.success && result.target)
            return result.target;
    }

    // Fallback: return nearest enemy
    return enemies.front();
}

void MageAI::ExecuteAdvancedRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Use offensive cooldowns
    UseOffensiveCooldowns();

    // Basic rotation based on spec
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            // Arcane Blast spam with Arcane Missiles procs
            if (IsSpellReady(ARCANE_BLAST))
                CastSpell(target, ARCANE_BLAST);
            break;

        case MageSpec::FIRE:
            // Fireball with Fire Blast for instant damage
            if (IsSpellReady(FIREBALL))
                CastSpell(target, FIREBALL);
            if (IsSpellReady(FIRE_BLAST))
                CastSpell(target, FIRE_BLAST);
            break;

        case MageSpec::FROST:
            // Frostbolt with Ice Lance for shatters
            if (IsSpellReady(FROSTBOLT))
                CastSpell(target, FROSTBOLT);
            if (IsSpellReady(ICE_LANCE))
                CastSpell(target, ICE_LANCE);
            break;
    }

    // Track damage
    _damageDealt += 100; // Simplified
}

// Spell school mastery
void MageAI::UpdateSchoolMastery()
{
    // Update mastery bonuses based on gear and talents
}

float MageAI::GetSchoolMasteryBonus(MageSchool school)
{
    // Calculate mastery bonus for spell school
    switch (school)
    {
        case MageSchool::ARCANE:
            return _currentSpec == MageSpec::ARCANE ? 1.15f : 1.0f;
        case MageSchool::FIRE:
            return _currentSpec == MageSpec::FIRE ? 1.15f : 1.0f;
        case MageSchool::FROST:
            return _currentSpec == MageSpec::FROST ? 1.15f : 1.0f;
        default:
            return 1.0f;
    }
}

void MageAI::AdaptToTargetResistances(::Unit* target)
{
    if (!target)
        return;

    // Check target resistances and adapt spell selection
    // This would require resistance checking implementation
}

MageSchool MageAI::GetMostEffectiveSchool(::Unit* target)
{
    if (!target)
        return MageSchool::GENERIC;

    // Determine most effective school based on target
    // For now, return specialization school
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            return MageSchool::ARCANE;
        case MageSpec::FIRE:
            return MageSchool::FIRE;
        case MageSpec::FROST:
            return MageSchool::FROST;
        default:
            return MageSchool::GENERIC;
    }
}

// Predictive casting
void MageAI::PredictEnemyMovement(::Unit* target)
{
    if (!target || !_positionManager)
        return;

    // Predict where target will be for ground-targeted spells
    Position predictedPos = _positionManager->PredictTargetPosition(target, 2.0f); // 2 seconds ahead
}

void MageAI::PrecastSpells(::Unit* target)
{
    if (!target)
        return;

    // Start casting before target is in range if approaching
    float distance = GetBot()->GetDistance2d(target);
    if (distance < 35.0f && distance > 30.0f)
    {
        // Start precasting
        if (_currentSpec == MageSpec::FIRE && IsSpellReady(PYROBLAST))
        {
            CastSpell(target, PYROBLAST);
        }
    }
}

void MageAI::HandleMovingTargets(::Unit* target)
{
    if (!target)
        return;

    // Use instant casts for moving targets
    if (target->isMoving())
    {
        OptimizeInstantCasts();
    }
}

void MageAI::OptimizeInstantCasts()
{
    // Prioritize instant cast spells
    if (IsSpellReady(FIRE_BLAST))
        CastSpell(_currentTarget, FIRE_BLAST);

    if (_currentSpec == MageSpec::FROST && IsSpellReady(ICE_LANCE))
        CastSpell(_currentTarget, ICE_LANCE);

    if (_currentSpec == MageSpec::ARCANE && IsSpellReady(ARCANE_BARRAGE))
        CastSpell(_currentTarget, ARCANE_BARRAGE);
}

// Performance optimization
void MageAI::UpdatePerformanceMetrics(uint32 diff)
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - _combatMetrics.lastMetricsUpdate).count();

    if (elapsed > 5000) // Update every 5 seconds
    {
        _combatMetrics.lastMetricsUpdate = now;
        AnalyzeCastingEffectiveness();
    }
}

void MageAI::OptimizeCastingSequence()
{
    OptimizeSpellPriorities();

    // Adjust for current situation
    if (ShouldConserveMana())
    {
        // Use more efficient spells
        if (GetBot())
            TC_LOG_DEBUG("module.playerbot.ai", "Mage {} switching to mana-efficient rotation", GetBot()->GetName());
        else
            TC_LOG_DEBUG("module.playerbot.ai", "Null mage switching to mana-efficient rotation");
    }
}

// MageSpellCalculator implementations
uint32 MageSpellCalculator::CalculateFireballDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // Base damage calculation (simplified)
    uint32 baseDamage = 500;

    // Apply spell power scaling (simplified for TrinityCore compatibility)
    baseDamage += (caster->GetLevel() - 1) * 15; // Level-based scaling

    // Apply specialization bonus
    baseDamage = static_cast<uint32>(baseDamage * GetSpecializationBonus(MageSpec::FIRE, MageAI::FIREBALL));

    // Apply resistance
    float resistance = CalculateResistance(MageAI::FIREBALL, caster, target);
    baseDamage = ApplyResistance(baseDamage, resistance);

    return baseDamage;
}

uint32 MageSpellCalculator::CalculateFrostboltDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // Base damage calculation (simplified)
    uint32 baseDamage = 450;

    // Apply spell power scaling (use level-based scaling)
    baseDamage += (caster->GetLevel() - 1) * 18; // Level-based scaling for Fireball

    // Apply specialization bonus
    baseDamage = static_cast<uint32>(baseDamage * GetSpecializationBonus(MageSpec::FROST, MageAI::FROSTBOLT));

    // Apply resistance
    float resistance = CalculateResistance(MageAI::FROSTBOLT, caster, target);
    baseDamage = ApplyResistance(baseDamage, resistance);

    return baseDamage;
}

uint32 MageSpellCalculator::CalculateArcaneMissilesDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // Base damage calculation (simplified) - total for all missiles
    uint32 baseDamage = 600;

    // Apply spell power scaling (use level-based scaling)
    baseDamage += (caster->GetLevel() - 1) * 22; // Level-based scaling for Frostbolt

    // Apply specialization bonus
    baseDamage = static_cast<uint32>(baseDamage * GetSpecializationBonus(MageSpec::ARCANE, MageAI::ARCANE_MISSILES));

    // Apply resistance
    float resistance = CalculateResistance(MageAI::ARCANE_MISSILES, caster, target);
    baseDamage = ApplyResistance(baseDamage, resistance);

    return baseDamage;
}

uint32 MageSpellCalculator::CalculateSpellManaCost(uint32 spellId, Player* caster)
{
    if (!caster)
        return 0;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    // Check cache
    auto it = _manaCostCache.find(spellId);
    if (it != _manaCostCache.end())
        return it->second;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NORMAL);
    if (!spellInfo)
        return 0;

    // Calculate base mana cost
    auto powerCosts = spellInfo->CalcPowerCost(caster, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (const auto& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    // Cache the result
    _manaCostCache[spellId] = manaCost;

    return manaCost;
}

uint32 MageSpellCalculator::ApplyArcanePowerBonus(uint32 damage, Player* caster)
{
    if (!caster)
        return damage;

    // Check for Arcane Power buff
    if (caster->HasAura(MageAI::ARCANE_POWER))
    {
        damage = static_cast<uint32>(damage * 1.3f); // 30% damage increase
    }

    return damage;
}

uint32 MageSpellCalculator::CalculateCastTime(uint32 spellId, Player* caster)
{
    if (!caster)
        return 0;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    // Check cache
    auto it = _castTimeCache.find(spellId);
    if (it != _castTimeCache.end())
    {
        // Apply haste
        float hasteModifier = GetHasteModifier(caster);
        return static_cast<uint32>(it->second / hasteModifier);
    }

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NORMAL);
    if (!spellInfo)
        return 0;

    uint32 castTime = spellInfo->GetRecoveryTime();

    // Cache the base cast time
    _castTimeCache[spellId] = castTime;

    // Apply haste
    float hasteModifier = GetHasteModifier(caster);
    return static_cast<uint32>(castTime / hasteModifier);
}

float MageSpellCalculator::GetHasteModifier(Player* caster)
{
    if (!caster)
        return 1.0f;

    // Get haste rating and convert to percentage
    float hastePct = caster->GetRatingBonusValue(CR_HASTE_SPELL);
    // Rating bonus value already returns percentage

    return 1.0f + (hastePct / 100.0f);
}

float MageSpellCalculator::CalculateCritChance(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster)
        return 0.0f;

    // Base crit chance from rating system
    float critChance = caster->GetRatingBonusValue(CR_CRIT_SPELL);

    // Add spell-specific bonuses
    // This would require spell-specific data

    return critChance;
}

bool MageSpellCalculator::WillCriticalHit(uint32 spellId, Player* caster, ::Unit* target)
{
    float critChance = CalculateCritChance(spellId, caster, target);
    return (rand_chance() < critChance);
}

float MageSpellCalculator::CalculateResistance(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0.0f;

    // Simplified resistance calculation
    // This would require proper resistance mechanics
    return 0.0f;
}

uint32 MageSpellCalculator::ApplyResistance(uint32 damage, float resistance)
{
    return static_cast<uint32>(damage * (1.0f - resistance));
}

float MageSpellCalculator::GetSpecializationBonus(MageSpec spec, uint32 spellId)
{
    // Apply specialization bonus to matching spells
    switch (spec)
    {
        case MageSpec::ARCANE:
            if (spellId == MageAI::ARCANE_MISSILES || spellId == MageAI::ARCANE_BLAST)
                return 1.15f;
            break;
        case MageSpec::FIRE:
            if (spellId == MageAI::FIREBALL || spellId == MageAI::PYROBLAST)
                return 1.15f;
            break;
        case MageSpec::FROST:
            if (spellId == MageAI::FROSTBOLT || spellId == MageAI::ICE_LANCE)
                return 1.15f;
            break;
    }

    return 1.0f;
}

uint32 MageSpellCalculator::GetOptimalRotationSpell(MageSpec spec, Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // Return primary spell for each spec
    switch (spec)
    {
        case MageSpec::ARCANE:
            return MageAI::ARCANE_BLAST;
        case MageSpec::FIRE:
            return MageAI::FIREBALL;
        case MageSpec::FROST:
            return MageAI::FROSTBOLT;
        default:
            return 0;
    }
}

void MageSpellCalculator::CacheSpellData(uint32 spellId)
{
    // Pre-cache spell data for performance
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NORMAL);
    if (!spellInfo)
        return;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    // Cache various spell properties
    // This would be expanded with more data
}

uint32 MageAI::GetNearbyEnemyCount(float range) const
{
    if (!GetBot())
        return 0;

    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, range);

    for (auto& target : targets)
    {
        if (GetBot()->IsValidAttackTarget(target))
            count++;
    }

    return count;
}

Position MageAI::GetSafeCastingPosition()
{
    if (!GetBot())
        return Position();

    // Return a safe position for casting
    if (_positionManager)
        return _positionManager->FindSafePosition(GetBot()->GetPosition(), OPTIMAL_CASTING_RANGE);

    // Fallback: current position
    return GetBot()->GetPosition();
}

} // namespace Playerbot