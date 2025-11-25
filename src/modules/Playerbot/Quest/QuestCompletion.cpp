/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "QuestCompletion.h"
#include "Core/PlayerBotHelpers.h"  // GetBotAI, GetGameSystems
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "Group.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "QuestPickup.h"
#include "QuestTurnIn.h"
#include "ObjectiveTracker.h"
#include "Movement/LeaderFollowBehavior.h"
#include "../AI/BotAI.h"
#include "Movement/BotMovementUtil.h"
#include "Interaction/Core/InteractionManager.h"
#include "../Combat/TargetSelector.h"
#include "Config/PlayerbotConfig.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"
#include "../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// Quest giver interaction range constant
constexpr float QUEST_GIVER_INTERACTION_RANGE = 5.0f;

/**
 * @brief Constructor
 */
QuestCompletion::QuestCompletion(Player* bot)
    : _bot(bot)
{
    if (!_bot)
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion: null bot!");
    _globalMetrics.Reset();
}

QuestCompletion::~QuestCompletion() {}

/**
 * @brief Start tracking quest completion for a bot
 * @param questId Quest ID to track
 * @param bot Bot player
 * @return True if quest tracking started successfully
 */
bool QuestCompletion::StartQuestCompletion(uint32 questId, Player* bot)
{
    if (!bot || !questId)
        return false;

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);

    // Check if bot has the quest
    if (bot->GetQuestStatus(questId) == QUEST_STATUS_NONE)
    {
        TC_LOG_DEBUG("playerbot", "QuestCompletion::StartQuestCompletion - Bot %s doesn't have quest %u",
            bot->GetName().c_str(), questId);
        return false;
    }

    // Initialize quest progress tracking
    QuestProgressData progress(questId, bot->GetGUID().GetCounter());
    progress.questGiverGuid = 0; // Will be populated when quest giver is found
    // Parse quest objectives
    ParseQuestObjectives(progress, quest);
    // Set appropriate completion strategy based on bot configuration
    if (bot->GetGroup())
        progress.strategy = QuestCompletionStrategy::GROUP_COORDINATION;
    else
        progress.strategy = QuestCompletionStrategy::EFFICIENT_COMPLETION;

    // Store progress data
    _botQuestProgress[bot->GetGUID().GetCounter()].push_back(progress);
    _botsInQuestMode.insert(bot->GetGUID().GetCounter());

    // Update metrics
    _globalMetrics.questsStarted++;
    _botMetrics[bot->GetGUID().GetCounter()].questsStarted++;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::StartQuestCompletion - Started tracking quest %u for bot %s",
        questId, bot->GetName().c_str());

    return true;
}

/**
 * @brief Update quest progress for a bot
 * @param bot Bot player
 */
void QuestCompletion::UpdateQuestProgress(Player* bot)
{
    if (!bot)
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    for (auto& progress : it->second)
    {
        // Skip completed quests
        if (progress.completionPercentage >= 100.0f)
            continue;

        // Update each objective
        bool anyProgress = false;
        for (auto& objective : progress.objectives)
        {
            if (objective.status == ObjectiveStatus::COMPLETED)
                continue;

            UpdateQuestObjectiveFromProgress(objective, bot);

            if (objective.currentCount > 0)
                anyProgress = true;
        }

        // Calculate overall completion percentage
        float totalProgress = 0.0f;
        uint32 completedObjectives = 0;
        for (const auto& objective : progress.objectives)
        {
            if (objective.requiredCount > 0)
            {
                float objProgress = std::min(1.0f,
                    static_cast<float>(objective.currentCount) / objective.requiredCount);
                totalProgress += objProgress;

                if (objective.status == ObjectiveStatus::COMPLETED)
                    completedObjectives++;
            }
        }

        if (!progress.objectives.empty())
        {
            progress.completionPercentage = (totalProgress / progress.objectives.size()) * 100.0f;
        }
        // Check for stuck state
        if (!anyProgress)
        {
            uint32 currentTime = GameTime::GetGameTimeMS();
            if (currentTime - progress.lastUpdateTime > STUCK_DETECTION_TIME)
            {
                progress.isStuck = true;
                progress.stuckTime = currentTime;
                DetectStuckState(bot, progress.questId);
            }
        }
        else
        {
            progress.isStuck = false;
            progress.stuckTime = 0;
            progress.lastUpdateTime = GameTime::GetGameTimeMS();
        }

        // Check if quest is ready for turn-in
        if (progress.completionPercentage >= 100.0f && progress.requiresTurnIn)
        {
            CompleteQuest(progress.questId, bot);
        }
    }
}

/**
 * @brief Complete a quest for the bot
 * @param questId Quest ID
 * @param bot Bot player
 */
void QuestCompletion::CompleteQuest(uint32 questId, Player* bot)
{
    if (!bot || !questId)
        return;

    // Check if quest can be completed
    if (!bot->CanCompleteQuest(questId))
    {
        TC_LOG_DEBUG("playerbot", "QuestCompletion::CompleteQuest - Bot %s cannot complete quest %u yet",
            bot->GetName().c_str(), questId);
        return;
    }

    // Mark quest as complete
    bot->CompleteQuest(questId);

    // Update progress tracking
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt != it->second.end())
        {
            questIt->completionPercentage = 100.0f;
            questIt->requiresTurnIn = true;

            // Add completion log entry
            questIt->completionLog.push_back("Quest completed at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    // Update metrics
    _globalMetrics.questsCompleted++;
    _botMetrics[bot->GetGUID().GetCounter()].questsCompleted++;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::CompleteQuest - Bot %s completed quest %u",
        bot->GetName().c_str(), questId);

    // Schedule turn-in through QuestTurnIn system (per-bot)
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        systems->GetQuestTurnIn()->ScheduleQuestTurnIn(bot, questId);
}

/**
 * @brief Turn in a completed quest
 * @param questId Quest ID
 * @param bot Bot player
 * @return True if quest was turned in successfully
 */
bool QuestCompletion::TurnInQuest(uint32 questId, Player* bot)
{
    if (!bot || !questId)
        return false;

    // Delegate to QuestTurnIn system (per-bot)
    if (IGameSystemsManager* systems = GetGameSystems(bot))
        return systems->GetQuestTurnIn()->TurnInQuest(questId, bot);
    return false;
}

/**
 * @brief Track quest objectives for a bot
 * @param bot Bot player
 */
void QuestCompletion::TrackQuestObjectives(Player* bot)
{
    if (!bot)
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    for (auto& progress : it->second)
    {
        // Skip completed quests
        if (progress.completionPercentage >= 100.0f)
            continue;

        // Execute objectives based on strategy
        switch (progress.strategy)
        {
            case QuestCompletionStrategy::EFFICIENT_COMPLETION:
                ExecuteEfficientStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::SAFE_COMPLETION:
                ExecuteSafeStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::GROUP_COORDINATION:
                ExecuteGroupStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::SOLO_FOCUS:
                ExecuteSoloStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::EXPERIENCE_MAXIMIZING:
                ExecuteExperienceStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::SPEED_COMPLETION:
                ExecuteSpeedStrategy(bot, progress);
                break;
            case QuestCompletionStrategy::THOROUGH_EXPLORATION:
                ExecuteExplorationStrategy(bot, progress);
                break;
        }
    }
}

/**
 * @brief Execute a quest objective
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::ExecuteObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot || objective.status == ObjectiveStatus::COMPLETED)
        return;

    // Check if we can execute this objective
    if (!CanExecuteObjective(bot, objective))
    {
        objective.status = ObjectiveStatus::BLOCKED;
        return;
    }

    objective.status = ObjectiveStatus::IN_PROGRESS;

    // Execute based on objective type
    switch (objective.type)
    {
        case QuestObjectiveType::KILL_CREATURE:
            HandleKillObjective(bot, objective);
            break;
        case QuestObjectiveType::COLLECT_ITEM:
            HandleCollectObjective(bot, objective);
            break;
        case QuestObjectiveType::TALK_TO_NPC:
            HandleTalkToNpcObjective(bot, objective);
            break;
        case QuestObjectiveType::REACH_LOCATION:
            HandleLocationObjective(bot, objective);
            break;
        case QuestObjectiveType::USE_GAMEOBJECT:
            HandleGameObjectObjective(bot, objective);
            break;
        case QuestObjectiveType::CAST_SPELL:
            HandleSpellCastObjective(bot, objective);
            break;
        case QuestObjectiveType::EMOTE_AT_TARGET:
            HandleEmoteObjective(bot, objective);
            break;
        case QuestObjectiveType::ESCORT_NPC:
            HandleEscortObjective(bot, objective);
            break;
        default:
            TC_LOG_DEBUG("playerbot", "QuestCompletion::ExecuteObjective - Unknown objective type %u",
                static_cast<uint32>(objective.type));
            break;
    }

    // Update time spent
    objective.timeSpent += GameTime::GetGameTimeMS() - objective.lastUpdateTime;
    objective.lastUpdateTime = GameTime::GetGameTimeMS();
}

/**
 * @brief Update objective progress
 * @param bot Bot player
 * @param questId Quest ID
 * @param objectiveIndex Objective index
 */
void QuestCompletion::UpdateObjectiveProgress(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (!bot)
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt == it->second.end() || objectiveIndex >= questIt->objectives.size())
        return;

    auto& objective = questIt->objectives[objectiveIndex];

    // Get quest from ObjectMgr to access objectives
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return;

    QuestObjectives const& questObjectives = quest->GetObjectives();
    if (objectiveIndex >= questObjectives.size())
        return;

    QuestObjective const& questObj = questObjectives[objectiveIndex];

    // Update objective count using Player::GetQuestObjectiveData
    objective.currentCount = bot->GetQuestObjectiveData(questObj);
    if (objective.currentCount >= objective.requiredCount)
    {
        objective.status = ObjectiveStatus::COMPLETED;
        _globalMetrics.objectivesCompleted++;
        _botMetrics[bot->GetGUID().GetCounter()].objectivesCompleted++;
    }
}

/**
 * @brief Check if an objective is complete
 * @param objective Objective data
 * @return True if objective is complete
 */
bool QuestCompletion::IsObjectiveComplete(const QuestObjectiveData& objective)
{
    return objective.status == ObjectiveStatus::COMPLETED ||
           (objective.requiredCount > 0 && objective.currentCount >= objective.requiredCount);
}

