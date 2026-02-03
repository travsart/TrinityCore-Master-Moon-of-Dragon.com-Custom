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
#include <vector>
#include <utility>

namespace Playerbot
{

// Forward declarations
struct RaceClassCombination;

/**
 * @brief Interface for Bot Character Distribution
 *
 * Manages realistic race/class distribution based on WoW 12.0 statistics with:
 * - Database-driven race/class distribution
 * - Gender distribution with race-specific preferences
 * - Class popularity tracking
 * - Cumulative distribution for efficient random selection
 *
 * Thread Safety: Load methods should be called during initialization only
 */
class TC_GAME_API IBotCharacterDistribution
{
public:
    virtual ~IBotCharacterDistribution() = default;

    // Initialization
    virtual bool LoadFromDatabase() = 0;
    virtual void ReloadDistributions() = 0;

    // Random selection
    virtual ::std::pair<uint8, uint8> GetRandomRaceClassByDistribution() = 0;
    virtual uint8 GetRandomGenderForRace(uint8 race) = 0;
    virtual uint8 GetRandomGenderForRaceClass(uint8 race, uint8 classId) = 0;

    // Statistics queries
    virtual float GetRaceClassPercentage(uint8 race, uint8 classId) const = 0;
    virtual float GetClassPopularity(uint8 classId) const = 0;
    virtual uint8 GetMalePercentageForRace(uint8 race) const = 0;

    // Top combinations
    virtual ::std::vector<RaceClassCombination> GetTopCombinations(uint32 limit = 25) const = 0;
    virtual ::std::vector<RaceClassCombination> GetPopularCombinations() const = 0;

    // Status
    virtual uint32 GetTotalCombinations() const = 0;
    virtual uint32 GetPopularCombinationsCount() const = 0;
    virtual bool IsLoaded() const = 0;
};

} // namespace Playerbot
