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
#include <string>
#include <atomic>
#include <chrono>

class Group;

namespace Playerbot
{

// Forward declarations
enum class QuestAcceptanceStrategy : uint8;
enum class QuestEligibility : uint8;
struct QuestGiverInfo;
struct QuestPickupRequest;
struct QuestPickupFilter;

// QuestPickupMetrics definition (needs full definition for return by value)
struct QuestPickupMetrics
{
    std::atomic<uint32> questsPickedUp{0};
    std::atomic<uint32> questsRejected{0};
    std::atomic<uint32> pickupAttempts{0};
    std::atomic<uint32> successfulPickups{0};
    std::atomic<float> averagePickupTime{5000.0f};
    std::atomic<float> questPickupEfficiency{0.8f};
    std::atomic<uint32> questGiversVisited{0};
    std::atomic<uint32> movementDistance{0};
    std::chrono::steady_clock::time_point lastUpdate;

    QuestPickupMetrics() = default;

    // Copy constructor needed since std::atomic is not copyable
    QuestPickupMetrics(const QuestPickupMetrics& other) noexcept
        : questsPickedUp(other.questsPickedUp.load())
        , questsRejected(other.questsRejected.load())
        , pickupAttempts(other.pickupAttempts.load())
        , successfulPickups(other.successfulPickups.load())
        , averagePickupTime(other.averagePickupTime.load())
        , questPickupEfficiency(other.questPickupEfficiency.load())
        , questGiversVisited(other.questGiversVisited.load())
        , movementDistance(other.movementDistance.load())
        , lastUpdate(other.lastUpdate)
    {
    }

    // Move constructor
    QuestPickupMetrics(QuestPickupMetrics&& other) noexcept
        : questsPickedUp(other.questsPickedUp.load())
        , questsRejected(other.questsRejected.load())
        , pickupAttempts(other.pickupAttempts.load())
        , successfulPickups(other.successfulPickups.load())
        , averagePickupTime(other.averagePickupTime.load())
        , questPickupEfficiency(other.questPickupEfficiency.load())
        , questGiversVisited(other.questGiversVisited.load())
        , movementDistance(other.movementDistance.load())
        , lastUpdate(std::move(other.lastUpdate))
    {
    }

    // Copy assignment operator
    QuestPickupMetrics& operator=(const QuestPickupMetrics& other) noexcept
    {
        if (this != &other)
        {
            questsPickedUp = other.questsPickedUp.load();
            questsRejected = other.questsRejected.load();
            pickupAttempts = other.pickupAttempts.load();
            successfulPickups = other.successfulPickups.load();
            averagePickupTime = other.averagePickupTime.load();
            questPickupEfficiency = other.questPickupEfficiency.load();
            questGiversVisited = other.questGiversVisited.load();
            movementDistance = other.movementDistance.load();
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }

    void Reset() {
        questsPickedUp = 0; questsRejected = 0; pickupAttempts = 0;
        successfulPickups = 0; averagePickupTime = 5000.0f;
        questPickupEfficiency = 0.8f; questGiversVisited = 0;
        movementDistance = 0;
        lastUpdate = std::chrono::steady_clock::now();
    }

    float GetSuccessRate() const {
        uint32 attempts = pickupAttempts.load();
        uint32 successful = successfulPickups.load();
        return attempts > 0 ? (float)successful / attempts : 0.0f;
    }
};

class TC_GAME_API IQuestPickup
{
public:
    virtual ~IQuestPickup() = default;

    // Core quest pickup functionality
    virtual bool PickupQuest(uint32 questId, Player* bot, uint32 questGiverGuid = 0) = 0;
    virtual bool PickupQuestFromGiver(Player* bot, uint32 questGiverGuid, uint32 questId = 0) = 0;
    virtual void PickupAvailableQuests(Player* bot) = 0;
    virtual void PickupQuestsInArea(Player* bot, float radius = 50.0f) = 0;

    // Quest discovery and scanning
    virtual ::std::vector<uint32> DiscoverNearbyQuests(Player* bot, float scanRadius = 100.0f) = 0;
    virtual ::std::vector<QuestGiverInfo> ScanForQuestGivers(Player* bot, float scanRadius = 100.0f) = 0;
    virtual ::std::vector<uint32> GetAvailableQuestsFromGiver(uint32 questGiverGuid, Player* bot) = 0;

    // Quest eligibility and validation
    virtual QuestEligibility CheckQuestEligibility(uint32 questId, Player* bot) = 0;
    virtual bool CanAcceptQuest(uint32 questId, Player* bot) = 0;
    virtual bool MeetsQuestRequirements(uint32 questId, Player* bot) = 0;

    // Quest filtering and prioritization
    virtual ::std::vector<uint32> FilterQuests(const ::std::vector<uint32>& questIds, Player* bot, const QuestPickupFilter& filter) = 0;
    virtual ::std::vector<uint32> PrioritizeQuests(const ::std::vector<uint32>& questIds, Player* bot, QuestAcceptanceStrategy strategy) = 0;
    virtual bool ShouldAcceptQuest(uint32 questId, Player* bot) = 0;

    // Quest giver interaction
    virtual bool InteractWithQuestGiver(Player* bot, uint32 questGiverGuid) = 0;
    virtual Position GetQuestGiverLocation(uint32 questGiverGuid) = 0;

    // Group quest coordination
    virtual void CoordinateGroupQuestPickup(Group* group, uint32 questId) = 0;
    virtual bool ShareQuestPickup(Group* group, uint32 questId, Player* initiator) = 0;

    // Automated quest pickup strategies
    virtual void ExecuteStrategy(Player* bot, QuestAcceptanceStrategy strategy) = 0;
    virtual void ProcessQuestPickupQueue(Player* bot) = 0;
    virtual void ScheduleQuestPickup(const QuestPickupRequest& request) = 0;

    // Performance monitoring
    virtual QuestPickupMetrics GetBotPickupMetrics(uint32 botGuid) = 0;
    virtual QuestPickupMetrics GetGlobalPickupMetrics() = 0;

    // Configuration and settings
    virtual void SetQuestAcceptanceStrategy(uint32 botGuid, QuestAcceptanceStrategy strategy) = 0;
    virtual QuestAcceptanceStrategy GetQuestAcceptanceStrategy(uint32 botGuid) = 0;
    virtual void SetQuestPickupFilter(uint32 botGuid, const QuestPickupFilter& filter) = 0;

    // Quest database integration
    virtual void LoadQuestGiverData() = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessPickupQueue() = 0;
    virtual void CleanupExpiredRequests() = 0;
};

} // namespace Playerbot
