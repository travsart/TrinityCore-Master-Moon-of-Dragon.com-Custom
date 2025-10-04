/*
 * Copyright (C) 2025 TrinityCore PlayerBot
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

#ifndef PLAYERBOT_PROFESSION_INTEGRATION_TEST_H
#define PLAYERBOT_PROFESSION_INTEGRATION_TEST_H

#include "Define.h"

class Player;

namespace Playerbot
{

class ProfessionManager;
class GatheringAutomation;
class FarmingCoordinator;
class ProfessionAuctionBridge;

/**
 * @brief Comprehensive integration test for Profession System Phases 1-3
 *
 * Tests the complete profession system integration:
 * - Phase 1: ProfessionManager (learning professions, skill tracking)
 * - Phase 2: Skill synchronization (level-based targets)
 * - Phase 3: GatheringAutomation, FarmingCoordinator, AuctionBridge
 *
 * Usage:
 * @code
 * ProfessionIntegrationTest test;
 * test.Initialize(botPlayer);
 * bool success = test.RunAllTests();
 * test.PrintTestSummary();
 * @endcode
 */
class TC_GAME_API ProfessionIntegrationTest
{
public:
    ProfessionIntegrationTest();
    ~ProfessionIntegrationTest();

    /**
     * @brief Initialize test with a player
     * @param player Player to test profession systems with
     * @return true if initialization succeeded
     */
    bool Initialize(Player* player);

    /**
     * @brief Run all integration tests
     * @return true if all tests passed
     */
    bool RunAllTests();

    /**
     * @brief Print comprehensive test summary
     */
    void PrintTestSummary();

    /**
     * @brief Clean up test resources
     */
    void Cleanup();

private:
    // Phase-specific test methods
    bool TestPhase1_ProfessionManager();
    bool TestPhase2_SkillTracking();
    bool TestPhase3_GatheringAutomation();
    bool TestPhase3_FarmingCoordination();
    bool TestPhase3_AuctionBridge();

    // Test data
    ProfessionManager* _professionManager;
    GatheringAutomation* _gatheringAutomation;
    FarmingCoordinator* _farmingCoordinator;
    ProfessionAuctionBridge* _auctionBridge;
    Player* _testPlayer;
};

} // namespace Playerbot

#endif // PLAYERBOT_PROFESSION_INTEGRATION_TEST_H
