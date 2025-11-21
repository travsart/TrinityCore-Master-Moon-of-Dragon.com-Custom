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
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include "Chat.h"
#include "GameTime.h"

// Forward declarations
class Player;
class Guild;

namespace Playerbot
{

// Enum definitions (needs full definition for struct members)
enum class GuildActivityType : uint8
{
    CHAT_PARTICIPATION      = 0,
    GUILD_BANK_INTERACTION  = 1,
    GUILD_EVENT_ATTENDANCE  = 2,
    OFFICER_DUTIES          = 3,
    RECRUITMENT_ASSISTANCE  = 4,
    GUILD_REPAIR_USAGE      = 5,
    ACHIEVEMENT_CONTRIBUTION= 6,
    SOCIAL_INTERACTION      = 7
};

enum class GuildChatStyle : uint8
{
    MINIMAL         = 0,  // Rare, essential responses only
    MODERATE        = 1,  // Regular participation
    ACTIVE          = 2,  // Frequent communication
    SOCIAL          = 3,  // Chatty and friendly
    PROFESSIONAL    = 4,  // Focused on guild business
    HELPFUL         = 5   // Offers assistance frequently
};

enum class GuildRole : uint8
{
    MEMBER          = 0,
    VETERAN         = 1,
    OFFICER         = 2,
    LEADER          = 3,
    BANKER          = 4,
    RECRUITER       = 5,
    EVENT_ORGANIZER = 6
};

// GuildChatMessage definition (needs full definition for parameter passing)
struct GuildChatMessage
{
    uint32 senderId;
    std::string senderName;
    std::string content;
    ChatMsg chatType;
    uint32 timestamp;
    bool requiresResponse;
    std::vector<std::string> keywords;
    float relevanceScore;

    GuildChatMessage() : senderId(0), chatType(CHAT_MSG_GUILD), timestamp(GameTime::GetGameTimeMS())
        , requiresResponse(false), relevanceScore(0.0f) {}
};

// Guild profile structure
struct GuildProfile
{
    GuildChatStyle chatStyle;
    GuildRole preferredRole;
    std::vector<GuildActivityType> activeActivities;
    float participationLevel; // 0.0 = minimal, 1.0 = maximum
    float helpfulnessLevel;
    float leadershipAmbition;
    std::vector<std::string> expertise; // Areas of knowledge
    std::vector<std::string> interests; // Topics of interest
    std::unordered_set<uint32> friendlyMembers;
    std::unordered_set<std::string> chatTriggers;
    uint32 dailyActivityQuota;
    bool autoAcceptGuildInvites;

    GuildProfile() : chatStyle(GuildChatStyle::MODERATE), preferredRole(GuildRole::MEMBER)
        , participationLevel(0.7f), helpfulnessLevel(0.8f), leadershipAmbition(0.3f)
        , dailyActivityQuota(10), autoAcceptGuildInvites(true) {}
};

// Guild participation tracking
struct GuildParticipation
{
    uint32 playerGuid;
    uint32 guildId;
    std::vector<GuildChatMessage> recentMessages;
    std::unordered_map<GuildActivityType, uint32> activityCounts;
    uint32 totalChatMessages;
    uint32 helpfulResponses;
    uint32 eventsAttended;
    float socialScore;
    float contributionScore;
    uint32 lastActivity;
    uint32 joinDate;

    // Default constructor for std::unordered_map compatibility
    GuildParticipation() : playerGuid(0), guildId(0)
        , totalChatMessages(0), helpfulResponses(0), eventsAttended(0)
        , socialScore(0.5f), contributionScore(0.5f), lastActivity(GameTime::GetGameTimeMS())
        , joinDate(GameTime::GetGameTimeMS()) {}

    GuildParticipation(uint32 pGuid, uint32 gId) : playerGuid(pGuid), guildId(gId)
        , totalChatMessages(0), helpfulResponses(0), eventsAttended(0)
        , socialScore(0.5f), contributionScore(0.5f), lastActivity(GameTime::GetGameTimeMS())
        , joinDate(GameTime::GetGameTimeMS()) {}
};

// Guild metrics tracking
struct GuildMetrics
{
    std::atomic<uint32> guildInteractions{0};
    std::atomic<uint32> chatMessages{0};
    std::atomic<uint32> bankTransactions{0};
    std::atomic<uint32> eventsParticipated{0};
    std::atomic<uint32> helpfulActions{0};
    std::atomic<float> averageParticipationScore{0.7f};
    std::atomic<float> socialIntegrationScore{0.8f};
    std::atomic<float> contributionRating{0.75f};
    std::chrono::steady_clock::time_point lastUpdate;

    // Default constructor
    GuildMetrics() = default;

    // Copy constructor for atomic members
    GuildMetrics(const GuildMetrics& other) :
        guildInteractions(other.guildInteractions.load()),
        chatMessages(other.chatMessages.load()),
        bankTransactions(other.bankTransactions.load()),
        eventsParticipated(other.eventsParticipated.load()),
        helpfulActions(other.helpfulActions.load()),
        averageParticipationScore(other.averageParticipationScore.load()),
        socialIntegrationScore(other.socialIntegrationScore.load()),
        contributionRating(other.contributionRating.load()),
        lastUpdate(other.lastUpdate) {}

