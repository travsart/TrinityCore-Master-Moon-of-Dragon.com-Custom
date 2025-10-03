/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED ClassAI Implementation - Combat Only
 *
 * This implementation provides:
 * 1. Combat-only specialization without interfering with base behaviors
 * 2. No movement control - delegated to BotAI strategies
 * 3. No throttling that breaks following
 * 4. Clean integration with BotAI update chain
 */

#include "ClassAI.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Object.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include <chrono>

namespace Playerbot
{

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ClassAI::ClassAI(Player* bot) : BotAI(bot),
    _currentCombatTarget(nullptr),
    _inCombat(false),
    _combatTime(0),
    _lastTargetSwitch(0)
{
    // Initialize component managers for class-specific mechanics
    _actionQueue = std::make_unique<ActionPriorityQueue>();
    _cooldownManager = std::make_unique<CooldownManager>();
    _resourceManager = std::make_unique<ResourceManager>(bot);

    TC_LOG_DEBUG("playerbot.classai", "ClassAI created for bot {}",
                 bot ? bot->GetName() : "null");
}

ClassAI::~ClassAI() = default;

// ============================================================================
// COMBAT UPDATE - Called by BotAI when in combat
// ============================================================================

void ClassAI::OnCombatUpdate(uint32 diff)
{
    // CRITICAL: This method is called BY BotAI::UpdateAI() when in combat
    // It does NOT replace UpdateAI(), it extends it for combat only

    if (!GetBot() || !GetBot()->IsAlive())
        return;

    // Update component managers
    _cooldownManager->Update(diff);
    _combatTime += diff;

    // Update combat state
    UpdateCombatState(diff);

    // Update targeting - select best target
    UpdateTargeting();

    // Class-specific combat updates
    if (_currentCombatTarget)
    {
        // Update class-specific rotation
        UpdateRotation(_currentCombatTarget);

        // Update class-specific cooldowns
        UpdateCooldowns(diff);
    }
    else
    {
        // No target in combat - try to apply buffs
        UpdateBuffs();
    }

    // Performance logging (throttled)
    static uint32 lastLogTime = 0;
    uint32 currentTime = getMSTime();
    if (currentTime - lastLogTime > 5000) // Every 5 seconds
    {
        TC_LOG_DEBUG("playerbot.combat",
                     "ClassAI::OnCombatUpdate for {} - Target: {}, CombatTime: {}ms",
                     GetBot()->GetName(),
                     _currentCombatTarget ? _currentCombatTarget->GetName() : "None",
                     _combatTime);
        lastLogTime = currentTime;
    }
}

// ============================================================================
// COMBAT STATE MANAGEMENT
// ============================================================================

void ClassAI::OnCombatStart(::Unit* target)
{
    // Called by BotAI when entering combat
    _inCombat = true;
    _combatTime = 0;
    _currentCombatTarget = target;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} entering combat with {}",
                 GetBot()->GetName(), target ? target->GetName() : "unknown");

    // Let BotAI handle base combat start logic
    BotAI::OnCombatStart(target);
}

void ClassAI::OnCombatEnd()
{
    // Called by BotAI when leaving combat
    _inCombat = false;
    _combatTime = 0;
    _currentCombatTarget = nullptr;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} leaving combat", GetBot()->GetName());

    // Let BotAI handle base combat end logic
    BotAI::OnCombatEnd();
}

void ClassAI::OnTargetChanged(::Unit* newTarget)
{
    _currentCombatTarget = newTarget;
    _lastTargetSwitch = _combatTime;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} switching target to {}",
                 GetBot()->GetName(), newTarget ? newTarget->GetName() : "none");
}

// ============================================================================
// TARGETING
// ============================================================================

void ClassAI::UpdateTargeting()
{
    // Select best combat target
    ::Unit* bestTarget = GetBestAttackTarget();

    if (bestTarget != _currentCombatTarget)
    {
        OnTargetChanged(bestTarget);
    }
}

