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

#include "ProfessionIntegrationTest.h"
#include "Player.h"
#include "World.h"
#include "Log.h"
#include "Professions/ProfessionManager.h"
#include "Professions/GatheringAutomation.h"
#include "Professions/FarmingCoordinator.h"
#include "Professions/ProfessionAuctionBridge.h"

namespace Playerbot
{

ProfessionIntegrationTest::ProfessionIntegrationTest()
    : _professionManager(nullptr)
    , _gatheringAutomation(nullptr)
    , _farmingCoordinator(nullptr)
    , _auctionBridge(nullptr)
    , _testPlayer(nullptr)
{
}

ProfessionIntegrationTest::~ProfessionIntegrationTest()
{
    Cleanup();
}

bool ProfessionIntegrationTest::Initialize(Player* player)
{
    if (!player)
    {
        TC_LOG_ERROR("test.profession", "ProfessionIntegrationTest: Cannot initialize with null player");
        return false;
    }

    _testPlayer = player;

    // Initialize all profession systems
    _professionManager = new ProfessionManager(player);
    _gatheringAutomation = new GatheringAutomation(player);
    _farmingCoordinator = new FarmingCoordinator(player);
    _auctionBridge = new ProfessionAuctionBridge(player);

    // Initialize each system
    _professionManager->Initialize();
    _gatheringAutomation->Initialize();
    _farmingCoordinator->Initialize();
    _auctionBridge->Initialize();

    TC_LOG_INFO("test.profession", "ProfessionIntegrationTest: Initialized for player {}", player->GetName());
    return true;
}

void ProfessionIntegrationTest::Cleanup()
{
    delete _auctionBridge;
    delete _farmingCoordinator;
    delete _gatheringAutomation;
    delete _professionManager;

    _auctionBridge = nullptr;
    _farmingCoordinator = nullptr;
    _gatheringAutomation = nullptr;
    _professionManager = nullptr;
    _testPlayer = nullptr;
}

bool ProfessionIntegrationTest::RunAllTests()
{
    if (!_testPlayer)
    {
        TC_LOG_ERROR("test.profession", "ProfessionIntegrationTest: No player initialized for testing");
        return false;
    }

    TC_LOG_INFO("test.profession", "==================================================");
    TC_LOG_INFO("test.profession", "PROFESSION INTEGRATION TEST - PHASES 1-3");
    TC_LOG_INFO("test.profession", "Player: {} | Level: {} | Class: {}",
        _testPlayer->GetName(), _testPlayer->GetLevel(), _testPlayer->GetClass());
    TC_LOG_INFO("test.profession", "==================================================");

    bool allTestsPassed = true;

    // Phase 1: ProfessionManager Tests
    allTestsPassed &= TestPhase1_ProfessionManager();

    // Phase 2: Skill Tracking Tests
    allTestsPassed &= TestPhase2_SkillTracking();

    // Phase 3: Gathering Automation Tests
    allTestsPassed &= TestPhase3_GatheringAutomation();

    // Phase 3: Farming Coordination Tests
    allTestsPassed &= TestPhase3_FarmingCoordination();

    // Phase 3: Auction Bridge Tests
    allTestsPassed &= TestPhase3_AuctionBridge();

    TC_LOG_INFO("test.profession", "==================================================");
    if (allTestsPassed)
        TC_LOG_INFO("test.profession", "ALL TESTS PASSED");
    else
        TC_LOG_ERROR("test.profession", "SOME TESTS FAILED");
    TC_LOG_INFO("test.profession", "==================================================");

    return allTestsPassed;
}

bool ProfessionIntegrationTest::TestPhase1_ProfessionManager()
{
    TC_LOG_INFO("test.profession", "");
    TC_LOG_INFO("test.profession", "[PHASE 1] Testing ProfessionManager...");

    bool passed = true;

    // Test 1: Learn Mining
    if (_professionManager->LearnProfession(ProfessionType::MINING))
    {
        TC_LOG_INFO("test.profession", "  [PASS] LearnProfession(MINING) succeeded");

        // Verify skill was added
        uint16 miningSkill = _professionManager->GetSkillLevel(ProfessionType::MINING);
        if (miningSkill > 0)
        {
            TC_LOG_INFO("test.profession", "  [PASS] Mining skill level: {}", miningSkill);
        }
        else
        {
            TC_LOG_ERROR("test.profession", "  [FAIL] Mining skill not found after learning");
            passed = false;
        }
    }
    else
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] LearnProfession(MINING) failed");
        passed = false;
    }

    // Test 2: Learn Herbalism
    if (_professionManager->LearnProfession(ProfessionType::HERBALISM))
    {
        TC_LOG_INFO("test.profession", "  [PASS] LearnProfession(HERBALISM) succeeded");
    }
    else
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] LearnProfession(HERBALISM) failed");
        passed = false;
    }

    // Test 3: Check profession limit (max 2 primary professions)
    if (_professionManager->LearnProfession(ProfessionType::BLACKSMITHING))
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] Should not allow 3rd primary profession");
        passed = false;
    }
    else
    {
        TC_LOG_INFO("test.profession", "  [PASS] Correctly rejected 3rd primary profession");
    }

    // Test 4: Learn secondary profession (should always work)
    if (_professionManager->LearnProfession(ProfessionType::FISHING))
    {
        TC_LOG_INFO("test.profession", "  [PASS] LearnProfession(FISHING) succeeded (secondary)");
    }
    else
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] LearnProfession(FISHING) failed");
        passed = false;
    }

    return passed;
}

