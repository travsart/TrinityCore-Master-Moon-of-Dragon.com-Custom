/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "DecisionFusionSystem.h"
#include "BotAI.h"
#include "Log.h"
#include "Config.h"
#include <algorithm>
#include <unordered_map>
#include <sstream>

namespace bot { namespace ai {

// Constructor with default weights
DecisionFusionSystem::DecisionFusionSystem()
    : _urgencyThreshold(0.85f)
    , _debugLogging(false)
    , _stats{}
{
    // Load from configuration
    _debugLogging = sConfigMgr->GetBoolDefault("Playerbot.AI.DecisionFusion.LogDecisions", false);
    _urgencyThreshold = sConfigMgr->GetFloatDefault("Playerbot.AI.DecisionFusion.UrgencyThreshold", 0.85f);

    // Load system weights from configuration (with defaults)
    _systemWeights[static_cast<size_t>(DecisionSource::BEHAVIOR_PRIORITY)] =
        sConfigMgr->GetFloatDefault("Playerbot.AI.DecisionFusion.Weight.BehaviorPriority", 0.25f);
    _systemWeights[static_cast<size_t>(DecisionSource::ACTION_PRIORITY)] =
        sConfigMgr->GetFloatDefault("Playerbot.AI.DecisionFusion.Weight.ActionPriority", 0.15f);
    _systemWeights[static_cast<size_t>(DecisionSource::BEHAVIOR_TREE)] =
        sConfigMgr->GetFloatDefault("Playerbot.AI.DecisionFusion.Weight.BehaviorTree", 0.30f);
    _systemWeights[static_cast<size_t>(DecisionSource::ADAPTIVE_BEHAVIOR)] =
        sConfigMgr->GetFloatDefault("Playerbot.AI.DecisionFusion.Weight.AdaptiveBehavior", 0.10f);
    _systemWeights[static_cast<size_t>(DecisionSource::WEIGHTING_SYSTEM)] =
        sConfigMgr->GetFloatDefault("Playerbot.AI.DecisionFusion.Weight.WeightingSystem", 0.20f);

    // Normalize weights to ensure they sum to 1.0
    NormalizeWeights();
}

void DecisionFusionSystem::SetSystemWeights(
    float behaviorPriorityWeight,
    float actionPriorityWeight,
    float behaviorTreeWeight,
    float adaptiveWeight,
    float weightingSystemWeight)
{
    _systemWeights[static_cast<size_t>(DecisionSource::BEHAVIOR_PRIORITY)] = behaviorPriorityWeight;
    _systemWeights[static_cast<size_t>(DecisionSource::ACTION_PRIORITY)] = actionPriorityWeight;
    _systemWeights[static_cast<size_t>(DecisionSource::BEHAVIOR_TREE)] = behaviorTreeWeight;
    _systemWeights[static_cast<size_t>(DecisionSource::ADAPTIVE_BEHAVIOR)] = adaptiveWeight;
    _systemWeights[static_cast<size_t>(DecisionSource::WEIGHTING_SYSTEM)] = weightingSystemWeight;

    NormalizeWeights();
}

void DecisionFusionSystem::NormalizeWeights()
{
    float sum = 0.0f;
    for (float weight : _systemWeights)
        sum += weight;

    if (sum > 0.0f)
    {
        for (float& weight : _systemWeights)
            weight /= sum;
    }
}

std::vector<DecisionVote> DecisionFusionSystem::CollectVotes(BotAI* ai, CombatContext context)
{
    std::vector<DecisionVote> votes;
    votes.reserve(static_cast<size_t>(DecisionSource::MAX));

    if (!ai)
        return votes;

    // TODO: Integrate with existing decision systems
    // This is a placeholder implementation that will be extended
    // as we integrate with BehaviorPriorityManager, ActionPriorityQueue, etc.

    // Example vote collection (to be replaced with actual system integration):
    // 1. Query BehaviorPriorityManager for highest priority behavior
    // 2. Query ActionPriorityQueue for highest priority spell
    // 3. Query Behavior Tree for recommended action
    // 4. Query AdaptiveBehaviorManager for role-specific recommendations
    // 5. Query ActionScoringEngine for utility-scored actions

    if (_debugLogging)
    {
        TC_LOG_DEBUG("playerbot", "DecisionFusionSystem: Collecting votes for bot {}, context {}",
            ai->GetBot() ? ai->GetBot()->GetName() : "unknown",
            static_cast<uint32>(context));
    }

    return votes;
}

DecisionResult DecisionFusionSystem::FuseDecisions(const std::vector<DecisionVote>& votes)
{
    DecisionResult result;

    if (votes.empty())
        return result;

    // Increment decision counter
    ++_stats.totalDecisions;

    // Check for unanimous decisions
    if (AreVotesUnanimous(votes))
        ++_stats.unanimousDecisions;
    else
        ++_stats.conflictResolutions;

    // Step 1: Check for urgent overrides
    const DecisionVote* urgentVote = FindHighestUrgencyVote(votes);
    if (urgentVote && urgentVote->urgency >= _urgencyThreshold)
    {
        // Urgent action takes priority regardless of consensus
        result.actionId = urgentVote->actionId;
        result.target = urgentVote->target;
        result.consensusScore = urgentVote->CalculateWeightedScore(_systemWeights[static_cast<size_t>(urgentVote->source)]);
        result.contributingVotes.push_back(*urgentVote);
        result.fusionReasoning = "URGENT: " + urgentVote->reasoning;

        ++_stats.urgencyOverrides;
        ++_stats.systemWins[static_cast<size_t>(urgentVote->source)];

        if (_debugLogging)
            LogDecision(result, votes);

        return result;
    }

    // Step 2: Group votes by action ID and calculate consensus scores
    std::unordered_map<uint32, std::vector<const DecisionVote*>> votesByAction;
    for (const auto& vote : votes)
    {
        if (vote.actionId != 0)
            votesByAction[vote.actionId].push_back(&vote);
    }

    // Step 3: Calculate consensus score for each action
    struct ActionConsensus
    {
        uint32 actionId;
        Unit* target;
        float consensusScore;
        std::vector<DecisionVote> contributingVotes;
        DecisionSource primarySource;
    };

    std::vector<ActionConsensus> consensuses;
    consensuses.reserve(votesByAction.size());

    for (const auto& [actionId, actionVotes] : votesByAction)
    {
        ActionConsensus consensus;
        consensus.actionId = actionId;
        consensus.target = nullptr;
        consensus.consensusScore = 0.0f;

        float highestVoteScore = 0.0f;

        for (const DecisionVote* vote : actionVotes)
        {
            float weightedScore = vote->CalculateWeightedScore(_systemWeights[static_cast<size_t>(vote->source)]);
            consensus.consensusScore += weightedScore;
            consensus.contributingVotes.push_back(*vote);

            // Track highest scoring vote for target selection
            if (weightedScore > highestVoteScore)
            {
                highestVoteScore = weightedScore;
                consensus.target = vote->target;
                consensus.primarySource = vote->source;
            }

            if (_debugLogging)
                LogVote(*vote, weightedScore);
        }

        consensuses.push_back(std::move(consensus));
    }

    // Step 4: Select action with highest consensus score
    if (consensuses.empty())
        return result;

    auto bestConsensus = std::max_element(consensuses.begin(), consensuses.end(),
        [](const ActionConsensus& a, const ActionConsensus& b) {
            return a.consensusScore < b.consensusScore;
        });

    // Step 5: Build final result
    result.actionId = bestConsensus->actionId;
    result.target = bestConsensus->target;
    result.consensusScore = bestConsensus->consensusScore;
    result.contributingVotes = std::move(bestConsensus->contributingVotes);

    // Build reasoning string
    std::ostringstream reasoning;
    reasoning << "Consensus from " << result.contributingVotes.size() << " system(s): ";
    for (size_t i = 0; i < result.contributingVotes.size(); ++i)
    {
        if (i > 0)
            reasoning << ", ";
        reasoning << GetSourceName(result.contributingVotes[i].source);
    }
    result.fusionReasoning = reasoning.str();

    // Update statistics
    ++_stats.systemWins[static_cast<size_t>(bestConsensus->primarySource)];

    if (_debugLogging)
        LogDecision(result, votes);

    return result;
}

void DecisionFusionSystem::LogVote(const DecisionVote& vote, float weightedScore) const
{
    TC_LOG_DEBUG("playerbot", "DecisionFusion Vote: [{}] Action {} | Confidence {:.2f} | Urgency {:.2f} | Weighted {:.2f} | Reason: {}",
        GetSourceName(vote.source),
        vote.actionId,
        vote.confidence,
        vote.urgency,
        weightedScore,
        vote.reasoning);
}

void DecisionFusionSystem::LogDecision(const DecisionResult& result, const std::vector<DecisionVote>& allVotes) const
{
    if (!result.IsValid())
    {
        TC_LOG_DEBUG("playerbot", "DecisionFusion Result: NO VALID DECISION (received {} votes)", allVotes.size());
        return;
    }

    TC_LOG_DEBUG("playerbot", "DecisionFusion Result: Action {} | Score {:.2f} | Votes {} | Reason: {}",
        result.actionId,
        result.consensusScore,
        result.contributingVotes.size(),
        result.fusionReasoning);

    // Log contributing systems
    std::ostringstream systems;
    for (size_t i = 0; i < result.contributingVotes.size(); ++i)
    {
        if (i > 0)
            systems << ", ";
        systems << GetSourceName(result.contributingVotes[i].source)
                << " (" << result.contributingVotes[i].confidence << ")";
    }

    TC_LOG_DEBUG("playerbot", "DecisionFusion Contributing Systems: {}", systems.str());
}

bool DecisionFusionSystem::AreVotesUnanimous(const std::vector<DecisionVote>& votes) const
{
    if (votes.size() <= 1)
        return true;

    uint32 firstAction = 0;
    for (const auto& vote : votes)
    {
        if (vote.actionId == 0)
            continue;

        if (firstAction == 0)
            firstAction = vote.actionId;
        else if (firstAction != vote.actionId)
            return false;
    }

    return true;
}

const DecisionVote* DecisionFusionSystem::FindHighestUrgencyVote(const std::vector<DecisionVote>& votes) const
{
    const DecisionVote* highestUrgency = nullptr;
    float maxUrgency = 0.0f;

    for (const auto& vote : votes)
    {
        if (vote.urgency > maxUrgency)
        {
            maxUrgency = vote.urgency;
            highestUrgency = &vote;
        }
    }

    return highestUrgency;
}

const char* DecisionFusionSystem::GetSourceName(DecisionSource source)
{
    switch (source)
    {
        case DecisionSource::BEHAVIOR_PRIORITY:  return "BehaviorPriority";
        case DecisionSource::ACTION_PRIORITY:    return "ActionPriority";
        case DecisionSource::BEHAVIOR_TREE:      return "BehaviorTree";
        case DecisionSource::ADAPTIVE_BEHAVIOR:  return "AdaptiveBehavior";
        case DecisionSource::WEIGHTING_SYSTEM:   return "WeightingSystem";
        default:                                  return "Unknown";
    }
}

}} // namespace bot::ai
