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
#include "World.h"
#include "Creature.h"
#include "../../Game/QuestAcceptanceManager.h"

namespace Playerbot
{

QuestStrategy::QuestStrategy()
    : Strategy("quest")
    , _currentPhase(QuestPhase::IDLE)
    , _phaseTimer(0)
    , _lastObjectiveUpdate(0)
    , _currentQuestId(0)
    , _currentObjectiveIndex(0)
    , _lastQuestGiverSearchTime(0)
    , _questGiverSearchFailures(0)
    , _objectivesCompleted(0)
    , _questsCompleted(0)
    , _averageObjectiveTime(0)
    , _acceptanceManager(nullptr)
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
    bool activeFlag = _active.load();
    TC_LOG_ERROR("module.playerbot.quest", "🔍 QuestStrategy::IsActive ENTRY: ai={}, _active={}",
                 (void*)ai, activeFlag);

    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ QuestStrategy::IsActive: NULL check failed, returning false");
        return false;
    }

    Player* bot = ai->GetBot();

    // NOT active during combat (combat takes priority)
    if (bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚔️ QuestStrategy::IsActive: Bot {} in combat, returning false", bot->GetName());
        return false;
    }

    // Active for ALL levels - questing is always valuable
    // - Below max level: Quest for XP (high priority)
    // - At max level: Quest for gold, reputation, achievements (lower priority)
    TC_LOG_ERROR("module.playerbot.quest", "📋 QuestStrategy::IsActive: Bot {} - _active={}, returning {}",
                 bot->GetName(), activeFlag, activeFlag);
    return activeFlag;
}

float QuestStrategy::GetRelevance(BotAI* ai) const
{
    // CRITICAL DEBUG: Force ERROR level logging to see this!
    TC_LOG_ERROR("module.playerbot.quest", "🔍 QuestStrategy::GetRelevance ENTRY: ai={}, ai->GetBot()={}",
                 (void*)ai, ai ? (void*)ai->GetBot() : nullptr);

    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ QuestStrategy::GetRelevance: NULL check failed, returning 0.0f");
        return 0.0f;
    }

    Player* bot = ai->GetBot();

    // Combat has higher priority - return 0 if in combat
    if (bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚔️ QuestStrategy::GetRelevance: Bot {} in combat, returning 0.0f", bot->GetName());
        return 0.0f;
    }

    bool isMaxLevel = (bot->GetLevel() >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    bool hasObjectives = HasActiveObjectives(ai);

    TC_LOG_ERROR("module.playerbot.quest", "📊 QuestStrategy::GetRelevance: Bot {} - Level={}, MaxLevel={}, hasObjectives={}",
                 bot->GetName(), bot->GetLevel(), isMaxLevel, hasObjectives);

    // HIGH PRIORITY: Has active quest objectives to complete
    if (hasObjectives)
    {
        // Pre-max level: Very high priority (quest for XP)
        // Max level: Medium-high priority (quest for gold/rep)
        float relevance = isMaxLevel ? 60.0f : 70.0f;
        TC_LOG_ERROR("module.playerbot.quest", "✅ QuestStrategy::GetRelevance: Bot {} has objectives, returning {:.1f}",
                     bot->GetName(), relevance);
        return relevance;
    }

    // MEDIUM/LOW PRIORITY: No active quests - search for quest givers
    float relevance;
    if (isMaxLevel)
    {
        // Max level: Lower priority (quests are optional for gold/rep)
        relevance = 30.0f; // Lower than loot=60, solo=10-50
    }
    else
    {
        // Pre-max level: Medium-high priority (actively seek quests to level up)
        relevance = 50.0f; // Higher than solo, actively search for quests
    }

    TC_LOG_ERROR("module.playerbot.quest", "📍 QuestStrategy::GetRelevance: Bot {} (Level {}, MaxLevel={}) no objectives, returning {:.1f}",
                 bot->GetName(), bot->GetLevel(), isMaxLevel, relevance);
    return relevance;
}

void QuestStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ UpdateBehavior: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    TC_LOG_ERROR("module.playerbot.quest", "🚀 UpdateBehavior: Bot {} starting quest behavior update", bot->GetName());

    // Don't interrupt combat
    if (bot->IsInCombat())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚔️ UpdateBehavior: Bot {} in combat, skipping", bot->GetName());
        return;
    }

    // Update objective tracker periodically
    uint32 currentTime = getMSTime();
    if (currentTime - _lastObjectiveUpdate > 2000) // Every 2 seconds
    {
        ObjectiveTracker::instance()->UpdateBotTracking(bot, diff);
        _lastObjectiveUpdate = currentTime;
    }

    // Check if bot has active quests
    bool hasActiveQuests = false;
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId != 0)
        {
            hasActiveQuests = true;
            TC_LOG_ERROR("module.playerbot.quest", "📋 UpdateBehavior: Bot {} has active quest {} in slot {}",
                         bot->GetName(), questId, slot);
            break;
        }
    }

    TC_LOG_ERROR("module.playerbot.quest", "📊 UpdateBehavior: Bot {} hasActiveQuests={}",
                 bot->GetName(), hasActiveQuests);

    if (hasActiveQuests)
    {
        TC_LOG_ERROR("module.playerbot.quest", "🎯 UpdateBehavior: Bot {} processing quest objectives", bot->GetName());
        // Process quest objectives
        ProcessQuestObjectives(ai);
    }
    else
    {
        TC_LOG_ERROR("module.playerbot.quest", "🔍 UpdateBehavior: Bot {} has NO active quests - calling SearchForQuestGivers", bot->GetName());
        // No active quests - search for quest givers to accept new quests
        SearchForQuestGivers(ai);
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ UpdateBehavior: Bot {} quest behavior update complete", bot->GetName());
}

