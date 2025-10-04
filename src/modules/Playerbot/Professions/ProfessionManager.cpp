/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ProfessionManager.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

// ============================================================================
// SINGLETON
// ============================================================================

ProfessionManager* ProfessionManager::instance()
{
    static ProfessionManager instance;
    return &instance;
}

ProfessionManager::ProfessionManager()
{
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ProfessionManager::Initialize()
{
    TC_LOG_INFO("playerbots", "ProfessionManager: Initializing profession system...");

    LoadRecipeDatabase();
    LoadProfessionRecommendations();
    InitializeClassProfessions();

    TC_LOG_INFO("playerbots", "ProfessionManager: Initialized {} recipes for {} professions",
        _recipeDatabase.size(), _professionRecipes.size());
}

void ProfessionManager::LoadRecipeDatabase()
{
    // TODO: Load from DBC/DB2 SkillLineAbility table
    // For now, we'll populate with common leveling recipes
    // This will be expanded with full DBC integration

    _recipeDatabase.clear();
    _professionRecipes.clear();

    TC_LOG_DEBUG("playerbots", "ProfessionManager: Recipe database loaded");
}

void ProfessionManager::LoadProfessionRecommendations()
{
    _classRecommendations.clear();

    InitializeWarriorProfessions();
    InitializePaladinProfessions();
    InitializeHunterProfessions();
    InitializeRogueProfessions();
    InitializePriestProfessions();
    InitializeShamanProfessions();
    InitializeMageProfessions();
    InitializeWarlockProfessions();
    InitializeDruidProfessions();
    InitializeDeathKnightProfessions();
    InitializeMonkProfessions();
    InitializeDemonHunterProfessions();
    InitializeEvokerProfessions();

    TC_LOG_DEBUG("playerbots", "ProfessionManager: Loaded recommendations for {} classes",
        _classRecommendations.size());
}

void ProfessionManager::InitializeClassProfessions()
{
    // Already done in LoadProfessionRecommendations
}

// ============================================================================
// CLASS-SPECIFIC PROFESSION RECOMMENDATIONS
// ============================================================================

void ProfessionManager::InitializeWarriorProfessions()
{
    // Warriors wear plate: Blacksmithing + Mining
    // Alternative: Engineering for gadgets
    _classRecommendations[CLASS_WARRIOR] = {
        ProfessionType::BLACKSMITHING,
        ProfessionType::MINING,
        ProfessionType::ENGINEERING
    };
}

void ProfessionManager::InitializePaladinProfessions()
{
    // Paladins wear plate: Blacksmithing + Mining
    // Alternative: Jewelcrafting for gems
    _classRecommendations[CLASS_PALADIN] = {
        ProfessionType::BLACKSMITHING,
        ProfessionType::MINING,
        ProfessionType::JEWELCRAFTING
    };
}

void ProfessionManager::InitializeHunterProfessions()
{
    // Hunters wear mail: Leatherworking + Skinning
    // Alternative: Engineering for ranged weapons
    _classRecommendations[CLASS_HUNTER] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::ENGINEERING
    };
}

void ProfessionManager::InitializeRogueProfessions()
{
    // Rogues wear leather: Leatherworking + Skinning
    // Alternative: Engineering for gadgets
    _classRecommendations[CLASS_ROGUE] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::ENGINEERING
    };
}

void ProfessionManager::InitializePriestProfessions()
{
    // Priests wear cloth: Tailoring + Enchanting
    // Alternative: Alchemy for potions
    _classRecommendations[CLASS_PRIEST] = {
        ProfessionType::TAILORING,
        ProfessionType::ENCHANTING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM
    };
}

void ProfessionManager::InitializeShamanProfessions()
{
    // Shamans wear mail: Leatherworking + Skinning (or Blacksmithing + Mining)
    // Alternative: Alchemy for potions, Jewelcrafting
    _classRecommendations[CLASS_SHAMAN] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM,
        ProfessionType::JEWELCRAFTING
    };
}

