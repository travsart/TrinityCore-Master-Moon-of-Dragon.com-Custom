/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotResurrectionScript.h"
#include "Player.h"
#include "Corpse.h"
#include "WorldSession.h"
#include "BotSession.h"
#include "BotAI.h"
#include "GameTime.h"
#include "Log.h"

namespace Playerbot
{

// Constructor - Register with ScriptMgr
BotResurrectionScript::BotResurrectionScript() : PlayerScript("BotResurrectionScript")
{
    TC_LOG_INFO("module.playerbot.script", "BotResurrectionScript registered with ScriptMgr");
}

// OnPlayerRepop Hook Implementation
void BotResurrectionScript::OnPlayerRepop(Player* player)
{
    // DEBUG: Log every OnPlayerRepop call
    TC_LOG_INFO("playerbot.death",
        "🔔 OnPlayerRepop hook fired! player={}, IsBot check pending...",
        player ? player->GetName() : "nullptr");

    // Validation: Null pointer check
    if (!player)
    {
        TC_LOG_ERROR("playerbot.death",
            "OnPlayerRepop called with nullptr player - this should never happen");
        return;
    }

    // Early exit: Only process bots
    if (!IsPlayerBot(player))
    {
        TC_LOG_INFO("playerbot.death",
            "OnPlayerRepop: {} is not a bot, skipping",
            player->GetName());
        return;  // Not a bot, let human players handle resurrection manually
    }

    TC_LOG_INFO("playerbot.death",
        "🤖 OnPlayerRepop: Bot {} detected, checking resurrection eligibility...",
        player->GetName());

    // Log hook invocation for debugging
    TC_LOG_DEBUG("playerbot.death",
        "OnPlayerRepop hook fired for bot {} (guid={}, deathState={}, isAlive={}, isGhost={})",
        player->GetName(),
        player->GetGUID().ToString(),
        static_cast<int>(player->getDeathState()),
        player->IsAlive(),
        player->HasPlayerFlag(PLAYER_FLAGS_GHOST));

    // Validate resurrection conditions
    std::string failureReason;
    if (!ValidateResurrectionConditions(player, failureReason))
    {
        TC_LOG_DEBUG("playerbot.death",
            "Bot {} not eligible for auto-resurrection: {}",
            player->GetName(), failureReason);
        return;
    }

    // All checks passed - execute resurrection
    TC_LOG_INFO("playerbot.death",
        "✅ Bot {} eligible for auto-resurrection - executing...",
        player->GetName());

    if (ExecuteResurrection(player))
    {
        TC_LOG_INFO("playerbot.death",
            "✅ Bot {} successfully auto-resurrected! Health={}/{}, Mana={}/{}",
            player->GetName(),
            player->GetHealth(), player->GetMaxHealth(),
            player->GetPower(POWER_MANA), player->GetMaxPower(POWER_MANA));
    }
    else
    {
        TC_LOG_ERROR("playerbot.death",
            "❌ Bot {} resurrection FAILED despite passing validation checks",
            player->GetName());
    }
}

// Helper: Check if player is a bot
bool BotResurrectionScript::IsPlayerBot(Player const* player)
{
    if (!player)
        return false;

    // Check if session is a BotSession
    WorldSession* session = player->GetSession();
    if (!session)
        return false;

    // BotSession inherits from WorldSession
    BotSession* botSession = dynamic_cast<BotSession*>(session);
    return (botSession != nullptr);
}

// Helper: Validate resurrection conditions
bool BotResurrectionScript::ValidateResurrectionConditions(Player* player, std::string& failureReason)
{
    // VALIDATION 1: Bot must be dead (not alive)
    // This check mirrors HandleReclaimCorpse line 396
    if (player->IsAlive())
    {
        failureReason = "Bot is already alive";
        return false;
    }

    // VALIDATION 2: Not in arena
    // Arenas have special resurrection rules, don't auto-resurrect
    // Mirrors HandleReclaimCorpse line 402
    if (player->InArena())
    {
        failureReason = "Bot is in arena (special resurrection rules apply)";
        return false;
    }

    // VALIDATION 3: Must have ghost flag
    // Ghost flag indicates spirit has been released
    // Mirrors HandleReclaimCorpse line 408
    if (!player->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        failureReason = "Bot does not have PLAYER_FLAGS_GHOST (spirit not released)";
        return false;
    }

    // VALIDATION 4: Corpse must exist
    // Cannot resurrect without a corpse
    // Mirrors HandleReclaimCorpse line 413
    Corpse* corpse = player->GetCorpse();
    if (!corpse)
    {
        failureReason = "Bot has no corpse";
        return false;
    }

    // VALIDATION 5A: Corpse freshness check
    // Prevents auto-resurrection during OnPlayerRepop() before graveyard teleport
    // If corpse was created within last 10 seconds, bot hasn't had time to teleport to graveyard yet
    // This fixes the "instantly appearing at corpse" bug where bots resurrect at death location
    time_t ghostTime = corpse->GetGhostTime();
    time_t currentTime = GameTime::GetGameTime();
    time_t corpseAge = currentTime - ghostTime;

    if (corpseAge < 10)
    {
        failureReason = fmt::format(
            "Corpse too fresh (created {} seconds ago, need 10+ seconds for graveyard teleport)",
            corpseAge);

        TC_LOG_DEBUG("playerbot.death",
            "Bot {} corpse age check: ghostTime={}, currentTime={}, age={}s, minimum=10s",
            player->GetName(), ghostTime, currentTime, corpseAge);

        return false;
    }

    // VALIDATION 5B: Ghost time delay check
    // Prevents immediate resurrection - must wait 30 seconds after graveyard teleport
    // Mirrors HandleReclaimCorpse line 418-422
    uint32 corpseReclaimDelay = player->GetCorpseReclaimDelay(
        corpse->GetType() == CORPSE_RESURRECTABLE_PVP);

    time_t requiredTime = ghostTime + corpseReclaimDelay;

    if (requiredTime > currentTime)
    {
        uint32 remainingSeconds = static_cast<uint32>(requiredTime - currentTime);
        failureReason = fmt::format(
            "Ghost time delay not expired (need to wait {} more seconds)",
            remainingSeconds);

        TC_LOG_TRACE("playerbot.death",
            "Bot {} ghost time delay: ghostTime={}, currentTime={}, delay={}, remaining={}s",
            player->GetName(), ghostTime, currentTime, corpseReclaimDelay, remainingSeconds);

        return false;
    }

    // VALIDATION 6: Distance check
    // Must be within 39 yards of corpse
    // Mirrors HandleReclaimCorpse line 424-428
    float distance = player->GetDistance2d(corpse);
    if (!corpse->IsWithinDistInMap(player, CORPSE_RECLAIM_RADIUS, true))
    {
        failureReason = fmt::format(
            "Bot too far from corpse ({:.1f} yards > {} yards max)",
            distance, CORPSE_RECLAIM_RADIUS);

        TC_LOG_TRACE("playerbot.death",
            "Bot {} position: ({:.2f}, {:.2f}, {:.2f}), Corpse position: ({:.2f}, {:.2f}, {:.2f}), Distance: {:.2f}y",
            player->GetName(),
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
            corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ(),
            distance);

        return false;
    }

    // VALIDATION 7: Map check
    // Ensure bot and corpse are on same map
    if (player->GetMapId() != corpse->GetMapId())
    {
        failureReason = fmt::format(
            "Bot on different map than corpse (bot map={}, corpse map={})",
            player->GetMapId(), corpse->GetMapId());
        return false;
    }

    // All validations passed
    TC_LOG_DEBUG("playerbot.death",
        "Bot {} passed all 7 validation checks: alive=false, arena=false, ghost=true, "
        "corpse=exists, delay=expired, distance={:.1f}y<39.0y, map=same",
        player->GetName(), distance);

    return true;
}

// Helper: Execute resurrection
bool BotResurrectionScript::ExecuteResurrection(Player* player)
{
    try
    {
        // Log pre-resurrection state
        TC_LOG_DEBUG("playerbot.death",
            "Bot {} pre-resurrection: deathState={}, health={}/{}, mana={}/{}, isGhost={}",
            player->GetName(),
            static_cast<int>(player->getDeathState()),
            player->GetHealth(), player->GetMaxHealth(),
            player->GetPower(POWER_MANA), player->GetMaxPower(POWER_MANA),
            player->HasPlayerFlag(PLAYER_FLAGS_GHOST));

        // RESURRECTION STEP 1: Call ResurrectPlayer
        // This is the main TrinityCore API for resurrection
        // Restores 50% health/mana, removes ghost aura, sets death state to ALIVE
        // Thread-safe: only modifies player's own object
        player->ResurrectPlayer(RESURRECTION_HEALTH_RESTORE);

        // RESURRECTION STEP 2: Spawn corpse bones
        // Creates the corpse remains at the corpse location
        // Must be called after ResurrectPlayer to properly clean up corpse
        player->SpawnCorpseBones();

        // Log post-resurrection state
        TC_LOG_DEBUG("playerbot.death",
            "Bot {} post-resurrection: deathState={}, health={}/{}, mana={}/{}, isGhost={}",
            player->GetName(),
            static_cast<int>(player->getDeathState()),
            player->GetHealth(), player->GetMaxHealth(),
            player->GetPower(POWER_MANA), player->GetMaxPower(POWER_MANA),
            player->HasPlayerFlag(PLAYER_FLAGS_GHOST));

        // Verify resurrection succeeded
        if (!player->IsAlive())
        {
            TC_LOG_ERROR("playerbot.death",
                "Bot {} ResurrectPlayer() called but bot still not alive! deathState={}",
                player->GetName(), static_cast<int>(player->getDeathState()));
            return false;
        }

        // Notify DeathRecoveryManager of resurrection success
        // This allows the death recovery state machine to transition properly
        if (WorldSession* session = player->GetSession())
        {
            if (BotSession* botSession = dynamic_cast<BotSession*>(session))
            {
                if (BotAI* ai = botSession->GetAI())
                {
                    if (DeathRecoveryManager* drm = ai->GetDeathRecoveryManager())
                    {
                        // DeathRecoveryManager will detect resurrection in next Update() cycle
                        TC_LOG_DEBUG("playerbot.death",
                            "Bot {} DeathRecoveryManager will detect resurrection on next update",
                            player->GetName());
                    }
                }
            }
        }

        return true;
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("playerbot.death",
            "Exception during bot {} resurrection: {}",
            player->GetName(), ex.what());
        return false;
    }
    catch (...)
    {
        TC_LOG_ERROR("playerbot.death",
            "Unknown exception during bot {} resurrection",
            player->GetName());
        return false;
    }
}

} // namespace Playerbot
