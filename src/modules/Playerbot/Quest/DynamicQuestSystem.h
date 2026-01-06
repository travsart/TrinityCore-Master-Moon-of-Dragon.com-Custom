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
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "QuestDef.h"
#include "Position.h"
#include "../Core/DI/Interfaces/IUnifiedQuestManager.h"  // For QuestSelectionStrategy enum
#include "../Core/DI/Interfaces/IDynamicQuestSystem.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

class Player;
class Quest;
class Group;

namespace Playerbot
{

enum class QuestPriority : uint8
{
    TRIVIAL     = 0,  // Gray quests, very low reward
    LOW         = 1,  // Below level quests
    NORMAL      = 2,  // At-level quests
    HIGH        = 3,  // Important story/chain quests
    CRITICAL    = 4,  // Elite/group/dungeon quests
    LEGENDARY   = 5   // Raid/epic questlines
};

enum class QuestType : uint8
{
    KILL_COLLECT    = 0,  // Kill X mobs, collect Y items
    DELIVERY        = 1,  // Take item from A to B
    ESCORT          = 2,  // Protect NPC during movement
    EXPLORATION     = 3,  // Discover areas/locations
    INTERACTION     = 4,  // Talk to NPCs, use objects
    DUNGEON         = 5,  // Instance-specific quests
    ELITE           = 6,  // Requires group coordination
    DAILY           = 7,  // Repeatable daily quests
    SEASONAL        = 8,  // Event-specific quests
    PVP             = 9   // Player vs Player objectives
};

// QuestSelectionStrategy enum defined in IUnifiedQuestManager.h interface

struct QuestMetadata
{
    uint32 questId;
    QuestType type;
    QuestPriority priority;
    uint32 recommendedLevel;
    uint32 minLevel;
    uint32 maxLevel;
    uint32 requiredPlayers;
    uint32 estimatedDuration;  // in seconds
    float difficultyRating;    // 0.0 - 10.0
    std::vector<uint32> prerequisites;
    std::vector<uint32> followupQuests;
    std::vector<uint32> requiredSkills;
    std::vector<uint32> recommendedClasses;
    Position questLocation;
    uint32 questGiver;
    bool isElite;
    bool isDungeon;
    bool isRaid;
    bool isDaily;
    bool isSeasonal;

    // Default constructor
    QuestMetadata() : questId(0), type(QuestType::KILL_COLLECT)
        , priority(QuestPriority::NORMAL), recommendedLevel(1), minLevel(1)
        , maxLevel(80), requiredPlayers(1), estimatedDuration(1200) // 20 minutes
        , difficultyRating(5.0f), questGiver(0), isElite(false)
        , isDungeon(false), isRaid(false), isDaily(false), isSeasonal(false) {}

    QuestMetadata(uint32 id) : questId(id), type(QuestType::KILL_COLLECT)
        , priority(QuestPriority::NORMAL), recommendedLevel(1), minLevel(1)
        , maxLevel(80), requiredPlayers(1), estimatedDuration(1200) // 20 minutes
        , difficultyRating(5.0f), questGiver(0), isElite(false)
        , isDungeon(false), isRaid(false), isDaily(false), isSeasonal(false) {}
};

struct QuestProgress
{
    uint32 questId;
    uint32 botGuid;
    uint32 startTime;
    uint32 lastUpdateTime;
    float completionPercentage;
    std::unordered_map<uint32, uint32> objectiveProgress; // objectiveIndex -> current count
    std::unordered_map<uint32, uint32> objectiveTargets;  // objectiveIndex -> required count
    std::vector<Position> visitedLocations;
    std::vector<uint32> killedCreatures;
    std::vector<uint32> collectedItems;
    bool isStuck;
    uint32 stuckTime;
    uint32 retryCount;

    QuestProgress(uint32 qId, uint32 guid) : questId(qId), botGuid(guid)
        , startTime(GameTime::GetGameTimeMS()), lastUpdateTime(GameTime::GetGameTimeMS())
        , completionPercentage(0.0f), isStuck(false), stuckTime(0), retryCount(0) {}
};

class TC_GAME_API DynamicQuestSystem final : public IDynamicQuestSystem
{
public:
    explicit DynamicQuestSystem(Player* bot);
    ~DynamicQuestSystem();
    DynamicQuestSystem(DynamicQuestSystem const&) = delete;
    DynamicQuestSystem& operator=(DynamicQuestSystem const&) = delete;

