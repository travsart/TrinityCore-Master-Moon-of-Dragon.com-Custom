/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BotTalentManager.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Config.h"
#include "Group/RoleDefinitions.h"
#include "DB2Stores.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <random>

namespace Playerbot
{

// ====================================================================
// SINGLETON
// ====================================================================

BotTalentManager* BotTalentManager::instance()
{
    static BotTalentManager instance;
    return &instance;
}

// ====================================================================
// INITIALIZATION
// ====================================================================

bool BotTalentManager::LoadLoadouts()
{
    TC_LOG_INFO("playerbot", "BotTalentManager: Loading talent loadouts...");

    auto startTime = ::std::chrono::steady_clock::now();

    // Clear existing data
    _loadoutCache.clear();
    _classSpecs.clear();
    _stats = TalentStats{};

    // Load from database
    LoadLoadoutsFromDatabase();

    // If database is empty, build default loadouts
    if (_loadoutCache.empty())
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: No loadouts in database, building defaults...");
        BuildDefaultLoadouts();
    }

    // Build class->specs lookup
    for (auto const& [key, loadout] : _loadoutCache)
    {
        auto& specs = _classSpecs[loadout.classId];
        if (::std::find(specs.begin(), specs.end(), loadout.specId) == specs.end())
        {
            specs.push_back(loadout.specId);
        }
    }

    // Validate loadouts
    ValidateLoadouts();

    // Calculate statistics
    _stats.totalLoadouts = static_cast<uint32>(_loadoutCache.size());
    for (auto const& [key, loadout] : _loadoutCache)
    {
        _stats.loadoutsPerClass[loadout.classId]++;
        if (loadout.HasHeroTalents())
            ++_stats.loadoutsWithHeroTalents;

        _stats.averageTalentsPerLoadout += loadout.GetTalentCount();
    }

    if (_stats.totalLoadouts > 0)
        _stats.averageTalentsPerLoadout /= _stats.totalLoadouts;

    auto endTime = ::std::chrono::steady_clock::now();
    auto loadTime = ::std::chrono::duration_cast<::std::chrono::milliseconds>(endTime - startTime).count();

    // Mark as initialized
    _initialized.store(true, ::std::memory_order_release);

    TC_LOG_INFO("playerbot", "BotTalentManager: Loaded {} talent loadouts in {}ms",
        _stats.totalLoadouts, loadTime);

    PrintLoadoutReport();

    return true;
}

void BotTalentManager::ReloadLoadouts()
{
    TC_LOG_INFO("playerbot", "BotTalentManager: Reloading loadouts...");
    _initialized.store(false, ::std::memory_order_release);
    LoadLoadouts();
}

void BotTalentManager::LoadLoadoutsFromDatabase()
{
    TC_LOG_DEBUG("playerbot", "BotTalentManager: Loading loadouts from database...");

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_BOT_TALENT_LOADOUTS);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: No loadouts found in database (table may not exist)");
        return;
    }

    uint32 loadedCount = 0;

    do
    {
        Field* fields = result->Fetch();

        TalentLoadout loadout;
        loadout.classId = fields[0].GetUInt8();
        loadout.specId = fields[1].GetUInt8();
        loadout.minLevel = fields[2].GetUInt32();
        loadout.maxLevel = fields[3].GetUInt32();
        loadout.description = fields[6].GetString();

        // Parse talent string (comma-separated talent entry IDs)
        ::std::string talentString = fields[4].GetString();
        if (!talentString.empty())
        {
            ::std::istringstream ss(talentString);
            ::std::string token;
            while (::std::getline(ss, token, ','))
            {
                uint32 talentEntry = ::std::stoul(token);
                loadout.talentEntries.push_back(talentEntry);
            }
        }

        // Parse hero talent string
        ::std::string heroTalentString = fields[5].GetString();
        if (!heroTalentString.empty())
        {
            ::std::istringstream ss(heroTalentString);
            ::std::string token;
            while (::std::getline(ss, token, ','))
            {
                uint32 heroTalentEntry = ::std::stoul(token);
                loadout.heroTalentEntries.push_back(heroTalentEntry);
            }
        }

        // Store in cache
        uint32 key = MakeLoadoutKey(loadout.classId, loadout.specId, loadout.minLevel);
        _loadoutCache[key] = ::std::move(loadout);
        ++loadedCount;

    } while (result->NextRow());

    TC_LOG_INFO("playerbot", "BotTalentManager: Loaded {} loadouts from database", loadedCount);
}

