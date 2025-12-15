/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Strategy.h"
#include "../../Quest/ObjectiveTracker.h"
#include "Movement/BotMovementUtil.h"
#include "Player.h"
#include "QuestDef.h"
#include "../../Game/QuestAcceptanceManager.h"
#include <vector>

namespace Playerbot
{

class BotAI;
class QuestAcceptanceManager;

/**
 * @brief Quest Strategy - Drives bot movement and actions for quest objectives
 *
 * This strategy activates when bots have active quests and guides them to:
 * - Move to quest objective locations
 * - Kill quest mobs
 * - Collect quest items
 * - Explore quest areas
 * - Turn in completed quests
 *
 * Integrates with:
 * - QuestManager (quest acceptance/turn-in)
 * - ObjectiveTracker (objective prioritization)
 * - BotMovementUtil (movement execution)
 * - GroupCombatStrategy (combat during quests)
 */
class TC_GAME_API QuestStrategy : public Strategy
{
public:
    QuestStrategy();
    ~QuestStrategy() override = default;

    // Strategy interface
    void InitializeActions() override;
    void InitializeTriggers() override;
    void InitializeValues() override;
    bool IsActive(BotAI* ai) const override;
    float GetRelevance(BotAI* ai) const override;
    void UpdateBehavior(BotAI* ai, uint32 diff) override;

    // ========================================================================
    // GRIND FALLBACK INTEGRATION
    // ========================================================================

    /**
     * @brief Get number of consecutive quest giver search failures
     * @return Failure count (used by GrindStrategy to determine when to activate)
     */
    uint32 GetQuestGiverSearchFailures() const { return _questGiverSearchFailures; }

    /**
     * @brief Check if quest strategy has exhausted all options
     * @return true if no quests, no quest givers, and no quest hubs found
     */
    bool HasExhaustedQuestOptions() const { return _questGiverSearchFailures >= 3; }

    /**
     * @brief Reset quest search failure counter (called when quests become available)
     */
    void ResetQuestSearchFailures() { _questGiverSearchFailures = 0; }

private:
    // Quest navigation phases
    enum class QuestPhase
    {
        IDLE,                   // No quest activity
        MOVING_TO_OBJECTIVE,    // Navigating to quest area
        ENGAGING_TARGETS,       // Killing quest mobs
        COLLECTING_ITEMS,       // Looting quest items
        EXPLORING_AREA,         // Exploring for quest completion
        TURNING_IN,             // Moving to turn in quest
    };

    // Core quest execution
    void ProcessQuestObjectives(BotAI* ai);
    void SearchForQuestGivers(BotAI* ai);
    void NavigateToObjective(BotAI* ai, ObjectiveState const& objective);
    void EngageQuestTargets(BotAI* ai, ObjectiveState const& objective);
    void CollectQuestItems(BotAI* ai, ObjectiveState const& objective);
    void ExploreQuestArea(BotAI* ai, ObjectiveState const& objective);
    void TurnInQuest(BotAI* ai, uint32 questId);

    // Quest item usage system (for quests that require using items on targets)
    void UseQuestItemOnTarget(BotAI* ai, ObjectiveState const& objective);

    // ========================================================================
    // QUEST OBJECTIVE TYPE HANDLERS (complete coverage of all 22 objective types)
    // ========================================================================

    // TALKTO objectives - interact with NPC via gossip/dialog
    void TalkToNpc(BotAI* ai, ObjectiveState const& objective);

    // Currency objectives - check if bot has required currency
    void HandleCurrencyObjective(BotAI* ai, ObjectiveState const& objective);

    // Money objectives - check if bot has required gold/silver/copper
    void HandleMoneyObjective(BotAI* ai, ObjectiveState const& objective);

    // Objective analysis
    ObjectivePriority GetCurrentObjective(BotAI* ai) const;
    bool HasActiveObjectives(BotAI* ai) const;
    bool ShouldEngageTarget(BotAI* ai, ::Unit* target, ObjectiveState const& objective) const;