void QuestStrategy::ProcessQuestObjectives(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    TC_LOG_ERROR("module.playerbot.quest", "📍 ProcessQuestObjectives: Bot {} starting objective processing", bot->GetName());

    // Get highest priority objective from ObjectiveTracker
    ObjectiveTracker::ObjectivePriority priority = ObjectiveTracker::instance()->GetHighestPriorityObjective(bot);

    TC_LOG_ERROR("module.playerbot.quest", "🎯 ProcessQuestObjectives: Bot {} - priority.questId={}, priority.objectiveIndex={}",
                 bot->GetName(), priority.questId, priority.objectiveIndex);

    if (priority.questId == 0)
    {
        // ObjectiveTracker doesn't know about bot's quests - initialize it
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} ObjectiveTracker returned questId=0, initializing from quest log",
                     bot->GetName());

        // Scan quest log and register all active quests with ObjectiveTracker
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (questId == 0)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            TC_LOG_ERROR("module.playerbot.quest", "🔄 ProcessQuestObjectives: Bot {} registering quest {} with ObjectiveTracker (Quest has {} objectives)",
                         bot->GetName(), questId, quest->Objectives.size());

            // Skip quests with no objectives (autocomplete/scripted quests)
            if (quest->Objectives.empty())
            {
                TC_LOG_ERROR("module.playerbot.quest", "⚠️ Quest {} has NO objectives - skipping registration (likely autocomplete/scripted quest)",
                            questId);
                continue;
            }

            // Register each objective of this quest
            for (uint32 i = 0; i < quest->Objectives.size(); ++i)
            {
                QuestObjective const& objective = quest->Objectives[i];

                // Map TrinityCore objective type to our QuestObjectiveType enum
                QuestObjectiveType objType = static_cast<QuestObjectiveType>(objective.Type);

                // Create objective data using constructor (questId, index, type, targetId, requiredCount)
                QuestObjectiveData objData(questId, i, objType, objective.ObjectID, objective.Amount);

                ObjectiveTracker::instance()->StartTrackingObjective(bot, objData);
            }
        }

        // Try again after initialization
        priority = ObjectiveTracker::instance()->GetHighestPriorityObjective(bot);
        TC_LOG_ERROR("module.playerbot.quest", "🔄 ProcessQuestObjectives: Bot {} after initialization - priority.questId={}, priority.objectiveIndex={}",
                     bot->GetName(), priority.questId, priority.objectiveIndex);

        if (priority.questId == 0)
        {
            // Still no objectives - bot has only autocomplete/scripted quests with no trackable objectives
            // Fall back to searching for new quests instead of idling
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ ProcessQuestObjectives: Bot {} has NO trackable objectives (only autocomplete/scripted quests), searching for new quests",
                         bot->GetName());

            // Search for quest givers to get quests with actual objectives
            SearchForQuestGivers(ai);
            return;
        }
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
    TC_LOG_ERROR("module.playerbot.quest", "🔀 ProcessQuestObjectives: Bot {} - Routing objective type {} for quest {}",
                 bot->GetName(), questObjective->Type, objective.questId);

    switch (questObjective->Type)
    {
        case QUEST_OBJECTIVE_MONSTER:
        case QUEST_OBJECTIVE_PLAYERKILLS:
            TC_LOG_ERROR("module.playerbot.quest", "⚔️ ProcessQuestObjectives: Bot {} - Calling EngageQuestTargets for quest {}",
                         bot->GetName(), objective.questId);
            EngageQuestTargets(ai, objective);
            break;

        case QUEST_OBJECTIVE_ITEM:
        case QUEST_OBJECTIVE_GAMEOBJECT:
            TC_LOG_ERROR("module.playerbot.quest", "📦 ProcessQuestObjectives: Bot {} - Calling CollectQuestItems for quest {}",
                         bot->GetName(), objective.questId);
            CollectQuestItems(ai, objective);
            break;

        case QUEST_OBJECTIVE_AREATRIGGER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
        case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
            TC_LOG_ERROR("module.playerbot.quest", "🗺️ ProcessQuestObjectives: Bot {} - Calling ExploreQuestArea for quest {}",
                         bot->GetName(), objective.questId);
            ExploreQuestArea(ai, objective);
            break;

        default:
            TC_LOG_ERROR("module.playerbot.quest", "❓ ProcessQuestObjectives: Bot {} - Unknown objective type {}, calling NavigateToObjective for quest {}",
                         bot->GetName(), questObjective->Type, objective.questId);
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
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CollectQuestItems: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    TC_LOG_ERROR("module.playerbot.quest", "📦 CollectQuestItems: Bot {} starting item collection for quest {} objective {}",
                 bot->GetName(), objective.questId, objective.objectiveIndex);

    // Check if bot already has required items
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest || objective.objectiveIndex >= quest->Objectives.size())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CollectQuestItems: Bot {} - Invalid quest {} or objective index {}",
                     bot->GetName(), objective.questId, objective.objectiveIndex);
        return;
    }

    QuestObjective const& questObjective = quest->Objectives[objective.objectiveIndex];

    TC_LOG_ERROR("module.playerbot.quest", "🔍 CollectQuestItems: Bot {} - Quest {} requires item {} (Amount: {})",
                 bot->GetName(), objective.questId, questObjective.ObjectID, questObjective.Amount);

    // Check item count
    uint32 itemCount = bot->GetItemCount(questObjective.ObjectID, false);
    TC_LOG_ERROR("module.playerbot.quest", "📊 CollectQuestItems: Bot {} currently has {} / {} of item {}",
                 bot->GetName(), itemCount, questObjective.Amount, questObjective.ObjectID);

    if (itemCount >= questObjective.Amount)
    {
        // Objective complete
        TC_LOG_ERROR("module.playerbot.quest", "✅ CollectQuestItems: Bot {} completed item objective {} for quest {} ({} / {})",
                     bot->GetName(), objective.objectiveIndex, objective.questId, itemCount, questObjective.Amount);
        return;
    }

    // Find quest object to interact with
    TC_LOG_ERROR("module.playerbot.quest", "🔎 CollectQuestItems: Bot {} searching for quest object...",
                 bot->GetName());
    GameObject* questObject = FindQuestObject(ai, objective);

    if (!questObject)
    {
        // No object found - navigate to objective area
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ CollectQuestItems: Bot {} - NO quest object found, navigating to objective area",
                     bot->GetName());
        NavigateToObjective(ai, objective);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ CollectQuestItems: Bot {} found quest object {} at distance {:.1f}",
                 bot->GetName(), questObject->GetEntry(), bot->GetDistance(questObject));

    // Move to object
    float distance = bot->GetDistance(questObject);
    if (distance > INTERACTION_DISTANCE)
    {
        Position objPos;
        objPos.Relocate(questObject->GetPositionX(), questObject->GetPositionY(), questObject->GetPositionZ());
        TC_LOG_ERROR("module.playerbot.quest", "🚶 CollectQuestItems: Bot {} moving to object {} (distance: {:.1f}, pos: {:.1f}, {:.1f}, {:.1f})",
                     bot->GetName(), questObject->GetEntry(), distance,
                     objPos.GetPositionX(), objPos.GetPositionY(), objPos.GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, objPos);
        return;
    }

    // Interact with object
    TC_LOG_ERROR("module.playerbot.quest", "🎯 CollectQuestItems: Bot {} interacting with object {} (distance: {:.1f} <= INTERACTION_DISTANCE)",
                 bot->GetName(), questObject->GetEntry(), distance);
    bot->PrepareGossipMenu(questObject, questObject->GetGOInfo()->type == GAMEOBJECT_TYPE_QUESTGIVER ? 0 : questObject->GetGOInfo()->entry);
    bot->SendPreparedGossip(questObject);

    TC_LOG_ERROR("module.playerbot.quest", "✅ CollectQuestItems: Bot {} interaction sent for object {}",
                 bot->GetName(), questObject->GetEntry());
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
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ TurnInQuest: Invalid quest ID {}", questId);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "🎯 TurnInQuest: Bot {} attempting to turn in quest {} ({})",
                 bot->GetName(), questId, quest->GetLogTitle());

    // Step 1: Find quest ender location (multi-tier fallback)
    QuestEnderLocation location;
    if (!FindQuestEnderLocation(ai, questId, location))
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to find quest ender location for quest {}",
                     bot->GetName(), questId);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ TurnInQuest: Bot {} found quest ender NPC {} at ({:.1f}, {:.1f}, {:.1f}) - foundViaSpawn={}, foundViaPOI={}, requiresSearch={}",
                 bot->GetName(), location.npcEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 location.foundViaSpawn, location.foundViaPOI, location.requiresSearch);

    // Step 2: Check if NPC is already in range
    if (CheckForQuestEnderInRange(ai, location.npcEntry))
    {
        // NPC is in range - complete turn-in immediately
        TC_LOG_ERROR("module.playerbot.quest", "✅ TurnInQuest: Bot {} found quest ender NPC {} in range, completing turn-in immediately",
                     bot->GetName(), location.npcEntry);
        return; // CheckForQuestEnderInRange() handles the turn-in
    }

    // Step 3: Navigate to quest ender location
    if (!NavigateToQuestEnder(ai, location))
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ TurnInQuest: Bot {} failed to navigate to quest ender NPC {}",
                     bot->GetName(), location.npcEntry);
        return;
    }

    TC_LOG_ERROR("module.playerbot.quest", "🚶 TurnInQuest: Bot {} navigating to quest ender NPC {} at ({:.1f}, {:.1f}, {:.1f})",
                 bot->GetName(), location.npcEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());

    // Navigation is in progress - next UpdateBehavior() cycle will check for NPC in range
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