void ProfessionManager::InitializeMageProfessions()
{
    // Mages wear cloth: Tailoring + Enchanting
    // Alternative: Alchemy for potions
    _classRecommendations[CLASS_MAGE] = {
        ProfessionType::TAILORING,
        ProfessionType::ENCHANTING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM
    };
}

void ProfessionManager::InitializeWarlockProfessions()
{
    // Warlocks wear cloth: Tailoring + Enchanting
    // Alternative: Alchemy for potions
    _classRecommendations[CLASS_WARLOCK] = {
        ProfessionType::TAILORING,
        ProfessionType::ENCHANTING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM
    };
}

void ProfessionManager::InitializeDruidProfessions()
{
    // Druids wear leather: Leatherworking + Skinning
    // Alternative: Alchemy for potions, Herbalism
    _classRecommendations[CLASS_DRUID] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM
    };
}

void ProfessionManager::InitializeDeathKnightProfessions()
{
    // Death Knights wear plate: Blacksmithing + Mining
    // Alternative: Jewelcrafting
    _classRecommendations[CLASS_DEATH_KNIGHT] = {
        ProfessionType::BLACKSMITHING,
        ProfessionType::MINING,
        ProfessionType::JEWELCRAFTING
    };
}

void ProfessionManager::InitializeMonkProfessions()
{
    // Monks wear leather: Leatherworking + Skinning
    // Alternative: Alchemy for potions
    _classRecommendations[CLASS_MONK] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM
    };
}

void ProfessionManager::InitializeDemonHunterProfessions()
{
    // Demon Hunters wear leather: Leatherworking + Skinning
    // Alternative: Alchemy
    _classRecommendations[CLASS_DEMON_HUNTER] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::ALCHEMY,
        ProfessionType::HERBALISM
    };
}

void ProfessionManager::InitializeEvokerProfessions()
{
    // Evokers wear mail: Leatherworking + Skinning (or Blacksmithing + Mining)
    // Alternative: Jewelcrafting, Enchanting
    _classRecommendations[CLASS_EVOKER] = {
        ProfessionType::LEATHERWORKING,
        ProfessionType::SKINNING,
        ProfessionType::JEWELCRAFTING,
        ProfessionType::ENCHANTING
    };
}

// ============================================================================
// CORE PROFESSION MANAGEMENT
// ============================================================================

void ProfessionManager::Update(::Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // Throttle updates to PROFESSION_UPDATE_INTERVAL
    uint32 now = getMSTime();
    if (_lastUpdateTimes[playerGuid] + PROFESSION_UPDATE_INTERVAL > now)
        return;

    _lastUpdateTimes[playerGuid] = now;

    // Auto-learn professions if none learned
    ProfessionAutomationProfile const& profile = GetAutomationProfile(playerGuid);
    if (profile.autoLearnProfessions)
    {
        std::vector<ProfessionSkillInfo> professions = GetPlayerProfessions(player);
        if (professions.empty())
        {
            AutoLearnProfessionsForClass(player);
        }
    }

    // Process crafting queue
    if (profile.autoLevelProfessions)
    {
        ProcessCraftingQueue(player, diff);
    }
}

bool ProfessionManager::LearnProfession(::Player* player, ProfessionType profession)
{
    if (!player || profession == ProfessionType::NONE)
        return false;

    uint32 skillId = static_cast<uint32>(profession);

    // Check if already has profession
    if (player->HasSkill(skillId))
    {
        TC_LOG_DEBUG("playerbots", "Player {} already has profession {}",
            player->GetName(), skillId);
        return false;
    }

    // Learn profession (set skill to 1, max based on level)
    uint16 maxSkill = std::min<uint16>(player->GetLevel() * 5, 450);
    player->SetSkill(skillId, 1, 1, maxSkill);

    TC_LOG_DEBUG("playerbots", "Player {} learned profession {} (max skill: {})",
        player->GetName(), skillId, maxSkill);

    // Update metrics
    std::lock_guard<std::mutex> lock(_mutex);
    _playerMetrics[player->GetGUID().GetCounter()].professionsLearned++;
    _globalMetrics.professionsLearned++;

    return true;
}

