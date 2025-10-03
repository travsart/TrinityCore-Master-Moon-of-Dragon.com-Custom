/*
 * Copyright (C) 2024 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITYCORE_BOT_QUEST_MANAGER_H
#define TRINITYCORE_BOT_QUEST_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"
#include "QuestDef.h"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <deque>

class Player;
class Creature;
class GameObject;
class Quest;
class WorldObject;
class ItemTemplate;
enum QuestStatus : uint8;

namespace Playerbot
{
    // Forward declarations
    class BotAI;
    class QuestSelectionStrategy;
    class QuestCache;
    struct QuestEvaluation;

    /**
     * BotQuestManager - Complete quest automation system for PlayerBots
     *
     * Handles all aspects of quest management including:
     * - Quest discovery and acceptance
     * - Objective tracking and completion
     * - Quest turn-in and reward selection
     * - Strategic quest prioritization
     * - Performance-optimized caching
     */
    class QuestManager
    {
    public:
        explicit QuestManager(Player* bot, BotAI* ai);
        ~QuestManager();

        // Core lifecycle methods
        void Initialize();
        void Update(uint32 diff);
        void Reset();
        void Shutdown();

        // Quest operations
        bool CanAcceptQuest(uint32 questId) const;
        bool AcceptQuest(uint32 questId, WorldObject* questGiver = nullptr);
        bool CompleteQuest(uint32 questId, WorldObject* questGiver = nullptr);
        bool TurnInQuest(uint32 questId, uint32 rewardChoice = 0, WorldObject* questGiver = nullptr);
        bool AbandonQuest(uint32 questId);

        // Quest discovery
        void ScanForQuests();
        std::vector<uint32> GetAvailableQuests() const;
        std::vector<uint32> GetActiveQuests() const;
        std::vector<uint32> GetCompletableQuests() const;

        // Quest selection and evaluation
        uint32 SelectBestQuest(std::vector<uint32> const& availableQuests);
        float EvaluateQuest(Quest const* quest) const;
        uint32 SelectBestReward(Quest const* quest) const;

        // Quest progress tracking
        QuestStatus GetQuestStatus(uint32 questId) const;
        void UpdateQuestProgress();
        bool IsQuestComplete(uint32 questId) const;
        float GetQuestCompletionPercent(uint32 questId) const;

        // Quest giver interaction
        bool IsQuestGiverNearby() const;
        Creature* FindNearestQuestGiver() const;
        GameObject* FindNearestQuestObject() const;
        bool MoveToQuestGiver(WorldObject* questGiver);

        // Group quest sharing
        void ShareGroupQuests();
        bool AcceptSharedQuest(uint32 questId);

        // Performance monitoring
        float GetCPUUsage() const { return m_cpuUsage; }
        size_t GetMemoryUsage() const;
        uint32 GetUpdateCount() const { return m_updateCount; }

        // Configuration
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool enabled) { m_enabled = enabled; }

        // Statistics
        struct Statistics
        {
            uint32 questsAccepted = 0;
            uint32 questsCompleted = 0;
            uint32 questsAbandoned = 0;
            uint32 questsFailed = 0;
            uint32 totalXPEarned = 0;
            uint32 totalGoldEarned = 0;
            float avgTimePerQuest = 0.0f;
            uint32 dailyQuestsCompleted = 0;
            uint32 dungeonQuestsCompleted = 0;
        };

        Statistics const& GetStatistics() const { return m_stats; }

    private:
        // Quest phases for state machine
        enum class QuestPhase
        {
            IDLE,           // No quest activity
            SCANNING,       // Looking for available quests
            ACCEPTING,      // Accepting new quests
            PROGRESSING,    // Working on quest objectives
            COMPLETING,     // Turning in completed quests
            MANAGING        // Managing quest log (abandoning, etc.)
        };

        // Quest evaluation criteria
        struct QuestPriority
        {
            uint32 questId = 0;
            float score = 0.0f;
            float xpReward = 0.0f;
            float goldReward = 0.0f;
            float itemValue = 0.0f;
            float reputationValue = 0.0f;
            float distanceScore = 0.0f;
            float levelScore = 0.0f;
            float groupBonus = 0.0f;
            bool isDaily = false;
            bool isDungeon = false;
            bool isGroupQuest = false;
        };

        // Quest giver tracking
        struct QuestGiverInfo
        {
            ObjectGuid guid;
            uint32 entry = 0;
            float distance = 0.0f;
            uint32 availableQuests = 0;
            uint32 completableQuests = 0;
            uint32 lastCheckTime = 0;
        };

        // Quest progress tracking
        struct QuestProgress
        {
            uint32 questId = 0;
            uint32 startTime = 0;
            uint32 lastUpdateTime = 0;
            std::array<uint32, 32> objectiveProgress;  // Max objectives per quest (reasonable limit)
            float completionPercent = 0.0f;
            bool isComplete = false;
            uint32 attemptCount = 0;
        };

        // Internal state management
        void UpdateQuestPhase(uint32 diff);
        void ProcessIdlePhase();
        void ProcessScanningPhase();
        void ProcessAcceptingPhase();
        void ProcessProgressingPhase();
        void ProcessCompletingPhase();
        void ProcessManagingPhase();

        // Quest discovery methods
        void ScanCreatureQuestGivers();
        void ScanGameObjectQuestGivers();
        void ScanItemQuests();
        void UpdateQuestGiverCache();

        // Quest evaluation methods
        QuestPriority EvaluateQuestPriority(Quest const* quest) const;
        float CalculateXPValue(Quest const* quest) const;
        float CalculateGoldValue(Quest const* quest) const;
        float CalculateItemValue(Quest const* quest) const;
        float CalculateReputationValue(Quest const* quest) const;
        float CalculateDistanceScore(Quest const* quest) const;
        float CalculateLevelScore(Quest const* quest) const;
        float CalculateGroupBonus(Quest const* quest) const;

        // Quest management methods
        void AcceptBestQuests();
        void TurnInCompletedQuests();
        void AbandonLowPriorityQuests();
        void ManageQuestLog();
        bool ShouldAbandonQuest(uint32 questId) const;

        // Objective tracking methods
        void UpdateObjectiveProgress(uint32 questId);
        bool CheckKillObjective(Quest const* quest, uint32 objectiveIndex) const;
        bool CheckItemObjective(Quest const* quest, uint32 objectiveIndex) const;
        bool CheckAreaObjective(Quest const* quest, uint32 objectiveIndex) const;

        // Reward selection methods
        uint32 SelectItemReward(Quest const* quest) const;
        uint32 SelectChoiceReward(Quest const* quest) const;
        bool IsRewardUseful(ItemTemplate const* itemTemplate) const;
        float CalculateItemScore(ItemTemplate const* itemTemplate) const;

        // Helper methods
        bool IsQuestTooLowLevel(Quest const* quest) const;
        bool IsQuestTooHighLevel(Quest const* quest) const;
        bool HasQuestPrerequisites(Quest const* quest) const;
        bool MeetsQuestRequirements(Quest const* quest) const;
        uint32 GetQuestLogSpace() const;
        void RecordQuestTime(uint32 questId, uint32 timeSpent);
        void UpdateStatistics(Quest const* quest, bool completed);

        // Cache management
        void UpdateQuestCache();
        void InvalidateCache();
        void ClearQuestGiverCache();

        // Performance tracking
        void StartPerformanceTimer();
        void EndPerformanceTimer();
        void UpdatePerformanceMetrics();

    private:
        Player* m_bot;
        BotAI* m_ai;
        bool m_enabled;

        // State management
        QuestPhase m_currentPhase;
        uint32 m_phaseTimer;
        uint32 m_timeSinceLastUpdate;
        uint32 m_updateInterval;

        // Quest tracking
        std::unordered_map<uint32, QuestProgress> m_questProgress;
        std::unordered_set<uint32> m_ignoredQuests;
        std::deque<uint32> m_recentlyCompleted;

        // Quest giver cache
        std::vector<QuestGiverInfo> m_questGivers;
        uint32 m_lastQuestGiverScan;
        uint32 m_questGiverScanInterval;

        // Quest evaluation cache
        std::unordered_map<uint32, QuestPriority> m_questPriorities;
        uint32 m_lastPriorityCalculation;

        // Available quests cache
        std::vector<uint32> m_availableQuests;
        std::vector<uint32> m_completableQuests;
        uint32 m_lastAvailableScan;

        // Configuration
        bool m_autoAccept;
        bool m_autoComplete;
        bool m_acceptDailies;
        bool m_acceptDungeonQuests;
        bool m_prioritizeGroupQuests;
        uint32 m_maxActiveQuests;
        uint32 m_maxTravelDistance;
        float m_minQuestLevel;
        float m_maxQuestLevel;

        // Performance metrics
        std::chrono::high_resolution_clock::time_point m_performanceStart;
        std::chrono::microseconds m_lastUpdateDuration;
        std::chrono::microseconds m_totalUpdateTime;
        uint32 m_updateCount;
        float m_cpuUsage;

        // Statistics
        Statistics m_stats;

        // Quest strategy handler
        std::unique_ptr<QuestSelectionStrategy> m_strategy;

        // Quest cache for performance
        std::unique_ptr<QuestCache> m_cache;
    };

    /**
     * QuestStrategy - Strategic quest selection AI
     */
    class QuestSelectionStrategy
    {
    public:
        enum class Strategy
        {
            SIMPLE,         // Basic quest selection
            OPTIMAL,        // Optimized for XP/hour
            GROUP,          // Prioritize group quests
            COMPLETIONIST,  // Complete all quests in zone
            SPEED_LEVELING  // Fastest leveling path
        };

        explicit QuestSelectionStrategy(Strategy strategy = Strategy::OPTIMAL);

        float EvaluateQuest(Quest const* quest, Player* bot) const;
        std::vector<uint32> SelectQuestPath(std::vector<uint32> const& available, Player* bot) const;
        Strategy GetStrategy() const { return m_strategy; }
        void SetStrategy(Strategy strategy) { m_strategy = strategy; }

    private:
        Strategy m_strategy;
    };

    /**
     * QuestCache - Performance-optimized quest data caching
     */
    class QuestCache
    {
    public:
        explicit QuestCache();

        void Update(Player* bot);
        void Invalidate();

        bool GetQuestStatus(uint32 questId, QuestStatus& status) const;
        bool GetQuestProgress(uint32 questId, float& progress) const;
        bool IsQuestCached(uint32 questId) const;

        std::vector<uint32> const& GetActiveQuests() const { return m_activeQuests; }
        std::vector<uint32> const& GetCompletableQuests() const { return m_completableQuests; }
        uint32 GetLastUpdateTime() const { return m_lastUpdateTime; }

    private:
        struct CachedQuest
        {
            QuestStatus status;
            float progress;
            uint32 updateTime;
        };

        std::unordered_map<uint32, CachedQuest> m_questCache;
        std::vector<uint32> m_activeQuests;
        std::vector<uint32> m_completableQuests;
        uint32 m_lastUpdateTime;
        bool m_isDirty;
    };

} // namespace Playerbot

#endif // TRINITYCORE_BOT_QUEST_MANAGER_H