/**
 * @brief Handle kill objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleKillObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find creatures to kill
    if (!FindKillTarget(bot, objective))
    {
        // No targets found, move to spawn location if known
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
        return;
    }

    // Get nearest target
    Unit* target = nullptr;
    float minDistance = objective.searchRadius;

    // DEADLOCK FIX: Use lock-free spatial grid
    Map* map = bot->GetMap();
    if (map)
    {
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
        }

        if (spatialGrid)
        {
            // DEADLOCK FIX: Use snapshot-based query (thread-safe, lock-free)
            std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
                spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

            ObjectGuid targetGuid;  // Store GUID instead of pointer

            for (auto const& snapshot : nearbyCreatures)
            {
                // Use snapshot fields instead of calling methods on pointers
                if (snapshot.isDead || !snapshot.isVisible)
                    continue;
                if (snapshot.entry != objective.targetId)
                    continue;

                float distance = bot->GetExactDist(snapshot.position);
                if (distance < minDistance)
                {
                    minDistance = distance;
                    targetGuid = snapshot.guid;  // Store GUID
                }
            }

            // Store the GUID for later use (main thread will resolve)
            if (!targetGuid.IsEmpty())
                target = ObjectAccessor::GetCreature(*bot, targetGuid);  // Safe: this method may run on main thread context
        }
    }

    if (target)
    {
        // Engage target through combat system
        if (BotAI* ai = dynamic_cast<BotAI*>(bot->GetAI()))
        {
            // Set the bot's target to the creature
            bot->SetSelection(target->GetGUID());
            // Attack the target
            bot->Attack(target, true);
        }

        // Track kill credit - Update using proper API
        Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
        if (quest)
        {
            QuestObjectives const& questObjectives = quest->GetObjectives();
            if (objective.objectiveIndex < questObjectives.size())
            {
                objective.currentCount = bot->GetQuestObjectiveData(questObjectives[objective.objectiveIndex]);
            }
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleKillObjective - Bot %s engaging %s for quest %u",
            bot->GetName().c_str(), target->GetName().c_str(), objective.questId);
    }
}

/**
 * @brief Handle collect objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleCollectObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Check if we already have the items
    uint32 itemCount = bot->GetItemCount(objective.targetId);
    if (itemCount >= objective.requiredCount)
    {
        objective.currentCount = itemCount;
        objective.status = ObjectiveStatus::COMPLETED;
        return;
    }

    // Find items to collect (from creatures or game objects)
    if (!FindCollectibleItem(bot, objective))
    {
        // Move to collection area
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
        return;
    }

    // Update collection progress
    objective.currentCount = bot->GetItemCount(objective.targetId);
}

/**
 * @brief Handle talk to NPC objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleTalkToNpcObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find the NPC
    Creature* npc = nullptr;
    if (objective.targetId)
    {
        // DEADLOCK FIX: Use lock-free spatial grid
        Map* map = bot->GetMap();
        if (map)
        {
            DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
            {
                sSpatialGridManager.CreateGrid(map);
                spatialGrid = sSpatialGridManager.GetGrid(map);
            }
            if (spatialGrid)
            {
                // DEADLOCK FIX: Use snapshot-based query (thread-safe, lock-free)
                std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
                    spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);
                ObjectGuid npcGuid;
                float minDistance = objective.searchRadius;

                for (auto const& snapshot : nearbyCreatures)
                {
                    // Use snapshot fields instead of pointer method calls
                    if (!snapshot.isVisible)
                        continue;
                    if (snapshot.entry != objective.targetId)
                        continue;
                    float distance = bot->GetExactDist(snapshot.position);
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        npcGuid = snapshot.guid;
                    }
                }

                // Resolve GUID to pointer after loop (safe if method runs on main thread context)
                if (!npcGuid.IsEmpty())
                    npc = ObjectAccessor::GetCreature(*bot, npcGuid);
            }
        }
    }

    if (npc)
    {
        // Move to NPC if not in range
        if (bot->GetDistance(npc) > QUEST_GIVER_INTERACTION_RANGE)
        {
            BotMovementUtil::MoveToUnit(bot, npc, QUEST_GIVER_INTERACTION_RANGE - 1.0f);
            return;
        }

        // Interact with NPC - use StartInteraction instead of InteractWithNPC
        InteractionManager::Instance()->StartInteraction(bot, npc, InteractionType::None);

        // Mark objective as complete
        objective.status = ObjectiveStatus::COMPLETED;
        objective.currentCount = objective.requiredCount;

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleTalkToNpcObjective - Bot %s talked to %s for quest %u",
            bot->GetName().c_str(), npc->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to NPC location if known
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Handle location objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleLocationObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Check if we're at the location
    float distance = bot->GetDistance(objective.targetLocation);
    if (distance <= OBJECTIVE_COMPLETION_RADIUS)
    {
        // Trigger area exploration
        bot->AreaExploredOrEventHappens(objective.questId);

        objective.status = ObjectiveStatus::COMPLETED;
        objective.currentCount = objective.requiredCount;

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleLocationObjective - Bot %s reached location for quest %u",
            bot->GetName().c_str(), objective.questId);
    }
    else
    {
        // Navigate to location
        NavigateToObjective(bot, objective);
    }
}
/**
 * @brief Handle game object objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleGameObjectObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // DEADLOCK FIX: Use lock-free spatial grid to find game object
    GameObject* gameObject = nullptr;
    if (objective.targetId)
    {
        Map* map = bot->GetMap();
        if (!map)
            return;

        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
                return;
        }

        // Query nearby GameObjects (lock-free!)
        std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
            spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), objective.searchRadius);
        // Find matching GameObject using snapshots
        ObjectGuid targetGuid;
        for (auto const& snapshot : nearbyObjects)
        {
            if (snapshot.entry == objective.targetId && snapshot.isSpawned)
            {
                targetGuid = snapshot.guid;
                break;
            }
        }

        // Resolve GUID after loop
        if (!targetGuid.IsEmpty())
            gameObject = ObjectAccessor::GetGameObject(*bot, targetGuid);
    }

    if (gameObject)
    {
        // Move to object if not in range
        if (bot->GetDistance(gameObject) > QUEST_GIVER_INTERACTION_RANGE)
        {
            BotMovementUtil::MoveToPosition(bot, gameObject->GetPosition());
            return;
        }

        // Use the game object
        gameObject->Use(bot);

        // Update progress
        objective.currentCount++;
        if (objective.currentCount >= objective.requiredCount)
        {
            objective.status = ObjectiveStatus::COMPLETED;
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleGameObjectObjective - Bot %s used %s for quest %u",
            bot->GetName().c_str(), gameObject->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to object location if known
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Handle spell cast objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleSpellCastObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Check if bot knows the spell
    if (!bot->HasSpell(objective.targetId))
    {
        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleSpellCastObjective - Bot %s doesn't know spell %u for quest %u",
            bot->GetName().c_str(), objective.targetId, objective.questId);
        objective.status = ObjectiveStatus::BLOCKED;
        return;
    }

    // Find target for spell if needed
    Unit* target = bot->GetSelectedUnit();
    if (!target && objective.targetLocation.GetPositionX() != 0)
    {
        // Move to cast location
        NavigateToObjective(bot, objective);
        return;
    }

    // Cast the spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(objective.targetId, DIFFICULTY_NONE);
    if (spellInfo)
    {
        bot->CastSpell(target ? target : bot, objective.targetId, false);

        // Update progress
        objective.currentCount++;
        if (objective.currentCount >= objective.requiredCount)
        {
            objective.status = ObjectiveStatus::COMPLETED;
        }
        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleSpellCastObjective - Bot %s cast spell %u for quest %u",
            bot->GetName().c_str(), objective.targetId, objective.questId);
    }
}

/**
 * @brief Handle emote objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleEmoteObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find target for emote
    Unit* target = nullptr;
    if (objective.targetId)
    {
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
        Map* map = bot->GetMap();
        if (!map)
            return;

        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
                return;
        }

        // Use snapshot-based query (thread-safe, lock-free)
        std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
            spatialGrid->QueryNearbyCreatures(bot->GetPosition(), QUEST_GIVER_INTERACTION_RANGE);

        ObjectGuid targetGuid;
        for (auto const& snapshot : nearbyCreatures)
        {
            if (snapshot.entry == objective.targetId && snapshot.isVisible)
            {
                targetGuid = snapshot.guid;
                break;  // Use first matching creature
            }
        }

        // Resolve GUID to pointer after loop
        if (!targetGuid.IsEmpty())
            target = ObjectAccessor::GetCreature(*bot, targetGuid);
    }

    if (target)
    {
        // Perform emote (cast targetId to Emote enum)
        bot->HandleEmoteCommand(static_cast<Emote>(objective.targetId));

        // Update progress
        objective.currentCount++;
        if (objective.currentCount >= objective.requiredCount)
        {
            objective.status = ObjectiveStatus::COMPLETED;
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleEmoteObjective - Bot %s performed emote for quest %u",
            bot->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to target location
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Handle escort objectives
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::HandleEscortObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Find escort target
    Creature* escortTarget = nullptr;
    if (objective.targetId)
    {
        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
        Map* map = bot->GetMap();
        if (!map)
            return;

        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            sSpatialGridManager.CreateGrid(map);
            spatialGrid = sSpatialGridManager.GetGrid(map);
            if (!spatialGrid)
                return;
        }
        // Use snapshot-based query (thread-safe, lock-free)
        std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
            spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

        ObjectGuid escortGuid;
        for (auto const& snapshot : nearbyCreatures)
        {
            if (snapshot.entry == objective.targetId && !snapshot.isDead)
            {
                escortGuid = snapshot.guid;
                break;  // Use first alive matching creature
            }
        }

        // Resolve GUID to pointer after loop
        if (!escortGuid.IsEmpty())
            escortTarget = ObjectAccessor::GetCreature(*bot, escortGuid);
    }

    if (escortTarget)
    {
        // Follow the escort target
        if (bot->GetDistance(escortTarget) > 10.0f)
        {
            BotMovementUtil::MoveToUnit(bot, escortTarget, 5.0f);
        }

        // Check if escort is complete (handled by quest system)
        if (bot->IsQuestObjectiveComplete(objective.questId, objective.objectiveIndex))
        {
            objective.status = ObjectiveStatus::COMPLETED;
            objective.currentCount = objective.requiredCount;
        }

        TC_LOG_DEBUG("playerbot", "QuestCompletion::HandleEscortObjective - Bot %s escorting %s for quest %u",
            bot->GetName().c_str(), escortTarget->GetName().c_str(), objective.questId);
    }
    else
    {
        // Move to escort start location
        if (objective.targetLocation.GetPositionX() != 0)
        {
            NavigateToObjective(bot, objective);
        }
    }
}

/**
 * @brief Navigate to quest objective
 * @param bot Bot player
 * @param objective Objective data
 */
void QuestCompletion::NavigateToObjective(Player* bot, const QuestObjectiveData& objective)
{
    if (!bot)
        return;

    // Get optimal position for objective
    Position targetPos = GetOptimalObjectivePosition(bot, objective);
    // Use movement utility to navigate
    BotMovementUtil::MoveToPosition(bot, targetPos);

    TC_LOG_DEBUG("playerbot", "QuestCompletion::NavigateToObjective - Bot %s moving to objective for quest %u",
        bot->GetName().c_str(), objective.questId);
}

/**
 * @brief Find target for kill objective
 * @param bot Bot player
 * @param objective Objective data
 * @return True if target found
 */
bool QuestCompletion::FindKillTarget(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return false;
    // DEADLOCK FIX: Use lock-free spatial grid with snapshots
    Map* map = bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // Use snapshot-based query (thread-safe, lock-free)
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

    // Filter valid targets using snapshot data
    DoubleBufferedSpatialGrid::CreatureSnapshot nearestSnapshot;
    float minDistance = objective.searchRadius;
    bool found = false;

    for (auto const& snapshot : nearbyCreatures)
    {
        // Filter: must match entry, be alive, visible, and hostile
        if (snapshot.entry != objective.targetId)
            continue;
        if (snapshot.isDead || !snapshot.isVisible)
            continue;
        // Note: faction check would require additional snapshot field or be done after GUID resolution

        float distance = bot->GetExactDist(snapshot.position);
        if (distance < minDistance)
        {
            minDistance = distance;
            nearestSnapshot = snapshot;
            found = true;
        }
    }

    if (found)
    {
        // Update objective location to nearest target (using snapshot position)
        objective.targetLocation = nearestSnapshot.position;
        return true;
    }

    return false;
}

/**
 * @brief Find collectible item for objective
 * @param bot Bot player
 * @param objective Objective data
 * @return True if item source found
 */
bool QuestCompletion::FindCollectibleItem(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
        return false;

    Map* map = bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // DEADLOCK FIX: First check if item drops from creatures using snapshots
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), objective.searchRadius);

    for (auto const& snapshot : nearbyCreatures)
    {
        // Filter: must be alive and not friendly (assume non-friendly if we can't check faction)
        if (snapshot.isDead)
            continue;

        // Check if creature can drop the item (would need loot template access)
        // For now, assume creatures in quest area can drop quest items
        float distance = bot->GetExactDist(snapshot.position);
        if (distance < objective.searchRadius)
        {
            objective.targetLocation = snapshot.position;
            return true;
        }
    }

    // DEADLOCK FIX: Check for game objects using snapshots
    std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
        spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), objective.searchRadius);

    for (auto const& snapshot : nearbyObjects)
    {
        // Check if game object is lootable using snapshot data
        if (snapshot.goType == GAMEOBJECT_TYPE_CHEST ||
            snapshot.goType == GAMEOBJECT_TYPE_GOOBER)
        {
            objective.targetLocation = snapshot.position;
            return true;
        }
    }

    return false;
}

/**
 * @brief Get optimal position for objective
 * @param bot Bot player
 * @param objective Objective data
 * @return Optimal position
 */
Position QuestCompletion::GetOptimalObjectivePosition(Player* bot, const QuestObjectiveData& objective)
{
    if (!bot)
        return Position();

    // If we have a specific target location, use it
    if (objective.targetLocation.GetPositionX() != 0)
        return objective.targetLocation;

    // Otherwise, try to find objective locations from database
    std::vector<Position> locations = GetObjectiveLocations(objective);
    if (!locations.empty())
    {
        // Return nearest location
        return *std::min_element(locations.begin(), locations.end(),
            [bot](const Position& a, const Position& b)
            {
                return bot->GetDistance(a) < bot->GetDistance(b);
            });
    }

    // Default to bot's current position
    return bot->GetPosition();
}

/**
 * @brief Get possible locations for objective
 * @param objective Objective data
 * @return Vector of possible positions
 */
std::vector<Position> QuestCompletion::GetObjectiveLocations(const QuestObjectiveData& objective)
{
    std::vector<Position> locations;

    // Add primary location if set
    if (objective.targetLocation.GetPositionX() != 0)
        locations.push_back(objective.targetLocation);

    // Could query database for spawn points of objective creatures/objects
    // For now, return what we have

    return locations;
}

/**
 * @brief Parse quest objectives from quest template
 * @param progress Quest progress data
 * @param quest Quest template
 */
void QuestCompletion::ParseQuestObjectives(QuestProgressData& progress, const Quest* quest)
{
    if (!quest)
        return;

    // Parse objectives from Quest::Objectives vector (modern TrinityCore API)
    QuestObjectives const& questObjectives = quest->GetObjectives();

    for (size_t i = 0; i < questObjectives.size(); ++i)
    {
        QuestObjective const& obj = questObjectives[i];

        // Map TrinityCore objective types to our internal types
        QuestObjectiveType internalType;
        switch (obj.Type)
        {
            case QUEST_OBJECTIVE_MONSTER:
                internalType = QuestObjectiveType::KILL_CREATURE;
                break;
            case QUEST_OBJECTIVE_ITEM:
                internalType = QuestObjectiveType::COLLECT_ITEM;
                break;
            case QUEST_OBJECTIVE_GAMEOBJECT:
                internalType = QuestObjectiveType::USE_GAMEOBJECT;
                break;
            case QUEST_OBJECTIVE_TALKTO:
                internalType = QuestObjectiveType::TALK_TO_NPC;
                break;
            case QUEST_OBJECTIVE_AREATRIGGER:
            case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
            case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
                internalType = QuestObjectiveType::REACH_LOCATION;
                break;
            case QUEST_OBJECTIVE_LEARNSPELL:
                internalType = QuestObjectiveType::LEARN_SPELL;
                break;
            default:
                internalType = QuestObjectiveType::CUSTOM_OBJECTIVE;
                break;
        }

        if (obj.Amount > 0)
        {
            QuestObjectiveData objective(quest->GetQuestId(), static_cast<uint32>(i),
                                        internalType, obj.ObjectID, obj.Amount);
            objective.description = obj.Description;
            objective.isOptional = (obj.Flags & QUEST_OBJECTIVE_FLAG_OPTIONAL) != 0;
            progress.objectives.push_back(objective);
        }
    }
}

/**
 * @brief Update quest objective from game progress
 * @param objective Objective data to update
 * @param bot Bot player
 */
void QuestCompletion::UpdateQuestObjectiveFromProgress(QuestObjectiveData& objective, Player* bot)
{
    if (!bot)
        return;

    // Get quest from ObjectMgr to access objectives
    Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
    if (!quest)
        return;

    QuestObjectives const& questObjectives = quest->GetObjectives();
    if (objective.objectiveIndex >= questObjectives.size())
        return;

    QuestObjective const& questObj = questObjectives[objective.objectiveIndex];

    // Update based on objective type
    switch (objective.type)
    {
        case QuestObjectiveType::KILL_CREATURE:
        case QuestObjectiveType::USE_GAMEOBJECT:
        case QuestObjectiveType::TALK_TO_NPC:
            // Use GetQuestObjectiveData to retrieve progress
            objective.currentCount = bot->GetQuestObjectiveData(questObj);
            break;

        case QuestObjectiveType::COLLECT_ITEM:
            objective.currentCount = bot->GetItemCount(objective.targetId);
            break;

        default:
            // Check if objective is complete through quest system
            if (bot->IsQuestObjectiveComplete(objective.questId, objective.objectiveIndex))
            {
                objective.currentCount = objective.requiredCount;
            }
            else
            {
                objective.currentCount = bot->GetQuestObjectiveData(questObj);
            }
            break;
    }

    // Update status
    if (objective.currentCount >= objective.requiredCount)
    {
        objective.status = ObjectiveStatus::COMPLETED;
    }
}

/**
 * @brief Check if objective can be executed
 * @param bot Bot player
 * @param objective Objective data
 * @return True if objective can be executed
 */
bool QuestCompletion::CanExecuteObjective(Player* bot, const QuestObjectiveData& objective)
{
    if (!bot)
        return false;

    // Check if objective is already complete
    if (objective.status == ObjectiveStatus::COMPLETED)
        return false;

    // Check if bot is alive
    if (!bot->IsAlive())
        return false;

    // Check if bot is in combat for non-combat objectives
    if (bot->IsInCombat() && objective.type != QuestObjectiveType::KILL_CREATURE)
        return false;

    // Check if objective has failed too many times
    if (objective.retryCount >= MAX_OBJECTIVE_RETRIES)
        return false;

    // Check group requirements
    if (objective.requiresGroup && !bot->GetGroup())
        return false;

    return true;
}

