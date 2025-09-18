/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ProtectionSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "Pet.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Log.h"
#include "PlayerbotAI.h"
#include "MotionMaster.h"
#include "ThreatMgr.h"
#include <algorithm>

namespace Playerbot
{

ProtectionSpecialization::ProtectionSpecialization(Player* bot)
    : WarriorSpecialization(bot), _lastTaunt(0), _lastShieldBlock(0), _lastShieldWall(0),
      _shieldBlockCharges(0), _hasShieldEquipped(false), _lastThreatCheck(0), _lastShieldCheck(0),
      _lastPositionCheck(0), _lastSunderCheck(0), _lastRotationUpdate(0), _lastTargetScan(0),
      _emergencyMode(false), _emergencyStartTime(0)
{
}

void ProtectionSpecialization::UpdateRotation(Unit* target)
{
    if (!target || !_bot)
        return;

    uint32 now = getMSTime();

    // Performance optimization - don't update rotation too frequently
    if (now - _lastRotationUpdate < 200) // 200ms throttle
        return;

    _lastRotationUpdate = now;

    // Update mechanics
    UpdateThreatManagement();
    UpdateShieldMastery();
    UpdateStance();
    UpdateTaunt();
    ManageSunderArmor();

    // Handle multiple enemies
    HandleMultipleEnemies();

    // Handle emergencies
    if (IsInDangerousSituation())
    {
        HandleEmergencies();
        return;
    }

    // Protection rotation priority
    // 1. Shield Slam (highest priority when available)
    if (ShouldCastShieldSlam(target) && HasEnoughResource(SHIELD_SLAM_RAGE_COST))
    {
        CastShieldSlam(target);
        return;
    }

    // 2. Revenge when it's available (requires block/dodge/parry)
    if (ShouldCastRevenge(target) && HasEnoughResource(REVENGE_RAGE_COST))
    {
        CastRevenge(target);
        return;
    }

    // 3. Thunder Clap for multiple enemies
    if (ShouldCastThunderClap())
    {
        CastThunderClap();
        return;
    }

    // 4. Devastate for threat and sunder armor
    if (ShouldCastDevastateorSunder(target) && HasEnoughResource(DEVASTATE_RAGE_COST))
    {
        CastDevastate(target);
        return;
    }

    // 5. Sunder Armor if devastate not available
    if (NeedsSunderArmor(target) && HasEnoughResource(15))
    {
        CastSunderArmor(target);
        return;
    }

    // 6. Heroic Strike if high on rage
    if (GetRagePercent() > 80.0f && HasEnoughResource(15))
    {
        if (_bot->CastSpell(target, HEROIC_STRIKE, false))
        {
            TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast heroic strike (rage dump)",
                        _bot->GetName());
        }
        return;
    }

    // 7. Basic attacks if in range
    if (IsInMeleeRange(target) && !_bot->HasUnitState(UNIT_STATE_CASTING))
    {
        _bot->AttackerStateUpdate(target);
    }
}

void ProtectionSpecialization::UpdateBuffs()
{
    if (!_bot)
        return;

    // Maintain defensive stance
    UpdateDefensiveStance();

    // Maintain battle shout
    CastShout();

    // Use shield block when needed
    if (HasShieldEquipped() && _shieldBlockCharges < 2)
    {
        CastShieldBlock();
    }

    // Use defensive cooldowns when needed
    UseDefensiveCooldowns();

    // Maintain shield wall in dangerous situations
    if (ShouldUseShieldWall())
    {
        UseShieldWall();
    }
}

void ProtectionSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update internal cooldown tracking
    for (auto& [spellId, cooldownEnd] : _cooldowns)
    {
        if (cooldownEnd > diff)
            cooldownEnd -= diff;
        else
            cooldownEnd = 0;
    }

    UpdateDefensiveCooldowns();

    // Update shield block charges decay
    if (_shieldBlockCharges > 0)
    {
        uint32 now = getMSTime();
        if (now - _lastShieldBlock > SHIELD_BLOCK_DURATION)
        {
            _shieldBlockCharges = std::max(0u, _shieldBlockCharges - 1);
            _lastShieldBlock = now;
        }
    }
}