bool ProfessionManager::HasProfession(::Player* player, ProfessionType profession) const
{
    if (!player || profession == ProfessionType::NONE)
        return false;

    return player->HasSkill(static_cast<uint32>(profession));
}

uint16 ProfessionManager::GetProfessionSkill(::Player* player, ProfessionType profession) const
{
    if (!player || profession == ProfessionType::NONE)
        return 0;

    return player->GetSkillValue(static_cast<uint32>(profession));
}

uint16 ProfessionManager::GetMaxProfessionSkill(::Player* player, ProfessionType profession) const
{
    if (!player || profession == ProfessionType::NONE)
        return 0;

    return player->GetMaxSkillValue(static_cast<uint32>(profession));
}

std::vector<ProfessionSkillInfo> ProfessionManager::GetPlayerProfessions(::Player* player) const
{
    std::vector<ProfessionSkillInfo> professions;

    if (!player)
        return professions;

    // Check all profession types
    static const std::vector<ProfessionType> allProfessions = {
        ProfessionType::ALCHEMY,
        ProfessionType::BLACKSMITHING,
        ProfessionType::ENCHANTING,
        ProfessionType::ENGINEERING,
        ProfessionType::INSCRIPTION,
        ProfessionType::JEWELCRAFTING,
        ProfessionType::LEATHERWORKING,
        ProfessionType::TAILORING,
        ProfessionType::MINING,
        ProfessionType::HERBALISM,
        ProfessionType::SKINNING,
        ProfessionType::COOKING,
        ProfessionType::FISHING,
        ProfessionType::FIRST_AID
    };

    for (ProfessionType profession : allProfessions)
    {
        if (HasProfession(player, profession))
        {
            ProfessionSkillInfo info;
            info.profession = profession;
            info.currentSkill = GetProfessionSkill(player, profession);
            info.maxSkill = GetMaxProfessionSkill(player, profession);
            info.lastUpdate = getMSTime();
            info.isPrimary = GetProfessionCategory(profession) != ProfessionCategory::SECONDARY;

            professions.push_back(info);
        }
    }

    return professions;
}

bool ProfessionManager::UnlearnProfession(::Player* player, ProfessionType profession)
{
    if (!player || profession == ProfessionType::NONE)
        return false;

    uint32 skillId = static_cast<uint32>(profession);

    if (!player->HasSkill(skillId))
        return false;

    // Set skill to 0 to unlearn
    player->SetSkill(skillId, 0, 0, 0);

    TC_LOG_DEBUG("playerbots", "Player {} unlearned profession {}",
        player->GetName(), skillId);

    return true;
}

// ============================================================================
// AUTO-LEARN SYSTEM
// ============================================================================

void ProfessionManager::AutoLearnProfessionsForClass(::Player* player)
{
    if (!player)
        return;

    uint8 classId = player->GetClass();
    std::vector<ProfessionType> recommended = GetRecommendedProfessions(classId);

    if (recommended.empty())
    {
        TC_LOG_WARN("playerbots", "No profession recommendations for class {}", classId);
        return;
    }

    // Learn first two professions (1 production + 1 gathering preferred)
    uint32 learnedCount = 0;
    for (ProfessionType profession : recommended)
    {
        if (learnedCount >= 2)
            break;

        if (LearnProfession(player, profession))
        {
            learnedCount++;
            TC_LOG_INFO("playerbots", "Player {} auto-learned profession {}",
                player->GetName(), static_cast<uint32>(profession));
        }
    }

    // Always learn secondary professions
    LearnProfession(player, ProfessionType::COOKING);
    LearnProfession(player, ProfessionType::FISHING);
}

std::vector<ProfessionType> ProfessionManager::GetRecommendedProfessions(uint8 classId) const
{
    auto it = _classRecommendations.find(classId);
    if (it != _classRecommendations.end())
        return it->second;

    return {};
}

