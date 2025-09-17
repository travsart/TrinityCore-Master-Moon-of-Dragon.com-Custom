/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "Strategy.h"
#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "QuestDef.h"

namespace Playerbot
{

// Strategy implementation
Strategy::Strategy(std::string const& name) : _name(name)
{
}

float Strategy::GetRelevance(BotAI* ai) const
{
    if (!ai)
        return 0.0f;

    StrategyRelevance relevance = CalculateRelevance(ai);
    return relevance.GetOverallRelevance();
}

StrategyRelevance Strategy::CalculateRelevance(BotAI* ai) const
{
    StrategyRelevance relevance;
    // Base implementation returns neutral relevance
    // Derived classes should override this
    return relevance;
}

void Strategy::AddAction(std::string const& name, std::shared_ptr<Action> action)
{
    if (action)
        _actions[name] = action;
}

std::shared_ptr<Action> Strategy::GetAction(std::string const& name) const
{
    auto it = _actions.find(name);
    return (it != _actions.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Action>> Strategy::GetActions() const
{
    std::vector<std::shared_ptr<Action>> actions;
    actions.reserve(_actions.size());

    for (auto const& pair : _actions)
        actions.push_back(pair.second);

    return actions;
}

void Strategy::AddTrigger(std::shared_ptr<Trigger> trigger)
{
    if (trigger)
        _triggers.push_back(trigger);
}

std::vector<std::shared_ptr<Trigger>> Strategy::GetTriggers() const
{
    return _triggers;
}

void Strategy::AddValue(std::string const& name, std::shared_ptr<Value> value)
{
    if (value)
        _values[name] = value;
}

std::shared_ptr<Value> Strategy::GetValue(std::string const& name) const
{
    auto it = _values.find(name);
    return (it != _values.end()) ? it->second : nullptr;
}

// CombatStrategy implementation
void CombatStrategy::InitializeActions()
{
    // Combat actions will be initialized by derived classes
}

void CombatStrategy::InitializeTriggers()
{
    // Combat triggers will be initialized by derived classes
}

float CombatStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai)
        return 0.0f;

    StrategyRelevance relevance = CalculateRelevance(ai);

    // Combat strategies are more relevant when in combat or under threat
    Player* bot = ai->GetBot();
    if (bot && bot->IsInCombat())
        relevance.combatRelevance += 100.0f;

    if (bot && bot->getAttackers().size() > 0)
        relevance.combatRelevance += 50.0f;

    return relevance.GetOverallRelevance();
}

bool CombatStrategy::ShouldFlee(BotAI* ai) const
{
    if (!ai)
        return false;

    Player* bot = ai->GetBot();
    if (!bot)
        return false;

    // Flee if health is critically low
    if (bot->GetHealthPct() < 15.0f)
        return true;

    // Flee if outnumbered significantly
    if (bot->getAttackers().size() > 3)
        return true;

    return false;
}

Unit* CombatStrategy::SelectTarget(BotAI* ai) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    // Priority: Current target if valid
    if (Unit* currentTarget = bot->GetSelectedUnit())
    {
        if (currentTarget->IsAlive() && bot->IsValidAttackTarget(currentTarget))
            return currentTarget;
    }

    // Find nearest hostile target
    Unit* nearestEnemy = nullptr;
    float nearestDistance = 30.0f; // Max combat range

    auto const& attackers = bot->getAttackers();
    for (Unit* attacker : attackers)
    {
        if (!attacker || !attacker->IsAlive())
            continue;

        float distance = bot->GetDistance(attacker);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestEnemy = attacker;
        }
    }

    return nearestEnemy;
}

// QuestStrategy implementation
void QuestStrategy::InitializeActions()
{
    // Quest actions will be initialized by derived classes
}

void QuestStrategy::InitializeTriggers()
{
    // Quest triggers will be initialized by derived classes
}

float QuestStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai)
        return 0.0f;

    StrategyRelevance relevance = CalculateRelevance(ai);

    // Quest strategies are more relevant when bot has active quests
    Player* bot = ai->GetBot();
    if (bot)
    {
        // Increase relevance based on number of active quests
        uint32 questCount = 0;
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            if (uint32 questId = bot->GetQuestSlotQuestId(slot))
            {
                if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
                {
                    if (!bot->CanCompleteQuest(questId))
                        questCount++;
                }
            }
        }

        relevance.questRelevance += questCount * 20.0f;
    }

    return relevance.GetOverallRelevance();
}

