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

// Forward declarations
class Player;
class Creature;

namespace Playerbot
{
    class DungeonScript;
    enum class MechanicType : uint8;

/**
 * @brief Interface for Dungeon Script Management
 *
 * Abstracts dungeon script registration and lookup to enable dependency injection and testing.
 *
 * **Responsibilities:**
 * - Register dungeon scripts for maps and bosses
 * - Lookup scripts by map ID or boss entry
 * - Execute boss mechanics with fallback to generic
 * - Provide script statistics
 *
 * **Testability:**
 * - Can be mocked for testing without real dungeon scripts
 * - Enables testing dungeon behavior logic in isolation
 *
 * Example:
 * @code
 * auto scriptMgr = Services::Container().Resolve<IDungeonScriptMgr>();
 * DungeonScript* script = scriptMgr->GetScriptForMap(mapId);
 * if (script) {
 *     script->HandleBossMechanic(player, boss);
 * }
 * @endcode
 */
class TC_GAME_API IDungeonScriptMgr
{
public:
    /**
     * @brief Script statistics structure
     */
    struct ScriptStats
    {
        uint32 scriptsRegistered;
        uint32 bossMappings;
        uint32 scriptHits;
        uint32 scriptMisses;
        uint32 mechanicExecutions;

        ScriptStats() : scriptsRegistered(0), bossMappings(0),
            scriptHits(0), scriptMisses(0), mechanicExecutions(0) {}
    };

    virtual ~IDungeonScriptMgr() = default;

    /**
     * @brief Initialize script manager
     */
    virtual void Initialize() = 0;

    /**
     * @brief Load all dungeon scripts
     */
    virtual void LoadScripts() = 0;

    /**
     * @brief Register a dungeon script
     * @param script Script to register (manager takes ownership)
     */
    virtual void RegisterScript(DungeonScript* script) = 0;

    /**
     * @brief Register boss entry to script mapping
     * @param bossEntry Creature entry ID for boss
     * @param script Script handling this boss
     */
    virtual void RegisterBossScript(uint32 bossEntry, DungeonScript* script) = 0;

    /**
     * @brief Get script for map ID
     * @param mapId Map ID to look up
     * @return Script pointer or nullptr if not found
     */
    virtual DungeonScript* GetScriptForMap(uint32 mapId) const = 0;

    /**
     * @brief Get script for boss entry
     * @param bossEntry Creature entry ID
     * @return Script pointer or nullptr if not found
     */
    virtual DungeonScript* GetScriptForBoss(uint32 bossEntry) const = 0;

    /**
     * @brief Check if script exists for map
     * @param mapId Map ID to check
     * @return true if script exists, false otherwise
     */
    virtual bool HasScriptForMap(uint32 mapId) const = 0;

    /**
     * @brief Check if script exists for boss
     * @param bossEntry Boss entry to check
     * @return true if script exists, false otherwise
     */
    virtual bool HasScriptForBoss(uint32 bossEntry) const = 0;

    /**
     * @brief Execute boss mechanic with automatic fallback
     * @param player Player executing mechanic
     * @param boss Boss being fought
     * @param mechanic Mechanic type to execute
     */
    virtual void ExecuteBossMechanic(::Player* player, ::Creature* boss, MechanicType mechanic) = 0;

    /**
     * @brief Get number of registered scripts
     * @return Script count
     */
    virtual uint32 GetScriptCount() const = 0;

    /**
     * @brief Get number of registered boss mappings
     * @return Boss mapping count
     */
    virtual uint32 GetBossMappingCount() const = 0;

    /**
     * @brief Get statistics on script usage
     * @return Script statistics
     */
    virtual ScriptStats GetStats() const = 0;

    /**
     * @brief List all registered scripts (for debugging)
     */
    virtual void ListAllScripts() const = 0;

    /**
     * @brief Get script info by name
     * @param name Script name
     * @return Script pointer or nullptr if not found
     */
    virtual DungeonScript* GetScriptByName(::std::string const& name) const = 0;
};

} // namespace Playerbot