    // Assignment operator for atomic members
    GuildMetrics& operator=(const GuildMetrics& other) {
        if (this != &other) {
            guildInteractions.store(other.guildInteractions.load());
            chatMessages.store(other.chatMessages.load());
            bankTransactions.store(other.bankTransactions.load());
            eventsParticipated.store(other.eventsParticipated.load());
            helpfulActions.store(other.helpfulActions.load());
            averageParticipationScore.store(other.averageParticipationScore.load());
            socialIntegrationScore.store(other.socialIntegrationScore.load());
            contributionRating.store(other.contributionRating.load());
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }

    void Reset() {
        guildInteractions = 0; chatMessages = 0; bankTransactions = 0;
        eventsParticipated = 0; helpfulActions = 0; averageParticipationScore = 0.7f;
        socialIntegrationScore = 0.8f; contributionRating = 0.75f;
        lastUpdate = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Interface for comprehensive guild integration and automation
 *
 * Defines the contract for automated guild participation including chat,
 * bank management, event coordination, and social activities.
 */
class TC_GAME_API IGuildIntegration
{
public:
    virtual ~IGuildIntegration() = default;

    // Core guild functionality
    virtual void ProcessGuildInteraction() = 0;
    virtual void HandleGuildChat(const GuildChatMessage& message) = 0;
    virtual void ParticipateInGuildActivities() = 0;
    virtual void ManageGuildResponsibilities() = 0;

    // Guild chat automation
    virtual void AutomateGuildChatParticipation() = 0;
    virtual void RespondToGuildChat(const GuildChatMessage& message) = 0;
    virtual void InitiateGuildConversation() = 0;
    virtual void ShareGuildInformation(const std::string& topic) = 0;

    // Guild bank management
    virtual void AutomateGuildBankInteractions() = 0;
    virtual void DepositItemsToGuildBank() = 0;
    virtual void WithdrawNeededItems() = 0;
    virtual void OrganizeGuildBank() = 0;
    virtual void ManageGuildBankPermissions() = 0;

    // Guild event coordination
    virtual void CoordinateGuildEvents() = 0;
    virtual void ScheduleGuildActivities() = 0;
    virtual void ManageGuildCalendar() = 0;
    virtual void OrganizeGuildRuns() = 0;

    // Guild profiles and participation
    virtual void SetGuildProfile(const GuildProfile& profile) = 0;
    virtual GuildProfile GetGuildProfile() = 0;
    virtual GuildParticipation GetGuildParticipation() = 0;
    virtual void UpdateGuildParticipation(GuildActivityType activityType) = 0;

    // Guild recruitment assistance
    virtual void AssistWithRecruitment() = 0;
    virtual void EvaluateRecruitmentCandidates() = 0;
    virtual void WelcomeNewGuildMembers() = 0;
    virtual void MentorJuniorMembers() = 0;

    // Guild leadership support
    virtual void SupportGuildLeadership() = 0;
    virtual void HandleOfficerDuties() = 0;
    virtual void AssistWithGuildManagement() = 0;
    virtual void ProvideMemberFeedback() = 0;

    // Chat intelligence and response generation
    virtual std::string GenerateGuildChatResponse(const GuildChatMessage& message) = 0;
    virtual std::string GenerateConversationStarter() = 0;
    virtual bool ShouldRespondToMessage(const GuildChatMessage& message) = 0;
    virtual void LearnFromGuildConversations() = 0;

    // Guild achievement coordination
    virtual void ContributeToGuildAchievements() = 0;
    virtual void CoordinateAchievementEfforts(Guild* guild) = 0;
    virtual void TrackAchievementProgress() = 0;
    virtual void CelebrateGuildAchievements() = 0;

    // Guild social features
    virtual void OrganizeSocialEvents() = 0;
    virtual void ParticipateInGuildTradition() = 0;
    virtual void MaintainGuildFriendships() = 0;
    virtual void HandleGuildConflicts() = 0;

    // Performance monitoring
    virtual GuildMetrics GetPlayerGuildMetrics() = 0;
    virtual GuildMetrics GetGuildBotMetrics(uint32 guildId) = 0;

    // Guild bank automation
    virtual void OptimizeGuildBankUsage() = 0;
    virtual void AutoDepositValuableItems() = 0;
    virtual void AutoWithdrawNeededConsumables() = 0;
    virtual void ManageGuildBankTabs() = 0;
    virtual void TrackGuildBankActivity() = 0;

    // Guild event management
    virtual void CreateGuildEvent(const std::string& eventType) = 0;
    virtual void ManageGuildCalendarEvents() = 0;
    virtual void CoordinateRaidScheduling() = 0;
    virtual void OrganizePvPEvents() = 0;

    // Advanced guild AI
    virtual void AnalyzeGuildDynamics(Guild* guild) = 0;
    virtual void AdaptToGuildCulture() = 0;
    virtual void DetectGuildMoodAndTone(Guild* guild) = 0;
    virtual void AdjustBehaviorToGuildNorms() = 0;

    // Configuration and customization
    virtual void SetGuildAutomationLevel(float level) = 0;
    virtual void EnableGuildActivity(GuildActivityType activity, bool enable) = 0;
    virtual void SetGuildChatFrequency(float frequency) = 0;
    virtual void ConfigureGuildBankAccess(bool autoDeposit, bool autoWithdraw) = 0;

    // Error handling and recovery
    virtual void HandleGuildInteractionError(const std::string& error) = 0;
    virtual void RecoverFromGuildFailure() = 0;
    virtual void HandleGuildLeaving() = 0;
    virtual void HandleGuildInvitations(uint32 guildId) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateGuildParticipation() = 0;
    virtual void ProcessGuildEvents() = 0;
    virtual void CleanupGuildData() = 0;
};

} // namespace Playerbot
