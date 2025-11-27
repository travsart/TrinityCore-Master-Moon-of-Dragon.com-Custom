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
#include "Threading/LockHierarchy.h"
#include "Player.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Chat.h"
#include "../Core/DI/Interfaces/IGuildIntegration.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <chrono>
#include "GameTime.h"

namespace Playerbot
{

// Enums and structs defined in IGuildIntegration.h interface:
// - enum class GuildActivityType
// - enum class GuildChatStyle
// - enum class GuildRole
// - struct GuildChatMessage
// - struct GuildProfile
// - struct GuildParticipation

/**
 * @brief Comprehensive guild integration system for automated guild participation
 *
 * This system provides intelligent guild interactions including chat participation,
 * guild bank management, event coordination, and social activities for playerbots.
 */
class TC_GAME_API GuildIntegration final : public IGuildIntegration
{
public:
    explicit GuildIntegration(Player* bot);
    ~GuildIntegration();
    GuildIntegration(GuildIntegration const&) = delete;
    GuildIntegration& operator=(GuildIntegration const&) = delete;

    // Core guild functionality using TrinityCore's Guild system
    void ProcessGuildInteraction() override;
    void HandleGuildChat(const GuildChatMessage& message) override;
    void ParticipateInGuildActivities() override;
    void ManageGuildResponsibilities() override;

    // Guild chat automation
    void AutomateGuildChatParticipation() override;
    void RespondToGuildChat(const GuildChatMessage& message) override;
    void InitiateGuildConversation() override;
    void ShareGuildInformation(const std::string& topic) override;

    // Guild bank management
    void AutomateGuildBankInteractions() override;
    void DepositItemsToGuildBank() override;
    void WithdrawNeededItems() override;
    void OrganizeGuildBank() override;
    void ManageGuildBankPermissions() override;

    // Guild event coordination
    void CoordinateGuildEvents() override;
    void ScheduleGuildActivities() override;
    void ManageGuildCalendar() override;
    void OrganizeGuildRuns() override;

    // Advanced guild features (GuildProfile struct defined in IGuildIntegration.h)
    void SetGuildProfile(const GuildProfile& profile) override;
    GuildProfile GetGuildProfile() override;

    // Guild participation tracking (GuildParticipation struct defined in IGuildIntegration.h)
    GuildParticipation GetGuildParticipation() override;
    void UpdateGuildParticipation(GuildActivityType activityType) override;

    // Guild recruitment assistance
    void AssistWithRecruitment() override;
    void EvaluateRecruitmentCandidates() override;
    void WelcomeNewGuildMembers() override;
    void MentorJuniorMembers() override;

    // Guild leadership support
    void SupportGuildLeadership() override;
    void HandleOfficerDuties() override;
    void AssistWithGuildManagement() override;
    void ProvideMemberFeedback() override;

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

    std::string GenerateGuildChatResponse(const GuildChatMessage& message) override;
    std::string GenerateConversationStarter() override;
    bool ShouldRespondToMessage(const GuildChatMessage& message) override;
    void LearnFromGuildConversations() override;

    // Guild achievement coordination
    void ContributeToGuildAchievements() override;
    void CoordinateAchievementEfforts(Guild* guild) override;
    void TrackAchievementProgress() override;
    void CelebrateGuildAchievements() override;

    // Guild social features
    void OrganizeSocialEvents() override;
    void ParticipateInGuildTradition() override;
    void MaintainGuildFriendships() override;
    void HandleGuildConflicts() override;

    // Performance monitoring (GuildMetrics struct defined in IGuildIntegration.h)
    GuildMetrics GetPlayerGuildMetrics() override;
    GuildMetrics GetGuildBotMetrics(uint32 guildId) override;

    // Guild bank automation
    void OptimizeGuildBankUsage() override;
    void AutoDepositValuableItems() override;
    void AutoWithdrawNeededConsumables() override;
    void ManageGuildBankTabs() override;
    void TrackGuildBankActivity() override;

    // Guild event management
    void CreateGuildEvent(const std::string& eventType) override;
    void ManageGuildCalendarEvents() override;
    void CoordinateRaidScheduling() override;
    void OrganizePvPEvents() override;

    // Advanced guild AI
    void AnalyzeGuildDynamics(Guild* guild) override;
    void AdaptToGuildCulture() override;
    void DetectGuildMoodAndTone(Guild* guild) override;
    void AdjustBehaviorToGuildNorms() override;

