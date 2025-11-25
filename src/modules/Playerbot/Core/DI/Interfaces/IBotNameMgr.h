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
#include <string>

/**
 * @brief Interface for Bot Name Management
 *
 * Abstracts bot name allocation and tracking to enable dependency injection and testing.
 *
 * **Responsibilities:**
 * - Allocate unique names for bot characters
 * - Track name usage by character GUID
 * - Release names when characters are deleted
 * - Validate name availability
 * - Provide name statistics
 *
 * **Testability:**
 * - Can be mocked for testing without database
 * - Enables testing name allocation logic in isolation
 *
 * Example:
 * @code
 * auto nameMgr = Services::Container().Resolve<IBotNameMgr>();
 * std::string name = nameMgr->AllocateName(GENDER_MALE, characterGuid);
 * if (!name.empty())
 {
 *     // Name allocated successfully
 * }
 * @endcode
 */
class TC_GAME_API IBotNameMgr
{
public:
    virtual ~IBotNameMgr() = default;

    /**
     * @brief Initialize name manager
     * @return true if initialization successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown name manager
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Allocate a name for a character
     * @param gender Character gender (0 = male, 1 = female)
     * @param characterGuid Character GUID
     * @return Allocated name, or empty string if no name available
     */
    virtual std::string AllocateName(uint8 gender, uint32 characterGuid) = 0;

    /**
     * @brief Release a name by character GUID
     * @param characterGuid Character GUID
     */
    virtual void ReleaseName(uint32 characterGuid) = 0;

    /**
     * @brief Release a name by name string
     * @param name Name to release
     */
    virtual void ReleaseName(std::string const& name) = 0;

    /**
     * @brief Check if name is available
     * @param name Name to check
     * @return true if name is available, false otherwise
     */
    virtual bool IsNameAvailable(std::string const& name) const = 0;

    /**
     * @brief Get character name by GUID
     * @param characterGuid Character GUID
     * @return Character name, or empty string if not found
     */
    virtual std::string GetCharacterName(uint32 characterGuid) const = 0;

    /**
     * @brief Get available name count for gender
     * @param gender Gender (0 = male, 1 = female)
     * @return Number of available names for this gender
     */
    virtual uint32 GetAvailableNameCount(uint8 gender) const = 0;

    /**
     * @brief Get total name count
     * @return Total number of names in database
     */
    virtual uint32 GetTotalNameCount() const = 0;

    /**
     * @brief Get used name count
     * @return Number of names currently in use
     */
    virtual uint32 GetUsedNameCount() const = 0;

    /**
     * @brief Reload names from database
     */
    virtual void ReloadNames() = 0;
};
