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
#include "Movement/BotMovementUtil.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "../Combat/CombatBehaviorIntegration.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Object.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "MotionMaster.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
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

    // Initialize unified combat behavior system
    // This provides advanced combat coordination across all managers
    try {
        _combatBehaviors = std::make_unique<CombatBehaviorIntegration>(bot);
        TC_LOG_DEBUG("playerbot.classai", "CombatBehaviorIntegration initialized for bot {}",
                     bot ? bot->GetName() : "null");
    }
    catch (const std::exception& e) {
        TC_LOG_ERROR("playerbot.classai", "Failed to initialize CombatBehaviorIntegration for bot {}: {}",
                     bot ? bot->GetName() : "null", e.what());
        _combatBehaviors = nullptr;
    }

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

    // CRITICAL: Execute pending spell if ready (every frame, like players)
    // This mirrors Player::Update() which calls CanExecutePendingSpellCastRequest()
    // and ExecutePendingSpellCastRequest() every frame - see Player.cpp:946-947

    // DIAGNOSTIC: Log spell queue check
    static uint32 lastSpellQueueLog = 0;
    uint32 currentTime = getMSTime();
    if (currentTime - lastSpellQueueLog > 500) // Every 500ms
    {
        TC_LOG_ERROR("module.playerbot.classai", "🔍 OnCombatUpdate: Checking spell queue for bot {} - hasPending={}",
                    GetBot()->GetName(), _pendingSpellCastRequest != nullptr);
        lastSpellQueueLog = currentTime;
    }

    if (CanExecutePendingSpell())
    {
        TC_LOG_ERROR("module.playerbot.classai", "✅ OnCombatUpdate: Calling ExecutePendingSpell for bot {}",
                    GetBot()->GetName());
        ExecutePendingSpell();
    }

    // Update unified combat behavior system
    // This manages all advanced combat behaviors including interrupts, defensive actions,
    // crowd control, target prioritization, and emergency responses
    if (_combatBehaviors)
    {
        _combatBehaviors->Update(diff);
    }

    // DIAGNOSTIC: Log that OnCombatUpdate is being called
    static uint32 lastCombatLog = 0;
    if (currentTime - lastCombatLog > 2000) // Every 2 seconds (reuse currentTime from spell queue check)
    {
        TC_LOG_ERROR("module.playerbot", "⚔️ ClassAI::OnCombatUpdate: Bot {} - currentTarget={}, combatTime={}ms, behaviors={}",
                     GetBot()->GetName(),
                     _currentCombatTarget ? _currentCombatTarget->GetName() : "NONE",
                     _combatTime,
                     _combatBehaviors ? "active" : "inactive");
        lastCombatLog = currentTime;
    }

    // Update component managers
    _cooldownManager->Update(diff);
    _combatTime += diff;

    // Update combat state
    UpdateCombatState(diff);

    // Update targeting - select best target
    // If combat behaviors suggest a priority target, use it
    if (_combatBehaviors)
    {
        if (Unit* priorityTarget = _combatBehaviors->GetPriorityTarget())
        {
            if (priorityTarget != _currentCombatTarget && GetBot()->IsValidAttackTarget(priorityTarget))
            {
                OnTargetChanged(priorityTarget);
            }
        }
        else
        {
            UpdateTargeting();
        }
    }
    else
    {
        UpdateTargeting();
    }

    // Check for emergency actions from combat behavior system
    // These take priority over normal rotation (interrupts, defensives, etc.)
    if (_combatBehaviors && _combatBehaviors->HandleEmergencies())
    {
        // Emergency action was taken, skip normal rotation this update
        TC_LOG_DEBUG("playerbot.classai", "Bot {} handled emergency action, skipping rotation",
                     GetBot()->GetName());
        return;
    }

    // Check for high-priority combat behaviors before rotation
    if (_combatBehaviors && _combatBehaviors->HasPendingAction())
    {
        RecommendedAction action = _combatBehaviors->GetNextAction();
        if (RequiresImmediateAction(action.urgency))
        {
            bool executed = ExecuteRecommendedAction(action);
            _combatBehaviors->RecordActionResult(action, executed);

            if (executed && IsEmergencyAction(action.urgency))
            {
                // Emergency or critical action executed, skip normal rotation
                TC_LOG_DEBUG("playerbot.classai", "Bot {} executed {} urgency action: {} ({})",
                             GetBot()->GetName(), GetUrgencyName(action.urgency),
                             GetActionName(action.type), action.reason.c_str());
                return;
            }
        }
    }

    // Class-specific combat updates
    if (_currentCombatTarget)
    {
        TC_LOG_ERROR("module.playerbot", "🗡️ Calling UpdateRotation for {} (class {}) with target {}",
                     GetBot()->GetName(), GetBot()->GetClass(), _currentCombatTarget->GetName());

        // DIAGNOSTIC: Check if this is actually the derived class
        const char* className = typeid(*this).name();
        TC_LOG_ERROR("module.playerbot", "🔍 AI Type: {}", className);

        // FIX FOR ISSUE #3: Ensure melee bots continuously face their target
        // This prevents the "facing wrong direction" bug where melee bots don't attack
        // NOTE: Combat movement is now handled by CombatMovementStrategy for role-based positioning
        float optimalRange = GetOptimalRange(_currentCombatTarget);
        if (optimalRange <= 5.0f) // Melee range
        {
            GetBot()->SetFacingToObject(_currentCombatTarget);
        }

        // NOTE: Combat movement (chase, positioning, range management) is delegated to
        // CombatMovementStrategy which provides superior role-based positioning.
        // ClassAI now focuses solely on ability rotation and cooldown management.

        TC_LOG_ERROR("module.playerbot", "⏩ About to call UpdateRotation() virtual method");
        TC_LOG_ERROR("module.playerbot", "⏩ Target valid: {}, target name: {}",
                     _currentCombatTarget != nullptr,
                     _currentCombatTarget ? _currentCombatTarget->GetName() : "NULL");

        // Update class-specific rotation
        try
        {
            TC_LOG_ERROR("module.playerbot", "⏩⏩⏩ INSIDE TRY BLOCK - calling UpdateRotation");
            UpdateRotation(_currentCombatTarget);
            TC_LOG_ERROR("module.playerbot", "⏩⏩⏩ UpdateRotation call completed without exception");
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot", "❌❌❌ EXCEPTION in UpdateRotation: {}", e.what());
        }
        catch (...)
        {
            TC_LOG_ERROR("module.playerbot", "❌❌❌ UNKNOWN EXCEPTION in UpdateRotation");
        }

        TC_LOG_ERROR("module.playerbot", "✅ Returned from UpdateRotation()");

        // Update class-specific cooldowns
        UpdateCooldowns(diff);
    }
    else
    {
        TC_LOG_ERROR("module.playerbot", "⚠️ NO TARGET in combat for {}, applying buffs instead", GetBot()->GetName());

        // No target in combat - try to apply buffs
        UpdateBuffs();
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

    // Notify combat behavior system
    if (_combatBehaviors)
    {
        _combatBehaviors->OnCombatStart();
        TC_LOG_DEBUG("playerbot.classai", "CombatBehaviorIntegration notified of combat start for bot {}",
                     GetBot()->GetName());
    }

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

    // Notify combat behavior system
    if (_combatBehaviors)
    {
        _combatBehaviors->OnCombatEnd();
        TC_LOG_DEBUG("playerbot.classai", "CombatBehaviorIntegration notified of combat end for bot {}",
                     GetBot()->GetName());
    }

    // Let BotAI handle base combat end logic
    BotAI::OnCombatEnd();
}