bool ProtectionSpecialization::CanUseAbility(uint32 spellId)
{
    if (!_bot)
        return false;

    if (_bot->HasSpellCooldown(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    return CanUseAbility();
}

void ProtectionSpecialization::OnCombatStart(Unit* target)
{
    if (!target || !_bot)
        return;

    // Switch to defensive stance
    if (!IsInStance(WarriorStance::DEFENSIVE))
    {
        SwitchStance(WarriorStance::DEFENSIVE);
    }

    // Charge if not in melee range
    if (!IsInMeleeRange(target) && !_bot->HasSpellCooldown(CHARGE))
    {
        CastCharge(target);
    }

    // Initialize shield block if we have a shield
    if (HasShieldEquipped())
    {
        CastShieldBlock();
    }

    // Reset combat state
    _emergencyMode = false;
    _emergencyStartTime = 0;
    _controlledTargets.clear();
    _looseTargets.clear();
    _threatLevels.clear();
    _sunderArmorStacks.clear();

    TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} entered combat with target {}",
                _bot->GetName(), target->GetName());
}

void ProtectionSpecialization::OnCombatEnd()
{
    // Reset combat state
    _emergencyMode = false;
    _emergencyStartTime = 0;
    _shieldBlockCharges = 0;
    _controlledTargets.clear();
    _looseTargets.clear();
    _threatLevels.clear();
    _sunderArmorStacks.clear();

    // Clear threat queue
    while (!_threatQueue.empty())
        _threatQueue.pop();

    TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} combat ended",
                _bot->GetName());
}

bool ProtectionSpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case SHIELD_SLAM:
            return HasEnoughRage(SHIELD_SLAM_RAGE_COST);
        case REVENGE:
            return HasEnoughRage(REVENGE_RAGE_COST);
        case DEVASTATE:
            return HasEnoughRage(DEVASTATE_RAGE_COST);
        case SUNDER_ARMOR:
            return HasEnoughRage(15);
        case THUNDER_CLAP:
            return HasEnoughRage(20);
        case CONCUSSION_BLOW:
            return HasEnoughRage(15);
        case TAUNT:
        case CHALLENGING_SHOUT:
        case SHIELD_BLOCK:
        case SHIELD_WALL:
        case SPELL_REFLECTION:
            return true; // No rage cost
        default:
            return HasEnoughRage(15); // Default rage cost
    }
}

void ProtectionSpecialization::ConsumeResource(uint32 spellId)
{
    // Resources are consumed automatically by spell casting
    // This method is for tracking purposes
    uint32 now = getMSTime();
    _cooldowns[spellId] = now + 1500; // 1.5s global cooldown
}

Position ProtectionSpecialization::GetOptimalPosition(Unit* target)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    return GetOptimalTankPosition();
}

float ProtectionSpecialization::GetOptimalRange(Unit* target)
{
    return OPTIMAL_MELEE_RANGE;
}

void ProtectionSpecialization::UpdateStance()
{
    // Protection warriors should stay in defensive stance
    if (!IsInStance(WarriorStance::DEFENSIVE))
    {
        SwitchStance(WarriorStance::DEFENSIVE);
    }
}

WarriorStance ProtectionSpecialization::GetOptimalStance(Unit* target)
{
    return WarriorStance::DEFENSIVE;
}

void ProtectionSpecialization::SwitchStance(WarriorStance stance)
{
    if (stance == WarriorStance::DEFENSIVE)
    {
        EnterDefensiveStance();
    }
    else
    {
        WarriorSpecialization::SwitchStance(stance);
    }
}

// Private methods

void ProtectionSpecialization::UpdateThreatManagement()
{
    uint32 now = getMSTime();

    if (now - _lastThreatCheck < 1000) // Check every second
        return;

    _lastThreatCheck = now;

    UpdateThreatList();
    MaintainThreatOnAll();
    PickupLooseTargets();
}

void ProtectionSpecialization::UpdateShieldMastery()
{
    uint32 now = getMSTime();

    if (now - _lastShieldCheck < 1000) // Check every second
        return;

    _lastShieldCheck = now;

    _hasShieldEquipped = HasShieldEquipped();
    OptimizeShieldUsage();
}

void ProtectionSpecialization::UpdateDefensiveStance()
{
    if (!IsInStance(WarriorStance::DEFENSIVE))
    {
        SwitchStance(WarriorStance::DEFENSIVE);
    }
}