::Unit* ClassAI::GetBestAttackTarget()
{
    if (!GetBot())
        return nullptr;

    // Priority 1: Current victim
    if (::Unit* victim = GetBot()->GetVictim())
        return victim;

    // Priority 2: Selected target
    if (ObjectGuid targetGuid = GetBot()->GetTarget())
    {
        if (::Unit* target = ObjectAccessor::GetUnit(*GetBot(), targetGuid))
        {
            if (GetBot()->IsValidAttackTarget(target))
                return target;
        }
    }

    // Priority 3: Nearest hostile
    return GetNearestEnemy();
}

::Unit* ClassAI::GetNearestEnemy(float maxRange)
{
    if (!GetBot())
        return nullptr;

    ::Unit* nearestEnemy = nullptr;
    float nearestDistance = maxRange;

    // Find nearest enemy within range
    std::list<::Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), maxRange);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, maxRange);

    for (::Unit* target : targets)
    {
        if (!target || !GetBot()->IsValidAttackTarget(target))
            continue;

        float distance = GetBot()->GetDistance(target);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestEnemy = target;
        }
    }

    return nearestEnemy;
}

// ============================================================================
// HEALING SUPPORT
// ============================================================================

::Unit* ClassAI::GetBestHealTarget()
{
    if (!GetBot() || !GetBot()->GetGroup())
        return GetBot(); // Self-heal if not in group

    ::Unit* lowestHealthTarget = nullptr;
    float lowestHealthPct = 100.0f;

    // Check group members
    Group* group = GetBot()->GetGroup();
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
        {
            if (!member->IsAlive() || !member->IsWithinDistInMap(GetBot(), 40.0f))
                continue;

            float healthPct = member->GetHealthPct();
            if (healthPct < lowestHealthPct)
            {
                lowestHealthPct = healthPct;
                lowestHealthTarget = member;
            }
        }
    }

    return lowestHealthTarget ? lowestHealthTarget : GetBot();
}

::Unit* ClassAI::GetLowestHealthAlly(float maxRange)
{
    // Similar to GetBestHealTarget but with range parameter
    return GetBestHealTarget();
}

// ============================================================================
// COMBAT STATE TRACKING
// ============================================================================

void ClassAI::UpdateCombatState(uint32 diff)
{
    // Track combat metrics and state
    // This is for internal class AI tracking, not for movement or base behaviors
}

// ============================================================================
// COOLDOWN MANAGEMENT
// ============================================================================

void ClassAI::UpdateCooldowns(uint32 diff)
{
    // Default implementation - derived classes can override
    // This is for tracking class-specific ability cooldowns
}

bool ClassAI::CanUseAbility(uint32 spellId)
{
    return IsSpellReady(spellId) && HasEnoughResource(spellId);
}

bool ClassAI::IsSpellReady(uint32 spellId)
{
    if (!spellId || !GetBot())
        return false;

    // Check if spell is on cooldown
    return _cooldownManager->IsReady(spellId) && _cooldownManager->IsGCDReady();
}

// ============================================================================
// SPELL UTILITIES
// ============================================================================

bool ClassAI::IsInRange(::Unit* target, uint32 spellId)
{
    if (!target || !spellId || !GetBot())
        return false;

    float range = GetSpellRange(spellId);
    if (range <= 0.0f)
        return true; // No range restriction

    return GetBot()->GetDistance(target) <= range;
}

bool ClassAI::HasLineOfSight(::Unit* target)
{
    if (!target || !GetBot())
        return false;

    return GetBot()->IsWithinLOSInMap(target);
}

bool ClassAI::IsSpellUsable(uint32 spellId)
{
    if (!spellId || !GetBot())
        return false;

    // Check if bot knows the spell
    if (!GetBot()->HasSpell(spellId))
        return false;

    // Check if spell is ready
    if (!IsSpellReady(spellId))
        return false;

    // Check resource requirements
    if (!HasEnoughResource(spellId))
        return false;

    return true;
}

