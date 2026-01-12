/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotPostLoginConfigurator.h"
#include "BotTemplateRepository.h"
#include "Player.h"
#include "Item.h"
#include "Log.h"
#include "SpellMgr.h"
#include "ObjectMgr.h"
#include "DB2Stores.h"
#include "World.h"
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

BotPostLoginConfigurator* BotPostLoginConfigurator::Instance()
{
    static BotPostLoginConfigurator instance;
    return &instance;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool BotPostLoginConfigurator::Initialize()
{
    if (_initialized.load())
        return true;

    TC_LOG_INFO("module.playerbot", "BotPostLoginConfigurator: Initializing...");

    _pendingConfigs.clear();
    ResetStats();

    _initialized.store(true);
    TC_LOG_INFO("module.playerbot", "BotPostLoginConfigurator: Initialization complete");
    return true;
}

void BotPostLoginConfigurator::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("module.playerbot", "BotPostLoginConfigurator: Shutting down...");

    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _pendingConfigs.clear();
    }

    _initialized.store(false);
    TC_LOG_INFO("module.playerbot", "BotPostLoginConfigurator: Shutdown complete");
}

// ============================================================================
// CONFIGURATION REGISTRATION
// ============================================================================

void BotPostLoginConfigurator::RegisterPendingConfig(BotPendingConfiguration config)
{
    std::lock_guard<std::mutex> lock(_configMutex);

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Registering pending config for bot {} - Template: {}, Level: {}, GS: {}",
        config.botGuid.ToString(), config.templateId, config.targetLevel, config.targetGearScore);

    _pendingConfigs[config.botGuid] = std::move(config);
    _stats.pendingConfigs.fetch_add(1);
}

bool BotPostLoginConfigurator::HasPendingConfiguration(ObjectGuid botGuid) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    return _pendingConfigs.find(botGuid) != _pendingConfigs.end();
}

BotPendingConfiguration const* BotPostLoginConfigurator::GetPendingConfiguration(ObjectGuid botGuid) const
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _pendingConfigs.find(botGuid);
    return it != _pendingConfigs.end() ? &it->second : nullptr;
}

void BotPostLoginConfigurator::RemovePendingConfiguration(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_configMutex);
    auto it = _pendingConfigs.find(botGuid);
    if (it != _pendingConfigs.end())
    {
        _pendingConfigs.erase(it);
        _stats.pendingConfigs.fetch_sub(1);
    }
}

// ============================================================================
// CONFIGURATION APPLICATION
// ============================================================================

