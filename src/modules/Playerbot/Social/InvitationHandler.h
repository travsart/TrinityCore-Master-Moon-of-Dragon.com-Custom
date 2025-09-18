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
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot
{

enum class InvitationResponse : uint8
{
    ACCEPT              = 0,
    DECLINE             = 1,
    IGNORE              = 2,
    DELAYED_ACCEPT      = 3,
    CONDITIONAL_ACCEPT  = 4
};

enum class InvitationReason : uint8
{
    ALREADY_IN_GROUP        = 0,
    DIFFERENT_FACTION       = 1,
    LEVEL_TOO_LOW           = 2,
    LEVEL_TOO_HIGH          = 3,
    BUSY_WITH_TASK          = 4,
    NOT_INTERESTED          = 5,
    ROLE_NOT_NEEDED         = 6,
    INVITER_REPUTATION      = 7,
    RANDOM_DECLINE          = 8,
    BOT_OFFLINE_MODE        = 9,
    QUEST_INCOMPATIBLE      = 10,
    ZONE_MISMATCH           = 11,
    COMBAT_STATE            = 12
};

struct InvitationCriteria
{
    uint32 minLevel;
    uint32 maxLevel;
    uint32 maxLevelDifference;
    bool sameFactionOnly;
    bool acceptCrossFaction;
    std::vector<uint32> preferredClasses;
    std::vector<uint32> preferredRaces;
    float acceptanceRate;           // 0.0 - 1.0 probability
    uint32 responseDelayMin;        // Minimum response time (ms)
    uint32 responseDelayMax;        // Maximum response time (ms)
    bool requiresQuestCompatibility;
    bool considerReputation;

    InvitationCriteria()
        : minLevel(1), maxLevel(80), maxLevelDifference(10)
        , sameFactionOnly(false), acceptCrossFaction(true)
        , acceptanceRate(0.85f), responseDelayMin(2000), responseDelayMax(8000)
        , requiresQuestCompatibility(false), considerReputation(true) {}
};

struct InvitationRequest
{
    ObjectGuid inviterGuid;
    ObjectGuid botGuid;
    uint32 groupId;
    uint32 requestTime;
    uint32 responseTime;
    InvitationResponse response;
    InvitationReason reason;
    bool hasResponded;
    std::string customMessage;

    InvitationRequest(ObjectGuid inviter, ObjectGuid bot, uint32 groupId)
        : inviterGuid(inviter), botGuid(bot), groupId(groupId)
        , requestTime(getMSTime()), responseTime(0)
        , response(InvitationResponse::IGNORE), reason(InvitationReason::NOT_INTERESTED)
        , hasResponded(false) {}
};

class TC_GAME_API InvitationHandler
{
public:
    static InvitationHandler* instance();

    // Core invitation handling
    InvitationResponse HandleGroupInvitation(Player* inviter, Player* bot, Group* group = nullptr);
    InvitationResponse HandleGuildInvitation(Player* inviter, Player* bot, uint32 guildId);
    InvitationResponse HandleArenaTeamInvitation(Player* inviter, Player* bot, uint32 arenaTeamId);
    InvitationResponse HandleTradeInvitation(Player* inviter, Player* bot);

    // Invitation decision making
    bool ShouldAcceptInvitation(Player* inviter, Player* bot, const InvitationCriteria& criteria);
    InvitationReason DetermineDeclineReason(Player* inviter, Player* bot);
    uint32 CalculateResponseDelay(Player* bot, const InvitationCriteria& criteria);

    // Bot behavior configuration
    void SetBotInvitationCriteria(uint32 botGuid, const InvitationCriteria& criteria);
    InvitationCriteria GetBotInvitationCriteria(uint32 botGuid);
    void SetGlobalAcceptanceRate(float rate) { _globalAcceptanceRate = rate; }

    // Reputation and relationship system
    void UpdateInviterReputation(ObjectGuid inviterGuid, ObjectGuid botGuid, bool positiveInteraction);
    float GetInviterReputation(ObjectGuid inviterGuid, ObjectGuid botGuid);
    void BlacklistInviter(ObjectGuid inviterGuid, ObjectGuid botGuid, uint32 durationMs = 3600000); // 1 hour

    // Smart invitation responses
    void SendInvitationResponse(Player* bot, Player* inviter, InvitationResponse response, InvitationReason reason = InvitationReason::NOT_INTERESTED);
    void SendDelayedResponse(const InvitationRequest& request);
    void ProcessPendingInvitations();

    // Contextual invitation behavior
    struct InvitationContext
    {
        enum Type
        {
            QUEST_GROUP     = 0,
            DUNGEON_GROUP   = 1,
            RAID_GROUP      = 2,
            SOCIAL_GROUP    = 3,
            PVP_GROUP       = 4,
            GUILD_GROUP     = 5
        };

        Type type;
        uint32 targetId;        // Quest ID, Dungeon ID, etc.
        Position location;
        uint32 estimatedDuration;
        std::vector<uint32> requiredRoles;
        bool isUrgent;

        InvitationContext(Type t, uint32 id = 0) : type(t), targetId(id), estimatedDuration(3600000), isUrgent(false) {}
    };

    InvitationResponse EvaluateContextualInvitation(Player* inviter, Player* bot, const InvitationContext& context);

    // Bot availability and state
    bool IsBotAvailable(Player* bot);
    bool IsBotBusy(Player* bot);
    void SetBotBusyState(uint32 botGuid, bool busy, const std::string& reason = "");

    // Invitation statistics
    struct InvitationStatistics
    {
        std::atomic<uint32> totalInvitationsReceived{0};
        std::atomic<uint32> totalInvitationsAccepted{0};
        std::atomic<uint32> totalInvitationsDeclined{0};
        std::atomic<uint32> totalInvitationsIgnored{0};
        std::atomic<float> averageResponseTime{5000.0f};
        std::atomic<float> currentAcceptanceRate{0.85f};
        std::chrono::steady_clock::time_point lastInvitation;

        void Reset() {
            totalInvitationsReceived = 0; totalInvitationsAccepted = 0;
            totalInvitationsDeclined = 0; totalInvitationsIgnored = 0;
            averageResponseTime = 5000.0f; currentAcceptanceRate = 0.85f;
            lastInvitation = std::chrono::steady_clock::now();
        }

        float GetAcceptanceRate() const {
            uint32 total = totalInvitationsReceived.load();
            uint32 accepted = totalInvitationsAccepted.load();
            return total > 0 ? (float)accepted / total : 0.85f;
        }
    };

    InvitationStatistics GetBotStatistics(uint32 botGuid);
    InvitationStatistics GetGlobalStatistics();

    // Configuration management
    void LoadInvitationConfiguration();
    void SaveInvitationConfiguration();
    void ResetBotInvitationBehavior(uint32 botGuid);

    // Update and maintenance
    void Update(uint32 diff);

private:
    InvitationHandler();
    ~InvitationHandler() = default;

    // Invitation data storage
    std::unordered_map<uint32, InvitationCriteria> _botCriteria;
    std::unordered_map<uint32, InvitationStatistics> _botStatistics;
    std::vector<InvitationRequest> _pendingInvitations;
    mutable std::mutex _invitationMutex;

    // Reputation system
    struct ReputationData
    {
        std::unordered_map<ObjectGuid, float> inviterRatings; // inviter -> rating (-1.0 to 1.0)
        std::unordered_map<ObjectGuid, uint32> blacklistedUntil; // inviter -> expiry time
        mutable std::mutex reputationMutex;

        float GetRating(ObjectGuid inviterGuid) {
            std::lock_guard<std::mutex> lock(reputationMutex);
            auto it = inviterRatings.find(inviterGuid);
            return it != inviterRatings.end() ? it->second : 0.0f;
        }

        void UpdateRating(ObjectGuid inviterGuid, float change) {
            std::lock_guard<std::mutex> lock(reputationMutex);
            float& rating = inviterRatings[inviterGuid];
            rating = std::clamp(rating + change, -1.0f, 1.0f);
        }

        bool IsBlacklisted(ObjectGuid inviterGuid) {
            std::lock_guard<std::mutex> lock(reputationMutex);
            auto it = blacklistedUntil.find(inviterGuid);
            return it != blacklistedUntil.end() && it->second > getMSTime();
        }
    };

    std::unordered_map<uint32, ReputationData> _botReputations; // botGuid -> reputation data

    // Global settings
    std::atomic<float> _globalAcceptanceRate{0.85f};
    std::atomic<bool> _enableReputationSystem{true};
    std::atomic<bool> _enableDelayedResponses{true};

    // Bot state tracking
    struct BotState
    {
        bool isBusy;
        std::string busyReason;
        uint32 busyUntil;
        uint32 lastInvitationTime;
        uint32 recentInvitationCount;

        BotState() : isBusy(false), busyUntil(0), lastInvitationTime(0), recentInvitationCount(0) {}
    };

    std::unordered_map<uint32, BotState> _botStates;
    mutable std::mutex _stateMutex;

    // Helper functions
    bool EvaluateLevelCompatibility(Player* inviter, Player* bot, const InvitationCriteria& criteria);
    bool EvaluateFactionCompatibility(Player* inviter, Player* bot, const InvitationCriteria& criteria);
    bool EvaluateQuestCompatibility(Player* inviter, Player* bot, const InvitationCriteria& criteria);
    bool EvaluateZoneCompatibility(Player* inviter, Player* bot);
    float CalculateAcceptanceProbability(Player* inviter, Player* bot, const InvitationCriteria& criteria);
    std::string GenerateDeclineMessage(InvitationReason reason, Player* bot);
    void UpdateInvitationStatistics(uint32 botGuid, InvitationResponse response, uint32 responseTime);
    void CleanupExpiredData();

    // Behavioral patterns
    void ApplyPersonalityToResponse(Player* bot, InvitationRequest& request);
    void HandleSpamPrevention(ObjectGuid inviterGuid, ObjectGuid botGuid);
    bool ShouldIgnoreBasedOnHistory(ObjectGuid inviterGuid, ObjectGuid botGuid);

    // Constants
    static constexpr float DEFAULT_ACCEPTANCE_RATE = 0.85f;
    static constexpr uint32 MIN_RESPONSE_DELAY = 1000; // 1 second
    static constexpr uint32 MAX_RESPONSE_DELAY = 15000; // 15 seconds
    static constexpr uint32 SPAM_PREVENTION_WINDOW = 60000; // 1 minute
    static constexpr uint32 MAX_INVITATIONS_PER_MINUTE = 3;
    static constexpr float REPUTATION_POSITIVE_GAIN = 0.1f;
    static constexpr float REPUTATION_NEGATIVE_LOSS = 0.2f;
    static constexpr uint32 DEFAULT_BLACKLIST_DURATION = 3600000; // 1 hour
    static constexpr uint32 INVITATION_CLEANUP_INTERVAL = 300000; // 5 minutes
};

/**
 * Bot Invitation Response Examples:
 *
 * ACCEPT SCENARIOS:
 * - Compatible level range (within 10 levels)
 * - Same faction or cross-faction enabled
 * - Bot is available and not busy
 * - Good reputation with inviter
 * - Quest/activity compatible
 *
 * DECLINE SCENARIOS:
 * - Level difference too high
 * - Already in group
 * - Busy with current task
 * - Poor reputation with inviter
 * - Random decline (based on acceptance rate)
 *
 * IGNORE SCENARIOS:
 * - Inviter is blacklisted
 * - Bot is in "offline" mode
 * - Spam prevention triggered
 *
 * DELAYED ACCEPT:
 * - Bot considers invitation for 2-8 seconds
 * - More realistic human-like behavior
 * - Response time varies by bot "personality"
 *
 * CONDITIONAL ACCEPT:
 * - "I can help but only for 30 minutes"
 * - "Sure, but I need to finish this quest first"
 * - Context-dependent acceptance
 */

} // namespace Playerbot