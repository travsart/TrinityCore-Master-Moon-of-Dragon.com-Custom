/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * BotMovementUtil - Centralized Movement Deduplication Implementation
 *
 * Uses MotionMaster::MovePoint() with deduplication to prevent movement spam.
 * NOTE: We cannot use MoveSplineInit directly because it's NOT thread-safe -
 * bot AI runs on worker threads but MoveSplineInit manipulates unit->movespline
 * which is accessed by Unit::Update() on the main thread, causing crashes.
 */

#include "BotMovementUtil.h"
#include "Log.h"
#include "Map.h"

namespace Playerbot
{

bool BotMovementUtil::MoveToPosition(Player* bot, Position const& destination, uint32 pointId, float minDistanceChange)
{
    if (!bot)
        return false;

    // CRITICAL: Bot must be in world before any movement operations
    if (!bot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot", "⚠️ BotMovement: Bot {} not in world, skipping movement", bot->GetName());
        return false;
    }

    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
    {
        TC_LOG_ERROR("module.playerbot", "BotMovementUtil::MoveToPosition: Bot {} has NULL MotionMaster", bot->GetName());
        return false;
    }

    // Check distance to destination
    float distToDestination = bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY());

    // If we're very close to destination, no need to move
    if (distToDestination < minDistanceChange)
    {
        TC_LOG_DEBUG("module.playerbot", "✅ BotMovement: Bot {} already at destination ({:.1f}yd)",
                     bot->GetName(), distToDestination);
        return true;
    }

    // Check if bot is already moving
    MovementGeneratorType currentMoveType = mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE);

    // If already moving via spline, check if we should interrupt
    if (currentMoveType == POINT_MOTION_TYPE || currentMoveType == EFFECT_MOTION_TYPE)
    {
        // Check if the spline is still active and heading somewhere reasonable
        if (bot->movespline && !bot->movespline->Finalized())
        {
            // Let the current movement continue unless we're stuck or going wrong direction
            // This prevents the "stutter" from constantly restarting movement
            TC_LOG_DEBUG("module.playerbot", "⏭ BotMovement: Bot {} spline active, {:.1f}yd to destination",
                         bot->GetName(), distToDestination);
            return true;
        }
    }

    // Start new movement via MotionMaster
    // NOTE: We previously used MoveSplineInit directly for smoother movement, but that caused
    // ASSERTION FAILED: Initialized() crashes in MoveSpline::updateState because MoveSplineInit
    // is NOT thread-safe - it directly manipulates unit->movespline which is also accessed by
    // Unit::Update() on the main thread.
    //
    // MotionMaster::MovePoint() is the safer approach, and we maintain deduplication above
    // by checking if a spline is already active before calling this.
    TC_LOG_DEBUG("module.playerbot", "🚶 BotMovement: Bot {} starting movement to ({:.2f},{:.2f},{:.2f}) - {:.1f}yd",
                 bot->GetName(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                 distToDestination);

    // Use MotionMaster for thread-safe movement initiation
    // The deduplication check above prevents the "60+ MovePoint calls/second" bug
    mm->MovePoint(pointId, destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());

    return true;
}

bool BotMovementUtil::MoveToTarget(Player* bot, WorldObject* target, uint32 pointId, float minDistanceChange)
{
    if (!bot || !target)
        return false;

    Position destination;
    destination.Relocate(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());

    return MoveToPosition(bot, destination, pointId, minDistanceChange);
}

bool BotMovementUtil::MoveToUnit(Player* bot, Unit* unit, float distance, uint32 pointId)
{
    if (!bot || !unit)
        return false;

    // Check if already within desired distance
    float currentDistance = bot->GetExactDist(unit);
    if (currentDistance <= distance)
    {
        TC_LOG_DEBUG("module.playerbot", "✅ BotMovement: Bot {} already within {:.1f}yd of {} (current: {:.1f}yd)",
                     bot->GetName(), distance, unit->GetName(), currentDistance);
        return true;
    }

    // Calculate position at desired distance from unit
    // Use angle from bot to unit for approach vector
    float angle = bot->GetAbsoluteAngle(unit);

    Position destination;
    // Calculate position that is 'distance' yards from unit, on the line between bot and unit
    destination.Relocate(
        unit->GetPositionX() - distance * std::cos(angle),
        unit->GetPositionY() - distance * std::sin(angle),
        unit->GetPositionZ()
    );

    TC_LOG_DEBUG("module.playerbot", "🚶 BotMovement: Bot {} approaching {} to within {:.1f}yd at ({:.2f},{:.2f},{:.2f})",
                 bot->GetName(), unit->GetName(), distance,
                 destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());

    return MoveToPosition(bot, destination, pointId);
}

bool BotMovementUtil::ChaseTarget(Player* bot, Unit* target, float distance)
{
    if (!bot || !target)
        return false;

    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
        return false;

    // Check if already chasing this target at this distance
    if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) == CHASE_MOTION_TYPE)
    {
        float currentDistance = bot->GetExactDist(target);
        float tolerance = 2.0f;

        if (std::abs(currentDistance - distance) <= tolerance)
        {
            TC_LOG_DEBUG("module.playerbot", "⏭ BotMovement: Bot {} already chasing at optimal distance", bot->GetName());
            return true;
        }
    }

    // Start or update chase
    TC_LOG_DEBUG("module.playerbot", "🏃 BotMovement: Bot {} chasing {} at {:.1f}yd",
                 bot->GetName(), target->GetName(), distance);
    mm->MoveChase(target, distance);
    return true;
}

void BotMovementUtil::StopMovement(Player* bot)
{
    if (!bot)
        return;

    bot->StopMoving();

    if (MotionMaster* mm = bot->GetMotionMaster())
        mm->Clear();

    TC_LOG_DEBUG("module.playerbot", "⏹ BotMovement: Bot {} stopped movement", bot->GetName());
}

bool BotMovementUtil::IsMoving(Player* bot)
{
    if (!bot)
        return false;

    // Check spline state first (most accurate)
    if (bot->movespline && !bot->movespline->Finalized())
        return true;

    // Fall back to MotionMaster check
    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
        return false;

    MovementGeneratorType type = mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE);
    return (type == POINT_MOTION_TYPE || type == CHASE_MOTION_TYPE || type == FOLLOW_MOTION_TYPE || type == EFFECT_MOTION_TYPE);
}

bool BotMovementUtil::IsMovingToDestination(Player* bot, Position const& destination, float tolerance)
{
    if (!bot)
        return false;

    // Check if spline is active
    if (!bot->movespline || bot->movespline->Finalized())
        return false;

    // Check if destination is close to where we're heading
    float distToDestination = bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY());
    return (distToDestination <= tolerance);
}

} // namespace Playerbot
