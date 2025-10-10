/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "SoloCombatStrategy.h"
#include "../BotAI.h"
#include "../ClassAI/ClassAI.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "Log.h"
#include "MotionMaster.h"

namespace Playerbot
{

// ============================================================================
// CONSTRUCTOR / INITIALIZATION
// ============================================================================

SoloCombatStrategy::SoloCombatStrategy()
    : Strategy("solo_combat")
{
    TC_LOG_DEBUG("module.playerbot.strategy", "SoloCombatStrategy: Initialized");
}

void SoloCombatStrategy::InitializeActions()
{
    // No actions needed - ClassAI handles combat execution
    // This strategy only provides positioning coordination
    TC_LOG_DEBUG("module.playerbot.strategy", "SoloCombatStrategy: No actions (ClassAI handles combat)");
}

void SoloCombatStrategy::InitializeTriggers()
{
    // No triggers needed - relevance system handles activation
    // IsActive() and GetRelevance() provide reactive activation
    TC_LOG_DEBUG("module.playerbot.strategy", "SoloCombatStrategy: No triggers (using relevance system)");
}

void SoloCombatStrategy::InitializeValues()
{
    // No values needed for this coordination strategy
    TC_LOG_DEBUG("module.playerbot.strategy", "SoloCombatStrategy: No values");
}

// ============================================================================
// ACTIVATION LOGIC
// ============================================================================

bool SoloCombatStrategy::IsActive(BotAI* ai) const
{
    // Null safety checks
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // NOT active if bot is in a group
    // Grouped bots use GroupCombatStrategy instead
    if (bot->GetGroup())
        return false;

    // Active when:
    // 1. Strategy is explicitly activated (_active flag)
    // 2. Bot is solo (not in group - checked above)
    // 3. Bot is in combat (bot->IsInCombat() = true)
    bool active = _active.load();
    bool inCombat = bot->IsInCombat();

    // Diagnostic logging (throttled via static counter)
    static uint32 logCounter = 0;
    if ((++logCounter % 100) == 0)  // Every 100 calls (~5 seconds)
    {
        TC_LOG_DEBUG("module.playerbot.strategy",
            "SoloCombatStrategy::IsActive: Bot {} - active={}, inCombat={}, hasGroup={}, result={}",
            bot->GetName(), active, inCombat, bot->GetGroup() != nullptr, (active && inCombat));
    }

    return active && inCombat;
}

float SoloCombatStrategy::GetRelevance(BotAI* ai) const
{
    // Null safety checks
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // Not relevant if in a group (GroupCombatStrategy handles that)
    if (bot->GetGroup())
        return 0.0f;

    // Not relevant if not in combat
    if (!bot->IsInCombat())
        return 0.0f;

    // HIGH PRIORITY when solo and in combat
    // Priority 70 = between GroupCombat (80) and Quest (50)
    // This ensures combat takes priority over all non-combat activities
    return 70.0f;
}

// ============================================================================
// MAIN UPDATE LOGIC
// ============================================================================

void SoloCombatStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    // CRITICAL: This is called EVERY FRAME when strategy is active
    // Performance target: <0.1ms per call
    // Only positioning logic - ClassAI handles spell rotation

    // Null safety checks
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Validate combat state
    if (!bot->IsInCombat())
    {
        TC_LOG_TRACE("module.playerbot.strategy",
            "SoloCombatStrategy: Bot {} not in combat, strategy should deactivate",
            bot->GetName());
        return;
    }

    // Get current combat target
    Unit* target = bot->GetVictim();

    if (!target)
    {
        TC_LOG_DEBUG("module.playerbot.strategy",
            "SoloCombatStrategy: Bot {} in combat but no victim, waiting for target",
            bot->GetName());
        return;
    }

    // Validate target is alive and attackable
    if (!target->IsAlive())
    {
        TC_LOG_DEBUG("module.playerbot.strategy",
            "SoloCombatStrategy: Bot {} target {} is dead, combat should end",
            bot->GetName(), target->GetName());
        return;
    }

    // ========================================================================
    // POSITIONING LOGIC - Move to optimal combat range
    // ========================================================================

    // Get optimal range for this bot's class/spec
    float optimalRange = GetOptimalCombatRange(ai, target);
    float currentDistance = bot->GetDistance(target);

    // Diagnostic logging (throttled)
    static uint32 updateCounter = 0;
    bool shouldLog = ((++updateCounter % 50) == 0);  // Every 50 calls (~2.5 seconds)

    if (shouldLog)
    {
        TC_LOG_DEBUG("module.playerbot.strategy",
            "SoloCombatStrategy: Bot {} engaging {} - distance={:.1f}yd, optimal={:.1f}yd",
            bot->GetName(), target->GetName(), currentDistance, optimalRange);
    }