bool BotPostLoginConfigurator::ApplyPendingConfiguration(Player* player)
{
    if (!player)
    {
        TC_LOG_ERROR("module.playerbot.configurator", "ApplyPendingConfiguration: null player");
        return false;
    }

    ObjectGuid playerGuid = player->GetGUID();

    // Get pending configuration
    BotPendingConfiguration config;
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        auto it = _pendingConfigs.find(playerGuid);
        if (it == _pendingConfigs.end())
        {
            // No pending configuration - this is normal for regular bots
            return false;
        }
        config = it->second;
    }

    auto startTime = std::chrono::steady_clock::now();

    TC_LOG_INFO("module.playerbot.configurator",
        "Applying post-login configuration for bot {} - Template: {}, Level: {}, GS: {}",
        player->GetName(), config.templateId, config.targetLevel, config.targetGearScore);

    _stats.totalConfigured.fetch_add(1);

    bool success = true;

    // Get template if not cached
    BotTemplate const* tmpl = config.templatePtr;
    if (!tmpl && config.templateId > 0)
    {
        tmpl = sBotTemplateRepository->GetTemplateById(config.templateId);
    }

    if (!tmpl)
    {
        TC_LOG_ERROR("module.playerbot.configurator",
            "Failed to get template {} for bot {}",
            config.templateId, player->GetName());
        _stats.failedConfigs.fetch_add(1);
        RemovePendingConfiguration(playerGuid);
        return false;
    }

    // Step 1: Apply Level
    if (config.targetLevel > 1 && config.targetLevel != player->GetLevel())
    {
        TC_LOG_DEBUG("module.playerbot.configurator",
            "Applying level {} to bot {} (current: {})",
            config.targetLevel, player->GetName(), player->GetLevel());

        if (!ApplyLevel(player, config.targetLevel))
        {
            TC_LOG_WARN("module.playerbot.configurator",
                "Failed to apply level {} to bot {}",
                config.targetLevel, player->GetName());
            success = false;
        }
    }

    // Step 2: Apply Specialization (must be before talents)
    uint32 specId = config.specId > 0 ? config.specId : tmpl->specId;
    if (specId > 0)
    {
        TC_LOG_DEBUG("module.playerbot.configurator",
            "Applying specialization {} to bot {}",
            specId, player->GetName());

        if (!ApplySpecialization(player, specId))
        {
            TC_LOG_WARN("module.playerbot.configurator",
                "Failed to apply specialization {} to bot {}",
                specId, player->GetName());
            // Continue anyway - some operations may still work
        }
    }

    // Step 3: Learn class spells for level
    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applying class spells to bot {}",
        player->GetName());

    ApplyClassSpells(player);

    // Step 4: Apply Talents
    if (!tmpl->talents.talentIds.empty())
    {
        TC_LOG_DEBUG("module.playerbot.configurator",
            "Applying {} talents to bot {}",
            tmpl->talents.talentIds.size(), player->GetName());

        if (!ApplyTalents(player, tmpl))
        {
            TC_LOG_WARN("module.playerbot.configurator",
                "Some talents failed to apply for bot {}",
                player->GetName());
            // Continue anyway
        }
    }

    // Step 5: Apply Gear
    if (config.targetGearScore > 0 || !tmpl->gearSets.empty())
    {
        TC_LOG_DEBUG("module.playerbot.configurator",
            "Applying gear (target GS: {}) to bot {}",
            config.targetGearScore, player->GetName());

        if (!ApplyGear(player, tmpl, config.targetGearScore))
        {
            TC_LOG_WARN("module.playerbot.configurator",
                "Some gear failed to apply for bot {}",
                player->GetName());
            // Continue anyway
        }
    }

    // Step 6: Apply Action Bars
    if (!tmpl->actionBars.buttons.empty())
    {
        TC_LOG_DEBUG("module.playerbot.configurator",
            "Applying {} action buttons to bot {}",
            tmpl->actionBars.buttons.size(), player->GetName());

        if (!ApplyActionBars(player, tmpl))
        {
            TC_LOG_WARN("module.playerbot.configurator",
                "Some action bars failed to apply for bot {}",
                player->GetName());
            // Continue anyway
        }
    }

    // Update all stats after configuration
    player->UpdateAllStats();

    // Set full health and power
    player->SetFullHealth();
    for (uint8 i = 0; i < MAX_POWERS; ++i)
    {
        Powers power = Powers(i);
        if (player->GetMaxPower(power) > 0)
            player->SetFullPower(power);
    }

    // Calculate timing
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    _stats.totalConfigTimeMs.fetch_add(durationMs);

    if (success)
    {
        _stats.successfulConfigs.fetch_add(1);
        TC_LOG_INFO("module.playerbot.configurator",
            "Successfully configured bot {} in {}ms - Level: {}, Spec: {}, GS: {}",
            player->GetName(), durationMs, player->GetLevel(), specId, config.targetGearScore);
    }
    else
    {
        _stats.failedConfigs.fetch_add(1);
        TC_LOG_WARN("module.playerbot.configurator",
            "Partially configured bot {} in {}ms (some steps failed)",
            player->GetName(), durationMs);
    }

    // Remove pending configuration
    RemovePendingConfiguration(playerGuid);

    return success;
}

// ============================================================================
// INDIVIDUAL CONFIGURATION STEPS
// ============================================================================

bool BotPostLoginConfigurator::ApplyLevel(Player* player, uint32 targetLevel)
{
    if (!player || targetLevel < 1 || targetLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        return false;

    uint32 currentLevel = player->GetLevel();

    if (currentLevel == targetLevel)
        return true;

    // Use GiveLevel for proper stat calculation
    // GiveLevel automatically handles:
    // - Stat recalculation
    // - Talent tier unlocking
    // - Skill updates
    // - Specialization spells
    // - Health/mana restoration
    player->GiveLevel(targetLevel);

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applied level {} to bot {} (was: {})",
        targetLevel, player->GetName(), currentLevel);

    return player->GetLevel() == targetLevel;
}