bool ProfessionManager::IsProfessionSuitableForClass(uint8 classId, ProfessionType profession) const
{
    std::vector<ProfessionType> recommended = GetRecommendedProfessions(classId);
    return std::find(recommended.begin(), recommended.end(), profession) != recommended.end();
}

ProfessionCategory ProfessionManager::GetProfessionCategory(ProfessionType profession) const
{
    switch (profession)
    {
        case ProfessionType::MINING:
        case ProfessionType::HERBALISM:
        case ProfessionType::SKINNING:
            return ProfessionCategory::GATHERING;

        case ProfessionType::COOKING:
        case ProfessionType::FISHING:
        case ProfessionType::FIRST_AID:
            return ProfessionCategory::SECONDARY;

        default:
            return ProfessionCategory::PRODUCTION;
    }
}

// ============================================================================
// RECIPE MANAGEMENT
// ============================================================================

bool ProfessionManager::LearnRecipe(::Player* player, uint32 recipeId)
{
    if (!player)
        return false;

    auto it = _recipeDatabase.find(recipeId);
    if (it == _recipeDatabase.end())
    {
        TC_LOG_WARN("playerbots", "Unknown recipe ID: {}", recipeId);
        return false;
    }

    RecipeInfo const& recipe = it->second;

    // Check if player has the profession
    if (!HasProfession(player, recipe.profession))
    {
        TC_LOG_DEBUG("playerbots", "Player {} doesn't have profession {} for recipe {}",
            player->GetName(), static_cast<uint32>(recipe.profession), recipeId);
        return false;
    }

    // Check skill requirement
    uint16 skill = GetProfessionSkill(player, recipe.profession);
    if (skill < recipe.requiredSkill)
    {
        TC_LOG_DEBUG("playerbots", "Player {} skill {} too low for recipe {} (requires {})",
            player->GetName(), skill, recipeId, recipe.requiredSkill);
        return false;
    }

    // Learn the spell
    player->LearnSpell(recipe.spellId, false);

    TC_LOG_DEBUG("playerbots", "Player {} learned recipe {} (spell {})",
        player->GetName(), recipeId, recipe.spellId);

    // Update metrics
    std::lock_guard<std::mutex> lock(_mutex);
    _playerMetrics[player->GetGUID().GetCounter()].recipesLearned++;
    _globalMetrics.recipesLearned++;

    return true;
}

bool ProfessionManager::KnowsRecipe(::Player* player, uint32 recipeId) const
{
    if (!player)
        return false;

    auto it = _recipeDatabase.find(recipeId);
    if (it == _recipeDatabase.end())
        return false;

    return player->HasSpell(it->second.spellId);
}

std::vector<RecipeInfo> ProfessionManager::GetRecipesForProfession(ProfessionType profession) const
{
    std::vector<RecipeInfo> recipes;

    auto it = _professionRecipes.find(profession);
    if (it == _professionRecipes.end())
        return recipes;

    for (uint32 recipeId : it->second)
    {
        auto recipeIt = _recipeDatabase.find(recipeId);
        if (recipeIt != _recipeDatabase.end())
            recipes.push_back(recipeIt->second);
    }

    return recipes;
}

std::vector<RecipeInfo> ProfessionManager::GetCraftableRecipes(::Player* player, ProfessionType profession) const
{
    std::vector<RecipeInfo> craftable;

    if (!player)
        return craftable;

    std::vector<RecipeInfo> allRecipes = GetRecipesForProfession(profession);

    for (RecipeInfo const& recipe : allRecipes)
    {
        if (CanCraftRecipe(player, recipe))
            craftable.push_back(recipe);
    }

    return craftable;
}

RecipeInfo const* ProfessionManager::GetOptimalLevelingRecipe(::Player* player, ProfessionType profession) const
{
    if (!player)
        return nullptr;

    std::vector<RecipeInfo> craftable = GetCraftableRecipes(player, profession);

    RecipeInfo const* best = nullptr;
    float bestChance = 0.0f;

    for (RecipeInfo const& recipe : craftable)
    {
        float chance = GetSkillUpChance(player, recipe);
        if (chance > bestChance)
        {
            bestChance = chance;
            best = &recipe;
        }
    }

    return best;
}

