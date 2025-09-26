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
#include "Guild.h"
#include "GuildMgr.h"
#include "Chat.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

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

    GuildChatMessage() : senderId(0), chatType(CHAT_MSG_GUILD), timestamp(getMSTime())
        , requiresResponse(false), relevanceScore(0.0f) {}
};

/**
 * @brief Comprehensive guild integration system for automated guild participation
 *
 * This system provides intelligent guild interactions including chat participation,
 * guild bank management, event coordination, and social activities for playerbots.
 */
class TC_GAME_API GuildIntegration
{
public:
    static GuildIntegration* instance();

    // Core guild functionality using TrinityCore's Guild system
    void ProcessGuildInteraction(Player* player);
    void HandleGuildChat(Player* player, const GuildChatMessage& message);
    void ParticipateInGuildActivities(Player* player);
    void ManageGuildResponsibilities(Player* player);

    // Guild chat automation
    void AutomateGuildChatParticipation(Player* player);
    void RespondToGuildChat(Player* player, const GuildChatMessage& message);
    void InitiateGuildConversation(Player* player);
    void ShareGuildInformation(Player* player, const std::string& topic);

    // Guild bank management
    void AutomateGuildBankInteractions(Player* player);
    void DepositItemsToGuildBank(Player* player);
    void WithdrawNeededItems(Player* player);
    void OrganizeGuildBank(Player* player);
    void ManageGuildBankPermissions(Player* player);

    // Guild event coordination
    void CoordinateGuildEvents(Player* player);
    void ScheduleGuildActivities(Player* player);
    void ManageGuildCalendar(Player* player);
    void OrganizeGuildRuns(Player* player);

    // Advanced guild features
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

    void SetGuildProfile(uint32 playerGuid, const GuildProfile& profile);
    GuildProfile GetGuildProfile(uint32 playerGuid);

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

        GuildParticipation(uint32 pGuid, uint32 gId) : playerGuid(pGuid), guildId(gId)
            , totalChatMessages(0), helpfulResponses(0), eventsAttended(0)
            , socialScore(0.5f), contributionScore(0.5f), lastActivity(getMSTime())
            , joinDate(getMSTime()) {}
    };

    GuildParticipation GetGuildParticipation(uint32 playerGuid);
    void UpdateGuildParticipation(uint32 playerGuid, GuildActivityType activityType);

    // Guild recruitment assistance
    void AssistWithRecruitment(Player* player);
    void EvaluateRecruitmentCandidates(Player* player);
    void WelcomeNewGuildMembers(Player* player);
    void MentorJuniorMembers(Player* player);

    // Guild leadership support
    void SupportGuildLeadership(Player* player);
    void HandleOfficerDuties(Player* player);
    void AssistWithGuildManagement(Player* player);
    void ProvideMemberFeedback(Player* player);

    // Chat intelligence and response generation
    struct ChatIntelligence
    {
        std::unordered_map<std::string, std::vector<std::string>> responseTemplates;
        std::unordered_map<std::string, float> keywordWeights;
        std::vector<std::string> conversationStarters;
        std::vector<std::string> helpfulTips;
        std::unordered_map<std::string, std::string> topicResponses;
        uint32 lastResponseTime;
        float responseFrequency;

        ChatIntelligence() : lastResponseTime(0), responseFrequency(0.3f) {}
    };

    std::string GenerateGuildChatResponse(Player* player, const GuildChatMessage& message);
    std::string GenerateConversationStarter(Player* player);
    bool ShouldRespondToMessage(Player* player, const GuildChatMessage& message);
    void LearnFromGuildConversations(Player* player);

    // Guild achievement coordination
    void ContributeToGuildAchievements(Player* player);
    void CoordinateAchievementEfforts(Guild* guild);
    void TrackAchievementProgress(Player* player);
    void CelebrateGuildAchievements(Player* player);

    // Guild social features
    void OrganizeSocialEvents(Player* player);
    void ParticipateInGuildTradition(Player* player);
    void MaintainGuildFriendships(Player* player);
    void HandleGuildConflicts(Player* player);

    // Performance monitoring
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

    GuildMetrics GetPlayerGuildMetrics(uint32 playerGuid);
    GuildMetrics GetGuildBotMetrics(uint32 guildId);

    // Guild bank automation
    void OptimizeGuildBankUsage(Player* player);
    void AutoDepositValuableItems(Player* player);
    void AutoWithdrawNeededConsumables(Player* player);
    void ManageGuildBankTabs(Player* player);
    void TrackGuildBankActivity(Player* player);

    // Guild event management
    void CreateGuildEvent(Player* player, const std::string& eventType);
    void ManageGuildCalendarEvents(Player* player);
    void CoordinateRaidScheduling(Player* player);
    void OrganizePvPEvents(Player* player);

    // Advanced guild AI
    void AnalyzeGuildDynamics(Guild* guild);
    void AdaptToGuildCulture(Player* player);
    void DetectGuildMoodAndTone(Guild* guild);
    void AdjustBehaviorToGuildNorms(Player* player);

    // Configuration and customization
    void SetGuildAutomationLevel(uint32 playerGuid, float level);
    void EnableGuildActivity(uint32 playerGuid, GuildActivityType activity, bool enable);
    void SetGuildChatFrequency(uint32 playerGuid, float frequency);
    void ConfigureGuildBankAccess(uint32 playerGuid, bool autoDeposit, bool autoWithdraw);

    // Error handling and recovery
    void HandleGuildInteractionError(uint32 playerGuid, const std::string& error);
    void RecoverFromGuildFailure(uint32 playerGuid);
    void HandleGuildLeaving(Player* player);
    void HandleGuildInvitations(Player* player, uint32 guildId);

    // Update and maintenance
    void Update(uint32 diff);
    void UpdateGuildParticipation();
    void ProcessGuildEvents();
    void CleanupGuildData();

