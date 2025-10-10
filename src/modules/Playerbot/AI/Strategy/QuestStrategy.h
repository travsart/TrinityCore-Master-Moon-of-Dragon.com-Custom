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
#include "Quest/ObjectiveTracker.h"
#include "Movement/BotMovementUtil.h"
#include "Player.h"
#include "QuestDef.h"
#include "../../Game/QuestAcceptanceManager.h"

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
    void NavigateToObjective(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective);
    void EngageQuestTargets(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective);
    void CollectQuestItems(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective);
    void ExploreQuestArea(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective);
    void TurnInQuest(BotAI* ai, uint32 questId);

    // Objective analysis
    ObjectiveTracker::ObjectivePriority GetCurrentObjective(BotAI* ai) const;
    bool HasActiveObjectives(BotAI* ai) const;
    bool ShouldEngageTarget(BotAI* ai, ::Unit* target, ObjectiveTracker::ObjectiveState const& objective) const;

    // Movement helpers
    bool MoveToObjectiveLocation(BotAI* ai, Position const& location);
    bool MoveToQuestGiver(BotAI* ai, uint32 questId);
    Position GetObjectivePosition(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const;

    // Target detection
    ::Unit* FindQuestTarget(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const;
    GameObject* FindQuestObject(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const;
    ::Item* FindQuestItem(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const;

    // Quest turn-in system (multi-tier fallback)
    struct QuestEnderLocation
    {
        uint32 npcEntry = 0;
        Position position;
        bool foundViaSpawn = false;     // True if found via creature spawn data
        bool foundViaPOI = false;       // True if found via Quest POI
        bool requiresSearch = false;    // True if needs area search
    };

    bool FindQuestEnderLocation(BotAI* ai, uint32 questId, QuestEnderLocation& location);
    bool NavigateToQuestEnder(BotAI* ai, QuestEnderLocation const& location);
    bool CheckForQuestEnderInRange(BotAI* ai, uint32 npcEntry);
    bool CompleteQuestTurnIn(BotAI* ai, uint32 questId, ::Unit* questEnder);

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

    // Performance tracking
    uint32 _objectivesCompleted;
    uint32 _questsCompleted;
    uint32 _averageObjectiveTime;

    // Quest acceptance manager (enterprise-grade auto-acceptance)
    std::unique_ptr<QuestAcceptanceManager> _acceptanceManager;
};

} // namespace Playerbot