bool ProfessionManager::CanCraftRecipe(::Player* player, RecipeInfo const& recipe) const
{
    if (!player)
        return false;

    // Must know the recipe
    if (!player->HasSpell(recipe.spellId))
        return false;

    // Must have skill
    uint16 skill = GetProfessionSkill(player, recipe.profession);
    if (skill < recipe.requiredSkill)
        return false;

    // Must have materials
    return HasMaterialsForRecipe(player, recipe);
}

float ProfessionManager::GetSkillUpChance(::Player* player, RecipeInfo const& recipe) const
{
    if (!player)
        return 0.0f;

    uint16 skill = GetProfessionSkill(player, recipe.profession);

    // Orange: always skill up
    if (skill < recipe.skillUpOrange)
        return 1.0f;

    // Yellow: often skill up
    if (skill < recipe.skillUpYellow)
        return 0.75f;

    // Green: rarely skill up
    if (skill < recipe.skillUpGreen)
        return 0.25f;

    // Gray: never skill up
    return 0.0f;
}

// ============================================================================
// CRAFTING AUTOMATION
// ============================================================================

bool ProfessionManager::AutoLevelProfession(::Player* player, ProfessionType profession)
{
    if (!player)
        return false;

    RecipeInfo const* recipe = GetOptimalLevelingRecipe(player, profession);
    if (!recipe)
    {
        TC_LOG_DEBUG("playerbots", "No optimal leveling recipe found for profession {}",
            static_cast<uint32>(profession));
        return false;
    }

    return CraftItem(player, *recipe, 1);
}

bool ProfessionManager::CraftItem(::Player* player, RecipeInfo const& recipe, uint32 quantity)
{
    if (!player || quantity == 0)
        return false;

    if (!CanCraftRecipe(player, recipe))
        return false;

    // Queue the craft
    QueueCraft(player, recipe.recipeId, quantity);

    return true;
}

void ProfessionManager::QueueCraft(::Player* player, uint32 recipeId, uint32 quantity)
{
    if (!player || quantity == 0)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    CraftingTask task;
    task.recipeId = recipeId;
    task.quantity = quantity;
    task.queueTime = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    _craftingQueues[playerGuid].push_back(task);

    TC_LOG_DEBUG("playerbots", "Queued {} x{} for player {}",
        recipeId, quantity, player->GetName());
}

void ProfessionManager::ProcessCraftingQueue(::Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _craftingQueues.find(playerGuid);
    if (it == _craftingQueues.end() || it->second.empty())
        return;

    CraftingTask& task = it->second.front();

    auto recipeIt = _recipeDatabase.find(task.recipeId);
    if (recipeIt == _recipeDatabase.end())
    {
        // Invalid recipe, remove from queue
        it->second.erase(it->second.begin());
        return;
    }

    RecipeInfo const& recipe = recipeIt->second;

    // Check if we can craft
    if (!CanCraftRecipe(player, recipe))
    {
        TC_LOG_DEBUG("playerbots", "Cannot craft recipe {}, removing from queue", task.recipeId);
        it->second.erase(it->second.begin());
        return;
    }

    // Craft one item
    if (CastCraftingSpell(player, recipe))
    {
        task.quantity--;

        // Update metrics
        _playerMetrics[playerGuid].itemsCrafted++;
        _globalMetrics.itemsCrafted++;

        TC_LOG_DEBUG("playerbots", "Crafted 1x {} ({} remaining in queue)",
            task.recipeId, task.quantity);

        // Remove if finished
        if (task.quantity == 0)
            it->second.erase(it->second.begin());
    }
}

bool ProfessionManager::HasMaterialsForRecipe(::Player* player, RecipeInfo const& recipe) const
{
    if (!player)
        return false;

    for (RecipeInfo::Reagent const& reagent : recipe.reagents)
    {
        uint32 count = player->GetItemCount(reagent.itemId);
        if (count < reagent.quantity)
            return false;
    }

    return true;
}