bool BotPostLoginConfigurator::ApplySpecialization(Player* player, uint32 specId)
{
    if (!player || specId == 0)
        return false;

    // Validate specialization exists
    ChrSpecializationEntry const* specEntry = sChrSpecializationStore.LookupEntry(specId);
    if (!specEntry)
    {
        TC_LOG_WARN("module.playerbot.configurator",
            "Invalid specialization ID {} for bot {}",
            specId, player->GetName());
        return false;
    }

    // Validate class matches
    if (specEntry->ClassID != player->GetClass())
    {
        TC_LOG_WARN("module.playerbot.configurator",
            "Specialization {} is for class {} but bot {} is class {}",
            specId, specEntry->ClassID, player->GetName(), player->GetClass());
        return false;
    }

    // Set the primary specialization
    player->SetPrimarySpecialization(specId);

    // Learn specialization spells
    player->LearnSpecializationSpells();

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applied specialization {} to bot {}",
        specId, player->GetName());

    return true;
}

bool BotPostLoginConfigurator::ApplyTalents(Player* player, BotTemplate const* tmpl)
{
    if (!player || !tmpl)
        return false;

    // Must have specialization set first
    if (player->GetPrimarySpecialization() == ChrSpecialization::None)
    {
        TC_LOG_WARN("module.playerbot.configurator",
            "Cannot apply talents - bot {} has no specialization",
            player->GetName());
        return false;
    }

    // Temporarily allow talent changes without rest area
    player->SetUnitFlag2(UNIT_FLAG2_ALLOW_CHANGING_TALENTS);

    uint32 talentsLearned = 0;
    uint32 talentsFailed = 0;

    for (uint32 talentId : tmpl->talents.talentIds)
    {
        if (LearnTalent(player, talentId))
            ++talentsLearned;
        else
            ++talentsFailed;
    }

    // Remove temporary flag
    player->RemoveUnitFlag2(UNIT_FLAG2_ALLOW_CHANGING_TALENTS);

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applied talents to bot {}: {} learned, {} failed",
        player->GetName(), talentsLearned, talentsFailed);

    return talentsFailed == 0;
}

bool BotPostLoginConfigurator::LearnTalent(Player* player, uint32 talentId)
{
    if (!player || talentId == 0)
        return false;

    // Validate talent exists
    TalentEntry const* talentEntry = sTalentStore.LookupEntry(talentId);
    if (!talentEntry)
    {
        TC_LOG_TRACE("module.playerbot.configurator",
            "Invalid talent ID {} for bot {}",
            talentId, player->GetName());
        return false;
    }

    // Check class match
    if (talentEntry->ClassID != player->GetClass())
    {
        TC_LOG_TRACE("module.playerbot.configurator",
            "Talent {} is for class {} but bot {} is class {}",
            talentId, talentEntry->ClassID, player->GetName(), player->GetClass());
        return false;
    }

    // Try to learn the talent
    int32 spellOnCooldown = 0;
    TalentLearnResult result = player->LearnTalent(talentId, &spellOnCooldown);

    if (result == TALENT_LEARN_OK)
    {
        TC_LOG_TRACE("module.playerbot.configurator",
            "Bot {} learned talent {}",
            player->GetName(), talentId);
        return true;
    }

    // Log specific failure reason at trace level
    switch (result)
    {
        case TALENT_FAILED_AFFECTING_COMBAT:
            TC_LOG_TRACE("module.playerbot.configurator",
                "Talent {} failed for bot {}: in combat", talentId, player->GetName());
            break;
        case TALENT_FAILED_CANT_DO_THAT_RIGHT_NOW:
            TC_LOG_TRACE("module.playerbot.configurator",
                "Talent {} failed for bot {}: dead", talentId, player->GetName());
            break;
        case TALENT_FAILED_NO_PRIMARY_TREE_SELECTED:
            TC_LOG_TRACE("module.playerbot.configurator",
                "Talent {} failed for bot {}: no spec", talentId, player->GetName());
            break;
        case TALENT_FAILED_REST_AREA:
            TC_LOG_TRACE("module.playerbot.configurator",
                "Talent {} failed for bot {}: not in rest area", talentId, player->GetName());
            break;
        default:
            TC_LOG_TRACE("module.playerbot.configurator",
                "Talent {} failed for bot {}: unknown ({})", talentId, player->GetName(), int(result));
            break;
    }

    return false;
}

