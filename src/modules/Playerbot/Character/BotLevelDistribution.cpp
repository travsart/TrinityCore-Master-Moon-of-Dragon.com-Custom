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

#include "BotLevelDistribution.h"
#include "Config/PlayerbotConfig.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace Playerbot
{

BotLevelDistribution* BotLevelDistribution::instance()
{
    static BotLevelDistribution instance;
    return &instance;
}

bool BotLevelDistribution::LoadConfig()
{
    TC_LOG_INFO("playerbot", "BotLevelDistribution: Loading configuration...");

    m_enabled = sPlayerbotConfig->GetBool("Playerbot.Population.Enabled", false);
    if (!m_enabled)
    {
        TC_LOG_INFO("playerbot", "BotLevelDistribution: System disabled in config");
        return false;
    }

    m_numBrackets = sPlayerbotConfig->GetInt("Playerbot.Population.NumBrackets", 17);
    m_dynamicDistribution = sPlayerbotConfig->GetBool("Playerbot.Population.DynamicDistribution", false);
    m_realPlayerWeight = sPlayerbotConfig->GetFloat("Playerbot.Population.RealPlayerWeight", 1.0f);
    m_syncFactions = sPlayerbotConfig->GetBool("Playerbot.Population.SyncFactions", false);

    // Clear existing data
    m_allianceBrackets.clear();
    m_hordeBrackets.clear();
    m_allianceLevelMap.clear();
    m_hordeLevelMap.clear();

    // Load brackets for both factions
    LoadBrackets(TEAM_ALLIANCE, "Playerbot.Population.Alliance");
    LoadBrackets(TEAM_HORDE, "Playerbot.Population.Horde");

    // Validate configuration
    if (!ValidateConfig())
    {
        TC_LOG_ERROR("playerbot", "BotLevelDistribution: Configuration validation failed");
        m_loaded = false;
        return false;
    }

    // Normalize percentages to ensure they sum to 100%
    NormalizeBracketPercentages();

    m_loaded = true;
    TC_LOG_INFO("playerbot", "BotLevelDistribution: Loaded {} brackets for Alliance, {} for Horde",
        m_allianceBrackets.size(), m_hordeBrackets.size());

    PrintDistributionReport();
    return true;
}

void BotLevelDistribution::LoadBrackets(TeamId faction, ::std::string const& prefix)
{
    auto& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;
    auto& levelMap = (faction == TEAM_ALLIANCE) ? m_allianceLevelMap : m_hordeLevelMap;

    for (uint32 i = 1; i <= m_numBrackets; ++i)
    {
        ::std::string rangeKey;
        rangeKey.reserve(32);
        rangeKey = prefix;
        rangeKey += ".Range";
        rangeKey += ::std::to_string(i);

        uint32 minLevel = sPlayerbotConfig->GetInt(rangeKey + ".Min", 0);
        uint32 maxLevel = sPlayerbotConfig->GetInt(rangeKey + ".Max", 0);
        float percentage = sPlayerbotConfig->GetFloat(rangeKey + ".Pct", 0.0f);

        // Skip invalid brackets
    if (minLevel == 0 || maxLevel == 0 || percentage == 0.0f)
        {
            TC_LOG_DEBUG("playerbot", "BotLevelDistribution: Skipping invalid bracket {} (min={}, max={}, pct={})",
                i, minLevel, maxLevel, percentage);
            continue;
        }

        // Validate bracket
    if (minLevel > maxLevel)
        {
            TC_LOG_ERROR("playerbot", "BotLevelDistribution: Invalid bracket {} - minLevel ({}) > maxLevel ({})",
                i, minLevel, maxLevel);
            continue;
        }

        if (minLevel > 80 || maxLevel > 80)
        {
            TC_LOG_ERROR("playerbot", "BotLevelDistribution: Invalid bracket {} - levels exceed max (80)",
                i);
            continue;
        }

        // Create bracket
        LevelBracket bracket;
        bracket.minLevel = minLevel;
        bracket.maxLevel = maxLevel;
        bracket.targetPercentage = percentage;
        bracket.faction = faction;

        brackets.push_back(bracket);

        // Build level lookup map
    for (uint32 level = minLevel; level <= maxLevel; ++level)
        {
            levelMap[level] = brackets.size() - 1;
        }

        TC_LOG_DEBUG("playerbot", "BotLevelDistribution: Loaded bracket {}: Levels {}-{}, Target {}%",
            i, minLevel, maxLevel, percentage);
    }
}

bool BotLevelDistribution::ValidateConfig() const
{
    // Check Alliance brackets
    if (m_allianceBrackets.empty())
    {
        TC_LOG_ERROR("playerbot", "BotLevelDistribution: No Alliance brackets configured");
        return false;
    }

    // Check Horde brackets
    if (m_hordeBrackets.empty())
    {
        TC_LOG_ERROR("playerbot", "BotLevelDistribution: No Horde brackets configured");
        return false;
    }

    // Validate percentage sums
    float allianceSum = ::std::accumulate(m_allianceBrackets.begin(), m_allianceBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    float hordeSum = ::std::accumulate(m_hordeBrackets.begin(), m_hordeBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    if (::std::abs(allianceSum - 100.0f) > 1.0f)
    {
        TC_LOG_WARN("playerbot", "BotLevelDistribution: Alliance percentages sum to {:.2f}% (should be 100%)",
            allianceSum);
    }

    if (::std::abs(hordeSum - 100.0f) > 1.0f)
    {
        TC_LOG_WARN("playerbot", "BotLevelDistribution: Horde percentages sum to {:.2f}% (should be 100%)",
            hordeSum);
    }

    // Check for level coverage
    for (uint32 level = 1; level <= 80; ++level)
    {
        if (m_allianceLevelMap.find(level) == m_allianceLevelMap.end())
        {
            TC_LOG_WARN("playerbot", "BotLevelDistribution: Alliance missing coverage for level {}", level);
        }

        if (m_hordeLevelMap.find(level) == m_hordeLevelMap.end())
        {
            TC_LOG_WARN("playerbot", "BotLevelDistribution: Horde missing coverage for level {}", level);
        }
    }

    return true;
}

void BotLevelDistribution::NormalizeBracketPercentages()
{
    // Normalize Alliance
    float allianceSum = ::std::accumulate(m_allianceBrackets.begin(), m_allianceBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    if (allianceSum > 0.0f && ::std::abs(allianceSum - 100.0f) > 0.01f)
    {
        float factor = 100.0f / allianceSum;
        for (auto& bracket : m_allianceBrackets)
        {
            bracket.targetPercentage *= factor;
        }
        TC_LOG_INFO("playerbot", "BotLevelDistribution: Normalized Alliance percentages (sum was {:.2f}%)",
            allianceSum);
    }

    // Normalize Horde
    float hordeSum = ::std::accumulate(m_hordeBrackets.begin(), m_hordeBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    if (hordeSum > 0.0f && ::std::abs(hordeSum - 100.0f) > 0.01f)
    {
        float factor = 100.0f / hordeSum;
        for (auto& bracket : m_hordeBrackets)
        {
            bracket.targetPercentage *= factor;
        }
        TC_LOG_INFO("playerbot", "BotLevelDistribution: Normalized Horde percentages (sum was {:.2f}%)",
            hordeSum);
    }
}

void BotLevelDistribution::ReloadConfig()
{
    TC_LOG_INFO("playerbot", "BotLevelDistribution: Reloading configuration...");
    LoadConfig();
}

LevelBracket const* BotLevelDistribution::SelectBracket(TeamId faction) const
{
    if (!m_loaded)
        return nullptr;

    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;
    if (brackets.empty())
        return nullptr;

    // Use weighted selection based on priority (deviation from target)
    return SelectBracketWeighted(faction);
}

LevelBracket const* BotLevelDistribution::SelectBracketWeighted(TeamId faction) const
{
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;
    if (brackets.empty())
        return nullptr;

    // Calculate total bots for this faction
    uint32 totalBots = ::std::accumulate(brackets.begin(), brackets.end(), 0u,
        [](uint32 sum, LevelBracket const& bracket) { return sum + bracket.GetCount(); });

    // If no bots yet, use target distribution
    if (totalBots == 0)
    {
        float random = static_cast<float>(rand()) / RAND_MAX * 100.0f;
        float cumulative = 0.0f;

        for (auto const& bracket : brackets)
        {
            cumulative += bracket.targetPercentage;
            if (random <= cumulative)
                return &bracket;
        }

        return &brackets.back();
    }

    // Calculate priorities (negative deviation = needs more bots)
    ::std::vector<float> priorities;
    priorities.reserve(brackets.size());

    for (auto const& bracket : brackets)
    {
        float priority = bracket.GetSelectionPriority(totalBots);
        // Boost priority for underpopulated brackets
    if (priority > 0.0f)
            priority *= 2.0f;  // Double weight for underpopulated
        else
            priority = 0.01f;  // Minimal chance for overpopulated

        priorities.push_back(priority);
    }

    // Normalize priorities to probabilities
    float totalPriority = ::std::accumulate(priorities.begin(), priorities.end(), 0.0f);
    if (totalPriority <= 0.0f)
        return &brackets[rand() % brackets.size()];

    // Select bracket by weighted random
    float random = static_cast<float>(rand()) / RAND_MAX * totalPriority;
    float cumulative = 0.0f;

    for (size_t i = 0; i < brackets.size(); ++i)
    {
        cumulative += priorities[i];
        if (random <= cumulative)
            return &brackets[i];
    }

    return &brackets.back();
}

LevelBracket const* BotLevelDistribution::GetBracketForLevel(uint32 level, TeamId faction) const
{
    if (!m_loaded || level == 0 || level > 80)
        return nullptr;

    auto const& levelMap = (faction == TEAM_ALLIANCE) ? m_allianceLevelMap : m_hordeLevelMap;
    auto it = levelMap.find(level);
    if (it == levelMap.end())
        return nullptr;

    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;
    if (it->second >= brackets.size())
        return nullptr;

    return &brackets[it->second];
}

void BotLevelDistribution::IncrementBracket(uint32 level, TeamId faction)
{
    if (auto const* bracket = GetBracketForLevel(level, faction))
        bracket->IncrementCount();
}

void BotLevelDistribution::DecrementBracket(uint32 level, TeamId faction)
{
    if (auto const* bracket = GetBracketForLevel(level, faction))
        bracket->DecrementCount();
}

void BotLevelDistribution::RecalculateDistribution()
{
    TC_LOG_INFO("playerbot", "BotLevelDistribution: Recalculating distribution from active bot sessions...");

    // Phase 1: Reset all bracket counters to 0
    for (auto& bracket : m_allianceBrackets)
    {
        bracket.SetCount(0);
    }
    for (auto& bracket : m_hordeBrackets)
    {
        bracket.SetCount(0);
    }

    // Phase 2: Count bots by level and faction from BotSessionMgr
    // Get all active sessions and count bots
    uint32 allianceTotal = 0;
    uint32 hordeTotal = 0;
    uint32 skippedNoPlayer = 0;
    uint32 skippedInvalidLevel = 0;

    // Access the session manager to iterate sessions
    // Note: BotSessionMgr provides GetActiveSessionCount() but no direct iteration
    // We need to query the database for bot character levels and factions instead
    // This is more reliable as it doesn't depend on session state

    // Query database for all bot characters with their levels and factions
    // Use the characters database to get accurate counts
    QueryResult result = CharacterDatabase.Query(
        "SELECT c.level, c.race FROM characters c "
        "JOIN account a ON c.account = a.id "
        "WHERE a.battlenet_account IN (SELECT id FROM battlenet_accounts WHERE email LIKE 'bot%@bot.bot') "
        "AND c.online = 1"
    );

    if (!result)
    {
        TC_LOG_DEBUG("playerbot", "BotLevelDistribution: No online bot characters found in database");

        // Alternative: Try to get counts from config defaults if no online bots
        // This ensures the system has valid baseline data
        DistributionStats stats = GetDistributionStats();
        TC_LOG_INFO("playerbot", "BotLevelDistribution: Recalculation complete - Alliance: {}, Horde: {}, Total: {}",
            stats.allianceBots, stats.hordeBots, stats.totalBots);
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint8 level = fields[0].GetUInt8();
        uint8 race = fields[1].GetUInt8();

        // Validate level range
        if (level == 0 || level > 80)
        {
            ++skippedInvalidLevel;
            continue;
        }

        // Determine faction from race
        // Alliance races: Human(1), Dwarf(3), Night Elf(4), Gnome(7), Draenei(11), Worgen(22)
        // Horde races: Orc(2), Undead(5), Tauren(6), Troll(8), Blood Elf(10), Goblin(9)
        // Pandaren(24,25,26) can be either faction
        TeamId faction;
        switch (race)
        {
            case 1:  // Human
            case 3:  // Dwarf
            case 4:  // Night Elf
            case 7:  // Gnome
            case 11: // Draenei
            case 22: // Worgen
            case 25: // Alliance Pandaren
            case 29: // Void Elf
            case 30: // Lightforged Draenei
            case 32: // Kul Tiran
            case 34: // Dark Iron Dwarf
            case 37: // Mechagnome
                faction = TEAM_ALLIANCE;
                break;
            case 2:  // Orc
            case 5:  // Undead
            case 6:  // Tauren
            case 8:  // Troll
            case 9:  // Goblin
            case 10: // Blood Elf
            case 26: // Horde Pandaren
            case 27: // Nightborne
            case 28: // Highmountain Tauren
            case 31: // Zandalari Troll
            case 35: // Mag'har Orc
            case 36: // Vulpera
                faction = TEAM_HORDE;
                break;
            default:
                // Neutral pandaren or unknown race - default to Alliance
                faction = TEAM_ALLIANCE;
                TC_LOG_DEBUG("playerbot", "BotLevelDistribution: Unknown race {} defaulting to Alliance", race);
                break;
        }

        // Find and increment the appropriate bracket
        LevelBracket const* bracket = GetBracketForLevel(level, faction);
        if (bracket)
        {
            bracket->IncrementCount();
            if (faction == TEAM_ALLIANCE)
                ++allianceTotal;
            else
                ++hordeTotal;
        }
        else
        {
            TC_LOG_DEBUG("playerbot", "BotLevelDistribution: No bracket found for level {} faction {}",
                level, faction == TEAM_ALLIANCE ? "Alliance" : "Horde");
        }
    } while (result->NextRow());

    // Phase 3: Log summary
    TC_LOG_INFO("playerbot", "BotLevelDistribution: Recalculation complete - Alliance: {}, Horde: {}, Total: {}",
        allianceTotal, hordeTotal, allianceTotal + hordeTotal);

    if (skippedNoPlayer > 0)
    {
        TC_LOG_DEBUG("playerbot", "BotLevelDistribution: Skipped {} sessions with no player", skippedNoPlayer);
    }
    if (skippedInvalidLevel > 0)
    {
        TC_LOG_DEBUG("playerbot", "BotLevelDistribution: Skipped {} characters with invalid level", skippedInvalidLevel);
    }

    // Phase 4: Log distribution status
    DistributionStats stats = GetDistributionStats();
    TC_LOG_INFO("playerbot", "BotLevelDistribution: Distribution balance - {}/{} brackets within tolerance, avg deviation: {:.1f}%",
        stats.bracketsWithinTolerance, stats.totalBrackets, stats.averageDeviation * 100.0f);

    if (!stats.mostUnderpopulatedBracket.empty())
    {
        TC_LOG_INFO("playerbot", "BotLevelDistribution: Most underpopulated bracket: {}", stats.mostUnderpopulatedBracket);
    }
    if (!stats.mostOverpopulatedBracket.empty())
    {
        TC_LOG_INFO("playerbot", "BotLevelDistribution: Most overpopulated bracket: {}", stats.mostOverpopulatedBracket);
    }
}

DistributionStats BotLevelDistribution::GetDistributionStats() const
{
    DistributionStats stats{};
    stats.totalBrackets = m_allianceBrackets.size() + m_hordeBrackets.size();

    // Calculate Alliance stats
    for (auto const& bracket : m_allianceBrackets)
    {
        uint32 count = bracket.GetCount();
        stats.allianceBots += count;
        stats.totalBots += count;
    }

    // Calculate Horde stats
    for (auto const& bracket : m_hordeBrackets)
    {
        uint32 count = bracket.GetCount();
        stats.hordeBots += count;
        stats.totalBots += count;
    }

    // Calculate deviation statistics
    ::std::vector<float> deviations;
    float maxDeviation = 0.0f;
    float maxDevBracket = 0.0f;
    ::std::string maxDevName;

    for (auto const& bracket : m_allianceBrackets)
    {
        if (bracket.IsWithinTolerance(stats.allianceBots))
            ++stats.bracketsWithinTolerance;

        float deviation = ::std::abs(bracket.GetDeviation(stats.allianceBots));
        deviations.push_back(deviation);

        if (deviation > maxDeviation)
        {
            maxDeviation = deviation;
            maxDevBracket = bracket.GetDeviation(stats.allianceBots);
            maxDevName = "Alliance L";
            maxDevName += ::std::to_string(bracket.minLevel);
            maxDevName += "-";
            maxDevName += ::std::to_string(bracket.maxLevel);
        }
    }

    for (auto const& bracket : m_hordeBrackets)
    {
        if (bracket.IsWithinTolerance(stats.hordeBots))
            ++stats.bracketsWithinTolerance;

        float deviation = ::std::abs(bracket.GetDeviation(stats.hordeBots));
        deviations.push_back(deviation);

        if (deviation > maxDeviation)
        {
            maxDeviation = deviation;
            maxDevBracket = bracket.GetDeviation(stats.hordeBots);
            maxDevName = "Horde L";
            maxDevName += ::std::to_string(bracket.minLevel);
            maxDevName += "-";
            maxDevName += ::std::to_string(bracket.maxLevel);
        }
    }

    stats.averageDeviation = deviations.empty() ? 0.0f :
        ::std::accumulate(deviations.begin(), deviations.end(), 0.0f) / deviations.size();
    stats.maxDeviation = maxDeviation;

    if (maxDevBracket > 0.0f)
        stats.mostOverpopulatedBracket = maxDevName;
    else
        stats.mostUnderpopulatedBracket = maxDevName;

    return stats;
}

::std::vector<LevelBracket const*> BotLevelDistribution::GetUnderpopulatedBrackets(TeamId faction) const
{
    ::std::vector<LevelBracket const*> result;
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;

    uint32 totalBots = ::std::accumulate(brackets.begin(), brackets.end(), 0u,
        [](uint32 sum, LevelBracket const& bracket) { return sum + bracket.GetCount(); });

    if (totalBots == 0)
        return result;

    for (auto const& bracket : brackets)
    {
        if (!bracket.IsWithinTolerance(totalBots) && bracket.GetDeviation(totalBots) < 0.0f)
            result.push_back(&bracket);
    }

    // Sort by priority (most underpopulated first)
    ::std::sort(result.begin(), result.end(),
        [totalBots](LevelBracket const* a, LevelBracket const* b) {
            return a->GetDeviation(totalBots) < b->GetDeviation(totalBots);
        });

    return result;
}

::std::vector<LevelBracket const*> BotLevelDistribution::GetOverpopulatedBrackets(TeamId faction) const
{
    ::std::vector<LevelBracket const*> result;
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;

    uint32 totalBots = ::std::accumulate(brackets.begin(), brackets.end(), 0u,
        [](uint32 sum, LevelBracket const& bracket) { return sum + bracket.GetCount(); });

    if (totalBots == 0)
        return result;

    for (auto const& bracket : brackets)
    {
        if (!bracket.IsWithinTolerance(totalBots) && bracket.GetDeviation(totalBots) > 0.0f)
            result.push_back(&bracket);
    }

    // Sort by deviation (most overpopulated first)
    ::std::sort(result.begin(), result.end(),
        [totalBots](LevelBracket const* a, LevelBracket const* b) {
            return a->GetDeviation(totalBots) > b->GetDeviation(totalBots);
        });

    return result;
}

bool BotLevelDistribution::IsDistributionBalanced(TeamId faction) const
{
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;

    uint32 totalBots = ::std::accumulate(brackets.begin(), brackets.end(), 0u,
        [](uint32 sum, LevelBracket const& bracket) { return sum + bracket.GetCount(); });

    if (totalBots == 0)
        return true;  // Empty is considered balanced
    for (auto const& bracket : brackets)
    {
        if (!bracket.IsWithinTolerance(totalBots))
            return false;
    }

    return true;
}

void BotLevelDistribution::PrintDistributionReport() const
{
    TC_LOG_INFO("playerbot", "====================================");
    TC_LOG_INFO("playerbot", "Bot Level Distribution Configuration");
    TC_LOG_INFO("playerbot", "====================================");
    TC_LOG_INFO("playerbot", "Enabled: {}", m_enabled ? "YES" : "NO");
    TC_LOG_INFO("playerbot", "Brackets: {}", m_numBrackets);
    TC_LOG_INFO("playerbot", "Dynamic Distribution: {}", m_dynamicDistribution ? "YES" : "NO");
    TC_LOG_INFO("playerbot", "Tolerance: ±15%");
    TC_LOG_INFO("playerbot", "");

    // Alliance brackets
    TC_LOG_INFO("playerbot", "Alliance Brackets:");
    for (auto const& bracket : m_allianceBrackets)
    {
        TC_LOG_INFO("playerbot", "  L{}-{}: {:.1f}% {} {}",
            bracket.minLevel, bracket.maxLevel, bracket.targetPercentage,
            bracket.IsNaturalLeveling() ? "[NATURAL]" : "",
            bracket.SupportsDualSpec() ? "[DUAL-SPEC]" : "");
    }

    TC_LOG_INFO("playerbot", "");

    // Horde brackets
    TC_LOG_INFO("playerbot", "Horde Brackets:");
    for (auto const& bracket : m_hordeBrackets)
    {
        TC_LOG_INFO("playerbot", "  L{}-{}: {:.1f}% {} {}",
            bracket.minLevel, bracket.maxLevel, bracket.targetPercentage,
            bracket.IsNaturalLeveling() ? "[NATURAL]" : "",
            bracket.SupportsDualSpec() ? "[DUAL-SPEC]" : "");
    }

    TC_LOG_INFO("playerbot", "====================================");
}

::std::string BotLevelDistribution::GetDistributionSummary() const
{
    DistributionStats stats = GetDistributionStats();

    ::std::ostringstream oss;
    oss << "Total Bots: " << stats.totalBots
        << " | Alliance: " << stats.allianceBots
        << " | Horde: " << stats.hordeBots
        << " | Balanced: " << stats.bracketsWithinTolerance << "/" << stats.totalBrackets
        << " | Avg Deviation: " << ::std::fixed << ::std::setprecision(1) << (stats.averageDeviation * 100.0f) << "%";

    return oss.str();
}

} // namespace Playerbot