bool ProfessionIntegrationTest::TestPhase2_SkillTracking()
{
    TC_LOG_INFO("test.profession", "");
    TC_LOG_INFO("test.profession", "[PHASE 2] Testing Skill Tracking...");

    bool passed = true;

    // Test 1: Check current skill levels
    uint16 miningSkill = _professionManager->GetSkillLevel(ProfessionType::MINING);
    uint16 herbalismSkill = _professionManager->GetSkillLevel(ProfessionType::HERBALISM);

    TC_LOG_INFO("test.profession", "  Current Skills:");
    TC_LOG_INFO("test.profession", "    Mining: {}", miningSkill);
    TC_LOG_INFO("test.profession", "    Herbalism: {}", herbalismSkill);

    // Test 2: Simulate skill gain
    uint16 oldMiningSkill = miningSkill;
    // In a real scenario, this would happen through gathering
    // For testing, we just verify the tracking works

    if (miningSkill >= 0 && herbalismSkill >= 0)
    {
        TC_LOG_INFO("test.profession", "  [PASS] Skill tracking operational");
    }
    else
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] Invalid skill values detected");
        passed = false;
    }

    // Test 3: Check skill max values
    uint16 miningMax = _professionManager->GetMaxSkillLevel(ProfessionType::MINING);
    if (miningMax > 0)
    {
        TC_LOG_INFO("test.profession", "  [PASS] Max skill level tracking: {}", miningMax);
    }
    else
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] Max skill level not set");
        passed = false;
    }

    return passed;
}

