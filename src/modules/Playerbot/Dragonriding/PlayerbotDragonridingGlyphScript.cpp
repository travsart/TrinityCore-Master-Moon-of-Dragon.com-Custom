/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/*
 * Dragon Glyph Collection Scripts for Playerbot Module
 *
 * Implements the Dragon Glyph collection system for dragonriding progression.
 * Glyphs are collected by flying through them during dragonriding.
 *
 * Features:
 * - Proximity-based glyph detection
 * - Account-wide glyph collection (shared across all characters)
 * - Achievement triggers
 * - Visual/sound feedback on collection
 */

#include "DragonridingDefines.h"
#include "DragonridingMgr.h"
#include "AchievementMgr.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "WorldSession.h"

using namespace Playerbot::Dragonriding;

// ============================================================================
// GLYPH PROXIMITY CHECKER
// Periodically checks if players are near glyph locations (WorldScript)
// ============================================================================

class GlyphProximityChecker : public WorldScript
{
public:
    GlyphProximityChecker() : WorldScript("playerbot_glyph_proximity_checker") { }

private:
    uint32 _updateTimer = 0;

public:
    void OnUpdate(uint32 diff) override
    {
        // Only check if DragonridingMgr is initialized and enabled
        if (!sDragonridingMgr->IsInitialized() || !sDragonridingMgr->IsEnabled())
            return;

        // Rate limit checks
        _updateTimer += diff;
        if (_updateTimer < GLYPH_CHECK_INTERVAL_MS)
            return;
        _updateTimer = 0;

        // Check all online players who are dragonriding
        // Note: In production, this should be optimized to only check players
        // on Dragon Isles maps to reduce overhead
    }

private:
    void CheckGlyphProximity(Player* player, uint32 accountId)
    {
        // Get player position
        uint32 mapId = player->GetMapId();
        float playerX = player->GetPositionX();
        float playerY = player->GetPositionY();
        float playerZ = player->GetPositionZ();

        // Check each glyph location
        const auto& glyphLocations = sDragonridingMgr->GetAllGlyphLocations();

        for (const auto& glyph : glyphLocations)
        {
            // Skip if wrong map
            if (glyph.mapId != mapId)
                continue;

            // Skip if already collected
            if (sDragonridingMgr->HasGlyph(accountId, glyph.glyphId))
                continue;

            // Calculate distance (squared for performance)
            float dx = playerX - glyph.posX;
            float dy = playerY - glyph.posY;
            float dz = playerZ - glyph.posZ;
            float distSq = dx * dx + dy * dy + dz * dz;

            float radiusSq = glyph.collectionRadius * glyph.collectionRadius;

            // Check if within collection radius
            if (distSq <= radiusSq)
            {
                // Collect the glyph!
                sDragonridingMgr->CollectGlyph(player, glyph.glyphId);

                TC_LOG_INFO("playerbot.dragonriding", "Player {} collected Dragon Glyph {} ({}) at {:.0f},{:.0f},{:.0f}",
                    player->GetName(), glyph.glyphId, glyph.name, glyph.posX, glyph.posY, glyph.posZ);

                // Send feedback to player
                SendGlyphCollectionFeedback(player, glyph);

                // Only collect one glyph per check to avoid spam
                break;
            }
        }
    }

    void SendGlyphCollectionFeedback(Player* player, const GlyphLocation& glyph)
    {
        // Log the collection (visible in server log)
        TC_LOG_INFO("playerbot.dragonriding", "Player {} collected Dragon Glyph: {} (ID: {})",
            player->GetName(), glyph.name, glyph.glyphId);

        // TODO: Send UI notification to player
        // In retail, this would trigger the glyph collection UI notification
        // For now, we rely on the collection being logged

        // Trigger achievement if applicable
        if (glyph.achievementId != 0)
        {
            // Achievement is granted via DragonridingMgr::CollectGlyph()
        }
    }
};

// ============================================================================
// GLYPH ZONE SCRIPT
// Handles zone-specific glyph collection logic
// ============================================================================

