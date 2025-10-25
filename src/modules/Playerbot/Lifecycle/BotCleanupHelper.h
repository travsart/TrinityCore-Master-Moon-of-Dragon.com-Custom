/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _PLAYERBOT_BOT_CLEANUP_HELPER_H
#define _PLAYERBOT_BOT_CLEANUP_HELPER_H

#include "Player.h"
#include "Map.h"
#include "Log.h"

namespace Playerbot
{

/**
 * @brief Helper class to safely remove bot Player objects from the world
 *
 * PROBLEM:
 * TrinityCore's Object::RemoveFromWorld() sets m_inWorld = false but does NOT
 * call Map::RemoveUpdateObject(this), leaving dangling pointers in Map's
 * _updateObjects queue. When Map::SendObjectUpdates() processes the queue,
 * it encounters objects with IsInWorld() == false and crashes with
 * ASSERT(obj->IsInWorld()) at Map.cpp:1940.
 *
 * ROOT CAUSE:
 * Object::RemoveFromWorld() in Object.cpp:124 is missing the call to
 * Map::RemoveUpdateObject(this) that would clean up the update queue.
 *
 * RACE CONDITION (TOCTOU):
 * T1: Bot in combat → Map::AddUpdateObject(bot) adds to _updateObjects queue
 * T2: Bot logs out → RemoveFromWorld() sets IsInWorld() = false
 * T3: ❌ BUG: Does NOT call Map::RemoveUpdateObject(bot)
 * T4: Bot still in _updateObjects with IsInWorld() == false
 * T5: Map::SendObjectUpdates() processes queue
 * T6: ASSERT(obj->IsInWorld()) fails → 💥 CRASH!
 *
 * SOLUTION:
 * This helper class provides SafeRemoveFromWorld() which:
 * 1. Calls Map::RemoveUpdateObject(bot) FIRST to clean up the queue
 * 2. Then calls bot->RemoveFromWorld() safely
 *
 * This prevents the TOCTOU race condition by ensuring objects are removed
 * from the update queue BEFORE IsInWorld() is set to false.
 *
 * USAGE:
 * Replace direct RemoveFromWorld() calls in playerbot code:
 *
 * BEFORE (BUGGY):
 *   if (player->IsInWorld())
 *       player->RemoveFromWorld();
 *
 * AFTER (SAFE):
 *   if (player->IsInWorld())
 *       Playerbot::BotCleanupHelper::SafeRemoveFromWorld(player);
 *
 * AFFECTED FILES:
 * - src/modules/Playerbot/Lifecycle/BotWorldEntry.cpp:742
 * - src/modules/Playerbot/Session/BotSessionEnhanced.cpp:241
 *
 * @see DEATH_HANDLING_ANALYSIS_COMPLETE.md for comprehensive analysis
 * @see Map::SendObjectUpdates() at Map.cpp:1940 (crash location)
 * @see Object::RemoveFromWorld() at Object.cpp:124 (root cause)
 */
class BotCleanupHelper
{
public:
    /**
     * @brief Safely remove a bot Player from the world
     *
     * This function ensures the bot is properly removed from the Map's update
     * queue BEFORE calling RemoveFromWorld(), preventing crashes in
     * Map::SendObjectUpdates().
     *
     * @param bot The bot Player to remove (must not be nullptr)
     *
     * Thread Safety: This function is thread-safe when called from the same
     * thread that owns the Player object (typically the map update thread).
     * Do NOT call from worker threads without proper synchronization.
     *
     * Call Order:
     * 1. Get bot's current map
     * 2. Call Map::RemoveUpdateObject(bot) to remove from _updateObjects queue
     * 3. Call bot->RemoveFromWorld() to mark as not in world
     *
     * This ordering is CRITICAL to prevent the TOCTOU race condition.
     */
    static void SafeRemoveFromWorld(Player* bot)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.lifecycle", "BotCleanupHelper::SafeRemoveFromWorld: bot is nullptr!");
            return;
        }

        // CRITICAL FIX: Remove from map's update queue FIRST
        // This prevents Map::SendObjectUpdates() from accessing the bot after IsInWorld() == false
        //
        // The bug occurs because Object::RemoveFromWorld() does this:
        //   m_inWorld = false;           // Sets IsInWorld() to false
        //   ClearUpdateMask(true);
        //   // ❌ MISSING: Map::RemoveUpdateObject(this)
        //
        // So we do the missing cleanup HERE, BEFORE calling RemoveFromWorld().
        if (Map* map = bot->GetMap())
        {
            map->RemoveUpdateObject(bot);

            TC_LOG_DEBUG("playerbot.lifecycle",
                "BotCleanupHelper: Removed bot {} (GUID: {}) from map {} update queue",
                bot->GetName(),
                bot->GetGUID().ToString(),
                map->GetId());
        }
        else
        {
            TC_LOG_WARN("playerbot.lifecycle",
                "BotCleanupHelper: Bot {} has no map! Proceeding with RemoveFromWorld() anyway.",
                bot->GetName());
        }

        // Now safe to call RemoveFromWorld()
        // Even though this sets IsInWorld() = false, the bot is no longer in
        // Map's _updateObjects queue, so Map::SendObjectUpdates() won't try to process it.
        bot->RemoveFromWorld();

        TC_LOG_DEBUG("playerbot.lifecycle",
            "BotCleanupHelper: Bot {} safely removed from world (IsInWorld: {})",
            bot->GetName(),
            bot->IsInWorld() ? "TRUE" : "FALSE");
    }

    /**
     * @brief Check if a bot is safe to remove from world
     *
     * Performs validation checks before attempting to remove a bot.
     * This is optional but recommended for defensive programming.
     *
     * @param bot The bot to check
     * @return true if safe to call SafeRemoveFromWorld(), false otherwise
     */
    static bool CanSafelyRemove(Player* bot)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.lifecycle", "BotCleanupHelper::CanSafelyRemove: bot is nullptr!");
            return false;
        }

        if (!bot->IsInWorld())
        {
            TC_LOG_DEBUG("playerbot.lifecycle",
                "BotCleanupHelper::CanSafelyRemove: Bot {} is already not in world",
                bot->GetName());
            return false; // Already removed, don't call RemoveFromWorld() again
        }

        return true;
    }
};

} // namespace Playerbot

#endif // _PLAYERBOT_BOT_CLEANUP_HELPER_H