void BotTalentManager::BuildDefaultLoadouts()
{
    TC_LOG_INFO("playerbot", "BotTalentManager: Building default loadouts with TrinityCore talent data...");

    // Build loadouts for all class/spec combinations using sTalentStore
    for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
    {
        if (cls == CLASS_NONE)
            continue;

        // Get available specs from RoleDefinitions
        try
        {
            auto const& classData = RoleDefinitions::GetClassData(cls);

            for (auto const& specData : classData.specializations)
            {
                // Create loadouts for level brackets (1-10, 11-20, ... 71-80)
                for (uint32 minLevel = 1; minLevel <= 80; minLevel += 10)
                {
                    uint32 maxLevel = ::std::min(minLevel + 9, 80u);

                    TalentLoadout loadout;
                    loadout.classId = cls;
                    loadout.specId = specData.specId;
                    loadout.minLevel = minLevel;
                    loadout.maxLevel = maxLevel;
                    loadout.description = "Auto-generated from TrinityCore talent data";

                    // Collect all talents for this class/spec from sTalentStore
                    ::std::vector<TalentEntry const*> specTalents;
                    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
                    {
                        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
                        if (!talentInfo)
                            continue;

                        // Check if talent belongs to this class
                        // TrinityCore 11.x uses ClassID directly
                        if (talentInfo->ClassID != cls)
                            continue;

                        // Check if talent belongs to this spec (SpecID in TalentEntry)
                        // SpecID 0 means class-wide talent
                        if (talentInfo->SpecID != 0 && talentInfo->SpecID != specData.specId)
                            continue;

                        specTalents.push_back(talentInfo);
                    }

                    // Sort talents by tier row for logical progression
                    ::std::sort(specTalents.begin(), specTalents.end(),
                        [](TalentEntry const* a, TalentEntry const* b)
                        {
                            return a->TierID < b->TierID;
                        });

                    // Add appropriate number of talents based on level bracket
                    // Calculate max talents for this level
                    uint32 maxTalentPoints = CalculateTalentPointsForLevel(maxLevel);
                    uint32 addedTalents = 0;

                    for (TalentEntry const* talent : specTalents)
                    {
                        if (addedTalents >= maxTalentPoints)
                            break;

                        // Check if spell exists and is valid
                        if (talent->SpellID == 0)
                            continue;

                        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(talent->SpellID, DIFFICULTY_NONE);
                        if (!spellInfo)
                            continue;

                        loadout.talentEntries.push_back(talent->ID);
                        ++addedTalents;
                    }

                    // Hero talents for level 71+
                    if (minLevel >= 71)
                    {
                        // Hero talents are a separate system in 11.x
                        // They're associated with "Hero Talent Trees"
                        // For now, mark as ready for hero talents
                        loadout.description = "Auto-generated with hero talent support";
                    }

                    uint32 key = MakeLoadoutKey(cls, specData.specId, minLevel);
                    _loadoutCache[key] = ::std::move(loadout);
                }
            }
        }
        catch (...)
        {
            TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to get class data for class {}", cls);
        }
    }

    TC_LOG_INFO("playerbot", "BotTalentManager: Built {} default loadouts with {} talents from TrinityCore DB2",
        _loadoutCache.size(), sTalentStore.GetNumRows());
}

