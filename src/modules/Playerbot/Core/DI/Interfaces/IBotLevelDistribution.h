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

namespace Playerbot
{

// Forward declarations
struct LevelBracket;

/**
 * @brief Interface for Bot Level Distribution
 *
 * Manages realistic level distribution across bot population with:
 * - Configurable level brackets with target percentages
 * - Thread-safe bracket tracking
 * - Dynamic rebalancing
 * - Faction-specific distributions
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API IBotLevelDistribution
{
public:
    virtual ~IBotLevelDistribution() = default;

    // Configuration
    virtual bool LoadConfig() = 0;
    virtual void ReloadConfig() = 0;

    // Bracket selection
    virtual LevelBracket const* SelectBracket(TeamId faction) const = 0;
    virtual LevelBracket const* GetBracketForLevel(uint32 level, TeamId faction) const = 0;

    // Balance checking
    virtual bool IsDistributionBalanced(TeamId faction) const = 0;

    // Bracket tracking
    virtual void IncrementBracket(uint32 level, TeamId faction) = 0;
    virtual void DecrementBracket(uint32 level, TeamId faction) = 0;
    virtual void RecalculateDistribution() = 0;

    // Status queries
    virtual uint32 GetNumBrackets() const = 0;
    virtual bool IsEnabled() const = 0;
    virtual bool IsDynamicDistribution() const = 0;

    // Reporting
    virtual void PrintDistributionReport() const = 0;
    virtual ::std::string GetDistributionSummary() const = 0;
};

} // namespace Playerbot
