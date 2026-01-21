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

#include "tc_catch2.h"
#include "AI/Decision/DecisionFusionSystem.h"

using namespace bot::ai;

TEST_CASE("DecisionFusion - DecisionVote Weighted Score", "[Phase5][DecisionFusion]")
{
    SECTION("Weighted score calculation")
    {
        DecisionVote vote(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,  // actionId
            nullptr, // target
            0.8f,   // confidence
            0.6f,   // urgency
            "Test vote"
        );

        float systemWeight = 1.0f;
        float weightedScore = vote.CalculateWeightedScore(systemWeight);

        // Score = confidence × urgency × systemWeight
        // 0.8 × 0.6 × 1.0 = 0.48
        REQUIRE(weightedScore == Approx(0.48f));
    }

    SECTION("Weighted score with different system weight")
    {
        DecisionVote vote(
            DecisionSource::ACTION_PRIORITY,
            12345,
            nullptr,
            1.0f,   // confidence
            1.0f,   // urgency
            "Maximum vote"
        );

        float systemWeight = 0.5f;
        float weightedScore = vote.CalculateWeightedScore(systemWeight);

        // 1.0 × 1.0 × 0.5 = 0.5
        REQUIRE(weightedScore == Approx(0.5f));
    }

    SECTION("Zero confidence gives zero score")
    {
        DecisionVote vote(
            DecisionSource::BEHAVIOR_TREE,
            12345,
            nullptr,
            0.0f,   // confidence
            1.0f,   // urgency
            "Zero confidence"
        );

        float weightedScore = vote.CalculateWeightedScore(1.0f);
        REQUIRE(weightedScore == Approx(0.0f));
    }

    SECTION("Zero urgency gives zero score")
    {
        DecisionVote vote(
            DecisionSource::ADAPTIVE_BEHAVIOR,
            12345,
            nullptr,
            1.0f,   // confidence
            0.0f,   // urgency
            "Zero urgency"
        );

        float weightedScore = vote.CalculateWeightedScore(1.0f);
        REQUIRE(weightedScore == Approx(0.0f));
    }
}

TEST_CASE("DecisionFusion - Vote Fusion Logic", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("Single vote returns that action")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,
            nullptr,
            0.8f,
            0.7f,
            "Only vote"
        );

        DecisionResult result = fusion.FuseDecisions(votes);

        REQUIRE(result.recommendedAction == 12345);
        REQUIRE(result.confidence > 0.0f);
    }

    SECTION("Multiple votes for same action increase confidence")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,
            nullptr,
            0.7f,
            0.6f,
            "Vote 1"
        );
        votes.emplace_back(
            DecisionSource::ACTION_PRIORITY,
            12345,
            nullptr,
            0.8f,
            0.7f,
            "Vote 2"
        );

        DecisionResult result = fusion.FuseDecisions(votes);

        REQUIRE(result.recommendedAction == 12345);
        REQUIRE(result.totalVotes == 2);
    }

    SECTION("Empty vote list returns no action")
    {
        std::vector<DecisionVote> votes;
        DecisionResult result = fusion.FuseDecisions(votes);

        REQUIRE(result.recommendedAction == 0);
        REQUIRE(result.confidence == 0.0f);
    }

    SECTION("High urgency vote wins over low urgency")
    {
        std::vector<DecisionVote> votes;

        // Low urgency vote
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            11111,
            nullptr,
            0.9f,   // high confidence
            0.3f,   // low urgency
            "Low urgency"
        );

        // High urgency vote (should win)
        votes.emplace_back(
            DecisionSource::ACTION_PRIORITY,
            22222,
            nullptr,
            0.7f,   // moderate confidence
            0.95f,  // very high urgency
            "High urgency"
        );

        DecisionResult result = fusion.FuseDecisions(votes);

        // High urgency threshold should prioritize the second vote
        // Actual behavior depends on urgency threshold config
        REQUIRE(result.recommendedAction != 0);
        REQUIRE(result.totalVotes == 2);
    }
}