uint32 BotTalentManager::CalculateTalentPointsForLevel(uint32 level) const
{
    // TrinityCore 11.x Dragonflight/War Within talent system
    // Players get talent points starting from level 10
    // Class talents: levels 10-70 (about 31 points)
    // Spec talents: levels 10-70 (about 30 points)
    // Hero talents: levels 71-80 (10 points)

    if (level < 10)
        return 0;

    // DESIGN NOTE: Simplified talent point calculation
    // Current behavior: Linear approximation - ~1 point per level from 10-70, plus hero talents 71-80
    // Full implementation should:
    // - Query ChrSpecializationEntry for actual talent point progression per level
    // - Account for class-specific talent tree structures (class vs spec talents)
    // - Reference TalentTabEntry for proper tier unlock levels
    // - Consider PvP talent points as separate progression (if applicable)
    // Reference: TrinityCore DB2 stores (sTalentStore, sChrSpecializationStore)
    uint32 classAndSpecPoints = ::std::min(level - 9, 61u);  // ~61 points at 70

    // Hero talents at 71+
    uint32 heroPoints = (level >= 71) ? ::std::min(level - 70, 10u) : 0;

    return classAndSpecPoints + heroPoints;
}

void BotTalentManager::ValidateLoadouts()
{
    TC_LOG_DEBUG("playerbot", "BotTalentManager: Validating loadouts...");

    uint32 invalidCount = 0;

    for (auto const& [key, loadout] : _loadoutCache)
    {
        // Check level range
    if (loadout.minLevel > loadout.maxLevel || loadout.maxLevel > 80)
        {
            TC_LOG_WARN("playerbot", "BotTalentManager: Invalid level range for class {} spec {} ({}-{})",
                loadout.classId, loadout.specId, loadout.minLevel, loadout.maxLevel);
            ++invalidCount;
        }

        // Check hero talents only for 71+
    if (loadout.HasHeroTalents() && loadout.minLevel < 71)
        {
            TC_LOG_WARN("playerbot", "BotTalentManager: Hero talents found for level {} (class {} spec {})",
                loadout.minLevel, loadout.classId, loadout.specId);
            ++invalidCount;
        }
    }

    if (invalidCount > 0)
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: Found {} invalid loadouts", invalidCount);
    }
    else
    {
        TC_LOG_DEBUG("playerbot", "BotTalentManager: All loadouts validated successfully");
    }
}

// ====================================================================
// SPECIALIZATION SELECTION
// ====================================================================

SpecChoice BotTalentManager::SelectSpecialization(uint8 cls, TeamId faction, uint32 level)
{
    // Wait for initialization
    while (!_initialized.load(::std::memory_order_acquire))
    {
        ::std::this_thread::sleep_for(::std::chrono::milliseconds(10));
    }

    ::std::vector<uint8> availableSpecs = GetAvailableSpecs(cls);
    if (availableSpecs.empty())
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: No available specs for class {}", cls);
        return SpecChoice{};
    }

    // Select by distribution
    uint8 selectedSpec = SelectByDistribution(cls, availableSpecs);

    // Get spec metadata from RoleDefinitions
    try
    {
        auto const& specData = RoleDefinitions::GetSpecializationData(cls, selectedSpec);

        SpecChoice choice;
        choice.specId = selectedSpec;
        choice.specName = specData.name;
        choice.primaryRole = specData.primaryRole;
        choice.confidence = GetSpecPopularity(cls, selectedSpec);

        return choice;
    }
    catch (...)
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to get spec data for class {} spec {}", cls, selectedSpec);
        return SpecChoice{};
    }
}

SpecChoice BotTalentManager::SelectSecondarySpecialization(uint8 cls, TeamId faction, uint32 level, uint8 primarySpec)
{
    ::std::vector<uint8> availableSpecs = GetAvailableSpecs(cls);

    // Remove primary spec from options
    availableSpecs.erase(
        ::std::remove(availableSpecs.begin(), availableSpecs.end(), primarySpec),
        availableSpecs.end()
    );

    if (availableSpecs.empty())
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: No secondary specs available for class {}", cls);
        return SpecChoice{};
    }

    // Select complementary spec
    uint8 selectedSpec = SelectComplementarySpec(cls, primarySpec);

    // Fallback to random if no complementary found
    if (selectedSpec == 0 || ::std::find(availableSpecs.begin(), availableSpecs.end(), selectedSpec) == availableSpecs.end())
    {
        selectedSpec = availableSpecs[rand() % availableSpecs.size()];
    }

    // Get spec metadata
    try
    {
        auto const& specData = RoleDefinitions::GetSpecializationData(cls, selectedSpec);

        SpecChoice choice;
        choice.specId = selectedSpec;
        choice.specName = specData.name;
        choice.primaryRole = specData.primaryRole;
        choice.confidence = 0.7f;  // Lower confidence for secondary

        return choice;
    }
    catch (...)
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to get secondary spec data for class {} spec {}", cls, selectedSpec);
        return SpecChoice{};
    }
}