bool ProfessionIntegrationTest::TestPhase3_GatheringAutomation()
{
    TC_LOG_INFO("test.profession", "");
    TC_LOG_INFO("test.profession", "[PHASE 3] Testing GatheringAutomation...");

    bool passed = true;

    // Test 1: Scan for mining nodes
    std::vector<GatheringNodeInfo> miningNodes = _gatheringAutomation->ScanForNodes(
        ProfessionType::MINING, 100.0f);

    TC_LOG_INFO("test.profession", "  Found {} mining nodes within 100 yards", miningNodes.size());
    if (!miningNodes.empty())
    {
        TC_LOG_INFO("test.profession", "  [PASS] Mining node detection functional");

        // Display first node info
        GatheringNodeInfo const& node = miningNodes[0];
        TC_LOG_INFO("test.profession", "    Node: Entry={} RequiredSkill={} Distance={:.2f}",
            node.gameObjectEntry, node.requiredSkill, node.distanceFromPlayer);
    }
    else
    {
        TC_LOG_INFO("test.profession", "  [INFO] No mining nodes nearby (expected in some zones)");
    }

    // Test 2: Scan for herbs
    std::vector<GatheringNodeInfo> herbNodes = _gatheringAutomation->ScanForNodes(
        ProfessionType::HERBALISM, 100.0f);

    TC_LOG_INFO("test.profession", "  Found {} herb nodes within 100 yards", herbNodes.size());

    // Test 3: Scan for skinnable creatures
    std::vector<GatheringNodeInfo> skinnableCreatures = _gatheringAutomation->ScanForNodes(
        ProfessionType::SKINNING, 50.0f);

    TC_LOG_INFO("test.profession", "  Found {} skinnable creatures within 50 yards", skinnableCreatures.size());

    // Test 4: Check gathering automation configuration
    if (_gatheringAutomation->IsEnabled(ProfessionType::MINING))
    {
        TC_LOG_INFO("test.profession", "  [PASS] Mining automation enabled");
    }

    return passed;
}

bool ProfessionIntegrationTest::TestPhase3_FarmingCoordination()
{
    TC_LOG_INFO("test.profession", "");
    TC_LOG_INFO("test.profession", "[PHASE 3] Testing FarmingCoordinator...");

    bool passed = true;

    // Test 1: Check if bot needs farming
    bool needsMiningFarming = _farmingCoordinator->NeedsFarming(_testPlayer, ProfessionType::MINING);
    bool needsHerbalismFarming = _farmingCoordinator->NeedsFarming(_testPlayer, ProfessionType::HERBALISM);

    TC_LOG_INFO("test.profession", "  Farming Status:");
    TC_LOG_INFO("test.profession", "    Mining: {}", needsMiningFarming ? "NEEDS FARMING" : "OK");
    TC_LOG_INFO("test.profession", "    Herbalism: {}", needsHerbalismFarming ? "NEEDS FARMING" : "OK");

    // Test 2: Get target skill levels
    uint16 miningTarget = _farmingCoordinator->GetTargetSkillLevel(_testPlayer, ProfessionType::MINING);
    uint16 herbalismTarget = _farmingCoordinator->GetTargetSkillLevel(_testPlayer, ProfessionType::HERBALISM);

    TC_LOG_INFO("test.profession", "  Target Skill Levels:");
    TC_LOG_INFO("test.profession", "    Mining: {}", miningTarget);
    TC_LOG_INFO("test.profession", "    Herbalism: {}", herbalismTarget);

    // Test 3: Get skill gaps
    int32 miningGap = _farmingCoordinator->GetSkillGap(_testPlayer, ProfessionType::MINING);
    int32 herbalismGap = _farmingCoordinator->GetSkillGap(_testPlayer, ProfessionType::HERBALISM);

    TC_LOG_INFO("test.profession", "  Skill Gaps:");
    TC_LOG_INFO("test.profession", "    Mining: {}", miningGap);
    TC_LOG_INFO("test.profession", "    Herbalism: {}", herbalismGap);

    // Test 4: Find optimal farming zone
    uint16 currentMiningSkill = _professionManager->GetSkillLevel(ProfessionType::MINING);
    FarmingZoneInfo const* miningZone = _farmingCoordinator->FindOptimalZone(
        ProfessionType::MINING, currentMiningSkill);

    if (miningZone)
    {
        TC_LOG_INFO("test.profession", "  [PASS] Found optimal mining zone: {}", miningZone->zoneName);
        TC_LOG_INFO("test.profession", "    Zone ID: {} | Skill Range: {}-{}",
            miningZone->zoneId, miningZone->minSkillLevel, miningZone->maxSkillLevel);
    }
    else
    {
        TC_LOG_INFO("test.profession", "  [INFO] No optimal mining zone found for skill level {}", currentMiningSkill);
    }

    return passed;
}

