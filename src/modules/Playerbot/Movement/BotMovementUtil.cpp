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

    // MINE/CAVE FIX: Use 3D distance to properly detect vertical distance
    // Previously used 2D distance which caused bots to think they were "at destination"
    // when standing at mine entrance but spawn is directly below at different Z
    float distToDestination2D = bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY());
    float distToDestination3D = bot->GetExactDist(destination);
    float zDifference = std::abs(bot->GetPositionZ() - destination.GetPositionZ());

    // DIAGNOSTIC: Log movement attempts (DEBUG level to avoid spam)
    TC_LOG_DEBUG("module.playerbot.movement", "🔍 MoveToPosition: Bot {} dist2D={:.1f} dist3D={:.1f} zDiff={:.1f} minDist={:.1f} dest=({:.1f},{:.1f},{:.1f})",
                 bot->GetName(), distToDestination2D, distToDestination3D, zDifference, minDistanceChange,
                 destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());

    // MINE/CAVE FIX: Use 3D distance for "already at destination" check
    // This ensures bots will still move if there's significant vertical distance
    // even if horizontal distance is small (e.g., mine entrance above spawn inside)
    if (distToDestination3D < minDistanceChange)
    {
        TC_LOG_DEBUG("module.playerbot.movement", "✅ BotMovement: Bot {} already at destination (3D dist {:.1f}yd < {:.1f})",
                     bot->GetName(), distToDestination3D, minDistanceChange);
        return true;
    }

    // Check if bot is already moving
    MovementGeneratorType currentMoveType = mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE);

    // ========================================================================
    // CRITICAL FIX: Do NOT interrupt CHASE_MOTION_TYPE!
    // ========================================================================
    // When the bot is chasing a target (combat), calling MovePoint() interrupts
    // the chase and causes stuttering as SoloCombatStrategy immediately re-issues
    // MoveChase(). This creates rapid oscillation between CHASE and POINT motion.
    //
    // During combat, SoloCombatStrategy is the ONLY system that should control
    // movement. If MoveToPosition is called during chase, we should NOT interrupt.
    // ========================================================================
    if (currentMoveType == CHASE_MOTION_TYPE)
    {
        TC_LOG_DEBUG("module.playerbot.movement", "⚠️ BotMovement: Bot {} in CHASE mode - NOT INTERRUPTING chase to move to position (let combat strategy handle movement)",
                     bot->GetName());
        return false;  // Indicate that we did NOT start movement - chase continues
    }

    // Similarly, don't interrupt FOLLOW_MOTION_TYPE (following group leader)
    if (currentMoveType == FOLLOW_MOTION_TYPE)
    {
        TC_LOG_DEBUG("module.playerbot.movement", "⚠️ BotMovement: Bot {} in FOLLOW mode - NOT INTERRUPTING follow to move to position",
                     bot->GetName());
        return false;  // Indicate that we did NOT start movement - follow continues
    }

    // If already moving via spline, check if we should interrupt
    if (currentMoveType == POINT_MOTION_TYPE || currentMoveType == EFFECT_MOTION_TYPE)
    {
        // Check if the spline is still active and heading somewhere reasonable
        if (bot->movespline && !bot->movespline->Finalized())
        {
            // BUG FIX: Check if current spline is going to the CORRECT destination!
            // The spline might be going somewhere completely different (e.g., follow target)
            // We need to interrupt and redirect if the destination doesn't match
            G3D::Vector3 const& splineDestination = bot->movespline->FinalDestination();

            // Skip zero destination check (means spline not properly initialized)
            if (splineDestination != G3D::Vector3::zero())
            {
                float splineDestDist = std::sqrt(
                    (splineDestination.x - destination.GetPositionX()) * (splineDestination.x - destination.GetPositionX()) +
                    (splineDestination.y - destination.GetPositionY()) * (splineDestination.y - destination.GetPositionY()) +
                    (splineDestination.z - destination.GetPositionZ()) * (splineDestination.z - destination.GetPositionZ())
                );

                // If spline destination is close to requested destination (< 10 yards), let it continue
                if (splineDestDist < 10.0f)
                {
                    TC_LOG_DEBUG("module.playerbot.movement", "⏭ BotMovement: Bot {} spline ACTIVE going to correct destination (diff={:.1f}yd) - {:.1f}yd to dest (3D)",
                                 bot->GetName(), splineDestDist, distToDestination3D);
                    return true;
                }
                else
                {
                    // Spline is going to WRONG destination - must interrupt and redirect!
                    TC_LOG_DEBUG("module.playerbot.movement", "🔄 BotMovement: Bot {} spline going WRONG DIRECTION! Spline dest ({:.1f},{:.1f},{:.1f}) is {:.1f}yd from requested ({:.1f},{:.1f},{:.1f}) - INTERRUPTING",
                                 bot->GetName(),
                                 splineDestination.x, splineDestination.y, splineDestination.z, splineDestDist,
                                 destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
                    // Fall through to start new movement
                }
            }
            else
            {
                TC_LOG_DEBUG("module.playerbot.movement", "⚠️ BotMovement: Bot {} spline has zero destination, will restart",
                             bot->GetName());
            }
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.movement", "⚠️ BotMovement: Bot {} spline FINALIZED or NULL (moveType={}), will restart - {:.1f}yd to destination (3D)",
                         bot->GetName(), static_cast<int>(currentMoveType), distToDestination3D);
        }
    }
    else
    {
        TC_LOG_DEBUG("module.playerbot.movement", "📍 BotMovement: Bot {} not in POINT/EFFECT motion (moveType={}), will start new movement",
                     bot->GetName(), static_cast<int>(currentMoveType));
    }

    // Start new movement via MotionMaster
    // NOTE: We previously used MoveSplineInit directly for smoother movement, but that caused
    // ASSERTION FAILED: Initialized() crashes in MoveSpline::updateState because MoveSplineInit
    // is NOT thread-safe - it directly manipulates unit->movespline which is also accessed by
    // Unit::Update() on the main thread.
    //
    // MotionMaster::MovePoint() is the safer approach, and we maintain deduplication above
    // by checking if a spline is already active before calling this.
    TC_LOG_DEBUG("module.playerbot.movement", "🚶 BotMovement: Bot {} STARTING MOVEMENT to ({:.2f},{:.2f},{:.2f}) - {:.1f}yd (3D)",
                 bot->GetName(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                 distToDestination3D);

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