bool BotPostLoginConfigurator::ApplyGear(Player* player, BotTemplate const* tmpl, uint32 targetGearScore)
{
    if (!player || !tmpl)
        return false;

    // Select best gear set for target
    GearSetTemplate const* gearSet = SelectGearSet(tmpl, targetGearScore);
    if (!gearSet)
    {
        TC_LOG_DEBUG("module.playerbot.configurator",
            "No gear set found for bot {} (target GS: {})",
            player->GetName(), targetGearScore);
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Selected gear set iLvl {} (actual GS: {}) for bot {}",
        gearSet->targetItemLevel, gearSet->actualGearScore, player->GetName());

    uint32 itemsEquipped = 0;
    uint32 itemsFailed = 0;

    // Equipment slots (0-18)
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot >= gearSet->slots.size())
            break;

        GearSlotTemplate const& slotData = gearSet->slots[slot];
        if (slotData.itemId == 0)
            continue;

        if (EquipItem(player, slot, slotData.itemId))
            ++itemsEquipped;
        else
            ++itemsFailed;
    }

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applied gear to bot {}: {} equipped, {} failed",
        player->GetName(), itemsEquipped, itemsFailed);

    return itemsFailed == 0;
}

bool BotPostLoginConfigurator::EquipItem(Player* player, uint8 slot, uint32 itemId)
{
    if (!player || itemId == 0)
        return false;

    // Validate item exists
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
    {
        TC_LOG_TRACE("module.playerbot.configurator",
            "Invalid item ID {} for slot {} on bot {}",
            itemId, slot, player->GetName());
        return false;
    }

    // Remove existing item in slot
    Item* existingItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (existingItem)
    {
        player->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
    }

    // Create and equip new item
    // EquipNewItem handles: item creation, validation, stat application
    Item* newItem = player->EquipNewItem(slot, itemId, ItemContext::NONE, true);

    if (newItem)
    {
        TC_LOG_TRACE("module.playerbot.configurator",
            "Bot {} equipped item {} in slot {}",
            player->GetName(), itemId, slot);
        return true;
    }

    TC_LOG_TRACE("module.playerbot.configurator",
        "Failed to equip item {} in slot {} for bot {}",
        itemId, slot, player->GetName());
    return false;
}

bool BotPostLoginConfigurator::ApplyActionBars(Player* player, BotTemplate const* tmpl)
{
    if (!player || !tmpl)
        return false;

    uint32 buttonsSet = 0;

    for (ActionBarButton const& button : tmpl->actionBars.buttons)
    {
        // Calculate global slot: bar * 12 + slot
        uint8 globalSlot = static_cast<uint8>(button.actionBar * 12 + button.slot);

        // Set the action button
        player->AddActionButton(globalSlot, button.actionId, button.actionType);
        ++buttonsSet;
    }

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applied {} action buttons to bot {}",
        buttonsSet, player->GetName());

    return true;
}

bool BotPostLoginConfigurator::ApplyClassSpells(Player* player)
{
    if (!player)
        return false;

    // Learn default skills and spells for level
    player->LearnDefaultSkills();
    player->UpdateSkillsForLevel();

    // Learn specialization spells if spec is set
    if (player->GetPrimarySpecialization() != ChrSpecialization::None)
    {
        player->LearnSpecializationSpells();
    }

    TC_LOG_DEBUG("module.playerbot.configurator",
        "Applied class spells to bot {} (level {})",
        player->GetName(), player->GetLevel());

    return true;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

GearSetTemplate const* BotPostLoginConfigurator::SelectGearSet(BotTemplate const* tmpl, uint32 targetGearScore) const
{
    if (!tmpl || tmpl->gearSets.empty())
        return nullptr;

    // If target gear score is 0, use the lowest available set
    if (targetGearScore == 0)
    {
        GearSetTemplate const* lowest = nullptr;
        for (auto const& pair : tmpl->gearSets)
        {
            if (!lowest || pair.second.targetItemLevel < lowest->targetItemLevel)
                lowest = &pair.second;
        }
        return lowest;
    }

    // Find the closest gear set to target
    GearSetTemplate const* bestMatch = nullptr;
    uint32 bestDiff = std::numeric_limits<uint32>::max();

    for (auto const& pair : tmpl->gearSets)
    {
        uint32 setGS = pair.second.actualGearScore;
        uint32 diff = (setGS > targetGearScore) ? (setGS - targetGearScore) : (targetGearScore - setGS);

        if (diff < bestDiff)
        {
            bestDiff = diff;
            bestMatch = &pair.second;
        }
    }

    return bestMatch;
}

void BotPostLoginConfigurator::ResetStats()
{
    _stats.totalConfigured.store(0);
    _stats.successfulConfigs.store(0);
    _stats.failedConfigs.store(0);
    _stats.pendingConfigs.store(0);
    _stats.totalConfigTimeMs.store(0);
}

} // namespace Playerbot