void ProtectionSpecialization::UpdateTaunt()
{
    // Taunt management is handled in threat management
}

bool ProtectionSpecialization::ShouldCastShieldSlam(Unit* target)
{
    if (!target || !_bot)
        return false;

    if (!HasShieldEquipped())
        return false;

    if (_bot->HasSpellCooldown(SHIELD_SLAM))
        return false;

    if (!IsInMeleeRange(target))
        return false;

    return true; // Shield slam is highest priority when available
}

bool ProtectionSpecialization::ShouldCastRevenge(Unit* target)
{
    if (!target || !_bot)
        return false;

    if (_bot->HasSpellCooldown(REVENGE))
        return false;

    if (!IsInMeleeRange(target))
        return false;

    // Revenge requires a recent block, dodge, or parry
    // Since we can't easily detect these events, we'll use it on cooldown when available
    return true;
}

bool ProtectionSpecialization::ShouldCastDevastateorSunder(Unit* target)
{
    if (!target || !_bot)
        return false;

    if (!IsInMeleeRange(target))
        return false;

    // Use devastate if available, otherwise fall back to sunder armor
    if (!_bot->HasSpellCooldown(DEVASTATE))
        return true;

    return NeedsSunderArmor(target);
}

bool ProtectionSpecialization::ShouldCastThunderClap()
{
    if (!_bot)
        return false;

    if (_bot->HasSpellCooldown(THUNDER_CLAP))
        return false;

    // Check for multiple enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 10.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 10.0f);

    return targets.size() >= 2;
}

bool ProtectionSpecialization::ShouldTaunt(Unit* target)
{
    if (!target || !_bot)
        return false;

    if (_bot->HasSpellCooldown(TAUNT))
        return false;

    // Check if we don't have threat on the target
    return !HasThreat(target) || GetThreatPercent(target) < THREAT_THRESHOLD;
}

void ProtectionSpecialization::OptimizeShieldUsage()
{
    if (!HasShieldEquipped())
        return;

    // Use shield block when charges are low
    if (_shieldBlockCharges < 2)
    {
        CastShieldBlock();
    }
}

void ProtectionSpecialization::UpdateShieldBlock()
{
    uint32 now = getMSTime();

    // Update shield block charges
    if (_shieldBlockCharges > 0 && now - _lastShieldBlock > 5000) // 5 second charge duration
    {
        _shieldBlockCharges--;
    }
}

void ProtectionSpecialization::CastShieldBlock()
{
    if (!_bot || !HasShieldEquipped())
        return;

    if (_bot->HasSpellCooldown(SHIELD_BLOCK))
        return;

    if (_shieldBlockCharges >= 2)
        return;

    if (_bot->CastSpell(_bot, SHIELD_BLOCK, false))
    {
        _lastShieldBlock = getMSTime();
        _shieldBlockCharges = std::min(_shieldBlockCharges + 1, 2u);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} used shield block (charges: {})",
                    _bot->GetName(), _shieldBlockCharges);
    }
}

void ProtectionSpecialization::CastShieldWall()
{
    if (!_bot)
        return;

    if (_bot->GetHealthPct() > 30.0f)
        return;

    if (_bot->HasSpellCooldown(SHIELD_WALL))
        return;

    if (_bot->CastSpell(_bot, SHIELD_WALL, false))
    {
        _lastShieldWall = getMSTime();
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} used shield wall",
                    _bot->GetName());
    }
}

bool ProtectionSpecialization::HasShieldEquipped()
{
    if (!_bot)
        return false;

    Item* offHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    return offHand && offHand->GetTemplate()->Class == ITEM_CLASS_ARMOR &&
           offHand->GetTemplate()->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD;
}

bool ProtectionSpecialization::ShouldUseShieldWall()
{
    if (!_bot)
        return false;

    return _bot->GetHealthPct() < 30.0f && !_bot->HasSpellCooldown(SHIELD_WALL);
}

uint32 ProtectionSpecialization::GetShieldBlockCharges()
{
    return _shieldBlockCharges;
}