void QuestStrategy::SearchForQuestGivers(BotAI* ai)
{
    if (!ai || !ai->GetBot())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ SearchForQuestGivers: NULL ai or bot");
        return;
    }

    Player* bot = ai->GetBot();

    TC_LOG_ERROR("module.playerbot.quest", "🔍 SearchForQuestGivers: ENTRY for bot {}", bot->GetName());

    // Initialize QuestAcceptanceManager if not already done
    if (!_acceptanceManager)
    {
        _acceptanceManager = std::make_unique<QuestAcceptanceManager>(bot);
        TC_LOG_ERROR("module.playerbot.quest",
            "🎬 SearchForQuestGivers: Initialized QuestAcceptanceManager for bot {}",
            bot->GetName());
    }

    // THROTTLING: Prevent log spam from repeated failed searches
    uint32 currentTime = getMSTime();

    // Calculate backoff delay based on failure count (exponential backoff)
    // 0 failures: 0ms delay
    // 1 failure: 5 seconds
    // 2 failures: 10 seconds
    // 3 failures: 20 seconds
    // 4+ failures: 30 seconds
    uint32 backoffDelay = 0;
    if (_questGiverSearchFailures > 0)
    {
        backoffDelay = std::min(30000u, 5000u * (1u << (_questGiverSearchFailures - 1)));
    }

    TC_LOG_ERROR("module.playerbot.quest", "⏰ SearchForQuestGivers: Bot {} - failures={}, backoffDelay={}ms, timeSinceLastSearch={}ms",
                 bot->GetName(), _questGiverSearchFailures, backoffDelay, currentTime - _lastQuestGiverSearchTime);

    // Check if we're still in cooldown period
    if (currentTime - _lastQuestGiverSearchTime < backoffDelay)
    {
        TC_LOG_ERROR("module.playerbot.quest", "⏸️ SearchForQuestGivers: Bot {} still in cooldown ({} ms remaining), skipping search",
                     bot->GetName(), backoffDelay - (currentTime - _lastQuestGiverSearchTime));
        // Still in cooldown - don't search yet (prevents log spam)
        return;
    }

    // Update last search time
    _lastQuestGiverSearchTime = currentTime;

    TC_LOG_ERROR("module.playerbot.quest", "🔎 SearchForQuestGivers: Bot {} starting quest giver scan (50 yard radius)",
                 bot->GetName());

    TC_LOG_DEBUG("module.playerbot.strategy",
        "QuestStrategy: Bot {} (Level {}) searching for quest givers (no active quests)",
        bot->GetName(), bot->GetLevel());

    // Search for nearby creatures that might offer quests
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, 0, 50.0f); // 50 yard radius

    TC_LOG_ERROR("module.playerbot.quest", "🔬 SearchForQuestGivers: Bot {} found {} nearby creatures",
                 bot->GetName(), nearbyCreatures.size());

    Creature* closestQuestGiver = nullptr;
    float closestDistance = 999999.0f;
    uint32 questGiverCount = 0;
    uint32 questGiversWithEligibleQuests = 0;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        // CRITICAL: Check if bot can actually see this creature (phase check)
        if (!bot->CanSeeOrDetect(creature))
        {
            TC_LOG_ERROR("module.playerbot.quest", "👻 Creature {} (Entry: {}) is in different phase, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        TC_LOG_ERROR("module.playerbot.quest", "🔬 Checking creature: {} (Entry: {}), IsQuestGiver={}",
                     creature->GetName(), creature->GetEntry(), creature->IsQuestGiver());

        // Check if creature is a quest giver
        if (!creature->IsQuestGiver())
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ Creature {} (Entry: {}) is NOT a quest giver, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        questGiverCount++;

        // CRITICAL FIX: Check if this quest giver has any eligible quests for the bot
        // Don't waste time moving to NPCs that have no quests we can accept!
        QuestRelationResult objectQR = sObjectMgr->GetCreatureQuestRelations(creature->GetEntry());
        bool hasEligibleQuests = false;

        for (uint32 questId : objectQR)
        {
            Quest const* questTemplate = sObjectMgr->GetQuestTemplate(questId);
            if (!questTemplate)
                continue;

            // Check if quest is eligible using QuestAcceptanceManager
            if (_acceptanceManager->IsQuestEligible(questTemplate))
            {
                hasEligibleQuests = true;
                TC_LOG_ERROR("module.playerbot.quest", "✅ Quest giver {} (Entry: {}) has eligible quest {} ({})",
                             creature->GetName(), creature->GetEntry(), questId, questTemplate->GetLogTitle());
                break; // Found at least one eligible quest, this NPC is valid
            }
        }

        if (!hasEligibleQuests)
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ Quest giver {} (Entry: {}) has NO eligible quests for bot {}, skipping",
                         creature->GetName(), creature->GetEntry(), bot->GetName());
            continue;
        }

        questGiversWithEligibleQuests++;

        // Find the closest quest giver WITH ELIGIBLE QUESTS
        float distance = bot->GetDistance(creature);
        TC_LOG_ERROR("module.playerbot.quest", "✅ Found quest giver WITH ELIGIBLE QUESTS: {} (Entry: {}) at distance {:.1f}",
                     creature->GetName(), creature->GetEntry(), distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestQuestGiver = creature;
        }
    }

    TC_LOG_ERROR("module.playerbot.quest", "📊 SearchForQuestGivers: Bot {} found {} quest givers ({} with eligible quests) - closest with eligible quests: {} at {:.1f} yards",
                 bot->GetName(), questGiverCount, questGiversWithEligibleQuests,
                 closestQuestGiver ? closestQuestGiver->GetName() : "NONE",
                 closestQuestGiver ? closestDistance : 0.0f);

    if (!closestQuestGiver)
    {
        // Increment failure counter for exponential backoff
        _questGiverSearchFailures++;

        TC_LOG_ERROR("module.playerbot.quest",
            "❌ SearchForQuestGivers: Bot {} found no quest givers within 50 yards (failures: {}, next search in {}s)",
            bot->GetName(), _questGiverSearchFailures,
            std::min(30u, 5u * (1u << (_questGiverSearchFailures - 1))));

        // TODO: Implement pathfinding to known quest hubs based on bot level
        // For now, bot will just idle until a quest giver is nearby
        return;
    }

    // SUCCESS: Found a quest giver - reset failure counter
    _questGiverSearchFailures = 0;

    TC_LOG_ERROR("module.playerbot.quest",
        "✅ SearchForQuestGivers: Bot {} found quest giver {} at distance {:.1f}",
        bot->GetName(), closestQuestGiver->GetName(), closestDistance);

    // Move to quest giver
    if (closestDistance > INTERACTION_DISTANCE)
    {
        Position questGiverPos;
        questGiverPos.Relocate(closestQuestGiver->GetPositionX(),
                              closestQuestGiver->GetPositionY(),
                              closestQuestGiver->GetPositionZ());

        TC_LOG_ERROR("module.playerbot.quest",
            "🚶 SearchForQuestGivers: Bot {} moving to quest giver {} (distance: {:.1f}, pos: {:.1f}, {:.1f}, {:.1f})",
            bot->GetName(), closestQuestGiver->GetName(), closestDistance,
            questGiverPos.GetPositionX(), questGiverPos.GetPositionY(), questGiverPos.GetPositionZ());

        bool moveResult = BotMovementUtil::MoveToPosition(bot, questGiverPos);

        TC_LOG_ERROR("module.playerbot.quest",
            "🚶 SearchForQuestGivers: Bot {} MoveToPosition result: {}",
            bot->GetName(), moveResult ? "SUCCESS" : "FAILED");
        return;
    }

    // Bot is close enough - use ENTERPRISE-GRADE quest acceptance system
    TC_LOG_ERROR("module.playerbot.quest",
        "🎯 SearchForQuestGivers: Bot {} at quest giver {} (distance {:.1f} <= INTERACTION_DISTANCE) - Processing with QuestAcceptanceManager",
        bot->GetName(), closestQuestGiver->GetName(), closestDistance);

    // Let the enterprise-grade manager handle eligibility, scoring, and acceptance
    _acceptanceManager->ProcessQuestGiver(closestQuestGiver);

    TC_LOG_ERROR("module.playerbot.quest",
        "🏆 SearchForQuestGivers: Bot {} quest acceptance complete (Total accepted: {}, Dropped: {})",
        bot->GetName(),
        _acceptanceManager->GetQuestsAccepted(),
        _acceptanceManager->GetQuestsDropped());
}