::std::vector<uint8> BotTalentManager::GetAvailableSpecs(uint8 cls) const
{
    auto it = _classSpecs.find(cls);
    if (it != _classSpecs.end())
        return it->second;

    // Fallback to RoleDefinitions
    ::std::vector<uint8> specs;
    try
    {
        auto const& classData = RoleDefinitions::GetClassData(cls);
        for (auto const& specData : classData.specializations)
        {
            specs.push_back(specData.specId);
        }
    }
    catch (...)
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to get available specs for class {}", cls);
    }

    return specs;
}

uint8 BotTalentManager::SelectByDistribution(uint8 cls, ::std::vector<uint8> const& availableSpecs) const
{
    if (availableSpecs.empty())
        return 0;

    // Popularity-based weighting using spec roles for realistic distribution
    // Tank specs: 15% (less popular but always needed)
    // Healer specs: 20% (needed but fewer players pick)
    // DPS specs: 65% (most popular)
    ::std::vector<float> weights;
    float totalWeight = 0.0f;

    for (uint8 specId : availableSpecs)
    {
        float weight = 1.0f;
        try
        {
            auto const& specData = RoleDefinitions::GetSpecializationData(cls, specId);
            switch (specData.primaryRole)
            {
                case GroupRole::TANK:
                    weight = 0.15f;  // 15% of population
                    break;
                case GroupRole::HEALER:
                    weight = 0.20f;  // 20% of population
                    break;
                case GroupRole::MELEE_DPS:
                case GroupRole::RANGED_DPS:
                default:
                    weight = 0.65f / 2.0f;  // Split DPS between melee/ranged
                    break;
            }
        }
        catch (...)
        {
            // Default weight if spec data unavailable
            weight = 1.0f / availableSpecs.size();
        }

        weights.push_back(weight);
        totalWeight += weight;
    }

    // Normalize weights
    for (float& w : weights)
        w /= totalWeight;

    // Random selection based on weights
    float roll = static_cast<float>(rand()) / RAND_MAX;
    float cumulative = 0.0f;

    for (size_t i = 0; i < availableSpecs.size(); ++i)
    {
        cumulative += weights[i];
        if (roll <= cumulative)
            return availableSpecs[i];
    }

    // Fallback
    return availableSpecs[rand() % availableSpecs.size()];
}

uint8 BotTalentManager::SelectComplementarySpec(uint8 cls, uint8 primarySpec) const
{
    // Intelligent complementary spec selection
    // Prioritize specs with different roles for flexibility
    auto availableSpecs = GetAvailableSpecs(cls);

    // Remove primary spec
    availableSpecs.erase(
        ::std::remove(availableSpecs.begin(), availableSpecs.end(), primarySpec),
        availableSpecs.end()
    );

    if (availableSpecs.empty())
        return 0;

    // Get primary spec's role
    GroupRole primaryRole = GroupRole::MELEE_DPS;
    try
    {
        auto const& primarySpecData = RoleDefinitions::GetSpecializationData(cls, primarySpec);
        primaryRole = primarySpecData.primaryRole;
    }
    catch (...) {}

    // Priority for complementary specs:
    // 1. If primary is DPS, prefer healer/tank for versatility
    // 2. If primary is tank, prefer healer for solo/group flexibility
    // 3. If primary is healer, prefer tank for solo viability

    ::std::vector<::std::pair<uint8, int>> scoredSpecs;
    for (uint8 specId : availableSpecs)
    {
        int score = 0;
        try
        {
            auto const& specData = RoleDefinitions::GetSpecializationData(cls, specId);
            GroupRole role = specData.primaryRole;

            // Score based on role complementarity
            if (primaryRole == GroupRole::TANK)
            {
                if (role == GroupRole::HEALER) score = 3;  // Best: healer for tank
                else if (role != GroupRole::TANK) score = 2;  // Good: DPS
            }
            else if (primaryRole == GroupRole::HEALER)
            {
                if (role == GroupRole::TANK) score = 3;  // Best: tank for healer
                else if (role != GroupRole::HEALER) score = 2;  // Good: DPS
            }
            else  // DPS primary
            {
                if (role == GroupRole::HEALER) score = 3;  // Best: self-heal ability
                else if (role == GroupRole::TANK) score = 2;  // Good: tankiness
                else score = 1;  // OK: different DPS type
            }
        }
        catch (...)
        {
            score = 1;  // Default score
        }

        scoredSpecs.push_back({specId, score});
    }

    // Sort by score descending
    ::std::sort(scoredSpecs.begin(), scoredSpecs.end(),
        [](auto const& a, auto const& b) { return a.second > b.second; });

    // Return highest scored spec
    return scoredSpecs[0].first;
}