/**
 * @brief Execute efficient completion strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteEfficientStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Find nearest incomplete objective
    QuestObjectiveData* nearestObjective = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (auto& objective : progress.objectives)
    {
        if (objective.status == ObjectiveStatus::COMPLETED)
            continue;

        float distance = bot->GetDistance(objective.targetLocation);
        if (distance < minDistance)
        {
            minDistance = distance;
            nearestObjective = &objective;
        }
    }

    if (nearestObjective)
    {
        ExecuteObjective(bot, *nearestObjective);
    }
}

/**
 * @brief Execute safe completion strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteSafeStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Prioritize non-combat objectives when health is low
    if (bot->GetHealthPct() < 50.0f)
    {
        for (auto& objective : progress.objectives)
        {
            if (objective.status != ObjectiveStatus::COMPLETED &&
                objective.type != QuestObjectiveType::KILL_CREATURE)
            {
                ExecuteObjective(bot, objective);
                return;
            }
        }
    }

    // Otherwise execute normally
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Execute group coordination strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteGroupStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot || !bot->GetGroup())
    {
        ExecuteEfficientStrategy(bot, progress);
        return;
    }

    // Coordinate with group members
    Group* group = bot->GetGroup();
    // Share objective progress
    ShareObjectiveProgress(group, progress.questId);

    // Find objectives that group is working on
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED)
        {
            // Check if other group members are near this objective
            bool groupNearby = false;
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (member && member != bot && member->IsAlive())
                {
                    if (member->GetDistance(objective.targetLocation) < 50.0f)
                    {
                        groupNearby = true;
                        break;
                    }
                }
            }

            if (groupNearby)
            {
                ExecuteObjective(bot, objective);
                return;
            }
        }
    }

    // No group objectives, execute normally
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Share objective progress with group
 * @param group Group pointer
 * @param questId Quest ID
 */
void QuestCompletion::ShareObjectiveProgress(Group* group, uint32 questId)
{
    if (!group)
        return;

    // Update group quest sharing data
    _groupQuestSharing[group->GetGUID().GetCounter()].push_back(questId);
    _groupObjectiveSync[group->GetGUID().GetCounter()][questId] = GameTime::GetGameTimeMS();
}

/**
 * @brief Detect stuck state for quest
 * @param bot Bot player
 * @param questId Quest ID
 */
void QuestCompletion::DetectStuckState(Player* bot, uint32 questId)
{
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::DetectStuckState - Bot %s stuck on quest %u",
        bot->GetName().c_str(), questId);

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt != it->second.end())
    {
        questIt->isStuck = true;
        questIt->stuckTime = GameTime::GetGameTimeMS();
        questIt->consecutiveFailures++;

        // Update metrics
        _globalMetrics.stuckInstances++;
        _botMetrics[bot->GetGUID().GetCounter()].stuckInstances++;

        // Try to recover
        RecoverFromStuckState(bot, questId);
    }
}

/**
 * @brief Recover from stuck state
 * @param bot Bot player
 * @param questId Quest ID
 */
void QuestCompletion::RecoverFromStuckState(Player* bot, uint32 questId)
{
    if (!bot)
        return;

    TC_LOG_DEBUG("playerbot", "QuestCompletion::RecoverFromStuckState - Attempting recovery for bot %s on quest %u",
        bot->GetName().c_str(), questId);

    // Try different recovery strategies
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt != it->second.end())
    {
        // Reset stuck objectives
        for (auto& objective : questIt->objectives)
        {
            if (objective.status == ObjectiveStatus::IN_PROGRESS)
            {
                objective.status = ObjectiveStatus::NOT_STARTED;
                objective.retryCount++;
            }
        }

        // Change strategy
        if (questIt->strategy == QuestCompletionStrategy::EFFICIENT_COMPLETION)
            questIt->strategy = QuestCompletionStrategy::SAFE_COMPLETION;

        // Reset stuck flag
        questIt->isStuck = false;
        questIt->stuckTime = 0;
    }
}

/**
 * @brief Get bot completion metrics
 * @param botGuid Bot GUID
 * @return Completion metrics
 */
IQuestCompletion::QuestCompletionMetricsSnapshot QuestCompletion::GetBotCompletionMetrics(uint32 botGuid)
{
    auto it = _botMetrics.find(botGuid);
    IQuestCompletion::QuestCompletionMetricsSnapshot result{};

    if (it != _botMetrics.end())
    {
        auto snapshot = it->second.CreateSnapshot();
        result.questsStarted = snapshot.questsStarted;
        result.questsCompleted = snapshot.questsCompleted;
        result.questsFailed = snapshot.questsFailed;
        result.objectivesCompleted = snapshot.objectivesCompleted;
        result.stuckInstances = snapshot.stuckInstances;
        result.averageCompletionTime = snapshot.averageCompletionTime;
        result.completionSuccessRate = snapshot.completionSuccessRate;
        result.objectiveEfficiency = snapshot.objectiveEfficiency;
        result.totalDistanceTraveled = snapshot.totalDistanceTraveled;
    }

    return result;
}

/**
 * @brief Get global completion metrics
 * @return Global completion metrics
 */
IQuestCompletion::QuestCompletionMetricsSnapshot QuestCompletion::GetGlobalCompletionMetrics()
{
    auto snapshot = _globalMetrics.CreateSnapshot();
    IQuestCompletion::QuestCompletionMetricsSnapshot result{};
    result.questsStarted = snapshot.questsStarted;
    result.questsCompleted = snapshot.questsCompleted;
    result.questsFailed = snapshot.questsFailed;
    result.objectivesCompleted = snapshot.objectivesCompleted;
    result.stuckInstances = snapshot.stuckInstances;
    result.averageCompletionTime = snapshot.averageCompletionTime;
    result.completionSuccessRate = snapshot.completionSuccessRate;
    result.objectiveEfficiency = snapshot.objectiveEfficiency;
    result.totalDistanceTraveled = snapshot.totalDistanceTraveled;
    return result;
}

/**
 * @brief Set quest completion strategy for bot
 * @param botGuid Bot GUID
 * @param strategy Completion strategy
 */
void QuestCompletion::SetQuestCompletionStrategy(uint32 botGuid, QuestCompletionStrategy strategy)
{
    _botStrategies[botGuid] = strategy;
}

/**
 * @brief Get quest completion strategy for bot
 * @param botGuid Bot GUID
 * @return Completion strategy
 */
QuestCompletionStrategy QuestCompletion::GetQuestCompletionStrategy(uint32 botGuid)
{
    auto it = _botStrategies.find(botGuid);
    if (it != _botStrategies.end())
        return it->second;

    return QuestCompletionStrategy::EFFICIENT_COMPLETION;
}

/**
 * @brief Update quest completion system
 * @param diff Time difference
 */
void QuestCompletion::Update(uint32 diff)
{
    // Clean up completed quests periodically
    static uint32 cleanupTimer = 0;
    cleanupTimer += diff;

    if (cleanupTimer >= 60000) // Every minute
    {
        CleanupCompletedQuests();
        cleanupTimer = 0;
    }

    // Note: Quest turn-in processing is handled by QuestTurnIn system
}

/**
 * @brief Clean up completed quests
 */
void QuestCompletion::CleanupCompletedQuests()
{
    uint32 currentTime = GameTime::GetGameTimeMS();

    for (auto& [botGuid, progressList] : _botQuestProgress)
    {
        progressList.erase(
            std::remove_if(progressList.begin(), progressList.end(),
                [currentTime](const QuestProgressData& progress)
                {
                    return progress.completionPercentage >= 100.0f &&
                           (currentTime - progress.lastUpdateTime) > 300000; // 5 minutes
                }),
            progressList.end()
        );
    }
}

/**
 * @brief Execute solo focus strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteSoloStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Focus on objectives that don't require group
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED && !objective.requiresGroup)
        {
            ExecuteObjective(bot, objective);
            return;
        }
    }

    // If all require group, try anyway
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Execute experience maximizing strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteExperienceStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Prioritize kill objectives for experience
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED &&
            objective.type == QuestObjectiveType::KILL_CREATURE)
        {
            ExecuteObjective(bot, objective);
            return;
        }
    }

    // Then do other objectives
    ExecuteEfficientStrategy(bot, progress);
}

/**
 * @brief Execute speed completion strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteSpeedStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Execute multiple objectives in parallel if possible
    uint32 activeObjectives = 0;

    for (auto& objective : progress.objectives)
    {
        if (objective.status == ObjectiveStatus::IN_PROGRESS)
            activeObjectives++;
    }

    if (activeObjectives < MAX_CONCURRENT_OBJECTIVES)
    {
        for (auto& objective : progress.objectives)
        {
            if (objective.status == ObjectiveStatus::NOT_STARTED)
            {
                ExecuteObjective(bot, objective);
                activeObjectives++;

                if (activeObjectives >= MAX_CONCURRENT_OBJECTIVES)
                    break;
            }
        }
    }
}

/**
 * @brief Execute exploration strategy
 * @param bot Bot player
 * @param progress Quest progress data
 */
void QuestCompletion::ExecuteExplorationStrategy(Player* bot, QuestProgressData& progress)
{
    if (!bot)
        return;

    // Explore widely while completing objectives
    for (auto& objective : progress.objectives)
    {
        if (objective.status != ObjectiveStatus::COMPLETED)
        {
            // Increase search radius for exploration
            objective.searchRadius = OBJECTIVE_SEARCH_RADIUS * 1.5f;
            ExecuteObjective(bot, objective);
            return;
        }
    }
}

} // namespace Playerbot/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "QuestCompletion.h"
#include "Core/PlayerBotHelpers.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "Group.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Movement/BotMovementUtil.h"
#include "ObjectAccessor.h"
#include "../Spatial/SpatialGridManager.h"
#include "Config/PlayerbotConfig.h"
#include "SpellMgr.h"
#include <algorithm>
#include <numeric>
#include <queue>
#include <unordered_set>

namespace Playerbot
{

// 1. FindObjectiveTarget - Find specific targets for quest objectives
bool QuestCompletion::FindObjectiveTarget(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::FindObjectiveTarget - null bot!");
        return false;
    }

    Map* map = bot->GetMap();
    if (!map)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::FindObjectiveTarget - Bot %s has no map",
            bot->GetName().c_str());
        return false;
    }

    // Get or create spatial grid
    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
        {
            TC_LOG_ERROR("playerbot.quest", "QuestCompletion::FindObjectiveTarget - Failed to create spatial grid");
            return false;
        }
    }

    bool targetFound = false;
    float searchRadius = objective.searchRadius > 0 ? objective.searchRadius : OBJECTIVE_SEARCH_RADIUS;

    switch (objective.type)
    {
        case QuestObjectiveType::KILL_CREATURE:
        case QuestObjectiveType::TALK_TO_NPC:
        case QuestObjectiveType::ESCORT_NPC:
        {
            // Query creatures using lock-free spatial grid
            std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
                spatialGrid->QueryNearbyCreatures(bot->GetPosition(), searchRadius);

            float minDistance = searchRadius;
            DoubleBufferedSpatialGrid::CreatureSnapshot bestTarget;

            for (auto const& snapshot : nearbyCreatures)
            {
                // Check if creature matches objective requirements
                if (snapshot.entry != objective.targetId &&
                    std::find(objective.alternativeTargets.begin(),
                             objective.alternativeTargets.end(),
                             snapshot.entry) == objective.alternativeTargets.end())
                    continue;

                // Additional filters based on objective type
                if (objective.type == QuestObjectiveType::KILL_CREATURE && snapshot.isDead)
                    continue;
                if (!snapshot.isVisible)
                    continue;

                float distance = bot->GetExactDist(snapshot.position);
                if (distance < minDistance)
                {
                    minDistance = distance;
                    bestTarget = snapshot;
                    targetFound = true;
                }
            }

            if (targetFound)
            {
                objective.targetLocation = bestTarget.position;
                TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindObjectiveTarget - Found creature %u at %.2f yards for quest %u",
                    bestTarget.entry, minDistance, objective.questId);
            }
            break;
        }

        case QuestObjectiveType::USE_GAMEOBJECT:
        case QuestObjectiveType::COLLECT_ITEM:
        {
            // Query game objects using lock-free spatial grid
            std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> nearbyObjects =
                spatialGrid->QueryNearbyGameObjects(bot->GetPosition(), searchRadius);

            float minDistance = searchRadius;
            DoubleBufferedSpatialGrid::GameObjectSnapshot bestTarget;

            for (auto const& snapshot : nearbyObjects)
            {
                // Check if object matches objective requirements
                if (snapshot.entry != objective.targetId &&
                    std::find(objective.alternativeTargets.begin(),
                             objective.alternativeTargets.end(),
                             snapshot.entry) == objective.alternativeTargets.end())
                    continue;

                if (!snapshot.isSpawned)
                    continue;

                // Check if object is interactable for collection
                if (objective.type == QuestObjectiveType::COLLECT_ITEM)
                {
                    if (snapshot.goType != GAMEOBJECT_TYPE_CHEST &&
                        snapshot.goType != GAMEOBJECT_TYPE_GOOBER &&
                        snapshot.goType != GAMEOBJECT_TYPE_QUESTGIVER)
                        continue;
                }

                float distance = bot->GetExactDist(snapshot.position);
                if (distance < minDistance)
                {
                    minDistance = distance;
                    bestTarget = snapshot;
                    targetFound = true;
                }
            }

            if (targetFound)
            {
                objective.targetLocation = bestTarget.position;
                TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindObjectiveTarget - Found game object %u at %.2f yards for quest %u",
                    bestTarget.entry, minDistance, objective.questId);
            }
            break;
        }

        case QuestObjectiveType::REACH_LOCATION:
        {
            // For location objectives, the target is the location itself
            if (objective.targetLocation.GetPositionX() != 0)
            {
                targetFound = true;
                TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindObjectiveTarget - Location objective has target position for quest %u",
                    objective.questId);
            }
            break;
        }

        default:
            TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindObjectiveTarget - Unsupported objective type %u for quest %u",
                static_cast<uint32>(objective.type), objective.questId);
            break;
    }

    return targetFound;
}

