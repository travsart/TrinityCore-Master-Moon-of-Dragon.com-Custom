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
#include <cstdint>
#include <memory>
#include <string>
#include <functional>

class Player;

namespace Playerbot
{

class BotAI;

/**
 * @brief Interface for Bot AI Factory
 *
 * Abstracts AI creation to enable dependency injection and testing.
 * Factory for creating class-specific and specialized bot AI instances.
 *
 * **Testability:**
 * - Can be mocked for testing without creating real AI instances
 * - Enables testing of AI creation logic in isolation
 */
class TC_GAME_API IBotAIFactory
{
public:
    virtual ~IBotAIFactory() = default;

    /**
     * @brief Create AI for bot based on class
     *
     * @param bot Player to create AI for
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreateAI(Player* bot) = 0;

    /**
     * @brief Create class-specific AI
     *
     * @param bot Player to create AI for
     * @param classId Class ID (CLASS_WARRIOR, CLASS_MAGE, etc.)
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreateClassAI(Player* bot, uint8 classId) = 0;

    /**
     * @brief Create class and spec-specific AI
     *
     * @param bot Player to create AI for
     * @param classId Class ID
     * @param spec Specialization ID
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreateClassAI(Player* bot, uint8 classId, uint8 spec) = 0;

    /**
     * @brief Create specialized AI by type
     *
     * @param bot Player to create AI for
     * @param type AI type string (e.g., "pvp", "raid", "dungeon")
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreateSpecializedAI(Player* bot, ::std::string const& type) = 0;

    /**
     * @brief Create PvP-oriented AI
     *
     * @param bot Player to create AI for
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreatePvPAI(Player* bot) = 0;

    /**
     * @brief Create PvE-oriented AI
     *
     * @param bot Player to create AI for
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreatePvEAI(Player* bot) = 0;

    /**
     * @brief Create raid-oriented AI
     *
     * @param bot Player to create AI for
     * @return Unique pointer to created AI, or nullptr on failure
     */
    virtual ::std::unique_ptr<BotAI> CreateRaidAI(Player* bot) = 0;

    /**
     * @brief Register custom AI creator
     *
     * @param type AI type identifier
     * @param creator Function that creates AI instance
     */
    virtual void RegisterAICreator(::std::string const& type,
                                   ::std::function<::std::unique_ptr<BotAI>(Player*)> creator) = 0;

    /**
     * @brief Initialize default triggers for AI
     *
     * @param ai AI instance to initialize
     */
    virtual void InitializeDefaultTriggers(BotAI* ai) = 0;

    /**
     * @brief Initialize default values for AI
     *
     * @param ai AI instance to initialize
     */
    virtual void InitializeDefaultValues(BotAI* ai) = 0;
};

} // namespace Playerbot