float BotTalentManager::GetSpecPopularity(uint8 cls, uint8 specId) const
{
    // Popularity tracking based on role distribution
    // This is used for confidence calculation in spec selection

    float basePopularity = 0.5f;
    try
    {
        auto const& specData = RoleDefinitions::GetSpecializationData(cls, specId);

        // Adjust popularity based on role demand
        // DPS specs are most popular but most competitive
        // Tank/healer specs are less popular but more in-demand
        switch (specData.primaryRole)
        {
            case GroupRole::TANK:
                basePopularity = 0.7f;  // High demand, lower competition
                break;
            case GroupRole::HEALER:
                basePopularity = 0.75f;  // Very high demand, lower competition
                break;
            case GroupRole::MELEE_DPS:
                basePopularity = 0.85f;  // Popular, good performance
                break;
            case GroupRole::RANGED_DPS:
                basePopularity = 0.9f;  // Very popular, safe choice
                break;
            default:
                basePopularity = 0.6f;
                break;
        }

        // Adjust by class meta (some classes have stronger specs)
        // This could be enhanced with actual game data
        if (cls == CLASS_WARRIOR || cls == CLASS_PALADIN || cls == CLASS_DEATH_KNIGHT)
        {
            // Plate classes are generally strong
            basePopularity *= 1.1f;
        }
    }
    catch (...) {}

    return ::std::min(basePopularity, 1.0f);
}

// ====================================================================
// TALENT LOADOUT QUERIES
// ====================================================================

TalentLoadout const* BotTalentManager::GetTalentLoadout(uint8 cls, uint8 specId, uint32 level) const
{
    if (!_initialized.load(::std::memory_order_acquire))
        return nullptr;

    uint32 key = FindBestLoadout(cls, specId, level);
    if (key == 0)
        return nullptr;

    auto it = _loadoutCache.find(key);
    return it != _loadoutCache.end() ? &it->second : nullptr;
}

uint32 BotTalentManager::FindBestLoadout(uint8 cls, uint8 specId, uint32 level) const
{
    // Try exact level bracket first
    uint32 key = MakeLoadoutKey(cls, specId, level);
    if (_loadoutCache.count(key) > 0)
        return key;

    // Search for nearest loadout
    for (auto const& [cacheKey, loadout] : _loadoutCache)
    {
        if (loadout.classId == cls && loadout.specId == specId)
        {
            if (loadout.IsValidForLevel(level))
                return cacheKey;
        }
    }

    return 0;
}

::std::vector<TalentLoadout const*> BotTalentManager::GetAllLoadouts(uint8 cls, uint8 specId) const
{
    ::std::vector<TalentLoadout const*> result;

    for (auto const& [key, loadout] : _loadoutCache)
    {
        if (loadout.classId == cls && loadout.specId == specId)
        {
            result.push_back(&loadout);
        }
    }

    return result;
}