void ClassAI::OnTargetChanged(::Unit* newTarget)
{
    _currentCombatTarget = newTarget;
    _lastTargetSwitch = _combatTime;

    TC_LOG_DEBUG("playerbot.classai", "Bot {} switching target to {}",
                 GetBot()->GetName(), newTarget ? newTarget->GetName() : "none");

    // FIX FOR ISSUE #3: Explicitly set facing for melee combat
    // Melee bots must face their target to attack properly
    if (newTarget && GetBot())
    {
        float optimalRange = GetOptimalRange(newTarget);

        // Melee classes (optimal range <= 5 yards) need to face target
        if (optimalRange <= 5.0f)
        {
            GetBot()->SetFacingToObject(newTarget);
            TC_LOG_TRACE("module.playerbot.classai",
                "Bot {} (melee) now facing target {} (FIX FOR ISSUE #3)",
                GetBot()->GetName(), newTarget->GetName());
        }
    }
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

    // Priority 2: Group leader's target (FIX FOR ISSUE #2)
    // Bots should assist the group leader's target for coordinated combat
    if (Group* group = GetBot()->GetGroup())
    {
        ObjectGuid leaderGuid = group->GetLeaderGUID();

        // Find leader in group members (avoid ObjectAccessor for thread safety)
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                if (member->GetGUID() == leaderGuid)
                {
                    // Found leader - get their target
                    if (::Unit* leaderTarget = member->GetVictim())
                    {
                        if (GetBot()->IsValidAttackTarget(leaderTarget))
                        {
                            TC_LOG_TRACE("module.playerbot.classai",
                                "Bot {} assisting leader {} target: {} (FIX FOR ISSUE #2)",
                                GetBot()->GetName(), member->GetName(), leaderTarget->GetName());
                            return leaderTarget;
                        }
                    }
                    break;
                }
            }
        }
    }

    // Priority 3: Selected target (if different from victim)
    // FIX #20: Avoid ObjectAccessor::GetUnit() to prevent TrinityCore deadlock
    // Check if bot has a selected target that's not the current victim
    ObjectGuid targetGuid = GetBot()->GetTarget();
    if (!targetGuid.IsEmpty())
    {
        // Check if victim matches selected target (no ObjectAccessor needed)
        if (::Unit* victim = GetBot()->GetVictim())
        {
            if (victim->GetGUID() == targetGuid && GetBot()->IsValidAttackTarget(victim))
                return victim;
        }
        // Selected target is different from victim - skip to avoid ObjectAccessor call
        // GetNearestEnemy() will handle finding a new target
    }

    // Priority 4: Nearest hostile
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
    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
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

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    auto remaining = GetBot()->GetSpellHistory()->GetRemainingCooldown(spellInfo);
    return remaining.count();
}

