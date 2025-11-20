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

class Player;

namespace Playerbot
{

// Forward declarations
enum class QuestEligibility : uint8;
struct ValidationResult;
struct ValidationContext;

// ValidationMetrics definition (needs full definition for return by value)
struct ValidationMetrics
{
    std::atomic<uint32> totalValidations{0};
    std::atomic<uint32> passedValidations{0};
    std::atomic<uint32> failedValidations{0};
    std::atomic<uint32> cacheHits{0};
    std::atomic<uint32> cacheMisses{0};
    std::atomic<float> averageValidationTime{5.0f};
    std::atomic<float> validationSuccessRate{0.85f};

    // Default constructor
    ValidationMetrics() = default;

    // Copy constructor for atomic members
    ValidationMetrics(const ValidationMetrics& other) :
        totalValidations(other.totalValidations.load()),
        passedValidations(other.passedValidations.load()),
        failedValidations(other.failedValidations.load()),
        cacheHits(other.cacheHits.load()),
        cacheMisses(other.cacheMisses.load()),
        averageValidationTime(other.averageValidationTime.load()),
        validationSuccessRate(other.validationSuccessRate.load()) {}

    // Assignment operator for atomic members
    ValidationMetrics& operator=(const ValidationMetrics& other) {
        if (this != &other) {
            totalValidations.store(other.totalValidations.load());
            passedValidations.store(other.passedValidations.load());
            failedValidations.store(other.failedValidations.load());
            cacheHits.store(other.cacheHits.load());
            cacheMisses.store(other.cacheMisses.load());
            averageValidationTime.store(other.averageValidationTime.load());
            validationSuccessRate.store(other.validationSuccessRate.load());
        }
        return *this;
    }

    void Reset() {
        totalValidations = 0; passedValidations = 0; failedValidations = 0;
        cacheHits = 0; cacheMisses = 0; averageValidationTime = 5.0f;
        validationSuccessRate = 0.85f;
    }

    float GetCacheHitRate() const {
        uint32 total = cacheHits.load() + cacheMisses.load();
        return total > 0 ? (float)cacheHits.load() / total : 0.0f;
    }
};

class TC_GAME_API IQuestValidation
{
public:
    virtual ~IQuestValidation() = default;

    // Core validation methods
    virtual bool ValidateQuestAcceptance(uint32 questId, Player* bot) = 0;
    virtual QuestEligibility GetDetailedEligibility(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<::std::string> GetValidationErrors(uint32 questId, Player* bot) = 0;
    virtual bool CanQuestBeStarted(uint32 questId, Player* bot) = 0;

    // Requirement validation
    virtual bool ValidateLevelRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateClassRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateRaceRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateFactionRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateSkillRequirements(uint32 questId, Player* bot) = 0;

    // Prerequisite validation
    virtual bool ValidateQuestPrerequisites(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestChainPosition(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<uint32> GetMissingPrerequisites(uint32 questId, Player* bot) = 0;
    virtual bool HasCompletedPrerequisiteQuests(uint32 questId, Player* bot) = 0;

    // Item and inventory validation
    virtual bool ValidateRequiredItems(uint32 questId, Player* bot) = 0;
    virtual bool ValidateInventorySpace(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestItemRequirements(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<::std::pair<uint32, uint32>> GetMissingItems(uint32 questId, Player* bot) = 0;

    // Status and state validation
    virtual bool ValidateQuestStatus(uint32 questId, Player* bot) = 0;
    virtual bool IsQuestAlreadyCompleted(uint32 questId, Player* bot) = 0;
    virtual bool IsQuestInProgress(uint32 questId, Player* bot) = 0;
    virtual bool IsQuestLogFull(Player* bot) = 0;
    virtual bool IsQuestRepeatable(uint32 questId, Player* bot) = 0;

    // Reputation and standing validation
    virtual bool ValidateReputationRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateMinimumReputation(uint32 questId, Player* bot) = 0;
    virtual bool ValidateMaximumReputation(uint32 questId, Player* bot) = 0;
    virtual ::std::vector<::std::pair<uint32, int32>> GetReputationRequirements(uint32 questId) = 0;

    // Time and availability validation
    virtual bool ValidateQuestAvailability(uint32 questId, Player* bot) = 0;
    virtual bool ValidateSeasonalAvailability(uint32 questId) = 0;
    virtual bool ValidateDailyQuestLimits(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestTimer(uint32 questId, Player* bot) = 0;

    // Zone and location validation
    virtual bool ValidateZoneRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateAreaRequirements(uint32 questId, Player* bot) = 0;
    virtual bool IsInCorrectZone(uint32 questId, Player* bot) = 0;
    virtual bool CanQuestBeStartedAtLocation(uint32 questId, const Position& location) = 0;

    // Group and party validation
    virtual bool ValidateGroupRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidatePartyQuestRequirements(uint32 questId, Player* bot) = 0;
    virtual bool ValidateRaidQuestRequirements(uint32 questId, Player* bot) = 0;
    virtual bool CanGroupMemberShareQuest(uint32 questId, Player* sharer, Player* receiver) = 0;

    // Advanced validation
    virtual bool ValidateWithContext(ValidationContext& context) = 0;
    virtual bool ValidateQuestObjectives(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestRewards(uint32 questId, Player* bot) = 0;
    virtual bool ValidateQuestDifficulty(uint32 questId, Player* bot) = 0;

    // Validation caching and optimization
    virtual ValidationResult GetCachedValidation(uint32 questId, uint32 botGuid) = 0;
    virtual void CacheValidationResult(uint32 questId, uint32 botGuid, const ValidationResult& result) = 0;
    virtual void InvalidateValidationCache(uint32 botGuid) = 0;
    virtual void CleanupExpiredCache() = 0;

    // Batch validation for efficiency
    virtual ::std::unordered_map<uint32, ValidationResult> ValidateMultipleQuests(
        const ::std::vector<uint32>& questIds, Player* bot) = 0;
    virtual ::std::vector<uint32> FilterValidQuests(const ::std::vector<uint32>& questIds, Player* bot) = 0;
    virtual ::std::vector<uint32> GetEligibleQuests(Player* bot, const ::std::vector<uint32>& candidates) = 0;

    // Error reporting and diagnostics
    virtual ::std::string GetDetailedValidationReport(uint32 questId, Player* bot) = 0;
    virtual void LogValidationFailure(uint32 questId, Player* bot, const ::std::string& reason) = 0;
    virtual ::std::vector<::std::string> GetRecommendationsForFailedQuest(uint32 questId, Player* bot) = 0;

    // Performance monitoring
    virtual ValidationMetrics GetValidationMetrics() const = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
};

} // namespace Playerbot