    // Movement helpers
    bool MoveToObjectiveLocation(BotAI* ai, Position const& location);
    bool MoveToQuestGiver(BotAI* ai, uint32 questId);
    Position GetObjectivePosition(BotAI* ai, ObjectiveState const& objective) const;

    // Target detection
    ::Unit* FindQuestTarget(BotAI* ai, ObjectiveState const& objective) const;
    GameObject* FindQuestObject(BotAI* ai, ObjectiveState const& objective) const;
    ::Item* FindQuestItem(BotAI* ai, ObjectiveState const& objective) const;

    // Quest turn-in system (multi-tier fallback with creature AND gameobject support)
    struct QuestEnderLocation
    {
        uint32 objectEntry = 0;         // Creature OR GameObject entry ID
        Position position;
        bool isGameObject = false;      // True if quest ender is a GameObject, false if Creature
        bool foundViaSpawn = false;     // True if found via spawn data (creature or gameobject)
        bool foundViaPOI = false;       // True if found via Quest POI
        bool requiresSearch = false;    // True if needs area search

        // Helper methods for clarity
        bool IsCreature() const { return !isGameObject && objectEntry != 0; }
        bool IsGameObject() const { return isGameObject && objectEntry != 0; }
        bool IsValid() const { return objectEntry != 0; }
    };

    bool FindQuestEnderLocation(BotAI* ai, uint32 questId, QuestEnderLocation& location);
    bool NavigateToQuestEnder(BotAI* ai, QuestEnderLocation const& location);
    bool CheckForQuestEnderInRange(BotAI* ai, QuestEnderLocation const& location);
    bool CheckForCreatureQuestEnderInRange(BotAI* ai, uint32 creatureEntry);
    bool CheckForGameObjectQuestEnderInRange(BotAI* ai, uint32 gameobjectEntry);
    bool CompleteQuestTurnIn(BotAI* ai, uint32 questId, ::Unit* questEnder);
    bool CompleteQuestTurnInAtGameObject(BotAI* ai, uint32 questId, GameObject* questEnder);

    // State management
    QuestPhase _currentPhase;
    uint32 _phaseTimer;
    uint32 _lastObjectiveUpdate;
    ObjectGuid _currentTargetGuid;
    uint32 _currentQuestId;
    uint32 _currentObjectiveIndex;
    Position _currentObjectivePosition;

    // Quest giver search throttling (prevent log spam)
    uint32 _lastQuestGiverSearchTime;
    uint32 _questGiverSearchFailures;

    // Quest area wandering (for respawn waiting)
    uint32 _lastWanderTime;
    uint32 _currentWanderPointIndex;
    ::std::vector<Position> _questAreaWanderPoints;

    // Performance tracking
    uint32 _objectivesCompleted;
    uint32 _questsCompleted;
    uint32 _averageObjectiveTime;

    // Quest acceptance manager (enterprise-grade auto-acceptance)
    ::std::unique_ptr<QuestAcceptanceManager> _acceptanceManager;

    // Helper methods for area wandering
    bool ShouldWanderInQuestArea(BotAI* ai, ObjectiveState const& objective) const;
    void InitializeQuestAreaWandering(BotAI* ai, ObjectiveState const& objective);
    void WanderInQuestArea(BotAI* ai);

    // Item loot source detection
    bool IsItemFromCreatureLoot(uint32 itemId) const;

    // NPC interaction detection (distinguish "talk to" NPCs from "killable neutral" mobs)
    bool RequiresSpellClickInteraction(uint32 creatureEntry) const;

    // Quest item usage tracking (prevent recasting on same/despawned targets)
    // Key: questId, Value: set of used target GUIDs
    ::std::unordered_map<uint32, ::std::set<ObjectGuid>> _usedQuestItemTargets;
    uint32 _lastQuestItemCastTime = 0;
    static constexpr uint32 QUEST_ITEM_CAST_COOLDOWN_MS = 5000; // 5 second cooldown between casts
};

} // namespace Playerbot