// ============================================================================
// SPELL QUEUEING SYSTEM - Enterprise-grade spell casting
// ============================================================================
// Mirrors TrinityCore's player spell queueing for proper validation timing
// See Player.cpp:30904-31051 for reference implementation

bool ClassAI::RequestBotSpellCast(uint32 spellId, ::Unit* target)
{
    if (!GetBot())
        return false;

    // Get spell info for validation
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        TC_LOG_TRACE("module.playerbot.classai", "Bot {} RequestBotSpellCast: Invalid spell ID {}",
                    GetBot()->GetName(), spellId);
        return false;
    }

    // Check if we can queue this spell (GCD, current cast, etc.)
    // Mirrors Player::CanRequestSpellCast()
    if (!CanRequestBotSpellCast(spellId))
    {
        TC_LOG_TRACE("module.playerbot.classai", "Bot {} cannot queue spell {} - GCD/cast time > 400ms",
                    GetBot()->GetName(), spellId);
        return false;
    }

    // Cancel any existing pending spell (like players - only one queued at a time)
    if (_pendingSpellCastRequest)
    {
        TC_LOG_TRACE("module.playerbot.classai", "Bot {} canceling previous pending spell {} to queue {}",
                    GetBot()->GetName(), _pendingSpellCastRequest->spellId, spellId);
        CancelPendingSpell();
    }

    // Queue the new spell
    _pendingSpellCastRequest = std::make_unique<BotSpellCastRequest>(spellId, target);

    TC_LOG_DEBUG("module.playerbot.classai", "Bot {} queued spell {} targeting {}",
                GetBot()->GetName(), spellId,
                target ? target->GetName() : "self");

    // Try to execute immediately if conditions are met
    // Mirrors Player::RequestSpellCast() behavior
    if (CanExecutePendingSpell())
    {
        ExecutePendingSpell();
    }

    return true;
}