// 2. CoordinateGroupQuestCompletion - Coordinate quest completion within a group
void QuestCompletion::CoordinateGroupQuestCompletion(Group* group, uint32 questId)
{
    if (!group || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::CoordinateGroupQuestCompletion - Invalid parameters");
        return;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::CoordinateGroupQuestCompletion - Quest %u not found", questId);
        return;
    }

    // Track group members with this quest
    std::vector<Player*> membersWithQuest;
    std::unordered_map<uint32, QuestProgressData*> memberProgress;

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || !member->IsAlive())
            continue;

        // Check if member has the quest
        if (member->GetQuestStatus(questId) != QUEST_STATUS_NONE &&
            member->GetQuestStatus(questId) != QUEST_STATUS_REWARDED)
        {
            membersWithQuest.push_back(member);

            // Get member's quest progress
            auto it = _botQuestProgress.find(member->GetGUID().GetCounter());
            if (it != _botQuestProgress.end())
            {
                auto questIt = std::find_if(it->second.begin(), it->second.end(),
                    [questId](const QuestProgressData& data) { return data.questId == questId; });
                if (questIt != it->second.end())
                {
                    memberProgress[member->GetGUID().GetCounter()] = &(*questIt);
                }
            }
        }
    }

    if (membersWithQuest.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::CoordinateGroupQuestCompletion - No group members have quest %u", questId);
        return;
    }

    // Analyze quest objectives to determine coordination strategy
    QuestObjectives const& objectives = quest->GetObjectives();
    bool hasSharedObjectives = false;
    bool hasIndividualObjectives = false;

    for (auto const& objective : objectives)
    {
        if (objective.Type == QUEST_OBJECTIVE_MONSTER ||
            objective.Type == QUEST_OBJECTIVE_GAMEOBJECT)
        {
            hasSharedObjectives = true;
        }
        else if (objective.Type == QUEST_OBJECTIVE_ITEM)
        {
            hasIndividualObjectives = true;
        }
    }

    // Assign roles and objectives to group members
    if (hasSharedObjectives)
    {
        // For shared objectives, group members should work together
        Position centerPoint;
        uint32 count = 0;

        // Calculate center point of all members
        for (Player* member : membersWithQuest)
        {
            centerPoint.m_positionX += member->GetPositionX();
            centerPoint.m_positionY += member->GetPositionY();
            centerPoint.m_positionZ += member->GetPositionZ();
            count++;
        }

        if (count > 0)
        {
            centerPoint.m_positionX /= count;
            centerPoint.m_positionY /= count;
            centerPoint.m_positionZ /= count;
        }

        // Update group quest sharing data
        _groupQuestSharing[group->GetGUID().GetCounter()].push_back(questId);
        _groupObjectiveSync[group->GetGUID().GetCounter()][questId] = GameTime::GetGameTimeMS();

        // Notify members to converge
        for (Player* member : membersWithQuest)
        {
            auto progress = memberProgress[member->GetGUID().GetCounter()];
            if (progress)
            {
                progress->strategy = QuestCompletionStrategy::GROUP_COORDINATION;
                progress->completionLog.push_back("Coordinating with group at " +
                    std::to_string(GameTime::GetGameTimeMS()));
            }
        }

        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::CoordinateGroupQuestCompletion - Coordinating %u members for quest %u",
            static_cast<uint32>(membersWithQuest.size()), questId);
    }

    if (hasIndividualObjectives)
    {
        // For individual objectives, spread out to cover more area
        float spreadAngle = 2.0f * M_PI / membersWithQuest.size();
        float currentAngle = 0.0f;
        float spreadRadius = 30.0f;

        for (size_t i = 0; i < membersWithQuest.size(); ++i)
        {
            Player* member = membersWithQuest[i];
            auto progress = memberProgress[member->GetGUID().GetCounter()];
            if (progress)
            {
                // Assign search sectors to each member
                for (auto& objective : progress->objectives)
                {
                    if (objective.type == QuestObjectiveType::COLLECT_ITEM)
                    {
                        // Calculate sector position
                        objective.targetLocation.m_positionX = member->GetPositionX() +
                            spreadRadius * cos(currentAngle);
                        objective.targetLocation.m_positionY = member->GetPositionY() +
                            spreadRadius * sin(currentAngle);
                        objective.targetLocation.m_positionZ = member->GetPositionZ();
                        objective.searchRadius = spreadRadius;
                    }
                }
            }
            currentAngle += spreadAngle;
        }
    }
}

// 3. SynchronizeGroupObjectives - Synchronize objective progress across group
void QuestCompletion::SynchronizeGroupObjectives(Group* group, uint32 questId)
{
    if (!group || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::SynchronizeGroupObjectives - Invalid parameters");
        return;
    }

    // Collect objective progress from all group members
    std::unordered_map<uint32, uint32> objectiveMaxProgress; // objectiveIndex -> max progress
    std::unordered_map<uint32, Position> objectiveLocations;  // objectiveIndex -> best location

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member)
            continue;

        // Get member's quest progress
        auto it = _botQuestProgress.find(member->GetGUID().GetCounter());
        if (it == _botQuestProgress.end())
            continue;

        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt == it->second.end())
            continue;

        // Track best progress for each objective
        for (auto const& objective : questIt->objectives)
        {
            uint32 currentMax = objectiveMaxProgress[objective.objectiveIndex];
            if (objective.currentCount > currentMax)
            {
                objectiveMaxProgress[objective.objectiveIndex] = objective.currentCount;
                if (objective.targetLocation.GetPositionX() != 0)
                {
                    objectiveLocations[objective.objectiveIndex] = objective.targetLocation;
                }
            }
        }
    }

    // Synchronize progress across all members
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member)
            continue;

        auto it = _botQuestProgress.find(member->GetGUID().GetCounter());
        if (it == _botQuestProgress.end())
            continue;

        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt == it->second.end())
            continue;

        // Update objectives with synchronized data
        for (auto& objective : questIt->objectives)
        {
            // Share location information
            if (objective.targetLocation.GetPositionX() == 0 &&
                objectiveLocations.count(objective.objectiveIndex))
            {
                objective.targetLocation = objectiveLocations[objective.objectiveIndex];
            }

            // Mark objectives as requiring group if appropriate
            if (objective.type == QuestObjectiveType::KILL_CREATURE ||
                objective.type == QuestObjectiveType::ESCORT_NPC ||
                objective.type == QuestObjectiveType::DEFEND_AREA)
            {
                objective.requiresGroup = true;
            }
        }

        questIt->lastUpdateTime = GameTime::GetGameTimeMS();
        questIt->completionLog.push_back("Synchronized with group at " +
            std::to_string(GameTime::GetGameTimeMS()));
    }

    // Update synchronization timestamp
    _groupObjectiveSync[group->GetGUID().GetCounter()][questId] = GameTime::GetGameTimeMS();

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::SynchronizeGroupObjectives - Synchronized quest %u for group %u",
        questId, group->GetGUID().GetCounter());
}

// 4. HandleGroupObjectiveConflict - Resolve conflicts when multiple members target same objective
void QuestCompletion::HandleGroupObjectiveConflict(Group* group, uint32 questId, uint32 objectiveIndex)
{
    if (!group || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleGroupObjectiveConflict - Invalid parameters");
        return;
    }

    // Find all members targeting this objective
    std::vector<std::pair<Player*, float>> membersTargeting; // member, distance to objective

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || !member->IsAlive())
            continue;

        auto it = _botQuestProgress.find(member->GetGUID().GetCounter());
        if (it == _botQuestProgress.end())
            continue;

        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt == it->second.end() || objectiveIndex >= questIt->objectives.size())
            continue;

        auto& objective = questIt->objectives[objectiveIndex];
        if (objective.status == ObjectiveStatus::IN_PROGRESS)
        {
            float distance = member->GetDistance(objective.targetLocation);
            membersTargeting.push_back({member, distance});
        }
    }

    if (membersTargeting.size() <= 1)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleGroupObjectiveConflict - No conflict for quest %u objective %u",
            questId, objectiveIndex);
        return;
    }

    // Sort by distance (closest member gets priority)
    std::sort(membersTargeting.begin(), membersTargeting.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Assign primary member to continue, others find alternatives
    bool isPrimary = true;
    for (auto const& [member, distance] : membersTargeting)
    {
        auto it = _botQuestProgress.find(member->GetGUID().GetCounter());
        if (it == _botQuestProgress.end())
            continue;

        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt == it->second.end() || objectiveIndex >= questIt->objectives.size())
            continue;

        auto& objective = questIt->objectives[objectiveIndex];

        if (isPrimary)
        {
            // Primary member continues with objective
            objective.status = ObjectiveStatus::IN_PROGRESS;
            questIt->completionLog.push_back("Assigned as primary for objective " +
                std::to_string(objectiveIndex) + " at " + std::to_string(GameTime::GetGameTimeMS()));
            isPrimary = false;
        }
        else
        {
            // Other members find alternative targets or objectives
            objective.status = ObjectiveStatus::NOT_STARTED;
            objective.retryCount++;

            // Try to find alternative target
            if (!objective.alternativeTargets.empty())
            {
                // Rotate to next alternative target
                uint32 currentTarget = objective.targetId;
                auto altIt = std::find(objective.alternativeTargets.begin(),
                                       objective.alternativeTargets.end(), currentTarget);
                if (altIt != objective.alternativeTargets.end())
                {
                    ++altIt;
                    if (altIt != objective.alternativeTargets.end())
                    {
                        objective.targetId = *altIt;
                        FindObjectiveTarget(member, objective);
                    }
                }
            }
            else
            {
                // Move to next objective
                for (size_t i = 0; i < questIt->objectives.size(); ++i)
                {
                    if (i != objectiveIndex &&
                        questIt->objectives[i].status != ObjectiveStatus::COMPLETED)
                    {
                        _botCurrentObjective[member->GetGUID().GetCounter()] = i;
                        break;
                    }
                }
            }

            questIt->completionLog.push_back("Reassigned from conflicting objective " +
                std::to_string(objectiveIndex) + " at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleGroupObjectiveConflict - Resolved conflict for %u members on quest %u objective %u",
        static_cast<uint32>(membersTargeting.size()), questId, objectiveIndex);
}

// 5. OptimizeQuestCompletionOrder - Optimize the order of quest completion for efficiency
void QuestCompletion::OptimizeQuestCompletionOrder(Player* bot)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::OptimizeQuestCompletionOrder - null bot!");
        return;
    }

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end() || it->second.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::OptimizeQuestCompletionOrder - No quests to optimize for bot %s",
            bot->GetName().c_str());
        return;
    }

    // Create optimization criteria
    struct QuestOptimization
    {
        uint32 questId;
        float priority;
        Position centerLocation;
        float averageDistance;
        float completionProgress;
        uint32 estimatedTime;
        bool isGroupQuest;
        uint32 level;
    };

    std::vector<QuestOptimization> questOptimizations;

    // Analyze each quest
    for (auto& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
            continue;

        QuestOptimization opt;
        opt.questId = progress.questId;
        opt.completionProgress = progress.completionPercentage;
        opt.isGroupQuest = false;

        // Calculate center location of objectives
        float sumX = 0, sumY = 0, sumZ = 0;
        uint32 locationCount = 0;

        for (auto const& objective : progress.objectives)
        {
            if (objective.targetLocation.GetPositionX() != 0)
            {
                sumX += objective.targetLocation.GetPositionX();
                sumY += objective.targetLocation.GetPositionY();
                sumZ += objective.targetLocation.GetPositionZ();
                locationCount++;
            }

            if (objective.requiresGroup)
                opt.isGroupQuest = true;
        }

        if (locationCount > 0)
        {
            opt.centerLocation.m_positionX = sumX / locationCount;
            opt.centerLocation.m_positionY = sumY / locationCount;
            opt.centerLocation.m_positionZ = sumZ / locationCount;
            opt.averageDistance = bot->GetDistance(opt.centerLocation);
        }
        else
        {
            opt.averageDistance = 1000.0f; // High value for unknown locations
        }

        // Get quest level and calculate priority
        Quest const* quest = sObjectMgr->GetQuestTemplate(progress.questId);
        if (quest)
        {
            opt.level = quest->GetQuestLevel();

            // Priority calculation (lower is better)
            opt.priority = 0.0f;

            // Factor 1: Distance (30% weight)
            opt.priority += opt.averageDistance * 0.3f;

            // Factor 2: Completion progress (40% weight) - prioritize nearly complete quests
            opt.priority += (100.0f - opt.completionProgress) * 0.4f;

            // Factor 3: Level appropriateness (20% weight)
            int32 levelDiff = std::abs(static_cast<int32>(bot->GetLevel()) - static_cast<int32>(opt.level));
            opt.priority += levelDiff * 20.0f * 0.2f;

            // Factor 4: Group requirement (10% weight)
            if (opt.isGroupQuest && !bot->GetGroup())
                opt.priority += 100.0f * 0.1f;

            // Factor 5: Time spent (bonus for stuck quests)
            if (progress.isStuck)
                opt.priority += 200.0f; // Deprioritize stuck quests
        }

        opt.estimatedTime = progress.estimatedCompletionTime;
        questOptimizations.push_back(opt);
    }

    // Sort quests by priority
    std::sort(questOptimizations.begin(), questOptimizations.end(),
        [](const QuestOptimization& a, const QuestOptimization& b) {
            return a.priority < b.priority;
        });

    // Reorder quest progress based on optimization
    std::vector<QuestProgressData> optimizedProgress;
    for (auto const& opt : questOptimizations)
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [&opt](const QuestProgressData& data) { return data.questId == opt.questId; });

        if (questIt != it->second.end())
        {
            optimizedProgress.push_back(*questIt);
            optimizedProgress.back().completionLog.push_back(
                "Quest priority optimized to " + std::to_string(opt.priority) +
                " at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    // Replace with optimized order
    it->second = std::move(optimizedProgress);

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::OptimizeQuestCompletionOrder - Optimized %u quests for bot %s",
        static_cast<uint32>(questOptimizations.size()), bot->GetName().c_str());
}

// 6. OptimizeObjectiveSequence - Optimize the sequence of objectives within a quest
void QuestCompletion::OptimizeObjectiveSequence(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::OptimizeObjectiveSequence - Invalid parameters");
        return;
    }

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt == it->second.end())
        return;

    // Skip if quest is already complete
    if (questIt->completionPercentage >= 100.0f)
        return;

    // Collect incomplete objectives
    std::vector<std::pair<size_t, float>> objectivePriorities; // index, priority

    for (size_t i = 0; i < questIt->objectives.size(); ++i)
    {
        auto const& objective = questIt->objectives[i];
        if (objective.status == ObjectiveStatus::COMPLETED)
            continue;

        float priority = 0.0f;

        // Factor 1: Distance to objective (40% weight)
        if (objective.targetLocation.GetPositionX() != 0)
        {
            float distance = bot->GetDistance(objective.targetLocation);
            priority += distance * 0.4f;
        }
        else
        {
            priority += 100.0f * 0.4f; // Unknown location penalty
        }

        // Factor 2: Progress made (30% weight) - prioritize partially complete objectives
        if (objective.requiredCount > 0)
        {
            float progress = static_cast<float>(objective.currentCount) / objective.requiredCount;
            priority += (1.0f - progress) * 100.0f * 0.3f;
        }

        // Factor 3: Retry count (20% weight) - deprioritize frequently failed objectives
        priority += objective.retryCount * 20.0f * 0.2f;

        // Factor 4: Optional objectives (10% weight)
        if (objective.isOptional)
            priority += 50.0f * 0.1f;

        // Factor 5: Combat requirement
        if (objective.type == QuestObjectiveType::KILL_CREATURE)
        {
            // Check bot's combat readiness
            if (bot->GetHealthPct() < 70.0f)
                priority += 30.0f; // Delay combat objectives if low health
        }

        objectivePriorities.push_back({i, priority});
    }

    if (objectivePriorities.empty())
        return;

    // Sort objectives by priority
    std::sort(objectivePriorities.begin(), objectivePriorities.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Set the next objective to execute
    _botCurrentObjective[bot->GetGUID().GetCounter()] = objectivePriorities[0].first;

    // Log optimization
    questIt->completionLog.push_back(
        "Objective sequence optimized, next: " + std::to_string(objectivePriorities[0].first) +
        " at " + std::to_string(GameTime::GetGameTimeMS()));

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::OptimizeObjectiveSequence - Optimized %u objectives for quest %u, bot %s",
        static_cast<uint32>(objectivePriorities.size()), questId, bot->GetName().c_str());
}

