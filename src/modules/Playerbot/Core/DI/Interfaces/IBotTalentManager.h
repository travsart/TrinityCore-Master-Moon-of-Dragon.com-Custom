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
#include <vector>

class Player;

namespace Playerbot
{

struct TalentLoadout;
struct SpecChoice;

/**
 * @brief Talent Manager Statistics
 */
struct TalentStats
{
    uint32 totalLoadouts{0};
    uint32 loadoutsPerClass[MAX_CLASSES]{};
    uint32 loadoutsWithHeroTalents{0};
    uint32 averageTalentsPerLoadout{0};
    uint32 specsApplied{0};
    uint32 loadoutsApplied{0};
    uint32 dualSpecsSetup{0};
};

/**
 * @brief Interface for Bot Talent Manager
 *
 * Automated talent and specialization system for bot world population.
 * Handles spec selection, talent loadout application, dual-spec, and hero talents.
 *
 * **Responsibilities:**
 * - Specialization selection with intelligent distribution
 * - Talent loadout management (database-driven)
 * - Dual-spec support (unlocks at level 10)
 * - Hero talent support (levels 71-80)
 * - TrinityCore API integration
 * - Thread-safe loadout cache access
 */
class TC_GAME_API IBotTalentManager
{
public:
    virtual ~IBotTalentManager() = default;

    // ====================================================================
    // INITIALIZATION
    // ====================================================================

    /**
     * Load talent loadouts from database
     * MUST be called before any talent operations
     */
    virtual bool LoadLoadouts() = 0;

    /**
     * Reload loadouts (for hot-reload during development)
     */
    virtual void ReloadLoadouts() = 0;

    /**
     * Check if loadouts are ready
     */
    virtual bool IsReady() const = 0;

    // ====================================================================
    // SPECIALIZATION SELECTION (Thread-safe)
    // ====================================================================

    /**
     * Select primary specialization for bot
     * Thread-safe, can be called from worker threads
     *
     * @param cls       Player class
     * @param faction   TEAM_ALLIANCE or TEAM_HORDE
     * @param level     Bot level
     * @return          SpecChoice with selected spec
     */
    virtual SpecChoice SelectSpecialization(uint8 cls, TeamId faction, uint32 level) = 0;

    /**
     * Select secondary specialization for dual-spec
     * Ensures different from primary spec
     *
     * @param cls       Player class
     * @param faction   TEAM_ALLIANCE or TEAM_HORDE
     * @param level     Bot level
     * @param primarySpec Already selected primary spec
     * @return          SpecChoice for secondary spec
     */
    virtual SpecChoice SelectSecondarySpecialization(uint8 cls, TeamId faction, uint32 level, uint8 primarySpec) = 0;

    /**
     * Get all available specs for a class
     */
    virtual ::std::vector<uint8> GetAvailableSpecs(uint8 cls) const = 0;

    // ====================================================================
    // TALENT LOADOUT QUERIES (Thread-safe)
    // ====================================================================

    /**
     * Get talent loadout for spec and level
     * Thread-safe, returns cached loadout
     *
     * @param cls       Player class
     * @param specId    Specialization ID
     * @param level     Bot level
     * @return          Pointer to loadout, or nullptr if not found
     */
    virtual TalentLoadout const* GetTalentLoadout(uint8 cls, uint8 specId, uint32 level) const = 0;

    /**
     * Get all loadouts for a class/spec combination
     */
    virtual ::std::vector<TalentLoadout const*> GetAllLoadouts(uint8 cls, uint8 specId) const = 0;

    // ====================================================================
    // TALENT APPLICATION (MAIN THREAD ONLY)
    // ====================================================================

    /**
     * Apply specialization to bot
     * MUST be called from main thread (Player API)
     *
     * @param bot       Bot player object
     * @param specId    Specialization to apply
     * @return          True if successful
     */
    virtual bool ApplySpecialization(Player* bot, uint8 specId) = 0;

    /**
     * Apply talent loadout to bot
     * MUST be called from main thread (Player API)
     *
     * @param bot       Bot player object
     * @param specId    Specialization ID
     * @param level     Bot level
     * @return          True if successful
     */
    virtual bool ApplyTalentLoadout(Player* bot, uint8 specId, uint32 level) = 0;

    /**
     * Activate specialization (switch active spec)
     * Used for dual-spec setup
     *
     * @param bot       Bot player object
     * @param specIndex Spec index (0 = primary, 1 = secondary)
     * @return          True if successful
     */
    virtual bool ActivateSpecialization(Player* bot, uint8 specIndex) = 0;

    /**
     * Complete workflow: Apply spec + talents in one call
     * MUST be called from main thread
     *
     * @param bot       Bot player object
     * @param specId    Specialization ID
     * @param level     Bot level
     * @return          True if successful
     */
    virtual bool SetupBotTalents(Player* bot, uint8 specId, uint32 level) = 0;

    // ====================================================================
    // DUAL-SPEC SUPPORT
    // ====================================================================

    /**
     * Check if level supports dual-spec
     * WoW 11.2: Dual-spec unlocks at level 10
     */
    virtual bool SupportsDualSpec(uint32 level) const = 0;

    /**
     * Enable dual-spec for bot
     * MUST be called from main thread
     *
     * @param bot       Bot player object
     * @return          True if successful
     */
    virtual bool EnableDualSpec(Player* bot) = 0;

    /**
     * Setup dual-spec with both talent loadouts
     *
     * @param bot       Bot player object
     * @param spec1     Primary specialization
     * @param spec2     Secondary specialization
     * @param level     Bot level
     * @return          True if successful
     */
    virtual bool SetupDualSpec(Player* bot, uint8 spec1, uint8 spec2, uint32 level) = 0;

    // ====================================================================
    // HERO TALENTS
    // ====================================================================

    /**
     * Check if level supports hero talents
     * WoW 11.2: Hero talents unlock at level 71
     */
    virtual bool SupportsHeroTalents(uint32 level) const = 0;

    /**
     * Apply hero talents for spec
     *
     * @param bot       Bot player object
     * @param specId    Specialization ID
     * @param level     Bot level (71-80)
     * @return          True if successful
     */
    virtual bool ApplyHeroTalents(Player* bot, uint8 specId, uint32 level) = 0;

    // ====================================================================
    // STATISTICS & DEBUGGING
    // ====================================================================

    /**
     * Get statistics
     */
    virtual TalentStats GetStats() const = 0;

    /**
     * Print loadout report to console
     */
    virtual void PrintLoadoutReport() const = 0;

    /**
     * Get formatted loadout summary
     */
    virtual ::std::string GetLoadoutSummary() const = 0;
};

} // namespace Playerbot
