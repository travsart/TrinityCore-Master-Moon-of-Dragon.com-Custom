/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include "Position.h"
#include <vector>
#include <unordered_map>
#include <atomic>
#include <chrono>

class Player;
class Group;

namespace Playerbot
{

// Forward declarations
enum class QuestPriority : uint8;
enum class QuestType : uint8;
// QuestSelectionStrategy enum defined in IUnifiedQuestManager.h (included below to avoid circular dependency issues)

struct QuestMetadata;
struct QuestProgress;

// QuestMetrics definition (needs full definition for return by value)
struct QuestMetrics
{
    std::atomic<uint32> questsStarted{0};
    std::atomic<uint32> questsCompleted{0};
    std::atomic<uint32> questsAbandoned{0};
    std::atomic<uint32> questsFailed{0};
    std::atomic<float> averageCompletionTime{1200.0f}; // 20 minutes
    std::atomic<float> successRate{0.85f};
    std::atomic<float> efficiencyRating{1.0f};
    std::atomic<uint32> experienceGained{0};
    std::atomic<uint32> goldEarned{0};
    std::chrono::steady_clock::time_point lastUpdate;

    // Default constructor
    QuestMetrics() : lastUpdate(std::chrono::steady_clock::now()) {}

    void Reset() {
        questsStarted = 0; questsCompleted = 0; questsAbandoned = 0; questsFailed = 0;
        averageCompletionTime = 1200.0f; successRate = 0.85f; efficiencyRating = 1.0f;
        experienceGained = 0; goldEarned = 0;
        lastUpdate = std::chrono::steady_clock::now();
    }

    float GetCompletionRate() const {
        uint32 started = questsStarted.load();
        uint32 completed = questsCompleted.load();
        return started > 0 ? (float)completed / started : 0.0f;
    }

    // Copy constructor for atomic members
    QuestMetrics(const QuestMetrics& other)
        : questsStarted(other.questsStarted.load()),
          questsCompleted(other.questsCompleted.load()),
          questsAbandoned(other.questsAbandoned.load()),
          questsFailed(other.questsFailed.load()),
          averageCompletionTime(other.averageCompletionTime.load()),
          successRate(other.successRate.load()),
          efficiencyRating(other.efficiencyRating.load()),
          experienceGained(other.experienceGained.load()),
          goldEarned(other.goldEarned.load()),
          lastUpdate(other.lastUpdate) {}

    // Assignment operator for atomic members
    QuestMetrics& operator=(const QuestMetrics& other) {
        if (this != &other) {
            questsStarted = other.questsStarted.load();
            questsCompleted = other.questsCompleted.load();
            questsAbandoned = other.questsAbandoned.load();
            questsFailed = other.questsFailed.load();
            averageCompletionTime = other.averageCompletionTime.load();
            successRate = other.successRate.load();
            efficiencyRating = other.efficiencyRating.load();
            experienceGained = other.experienceGained.load();
            goldEarned = other.goldEarned.load();
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }
};

// QuestReward definition (needs full definition for return by value)
struct QuestReward
{
    uint32 experience;
    uint32 gold;
    std::vector<uint32> items;
    std::vector<std::pair<uint32, uint32>> reputation; // factionId, amount
    uint32 talentPoints;
    float gearScore;
    float rewardValue;

    QuestReward() : experience(0), gold(0), talentPoints(0), gearScore(0.0f), rewardValue(0.0f) {}
};

class TC_GAME_API IDynamicQuestSystem
{
public:
    virtual ~IDynamicQuestSystem() = default;

    // Quest discovery and assignment
    virtual ::std::vector<uint32> DiscoverAvailableQuests(Player* bot) = 0;
    virtual ::std::vector<uint32> GetRecommendedQuests(Player* bot, QuestSelectionStrategy strategy) = 0;
    virtual bool AssignQuestToBot(uint32 questId, Player* bot) = 0;
    virtual void AutoAssignQuests(Player* bot, uint32 maxQuests) = 0;

    // Quest prioritization
    virtual QuestPriority CalculateQuestPriority(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<uint32> SortQuestsByPriority(const ::std::vector<uint32>& questIds, Player* bot) = 0;
    virtual bool ShouldAbandonQuest(uint32 questId, Player* bot) = 0;

    // Quest execution and coordination
    virtual void UpdateQuestProgress(Player* bot) = 0;
    virtual void ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual bool CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void HandleQuestCompletion(Player* bot, uint32 questId) = 0;

    // Group quest coordination
    virtual bool FormQuestGroup(uint32 questId, Player* initiator) = 0;
    virtual void CoordinateGroupQuest(Group* group, uint32 questId) = 0;
    virtual void ShareQuestProgress(Group* group, uint32 questId) = 0;
    virtual bool CanShareQuest(uint32 questId, Player* from, Player* to) = 0;

    // Quest pathfinding and navigation
    virtual Position GetNextQuestLocation(Player* bot, uint32 questId) = 0;
    virtual ::std::vector<Position> GenerateQuestPath(Player* bot, uint32 questId) = 0;
    virtual void HandleQuestNavigation(Player* bot, uint32 questId) = 0;
    virtual bool IsQuestLocationReachable(Player* bot, const Position& location) = 0;

    // Dynamic quest adaptation
    virtual void AdaptQuestDifficulty(uint32 questId, Player* bot) = 0;
    virtual void HandleQuestStuckState(Player* bot, uint32 questId) = 0;
    virtual void RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex) = 0;
    virtual void OptimizeQuestOrder(Player* bot) = 0;

    // Quest chain management
    virtual void TrackQuestChains(Player* bot) = 0;
    virtual ::std::vector<uint32> GetQuestChain(uint32 questId) = 0;
    virtual uint32 GetNextQuestInChain(uint32 completedQuestId) = 0;
    virtual void AdvanceQuestChain(Player* bot, uint32 completedQuestId) = 0;

    // Zone-based quest optimization
    virtual void OptimizeZoneQuests(Player* bot) = 0;
    virtual ::std::vector<uint32> GetZoneQuests(uint32 zoneId, Player* bot) = 0;
    virtual void PlanZoneCompletion(Player* bot, uint32 zoneId) = 0;
    virtual bool ShouldMoveToNewZone(Player* bot) = 0;

    // Quest reward analysis
    virtual QuestReward AnalyzeQuestReward(uint32 questId, Player* bot) = 0;
    virtual float CalculateQuestValue(uint32 questId, Player* bot) = 0;
    virtual bool IsQuestWorthwhile(uint32 questId, Player* bot) = 0;

    // Performance monitoring
    virtual QuestMetrics GetBotQuestMetrics(uint32 botGuid) = 0;
    virtual QuestMetrics GetGlobalQuestMetrics() = 0;

    // Configuration and settings
    virtual void SetQuestStrategy(uint32 botGuid, QuestSelectionStrategy strategy) = 0;
    virtual QuestSelectionStrategy GetQuestStrategy(uint32 botGuid) = 0;
    virtual void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests) = 0;
    virtual void EnableQuestGrouping(uint32 botGuid, bool enable) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void CleanupCompletedQuests() = 0;
    virtual void ValidateQuestStates() = 0;
};

} // namespace Playerbot