bool ClassAI::CanRequestBotSpellCast(uint32 spellId) const
{
    if (!GetBot())
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check global cooldown (must be ≤400ms remaining to queue)
    // Mirrors Player::CanRequestSpellCast() - Player.cpp:30935-30946
    if (GetBot()->GetSpellHistory()->GetRemainingGlobalCooldown(spellInfo) > Milliseconds(SPELL_QUEUE_TIME_WINDOW_MS))
    {
        TC_LOG_TRACE("module.playerbot.classai", "Bot {} CanRequestBotSpellCast: GCD > 400ms for spell {}",
                    GetBot()->GetName(), spellId);
        return false;
    }

    // Check current spell casts (must be ≤400ms remaining to queue)
    for (CurrentSpellTypes spellSlot : { CURRENT_MELEE_SPELL, CURRENT_GENERIC_SPELL })
    {
        if (Spell const* spell = GetBot()->GetCurrentSpell(spellSlot))
        {
            if (Milliseconds(spell->GetRemainingCastTime()) > Milliseconds(SPELL_QUEUE_TIME_WINDOW_MS))
            {
                TC_LOG_TRACE("module.playerbot.classai", "Bot {} CanRequestBotSpellCast: Spell cast time > 400ms for spell {}",
                            GetBot()->GetName(), spellId);
                return false;
            }
        }
    }

    return true;
}

bool ClassAI::CanExecutePendingSpell() const
{
    // DIAGNOSTIC: Log every check to trace execution flow
    if (!_pendingSpellCastRequest)
    {
        TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: NO PENDING REQUEST");
        return false;
    }

    TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: Bot {} has pending spell {} queued",
                GetBot() ? GetBot()->GetName() : "NULL", _pendingSpellCastRequest->spellId);

    if (!GetBot())
    {
        TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: NO BOT");
        return false;
    }

    // CRITICAL FIX: Don't check UNIT_STATE_CASTING for bots
    // Unlike players who have packet-driven spell casting, bots queue spells
    // and then ExecutePendingSpell() calls Spell::prepare() which sets the state.
    // Checking for UNIT_STATE_CASTING creates a catch-22 where the bot waits
    // forever for a spell to finish that never started.
    //
    // For bots, we only need to check:
    // 1. Is there actually a spell currently being cast? (GetCurrentSpell)
    // 2. Is GCD ready?
    //
    // If no current spell and GCD is ready, execute the pending spell.

    // Check if bot is currently casting a different spell
    if (Spell const* currentSpell = GetBot()->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: Bot {} CURRENTLY CASTING spell {}, waiting",
                    GetBot()->GetName(), currentSpell->GetSpellInfo()->Id);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: Bot {} NOT casting, checking GCD", GetBot()->GetName());

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_pendingSpellCastRequest->spellId,
                                                          GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: INVALID SPELL INFO for spell {}",
                    _pendingSpellCastRequest->spellId);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: Spell info valid, checking GCD");

    // Wait for global cooldown to expire completely (not just ≤400ms)
    auto gcdRemaining = GetBot()->GetSpellHistory()->GetRemainingGlobalCooldown(spellInfo);
    if (gcdRemaining > 0ms)
    {
        TC_LOG_ERROR("module.playerbot.classai", "🔍 CanExecutePendingSpell: Bot {} GCD NOT READY ({} ms remaining) for spell {}",
                    GetBot()->GetName(), gcdRemaining.count(), _pendingSpellCastRequest->spellId);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.classai", "✅ CanExecutePendingSpell: Bot {} READY TO EXECUTE spell {}",
                GetBot()->GetName(), _pendingSpellCastRequest->spellId);

    return true;
}