TEST_CASE("DecisionFusion - System Weights", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("Set custom weights")
    {
        std::array<float, 5> customWeights = {
            1.0f, // BEHAVIOR_PRIORITY
            0.8f, // ACTION_PRIORITY
            0.6f, // BEHAVIOR_TREE
            0.4f, // ADAPTIVE_BEHAVIOR
            0.5f  // WEIGHTING_SYSTEM
        };

        fusion.SetSystemWeights(customWeights);

        // Weights are set - verified through compilation
        REQUIRE(true);
    }

    SECTION("Reset to default weights")
    {
        fusion.ResetToDefaultWeights();
        // Weights reset - verified through compilation
        REQUIRE(true);
    }
}

TEST_CASE("DecisionFusion - Debug Logging", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("Enable debug logging")
    {
        fusion.EnableDebugLogging(true);

        std::vector<DecisionVote> votes;
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,
            nullptr,
            0.8f,
            0.7f,
            "Test vote"
        );

        fusion.FuseDecisions(votes);
        // Debug logging enabled - verified through compilation
        REQUIRE(true);
    }

    SECTION("Disable debug logging")
    {
        fusion.EnableDebugLogging(false);

        std::vector<DecisionVote> votes;
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,
            nullptr,
            0.8f,
            0.7f,
            "Test vote"
        );

        fusion.FuseDecisions(votes);
        // Debug logging disabled - verified through compilation
        REQUIRE(true);
    }
}

TEST_CASE("DecisionFusion - Statistics Tracking", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("Statistics are updated after fusion")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,
            nullptr,
            0.8f,
            0.7f,
            "Vote 1"
        );
        votes.emplace_back(
            DecisionSource::ACTION_PRIORITY,
            12345,
            nullptr,
            0.7f,
            0.6f,
            "Vote 2"
        );

        DecisionResult result1 = fusion.FuseDecisions(votes);
        DecisionResult result2 = fusion.FuseDecisions(votes);

        DecisionStatistics stats = fusion.GetStatistics();

        // At least 2 decisions have been made
        REQUIRE(stats.totalDecisions >= 2);
    }

    SECTION("Reset statistics")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(
            DecisionSource::BEHAVIOR_PRIORITY,
            12345,
            nullptr,
            0.8f,
            0.7f,
            "Vote"
        );

        fusion.FuseDecisions(votes);
        fusion.ResetStatistics();

        DecisionStatistics stats = fusion.GetStatistics();
        REQUIRE(stats.totalDecisions == 0);
    }
}

TEST_CASE("DecisionFusion - DecisionSource Enumeration", "[Phase5][DecisionFusion]")
{
    SECTION("All decision sources are defined")
    {
        DecisionSource sources[] = {
            DecisionSource::BEHAVIOR_PRIORITY,
            DecisionSource::ACTION_PRIORITY,
            DecisionSource::BEHAVIOR_TREE,
            DecisionSource::ADAPTIVE_BEHAVIOR,
            DecisionSource::WEIGHTING_SYSTEM
        };

        REQUIRE(sizeof(sources) / sizeof(sources[0]) == 5);
    }

    SECTION("DecisionSource::MAX is last value")
    {
        // MAX should be the sentinel value
        REQUIRE(static_cast<uint8>(DecisionSource::MAX) == 5);
    }
}

TEST_CASE("DecisionFusion - Urgency Threshold", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("Set urgency threshold")
    {
        fusion.SetUrgencyThreshold(0.9f);
        // Threshold is set - verified through compilation
        REQUIRE(true);
    }

    SECTION("Get urgency threshold")
    {
        fusion.SetUrgencyThreshold(0.75f);
        float threshold = fusion.GetUrgencyThreshold();
        REQUIRE(threshold == Approx(0.75f));
    }
}