void ProtectionSpecialization::GenerateThreat()
{
    Unit* target = _bot->GetSelectedUnit();
    if (!target)
        return;

    // Use high-threat abilities
    if (ShouldCastShieldSlam(target) && HasEnoughResource(SHIELD_SLAM_RAGE_COST))
    {
        CastShieldSlam(target);
    }
    else if (ShouldCastRevenge(target) && HasEnoughResource(REVENGE_RAGE_COST))
    {
        CastRevenge(target);
    }
    else if (ShouldCastDevastateorSunder(target) && HasEnoughResource(DEVASTATE_RAGE_COST))
    {
        CastDevastate(target);
    }
}

void ProtectionSpecialization::ManageMultipleTargets()
{
    std::vector<Unit*> enemies = GetThreatTargets();

    for (Unit* enemy : enemies)
    {
        if (!HasThreat(enemy))
        {
            if (ShouldTaunt(enemy))
            {
                CastTaunt(enemy);
                break; // Only taunt one target per update
            }
        }
    }
}

void ProtectionSpecialization::UpdateThreatList()
{
    uint32 now = getMSTime();

    // Clear old threat data
    _threatLevels.clear();

    // Get nearby enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 30.0f);

    // Update threat levels
    for (Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        float threatPercent = GetThreatPercent(target);
        _threatLevels[target->GetGUID().GetRawValue()] = threatPercent;

        // Add to threat queue
        ThreatPriority priority = ThreatPriority::MODERATE;
        if (threatPercent < 50.0f)
            priority = ThreatPriority::CRITICAL;
        else if (threatPercent < 80.0f)
            priority = ThreatPriority::HIGH;

        _threatQueue.push(ThreatTarget(target, priority, threatPercent));
    }
}

std::vector<Unit*> ProtectionSpecialization::GetThreatTargets()
{
    std::vector<Unit*> targets;

    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, u_check);
    Cell::VisitAllObjects(_bot, searcher, 30.0f);

    for (Unit* enemy : nearbyEnemies)
    {
        if (enemy && enemy->IsAlive())
            targets.push_back(enemy);
    }

    return targets;
}

Unit* ProtectionSpecialization::GetHighestThreatTarget()
{
    if (_threatQueue.empty())
        return nullptr;

    return _threatQueue.top().target;
}

bool ProtectionSpecialization::HasThreat(Unit* target)
{
    if (!target || !_bot)
        return false;

    return target->GetThreatMgr().GetThreat(_bot) > 0;
}

float ProtectionSpecialization::GetThreatPercent(Unit* target)
{
    if (!target || !_bot)
        return 0.0f;

    ThreatMgr& threatMgr = target->GetThreatMgr();
    float maxThreat = threatMgr.GetMaxThreat();
    float myThreat = threatMgr.GetThreat(_bot);

    if (maxThreat <= 0.0f)
        return 100.0f; // We're the only one with threat

    return (myThreat / maxThreat) * 100.0f;
}

void ProtectionSpecialization::CastShieldSlam(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, SHIELD_SLAM, false))
    {
        ConsumeResource(SHIELD_SLAM);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast shield slam on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void ProtectionSpecialization::CastRevenge(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, REVENGE, false))
    {
        ConsumeResource(REVENGE);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast revenge on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void ProtectionSpecialization::CastDevastate(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, DEVASTATE, false))
    {
        ConsumeResource(DEVASTATE);
        ApplySunderArmor(target);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast devastate on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void ProtectionSpecialization::CastSunderArmor(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, SUNDER_ARMOR, false))
    {
        ConsumeResource(SUNDER_ARMOR);
        ApplySunderArmor(target);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast sunder armor on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void ProtectionSpecialization::CastThunderClap()
{
    if (!_bot)
        return;

    if (_bot->CastSpell(_bot, THUNDER_CLAP, false))
    {
        ConsumeResource(THUNDER_CLAP);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast thunder clap",
                    _bot->GetName());
    }
}

void ProtectionSpecialization::CastConcussionBlow(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, CONCUSSION_BLOW, false))
    {
        ConsumeResource(CONCUSSION_BLOW);
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} cast concussion blow on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void ProtectionSpecialization::CastTaunt(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->CastSpell(target, TAUNT, false))
    {
        _lastTaunt = getMSTime();
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} taunted target {}",
                    _bot->GetName(), target->GetName());
    }
}