// ====================================================================
// TALENT APPLICATION (MAIN THREAD ONLY)
// ====================================================================

bool BotTalentManager::ApplySpecialization(Player* bot, uint8 specId)
{
    if (!bot)
        return false;

    TC_LOG_DEBUG("playerbot", "BotTalentManager: Applying spec {} to bot {}",
        specId, bot->GetName());

    // Set active specialization
    // NOTE: This must be called BEFORE GiveLevel() for proper spell learning
    bot->SetPrimarySpecialization(specId);
    // Learn specialization spells
    bot->LearnSpecializationSpells();
    TC_LOG_INFO("playerbot", "BotTalentManager: Applied spec {} to bot {}", specId, bot->GetName());

    ++_stats.specsApplied;
    return true;
}
bool BotTalentManager::ApplyTalentLoadout(Player* bot, uint8 specId, uint32 level)
{
    if (!bot)
        return false;

    TC_LOG_DEBUG("playerbot", "BotTalentManager: Applying talent loadout for spec {} level {} to bot {}",
        specId, level, bot->GetName());

    // Get loadout from cache
    TalentLoadout const* loadout = GetTalentLoadout(bot->GetClass(), specId, level);
    if (!loadout)
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: No loadout found for class {} spec {} level {}",
            bot->GetClass(), specId, level);
        return false;
    }

    // Learn regular talents
    uint32 talentsLearned = 0;
    for (uint32 talentEntry : loadout->talentEntries)
    {
        if (LearnTalent(bot, talentEntry))
            ++talentsLearned;
    }

    // Learn hero talents if level 71+
    if (SupportsHeroTalents(level) && loadout->HasHeroTalents())
    {
        for (uint32 heroTalentEntry : loadout->heroTalentEntries)
        {
            if (LearnHeroTalent(bot, heroTalentEntry))
                ++talentsLearned;
        }
    }

    LogTalentApplication(bot, specId, talentsLearned);

    ++_stats.loadoutsApplied;

    return talentsLearned > 0;
}

bool BotTalentManager::ActivateSpecialization(Player* bot, uint8 specIndex)
{
    if (!bot)
        return false;

    TC_LOG_DEBUG("playerbot", "BotTalentManager: Activating spec index {} for bot {}",
        specIndex, bot->GetName());

    // Get the ChrSpecializationEntry for this class and spec index
    ChrSpecializationEntry const* specEntry = sDB2Manager.GetChrSpecializationByIndex(bot->GetClass(), specIndex);
    if (!specEntry)
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: No ChrSpecializationEntry found for class {} spec index {}",
            bot->GetClass(), specIndex);
        return false;
    }

    // Use TrinityCore's ActivateTalentGroup to properly switch specs
    // This handles unlearning old talents, learning new ones, and updating the client
    bot->ActivateTalentGroup(specEntry);

    TC_LOG_INFO("playerbot", "BotTalentManager: Successfully activated spec {} (ID: {}) for bot {}",
        specIndex, specEntry->ID, bot->GetName());

    return true;
}

bool BotTalentManager::SetupBotTalents(Player* bot, uint8 specId, uint32 level)
{
    if (!bot)
        return false;
    TC_LOG_INFO("playerbot", "BotTalentManager: Setting up talents for bot {} (spec {}, level {})",
        bot->GetName(), specId, level);

    // Apply specialization
    if (!ApplySpecialization(bot, specId))
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to apply spec {} to bot {}", specId, bot->GetName());
        return false;
    }

    // Apply talent loadout
    if (!ApplyTalentLoadout(bot, specId, level))
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: Failed to apply talents for bot {}", bot->GetName());
        return false;
    }

    return true;
}

// ====================================================================
// DUAL-SPEC SUPPORT
// ====================================================================

bool BotTalentManager::EnableDualSpec(Player* bot)
{
    if (!bot)
        return false;

    // TODO: TrinityCore dual-spec is built-in to talent system
    // No need to explicitly "enable" it - specs 0 and 1 are always available
    TC_LOG_DEBUG("playerbot", "BotTalentManager: Dual-spec available for bot {}", bot->GetName());

    return true;
}

