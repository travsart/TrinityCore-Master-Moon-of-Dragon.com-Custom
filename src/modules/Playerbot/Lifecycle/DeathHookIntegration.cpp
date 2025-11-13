/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * DeathHookIntegration: Minimal core hooks for Playerbot death management
 */

#include "DeathHookIntegration.h"
#include "Player.h"
#include "Corpse.h"
#include "WorldSession.h"
#include "Log.h"
#include "CorpsePreventionManager.h"
#include "SafeCorpseManager.h"
#include "DeathRecoveryManager.h"
#include "../AI/BotAI.h"

namespace Playerbot
{

bool DeathHookIntegration::s_enabled = true;

void DeathHookIntegration::OnPlayerPreDeath(Player* player)
{
    if (!player || !player->GetSession()->IsBot())
        return;

    TC_LOG_DEBUG("playerbot.death.hook", "OnPlayerPreDeath: Bot {} about to die",
        player->GetName());

    // Strategy 1: Try to prevent corpse creation
    if (CorpsePreventionManager::IsEnabled() &&
        CorpsePreventionManager::ShouldPreventCorpse(player))
    {
        CorpsePreventionManager::OnBotBeforeDeath(player);
    }

    // Always cache death location before corpse creation
    if (BotAI* ai = dynamic_cast<BotAI*>(player->GetAI()))
    {
        if (DeathRecoveryManager* drm = ai->GetDeathRecoveryManager())
        {
            // This ensures corpse location is cached BEFORE the race condition
            drm->OnDeath();
        }
    }
}

void DeathHookIntegration::OnPlayerCorpseCreated(Player* player, Corpse* corpse)
{
    if (!player || !corpse || !player->GetSession()->IsBot())
        return;

    TC_LOG_DEBUG("playerbot.death.hook", "OnPlayerCorpseCreated: Bot {} corpse {} created",
        player->GetName(), corpse->GetGUID().ToString());

    // Strategy 2: Track corpse for safe deletion
    SafeCorpseManager::Instance().RegisterCorpse(player, corpse);

    // Check if we should have prevented this corpse
    if (CorpsePreventionManager::IsEnabled())
    {
        TC_LOG_WARN("playerbot.death.hook",
            "Corpse created despite prevention for bot {} - using SafeCorpseManager fallback",
            player->GetName());
    }
}

bool DeathHookIntegration::OnCorpsePreRemove(Corpse* corpse)
{
    if (!corpse)
        return true; // Allow removal

    ObjectGuid corpseGuid = corpse->GetGUID();
    // Check if this corpse is safe to remove
    if (!SafeCorpseManager::Instance().IsCorpseSafeToDelete(corpseGuid))
    {
        TC_LOG_DEBUG("playerbot.death.hook",
            "OnCorpsePreRemove: Delaying corpse {} removal - Map update in progress",
            corpseGuid.ToString());
        return false; // Prevent removal
    }

    TC_LOG_TRACE("playerbot.death.hook", "OnCorpsePreRemove: Corpse {} safe to remove",
        corpseGuid.ToString());
    return true; // Allow removal
}

void DeathHookIntegration::OnPlayerPreResurrection(Player* player)
{
    if (!player || !player->GetSession()->IsBot())
        return;

    TC_LOG_DEBUG("playerbot.death.hook", "OnPlayerPreResurrection: Bot {} about to resurrect",
        player->GetName());

    // Mark any corpse as safe for deletion
    ObjectGuid playerGuid = player->GetGUID();
    float x, y, z;
    uint32 mapId;

    if (SafeCorpseManager::Instance().GetCorpseLocation(playerGuid, x, y, z, mapId))
    {
        // We know the player is resurrecting, so corpse can be safely removed
        // after current Map update cycle completes
        TC_LOG_DEBUG("playerbot.death.hook",
            "Bot {} resurrecting - marking corpse for safe deletion", player->GetName());
    }
}

void DeathHookIntegration::OnPlayerPostResurrection(Player* player)
{
    if (!player || !player->GetSession()->IsBot())
        return;

    TC_LOG_DEBUG("playerbot.death.hook", "OnPlayerPostResurrection: Bot {} resurrected",
        player->GetName());

    // Notify DeathRecoveryManager
    if (BotAI* ai = dynamic_cast<BotAI*>(player->GetAI()))
    {
        if (DeathRecoveryManager* drm = ai->GetDeathRecoveryManager())
        {
            drm->OnResurrection();
        }
    }

    // Check if this was a prevented corpse that got resurrected
    if (player->GetByteValue(PLAYER_FIELD_BYTES2, 3) & 0x80)
    {
        CorpsePreventionManager::OnBotAfterDeath(player);
    }
}

} // namespace Playerbot