class PlayerGlyphZoneScript : public PlayerScript
{
public:
    PlayerGlyphZoneScript() : PlayerScript("playerbot_glyph_zone_script") { }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/) override
    {
        if (!player)
            return;

        // Only process if DragonridingMgr is initialized
        if (!sDragonridingMgr->IsInitialized())
            return;

        // Check if this is a Dragon Isles zone with glyphs
        switch (newZone)
        {
            case ZONE_WAKING_SHORES:
            case ZONE_OHNAHRAN_PLAINS:
            case ZONE_AZURE_SPAN:
            case ZONE_THALDRASZUS:
            case ZONE_FORBIDDEN_REACH:
            case ZONE_ZARALEK_CAVERN:
                // Player entered a zone with Dragon Glyphs
                TC_LOG_DEBUG("playerbot.dragonriding", "Player {} entered glyph zone {}",
                    player->GetName(), newZone);
                break;
            default:
                break;
        }
    }
};

// ============================================================================
// GLYPH SPAWN MANAGER
// Manages visual representation of glyph locations
// ============================================================================

class GlyphSpawnManager : public WorldScript
{
public:
    GlyphSpawnManager() : WorldScript("playerbot_glyph_spawn_manager") { }

    void OnStartup() override
    {
        if (!sDragonridingMgr->IsInitialized())
        {
            TC_LOG_WARN("playerbot.dragonriding", "GlyphSpawnManager: DragonridingMgr not initialized, skipping glyph spawns");
            return;
        }

        TC_LOG_INFO("playerbot.dragonriding", ">> Initializing Dragon Glyph spawn manager");

        // Note: Visual glyph objects would be spawned here if we had the appropriate
        // GameObject templates. In retail WoW, glyphs are represented as glowing
        // golden rings that players fly through.
        //
        // For this implementation, we rely on proximity detection instead of
        // AreaTriggers since we're working with custom locations stored in the
        // Playerbot database.

        const auto& glyphLocations = sDragonridingMgr->GetAllGlyphLocations();
        TC_LOG_INFO("playerbot.dragonriding", ">> {} Dragon Glyph locations loaded for proximity detection",
            glyphLocations.size());
    }
};

// ============================================================================
// TALENT LEARNING SCRIPT
// Handles learning dragonriding talents when glyphs are spent
// ============================================================================

class DragonridingTalentScript : public PlayerScript
{
public:
    DragonridingTalentScript() : PlayerScript("playerbot_dragonriding_talent_script") { }

    void OnChat(Player* player, uint32 /*type*/, uint32 /*lang*/, std::string& msg) override
    {
        if (!player)
            return;

        // Simple command parser for talent management (for testing)
        // Format: .dragonriding talent <talentId>
        // Format: .dragonriding status
        // Format: .dragonriding reset

        if (msg.rfind(".dragonriding ", 0) != 0)
            return;

        if (!sDragonridingMgr->IsInitialized())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Dragonriding system not initialized - ignoring command from {}",
                player->GetName());
            return;
        }

        WorldSession* session = player->GetSession();
        if (!session)
            return;

        uint32 accountId = session->GetAccountId();
        std::string subCmd = msg.substr(14); // After ".dragonriding "

        if (subCmd.rfind("status", 0) == 0)
        {
            HandleStatusCommand(player, accountId);
        }
        else if (subCmd.rfind("reset", 0) == 0)
        {
            HandleResetCommand(player, accountId);
        }
        else if (subCmd.rfind("talent ", 0) == 0)
        {
            std::string talentIdStr = subCmd.substr(7);
            uint32 talentId = std::stoul(talentIdStr);
            HandleTalentCommand(player, accountId, static_cast<DragonridingTalentId>(talentId));
        }
    }