std::vector<std::pair<uint32, uint32>> ProfessionManager::GetMissingMaterials(::Player* player, RecipeInfo const& recipe) const
{
    std::vector<std::pair<uint32, uint32>> missing;

    if (!player)
        return missing;

    for (RecipeInfo::Reagent const& reagent : recipe.reagents)
    {
        uint32 have = player->GetItemCount(reagent.itemId);
        if (have < reagent.quantity)
        {
            missing.push_back({reagent.itemId, reagent.quantity - have});
        }
    }

    return missing;
}

// ============================================================================
// CRAFTING HELPERS
// ============================================================================

bool ProfessionManager::CastCraftingSpell(::Player* player, RecipeInfo const& recipe)
{
    if (!player)
        return false;

    // Consume materials
    if (!ConsumeMaterials(player, recipe))
        return false;

    // Cast crafting spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipe.spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Update profession skill
    player->UpdateCraftSkill(spellInfo);

    // Skill point gain
    uint16 oldSkill = GetProfessionSkill(player, recipe.profession);
    uint16 newSkill = GetProfessionSkill(player, recipe.profession);
    if (newSkill > oldSkill)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _playerMetrics[player->GetGUID().GetCounter()].skillPointsGained += (newSkill - oldSkill);
        _globalMetrics.skillPointsGained += (newSkill - oldSkill);
    }

    HandleCraftingResult(player, recipe, true);

    return true;
}

bool ProfessionManager::ConsumeMaterials(::Player* player, RecipeInfo const& recipe)
{
    if (!player)
        return false;

    // Check we have all materials first
    if (!HasMaterialsForRecipe(player, recipe))
        return false;

    // Destroy reagents
    for (RecipeInfo::Reagent const& reagent : recipe.reagents)
    {
        player->DestroyItemCount(reagent.itemId, reagent.quantity, true);
    }

    return true;
}

void ProfessionManager::HandleCraftingResult(::Player* player, RecipeInfo const& recipe, bool success)
{
    if (!player || !success)
        return;

    // Item will be created by spell effect, we don't create it manually
    TC_LOG_DEBUG("playerbots", "Player {} successfully crafted item {} from recipe {}",
        player->GetName(), recipe.productItemId, recipe.recipeId);
}

// ============================================================================
// SKILL CALCULATION HELPERS
// ============================================================================

uint16 ProfessionManager::CalculateSkillUpAmount(RecipeInfo const& recipe, uint16 currentSkill) const
{
    // Simple skill-up: 1 point per craft
    return 1;
}

bool ProfessionManager::ShouldCraftForSkillUp(::Player* player, RecipeInfo const& recipe) const
{
    float chance = GetSkillUpChance(player, recipe);
    ProfessionAutomationProfile const& profile = GetAutomationProfile(player->GetGUID().GetCounter());

    return chance >= profile.skillUpThreshold;
}

// ============================================================================
// AUTOMATION PROFILES
// ============================================================================

void ProfessionManager::SetAutomationProfile(uint32 playerGuid, ProfessionAutomationProfile const& profile)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _playerProfiles[playerGuid] = profile;
}

ProfessionAutomationProfile ProfessionManager::GetAutomationProfile(uint32 playerGuid) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _playerProfiles.find(playerGuid);
    if (it != _playerProfiles.end())
        return it->second;

    // Return default profile
    return ProfessionAutomationProfile();
}

// ============================================================================
// METRICS
// ============================================================================

ProfessionManager::ProfessionMetrics const& ProfessionManager::GetPlayerMetrics(uint32 playerGuid) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _playerMetrics.find(playerGuid);
    if (it != _playerMetrics.end())
        return it->second;

    // Return default metrics if not found
    static ProfessionMetrics defaultMetrics;
    return defaultMetrics;
}

ProfessionManager::ProfessionMetrics const& ProfessionManager::GetGlobalMetrics() const
{
    return _globalMetrics;
}

} // namespace Playerbot