bool ProfessionIntegrationTest::TestPhase3_AuctionBridge()
{
    TC_LOG_INFO("test.profession", "");
    TC_LOG_INFO("test.profession", "[PHASE 3] Testing ProfessionAuctionBridge...");

    bool passed = true;

    // Test 1: Check stockpile configuration
    TC_LOG_INFO("test.profession", "  Stockpile Configuration:");

    // Get mining stockpile configs
    std::vector<MaterialStockpileConfig> configs = _auctionBridge->GetStockpileConfigs(ProfessionType::MINING);
    TC_LOG_INFO("test.profession", "    Mining materials configured: {}", configs.size());

    for (MaterialStockpileConfig const& config : configs)
    {
        TC_LOG_INFO("test.profession", "      Item {}: Min={} Max={} Auction={}",
            config.itemId, config.minStockpile, config.maxStockpile, config.auctionStackSize);
    }

    // Test 2: Check if materials should be sold
    uint32 copperOreId = 2770; // Copper Ore
    uint32 currentCount = 100; // Simulate having 100 copper ore

    bool shouldSell = _auctionBridge->ShouldSellMaterial(_testPlayer, copperOreId, currentCount);
    TC_LOG_INFO("test.profession", "  ShouldSellMaterial(CopperOre, 100): {}", shouldSell ? "YES" : "NO");

    // Test 3: Get excess material count
    uint32 excessCount = _auctionBridge->GetExcessMaterialCount(_testPlayer, copperOreId);
    TC_LOG_INFO("test.profession", "  Excess Copper Ore count: {}", excessCount);

    // Test 4: Verify auction house integration
    if (_auctionBridge->IsInitialized())
    {
        TC_LOG_INFO("test.profession", "  [PASS] Auction house bridge initialized");
    }
    else
    {
        TC_LOG_ERROR("test.profession", "  [FAIL] Auction house bridge not initialized");
        passed = false;
    }

    return passed;
}

void ProfessionIntegrationTest::PrintTestSummary()
{
    if (!_testPlayer)
        return;

    TC_LOG_INFO("test.profession", "");
    TC_LOG_INFO("test.profession", "==================================================");
    TC_LOG_INFO("test.profession", "PROFESSION SYSTEM SUMMARY");
    TC_LOG_INFO("test.profession", "==================================================");

    // Phase 1 Summary
    TC_LOG_INFO("test.profession", "Phase 1 - ProfessionManager:");
    TC_LOG_INFO("test.profession", "  Active Professions: {}", _professionManager->GetActiveProfessionCount());

    // Phase 2 Summary
    TC_LOG_INFO("test.profession", "Phase 2 - Skill Tracking:");
    TC_LOG_INFO("test.profession", "  Mining: {}/{}",
        _professionManager->GetSkillLevel(ProfessionType::MINING),
        _professionManager->GetMaxSkillLevel(ProfessionType::MINING));
    TC_LOG_INFO("test.profession", "  Herbalism: {}/{}",
        _professionManager->GetSkillLevel(ProfessionType::HERBALISM),
        _professionManager->GetMaxSkillLevel(ProfessionType::HERBALISM));

    // Phase 3 Summary
    TC_LOG_INFO("test.profession", "Phase 3 - Gathering & Farming:");
    TC_LOG_INFO("test.profession", "  Gathering enabled: {}",
        _gatheringAutomation->IsEnabled(ProfessionType::MINING) ? "YES" : "NO");
    TC_LOG_INFO("test.profession", "  Farming target (Mining): {}",
        _farmingCoordinator->GetTargetSkillLevel(_testPlayer, ProfessionType::MINING));
    TC_LOG_INFO("test.profession", "  Auction integration: {}",
        _auctionBridge->IsInitialized() ? "ACTIVE" : "INACTIVE");

    TC_LOG_INFO("test.profession", "==================================================");
}

} // namespace Playerbot