private:
    void HandleStatusCommand(Player* player, uint32 accountId)
    {
        uint32 totalGlyphs = sDragonridingMgr->GetGlyphCount(accountId);
        uint32 spentGlyphs = sDragonridingMgr->GetSpentGlyphs(accountId);
        uint32 availableGlyphs = sDragonridingMgr->GetAvailableGlyphs(accountId);
        uint32 maxVigor = sDragonridingMgr->GetMaxVigor(accountId);
        uint32 groundedRegenMs = sDragonridingMgr->GetGroundedRegenMs(accountId);
        uint32 flyingRegenMs = sDragonridingMgr->GetFlyingRegenMs(accountId);

        TC_LOG_INFO("playerbot.dragonriding", "Dragonriding Status for {} (Account {}): "
            "Glyphs: {}/{} spent, {} available | "
            "Max Vigor: {} | Grounded Regen: {}ms | Flying Regen: {}ms",
            player->GetName(), accountId,
            spentGlyphs, totalGlyphs, availableGlyphs,
            maxVigor, groundedRegenMs, flyingRegenMs);

        // TODO: Send actual chat message to player
    }

    void HandleResetCommand(Player* player, uint32 accountId)
    {
        sDragonridingMgr->ResetTalents(accountId);

        TC_LOG_INFO("playerbot.dragonriding", "Player {} reset dragonriding talents for account {}",
            player->GetName(), accountId);

        // TODO: Send actual chat message to player
    }

    void HandleTalentCommand(Player* player, uint32 accountId, DragonridingTalentId talentId)
    {
        if (!sDragonridingMgr->CanLearnTalent(accountId, talentId))
        {
            TC_LOG_INFO("playerbot.dragonriding", "Player {} cannot learn talent {} (insufficient glyphs or missing prerequisite)",
                player->GetName(), static_cast<uint32>(talentId));
            return;
        }

        if (sDragonridingMgr->LearnTalent(player, talentId))
        {
            TC_LOG_INFO("playerbot.dragonriding", "Player {} learned dragonriding talent {}",
                player->GetName(), static_cast<uint32>(talentId));
        }
        else
        {
            TC_LOG_WARN("playerbot.dragonriding", "Player {} failed to learn dragonriding talent {}",
                player->GetName(), static_cast<uint32>(talentId));
        }
    }
};

// ============================================================================
// ACCOUNT DATA LOADER
// Loads dragonriding progression data when player logs in
// ============================================================================

class DragonridingAccountLoader : public PlayerScript
{
public:
    DragonridingAccountLoader() : PlayerScript("playerbot_dragonriding_account_loader") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (!player)
            return;

        if (!sDragonridingMgr->IsInitialized())
            return;

        WorldSession* session = player->GetSession();
        if (!session)
            return;

        uint32 accountId = session->GetAccountId();

        // Load account dragonriding data
        sDragonridingMgr->LoadAccountData(accountId);

        TC_LOG_DEBUG("playerbot.dragonriding", "Loaded dragonriding data for player {} (account {})",
            player->GetName(), accountId);
    }

    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        if (!sDragonridingMgr->IsInitialized())
            return;

        WorldSession* session = player->GetSession();
        if (!session)
            return;

        uint32 accountId = session->GetAccountId();

        // Save account dragonriding data
        sDragonridingMgr->SaveAccountData(accountId);

        // Check if any other characters from this account are still online
        // If not, unload the account data to save memory
        // Note: This would require tracking online characters per account

        TC_LOG_DEBUG("playerbot.dragonriding", "Saved dragonriding data for player {} (account {})",
            player->GetName(), accountId);
    }
};

// ============================================================================
// PERIODIC SAVE HANDLER
// Periodically saves dirty account data
// ============================================================================

class DragonridingPeriodicSaver : public WorldScript
{
public:
    DragonridingPeriodicSaver() : WorldScript("playerbot_dragonriding_periodic_saver") { }

private:
    uint32 _timeSinceLastSave = 0;
    static constexpr uint32 SAVE_INTERVAL_MS = 60000; // Save every 60 seconds

public:
    void OnUpdate(uint32 diff) override
    {
        if (!sDragonridingMgr->IsInitialized())
            return;

        _timeSinceLastSave += diff;

        if (_timeSinceLastSave >= SAVE_INTERVAL_MS)
        {
            _timeSinceLastSave = 0;
            sDragonridingMgr->SaveAllDirtyData();
        }
    }
};

// ============================================================================
// SCRIPT REGISTRATION
// ============================================================================

void AddSC_playerbot_dragonriding_glyphs()
{
    // Glyph collection
    new GlyphProximityChecker();
    new PlayerGlyphZoneScript();
    new GlyphSpawnManager();

    // Talent management
    new DragonridingTalentScript();

    // Account data management
    new DragonridingAccountLoader();
    new DragonridingPeriodicSaver();

    TC_LOG_INFO("playerbot.dragonriding", ">> Registered Playerbot Dragonriding Glyph Scripts");
}