float ClassAI::GetSpellRange(uint32 spellId)
{
    if (!spellId)
        return 0.0f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0.0f;

    return spellInfo->GetMaxRange();
}

uint32 ClassAI::GetSpellCooldown(uint32 spellId)
{
    if (!spellId || !GetBot())
        return 0;

    return GetBot()->GetSpellHistory()->GetRemainingCooldown(spellId);
}

// ============================================================================
// SPELL CASTING
// ============================================================================

bool ClassAI::CastSpell(::Unit* target, uint32 spellId)
{
    if (!target || !spellId || !GetBot())
        return false;

    if (!IsSpellUsable(spellId))
        return false;

    if (!IsInRange(target, spellId))
        return false;

    if (!HasLineOfSight(target))
        return false;

    // Cast the spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    GetBot()->CastSpell(target, spellId, false);
    ConsumeResource(spellId);
    _cooldownManager->StartCooldown(spellId, spellInfo->RecoveryTime);

    return true;
}

bool ClassAI::CastSpell(uint32 spellId)
{
    // Self-cast version
    return CastSpell(GetBot(), spellId);
}

// ============================================================================
// AURA UTILITIES
// ============================================================================

bool ClassAI::HasAura(uint32 spellId, ::Unit* target)
{
    ::Unit* checkTarget = target ? target : GetBot();
    if (!checkTarget)
        return false;

    return checkTarget->HasAura(spellId);
}

uint32 ClassAI::GetAuraStacks(uint32 spellId, ::Unit* target)
{
    ::Unit* checkTarget = target ? target : GetBot();
    if (!checkTarget)
        return 0;

    if (Aura* aura = checkTarget->GetAura(spellId))
        return aura->GetStackAmount();

    return 0;
}

uint32 ClassAI::GetAuraRemainingTime(uint32 spellId, ::Unit* target)
{
    ::Unit* checkTarget = target ? target : GetBot();
    if (!checkTarget)
        return 0;

    if (Aura* aura = checkTarget->GetAura(spellId))
        return aura->GetDuration();

    return 0;
}

// ============================================================================
// MOVEMENT QUERIES (READ-ONLY)
// ============================================================================

bool ClassAI::IsMoving() const
{
    return GetBot() && GetBot()->IsMoving();
}

bool ClassAI::IsInMeleeRange(::Unit* target) const
{
    if (!target || !GetBot())
        return false;

    return GetBot()->IsWithinMeleeRange(target);
}

bool ClassAI::ShouldMoveToTarget(::Unit* target) const
{
    if (!target || !GetBot())
        return false;

    // ClassAI doesn't control movement, just provides information
    // Actual movement is handled by BotAI strategies
    float optimalRange = const_cast<ClassAI*>(this)->GetOptimalRange(target);
    float currentDistance = GetBot()->GetDistance(target);

    return currentDistance > optimalRange;
}

float ClassAI::GetDistanceToTarget(::Unit* target) const
{
    if (!target || !GetBot())
        return 0.0f;

    return GetBot()->GetDistance(target);
}

// ============================================================================
// POSITIONING PREFERENCES
// ============================================================================

Position ClassAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !GetBot())
        return Position();

    // Default implementation - stay at optimal range
    float optimalRange = GetOptimalRange(target);
    float angle = GetBot()->GetAngle(target);

    Position pos;
    pos.m_positionX = target->GetPositionX() - optimalRange * std::cos(angle);
    pos.m_positionY = target->GetPositionY() - optimalRange * std::sin(angle);
    pos.m_positionZ = target->GetPositionZ();
    pos.SetOrientation(target->GetOrientation());

    return pos;
}

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

void ClassAI::RecordPerformanceMetric(std::string const& metric, uint32 value)
{
    // Record class-specific performance metrics
    TC_LOG_TRACE("playerbot.performance", "ClassAI metric {} = {} for bot {}",
                 metric, value, GetBot() ? GetBot()->GetName() : "null");
}

} // namespace Playerbot