// ========================================================================
// QUEST TURN-IN SYSTEM - Multi-Tier Fallback Implementation
// ========================================================================

bool QuestStrategy::FindQuestEnderLocation(BotAI* ai, uint32 questId, QuestEnderLocation& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    TC_LOG_ERROR("module.playerbot.quest", "🔍 FindQuestEnderLocation: Bot {} searching for quest ender for quest {}",
                 bot->GetName(), questId);

    // Get quest ender NPC entry from quest relations
    auto questEnders = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questId);

    if (questEnders.begin() == questEnders.end())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ FindQuestEnderLocation: No quest ender found in creature_questender table for quest {}",
                     questId);
        return false;
    }

    // Get the first quest ender NPC entry
    uint32 questEnderEntry = questEnders.begin()->second;
    location.npcEntry = questEnderEntry;

    TC_LOG_ERROR("module.playerbot.quest", "✅ FindQuestEnderLocation: Quest {} ender is NPC entry {}",
                 questId, questEnderEntry);

    // ========================================================================
    // TIER 1: Creature Spawn Data (PREFERRED - Most Reliable)
    // ========================================================================
    TC_LOG_ERROR("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 1 - Searching creature spawn data for NPC {}",
                 questEnderEntry);

    // Get all spawn data for this creature
    auto const& spawnData = sObjectMgr->GetAllCreatureData();

    // Find the closest spawn of this creature to the bot
    float closestDistance = 999999.0f;
    CreatureData const* closestSpawn = nullptr;

    for (auto const& pair : spawnData)
    {
        CreatureData const& data = pair.second;

        // Check if this spawn is the quest ender we're looking for
        if (data.id != questEnderEntry)
            continue;

        // Check if spawn is on the same map as bot
        if (data.mapId != bot->GetMapId())
            continue;

        // Calculate distance from bot to this spawn
        float distance = bot->GetExactDist2d(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());

        TC_LOG_ERROR("module.playerbot.quest", "📍 FindQuestEnderLocation: Found spawn of NPC {} at ({:.1f}, {:.1f}, {:.1f}) on map {}, distance={:.1f}",
                     questEnderEntry,
                     data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(),
                     data.mapId, distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestSpawn = &data;
        }
    }

    if (closestSpawn)
    {
        // SUCCESS - Found via creature spawn data
        location.position.Relocate(
            closestSpawn->spawnPoint.GetPositionX(),
            closestSpawn->spawnPoint.GetPositionY(),
            closestSpawn->spawnPoint.GetPositionZ()
        );
        location.foundViaSpawn = true;

        TC_LOG_ERROR("module.playerbot.quest", "✅ TIER 1 SUCCESS: Found quest ender NPC {} via spawn data at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                     questEnderEntry,
                     location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                     closestDistance);
        return true;
    }

    TC_LOG_ERROR("module.playerbot.quest", "⚠️ TIER 1 FAILED: No spawn data found for NPC {}, falling back to TIER 2 (Quest POI)",
                 questEnderEntry);

    // ========================================================================
    // TIER 2: Quest POI Data (FALLBACK - Scripted/Event NPCs)
    // ========================================================================
    TC_LOG_ERROR("module.playerbot.quest", "🔬 FindQuestEnderLocation: TIER 2 - Searching Quest POI data for quest {}",
                 questId);

    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(questId);

    if (!poiData || poiData->Blobs.empty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ TIER 2 FAILED: No Quest POI data found for quest {}, falling back to TIER 3 (Area Search)",
                     questId);

        // TIER 3: Area search required
        location.requiresSearch = true;

        TC_LOG_ERROR("module.playerbot.quest", "⚠️ FindQuestEnderLocation: All automated methods failed - bot will need to search 50-yard radius for NPC {}",
                     questEnderEntry);

        // Return true with requiresSearch flag - bot will search area during navigation
        return true;
    }

    // Extract coordinates from POI blobs
    // Find the blob on the same map as bot
    QuestPOIBlobData const* validBlob = nullptr;

    for (auto const& blob : poiData->Blobs)
    {
        if (blob.MapID == static_cast<int32>(bot->GetMapId()))
        {
            validBlob = &blob;
            break;
        }
    }

    if (!validBlob || validBlob->Points.empty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "⚠️ TIER 2 FAILED: Quest POI data exists but no valid points on map {}, falling back to TIER 3",
                     bot->GetMapId());

        location.requiresSearch = true;
        return true;
    }

    // Use the first POI point as destination
    QuestPOIBlobPoint const& point = validBlob->Points[0];

    location.position.Relocate(
        static_cast<float>(point.X),
        static_cast<float>(point.Y),
        static_cast<float>(point.Z)
    );
    location.foundViaPOI = true;

    TC_LOG_ERROR("module.playerbot.quest", "✅ TIER 2 SUCCESS: Found quest POI coordinates at ({:.1f}, {:.1f}, {:.1f}) for quest {}",
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 questId);

    return true;
}