bool BotTalentManager::SetupDualSpec(Player* bot, uint8 spec1, uint8 spec2, uint32 level)
{
    if (!bot || !SupportsDualSpec(level))
        return false;

    TC_LOG_INFO("playerbot", "BotTalentManager: Setting up dual-spec for bot {} (specs {}/{}, level {})",
        bot->GetName(), spec1, spec2, level);

    // Enable dual-spec
    if (!EnableDualSpec(bot))
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to enable dual-spec for bot {}", bot->GetName());
        return false;
    }

    // Setup spec 1 (primary)
    if (!SetupBotTalents(bot, spec1, level))
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to setup primary spec {} for bot {}", spec1, bot->GetName());
        return false;
    }

    // Activate spec 2
    if (!ActivateSpecialization(bot, 1))
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to activate secondary spec for bot {}", bot->GetName());
        return false;
    }

    // Setup spec 2 (secondary)
    if (!SetupBotTalents(bot, spec2, level))
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to setup secondary spec {} for bot {}", spec2, bot->GetName());
        return false;
    }

    // Return to spec 1
    if (!ActivateSpecialization(bot, 0))
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: Failed to return to primary spec for bot {}", bot->GetName());
    }

    ++_stats.dualSpecsSetup;
    TC_LOG_INFO("playerbot", "BotTalentManager: Successfully setup dual-spec for bot {}", bot->GetName());

    return true;
}

// ====================================================================
// HERO TALENTS
// ====================================================================

bool BotTalentManager::ApplyHeroTalents(Player* bot, uint8 specId, uint32 level)
{
    if (!bot || !SupportsHeroTalents(level))
        return false;

    TC_LOG_DEBUG("playerbot", "BotTalentManager: Applying hero talents for bot {} (spec {}, level {})",
        bot->GetName(), specId, level);

    // Get loadout
    TalentLoadout const* loadout = GetTalentLoadout(bot->GetClass(), specId, level);
    if (!loadout || !loadout->HasHeroTalents())
    {
        TC_LOG_DEBUG("playerbot", "BotTalentManager: No hero talents found for class {} spec {} level {}",
            bot->GetClass(), specId, level);
        return false;
    }

    // Learn hero talents
    uint32 heroTalentsLearned = 0;
    for (uint32 heroTalentEntry : loadout->heroTalentEntries)
    {
        if (LearnHeroTalent(bot, heroTalentEntry))
            ++heroTalentsLearned;
    }

    TC_LOG_INFO("playerbot", "BotTalentManager: Applied {} hero talents to bot {}", heroTalentsLearned, bot->GetName());

    return heroTalentsLearned > 0;
}

// ====================================================================
// TALENT APPLICATION HELPERS
// ====================================================================

bool BotTalentManager::LearnTalent(Player* bot, uint32 talentEntry)
{
    if (!bot)
        return false;

    // Use TrinityCore's LearnTalent API for proper talent learning
    // This validates the talent against the player's class/spec and handles
    // talent point requirements, prerequisite talents, etc.
    int32 spellOnCooldown = 0;
    TalentLearnResult result = bot->LearnTalent(talentEntry, &spellOnCooldown);

    if (result != TALENT_LEARN_OK)
    {
        // Log the failure reason for debugging
        TC_LOG_DEBUG("playerbot", "BotTalentManager: LearnTalent failed for bot {} (talent {}, result {})",
            bot->GetName(), talentEntry, static_cast<int32>(result));

        // Fallback: try to learn the talent's spell directly via AddTalent
        // This is useful for bots that may not have the normal talent point restrictions
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentEntry);
        if (talentInfo)
        {
            // Add talent to the active spec
            if (bot->AddTalent(talentInfo, bot->GetActiveTalentGroup(), true))
            {
                TC_LOG_DEBUG("playerbot", "BotTalentManager: Added talent {} via AddTalent fallback for bot {}",
                    talentEntry, bot->GetName());
                return true;
            }
        }

        return false;
    }

    TC_LOG_DEBUG("playerbot", "BotTalentManager: Successfully learned talent {} for bot {}",
        talentEntry, bot->GetName());
    return true;
}