void ClassAI::ExecutePendingSpell()
{
    if (!_pendingSpellCastRequest || !GetBot())
        return;

    Player* bot = GetBot();
    uint32 spellId = _pendingSpellCastRequest->spellId;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        TC_LOG_ERROR("module.playerbot.classai", "Bot {} ExecutePendingSpell: Invalid spell ID {}",
                    bot->GetName(), spellId);
        CancelPendingSpell();
        return;
    }

    // Validate target is still valid
    ::Unit* target = _pendingSpellCastRequest->target;
    if (!_pendingSpellCastRequest->isSelfCast)
    {
        if (!target || !target->IsInWorld() || target->isDead())
        {
            TC_LOG_DEBUG("module.playerbot.classai", "Bot {} ExecutePendingSpell: Target invalid for spell {}, canceling",
                        bot->GetName(), spellId);
            CancelPendingSpell();
            return;
        }
    }
    else
    {
        target = bot; // Self-cast
    }

    // CRITICAL: Face the target before casting (required for spell validation)
    // Players auto-face when casting, bots need to do it explicitly
    if (target && target != bot)
    {
        bot->SetFacingToObject(target);
        TC_LOG_ERROR("module.playerbot.classai", "🎯 Bot {} facing target {} before spell cast",
                    bot->GetName(), target->GetName());
    }

    // Create spell cast targets
    // Mirrors Player::ExecutePendingSpellCastRequest() - Player.cpp:30948-31051
    SpellCastTargets targets;
    targets.SetUnitTarget(target);

    // CRITICAL: Use TRIGGERED_NONE - no flags, proper validation timing
    // This is the KEY difference from the workaround approach
    // Target validation happens in Spell::prepare(), not when queuing
    TriggerCastFlags triggerFlags = TRIGGERED_NONE;

    // Create Spell object (exactly like players do)
    // See Player.cpp:31038 - Spell* spell = new Spell(castingUnit, spellInfo, triggerFlag);
    Spell* spell = new Spell(bot, spellInfo, triggerFlags);

    // Prepare the spell (this is where proper validation happens at the right time)
    // See Player.cpp:31048 - spell->prepare(targets);
    // This will handle:
    // - Resource consumption
    // - Target validation at execute time (not queue time!)
    // - Range/LOS checks
    // - Cast time processing
    // - Combat state management
    SpellCastResult result = spell->prepare(targets);

    uint32 queuedDuration = getMSTime() - _pendingSpellCastRequest->queuedAtTime;

    if (result == SPELL_CAST_OK)
    {
        TC_LOG_ERROR("module.playerbot.classai", "✅ Bot {} executed queued spell {} on {} - queued for {}ms",
                    bot->GetName(), spellId,
                    target ? target->GetName() : "self", queuedDuration);
    }
    else
    {
        TC_LOG_ERROR("module.playerbot.classai", "⚠️ Bot {} spell {} failed with result {} - queued for {}ms",
                    bot->GetName(), spellId,
                    uint32(result), queuedDuration);
    }

    // Clear pending request
    _pendingSpellCastRequest = nullptr;
}