// 7. FindEfficientCompletionPath - Find the most efficient path to complete multiple quests
void QuestCompletion::FindEfficientCompletionPath(Player* bot, const std::vector<uint32>& questIds)
{
    if (!bot || questIds.empty())
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::FindEfficientCompletionPath - Invalid parameters");
        return;
    }

    // Collect all objective locations from all quests
    struct ObjectiveNode
    {
        uint32 questId;
        uint32 objectiveIndex;
        Position location;
        QuestObjectiveType type;
        bool completed;
    };

    std::vector<ObjectiveNode> objectives;

    for (uint32 questId : questIds)
    {
        auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
        if (it == _botQuestProgress.end())
            continue;

        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt == it->second.end())
            continue;

        for (size_t i = 0; i < questIt->objectives.size(); ++i)
        {
            auto const& obj = questIt->objectives[i];
            if (obj.status != ObjectiveStatus::COMPLETED &&
                obj.targetLocation.GetPositionX() != 0)
            {
                objectives.push_back({
                    questId,
                    static_cast<uint32>(i),
                    obj.targetLocation,
                    obj.type,
                    false
                });
            }
        }
    }

    if (objectives.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindEfficientCompletionPath - No objectives with locations found");
        return;
    }

    // Use nearest neighbor algorithm for path finding (TSP approximation)
    std::vector<ObjectiveNode> orderedPath;
    Position currentPos = bot->GetPosition();
    std::unordered_set<size_t> visited;

    while (visited.size() < objectives.size())
    {
        float minDistance = std::numeric_limits<float>::max();
        size_t nearestIndex = 0;

        for (size_t i = 0; i < objectives.size(); ++i)
        {
            if (visited.count(i))
                continue;

            float distance = currentPos.GetExactDist(objectives[i].location);

            // Apply type-based clustering bonus
            if (!orderedPath.empty() && orderedPath.back().type == objectives[i].type)
                distance *= 0.9f; // 10% bonus for same type objectives

            // Apply quest-based clustering bonus
            if (!orderedPath.empty() && orderedPath.back().questId == objectives[i].questId)
                distance *= 0.85f; // 15% bonus for same quest objectives

            if (distance < minDistance)
            {
                minDistance = distance;
                nearestIndex = i;
            }
        }

        visited.insert(nearestIndex);
        orderedPath.push_back(objectives[nearestIndex]);
        currentPos = objectives[nearestIndex].location;
    }

    // Apply the optimized path to quest progress
    for (auto const& node : orderedPath)
    {
        auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
        if (it == _botQuestProgress.end())
            continue;

        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [&node](const QuestProgressData& data) { return data.questId == node.questId; });

        if (questIt != it->second.end())
        {
            questIt->completionLog.push_back(
                "Path optimized for objective " + std::to_string(node.objectiveIndex) +
                " at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    // Calculate total path distance
    float totalDistance = 0.0f;
    Position prevPos = bot->GetPosition();
    for (auto const& node : orderedPath)
    {
        totalDistance += prevPos.GetExactDist(node.location);
        prevPos = node.location;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindEfficientCompletionPath - Optimized path for %u objectives, total distance: %.2f",
        static_cast<uint32>(orderedPath.size()), totalDistance);
}

// 8. MinimizeTravelTime - Minimize travel time for completing objectives
void QuestCompletion::MinimizeTravelTime(Player* bot, const std::vector<QuestObjectiveData>& objectives)
{
    if (!bot || objectives.empty())
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::MinimizeTravelTime - Invalid parameters");
        return;
    }

    // Build distance matrix for all objectives
    size_t n = objectives.size();
    std::vector<std::vector<float>> distances(n + 1, std::vector<float>(n + 1, 0.0f));

    // Include bot's current position as starting point (index 0)
    Position botPos = bot->GetPosition();

    // Calculate distances from bot to each objective
    for (size_t i = 0; i < n; ++i)
    {
        if (objectives[i].targetLocation.GetPositionX() != 0)
        {
            distances[0][i + 1] = botPos.GetExactDist(objectives[i].targetLocation);
            distances[i + 1][0] = distances[0][i + 1];
        }
        else
        {
            distances[0][i + 1] = 1000.0f; // Penalty for unknown location
            distances[i + 1][0] = 1000.0f;
        }
    }

    // Calculate distances between objectives
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            if (objectives[i].targetLocation.GetPositionX() != 0 &&
                objectives[j].targetLocation.GetPositionX() != 0)
            {
                float dist = objectives[i].targetLocation.GetExactDist(objectives[j].targetLocation);
                distances[i + 1][j + 1] = dist;
                distances[j + 1][i + 1] = dist;
            }
            else
            {
                distances[i + 1][j + 1] = 1000.0f;
                distances[j + 1][i + 1] = 1000.0f;
            }
        }
    }

    // Use greedy nearest neighbor with look-ahead for better results
    std::vector<size_t> bestPath;
    float bestDistance = std::numeric_limits<float>::max();

    // Try different starting objectives
    for (size_t start = 0; start < std::min(n, size_t(3)); ++start)
    {
        std::vector<size_t> path;
        std::unordered_set<size_t> visited;
        size_t current = 0; // Start from bot position
        float totalDistance = 0.0f;

        // First move to starting objective
        if (start < n)
        {
            totalDistance += distances[current][start + 1];
            current = start + 1;
            path.push_back(start);
            visited.insert(start);
        }

        // Build rest of path
        while (visited.size() < n)
        {
            float minDist = std::numeric_limits<float>::max();
            size_t nextObj = 0;

            for (size_t i = 0; i < n; ++i)
            {
                if (visited.count(i))
                    continue;

                float dist = distances[current][i + 1];

                // Apply heuristics
                if (i < objectives.size())
                {
                    // Prioritize partially complete objectives
                    if (objectives[i].currentCount > 0 && objectives[i].requiredCount > 0)
                    {
                        float progress = static_cast<float>(objectives[i].currentCount) / objectives[i].requiredCount;
                        dist *= (1.0f - progress * 0.3f); // Up to 30% bonus for partial completion
                    }

                    // Group similar objective types
                    if (!path.empty() && path.back() < objectives.size())
                    {
                        if (objectives[path.back()].type == objectives[i].type)
                            dist *= 0.9f; // 10% bonus for same type
                        if (objectives[path.back()].questId == objectives[i].questId)
                            dist *= 0.85f; // 15% bonus for same quest
                    }
                }

                if (dist < minDist)
                {
                    minDist = dist;
                    nextObj = i;
                }
            }

            totalDistance += minDist;
            current = nextObj + 1;
            path.push_back(nextObj);
            visited.insert(nextObj);
        }

        if (totalDistance < bestDistance)
        {
            bestDistance = totalDistance;
            bestPath = path;
        }
    }

    // Calculate estimated travel time
    float travelSpeed = bot->GetSpeed(MOVE_RUN); // yards per second
    float estimatedTime = bestDistance / travelSpeed;

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::MinimizeTravelTime - Optimized path for %u objectives, distance: %.2f, time: %.2f seconds",
        static_cast<uint32>(bestPath.size()), bestDistance, estimatedTime);

    // Update bot's current objective based on optimized path
    if (!bestPath.empty() && bestPath[0] < objectives.size())
    {
        _botCurrentObjective[bot->GetGUID().GetCounter()] = bestPath[0];
    }
}

// 9. HandleStuckObjective - Handle an objective that the bot is stuck on
void QuestCompletion::HandleStuckObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleStuckObjective - null bot!");
        return;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleStuckObjective - Bot %s stuck on objective %u for quest %u",
        bot->GetName().c_str(), objective.objectiveIndex, objective.questId);

    // Increment retry count
    objective.retryCount++;
    objective.status = ObjectiveStatus::BLOCKED;

    // Strategy 1: Try alternative targets
    if (!objective.alternativeTargets.empty() && objective.retryCount <= 2)
    {
        // Rotate through alternative targets
        size_t altIndex = (objective.retryCount - 1) % objective.alternativeTargets.size();
        uint32 originalTarget = objective.targetId;
        objective.targetId = objective.alternativeTargets[altIndex];

        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleStuckObjective - Switching to alternative target %u",
            objective.targetId);

        // Try to find the new target
        if (FindObjectiveTarget(bot, objective))
        {
            objective.status = ObjectiveStatus::NOT_STARTED;
            return;
        }

        // Restore original target if alternative not found
        objective.targetId = originalTarget;
    }

    // Strategy 2: Expand search radius
    if (objective.retryCount >= 2 && objective.retryCount <= 4)
    {
        objective.searchRadius = std::min(objective.searchRadius * 1.5f, 200.0f);
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleStuckObjective - Expanding search radius to %.2f",
            objective.searchRadius);

        if (FindObjectiveTarget(bot, objective))
        {
            objective.status = ObjectiveStatus::NOT_STARTED;
            return;
        }
    }

    // Strategy 3: Request group assistance
    if (bot->GetGroup() && objective.retryCount >= 3 && objective.retryCount <= 5)
    {
        Group* group = bot->GetGroup();

        // Check if any group member has completed this objective
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (!member || member == bot)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(objective.questId);
            if (quest && objective.objectiveIndex < quest->GetObjectives().size())
            {
                if (member->IsQuestObjectiveComplete(objective.questId, objective.objectiveIndex))
                {
                    // Follow successful group member
                    objective.targetLocation = member->GetPosition();
                    objective.status = ObjectiveStatus::NOT_STARTED;
                    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleStuckObjective - Following group member %s",
                        member->GetName().c_str());
                    return;
                }
            }
        }
    }

    // Strategy 4: Skip optional objectives
    if (objective.isOptional && objective.retryCount >= 4)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleStuckObjective - Skipping optional objective");
        SkipProblematicObjective(bot, objective);
        return;
    }

    // Strategy 5: Mark as problematic and move to next objective
    if (objective.retryCount >= MAX_OBJECTIVE_RETRIES)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleStuckObjective - Max retries reached, skipping objective");
        objective.status = ObjectiveStatus::FAILED;

        // Update metrics
        _globalMetrics.stuckInstances++;
        _botMetrics[bot->GetGUID().GetCounter()].stuckInstances++;

        // Find next objective
        auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
        if (it != _botQuestProgress.end())
        {
            auto questIt = std::find_if(it->second.begin(), it->second.end(),
                [&objective](const QuestProgressData& data) { return data.questId == objective.questId; });

            if (questIt != it->second.end())
            {
                questIt->consecutiveFailures++;
                questIt->completionLog.push_back(
                    "Failed objective " + std::to_string(objective.objectiveIndex) +
                    " after " + std::to_string(objective.retryCount) + " retries at " +
                    std::to_string(GameTime::GetGameTimeMS()));

                // If too many failures, consider abandoning quest
                if (questIt->consecutiveFailures >= 3)
                {
                    DiagnoseCompletionIssues(bot, objective.questId);
                }
            }
        }
    }
}

// 10. SkipProblematicObjective - Skip an objective that cannot be completed
void QuestCompletion::SkipProblematicObjective(Player* bot, QuestObjectiveData& objective)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::SkipProblematicObjective - null bot!");
        return;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::SkipProblematicObjective - Bot %s skipping objective %u for quest %u",
        bot->GetName().c_str(), objective.objectiveIndex, objective.questId);

    // Mark objective as skipped
    objective.status = ObjectiveStatus::SKIPPED;

    // Log the skip
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [&objective](const QuestProgressData& data) { return data.questId == objective.questId; });

        if (questIt != it->second.end())
        {
            questIt->completionLog.push_back(
                "Skipped problematic objective " + std::to_string(objective.objectiveIndex) +
                " at " + std::to_string(GameTime::GetGameTimeMS()));

            // Check if quest can still be completed without this objective
            bool canComplete = true;
            for (auto const& obj : questIt->objectives)
            {
                if (obj.status != ObjectiveStatus::COMPLETED &&
                    obj.status != ObjectiveStatus::SKIPPED &&
                    !obj.isOptional)
                {
                    canComplete = false;
                    break;
                }
            }

            if (!canComplete)
            {
                // Find next available objective
                for (size_t i = 0; i < questIt->objectives.size(); ++i)
                {
                    if (questIt->objectives[i].status == ObjectiveStatus::NOT_STARTED)
                    {
                        _botCurrentObjective[bot->GetGUID().GetCounter()] = i;
                        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::SkipProblematicObjective - Moving to objective %u",
                            static_cast<uint32>(i));
                        break;
                    }
                }
            }
            else
            {
                // Quest can be completed, update percentage
                float completed = 0;
                float total = 0;
                for (auto const& obj : questIt->objectives)
                {
                    if (!obj.isOptional)
                    {
                        total++;
                        if (obj.status == ObjectiveStatus::COMPLETED)
                            completed++;
                    }
                }

                if (total > 0)
                    questIt->completionPercentage = (completed / total) * 100.0f;
            }
        }
    }
}

// Continue with remaining methods in next part...// Continue from previous file...

// 11. ProcessQuestTurnIn - Process quest turn-in for a bot
void QuestCompletion::ProcessQuestTurnIn(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::ProcessQuestTurnIn - Invalid parameters");
        return;
    }

    // Check if quest is ready for turn-in
    if (!bot->CanCompleteQuest(questId))
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ProcessQuestTurnIn - Bot %s cannot turn in quest %u yet",
            bot->GetName().c_str(), questId);
        return;
    }

    // Find quest giver
    if (!FindQuestTurnInNpc(bot, questId))
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ProcessQuestTurnIn - Cannot find turn-in NPC for quest %u",
            questId);
        return;
    }

    // Get quest progress data
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt != it->second.end())
        {
            // Find quest giver NPC
            Creature* questGiver = nullptr;
            if (questIt->questGiverGuid)
            {
                questGiver = ObjectAccessor::GetCreature(*bot, ObjectGuid::Create<HighGuid::Unit>(questIt->questGiverGuid));
            }

            if (questGiver)
            {
                // Check distance to quest giver
                if (bot->GetDistance(questGiver) > QUEST_GIVER_INTERACTION_RANGE)
                {
                    // Move to quest giver
                    BotMovementUtil::MoveToUnit(bot, questGiver, QUEST_GIVER_INTERACTION_RANGE - 1.0f);
                    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ProcessQuestTurnIn - Moving to quest giver %s",
                        questGiver->GetName().c_str());
                    return;
                }

                // Complete quest dialog
                CompleteQuestDialog(bot, questId);

                // Handle reward selection
                HandleQuestRewardSelection(bot, questId);

                // Mark quest as completed in our tracking
                questIt->completionPercentage = 100.0f;
                questIt->requiresTurnIn = false;
                questIt->completionLog.push_back(
                    "Quest turned in at " + std::to_string(GameTime::GetGameTimeMS()));

                // Update metrics
                _globalMetrics.questsCompleted++;
                _botMetrics[bot->GetGUID().GetCounter()].questsCompleted++;

                TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ProcessQuestTurnIn - Bot %s turned in quest %u to %s",
                    bot->GetName().c_str(), questId, questGiver->GetName().c_str());
            }
            else
            {
                TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ProcessQuestTurnIn - Quest giver not found for quest %u",
                    questId);
            }
        }
    }
}

