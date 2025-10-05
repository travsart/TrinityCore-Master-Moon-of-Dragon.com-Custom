/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * BotMovementUtil - Centralized Movement Deduplication Implementation
 */

#include "BotMovementUtil.h"
#include "Log.h"

namespace Playerbot
{

bool BotMovementUtil::MoveToPosition(Player* bot, Position const& destination, uint32 pointId, float minDistanceChange)
{
    if (!bot)
        return false;

    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
    {
        TC_LOG_ERROR("module.playerbot", "BotMovementUtil::MoveToPosition: Bot {} has NULL MotionMaster", bot->GetName());
        return false;
    }

    // FIX: Check if bot is already moving to this exact destination
    // Prevents re-issuing movement every frame which cancels the previous movement
    if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) == POINT_MOTION_TYPE)
    {
        // Bot is already moving via MovePoint - check if it's the same destination
        float distToDestination = bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY());

        if (distToDestination > minDistanceChange)
        {
            // Different destination - issue new movement
            TC_LOG_DEBUG("module.playerbot", "🔄 BotMovement: Bot {} changing destination to ({:.2f},{:.2f},{:.2f})",
                         bot->GetName(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
            mm->MovePoint(pointId, destination);
            return true;
        }
        else
        {
            // Already moving to same destination - don't re-issue command
            TC_LOG_DEBUG("module.playerbot", "⏭️ BotMovement: Bot {} already moving to destination, skipping", bot->GetName());
            return true;
        }
    }
    else
    {
        // Not currently in point movement - issue new command
        TC_LOG_DEBUG("module.playerbot", "🎯 BotMovement: Bot {} moving to ({:.2f},{:.2f},{:.2f})",
                     bot->GetName(), destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
        mm->MovePoint(pointId, destination);
        return true;
    }
}

bool BotMovementUtil::MoveToTarget(Player* bot, WorldObject* target, uint32 pointId, float minDistanceChange)
{
    if (!bot || !target)
        return false;

    Position destination;
    destination.Relocate(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());

    return MoveToPosition(bot, destination, pointId, minDistanceChange);
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
        float currentDistance = bot->GetDistance(target);
        float tolerance = 2.0f;

        if (std::abs(currentDistance - distance) <= tolerance)
        {
            TC_LOG_DEBUG("module.playerbot", "⏭️ BotMovement: Bot {} already chasing at optimal distance", bot->GetName());
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

    TC_LOG_DEBUG("module.playerbot", "🛑 BotMovement: Bot {} stopped movement", bot->GetName());
}

bool BotMovementUtil::IsMoving(Player* bot)
{
    if (!bot)
        return false;

    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
        return false;

    MovementGeneratorType type = mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE);
    return (type == POINT_MOTION_TYPE || type == CHASE_MOTION_TYPE || type == FOLLOW_MOTION_TYPE);
}

bool BotMovementUtil::IsMovingToDestination(Player* bot, Position const& destination, float tolerance)
{
    if (!bot)
        return false;

    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm)
        return false;

    if (mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE) != POINT_MOTION_TYPE)
        return false;

    float distToDestination = bot->GetExactDist2d(destination.GetPositionX(), destination.GetPositionY());
    return (distToDestination <= tolerance);
}

} // namespace Playerbot
