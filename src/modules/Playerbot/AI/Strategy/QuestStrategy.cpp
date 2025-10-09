/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "QuestStrategy.h"
#include "../BotAI.h"
#include "Player.h"
#include "Group.h"
#include "QuestDef.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include "ObjectMgr.h"

namespace Playerbot
{

QuestStrategy::QuestStrategy()
    : Strategy("quest")
    , _currentPhase(QuestPhase::IDLE)
    , _phaseTimer(0)
    , _lastObjectiveUpdate(0)
    , _currentQuestId(0)
    , _currentObjectiveIndex(0)
    , _objectivesCompleted(0)
    , _questsCompleted(0)
    , _averageObjectiveTime(0)
{
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: Initialized");
}

void QuestStrategy::InitializeActions()
{
    // No actions needed - quest strategy drives movement directly
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No actions (direct movement control)");
}

void QuestStrategy::InitializeTriggers()
{
    // No triggers needed - relevance system handles activation
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No triggers (using relevance system)");
}

void QuestStrategy::InitializeValues()
{
    // No values needed for this simple strategy
    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No values");
}

bool QuestStrategy::IsActive(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Active if bot has active quests and is NOT in combat
    // Combat takes priority (handled by GroupCombatStrategy)
    if (bot->IsInCombat())
        return false;

    // Check if bot has active quests
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SLOT; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId != 0)
            return true; // Has at least one active quest
    }

    return false;
}

float QuestStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // Combat has higher priority - return 0 if in combat
    if (bot->IsInCombat())
        return 0.0f;

    // Check if we have quest objectives to complete
    if (HasActiveObjectives(ai))
        return 70.0f; // High relevance, but lower than combat (80.0f)

    return 0.0f;
}

void QuestStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Don't interrupt combat
    if (bot->IsInCombat())
        return;

    // Update objective tracker periodically
    uint32 currentTime = getMSTime();
    if (currentTime - _lastObjectiveUpdate > 2000) // Every 2 seconds
    {
        ObjectiveTracker::instance()->UpdateBotTracking(bot, diff);
        _lastObjectiveUpdate = currentTime;
    }

    // Process quest objectives
    ProcessQuestObjectives(ai);
}

void QuestStrategy::ProcessQuestObjectives(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Get highest priority objective from ObjectiveTracker
    ObjectiveTracker::ObjectivePriority priority = ObjectiveTracker::instance()->GetHighestPriorityObjective(bot);

    if (priority.questId == 0)
    {
        // No objectives - return to idle
        _currentPhase = QuestPhase::IDLE;
        return;
    }

    // Get objective state
    ObjectiveTracker::ObjectiveState objective = ObjectiveTracker::instance()->GetObjectiveState(
        bot, priority.questId, priority.objectiveIndex);

    // Cache current objective info
    _currentQuestId = objective.questId;
    _currentObjectiveIndex = objective.objectiveIndex;

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest)
        return;

    // Check if quest is complete - turn it in
    if (bot->GetQuestStatus(objective.questId) == QUEST_STATUS_COMPLETE)
    {
        TurnInQuest(ai, objective.questId);
        return;
    }

    // Determine objective type and execute appropriate behavior
    QuestObjective const* questObjective = quest->Objectives.size() > objective.objectiveIndex
        ? &quest->Objectives[objective.objectiveIndex]
        : nullptr;

    if (!questObjective)
        return;

    // Route to appropriate handler based on objective type
    switch (questObjective->Type)
    {
        case QUEST_OBJECTIVE_MONSTER:
        case QUEST_OBJECTIVE_PLAYER_KILLS:
            EngageQuestTargets(ai, objective);
            break;

        case QUEST_OBJECTIVE_ITEM:
        case QUEST_OBJECTIVE_GAMEOBJECT:
            CollectQuestItems(ai, objective);
            break;

        case QUEST_OBJECTIVE_AREATRIGGER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
            ExploreQuestArea(ai, objective);
            break;

        default:
            // Unknown objective type - try to navigate to objective location
            NavigateToObjective(ai, objective);
            break;
    }
}

void QuestStrategy::NavigateToObjective(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Get objective position from tracker
    Position objectivePos = GetObjectivePosition(ai, objective);

    if (objectivePos.GetExactDist2d(0.0f, 0.0f) < 0.1f)
    {
        TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: No valid position for objective {} of quest {}",
                     objective.objectiveIndex, objective.questId);
        return;
    }

    // Move to objective location
    MoveToObjectiveLocation(ai, objectivePos);
}

void QuestStrategy::EngageQuestTargets(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Find quest target near bot
    ::Unit* target = FindQuestTarget(ai, objective);

    if (!target)
    {
        // No target found - navigate to objective area
        NavigateToObjective(ai, objective);
        return;
    }

    // Check if we should engage this target
    if (!ShouldEngageTarget(ai, target, objective))
        return;

    // Set as combat target - GroupCombatStrategy will handle combat
    bot->SetTarget(target->GetGUID());

    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: Bot {} targeting quest mob {} for quest {}",
                 bot->GetName(), target->GetName(), objective.questId);
}