Quest const* QuestStrategy::SelectQuest(BotAI* ai) const
{
    if (!ai)
        return nullptr;

    Player* bot = ai->GetBot();
    if (!bot)
        return nullptr;

    // Find incomplete quest with highest reward
    Quest const* bestQuest = nullptr;
    uint32 bestReward = 0;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        if (uint32 questId = bot->GetQuestSlotQuestId(slot))
        {
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
            {
                if (!bot->CanCompleteQuest(questId))
                {
                    uint32 reward = quest->GetRewOrReqMoney() + quest->XPValue(bot);
                    if (reward > bestReward)
                    {
                        bestReward = reward;
                        bestQuest = quest;
                    }
                }
            }
        }
    }

    return bestQuest;
}

bool QuestStrategy::ShouldAbandonQuest(Quest const* quest) const
{
    if (!quest)
        return false;

    // Don't abandon important quests
    if (quest->HasFlag(QUEST_FLAGS_IMPORTANT))
        return false;

    // Could add logic for abandoning old/low-reward quests
    return false;
}

// SocialStrategy implementation
void SocialStrategy::InitializeActions()
{
    // Social actions will be initialized by derived classes
}

void SocialStrategy::InitializeTriggers()
{
    // Social triggers will be initialized by derived classes
}

float SocialStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai)
        return 0.0f;

    StrategyRelevance relevance = CalculateRelevance(ai);

    // Social strategies are more relevant when grouped or near other players
    Player* bot = ai->GetBot();
    if (bot)
    {
        if (Group* group = bot->GetGroup())
        {
            relevance.socialRelevance += 30.0f;
            relevance.socialRelevance += group->GetMembersCount() * 10.0f;
        }

        // Check for nearby players
        std::list<Player*> nearbyPlayers;
        Trinity::AnyPlayerInObjectRangeCheck checker(bot, 30.0f);
        Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(bot, nearbyPlayers, checker);
        Cell::VisitWorldObjects(bot, searcher, 30.0f);

        relevance.socialRelevance += nearbyPlayers.size() * 5.0f;
    }

    return relevance.GetOverallRelevance();
}

bool SocialStrategy::ShouldGroupWith(Player* player) const
{
    if (!player)
        return false;

    // Basic criteria for grouping
    // Could be expanded with more sophisticated logic
    return player->GetLevel() >= 10 && !player->GetGroup();
}

bool SocialStrategy::ShouldTrade(Player* player) const
{
    if (!player)
        return false;

    // Basic criteria for trading
    return player->GetLevel() >= 5;
}

std::string SocialStrategy::GenerateResponse(std::string const& message) const
{
    // Simple response generation
    // Could be expanded with more sophisticated AI
    if (message.find("hello") != std::string::npos ||
        message.find("hi") != std::string::npos)
        return "Hello there!";

    if (message.find("help") != std::string::npos)
        return "I'm here to help!";

    return "Interesting!";
}

// StrategyFactory implementation
StrategyFactory* StrategyFactory::instance()
{
    static StrategyFactory instance;
    return &instance;
}

void StrategyFactory::RegisterStrategy(std::string const& name,
                                     std::function<std::unique_ptr<Strategy>()> creator)
{
    _creators[name] = creator;
}

std::unique_ptr<Strategy> StrategyFactory::CreateStrategy(std::string const& name)
{
    auto it = _creators.find(name);
    if (it != _creators.end())
        return it->second();

    return nullptr;
}

std::vector<std::unique_ptr<Strategy>> StrategyFactory::CreateClassStrategies(uint8 classId, uint8 spec)
{
    std::vector<std::unique_ptr<Strategy>> strategies;

    // This will be expanded with class-specific strategies
    // For now, return empty vector

    return strategies;
}

std::vector<std::unique_ptr<Strategy>> StrategyFactory::CreateLevelStrategies(uint8 level)
{
    std::vector<std::unique_ptr<Strategy>> strategies;

    // This will be expanded with level-appropriate strategies
    // For now, return empty vector

    return strategies;
}

std::vector<std::unique_ptr<Strategy>> StrategyFactory::CreatePvPStrategies()
{
    std::vector<std::unique_ptr<Strategy>> strategies;

    // This will be expanded with PvP strategies
    // For now, return empty vector

    return strategies;
}

std::vector<std::unique_ptr<Strategy>> StrategyFactory::CreatePvEStrategies()
{
    std::vector<std::unique_ptr<Strategy>> strategies;

    // This will be expanded with PvE strategies
    // For now, return empty vector

    return strategies;
}

std::vector<std::string> StrategyFactory::GetAvailableStrategies() const
{
    std::vector<std::string> strategies;
    strategies.reserve(_creators.size());

    for (auto const& pair : _creators)
        strategies.push_back(pair.first);

    return strategies;
}

bool StrategyFactory::HasStrategy(std::string const& name) const
{
    return _creators.find(name) != _creators.end();
}

} // namespace Playerbot