void ProtectionSpecialization::UpdateDefensiveCooldowns()
{
    // This is handled in the main update loop
}

void ProtectionSpecialization::UseDefensiveCooldowns()
{
    if (!_bot)
        return;

    float healthPct = _bot->GetHealthPct();

    // Use last stand at low health
    if (ShouldUseLastStand())
    {
        CastLastStand();
    }

    // Use shield wall when very low
    if (ShouldUseShieldWall())
    {
        CastShieldWall();
    }

    // Use spell reflection against casters
    if (ShouldUseSpellReflection())
    {
        CastSpellReflection();
    }
}

void ProtectionSpecialization::CastLastStand()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(LAST_STAND))
        return;

    if (_bot->CastSpell(_bot, LAST_STAND, false))
    {
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} used last stand",
                    _bot->GetName());
    }
}

void ProtectionSpecialization::CastSpellReflection()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(SPELL_REFLECTION))
        return;

    if (_bot->CastSpell(_bot, SPELL_REFLECTION, false))
    {
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} used spell reflection",
                    _bot->GetName());
    }
}

void ProtectionSpecialization::CastChallenging()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(CHALLENGING_SHOUT))
        return;

    // Use challenging shout when overwhelmed
    std::vector<Unit*> enemies = GetThreatTargets();
    if (enemies.size() >= 3)
    {
        if (_bot->CastSpell(_bot, CHALLENGING_SHOUT, false))
        {
            TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} used challenging shout on {} enemies",
                        _bot->GetName(), enemies.size());
        }
    }
}

bool ProtectionSpecialization::ShouldUseLastStand()
{
    if (!_bot)
        return false;

    return _bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD && !_bot->HasSpellCooldown(LAST_STAND);
}

bool ProtectionSpecialization::ShouldUseSpellReflection()
{
    if (!_bot)
        return false;

    if (_bot->HasSpellCooldown(SPELL_REFLECTION))
        return false;

    // Check for nearby casting enemies
    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 20.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, enemies, u_check);
    Cell::VisitAllObjects(_bot, searcher, 20.0f);

    for (Unit* enemy : enemies)
    {
        if (enemy && enemy->HasUnitState(UNIT_STATE_CASTING))
            return true;
    }

    return false;
}

void ProtectionSpecialization::HandleMultipleEnemies()
{
    uint32 now = getMSTime();

    if (now - _lastTargetScan < 2000) // Scan every 2 seconds
        return;

    _lastTargetScan = now;

    ManageMultipleTargets();
    PickupLooseTargets();

    // Use AoE abilities if needed
    std::vector<Unit*> enemies = GetThreatTargets();
    if (enemies.size() >= 3)
    {
        if (ShouldCastThunderClap())
        {
            CastThunderClap();
        }

        // Use challenging shout if overwhelmed
        CastChallenging();
    }
}

void ProtectionSpecialization::MaintainThreatOnAll()
{
    std::vector<Unit*> enemies = GetThreatTargets();

    for (Unit* enemy : enemies)
    {
        float threatPercent = GetThreatPercent(enemy);
        if (threatPercent < THREAT_THRESHOLD)
        {
            // Try to gain threat through attacks
            if (IsInMeleeRange(enemy))
            {
                GenerateThreat();
                break; // One action per update
            }
            else if (ShouldTaunt(enemy))
            {
                CastTaunt(enemy);
                break;
            }
        }
    }
}

void ProtectionSpecialization::PickupLooseTargets()
{
    std::vector<Unit*> uncontrolled = GetUncontrolledEnemies();

    for (Unit* target : uncontrolled)
    {
        if (target && ShouldTaunt(target))
        {
            TauntLooseTarget(target);
            break; // One taunt per update
        }
    }
}

std::vector<Unit*> ProtectionSpecialization::GetUncontrolledEnemies()
{
    std::vector<Unit*> uncontrolled;
    std::vector<Unit*> allEnemies = GetThreatTargets();

    for (Unit* enemy : allEnemies)
    {
        if (!HasThreat(enemy) || GetThreatPercent(enemy) < 50.0f)
        {
            uncontrolled.push_back(enemy);
        }
    }

    return uncontrolled;
}