    // Quest discovery and assignment
    std::vector<uint32> DiscoverAvailableQuests(Player* bot) override;
    std::vector<uint32> GetRecommendedQuests(Player* bot, QuestSelectionStrategy strategy = QuestSelectionStrategy::LEVEL_PROGRESSION) override;
    bool AssignQuestToBot(uint32 questId, Player* bot) override;
    void AutoAssignQuests(Player* bot, uint32 maxQuests = 10) override;

    // Quest prioritization
    QuestPriority CalculateQuestPriority(uint32 questId, Player* bot) override;
    std::vector<uint32> SortQuestsByPriority(const std::vector<uint32>& questIds, Player* bot) override;
    bool ShouldAbandonQuest(uint32 questId, Player* bot) override;

    // Quest execution and coordination
    void UpdateQuestProgress(Player* bot) override;
    void ExecuteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    bool CanCompleteQuestObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void HandleQuestCompletion(Player* bot, uint32 questId) override;

    // Group quest coordination
    bool FormQuestGroup(uint32 questId, Player* initiator) override;
    void CoordinateGroupQuest(Group* group, uint32 questId) override;
    void ShareQuestProgress(Group* group, uint32 questId) override;
    bool CanShareQuest(uint32 questId, Player* from, Player* to) override;

    // Quest pathfinding and navigation
    Position GetNextQuestLocation(Player* bot, uint32 questId) override;
    std::vector<Position> GenerateQuestPath(Player* bot, uint32 questId) override;
    void HandleQuestNavigation(Player* bot, uint32 questId) override;
    bool IsQuestLocationReachable(Player* bot, const Position& location) override;

    // Dynamic quest adaptation
    void AdaptQuestDifficulty(uint32 questId, Player* bot) override;
    void HandleQuestStuckState(Player* bot, uint32 questId) override;
    void RetryFailedObjective(Player* bot, uint32 questId, uint32 objectiveIndex) override;
    void OptimizeQuestOrder(Player* bot) override;

    // Quest chain management
    void TrackQuestChains(Player* bot) override;
    std::vector<uint32> GetQuestChain(uint32 questId) override;
    uint32 GetNextQuestInChain(uint32 completedQuestId) override;
    void AdvanceQuestChain(Player* bot, uint32 completedQuestId) override;

    // Zone-based quest optimization
    void OptimizeZoneQuests(Player* bot) override;
    std::vector<uint32> GetZoneQuests(uint32 zoneId, Player* bot) override;
    void PlanZoneCompletion(Player* bot, uint32 zoneId) override;
    bool ShouldMoveToNewZone(Player* bot) override;

    // Quest reward analysis (QuestReward defined in IDynamicQuestSystem.h interface)
    QuestReward AnalyzeQuestReward(uint32 questId, Player* bot) override;
    float CalculateQuestValue(uint32 questId, Player* bot) override;
    bool IsQuestWorthwhile(uint32 questId, Player* bot) override;

    // Performance monitoring (QuestMetrics defined in IDynamicQuestSystem.h interface)
    QuestMetrics GetBotQuestMetrics(uint32 botGuid) override;
    QuestMetrics GetGlobalQuestMetrics() override;

    // Configuration and settings
    void SetQuestStrategy(uint32 botGuid, QuestSelectionStrategy strategy) override;
    QuestSelectionStrategy GetQuestStrategy(uint32 botGuid) override;
    void SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests) override;
    void EnableQuestGrouping(uint32 botGuid, bool enable) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void CleanupCompletedQuests() override;
    void ValidateQuestStates() override;

private:
    Player* _bot;

    // Per-bot data structures (instance members)
    std::unordered_map<uint32, std::vector<QuestProgress>> _botQuestProgress; // botGuid -> quests
    std::unordered_map<uint32, QuestSelectionStrategy> _botStrategies;
    std::unordered_map<uint32, QuestMetrics> _botMetrics;