private:
    GuildIntegration();
    ~GuildIntegration() = default;

    // Core guild data
    struct PlayerState {
        uint32 lastGuildBankInteraction = 0;
        uint32 lastChatTime = 0;
        uint32 lastEventParticipation = 0;
    };

    std::unordered_map<uint32, GuildProfile> _playerProfiles; // playerGuid -> profile
    std::unordered_map<uint32, GuildParticipation> _playerParticipation; // playerGuid -> participation
    std::unordered_map<uint32, GuildMetrics> _playerMetrics; // playerGuid -> metrics
    std::unordered_map<uint32, PlayerState> _playerStates; // playerGuid -> state
    mutable std::mutex _guildMutex;

    // Chat intelligence system
    std::unordered_map<uint32, ChatIntelligence> _chatIntelligence; // playerGuid -> intelligence
    std::unordered_map<std::string, std::vector<std::string>> _globalResponseTemplates;
    std::vector<std::string> _conversationTopics;
    mutable std::mutex _chatMutex;

    // Guild activity tracking
    struct GuildActivityTracker
    {
        uint32 guildId;
        std::unordered_map<uint32, uint32> memberActivity; // playerGuid -> activity count
        std::vector<std::pair<std::string, uint32>> recentEvents; // event, timestamp
        std::queue<GuildChatMessage> chatHistory;
        float overallMorale;
        float activityLevel;
        uint32 lastAnalysisTime;

        GuildActivityTracker(uint32 gId) : guildId(gId), overallMorale(0.8f)
            , activityLevel(0.6f), lastAnalysisTime(getMSTime()) {}
    };

    std::unordered_map<uint32, GuildActivityTracker> _guildTracking; // guildId -> tracker

    // Performance tracking
    GuildMetrics _globalMetrics;

    // Helper functions
    void InitializeChatTemplates();
    void LoadGuildSpecificData(uint32 guildId);
    bool IsAppropriateTimeToChat(Player* player);
    void UpdateGuildSocialGraph(uint32 guildId);

    // Chat response generation
    void OfferGuildAssistance(Player* player, const std::string& assistance);
    void SendGuildChatMessage(Player* player, const std::string& message);
    std::string SelectResponseTemplate(const std::string& category);
    std::string PersonalizeResponse(Player* player, const std::string& responseTemplate);
    float CalculateMessageRelevance(Player* player, const GuildChatMessage& message);
    std::vector<std::string> ExtractKeywords(const std::string& message);

    // Guild bank logic
    bool ShouldDepositItem(Player* player, uint32 itemId);
    bool ShouldWithdrawItem(Player* player, uint32 itemId);
    void ProcessGuildBankTransaction(Player* player, uint32 itemId, bool isDeposit);
    void OptimizeGuildBankLayout(Player* player);

    // Event coordination
    void PlanGuildEvent(Player* player, const std::string& eventType, uint32 proposedTime);
    void InviteMembersToEvent(Player* player, const std::string& eventId);
    void ManageEventAttendance(Player* player, const std::string& eventId);
    bool IsGoodTimeForEvent(uint32 proposedTime, const std::vector<uint32>& memberIds);

    // Social dynamics analysis
    void AnalyzeGuildPersonalities(uint32 guildId);
    void DetectGuildCliques(uint32 guildId);
    void IdentifyInfluentialMembers(uint32 guildId);
    void AssessGuildHealth(uint32 guildId);

    // Performance optimization
    void OptimizeGuildInteractions(Player* player);
    void CacheGuildData(uint32 guildId);
    void PrioritizeGuildActivities(Player* player);
    void UpdateGuildMetrics(uint32 playerGuid, GuildActivityType activity, bool wasSuccessful);

    // Constants
    static constexpr uint32 GUILD_UPDATE_INTERVAL = 60000; // 1 minute
    static constexpr uint32 CHAT_PROCESSING_INTERVAL = 5000; // 5 seconds
    static constexpr uint32 ACTIVITY_ANALYSIS_INTERVAL = 300000; // 5 minutes
    static constexpr uint32 MAX_CHAT_HISTORY = 100;
    static constexpr float MIN_PARTICIPATION_SCORE = 0.3f;
    static constexpr uint32 GUILD_BANK_CHECK_INTERVAL = 1800000; // 30 minutes
    static constexpr float DEFAULT_CHAT_FREQUENCY = 0.3f; // 30% chance to respond
    static constexpr uint32 EVENT_PLANNING_ADVANCE = 86400000; // 24 hours
    static constexpr uint32 MAX_GUILD_ACTIVITIES_PER_DAY = 15;
    static constexpr float SOCIAL_SCORE_DECAY = 0.01f; // Daily decay without activity
    static constexpr uint32 NEW_MEMBER_WELCOME_DELAY = 300000; // 5 minutes
};

} // namespace Playerbot