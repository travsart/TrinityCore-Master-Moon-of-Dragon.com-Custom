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

#pragma once

#include "Define.h"
#include "SharedDefines.h"
#include "Core/DI/Interfaces/IBotLevelDistribution.h"
#include <atomic>
#include <vector>
#include <map>
#include <string>
#include <random>

namespace Playerbot
{

/**
 * Thread-Safe Level Bracket
 *
 * Represents a level range with target distribution percentage.
 * Uses atomic counter for lock-free concurrent access.
 * Memory order relaxed is sufficient due to ±15% tolerance.
 */
struct LevelBracket
{
    uint32 minLevel;
    uint32 maxLevel;
    float targetPercentage;
    TeamId faction;

    // Thread-safe counter (relaxed memory order)
    mutable std::atomic<uint32> currentCount{0};

    // Copy constructor - manually copy atomic value
    LevelBracket(LevelBracket const& other)
        : minLevel(other.minLevel)
        , maxLevel(other.maxLevel)
        , targetPercentage(other.targetPercentage)
        , faction(other.faction)
        , currentCount(other.currentCount.load(std::memory_order_relaxed))
    {
    }

    // Default constructor
    LevelBracket() = default;

    // Copy assignment - manually copy atomic value
    LevelBracket& operator=(LevelBracket const& other)
    {
        if (this != &other)
        {
            minLevel = other.minLevel;
            maxLevel = other.maxLevel;
            targetPercentage = other.targetPercentage;
            faction = other.faction;
            currentCount.store(other.currentCount.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    // Thread-safe operations
    void IncrementCount() const
    {
        currentCount.fetch_add(1, std::memory_order_relaxed);
    }

    void DecrementCount() const
    {
        if (currentCount.load(std::memory_order_relaxed) > 0)
            currentCount.fetch_sub(1, std::memory_order_relaxed);
    }

    uint32 GetCount() const
    {
        return currentCount.load(std::memory_order_relaxed);
    }

    void SetCount(uint32 count) const
    {
        currentCount.store(count, std::memory_order_relaxed);
    }

    // Behavior flags
    bool IsNaturalLeveling() const
    {
        return minLevel <= 4;  // Levels 1-4 level naturally
    }

    bool SupportsDualSpec() const
    {
        return minLevel >= 10;  // WoW 11.2: Dual-spec unlocks at level 10
    }

    bool IsEndgame() const
    {
        return minLevel == 80;
    }

    // Get random level within bracket
    uint32 GetRandomLevel() const
    {
        if (minLevel == maxLevel)
            return minLevel;

        thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<uint32> distribution(minLevel, maxLevel);
        return distribution(generator);
    }

    // Calculate target count based on total bots
    uint32 GetTargetCount(uint32 totalBots) const
    {
        return static_cast<uint32>(totalBots * targetPercentage / 100.0f);
    }

    // Check if bracket is within tolerance (±15%)
    bool IsWithinTolerance(uint32 totalBots) const
    {
        uint32 target = GetTargetCount(totalBots);
        uint32 current = GetCount();

        float lowerBound = target * 0.85f;
        float upperBound = target * 1.15f;

        return current >= lowerBound && current <= upperBound;
    }

    // Get deviation from target (-1.0 = empty, 0.0 = perfect, +1.0 = double target)
    float GetDeviation(uint32 totalBots) const
    {
        uint32 target = GetTargetCount(totalBots);
        if (target == 0)
            return 0.0f;

        uint32 current = GetCount();
        return (static_cast<float>(current) - static_cast<float>(target)) / static_cast<float>(target);
    }

    // Priority for selection (negative deviation = needs more bots)
    float GetSelectionPriority(uint32 totalBots) const
    {
        return -GetDeviation(totalBots);  // Negative deviation = higher priority
    }
};

/**
 * Distribution Statistics
 */
struct DistributionStats
{
    uint32 totalBots;
    uint32 allianceBots;
    uint32 hordeBots;
    uint32 bracketsWithinTolerance;
    uint32 totalBrackets;
    float averageDeviation;
    float maxDeviation;
    std::string mostUnderpopulatedBracket;
    std::string mostOverpopulatedBracket;
};

/**
 * Bot Level Distribution System
 *
 * Purpose: Automated world population with level-appropriate bots
 *
 * Features:
 * - Configurable level brackets (default: every 5 levels)
 * - Thread-safe atomic counters (lock-free reads)
 * - Distribution tolerance checking (±15%)
 * - Weighted bracket selection based on deviation
 * - Separate Alliance/Horde distributions
 * - Natural leveling for levels 1-4 (no instant gear)
 * - Instant level-up + gear for levels 5+
 *
 * Thread Safety:
 * - Config data is immutable after LoadConfig()
 * - Atomic counters for current bot counts
 * - No locks required for bracket selection
 * - Relaxed memory ordering (tolerance allows eventual consistency)
 *
 * Performance:
 * - O(n) bracket selection (n = number of brackets, typically 17)
 * - Lock-free counter updates
 * - Minimal contention
 */
class TC_GAME_API BotLevelDistribution final : public IBotLevelDistribution
{
public:
    static BotLevelDistribution* instance();

    // IBotLevelDistribution interface implementation

    // Initialization
    bool LoadConfig() override;
    void ReloadConfig() override;

    // Bracket selection
    LevelBracket const* SelectBracket(TeamId faction) const override;
    LevelBracket const* SelectBracketWeighted(TeamId faction) const;
    LevelBracket const* GetBracketForLevel(uint32 level, TeamId faction) const override;

    // Distribution analysis
    DistributionStats GetDistributionStats() const;
    std::vector<LevelBracket const*> GetUnderpopulatedBrackets(TeamId faction) const;
    std::vector<LevelBracket const*> GetOverpopulatedBrackets(TeamId faction) const;
    bool IsDistributionBalanced(TeamId faction) const override;

    // Counter updates
    void IncrementBracket(uint32 level, TeamId faction) override;
    void DecrementBracket(uint32 level, TeamId faction) override;
    void RecalculateDistribution() override;

    // Configuration queries
    uint32 GetNumBrackets() const override { return m_numBrackets; }
    float GetTolerancePercent() const { return 15.0f; }  // ±15% tolerance
    bool IsEnabled() const override { return m_enabled; }
    bool IsDynamicDistribution() const override { return m_dynamicDistribution; }

    // Debugging
    void PrintDistributionReport() const override;
    std::string GetDistributionSummary() const;

private:
    BotLevelDistribution() = default;
    ~BotLevelDistribution() = default;
    BotLevelDistribution(BotLevelDistribution const&) = delete;
    BotLevelDistribution& operator=(BotLevelDistribution const&) = delete;

    // Config loading helpers
    void LoadBrackets(TeamId faction, std::string const& prefix);
    bool ValidateConfig() const;
    void NormalizeBracketPercentages();

    // Selection helpers
    LevelBracket const* SelectByPriority(std::vector<LevelBracket> const& brackets) const;
    float CalculateTotalPriority(std::vector<LevelBracket> const& brackets) const;

private:
    // Configuration (immutable after load)
    bool m_enabled = false;
    uint32 m_numBrackets = 17;
    bool m_dynamicDistribution = false;
    float m_realPlayerWeight = 1.0f;
    bool m_syncFactions = false;

    // Bracket storage (immutable after load)
    std::vector<LevelBracket> m_allianceBrackets;
    std::vector<LevelBracket> m_hordeBrackets;

    // Fast lookup map: level -> bracket index
    std::map<uint32, size_t> m_allianceLevelMap;
    std::map<uint32, size_t> m_hordeLevelMap;

    // RNG (thread-local for thread safety)
    mutable std::mt19937 m_generator{std::random_device{}()};

    // Status
    bool m_loaded = false;
};

} // namespace Playerbot

#define sBotLevelDistribution Playerbot::BotLevelDistribution::instance()
