// Modified UpdateBehavior method using safe references
// This replaces the existing UpdateBehavior method in LeaderFollowBehavior.cpp

void LeaderFollowBehavior::UpdateBehavior(uint32 diff, BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // ========================================================================
    // FIX FOR ISSUE #4: Use safe reference to get leader
    // ========================================================================

    // NEW: Use safe reference from BotAI instead of raw pointer
    Player* leader = ai->GetGroupLeader();

    if (!leader)
    {
        // Leader logged out or no longer exists - safe!
        TC_LOG_DEBUG("module.playerbot.follow",
            "Bot {} has no valid leader to follow",
            bot->GetName());

        // Clear follow target if leader is gone
        ClearFollowTarget();
        return;
    }

    // Leader exists and is valid - continue with follow logic
    SetFollowTarget(leader);

    // Check if bot is already following
    bool isFollowing = bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE;

    // Calculate distance to leader
    float distance = bot->GetExactDist(leader);
    _distanceToLeader = distance;

    // Check if leader is in sight
    bool inSight = bot->IsWithinLOSInMap(leader);
    if (inSight)
    {
        _leaderInSight = true;
        _lastSeenTime = getMSTime();
    }
    else
    {
        _leaderInSight = false;
    }

    // Determine if we need to follow
    bool shouldFollow = false;
    float followDistance = _followDistance;

    // Adjust follow distance based on situation
    if (leader->IsInCombat())
    {
        followDistance = 15.0f; // Stay closer in combat
    }
    else if (bot->HasUnitState(UNIT_STATE_CASTING))
    {
        followDistance = 30.0f; // Can be further when casting
    }

    // Check if we need to start following
    if (distance > followDistance + 2.0f) // Add buffer to prevent jitter
    {
        shouldFollow = true;
    }
    else if (distance < followDistance - 2.0f && isFollowing)
    {
        // Too close, stop following
        shouldFollow = false;
    }
    else if (isFollowing)
    {
        // Within acceptable range and already following, continue
        shouldFollow = true;
    }

    // Update movement
    if (shouldFollow && !isFollowing)
    {
        // Start following
        bot->GetMotionMaster()->MoveFollow(leader, followDistance, M_PI_F / 2);
        TC_LOG_DEBUG("module.playerbot.follow",
            "Bot {} started following leader {} at distance {}",
            bot->GetName(), leader->GetName(), distance);
    }
    else if (!shouldFollow && isFollowing)
    {
        // Stop following
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveIdle();
        TC_LOG_DEBUG("module.playerbot.follow",
            "Bot {} stopped following leader {} (distance: {})",
            bot->GetName(), leader->GetName(), distance);
    }

    // Handle combat synchronization
    if (leader->IsInCombat() && !bot->IsInCombat())
    {
        // Leader is in combat, bot should assist
        if (Unit* target = leader->GetVictim())
        {
            if (bot->IsValidAttackTarget(target))
            {
                bot->Attack(target, true);
                TC_LOG_DEBUG("module.playerbot.follow",
                    "Bot {} assisting leader {} against target {}",
                    bot->GetName(), leader->GetName(), target->GetName());
            }
        }
    }

    // Handle lost leader
    if (!inSight && GetTimeSinceLastSeen() > 5000)
    {
        // Haven't seen leader in 5 seconds, try to teleport
        if (distance > 100.0f)
        {
            bot->NearTeleportTo(
                leader->GetPositionX() + frand(-5.0f, 5.0f),
                leader->GetPositionY() + frand(-5.0f, 5.0f),
                leader->GetPositionZ(),
                leader->GetOrientation()
            );
            TC_LOG_INFO("module.playerbot.follow",
                "Bot {} teleported to leader {} (was {} yards away)",
                bot->GetName(), leader->GetName(), distance);
        }
    }
}