    // Check if bot is already chasing target
    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
    {
        TC_LOG_ERROR("module.playerbot.strategy",
            "SoloCombatStrategy: Bot {} has no MotionMaster!",
            bot->GetName());
        return;
    }

    MovementGeneratorType currentMotion = mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE);

    // DIAGNOSTIC: Log current motion type every time to detect conflicts
    TC_LOG_ERROR("module.playerbot.strategy",
        "🔍 SoloCombatStrategy: Bot {} motion check - currentMotion={} ({}), distance={:.1f}yd, optimal={:.1f}yd",
        bot->GetName(),
        static_cast<uint32>(currentMotion),
        currentMotion == CHASE_MOTION_TYPE ? "CHASE" :
        currentMotion == FOLLOW_MOTION_TYPE ? "FOLLOW" :
        currentMotion == POINT_MOTION_TYPE ? "POINT" :
        currentMotion == IDLE_MOTION_TYPE ? "IDLE" : "OTHER",
        currentDistance, optimalRange);

    // CRITICAL FIX: Only issue MoveChase if NOT already chasing
    // Re-issuing every frame causes speed-up and blinking issues
    if (currentMotion != CHASE_MOTION_TYPE)
    {
        // Start chasing target at optimal range
        mm->MoveChase(target, optimalRange);

        TC_LOG_ERROR("module.playerbot.strategy",
            "⚔️ SoloCombatStrategy: Bot {} STARTED CHASING {} at {:.1f}yd range (was motion type {})",
            bot->GetName(), target->GetName(), optimalRange, static_cast<uint32>(currentMotion));
    }
    else
    {
        // Already chasing - just let it continue
        // MotionMaster will handle distance adjustments automatically
        TC_LOG_ERROR("module.playerbot.strategy",
            "✅ SoloCombatStrategy: Bot {} ALREADY CHASING {} (distance {:.1f}/{:.1f}yd) - skipping MoveChase",
            bot->GetName(), target->GetName(), currentDistance, optimalRange);
    }

    // ========================================================================
    // SPELL EXECUTION - Delegated to ClassAI
    // ========================================================================
    // ClassAI::OnCombatUpdate() is called by BotAI::UpdateAI() when IsInCombat()
    // We don't call it here - just ensure positioning is correct
    // ClassAI will handle rotation, cooldowns, targeting, spell casting
}

// ============================================================================
// HELPER METHODS
// ============================================================================

float SoloCombatStrategy::GetOptimalCombatRange(BotAI* ai, Unit* target) const
{
    if (!ai || !ai->GetBot() || !target)
        return 5.0f;  // Default to melee range

    Player* bot = ai->GetBot();

    // PREFERRED: Get optimal range from ClassAI if available
    // ClassAI knows the bot's spec and can provide spec-specific ranges
    // Example: Feral Druid = melee, Balance Druid = ranged
    if (ClassAI* classAI = dynamic_cast<ClassAI*>(ai))
    {
        float classOptimalRange = classAI->GetOptimalRange(target);

        TC_LOG_TRACE("module.playerbot.strategy",
            "SoloCombatStrategy: Bot {} using ClassAI optimal range {:.1f}yd for class {}",
            bot->GetName(), classOptimalRange, bot->GetClass());

        return classOptimalRange;
    }

    // FALLBACK: Class-based default ranges
    // Used when ClassAI is not available or doesn't override GetOptimalRange()
    switch (bot->GetClass())
    {
        // Ranged classes - 25 yard optimal range
        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
            TC_LOG_TRACE("module.playerbot.strategy",
                "SoloCombatStrategy: Bot {} using default RANGED range 25.0yd for class {}",
                bot->GetName(), bot->GetClass());
            return 25.0f;

        // Melee classes - 5 yard melee range
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_MONK:
            TC_LOG_TRACE("module.playerbot.strategy",
                "SoloCombatStrategy: Bot {} using default MELEE range 5.0yd for class {}",
                bot->GetName(), bot->GetClass());
            return 5.0f;

        // Hybrid classes - depends on spec, but default to melee
        // ClassAI should handle spec-specific logic for these
        case CLASS_DRUID:      // Feral=melee, Balance=ranged (ClassAI determines)
        case CLASS_SHAMAN:     // Enhancement=melee, Elemental=ranged (ClassAI determines)
        case CLASS_DEMON_HUNTER:  // Melee primarily
        case CLASS_EVOKER:     // Ranged primarily
        default:
            TC_LOG_TRACE("module.playerbot.strategy",
                "SoloCombatStrategy: Bot {} using default MELEE range 5.0yd for hybrid/unknown class {}",
                bot->GetName(), bot->GetClass());
            return 5.0f;
    }
}

} // namespace Playerbot