bool QuestStrategy::NavigateToQuestEnder(BotAI* ai, QuestEnderLocation const& location)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Calculate distance to destination
    float distance = bot->GetExactDist2d(location.position.GetPositionX(), location.position.GetPositionY());

    TC_LOG_ERROR("module.playerbot.quest", "🚶 NavigateToQuestEnder: Bot {} navigating to NPC {} at ({:.1f}, {:.1f}, {:.1f}), distance={:.1f}",
                 bot->GetName(), location.npcEntry,
                 location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ(),
                 distance);

    // Check if already at destination
    if (distance < 10.0f)
    {
        TC_LOG_ERROR("module.playerbot.quest", "✅ NavigateToQuestEnder: Bot {} already at destination (distance={:.1f} < 10.0), checking for NPC in range",
                     bot->GetName(), distance);

        // Check for NPC in range
        return CheckForQuestEnderInRange(ai, location.npcEntry);
    }

    // Start navigation
    bool moveResult = BotMovementUtil::MoveToPosition(bot, location.position);

    if (!moveResult)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ NavigateToQuestEnder: Bot {} failed to start pathfinding to ({:.1f}, {:.1f}, {:.1f})",
                     bot->GetName(),
                     location.position.GetPositionX(), location.position.GetPositionY(), location.position.GetPositionZ());
        return false;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ NavigateToQuestEnder: Bot {} pathfinding started to NPC {} (distance={:.1f})",
                 bot->GetName(), location.npcEntry, distance);

    return true;
}