// 12. FindQuestTurnInNpc - Find the NPC to turn in a quest
bool QuestCompletion::FindQuestTurnInNpc(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::FindQuestTurnInNpc - Invalid parameters");
        return false;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::FindQuestTurnInNpc - Quest %u not found", questId);
        return false;
    }

    Map* map = bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // Search for quest enders in range
    std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> nearbyCreatures =
        spatialGrid->QueryNearbyCreatures(bot->GetPosition(), 100.0f);

    Creature* bestQuestGiver = nullptr;
    float minDistance = 100.0f;

    for (auto const& snapshot : nearbyCreatures)
    {
        if (snapshot.isDead || !snapshot.isVisible)
            continue;

        // Check if this creature can end the quest
        Creature* creature = ObjectAccessor::GetCreature(*bot, snapshot.guid);
        if (!creature)
            continue;

        // Check if creature can complete this quest
        if (creature->AI() && creature->IsQuestGiver())
        {
            // Check if this NPC can complete the quest
            // Note: In modern TrinityCore, quest giver information is stored differently
            // This is a simplified check - actual implementation would query quest_template tables
            float distance = bot->GetExactDist(snapshot.position);
            if (distance < minDistance)
            {
                minDistance = distance;
                bestQuestGiver = creature;
            }
        }
    }

    if (bestQuestGiver)
    {
        // Store quest giver info
        auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
        if (it != _botQuestProgress.end())
        {
            auto questIt = std::find_if(it->second.begin(), it->second.end(),
                [questId](const QuestProgressData& data) { return data.questId == questId; });

            if (questIt != it->second.end())
            {
                questIt->questGiverGuid = bestQuestGiver->GetGUID().GetCounter();
                questIt->questGiverLocation = bestQuestGiver->GetPosition();
            }
        }

        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindQuestTurnInNpc - Found quest giver %s at %.2f yards for quest %u",
            bestQuestGiver->GetName().c_str(), minDistance, questId);
        return true;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::FindQuestTurnInNpc - No quest giver found for quest %u",
        questId);
    return false;
}

// 13. HandleQuestRewardSelection - Handle quest reward selection
void QuestCompletion::HandleQuestRewardSelection(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleQuestRewardSelection - Invalid parameters");
        return;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return;

    // Analyze available rewards
    struct RewardOption
    {
        uint32 itemId;
        float value;
        ItemTemplate const* itemTemplate;
    };

    std::vector<RewardOption> rewards;

    // Check reward choice items
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        if (quest->RewardChoiceItemId[i] == 0)
            continue;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->RewardChoiceItemId[i]);
        if (!itemTemplate)
            continue;

        RewardOption option;
        option.itemId = quest->RewardChoiceItemId[i];
        option.itemTemplate = itemTemplate;
        option.value = 0.0f;

        // Calculate item value based on bot's class and spec
        uint32 botClass = bot->getClass();

        // Check if item is usable by class
        if (!(itemTemplate->AllowableClass & (1 << (botClass - 1))))
        {
            option.value -= 1000.0f; // Heavy penalty for unusable items
        }
        else
        {
            // Calculate value based on stats
            option.value += itemTemplate->ItemLevel * 10.0f;

            // Add value for primary stats based on class
            switch (botClass)
            {
                case CLASS_WARRIOR:
                case CLASS_PALADIN:
                case CLASS_DEATH_KNIGHT:
                    // Prioritize strength and stamina
                    for (uint32 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
                    {
                        if (itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_STRENGTH)
                            option.value += itemTemplate->ItemStat[j].ItemStatValue * 2.0f;
                        if (itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_STAMINA)
                            option.value += itemTemplate->ItemStat[j].ItemStatValue * 1.5f;
                    }
                    break;

                case CLASS_HUNTER:
                case CLASS_ROGUE:
                    // Prioritize agility
                    for (uint32 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
                    {
                        if (itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_AGILITY)
                            option.value += itemTemplate->ItemStat[j].ItemStatValue * 2.0f;
                    }
                    break;

                case CLASS_PRIEST:
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                    // Prioritize intellect
                    for (uint32 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
                    {
                        if (itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_INTELLECT)
                            option.value += itemTemplate->ItemStat[j].ItemStatValue * 2.0f;
                    }
                    break;

                case CLASS_SHAMAN:
                case CLASS_DRUID:
                    // Hybrid classes - check current spec/role
                    for (uint32 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
                    {
                        if (itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_INTELLECT ||
                            itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_AGILITY ||
                            itemTemplate->ItemStat[j].ItemStatType == ITEM_MOD_STRENGTH)
                            option.value += itemTemplate->ItemStat[j].ItemStatValue * 1.5f;
                    }
                    break;
            }

            // Check if it's an upgrade
            Item* currentItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, itemTemplate->InventoryType);
            if (currentItem)
            {
                ItemTemplate const* currentTemplate = currentItem->GetTemplate();
                if (currentTemplate && currentTemplate->ItemLevel >= itemTemplate->ItemLevel)
                {
                    option.value -= 500.0f; // Penalty for non-upgrade
                }
            }
        }

        rewards.push_back(option);
    }

    // Select best reward
    if (!rewards.empty())
    {
        auto bestReward = std::max_element(rewards.begin(), rewards.end(),
            [](const RewardOption& a, const RewardOption& b) {
                return a.value < b.value;
            });

        uint32 rewardIndex = std::distance(rewards.begin(), bestReward);

        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleQuestRewardSelection - Bot %s selected reward %u (item %u) for quest %u",
            bot->GetName().c_str(), rewardIndex, bestReward->itemId, questId);

        // The actual reward selection would be handled through the quest system
        // bot->RewardQuest(quest, rewardIndex, true);
    }
    else
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleQuestRewardSelection - No reward choices for quest %u",
            questId);
    }
}

// 14. CompleteQuestDialog - Complete quest dialog interactions
void QuestCompletion::CompleteQuestDialog(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::CompleteQuestDialog - Invalid parameters");
        return;
    }

    // Get quest progress
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt == it->second.end())
        return;

    // Find quest giver
    Creature* questGiver = nullptr;
    if (questIt->questGiverGuid)
    {
        questGiver = ObjectAccessor::GetCreature(*bot, ObjectGuid::Create<HighGuid::Unit>(questIt->questGiverGuid));
    }

    if (!questGiver)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::CompleteQuestDialog - Quest giver not found for quest %u",
            questId);
        return;
    }

    // Simulate dialog interaction sequence
    // 1. Open quest complete dialog
    bot->PlayerTalkClass->SendQuestGiverRequestItems(sObjectMgr->GetQuestTemplate(questId), questGiver->GetGUID(), true);

    // 2. Complete quest
    bot->CompleteQuest(questId);

    // 3. Send quest complete
    bot->PlayerTalkClass->SendQuestGiverQuestComplete(sObjectMgr->GetQuestTemplate(questId), questGiver->GetGUID(), true);

    // Log completion
    questIt->completionLog.push_back(
        "Quest dialog completed with " + questGiver->GetName() +
        " at " + std::to_string(GameTime::GetGameTimeMS()));

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::CompleteQuestDialog - Bot %s completed quest %u dialog with %s",
        bot->GetName().c_str(), questId, questGiver->GetName().c_str());
}

// 15. GetActiveQuests - Get list of active quests for a bot
std::vector<uint32> QuestCompletion::GetActiveQuests(Player* bot)
{
    std::vector<uint32> activeQuests;

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::GetActiveQuests - null bot!");
        return activeQuests;
    }

    // Check bot's quest log
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        QuestStatus status = bot->GetQuestStatus(questId);
        if (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_FAILED)
        {
            activeQuests.push_back(questId);
        }
    }

    // Also check our tracking data
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        for (auto const& progress : it->second)
        {
            if (progress.completionPercentage < 100.0f)
            {
                // Add if not already in list
                if (std::find(activeQuests.begin(), activeQuests.end(), progress.questId) == activeQuests.end())
                {
                    activeQuests.push_back(progress.questId);
                }
            }
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::GetActiveQuests - Bot %s has %u active quests",
        bot->GetName().c_str(), static_cast<uint32>(activeQuests.size()));

    return activeQuests;
}

// 16. GetCompletableQuests - Get list of quests ready to turn in
std::vector<uint32> QuestCompletion::GetCompletableQuests(Player* bot)
{
    std::vector<uint32> completableQuests;

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::GetCompletableQuests - null bot!");
        return completableQuests;
    }

    // Check bot's quest log for complete quests
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = bot->GetQuestSlotQuestId(slot);
        if (questId == 0)
            continue;

        if (bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
        {
            completableQuests.push_back(questId);
        }
        else if (bot->CanCompleteQuest(questId))
        {
            // Double-check if quest can be completed
            completableQuests.push_back(questId);
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::GetCompletableQuests - Bot %s has %u completable quests",
        bot->GetName().c_str(), static_cast<uint32>(completableQuests.size()));

    return completableQuests;
}

// 17. GetHighestPriorityQuest - Get the highest priority quest to work on
uint32 QuestCompletion::GetHighestPriorityQuest(Player* bot)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::GetHighestPriorityQuest - null bot!");
        return 0;
    }

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end() || it->second.empty())
        return 0;

    // Evaluate each quest's priority
    uint32 bestQuest = 0;
    float bestPriority = std::numeric_limits<float>::max();

    for (auto const& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
            continue;

        float priority = 0.0f;

        // Factor 1: Completion percentage (40% weight) - prioritize nearly complete
        priority += (100.0f - progress.completionPercentage) * 0.4f;

        // Factor 2: Time investment (20% weight) - prioritize quests we've spent time on
        float timeInvestment = static_cast<float>(progress.timeSpent) / 60000.0f; // minutes
        priority += std::max(0.0f, 30.0f - timeInvestment) * 0.2f; // favor quests with time invested

        // Factor 3: Quest level (20% weight)
        Quest const* quest = sObjectMgr->GetQuestTemplate(progress.questId);
        if (quest)
        {
            int32 levelDiff = std::abs(static_cast<int32>(bot->GetLevel()) - static_cast<int32>(quest->GetQuestLevel()));
            priority += levelDiff * 0.2f;
        }

        // Factor 4: Stuck status (10% weight)
        if (progress.isStuck)
            priority += 100.0f * 0.1f;

        // Factor 5: Group quest without group (10% weight)
        bool isGroupQuest = std::any_of(progress.objectives.begin(), progress.objectives.end(),
            [](const QuestObjectiveData& obj) { return obj.requiresGroup; });
        if (isGroupQuest && !bot->GetGroup())
            priority += 50.0f * 0.1f;

        if (priority < bestPriority)
        {
            bestPriority = priority;
            bestQuest = progress.questId;
        }
    }

    if (bestQuest)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::GetHighestPriorityQuest - Bot %s highest priority quest: %u (priority: %.2f)",
            bot->GetName().c_str(), bestQuest, bestPriority);
    }

    return bestQuest;
}

// 18. CalculateQuestProgress - Calculate completion percentage for a quest
float QuestCompletion::CalculateQuestProgress(uint32 questId, Player* bot)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::CalculateQuestProgress - Invalid parameters");
        return 0.0f;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return 0.0f;

    // Check if quest is complete
    if (bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE ||
        bot->GetQuestStatus(questId) == QUEST_STATUS_REWARDED)
        return 100.0f;

    // Calculate progress from objectives
    QuestObjectives const& objectives = quest->GetObjectives();
    if (objectives.empty())
        return 0.0f;

    float totalProgress = 0.0f;
    uint32 objectiveCount = 0;

    for (auto const& objective : objectives)
    {
        if (objective.Amount <= 0)
            continue;

        uint32 currentCount = bot->GetQuestObjectiveData(objective);
        float objectiveProgress = std::min(1.0f,
            static_cast<float>(currentCount) / static_cast<float>(objective.Amount));

        totalProgress += objectiveProgress;
        objectiveCount++;
    }

    float questProgress = objectiveCount > 0 ? (totalProgress / objectiveCount) * 100.0f : 0.0f;

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::CalculateQuestProgress - Quest %u progress for bot %s: %.2f%%",
        questId, bot->GetName().c_str(), questProgress);

    return questProgress;
}

// 19. SetMaxConcurrentQuests - Set maximum concurrent quests for a bot
void QuestCompletion::SetMaxConcurrentQuests(uint32 botGuid, uint32 maxQuests)
{
    if (!botGuid || maxQuests == 0)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::SetMaxConcurrentQuests - Invalid parameters");
        return;
    }

    // Store configuration (would be used in quest pickup logic)
    // For now, we'll limit active quest tracking
    auto it = _botQuestProgress.find(botGuid);
    if (it != _botQuestProgress.end())
    {
        // If we have more quests than the limit, prioritize them
        if (it->second.size() > maxQuests)
        {
            // Sort by priority (completion percentage and time investment)
            std::sort(it->second.begin(), it->second.end(),
                [](const QuestProgressData& a, const QuestProgressData& b) {
                    // Prioritize nearly complete quests
                    if (std::abs(a.completionPercentage - b.completionPercentage) > 20.0f)
                        return a.completionPercentage > b.completionPercentage;

                    // Then prioritize by time investment
                    return a.timeSpent > b.timeSpent;
                });

            // Keep only the top maxQuests
            if (it->second.size() > maxQuests)
            {
                it->second.resize(maxQuests);
            }
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::SetMaxConcurrentQuests - Set max quests to %u for bot %u",
        maxQuests, botGuid);
}

// 20. EnableGroupCoordination - Enable or disable group coordination for a bot
void QuestCompletion::EnableGroupCoordination(uint32 botGuid, bool enable)
{
    if (!botGuid)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::EnableGroupCoordination - Invalid bot GUID");
        return;
    }

    // Update strategy for all quests
    auto it = _botQuestProgress.find(botGuid);
    if (it != _botQuestProgress.end())
    {
        for (auto& progress : it->second)
        {
            if (enable)
            {
                // Check if bot is in a group
                Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botGuid));
                if (bot && bot->GetGroup())
                {
                    progress.strategy = QuestCompletionStrategy::GROUP_COORDINATION;
                    progress.completionLog.push_back(
                        "Group coordination enabled at " + std::to_string(GameTime::GetGameTimeMS()));
                }
            }
            else
            {
                if (progress.strategy == QuestCompletionStrategy::GROUP_COORDINATION)
                {
                    progress.strategy = QuestCompletionStrategy::EFFICIENT_COMPLETION;
                    progress.completionLog.push_back(
                        "Group coordination disabled at " + std::to_string(GameTime::GetGameTimeMS()));
                }
            }
        }
    }

    // Update bot strategy preference
    if (enable)
        _botStrategies[botGuid] = QuestCompletionStrategy::GROUP_COORDINATION;
    else if (_botStrategies[botGuid] == QuestCompletionStrategy::GROUP_COORDINATION)
        _botStrategies[botGuid] = QuestCompletionStrategy::EFFICIENT_COMPLETION;

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::EnableGroupCoordination - %s group coordination for bot %u",
        enable ? "Enabled" : "Disabled", botGuid);
}// Continue from previous file...