void ClassAI::CancelPendingSpell()
{
    if (_pendingSpellCastRequest && GetBot())
    {
        TC_LOG_TRACE("module.playerbot.classai", "Bot {} canceled pending spell {}",
                    GetBot()->GetName(), _pendingSpellCastRequest->spellId);
    }

    _pendingSpellCastRequest = nullptr;
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
    return GetBot() && GetBot()->isMoving();
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
    float angle = GetBot()->GetRelativeAngle(target);

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

// ============================================================================
// COMBAT BEHAVIOR INTEGRATION
// ============================================================================

bool ClassAI::ExecuteRecommendedAction(const RecommendedAction& action)
{
    if (!GetBot() || !action.target || action.spellId == 0)
    {
        TC_LOG_TRACE("playerbot.classai", "ExecuteRecommendedAction: Invalid parameters - bot={}, target={}, spell={}",
                     GetBot() ? "valid" : "null", action.target ? "valid" : "null", action.spellId);
        return false;
    }

    // Log the recommended action execution attempt
    TC_LOG_DEBUG("playerbot.classai", "Bot {} executing {} action: spell {} on {} (reason: {})",
                 GetBot()->GetName(), GetActionName(action.type), action.spellId,
                 action.target->GetName(), action.reason.c_str());

    // Check if the spell can be used
    if (!IsSpellUsable(action.spellId))
    {
        TC_LOG_TRACE("playerbot.classai", "Bot {} cannot use spell {} - not usable",
                     GetBot()->GetName(), action.spellId);
        return false;
    }

    // Check range to target
    if (!IsInRange(action.target, action.spellId))
    {
        TC_LOG_TRACE("playerbot.classai", "Bot {} cannot cast spell {} - target out of range",
                     GetBot()->GetName(), action.spellId);

        // For movement-related actions, we might want to move closer
        if (action.type == CombatActionType::MOVEMENT)
        {
            // Movement is handled by BotAI strategies, just log the need if position is valid
            if (action.position.m_positionX != 0.0f || action.position.m_positionY != 0.0f)
            {
                TC_LOG_DEBUG("playerbot.classai", "Bot {} needs to move to position ({}, {}, {}) for action",
                             GetBot()->GetName(), action.position.m_positionX,
                             action.position.m_positionY, action.position.m_positionZ);
            }
        }
        return false;
    }

    // Check line of sight
    if (!HasLineOfSight(action.target))
    {
        TC_LOG_TRACE("playerbot.classai", "Bot {} cannot cast spell {} - no line of sight",
                     GetBot()->GetName(), action.spellId);
        return false;
    }

    // Handle different action types with appropriate logic
    bool success = false;
    switch (action.type)
    {
        case CombatActionType::INTERRUPT:
        {
            // Interrupt requires special handling - face target quickly
            GetBot()->SetFacingToObject(action.target);
            success = CastSpell(action.target, action.spellId);
            if (success)
            {
                TC_LOG_INFO("playerbot.classai", "Bot {} successfully interrupted {} with spell {}",
                            GetBot()->GetName(), action.target->GetName(), action.spellId);
            }
            break;
        }

        case CombatActionType::DEFENSIVE:
        {
            // Defensive actions often target self or allies
            Unit* defTarget = action.target == GetBot() ? nullptr : action.target;
            if (defTarget)
                success = CastSpell(defTarget, action.spellId);
            else
                success = CastSpell(action.spellId);  // Self-cast

            if (success)
            {
                TC_LOG_INFO("playerbot.classai", "Bot {} activated defensive ability {} on {}",
                            GetBot()->GetName(), action.spellId,
                            defTarget ? defTarget->GetName() : "self");
            }
            break;
        }

        case CombatActionType::CROWD_CONTROL:
        {
            // CC requires careful targeting
            if (action.target != _currentCombatTarget)  // Don't CC our main target
            {
                success = CastSpell(action.target, action.spellId);
                if (success)
                {
                    TC_LOG_INFO("playerbot.classai", "Bot {} applied crowd control {} to {}",
                                GetBot()->GetName(), action.spellId, action.target->GetName());
                }
            }
            break;
        }

        case CombatActionType::EMERGENCY:
        {
            // Emergency actions are highest priority - try to force cast
            success = CastSpell(action.target, action.spellId);
            if (success)
            {
                TC_LOG_WARN("playerbot.classai", "Bot {} executed EMERGENCY action: {} on {}",
                            GetBot()->GetName(), action.spellId, action.target->GetName());
            }
            break;
        }

        case CombatActionType::COOLDOWN:
        {
            // Major cooldowns
            success = CastSpell(action.target, action.spellId);
            if (success)
            {
                TC_LOG_INFO("playerbot.classai", "Bot {} activated cooldown {} on {}",
                            GetBot()->GetName(), action.spellId, action.target->GetName());
            }
            break;
        }

        case CombatActionType::TARGET_SWITCH:
        {
            // Target switch is handled by OnTargetChanged, just validate
            if (action.target && action.target != _currentCombatTarget)
            {
                OnTargetChanged(action.target);
                success = true;
                TC_LOG_INFO("playerbot.classai", "Bot {} switched target to {}",
                            GetBot()->GetName(), action.target->GetName());
            }
            break;
        }

        case CombatActionType::CONSUMABLE:
        {
            // Consumables (potions, healthstones, etc.)
            // These typically don't have a target or target self
            success = CastSpell(action.spellId);
            if (success)
            {
                TC_LOG_INFO("playerbot.classai", "Bot {} used consumable {}",
                            GetBot()->GetName(), action.spellId);
            }
            break;
        }

        case CombatActionType::MOVEMENT:
        {
            // Movement is handled by BotAI strategies, log the request
            TC_LOG_DEBUG("playerbot.classai", "Bot {} requested movement action to ({}, {}, {})",
                         GetBot()->GetName(), action.position.m_positionX,
                         action.position.m_positionY, action.position.m_positionZ);
            // Return true to indicate the request was acknowledged
            success = true;
            break;
        }

        case CombatActionType::ROTATION:
        default:
        {
            // Normal rotation ability
            success = CastSpell(action.target, action.spellId);
            if (success)
            {
                TC_LOG_TRACE("playerbot.classai", "Bot {} cast rotation spell {} on {}",
                             GetBot()->GetName(), action.spellId, action.target->GetName());
            }
            break;
        }
    }

    // Record metrics if successful
    if (success)
    {
        RecordPerformanceMetric("recommended_action_success", 1);
    }
    else
    {
        RecordPerformanceMetric("recommended_action_fail", 1);
        TC_LOG_TRACE("playerbot.classai", "Bot {} failed to execute {} action: {} on {}",
                     GetBot()->GetName(), GetActionName(action.type), action.spellId,
                     action.target->GetName());
    }

    return success;
}

} // namespace Playerbot