    // Configuration and customization
    void SetGuildAutomationLevel(float level) override;
    void EnableGuildActivity(GuildActivityType activity, bool enable) override;
    void SetGuildChatFrequency(float frequency) override;
    void ConfigureGuildBankAccess(bool autoDeposit, bool autoWithdraw) override;

    // Error handling and recovery
    void HandleGuildInteractionError(const std::string& error) override;
    void RecoverFromGuildFailure() override;
    void HandleGuildLeaving() override;
    void HandleGuildInvitations(uint32 guildId) override;

    // Update and maintenance
    void Update(uint32 diff) override;
    void UpdateGuildParticipation() override;
    void ProcessGuildEvents() override;
    void CleanupGuildData() override;

private:
    Player* _bot;

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
    

    // Chat intelligence system
    std::unordered_map<uint32, ChatIntelligence> _chatIntelligence; // playerGuid -> intelligence
    std::unordered_map<std::string, std::vector<std::string>> _globalResponseTemplates;
    std::vector<std::string> _conversationTopics;
    

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

        // Default constructor for std::unordered_map compatibility
        GuildActivityTracker() : guildId(0), overallMorale(0.8f)
            , activityLevel(0.6f), lastAnalysisTime(GameTime::GetGameTimeMS()) {}

        GuildActivityTracker(uint32 gId) : guildId(gId), overallMorale(0.8f)
            , activityLevel(0.6f), lastAnalysisTime(GameTime::GetGameTimeMS()) {}
    };

    std::unordered_map<uint32, GuildActivityTracker> _guildTracking; // guildId -> tracker

    // Performance tracking
    GuildMetrics _globalMetrics;

    // Helper functions
    void InitializeChatTemplates();
    void LoadGuildSpecificData(uint32 guildId);
    bool IsAppropriateTimeToChat();
    void UpdateGuildSocialGraph(uint32 guildId);

    // Chat response generation
    void OfferGuildAssistance(const std::string& assistance);
    void SendGuildChatMessage(const std::string& message);
    std::string SelectResponseTemplate(const std::string& category);
    std::string PersonalizeResponse(const std::string& responseTemplate);
    float CalculateMessageRelevance(const GuildChatMessage& message);
    std::vector<std::string> ExtractKeywords(const std::string& message);

    // Guild bank logic
    bool ShouldDepositItem(uint32 itemId);
    bool ShouldWithdrawItem(uint32 itemId);
    void ProcessGuildBankTransaction(uint32 itemId, bool isDeposit);
    void OptimizeGuildBankLayout();

    // Guild bank helper methods
    uint8 FindBestTabForItem(Item* item) const;
    int8 FindEmptySlotInTab(Guild* guild, uint8 tabId) const;
    bool HasGuildBankDepositRights(uint8 tabId) const;
    bool HasGuildBankWithdrawRights(uint8 tabId) const;
    int32 GetRemainingWithdrawSlots(uint8 tabId) const;
    int8 FindItemInTab(Guild* guild, uint8 tabId, uint32 itemId) const;
    bool FindFreeInventorySlot(uint8& outBag, uint8& outSlot) const;

    // Needs analysis methods
    void AnalyzeConsumableNeeds(std::vector<uint32>& neededItems) const;
    void AnalyzeReagentNeeds(std::vector<uint32>& neededItems) const;
    void AnalyzeProfessionMaterialNeeds(std::vector<uint32>& neededItems) const;
    void AnalyzeEquipmentUpgrades(Guild* guild, std::vector<std::pair<uint8, uint8>>& upgradeLocations) const;

    // Item evaluation methods
    bool HasRelevantProfession(uint32 itemSubClass) const;
    bool IsEquipmentUpgrade(ItemTemplate const* itemTemplate) const;
    bool CanLearnRecipe(ItemTemplate const* itemTemplate) const;
    bool HasItemsNeedingSockets() const;

    // Event coordination
    void PlanGuildEvent(const std::string& eventType, uint32 proposedTime);
    void InviteMembersToEvent(const std::string& eventId);
    void ManageEventAttendance(const std::string& eventId);
    bool IsGoodTimeForEvent(uint32 proposedTime, const std::vector<uint32>& memberIds);

    // Social dynamics analysis
    void AnalyzeGuildPersonalities(uint32 guildId);
    void DetectGuildCliques(uint32 guildId);
    void IdentifyInfluentialMembers(uint32 guildId);
    void AssessGuildHealth(uint32 guildId);

    // Performance optimization
    void OptimizeGuildInteractions();
    void CacheGuildData(uint32 guildId);
    void PrioritizeGuildActivities();
    void UpdateGuildMetrics(GuildActivityType activity, bool wasSuccessful);

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