// 21. HandleDungeonQuests - Handle dungeon-specific quest objectives
void QuestCompletion::HandleDungeonQuests(Player* bot, uint32 dungeonId)
{
    if (!bot || !dungeonId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleDungeonQuests - Invalid parameters");
        return;
    }

    // Get all active quests for this dungeon
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    std::vector<QuestProgressData*> dungeonQuests;

    for (auto& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(progress.questId);
        if (!quest)
            continue;

        // Check if quest is for this dungeon
        // In TrinityCore, dungeon quests are often identified by zone or special flags
        bool isDungeonQuest = false;

        // Check objectives for dungeon-specific targets
        for (auto const& objective : progress.objectives)
        {
            if (objective.type == QuestObjectiveType::COMPLETE_DUNGEON ||
                objective.type == QuestObjectiveType::KILL_CREATURE ||
                objective.type == QuestObjectiveType::COLLECT_ITEM)
            {
                // Check if target is in dungeon (simplified check)
                // Real implementation would check creature/item spawn locations
                isDungeonQuest = true;
                break;
            }
        }

        if (isDungeonQuest)
        {
            dungeonQuests.push_back(&progress);
        }
    }

    if (dungeonQuests.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleDungeonQuests - No dungeon quests for dungeon %u",
            dungeonId);
        return;
    }

    // Coordinate with group for dungeon quests
    if (bot->GetGroup())
    {
        Group* group = bot->GetGroup();

        for (auto* progress : dungeonQuests)
        {
            // Set strategy to group coordination
            progress->strategy = QuestCompletionStrategy::GROUP_COORDINATION;

            // Share quest with group
            CoordinateGroupQuestCompletion(group, progress->questId);

            // Mark dungeon objectives
            for (auto& objective : progress->objectives)
            {
                if (objective.type == QuestObjectiveType::KILL_CREATURE ||
                    objective.type == QuestObjectiveType::COMPLETE_DUNGEON)
                {
                    objective.requiresGroup = true;

                    // Expand search radius for dungeon
                    objective.searchRadius = 200.0f;
                }
            }

            progress->completionLog.push_back(
                "Handling dungeon quest for dungeon " + std::to_string(dungeonId) +
                " at " + std::to_string(GameTime::GetGameTimeMS()));
        }

        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleDungeonQuests - Handling %u dungeon quests for bot %s in dungeon %u",
            static_cast<uint32>(dungeonQuests.size()), bot->GetName().c_str(), dungeonId);
    }
    else
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleDungeonQuests - Bot %s not in group for dungeon %u",
            bot->GetName().c_str(), dungeonId);

        // Mark dungeon quests as blocked if not in group
        for (auto* progress : dungeonQuests)
        {
            for (auto& objective : progress->objectives)
            {
                if (objective.requiresGroup)
                {
                    objective.status = ObjectiveStatus::BLOCKED;
                }
            }

            progress->completionLog.push_back(
                "Dungeon quest blocked (no group) for dungeon " + std::to_string(dungeonId) +
                " at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }
}

// 22. HandlePvPQuests - Handle PvP-specific quest objectives
void QuestCompletion::HandlePvPQuests(Player* bot, uint32 battlegroundId)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandlePvPQuests - null bot!");
        return;
    }

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    std::vector<QuestProgressData*> pvpQuests;

    // Find PvP quests
    for (auto& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
            continue;

        bool isPvPQuest = false;

        for (auto const& objective : progress.objectives)
        {
            if (objective.type == QuestObjectiveType::WIN_BATTLEGROUND ||
                objective.targetId == battlegroundId)
            {
                isPvPQuest = true;
                break;
            }
        }

        if (isPvPQuest)
        {
            pvpQuests.push_back(&progress);
        }
    }

    if (pvpQuests.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandlePvPQuests - No PvP quests for battleground %u",
            battlegroundId);
        return;
    }

    // Handle PvP quest objectives
    for (auto* progress : pvpQuests)
    {
        // Set PvP-specific strategy
        progress->strategy = QuestCompletionStrategy::SPEED_COMPLETION;

        for (auto& objective : progress->objectives)
        {
            if (objective.type == QuestObjectiveType::WIN_BATTLEGROUND)
            {
                // Check if in battleground
                if (bot->InBattleground())
                {
                    objective.status = ObjectiveStatus::IN_PROGRESS;

                    // Update based on battleground outcome
                    // This would be called when battleground ends
                    if (bot->GetBattleground())
                    {
                        // Simplified check - real implementation would track BG results
                        objective.currentCount++;
                        if (objective.currentCount >= objective.requiredCount)
                        {
                            objective.status = ObjectiveStatus::COMPLETED;
                            _globalMetrics.objectivesCompleted++;
                        }
                    }
                }
                else
                {
                    // Queue for battleground
                    objective.status = ObjectiveStatus::NOT_STARTED;
                    progress->completionLog.push_back(
                        "Queuing for battleground " + std::to_string(battlegroundId) +
                        " at " + std::to_string(GameTime::GetGameTimeMS()));
                }
            }
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandlePvPQuests - Handling %u PvP quests for bot %s",
        static_cast<uint32>(pvpQuests.size()), bot->GetName().c_str());
}

// 23. HandleSeasonalQuests - Handle seasonal/event quest objectives
void QuestCompletion::HandleSeasonalQuests(Player* bot)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleSeasonalQuests - null bot!");
        return;
    }

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    // Get current game events
    time_t gameTime = GameTime::GetGameTime();
    struct tm timeinfo;
    localtime_r(&gameTime, &timeinfo);

    std::vector<QuestProgressData*> seasonalQuests;

    for (auto& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(progress.questId);
        if (!quest)
            continue;

        // Check if quest is seasonal
        // In TrinityCore, seasonal quests often have special flags or are tied to events
        bool isSeasonal = false;

        // Check for holiday/event flags (simplified check)
        // Real implementation would check game_event_quest table
        if (quest->GetFlags() & QUEST_FLAGS_SPECIAL_MONTHLY)
        {
            isSeasonal = true;
        }

        if (isSeasonal)
        {
            seasonalQuests.push_back(&progress);
        }
    }

    if (seasonalQuests.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleSeasonalQuests - No seasonal quests for bot %s",
            bot->GetName().c_str());
        return;
    }

    // Handle seasonal quest objectives with special considerations
    for (auto* progress : seasonalQuests)
    {
        // Prioritize seasonal quests as they are time-limited
        progress->strategy = QuestCompletionStrategy::SPEED_COMPLETION;

        // Check if event is active
        bool eventActive = true; // Simplified - would check game_event table

        if (!eventActive)
        {
            // Mark objectives as blocked if event is not active
            for (auto& objective : progress->objectives)
            {
                objective.status = ObjectiveStatus::BLOCKED;
            }

            progress->completionLog.push_back(
                "Seasonal quest blocked (event inactive) at " + std::to_string(GameTime::GetGameTimeMS()));
        }
        else
        {
            // Process objectives with urgency
            for (auto& objective : progress->objectives)
            {
                if (objective.status != ObjectiveStatus::COMPLETED)
                {
                    // Increase search radius for seasonal items/npcs
                    objective.searchRadius = std::min(objective.searchRadius * 1.5f, 150.0f);

                    // Mark as high priority
                    ExecuteObjective(bot, objective);
                }
            }

            progress->completionLog.push_back(
                "Processing seasonal quest at " + std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleSeasonalQuests - Handling %u seasonal quests for bot %s",
        static_cast<uint32>(seasonalQuests.size()), bot->GetName().c_str());
}

// 24. HandleDailyQuests - Handle daily quest objectives
void QuestCompletion::HandleDailyQuests(Player* bot)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleDailyQuests - null bot!");
        return;
    }

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    std::vector<QuestProgressData*> dailyQuests;
    uint32 currentTime = GameTime::GetGameTimeMS();

    for (auto& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(progress.questId);
        if (!quest)
            continue;

        // Check if quest is daily
        if (quest->IsDaily())
        {
            dailyQuests.push_back(&progress);
        }
    }

    if (dailyQuests.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleDailyQuests - No daily quests for bot %s",
            bot->GetName().c_str());
        return;
    }

    // Sort daily quests by priority
    std::sort(dailyQuests.begin(), dailyQuests.end(),
        [currentTime](const QuestProgressData* a, const QuestProgressData* b) {
            // Prioritize by time remaining (daily reset)
            uint32 timeSinceStartA = currentTime - a->startTime;
            uint32 timeSinceStartB = currentTime - b->startTime;

            // Assume daily reset is at midnight (simplified)
            uint32 timeUntilResetA = 86400000 - (timeSinceStartA % 86400000); // ms in a day
            uint32 timeUntilResetB = 86400000 - (timeSinceStartB % 86400000);

            // Prioritize quests closer to reset
            if (timeUntilResetA < 7200000 || timeUntilResetB < 7200000) // 2 hours
            {
                return timeUntilResetA < timeUntilResetB;
            }

            // Otherwise prioritize by completion percentage
            return a->completionPercentage > b->completionPercentage;
        });

    // Process daily quests
    for (auto* progress : dailyQuests)
    {
        // Set appropriate strategy for daily quests
        uint32 timeSinceStart = currentTime - progress->startTime;
        uint32 timeUntilReset = 86400000 - (timeSinceStart % 86400000);

        if (timeUntilReset < 3600000) // Less than 1 hour until reset
        {
            progress->strategy = QuestCompletionStrategy::SPEED_COMPLETION;
            progress->completionLog.push_back(
                "Urgent: Daily quest reset in " + std::to_string(timeUntilReset / 60000) +
                " minutes at " + std::to_string(currentTime));
        }
        else
        {
            progress->strategy = QuestCompletionStrategy::EFFICIENT_COMPLETION;
        }

        // Process objectives
        for (auto& objective : progress->objectives)
        {
            if (objective.status != ObjectiveStatus::COMPLETED)
            {
                ExecuteObjective(bot, objective);
            }
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleDailyQuests - Processing %u daily quests for bot %s",
        static_cast<uint32>(dailyQuests.size()), bot->GetName().c_str());
}

// 25. HandleQuestCompletionError - Handle errors during quest completion
void QuestCompletion::HandleQuestCompletionError(Player* bot, uint32 questId, const std::string& error)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleQuestCompletionError - Invalid parameters");
        return;
    }

    TC_LOG_ERROR("playerbot.quest", "QuestCompletion::HandleQuestCompletionError - Bot %s quest %u: %s",
        bot->GetName().c_str(), questId, error.c_str());

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt == it->second.end())
        return;

    // Log the error
    questIt->completionLog.push_back(
        "ERROR: " + error + " at " + std::to_string(GameTime::GetGameTimeMS()));

    // Increment failure count
    questIt->consecutiveFailures++;

    // Analyze error type and take appropriate action
    if (error.find("target not found") != std::string::npos)
    {
        // Expand search radius or find alternative targets
        for (auto& objective : questIt->objectives)
        {
            if (objective.status == ObjectiveStatus::IN_PROGRESS)
            {
                objective.searchRadius = std::min(objective.searchRadius * 1.5f, 200.0f);
                objective.status = ObjectiveStatus::NOT_STARTED;
                objective.retryCount++;
            }
        }
    }
    else if (error.find("level") != std::string::npos || error.find("requirement") != std::string::npos)
    {
        // Quest has requirements bot doesn't meet
        questIt->isStuck = true;
        questIt->stuckTime = GameTime::GetGameTimeMS();

        // Mark all objectives as blocked
        for (auto& objective : questIt->objectives)
        {
            if (objective.status != ObjectiveStatus::COMPLETED)
            {
                objective.status = ObjectiveStatus::BLOCKED;
            }
        }
    }
    else if (error.find("inventory") != std::string::npos || error.find("full") != std::string::npos)
    {
        // Inventory issues
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleQuestCompletionError - Inventory full for bot %s",
            bot->GetName().c_str());

        // Would trigger inventory management here
        questIt->isStuck = true;
    }
    else if (error.find("group") != std::string::npos)
    {
        // Group requirement
        for (auto& objective : questIt->objectives)
        {
            objective.requiresGroup = true;
            if (!bot->GetGroup())
            {
                objective.status = ObjectiveStatus::BLOCKED;
            }
        }
    }

    // If too many consecutive failures, consider abandoning
    if (questIt->consecutiveFailures >= 5)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::HandleQuestCompletionError - Too many failures, considering abandon for quest %u",
            questId);
        DiagnoseCompletionIssues(bot, questId);
    }

    // Update metrics
    _globalMetrics.questsFailed++;
    _botMetrics[bot->GetGUID().GetCounter()].questsFailed++;

    // Try recovery
    RecoverFromCompletionFailure(bot, questId);
}

// 26. RecoverFromCompletionFailure - Attempt to recover from quest completion failure
void QuestCompletion::RecoverFromCompletionFailure(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::RecoverFromCompletionFailure - Invalid parameters");
        return;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::RecoverFromCompletionFailure - Attempting recovery for bot %s quest %u",
        bot->GetName().c_str(), questId);

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    auto questIt = std::find_if(it->second.begin(), it->second.end(),
        [questId](const QuestProgressData& data) { return data.questId == questId; });

    if (questIt == it->second.end())
        return;

    // Recovery strategy based on failure count
    if (questIt->consecutiveFailures <= 2)
    {
        // Strategy 1: Reset objectives and retry
        for (auto& objective : questIt->objectives)
        {
            if (objective.status == ObjectiveStatus::FAILED ||
                objective.status == ObjectiveStatus::BLOCKED)
            {
                objective.status = ObjectiveStatus::NOT_STARTED;
                objective.retryCount = 0;
                objective.timeSpent = 0;
            }
        }

        questIt->isStuck = false;
        questIt->strategy = QuestCompletionStrategy::SAFE_COMPLETION;

        questIt->completionLog.push_back(
            "Recovery: Reset objectives at " + std::to_string(GameTime::GetGameTimeMS()));
    }
    else if (questIt->consecutiveFailures <= 4)
    {
        // Strategy 2: Change approach
        questIt->strategy = QuestCompletionStrategy::THOROUGH_EXPLORATION;

        // Expand search areas
        for (auto& objective : questIt->objectives)
        {
            objective.searchRadius = std::min(objective.searchRadius * 2.0f, 250.0f);
        }

        // Request group help if available
        if (bot->GetGroup())
        {
            CoordinateGroupQuestCompletion(bot->GetGroup(), questId);
            questIt->strategy = QuestCompletionStrategy::GROUP_COORDINATION;
        }

        questIt->completionLog.push_back(
            "Recovery: Changed strategy at " + std::to_string(GameTime::GetGameTimeMS()));
    }
    else
    {
        // Strategy 3: Skip problematic objectives or abandon
        uint32 skippedCount = 0;
        uint32 completedCount = 0;

        for (auto& objective : questIt->objectives)
        {
            if (objective.status == ObjectiveStatus::COMPLETED)
            {
                completedCount++;
            }
            else if (objective.isOptional || objective.retryCount >= MAX_OBJECTIVE_RETRIES)
            {
                objective.status = ObjectiveStatus::SKIPPED;
                skippedCount++;
            }
        }

        // Check if quest can still be completed
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (quest)
        {
            uint32 requiredObjectives = quest->GetObjectives().size() - skippedCount;
            if (completedCount >= requiredObjectives * 0.8f) // 80% threshold
            {
                // Try to complete with what we have
                questIt->completionLog.push_back(
                    "Recovery: Attempting partial completion at " + std::to_string(GameTime::GetGameTimeMS()));
            }
            else
            {
                // Consider abandoning
                TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::RecoverFromCompletionFailure - Quest %u may need to be abandoned",
                    questId);
                AbandonUncompletableQuest(bot, questId);
            }
        }
    }
}