void QuestStrategy::CollectQuestItems(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Check if bot already has required items
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    // Check item count
    uint32 itemCount = bot->GetItemCount(questObjective.ObjectID, false);
    if (itemCount >= questObjective.Amount)
    {
        // Objective complete
        TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: Bot {} completed item objective {} for quest {}",
                     bot->GetName(), objective.objectiveIndex, objective.questId);
        return;
    }

    // Find quest object to interact with
    GameObject* questObject = FindQuestObject(ai, objective);

    if (!questObject)
    {
        // No object found - navigate to objective area
        NavigateToObjective(ai, objective);
        return;
    }

    // Move to object
    float distance = bot->GetDistance(questObject);
    if (distance > INTERACTION_DISTANCE)
    {
        Position objPos;
        objPos.Relocate(questObject->GetPositionX(), questObject->GetPositionY(), questObject->GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, objPos);
        return;
    }

    // Interact with object
    bot->PrepareGossipMenu(questObject, questObject->GetGoInfo()->type == GAMEOBJECT_TYPE_QUESTGIVER ? 0 : questObject->GetGOInfo()->entry);
    bot->SendPreparedGossip(questObject);

    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: Bot {} interacting with object {} for quest {}",
                 bot->GetName(), questObject->GetEntry(), objective.questId);
}

void QuestStrategy::ExploreQuestArea(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective)
{
    if (!ai || !ai->GetBot())
        return;

    // Simply navigate to the objective area
    NavigateToObjective(ai, objective);
}

void QuestStrategy::TurnInQuest(BotAI* ai, uint32 questId)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return;

    // QuestManager will handle the actual turn-in logic
    // Just move to quest giver
    MoveToQuestGiver(ai, questId);

    TC_LOG_DEBUG("module.playerbot.strategy", "QuestStrategy: Bot {} moving to turn in quest {}",
                 bot->GetName(), questId);
}

ObjectiveTracker::ObjectivePriority QuestStrategy::GetCurrentObjective(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return ObjectiveTracker::ObjectivePriority(0, 0);

    Player* bot = ai->GetBot();
    return ObjectiveTracker::instance()->GetHighestPriorityObjective(bot);
}

bool QuestStrategy::HasActiveObjectives(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    ObjectiveTracker::ObjectivePriority priority = GetCurrentObjective(ai);
    return priority.questId != 0;
}

bool QuestStrategy::ShouldEngageTarget(BotAI* ai, ::Unit* target, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot() || !target)
        return false;

    Player* bot = ai->GetBot();

    // Check if target is quest target
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return false;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_MONSTER)
        return false;

    if (target->GetEntry() != questObjective.ObjectID)
        return false;

    // Check if already at max kills
    uint32 currentKills = bot->GetQuestObjectiveData(objective.questId, questObjective.StorageIndex);
    if (currentKills >= static_cast<uint32>(questObjective.Amount))
        return false;

    return true;
}

bool QuestStrategy::MoveToObjectiveLocation(BotAI* ai, Position const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Check if already at location
    float distance = bot->GetExactDist2d(location.GetPositionX(), location.GetPositionY());
    if (distance < 10.0f) // Within 10 yards
        return true;

    // Use centralized movement utility
    return BotMovementUtil::MoveToPosition(bot, location);
}

bool QuestStrategy::MoveToQuestGiver(BotAI* ai, uint32 questId)
{
    if (!ai || !ai->GetBot())
        return false;

    // QuestManager has the quest giver finding logic
    // For now, just return false - QuestManager will handle turn-in
    return false;
}

Position QuestStrategy::GetObjectivePosition(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return Position();

    // Return cached position from objective state
    return objective.lastKnownPosition;
}

::Unit* QuestStrategy::FindQuestTarget(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_MONSTER)
        return nullptr;

    // Scan for creature with this entry
    std::vector<uint32> targets = ObjectiveTracker::instance()->ScanForKillTargets(bot, questObjective.ObjectID, 50.0f);

    if (targets.empty())
        return nullptr;

    // Return first target (ObjectAccessor will validate)
    return ObjectAccessor::GetUnit(*bot, ObjectGuid::Create<HighGuid::Creature>(0, questObjective.ObjectID, targets[0]));
}

GameObject* QuestStrategy::FindQuestObject(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_GAMEOBJECT)
        return nullptr;

    // Scan for game object with this entry
    std::vector<uint32> objects = ObjectiveTracker::instance()->ScanForGameObjects(bot, questObjective.ObjectID, 50.0f);

    if (objects.empty())
        return nullptr;

    // Return first object
    return ObjectAccessor::GetGameObject(*bot, ObjectGuid::Create<HighGuid::GameObject>(0, questObjective.ObjectID, objects[0]));
}

::Item* QuestStrategy::FindQuestItem(BotAI* ai, ObjectiveTracker::ObjectiveState const& objective) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
        return nullptr;

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    if (questObjective.Type != QUEST_OBJECTIVE_ITEM)
        return nullptr;

    // Check if bot has item in inventory
    return bot->GetItemByEntry(questObjective.ObjectID);
}

} // namespace Playerbot