void ProtectionSpecialization::TauntLooseTarget(Unit* target)
{
    if (!target)
        return;

    CastTaunt(target);
}

void ProtectionSpecialization::OptimizeTankPositioning()
{
    uint32 now = getMSTime();

    if (now - _lastPositionCheck < 3000) // Check every 3 seconds
        return;

    _lastPositionCheck = now;

    FaceAllEnemies();
    PositionForGroupProtection();
}

void ProtectionSpecialization::FaceAllEnemies()
{
    std::vector<Unit*> enemies = GetThreatTargets();

    if (enemies.empty())
        return;

    // Face the primary target
    Unit* primaryTarget = _bot->GetSelectedUnit();
    if (primaryTarget)
    {
        _bot->SetInFront(primaryTarget);
    }
}

void ProtectionSpecialization::PositionForGroupProtection()
{
    // Position between enemies and group members
    if (Group* group = _bot->GetGroup())
    {
        Position optimalPos = GetOptimalTankPosition();
        if (_bot->GetDistance2d(optimalPos) > 3.0f)
        {
            _bot->GetMotionMaster()->MovePoint(0, optimalPos);
        }
    }
}

Position ProtectionSpecialization::GetOptimalTankPosition()
{
    // For now, return current position
    // This can be enhanced with more sophisticated positioning logic
    return _bot->GetPosition();
}

void ProtectionSpecialization::ManageSunderArmor()
{
    uint32 now = getMSTime();

    if (now - _lastSunderCheck < 3000) // Check every 3 seconds
        return;

    _lastSunderCheck = now;

    Unit* target = _bot->GetSelectedUnit();
    if (target && NeedsSunderArmor(target))
    {
        ApplySunderArmor(target);
    }
}

void ProtectionSpecialization::ApplySunderArmor(Unit* target)
{
    if (!target)
        return;

    uint64 guid = target->GetGUID().GetRawValue();
    uint32 currentStacks = GetSunderArmorStacks(target);

    if (currentStacks < MAX_SUNDER_STACKS)
    {
        _sunderArmorStacks[guid] = currentStacks + 1;
    }
}

uint32 ProtectionSpecialization::GetSunderArmorStacks(Unit* target)
{
    if (!target)
        return 0;

    uint64 guid = target->GetGUID().GetRawValue();
    auto it = _sunderArmorStacks.find(guid);
    if (it != _sunderArmorStacks.end())
        return it->second;

    return 0;
}

bool ProtectionSpecialization::NeedsSunderArmor(Unit* target)
{
    if (!target)
        return false;

    return GetSunderArmorStacks(target) < MAX_SUNDER_STACKS;
}

void ProtectionSpecialization::HandleEmergencies()
{
    if (!_emergencyMode)
    {
        _emergencyMode = true;
        _emergencyStartTime = getMSTime();
        TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} entered emergency mode",
                    _bot->GetName());
    }

    UseEmergencyAbilities();
}

void ProtectionSpecialization::UseEmergencyAbilities()
{
    if (!_bot)
        return;

    float healthPct = _bot->GetHealthPct();

    // Use all available defensive cooldowns
    if (healthPct < 15.0f)
    {
        CastLastStand();
        CastShieldWall();
        UseEnragedRegeneration();
    }
    else if (healthPct < 25.0f)
    {
        CastShieldWall();
    }

    // Call for help if in a group
    CallForHelp();
}

bool ProtectionSpecialization::IsInDangerousSituation()
{
    if (!_bot)
        return false;

    // Check health
    if (_bot->GetHealthPct() < EMERGENCY_HEALTH_THRESHOLD)
        return true;

    // Check for overwhelming number of enemies
    std::vector<Unit*> enemies = GetThreatTargets();
    if (enemies.size() >= 5)
        return true;

    // Check for high-level enemies
    for (Unit* enemy : enemies)
    {
        if (enemy && enemy->GetLevel() > _bot->GetLevel() + 3)
            return true;
    }

    return false;
}

void ProtectionSpecialization::CallForHelp()
{
    // This would be implemented to request help from group members
    // For now, just log the request
    TC_LOG_DEBUG("playerbots", "ProtectionSpecialization: Bot {} is calling for help",
                _bot->GetName());
}

} // namespace Playerbot