bool QuestStrategy::CheckForQuestEnderInRange(BotAI* ai, uint32 npcEntry)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    TC_LOG_ERROR("module.playerbot.quest", "🔎 CheckForQuestEnderInRange: Bot {} scanning 50-yard radius for NPC entry {}",
                 bot->GetName(), npcEntry);

    // Scan for quest ender NPC in 50-yard radius
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, npcEntry, 50.0f);

    TC_LOG_ERROR("module.playerbot.quest", "📊 CheckForQuestEnderInRange: Bot {} found {} creatures with entry {} in 50-yard radius",
                 bot->GetName(), nearbyCreatures.size(), npcEntry);

    if (nearbyCreatures.empty())
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CheckForQuestEnderInRange: Bot {} found NO quest ender NPC {} in range",
                     bot->GetName(), npcEntry);
        return false;
    }

    // Find the closest valid quest ender
    Creature* closestQuestEnder = nullptr;
    float closestDistance = 999999.0f;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        // CRITICAL: Phase validation to prevent Ilario-type issues
        if (!bot->CanSeeOrDetect(creature))
        {
            TC_LOG_ERROR("module.playerbot.quest", "👻 CheckForQuestEnderInRange: NPC {} (Entry: {}) is in different phase, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        // Verify it's a quest giver (quest enders are also quest givers)
        if (!creature->IsQuestGiver())
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ CheckForQuestEnderInRange: NPC {} (Entry: {}) is NOT a quest giver, skipping",
                         creature->GetName(), creature->GetEntry());
            continue;
        }

        float distance = bot->GetDistance(creature);

        TC_LOG_ERROR("module.playerbot.quest", "✅ CheckForQuestEnderInRange: Found valid quest ender {} (Entry: {}) at distance {:.1f}",
                     creature->GetName(), creature->GetEntry(), distance);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestQuestEnder = creature;
        }
    }

    if (!closestQuestEnder)
    {
        TC_LOG_ERROR("module.playerbot.quest", "❌ CheckForQuestEnderInRange: Bot {} found creatures with entry {} but none are valid quest enders (phase mismatch or not alive)",
                     bot->GetName(), npcEntry);
        return false;
    }

    TC_LOG_ERROR("module.playerbot.quest", "✅ CheckForQuestEnderInRange: Bot {} found quest ender {} at distance {:.1f}",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Check if in interaction range
    if (closestDistance > INTERACTION_DISTANCE)
    {
        TC_LOG_ERROR("module.playerbot.quest", "🚶 CheckForQuestEnderInRange: Bot {} quest ender {} too far ({:.1f} > INTERACTION_DISTANCE), moving closer",
                     bot->GetName(), closestQuestEnder->GetName(), closestDistance);

        // Move to NPC
        Position npcPos;
        npcPos.Relocate(closestQuestEnder->GetPositionX(), closestQuestEnder->GetPositionY(), closestQuestEnder->GetPositionZ());
        BotMovementUtil::MoveToPosition(bot, npcPos);
        return false; // Not in range yet, but moving
    }

    // NPC is in interaction range - get all completed quests and turn them in
    TC_LOG_ERROR("module.playerbot.quest", "🎯 CheckForQuestEnderInRange: Bot {} at quest ender {} (distance {:.1f} <= INTERACTION_DISTANCE), processing quest turn-ins",
                 bot->GetName(), closestQuestEnder->GetName(), closestDistance);

    // Scan ALL active quests and turn in any that are complete with this NPC
    bool anyQuestTurnedIn = false;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        // Check if quest is complete
        if (bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE)
            continue;

        // Verify this NPC can end this quest
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        // Check if this NPC is a valid quest ender for this quest
        auto questEnders = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questId);
        bool isValidEnder = false;

        for (auto itr = questEnders.begin(); itr != questEnders.end(); ++itr)
        {
            if (itr->second == closestQuestEnder->GetEntry())
            {
                isValidEnder = true;
                break;
            }
        }

        if (!isValidEnder)
        {
            TC_LOG_ERROR("module.playerbot.quest", "⚠️ CheckForQuestEnderInRange: NPC {} is NOT a valid ender for quest {} ({}), skipping",
                         closestQuestEnder->GetName(), questId, quest->GetLogTitle());
            continue;
        }

        // Turn in the quest
        TC_LOG_ERROR("module.playerbot.quest", "🎯 CheckForQuestEnderInRange: Bot {} turning in quest {} ({}) to NPC {}",
                     bot->GetName(), questId, quest->GetLogTitle(), closestQuestEnder->GetName());

        if (CompleteQuestTurnIn(ai, questId, closestQuestEnder))
        {
            anyQuestTurnedIn = true;
            TC_LOG_ERROR("module.playerbot.quest", "✅ CheckForQuestEnderInRange: Bot {} successfully turned in quest {} ({})",
                         bot->GetName(), questId, quest->GetLogTitle());
        }
    }

    return anyQuestTurnedIn;
}

bool QuestStrategy::CompleteQuestTurnIn(BotAI* ai, uint32 questId, ::Unit* questEnder)
{
    if (!ai || !ai->GetBot() || !questEnder)
        return false;

    Player* bot = ai->GetBot();

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    TC_LOG_ERROR("module.playerbot.quest", "🏆 CompleteQuestTurnIn: Bot {} completing quest {} ({}) with NPC {}",
                 bot->GetName(), questId,
                 quest ? quest->GetLogTitle() : "UNKNOWN",
                 questEnder->GetName());

    // Prepare quest menu for this NPC
    bot->PrepareQuestMenu(questEnder->GetGUID());

    // Send prepared quest to trigger turn-in
    bot->SendPreparedQuest(questEnder);

    TC_LOG_ERROR("module.playerbot.quest", "✅ CompleteQuestTurnIn: Bot {} quest turn-in interaction sent for quest {}",
                 bot->GetName(), questId);

    // Increment quest completion counter
    _questsCompleted++;

    return true;
}

} // namespace Playerbot