// 27. AbandonUncompletableQuest - Abandon a quest that cannot be completed
void QuestCompletion::AbandonUncompletableQuest(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::AbandonUncompletableQuest - Invalid parameters");
        return;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::AbandonUncompletableQuest - Bot %s abandoning quest %u",
        bot->GetName().c_str(), questId);

    // Check if quest can be abandoned
    if (bot->GetQuestStatus(questId) == QUEST_STATUS_NONE ||
        bot->GetQuestStatus(questId) == QUEST_STATUS_REWARDED)
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::AbandonUncompletableQuest - Quest %u not active",
            questId);
        return;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (quest && (quest->GetFlags() & QUEST_FLAGS_NO_ABANDON))
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::AbandonUncompletableQuest - Quest %u cannot be abandoned",
            questId);
        return;
    }

    // Remove from tracking
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt != it->second.end())
        {
            // Log abandonment
            questIt->completionLog.push_back(
                "Quest abandoned after " + std::to_string(questIt->consecutiveFailures) +
                " failures at " + std::to_string(GameTime::GetGameTimeMS()));

            // Update metrics
            _globalMetrics.questsFailed++;
            _botMetrics[bot->GetGUID().GetCounter()].questsFailed++;

            // Remove from tracking
            it->second.erase(questIt);
        }
    }

    // Actually abandon the quest
    bot->AbandonQuest(questId);

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::AbandonUncompletableQuest - Bot %s abandoned quest %u",
        bot->GetName().c_str(), questId);
}

// 28. DiagnoseCompletionIssues - Diagnose why a quest cannot be completed
void QuestCompletion::DiagnoseCompletionIssues(Player* bot, uint32 questId)
{
    if (!bot || !questId)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::DiagnoseCompletionIssues - Invalid parameters");
        return;
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::DiagnoseCompletionIssues - Diagnosing quest %u for bot %s",
        questId, bot->GetName().c_str());

    std::vector<std::string> issues;

    // Get quest and progress data
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
    {
        issues.push_back("Quest template not found");
    }
    else
    {
        // Check level requirements
        if (quest->GetMinLevel() > bot->GetLevel())
        {
            issues.push_back("Level too low (require " + std::to_string(quest->GetMinLevel()) +
                           ", have " + std::to_string(bot->GetLevel()) + ")");
        }

        // Check class/race requirements
        if (quest->GetAllowableClasses() && !(quest->GetAllowableClasses() & (1 << (bot->getClass() - 1))))
        {
            issues.push_back("Class requirement not met");
        }

        if (quest->GetAllowableRaces().RawValue && !(quest->GetAllowableRaces().RawValue & (1 << (bot->getRace() - 1))))
        {
            issues.push_back("Race requirement not met");
        }

        // Check prerequisite quests
        if (quest->GetPrevQuestId() && bot->GetQuestStatus(abs(quest->GetPrevQuestId())) != QUEST_STATUS_REWARDED)
        {
            issues.push_back("Prerequisite quest " + std::to_string(abs(quest->GetPrevQuestId())) + " not completed");
        }
    }

    // Check progress data
    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it != _botQuestProgress.end())
    {
        auto questIt = std::find_if(it->second.begin(), it->second.end(),
            [questId](const QuestProgressData& data) { return data.questId == questId; });

        if (questIt != it->second.end())
        {
            // Check objectives
            for (auto const& objective : questIt->objectives)
            {
                if (objective.status == ObjectiveStatus::FAILED)
                {
                    issues.push_back("Objective " + std::to_string(objective.objectiveIndex) +
                                   " failed after " + std::to_string(objective.retryCount) + " retries");
                }
                else if (objective.status == ObjectiveStatus::BLOCKED)
                {
                    if (objective.requiresGroup && !bot->GetGroup())
                    {
                        issues.push_back("Objective " + std::to_string(objective.objectiveIndex) +
                                       " requires group");
                    }
                    else
                    {
                        issues.push_back("Objective " + std::to_string(objective.objectiveIndex) +
                                       " blocked");
                    }
                }
            }

            // Check if stuck
            if (questIt->isStuck)
            {
                uint32 stuckDuration = (GameTime::GetGameTimeMS() - questIt->stuckTime) / 1000;
                issues.push_back("Stuck for " + std::to_string(stuckDuration) + " seconds");
            }

            // Check time spent
            if (questIt->timeSpent > questIt->estimatedCompletionTime * 2)
            {
                issues.push_back("Taking too long (spent " + std::to_string(questIt->timeSpent / 60000) +
                               " minutes, estimated " + std::to_string(questIt->estimatedCompletionTime / 60000) + ")");
            }

            // Add diagnosis to log
            questIt->completionLog.push_back(
                "Diagnosis: " + std::to_string(issues.size()) + " issues found at " +
                std::to_string(GameTime::GetGameTimeMS()));
        }
    }

    // Check bot-specific issues
    if (bot->isDead())
    {
        issues.push_back("Bot is dead");
    }

    if (bot->GetItemCount(0, true) >= MAX_QUEST_LOG_SIZE)
    {
        issues.push_back("Quest log full");
    }

    // Log all issues
    if (!issues.empty())
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::DiagnoseCompletionIssues - Quest %u issues:", questId);
        for (auto const& issue : issues)
        {
            TC_LOG_DEBUG("playerbot.quest", "  - %s", issue.c_str());
        }

        // Determine if quest should be abandoned
        if (issues.size() >= 3 || std::any_of(issues.begin(), issues.end(),
            [](const std::string& issue) {
                return issue.find("requirement") != std::string::npos ||
                       issue.find("prerequisite") != std::string::npos;
            }))
        {
            TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::DiagnoseCompletionIssues - Recommending abandon for quest %u",
                questId);
        }
    }
    else
    {
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::DiagnoseCompletionIssues - No issues found for quest %u",
            questId);
    }
}

// 29. UpdateBotQuestCompletion - Update quest completion for a specific bot
void QuestCompletion::UpdateBotQuestCompletion(Player* bot, uint32 diff)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.quest", "QuestCompletion::UpdateBotQuestCompletion - null bot!");
        return;
    }

    // Check if bot is actively completing quests
    if (_botsInQuestMode.find(bot->GetGUID().GetCounter()) == _botsInQuestMode.end())
        return;

    auto it = _botQuestProgress.find(bot->GetGUID().GetCounter());
    if (it == _botQuestProgress.end())
        return;

    uint32 currentTime = GameTime::GetGameTimeMS();
    bool hasActiveQuests = false;

    // Update each quest
    for (auto& progress : it->second)
    {
        if (progress.completionPercentage >= 100.0f)
        {
            // Check if ready for turn-in
            if (progress.requiresTurnIn)
            {
                ProcessQuestTurnIn(bot, progress.questId);
            }
            continue;
        }

        hasActiveQuests = true;

        // Update time tracking
        progress.timeSpent += diff;

        // Check for timeout
        if (progress.timeSpent > QUEST_COMPLETION_TIMEOUT)
        {
            TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::UpdateBotQuestCompletion - Quest %u timed out for bot %s",
                progress.questId, bot->GetName().c_str());
            HandleQuestCompletionError(bot, progress.questId, "Quest timed out");
            continue;
        }

        // Update objectives
        bool madeProgress = false;
        for (auto& objective : progress.objectives)
        {
            if (objective.status == ObjectiveStatus::COMPLETED)
                continue;

            // Update objective progress
            UpdateQuestObjectiveFromProgress(objective, bot);

            if (objective.currentCount > 0)
                madeProgress = true;

            // Check if objective is complete
            if (objective.currentCount >= objective.requiredCount)
            {
                objective.status = ObjectiveStatus::COMPLETED;
                _globalMetrics.objectivesCompleted++;
                _botMetrics[bot->GetGUID().GetCounter()].objectivesCompleted++;
                madeProgress = true;
            }
        }

        // Update completion percentage
        float totalProgress = 0.0f;
        uint32 totalObjectives = 0;

        for (auto const& objective : progress.objectives)
        {
            if (objective.isOptional)
                continue;

            totalObjectives++;
            if (objective.status == ObjectiveStatus::COMPLETED)
            {
                totalProgress += 1.0f;
            }
            else if (objective.requiredCount > 0)
            {
                totalProgress += static_cast<float>(objective.currentCount) / objective.requiredCount;
            }
        }

        if (totalObjectives > 0)
        {
            progress.completionPercentage = (totalProgress / totalObjectives) * 100.0f;
        }

        // Check for stuck detection
        if (!madeProgress)
        {
            if (currentTime - progress.lastUpdateTime > STUCK_DETECTION_TIME)
            {
                progress.isStuck = true;
                progress.stuckTime = currentTime;
                DetectStuckState(bot, progress.questId);
            }
        }
        else
        {
            progress.isStuck = false;
            progress.stuckTime = 0;
            progress.lastUpdateTime = currentTime;
        }

        // Execute current objective based on strategy
        if (currentTime - _botLastObjectiveUpdate[bot->GetGUID().GetCounter()] > OBJECTIVE_UPDATE_INTERVAL)
        {
            TrackQuestObjectives(bot);
            _botLastObjectiveUpdate[bot->GetGUID().GetCounter()] = currentTime;
        }
    }

    // If no active quests, remove from quest mode
    if (!hasActiveQuests)
    {
        _botsInQuestMode.erase(bot->GetGUID().GetCounter());
        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::UpdateBotQuestCompletion - Bot %s has no active quests",
            bot->GetName().c_str());
    }

    // Update metrics
    auto& metrics = _botMetrics[bot->GetGUID().GetCounter()];
    metrics.lastUpdate = std::chrono::steady_clock::now();

    // Calculate average completion time
    if (metrics.questsCompleted > 0)
    {
        float totalTime = 0.0f;
        for (auto const& progress : it->second)
        {
            if (progress.completionPercentage >= 100.0f)
            {
                totalTime += progress.timeSpent;
            }
        }
        metrics.averageCompletionTime = totalTime / metrics.questsCompleted.load();
    }

    // Calculate success rate
    uint32 totalQuests = metrics.questsStarted.load();
    uint32 completedQuests = metrics.questsCompleted.load();
    if (totalQuests > 0)
    {
        metrics.completionSuccessRate = static_cast<float>(completedQuests) / totalQuests;
    }
}

// 30. ValidateQuestStates - Validate and synchronize quest states
void QuestCompletion::ValidateQuestStates()
{
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Validate all tracked quests
    for (auto& [botGuid, progressList] : _botQuestProgress)
    {
        Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botGuid));
        if (!bot)
        {
            TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Bot %u not found, skipping validation",
                botGuid);
            continue;
        }

        // Remove invalid quests
        progressList.erase(
            std::remove_if(progressList.begin(), progressList.end(),
                [bot, currentTime](const QuestProgressData& progress) {
                    // Check if quest is still valid
                    QuestStatus status = bot->GetQuestStatus(progress.questId);
                    if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_REWARDED)
                    {
                        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Removing invalid quest %u",
                            progress.questId);
                        return true;
                    }

                    // Check if quest has been completed elsewhere
                    if (status == QUEST_STATUS_COMPLETE && progress.completionPercentage < 100.0f)
                    {
                        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Quest %u completed externally",
                            progress.questId);
                        return true;
                    }

                    // Check for stale quests (not updated in 10 minutes)
                    if (currentTime - progress.lastUpdateTime > 600000)
                    {
                        TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Quest %u is stale",
                            progress.questId);
                        return true;
                    }

                    return false;
                }),
            progressList.end()
        );

        // Validate objective states
        for (auto& progress : progressList)
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(progress.questId);
            if (!quest)
                continue;

            // Sync objective progress with actual game state
            QuestObjectives const& questObjectives = quest->GetObjectives();
            for (size_t i = 0; i < progress.objectives.size() && i < questObjectives.size(); ++i)
            {
                auto& objective = progress.objectives[i];
                QuestObjective const& questObj = questObjectives[i];

                // Update current count from game state
                uint32 actualCount = bot->GetQuestObjectiveData(questObj);
                if (actualCount != objective.currentCount)
                {
                    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Syncing objective %u for quest %u: %u -> %u",
                        static_cast<uint32>(i), progress.questId, objective.currentCount, actualCount);
                    objective.currentCount = actualCount;

                    if (actualCount >= objective.requiredCount)
                    {
                        objective.status = ObjectiveStatus::COMPLETED;
                    }
                }

                // Validate objective status
                if (objective.status == ObjectiveStatus::COMPLETED &&
                    objective.currentCount < objective.requiredCount)
                {
                    objective.status = ObjectiveStatus::IN_PROGRESS;
                }

                // Reset failed objectives if retry limit not reached
                if (objective.status == ObjectiveStatus::FAILED &&
                    objective.retryCount < MAX_OBJECTIVE_RETRIES)
                {
                    objective.status = ObjectiveStatus::NOT_STARTED;
                }
            }

            // Recalculate completion percentage
            float totalProgress = 0.0f;
            uint32 totalObjectives = 0;

            for (auto const& objective : progress.objectives)
            {
                if (!objective.isOptional)
                {
                    totalObjectives++;
                    if (objective.status == ObjectiveStatus::COMPLETED)
                    {
                        totalProgress += 1.0f;
                    }
                    else if (objective.requiredCount > 0)
                    {
                        totalProgress += static_cast<float>(objective.currentCount) / objective.requiredCount;
                    }
                }
            }

            if (totalObjectives > 0)
            {
                float newPercentage = (totalProgress / totalObjectives) * 100.0f;
                if (std::abs(newPercentage - progress.completionPercentage) > 0.1f)
                {
                    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Updated quest %u progress: %.2f%% -> %.2f%%",
                        progress.questId, progress.completionPercentage, newPercentage);
                    progress.completionPercentage = newPercentage;
                }
            }
        }
    }

    // Validate group synchronization data
    for (auto it = _groupObjectiveSync.begin(); it != _groupObjectiveSync.end();)
    {
        bool groupValid = false;
        Group* group = sGroupMgr->GetGroupByGUID(ObjectGuid::Create<HighGuid::Group>(it->first));
        if (group)
        {
            // Check if group still has members working on quests
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (member && _botsInQuestMode.find(member->GetGUID().GetCounter()) != _botsInQuestMode.end())
                {
                    groupValid = true;
                    break;
                }
            }
        }

        if (!groupValid)
        {
            TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Removing invalid group sync data for group %u",
                it->first);
            it = _groupObjectiveSync.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Clean up orphaned bot metrics
    for (auto it = _botMetrics.begin(); it != _botMetrics.end();)
    {
        if (_botQuestProgress.find(it->first) == _botQuestProgress.end())
        {
            it = _botMetrics.erase(it);
        }
        else
        {
            ++it;
        }
    }

    TC_LOG_DEBUG("playerbot.quest", "QuestCompletion::ValidateQuestStates - Validation complete. Tracking %u bots with %u in quest mode",
        static_cast<uint32>(_botQuestProgress.size()), static_cast<uint32>(_botsInQuestMode.size()));
}

} // namespace Playerbot