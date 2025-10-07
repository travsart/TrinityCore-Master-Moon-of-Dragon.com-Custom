/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * BotMovementUtil - Centralized Movement Deduplication Utility
 *
 * CRITICAL: ALL bot movement MUST use these functions to prevent
 * the infinite movement cancellation bug (60+ MovePoint calls/second)
 *
 * ROOT CAUSE: Direct MovePoint() calls every frame cancel previous movement
 * SOLUTION: Check if already moving before issuing new movement command
 */

#pragma once

#include "Player.h"
#include "MotionMaster.h"
#include "Position.h"

namespace Playerbot
{

/**
 * BotMovementUtil - Centralized movement functions with deduplication
 *
 * USAGE:
 *   Instead of: bot->GetMotionMaster()->MovePoint(0, destination);
 *   Use:        BotMovementUtil::MoveToPosition(bot, destination);
 *
 * This prevents movement command spam and allows movements to complete.
 */
class TC_GAME_API BotMovementUtil
{
public:
    /**
     * Move bot to specific position with deduplication
     *
     * @param bot Player bot to move
     * @param destination Target position
     * @param pointId Movement point ID (default 0)
     * @param minDistanceChange Only re-issue movement if destination changed by this much (default 0.5 yards)
     * @return true if movement initiated or already in progress
     */
    static bool MoveToPosition(Player* bot, Position const& destination, uint32 pointId = 0, float minDistanceChange = 0.5f);

    /**
     * Move bot to target (position only, no chasing)
     *
     * @param bot Player bot to move
     * @param target Target to move to
     * @param pointId Movement point ID (default 0)
     * @param minDistanceChange Only re-issue if destination changed by this much
     * @return true if movement initiated or already in progress
     */
    static bool MoveToTarget(Player* bot, WorldObject* target, uint32 pointId = 0, float minDistanceChange = 0.5f);

    /**
     * Move bot to within specified distance of unit (for interaction, quest givers, etc.)
     *
     * @param bot Player bot to move
     * @param unit Target unit to approach
     * @param distance Desired distance from unit (e.g., 5.0f for quest giver interaction)
     * @param pointId Movement point ID (default 0)
     * @return true if movement initiated, already in range, or already moving there
     */
    static bool MoveToUnit(Player* bot, Unit* unit, float distance, uint32 pointId = 0);

    /**
     * Chase target at specific distance with deduplication
     *
     * @param bot Player bot to move
     * @param target Target to chase
     * @param distance Distance to maintain from target
     * @return true if chase initiated or already in progress
     */
    static bool ChaseTarget(Player* bot, Unit* target, float distance);

    /**
     * Stop bot movement immediately
     *
     * @param bot Player bot to stop
     */
    static void StopMovement(Player* bot);

    /**
     * Check if bot is currently moving
     *
     * @param bot Player bot to check
     * @return true if bot has active movement
     */
    static bool IsMoving(Player* bot);

    /**
     * Check if bot is moving to specific destination
     *
     * @param bot Player bot to check
     * @param destination Target position
     * @param tolerance Distance tolerance (default 1.0 yard)
     * @return true if bot is already moving to this destination
     */
    static bool IsMovingToDestination(Player* bot, Position const& destination, float tolerance = 1.0f);
};

} // namespace Playerbot