bool BotTalentManager::LearnHeroTalent(Player* bot, uint32 heroTalentEntry)
{
    if (!bot)
        return false;

    // Hero talents in WoW 11.x (The War Within) use the TraitConfig system
    // which is more complex than the traditional talent system.
    // For bot talent management, we use LearnSpell to directly teach the
    // hero talent's spell effect, bypassing the client-side TraitConfig UI.

    // Verify the spell exists before learning
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(heroTalentEntry, DIFFICULTY_NONE);
    if (!spellInfo)
    {
        TC_LOG_WARN("playerbot", "BotTalentManager: Hero talent spell {} not found for bot {}",
            heroTalentEntry, bot->GetName());
        return false;
    }

    // Check if bot already has this spell
    if (bot->HasSpell(heroTalentEntry))
    {
        TC_LOG_DEBUG("playerbot", "BotTalentManager: Bot {} already has hero talent spell {}",
            bot->GetName(), heroTalentEntry);
        return true;  // Already learned
    }

    // Learn the hero talent spell
    // Using LearnSpell is the appropriate fallback for bots since the full
    // TraitConfig system is designed for player client interaction
    bot->LearnSpell(heroTalentEntry, false);

    // Verify learning succeeded
    if (!bot->HasSpell(heroTalentEntry))
    {
        TC_LOG_ERROR("playerbot", "BotTalentManager: Failed to learn hero talent {} for bot {}",
            heroTalentEntry, bot->GetName());
        return false;
    }

    TC_LOG_DEBUG("playerbot", "BotTalentManager: Successfully learned hero talent {} ({}) for bot {}",
        heroTalentEntry, spellInfo->SpellName ? (*spellInfo->SpellName)[LOCALE_enUS] : "Unknown", bot->GetName());
    return true;
}

void BotTalentManager::LogTalentApplication(Player* bot, uint8 specId, uint32 talentCount)
{
    TC_LOG_INFO("playerbot", "BotTalentManager: Applied {} talents to bot {} (class {}, spec {})",
        talentCount, bot->GetName(), bot->GetClass(), specId);
}

// ====================================================================
// STATISTICS & DEBUGGING
// ====================================================================

void BotTalentManager::PrintLoadoutReport() const
{
    TC_LOG_INFO("playerbot", "====================================");
    TC_LOG_INFO("playerbot", "Bot Talent Manager Loadout Report");
    TC_LOG_INFO("playerbot", "====================================");
    TC_LOG_INFO("playerbot", "Total Loadouts: {}", _stats.totalLoadouts);
    TC_LOG_INFO("playerbot", "Average Talents: {}", _stats.averageTalentsPerLoadout);
    TC_LOG_INFO("playerbot", "Hero Talent Loadouts: {}", _stats.loadoutsWithHeroTalents);
    TC_LOG_INFO("playerbot", "");

    TC_LOG_INFO("playerbot", "Loadouts by Class:");
    for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
    {
        if (_stats.loadoutsPerClass[cls] > 0)
        {
            TC_LOG_INFO("playerbot", "  Class {}: {} loadouts", cls, _stats.loadoutsPerClass[cls]);
        }
    }

    TC_LOG_INFO("playerbot", "");
    TC_LOG_INFO("playerbot", "Runtime Statistics:");
    TC_LOG_INFO("playerbot", "  Specs Applied: {}", _stats.specsApplied);
    TC_LOG_INFO("playerbot", "  Loadouts Applied: {}", _stats.loadoutsApplied);
    TC_LOG_INFO("playerbot", "  Dual-Specs Setup: {}", _stats.dualSpecsSetup);
    TC_LOG_INFO("playerbot", "====================================");
}

::std::string BotTalentManager::GetLoadoutSummary() const
{
    ::std::ostringstream oss;
    oss << "Loadouts: " << _stats.totalLoadouts
        << " | Applied: " << _stats.loadoutsApplied
        << " | Dual-Spec: " << _stats.dualSpecsSetup;
    return oss.str();
}

} // namespace Playerbot