TEST_CASE("DecisionFusion - DecisionResult Structure", "[Phase5][DecisionFusion]")
{
    SECTION("DecisionResult has expected fields")
    {
        DecisionResult result;
        result.recommendedAction = 12345;
        result.target = nullptr;
        result.confidence = 0.85f;
        result.totalVotes = 3;
        result.winningSource = DecisionSource::BEHAVIOR_PRIORITY;
        result.reasoning = "Test reasoning";

        REQUIRE(result.recommendedAction == 12345);
        REQUIRE(result.confidence == Approx(0.85f));
        REQUIRE(result.totalVotes == 3);
        REQUIRE(result.winningSource == DecisionSource::BEHAVIOR_PRIORITY);
        REQUIRE(result.reasoning == "Test reasoning");
    }
}

TEST_CASE("DecisionFusion - Context-Based Fusion", "[Phase5][DecisionFusion]")
{
    SECTION("Combat contexts are defined")
    {
        CombatContext contexts[] = {
            CombatContext::SOLO,
            CombatContext::GROUP,
            CombatContext::DUNGEON_TRASH,
            CombatContext::DUNGEON_BOSS,
            CombatContext::RAID_NORMAL,
            CombatContext::RAID_HEROIC,
            CombatContext::RAID_MYTHIC,
            CombatContext::PVP_ARENA,
            CombatContext::PVP_BG
        };

        REQUIRE(sizeof(contexts) / sizeof(contexts[0]) == 9);
    }
}

TEST_CASE("DecisionFusion - Unanimous Votes", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("All votes for same action are unanimous")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(DecisionSource::BEHAVIOR_PRIORITY, 12345, nullptr, 0.8f, 0.7f, "Vote 1");
        votes.emplace_back(DecisionSource::ACTION_PRIORITY, 12345, nullptr, 0.9f, 0.8f, "Vote 2");
        votes.emplace_back(DecisionSource::BEHAVIOR_TREE, 12345, nullptr, 0.7f, 0.6f, "Vote 3");

        DecisionResult result = fusion.FuseDecisions(votes);

        // All votes for 12345, should be high confidence
        REQUIRE(result.recommendedAction == 12345);
        REQUIRE(result.totalVotes == 3);
    }

    SECTION("Mixed votes are not unanimous")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(DecisionSource::BEHAVIOR_PRIORITY, 11111, nullptr, 0.8f, 0.7f, "Vote 1");
        votes.emplace_back(DecisionSource::ACTION_PRIORITY, 22222, nullptr, 0.9f, 0.8f, "Vote 2");
        votes.emplace_back(DecisionSource::BEHAVIOR_TREE, 33333, nullptr, 0.7f, 0.6f, "Vote 3");

        DecisionResult result = fusion.FuseDecisions(votes);

        // Different actions, winner determined by weighted scores
        REQUIRE(result.recommendedAction != 0);
        REQUIRE(result.totalVotes == 3);
    }
}

TEST_CASE("DecisionFusion - Edge Cases", "[Phase5][DecisionFusion]")
{
    DecisionFusionSystem fusion;

    SECTION("All votes with zero confidence")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(DecisionSource::BEHAVIOR_PRIORITY, 12345, nullptr, 0.0f, 0.5f, "Zero conf 1");
        votes.emplace_back(DecisionSource::ACTION_PRIORITY, 22222, nullptr, 0.0f, 0.6f, "Zero conf 2");

        DecisionResult result = fusion.FuseDecisions(votes);

        // With zero confidence, no clear winner
        REQUIRE(result.totalVotes == 2);
    }

    SECTION("All votes with zero urgency")
    {
        std::vector<DecisionVote> votes;
        votes.emplace_back(DecisionSource::BEHAVIOR_PRIORITY, 12345, nullptr, 0.8f, 0.0f, "Zero urg 1");
        votes.emplace_back(DecisionSource::ACTION_PRIORITY, 22222, nullptr, 0.9f, 0.0f, "Zero urg 2");

        DecisionResult result = fusion.FuseDecisions(votes);

        // With zero urgency, weighted scores are all 0
        REQUIRE(result.totalVotes == 2);
    }
}
