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

void BotLevelDistribution::LoadBrackets(TeamId faction, std::string const& prefix)
{
    auto& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;
    auto& levelMap = (faction == TEAM_ALLIANCE) ? m_allianceLevelMap : m_hordeLevelMap;

    for (uint32 i = 1; i <= m_numBrackets; ++i)
    {
        std::string rangeKey = prefix + ".Range" + std::to_string(i);

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
    float allianceSum = std::accumulate(m_allianceBrackets.begin(), m_allianceBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    float hordeSum = std::accumulate(m_hordeBrackets.begin(), m_hordeBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    if (std::abs(allianceSum - 100.0f) > 1.0f)
    {
        TC_LOG_WARN("playerbot", "BotLevelDistribution: Alliance percentages sum to {:.2f}% (should be 100%)",
            allianceSum);
    }

    if (std::abs(hordeSum - 100.0f) > 1.0f)
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
    float allianceSum = std::accumulate(m_allianceBrackets.begin(), m_allianceBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    if (allianceSum > 0.0f && std::abs(allianceSum - 100.0f) > 0.01f)
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
    float hordeSum = std::accumulate(m_hordeBrackets.begin(), m_hordeBrackets.end(), 0.0f,
        [](float sum, LevelBracket const& bracket) { return sum + bracket.targetPercentage; });

    if (hordeSum > 0.0f && std::abs(hordeSum - 100.0f) > 0.01f)
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
    uint32 totalBots = std::accumulate(brackets.begin(), brackets.end(), 0u,
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
    std::vector<float> priorities;
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
    float totalPriority = std::accumulate(priorities.begin(), priorities.end(), 0.0f);
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
    // This would scan the database and update counters
    // Implementation depends on database schema
    TC_LOG_DEBUG("playerbot", "BotLevelDistribution: RecalculateDistribution() called (not yet implemented)");
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
    std::vector<float> deviations;
    float maxDeviation = 0.0f;
    float maxDevBracket = 0.0f;
    std::string maxDevName;

    for (auto const& bracket : m_allianceBrackets)
    {
        if (bracket.IsWithinTolerance(stats.allianceBots))
            ++stats.bracketsWithinTolerance;

        float deviation = std::abs(bracket.GetDeviation(stats.allianceBots));
        deviations.push_back(deviation);

        if (deviation > maxDeviation)
        {
            maxDeviation = deviation;
            maxDevBracket = bracket.GetDeviation(stats.allianceBots);
            maxDevName = "Alliance L" + std::to_string(bracket.minLevel) + "-" + std::to_string(bracket.maxLevel);
        }
    }

    for (auto const& bracket : m_hordeBrackets)
    {
        if (bracket.IsWithinTolerance(stats.hordeBots))
            ++stats.bracketsWithinTolerance;

        float deviation = std::abs(bracket.GetDeviation(stats.hordeBots));
        deviations.push_back(deviation);

        if (deviation > maxDeviation)
        {
            maxDeviation = deviation;
            maxDevBracket = bracket.GetDeviation(stats.hordeBots);
            maxDevName = "Horde L" + std::to_string(bracket.minLevel) + "-" + std::to_string(bracket.maxLevel);
        }
    }

    stats.averageDeviation = deviations.empty() ? 0.0f :
        std::accumulate(deviations.begin(), deviations.end(), 0.0f) / deviations.size();
    stats.maxDeviation = maxDeviation;

    if (maxDevBracket > 0.0f)
        stats.mostOverpopulatedBracket = maxDevName;
    else
        stats.mostUnderpopulatedBracket = maxDevName;

    return stats;
}

std::vector<LevelBracket const*> BotLevelDistribution::GetUnderpopulatedBrackets(TeamId faction) const
{
    std::vector<LevelBracket const*> result;
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;

    uint32 totalBots = std::accumulate(brackets.begin(), brackets.end(), 0u,
        [](uint32 sum, LevelBracket const& bracket) { return sum + bracket.GetCount(); });

    if (totalBots == 0)
        return result;

    for (auto const& bracket : brackets)
    {
        if (!bracket.IsWithinTolerance(totalBots) && bracket.GetDeviation(totalBots) < 0.0f)
            result.push_back(&bracket);
    }

    // Sort by priority (most underpopulated first)
    std::sort(result.begin(), result.end(),
        [totalBots](LevelBracket const* a, LevelBracket const* b) {
            return a->GetDeviation(totalBots) < b->GetDeviation(totalBots);
        });

    return result;
}

std::vector<LevelBracket const*> BotLevelDistribution::GetOverpopulatedBrackets(TeamId faction) const
{
    std::vector<LevelBracket const*> result;
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;

    uint32 totalBots = std::accumulate(brackets.begin(), brackets.end(), 0u,
        [](uint32 sum, LevelBracket const& bracket) { return sum + bracket.GetCount(); });

    if (totalBots == 0)
        return result;

    for (auto const& bracket : brackets)
    {
        if (!bracket.IsWithinTolerance(totalBots) && bracket.GetDeviation(totalBots) > 0.0f)
            result.push_back(&bracket);
    }

    // Sort by deviation (most overpopulated first)
    std::sort(result.begin(), result.end(),
        [totalBots](LevelBracket const* a, LevelBracket const* b) {
            return a->GetDeviation(totalBots) > b->GetDeviation(totalBots);
        });

    return result;
}

bool BotLevelDistribution::IsDistributionBalanced(TeamId faction) const
{
    auto const& brackets = (faction == TEAM_ALLIANCE) ? m_allianceBrackets : m_hordeBrackets;

    uint32 totalBots = std::accumulate(brackets.begin(), brackets.end(), 0u,
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

std::string BotLevelDistribution::GetDistributionSummary() const
{
    DistributionStats stats = GetDistributionStats();

    std::ostringstream oss;
    oss << "Total Bots: " << stats.totalBots
        << " | Alliance: " << stats.allianceBots
        << " | Horde: " << stats.hordeBots
        << " | Balanced: " << stats.bracketsWithinTolerance << "/" << stats.totalBrackets
        << " | Avg Deviation: " << std::fixed << std::setprecision(1) << (stats.averageDeviation * 100.0f) << "%";

    return oss.str();
}

} // namespace Playerbot