    // Group quest coordination (per-bot)
    std::unordered_map<uint32, std::vector<uint32>> _questGroups; // questId -> botGuids
    std::queue<std::pair<uint32, uint32>> _groupQuestRequests; // <questId, requesterGuid>

    // ========================================================================
    // STATIC/SHARED DATA - Loaded once, shared across all bot instances
    // This prevents 20,000+ quest entries being duplicated per bot
    // ========================================================================
    static std::unordered_map<uint32, QuestMetadata> _questMetadata;
    static std::unordered_map<uint32, std::vector<uint32>> _questChains; // questId -> chain
    static std::unordered_map<uint32, std::vector<uint32>> _questPrerequisites; // questId -> required quests
    static std::unordered_map<uint32, std::vector<uint32>> _questFollowups; // questId -> followup quests
    static std::unordered_map<uint32, std::vector<uint32>> _zoneQuests; // zoneId -> questIds
    static std::unordered_map<uint32, std::vector<Position>> _questHotspots; // zoneId -> optimal locations
    static std::mutex _staticDataMutex;
    static bool _staticDataInitialized;
    

    // Helper functions
    void LoadQuestMetadata();
    void AnalyzeQuestDependencies();
    void BuildQuestChains();
    void OptimizeQuestRoutes();
    QuestType DetermineQuestType(const Quest* quest);
    float CalculateQuestDifficulty(const Quest* quest, Player* bot);
    bool MeetsQuestRequirements(const Quest* quest, Player* bot);
    Position FindOptimalQuestStartLocation(uint32 questId, Player* bot);
    void UpdateQuestObjectiveProgress(QuestProgress& progress, const Quest* quest, Player* bot);
    bool IsQuestObjectiveComplete(const QuestProgress& progress, uint32 objectiveIndex);
    void HandleQuestGiverInteraction(Player* bot, uint32 questGiverId);
    void ProcessQuestTurnIn(Player* bot, uint32 questId);

    // Advanced quest strategies
    void ExecuteSoloStrategy(Player* bot);
    void ExecuteGroupStrategy(Player* bot);
    void ExecuteZoneStrategy(Player* bot);
    void ExecuteLevelStrategy(Player* bot);
    void ExecuteGearStrategy(Player* bot);
    void ExecuteStoryStrategy(Player* bot);
    void ExecuteReputationStrategy(Player* bot);

    // Quest difficulty scaling
    void ScaleQuestForBot(QuestMetadata& metadata, Player* bot);
    void AdjustQuestRewards(uint32 questId, Player* bot);
    void ModifyQuestObjectives(uint32 questId, Player* bot);

    // Performance optimization
    void OptimizeQuestSelection(Player* bot);
    void BalanceQuestLoad();
    void PredictQuestCompletionTime(uint32 questId, Player* bot);
    void AnalyzeQuestEfficiency(uint32 botGuid);

    // Gear optimization helpers
    float CalculateGearScoreImprovement(Player* bot, const std::vector<uint32>& items);

    // Quest metadata and chain helpers
    void PopulateQuestMetadata(uint32 questId);
    bool IsPartOfQuestChain(uint32 questId);

    // Constants
    static constexpr uint32 MAX_CONCURRENT_QUESTS = 25;
    static constexpr uint32 QUEST_UPDATE_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 STUCK_DETECTION_TIME = 30000; // 30 seconds
    static constexpr uint32 MAX_QUEST_RETRIES = 3;
    static constexpr float MIN_QUEST_VALUE_THRESHOLD = 0.1f;
    static constexpr uint32 QUEST_CLEANUP_INTERVAL = 300000; // 5 minutes
    static constexpr float ELITE_QUEST_DIFFICULTY_MULTIPLIER = 2.0f;
    static constexpr float GROUP_QUEST_EFFICIENCY_BONUS = 1.5f;
    static constexpr uint32 ZONE_OPTIMIZATION_THRESHOLD = 5; // 5+ quests in zone
};

} // namespace Playerbot